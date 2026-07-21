import assert from 'node:assert/strict';
import { existsSync } from 'node:fs';
import { readFile } from 'node:fs/promises';
import test from 'node:test';

import {
  createVereWasmHost,
  instantiateVereDiskWasmRuntime,
  MemoryVereWasmFileStore,
  runVereWasmProbe,
  runVereWasmProbeWithFileStore,
} from './vere-wasm-host.mjs';
import { encodePeek } from './mesa-pact.mjs';
import {
  atomFromBytesLE,
  cue,
} from './urbit-noun.mjs';
import {
  loadMesaOvumJam,
  mesaHeerOvumJam,
} from './ames-wasm-events.mjs';
import { extractMesaEffects } from './ames-wasm-effects.mjs';

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

const marsBootWasm = new URL(
  '../../../zig-out/bin/mars-boot-wasm.wasm',
  import.meta.url,
);

const vereDiskWasm = new URL(
  '../../../zig-out/bin/vere-disk-wasm.wasm',
  import.meta.url,
);

const brassPill = new URL(
  '../../../../urbit/bin/brass.pill',
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
  'runVereWasmProbe constructs a diskless fake boot sequence when built',
  {
    skip: existsSync(marsBootWasm)
      ? false
      : 'run zig build mars-boot-wasm to create the artifact',
  },
  async () => {
    let stderr = '';
    const { exitCode, plan } = await runVereWasmProbe(marsBootWasm, {
      memoryOptions: {
        loomExponent: 24,
        maximumBytes: 128 * 1024 * 1024,
      },
      onStderr: bytes => {
        stderr += textDecoder.decode(bytes).replace(/\r/g, '');
      },
    });

    assert.equal(exitCode, 0);
    assert.equal(plan.loomExponent, 24);
    assert.match(stderr, /mars-boot: fake zod events=7 life=1/);
  },
);

test(
  'runVereWasmProbe constructs a fake boot sequence from the real brass pill when present',
  {
    skip: existsSync(marsBootWasm) && existsSync(brassPill)
      ? false
      : 'run zig build mars-boot-wasm with ../urbit/bin/brass.pill present',
  },
  async () => {
    const pillBytes = new Uint8Array(await readFile(brassPill));
    let stderr = '';
    const { exitCode, plan } = await runVereWasmProbe(marsBootWasm, {
      args: ['--real-pill', '--loom', '29'],
      initialFiles: {
        '/brass.pill': pillBytes,
      },
      memoryOptions: {
        loomExponent: 29,
        overheadBytes: 512 * 1024 * 1024,
        maximumBytes: 1536 * 1024 * 1024,
      },
      onStderr: bytes => {
        stderr += textDecoder.decode(bytes).replace(/\r/g, '');
      },
    });

    assert.equal(exitCode, 0);
    assert.equal(plan.loomExponent, 29);
    assert.match(stderr, /boot: parsing %brass pill/);
    assert.match(stderr, /mars-boot: real brass fake zod events=\d+ life=\d+/);
  },
);

test(
  'runVereWasmProbe persists and reloads real brass boot events through hostfs',
  {
    skip: existsSync(marsBootWasm) && existsSync(brassPill)
      ? false
      : 'run zig build mars-boot-wasm with ../urbit/bin/brass.pill present',
  },
  async () => {
    const pillBytes = new Uint8Array(await readFile(brassPill));
    const fileStore = new MemoryVereWasmFileStore();
    const memoryOptions = {
      loomExponent: 29,
      overheadBytes: 512 * 1024 * 1024,
      maximumBytes: 1536 * 1024 * 1024,
    };

    let saveStderr = '';
    const saved = await runVereWasmProbeWithFileStore(marsBootWasm, {
      args: ['--real-pill', '--save-events', '--loom', '29'],
      fileStore,
      initialFiles: {
        '/brass.pill': pillBytes,
      },
      memoryOptions,
      onStderr: bytes => {
        saveStderr += textDecoder.decode(bytes).replace(/\r/g, '');
      },
    });

    assert.equal(saved.exitCode, 0);
    assert.match(saveStderr, /saved event log .* events=[1-9]\d*/);

    const snapshot = await fileStore.load();
    const eventLog = snapshot.files.get('/pier/.urb/log/events.bin');
    assert.ok(eventLog);
    assert.ok(eventLog.length > 16);
    assert.ok(snapshot.directories.has('/pier/.urb/log'));

    let loadStderr = '';
    const loaded = await runVereWasmProbeWithFileStore(marsBootWasm, {
      args: ['--real-pill', '--load-events', '--loom', '29'],
      fileStore,
      initialFiles: {
        '/brass.pill': pillBytes,
      },
      memoryOptions,
      onStderr: bytes => {
        loadStderr += textDecoder.decode(bytes).replace(/\r/g, '');
      },
    });

    assert.equal(loaded.exitCode, 0);
    assert.match(loadStderr, /loaded event log .* events=[1-9]\d*/);
  },
);

