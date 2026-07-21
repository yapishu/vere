import assert from 'node:assert/strict';
import { existsSync } from 'node:fs';
import test from 'node:test';

import {
  createVereWasmHost,
  MemoryVereWasmFileStore,
  runVereWasmProbe,
  runVereWasmProbeWithFileStore,
} from './vere-wasm-host.mjs';

const textEncoder = new TextEncoder();
const textDecoder = new TextDecoder();

function writeCString(memory, ptr, value) {
  const bytes = textEncoder.encode(`${value}\0`);
  new Uint8Array(memory.buffer).set(bytes, ptr);
}

function writeBytes(memory, ptr, value) {
  new Uint8Array(memory.buffer).set(textEncoder.encode(value), ptr);
}

function readBytes(memory, ptr, len) {
  return textDecoder.decode(new Uint8Array(memory.buffer, ptr, len));
}

test('createVereWasmHost implements the noun hostfs imports in memory', () => {
  const memory = new WebAssembly.Memory({ initial: 1, maximum: 2 });
  const { env, files } = createVereWasmHost({
    memory,
    initialFiles: {
      '/in': 'ames!',
    },
  });

  writeCString(memory, 16, '/in');
  writeCString(memory, 32, '/out');
  writeBytes(memory, 64, 'quic');

  assert.equal(env.u3_wasm_file_exists(16), 0);
  assert.equal(env.u3_wasm_file_size(16), 5n);
  assert.equal(env.u3_wasm_file_read(16, 80, 5n), 0);
  assert.equal(readBytes(memory, 80, 5), 'ames!');

  assert.equal(env.u3_wasm_file_write(32, 64, 4n), 0);
  assert.equal(textDecoder.decode(files.get('/out')), 'quic');

  const handle = env.u3_wasm_file_open(32, 0x01 | 0x02, 0o600);
  assert.ok(handle > 0);
  assert.equal(env.u3_wasm_file_handle_size(handle), 4n);
  assert.equal(env.u3_wasm_file_resize(handle, 8n), 0);
  writeBytes(memory, 96, 'ames');
  assert.equal(env.u3_wasm_file_write_at(handle, 4n, 96, 4n), 0);
  assert.equal(env.u3_wasm_file_read_at(handle, 0n, 112, 8n), 0);
  assert.equal(readBytes(memory, 112, 8), 'quicames');
  assert.equal(env.u3_wasm_file_close(handle), 0);
  assert.equal(env.u3_wasm_file_unlink(32), 0);
  assert.equal(env.u3_wasm_file_exists(32), -1);
});

test('MemoryVereWasmFileStore snapshots hostfs files and directories', async () => {
  const store = new MemoryVereWasmFileStore({
    files: {
      '/in': 'ames!',
    },
    directories: ['/tmp'],
  });

  const first = await store.load();
  assert.equal(textDecoder.decode(first.files.get('/in')), 'ames!');
  assert.ok(first.directories.has('/'));
  assert.ok(first.directories.has('/tmp'));

  first.files.set('/out', textEncoder.encode('quic'));
  first.directories.add('/var');
  await store.save(first);

  first.files.set('/out', textEncoder.encode('mutated'));
  first.directories.delete('/var');

  const second = await store.load();
  assert.equal(textDecoder.decode(second.files.get('/out')), 'quic');
  assert.ok(second.directories.has('/var'));

  await store.clear();
  const cleared = await store.load();
  assert.equal(cleared.files.size, 0);
  assert.deepEqual([...cleared.directories], ['/']);
});

const bootLiteWasm = new URL(
  '../../../zig-out/bin/noun-boot-lite-wasm.wasm',
  import.meta.url,
);

const hostfsWasm = new URL(
  '../../../zig-out/bin/noun-hostfs-wasm.wasm',
  import.meta.url,
);

const ivoryBootWasm = new URL(
  '../../../zig-out/bin/noun-ivory-boot-wasm.wasm',
  import.meta.url,
);

test(
  'runVereWasmProbe starts the linked noun boot-lite artifact when built',
  {
    skip: existsSync(bootLiteWasm)
      ? false
      : 'run zig build noun-boot-lite-wasm to create the artifact',
  },
  async () => {
    const { exitCode, plan } = await runVereWasmProbe(bootLiteWasm, {
      memoryOptions: {
        loomExponent: 24,
        maximumBytes: 128 * 1024 * 1024,
      },
    });

    assert.equal(exitCode, 0);
    assert.equal(plan.loomExponent, 24);
  },
);

test(
  'runVereWasmProbe boots the linked Ivory pill artifact when built',
  {
    skip: existsSync(ivoryBootWasm)
      ? false
      : 'run zig build noun-ivory-boot-wasm to create the artifact',
  },
  async () => {
    let stderr = '';
    const { exitCode, plan } = await runVereWasmProbe(ivoryBootWasm, {
      memoryOptions: {
        loomExponent: 26,
        overheadBytes: 256 * 1024 * 1024,
        maximumBytes: 512 * 1024 * 1024,
      },
      onStderr: bytes => {
        stderr += textDecoder.decode(bytes).replace(/\r/g, '');
      },
    });

    assert.equal(exitCode, 0);
    assert.equal(plan.loomExponent, 26);
    assert.match(stderr, /ivory: booted core [0-9a-f]+/);
  },
);

test(
  'runVereWasmProbeWithFileStore persists the linked hostfs probe output',
  {
    skip: existsSync(hostfsWasm)
      ? false
      : 'run zig build noun-hostfs-wasm to create the artifact',
  },
  async () => {
    const store = new MemoryVereWasmFileStore();
    const { exitCode, plan } = await runVereWasmProbeWithFileStore(hostfsWasm, {
      fileStore: store,
      initialFiles: {
        '/in': 'ames!',
      },
      memoryOptions: {
        loomExponent: 20,
        overheadBytes: 64 * 1024,
        minimumInitialBytes: 16 * 1024 * 1024,
        maximumBytes: 32 * 1024 * 1024,
      },
    });

    assert.equal(exitCode, 0);
    assert.equal(plan.loomExponent, 20);

    const persisted = await store.load();
    assert.equal(textDecoder.decode(persisted.files.get('/in')), 'ames!');
    assert.equal(textDecoder.decode(persisted.files.get('/out')), 'quic');
    assert.equal(persisted.files.has('/random'), false);
    assert.ok(persisted.directories.has('/tmp'));
  },
);