test(
  'runVereWasmProbeWithFileStore persists real brass boot events through u3_disk',
  {
    skip: existsSync(vereDiskWasm) && existsSync(brassPill)
      ? false
      : 'run zig build vere-disk-wasm with ../urbit/bin/brass.pill present',
  },
  async () => {
    const pillBytes = new Uint8Array(await readFile(brassPill));
    const fileStore = new MemoryVereWasmFileStore();
    let saveStderr = '';

    const saved = await runVereWasmProbeWithFileStore(
      vereDiskWasm,
      {
        args: ['--loom', '29'],
        fileStore,
        initialFiles: {
          '/brass.pill': pillBytes,
        },
        memoryOptions: {
          loomExponent: 29,
          overheadBytes: 512 * 1024 * 1024,
          maximumBytes: 1536 * 1024 * 1024,
        },
        onStderr: bytes => {
          saveStderr += textDecoder.decode(bytes).replace(/\r/g, '');
        },
      },
    );

    assert.equal(saved.exitCode, 0);
    assert.equal(saved.plan.loomExponent, 29);
    assert.match(saveStderr, /disk-wasm: saved events=[1-9]\d* committed=\d+/);
    assert.match(saveStderr, /disk-wasm: reloaded events=[1-9]\d* committed=\d+/);

    const persisted = await fileStore.load();
    const eventLog = persisted.files.get('/pier/.urb/log/0i0/events.bin');
    const meta = persisted.files.get('/pier/.urb/log/meta.bin');
    assert.ok(eventLog);
    assert.ok(eventLog.length > 16);
    assert.equal(meta?.length, 37);
    assert.ok(persisted.directories.has('/pier/.urb/log/0i0'));

    let loadStderr = '';
    const loaded = await runVereWasmProbeWithFileStore(vereDiskWasm, {
      args: ['--loom', '29', '--load-only'],
      fileStore,
      initialFiles: {
        '/brass.pill': pillBytes,
      },
      memoryOptions: {
        loomExponent: 29,
        overheadBytes: 512 * 1024 * 1024,
        maximumBytes: 1536 * 1024 * 1024,
      },
      onStderr: bytes => {
        loadStderr += textDecoder.decode(bytes).replace(/\r/g, '');
      },
    });

    assert.equal(loaded.exitCode, 0);
    assert.match(
      loadStderr,
      /disk-wasm: loaded existing events=[1-9]\d* committed=\d+/,
    );
  },
);

test(
  'runVereWasmProbeWithFileStore appends host-provided Mesa-load and packet ova',
  {
    skip: existsSync(vereDiskWasm) && existsSync(brassPill)
      ? false
      : 'run zig build vere-disk-wasm with ../urbit/bin/brass.pill present',
  },
  async () => {
    const pillBytes = new Uint8Array(await readFile(brassPill));
    const fileStore = new MemoryVereWasmFileStore();
    const memoryOptions = {
      loomExponent: 29,
      overheadBytes: 512 * 1024 * 1024,
      maximumBytes: 1536 * 1024 * 1024,
    };
    const initialFiles = {
      '/brass.pill': pillBytes,
    };
    const ovumPath = '/in/load-mesa.ovum.jam';
    const loadEffectsPath = '/out/load-mesa.effects.jam';
    const heerPath = '/in/peek-heer.ovum.jam';
    const heerEffectsPath = '/out/peek-heer.effects.jam';
    const packet = encodePeek({
      ship: 0x100n,
      path: '/~zod/0/1/c/x/1/base/sys/kelvin',
    });

    const saved = await runVereWasmProbeWithFileStore(vereDiskWasm, {
      args: ['--loom', '29'],
      fileStore,
      initialFiles,
      memoryOptions,
    });
    assert.equal(saved.exitCode, 0);

    let pokeStderr = '';
    const poked = await runVereWasmProbeWithFileStore(vereDiskWasm, {
      args: [
        '--loom',
        '29',
        '--load-only',
        '--run-boot',
        '--poke-ovum',
        ovumPath,
        '--effects-out',
        loadEffectsPath,
        '--poke-ovum',
        heerPath,
        '--effects-out',
        heerEffectsPath,
      ],
      fileStore,
      initialFiles: {
        ...initialFiles,
        [ovumPath]: loadMesaOvumJam(),
        [heerPath]: mesaHeerOvumJam({ packet }),
      },
      memoryOptions,
      onStderr: bytes => {
        pokeStderr += textDecoder.decode(bytes).replace(/\r/g, '');
      },
    });

    assert.equal(poked.exitCode, 0);
    assert.match(
      pokeStderr,
      /disk-wasm: host ovum \/in\/load-mesa\.ovum\.jam committed event=21 core=[0-9a-f]+ effects=\d+/,
    );
    assert.match(
      pokeStderr,
      /disk-wasm: wrote effects \/out\/load-mesa\.effects\.jam bytes=\d+/,
    );
    assert.match(
      pokeStderr,
      /disk-wasm: host ovum \/in\/peek-heer\.ovum\.jam committed event=22 core=[0-9a-f]+ effects=\d+/,
    );
    assert.match(
      pokeStderr,
      /disk-wasm: wrote effects \/out\/peek-heer\.effects\.jam bytes=\d+/,
    );

    for (const effectsPath of [loadEffectsPath, heerEffectsPath]) {
      const effectsBytes = poked.host.files.get(effectsPath);
      assert.ok(effectsBytes?.length > 0);
      const effects = cue(atomFromBytesLE(effectsBytes));
      assert.ok(effects === 0n || Array.isArray(effects));
      assert.deepEqual(extractMesaEffects(effectsBytes), {
        sends: [],
        pushes: [],
        binds: [],
        unknown: [],
      });
    }

    let replayStderr = '';
    const replayed = await runVereWasmProbeWithFileStore(vereDiskWasm, {
      args: ['--loom', '29', '--load-only', '--run-boot'],
      fileStore,
      initialFiles,
      memoryOptions,
      onStderr: bytes => {
        replayStderr += textDecoder.decode(bytes).replace(/\r/g, '');
      },
    });

    assert.equal(replayed.exitCode, 0);
    assert.match(replayStderr, /disk-wasm: loaded existing events=\d+ committed=22/);
    assert.match(replayStderr, /disk-wasm: replayed extra events=2 final=22/);
  },
);

test(
  'instantiateVereDiskWasmRuntime keeps real brass runtime resident across host pokes',
  {
    skip: existsSync(vereDiskWasm) && existsSync(brassPill)
      ? false
      : 'run zig build vere-disk-wasm with ../urbit/bin/brass.pill present',
  },
  async () => {
    const pillBytes = new Uint8Array(await readFile(brassPill));
    const fileStore = new MemoryVereWasmFileStore();
    const memoryOptions = {
      loomExponent: 29,
      overheadBytes: 512 * 1024 * 1024,
      maximumBytes: 1536 * 1024 * 1024,
    };
    let stderr = '';

    const runtime = await instantiateVereDiskWasmRuntime(vereDiskWasm, {
      fileStore,
      initialFiles: {
        '/brass.pill': pillBytes,
      },
      memoryOptions,
      onStderr: bytes => {
        stderr += textDecoder.decode(bytes).replace(/\r/g, '');
      },
    });

    runtime.init({
      loomExponent: 29,
      ship: 0x100n,
    });
    assert.match(stderr, /disk-wasm: reactor saved events=[1-9]\d* committed=\d+/);
    assert.match(stderr, /disk-wasm: reactor loaded events=[1-9]\d* committed=\d+/);
    const bootEvent = runtime.event();
    assert.ok(bootEvent > 0n);

    const loadEffectsPath = '/out/reactor-load-mesa.effects.jam';
    runtime.pokeLoadMesa({ effectsPath: loadEffectsPath });
    assert.equal(runtime.event(), bootEvent + 1n);
    assert.ok(runtime.host.files.get(loadEffectsPath)?.length > 0);

    const packet = encodePeek({
      ship: 0x100n,
      path: '/~zod/0/1/c/x/1/base/sys/kelvin',
    });
    const heerPath = '/in/reactor-peek-heer.ovum.jam';
    const heerEffectsPath = '/out/reactor-peek-heer.effects.jam';
    runtime.host.files.set(heerPath, mesaHeerOvumJam({ packet }));

    runtime.pokeOvum({
      ovumPath: heerPath,
      effectsPath: heerEffectsPath,
    });
    assert.equal(runtime.event(), bootEvent + 2n);
    assert.ok(runtime.host.files.get(heerEffectsPath)?.length > 0);
    assert.match(
      stderr,
      /disk-wasm: host ovum \/in\/reactor-peek-heer\.ovum\.jam committed event=\d+ core=[0-9a-f]+ effects=\d+/,
    );

    for (const effectsPath of [loadEffectsPath, heerEffectsPath]) {
      const effectsBytes = runtime.host.files.get(effectsPath);
      const effects = cue(atomFromBytesLE(effectsBytes));
      assert.ok(effects === 0n || Array.isArray(effects));
      assert.deepEqual(extractMesaEffects(effectsBytes), {
        sends: [],
        pushes: [],
        binds: [],
        unknown: [],
      });
    }

    runtime.shutdown();
    await runtime.save();
    const persisted = await fileStore.load();
    assert.ok(persisted.files.get('/pier/.urb/log/0i0/events.bin')?.length > 0);
  },
);

test(
  'runVereWasmProbe requires a real pill before running Arvo bootstrap',
  {
    skip: existsSync(marsBootWasm)
      ? false
      : 'run zig build mars-boot-wasm to create the artifact',
  },
  async () => {
    let stderr = '';
    const { exitCode } = await runVereWasmProbe(marsBootWasm, {
      args: ['--run-boot'],
      memoryOptions: {
        loomExponent: 24,
        maximumBytes: 128 * 1024 * 1024,
      },
      onStderr: bytes => {
        stderr += textDecoder.decode(bytes).replace(/\r/g, '');
      },
    });

    assert.equal(exitCode, 1);
    assert.match(stderr, /--run-boot requires --real-pill/);
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
