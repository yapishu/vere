import { createVereWasmMemory } from './vere-wasm-memory.mjs';

const DEFAULT_BOOT_LITE_WASM_URL = new URL(
  '../../../zig-out/bin/noun-boot-lite-wasm.wasm',
  import.meta.url,
);

const textDecoder = new TextDecoder();
const textEncoder = new TextEncoder();

const U3FS_O_CREATE = 0x04;
const U3FS_O_TRUNC = 0x08;
const U3FS_O_EXCL = 0x10;

const WASI_ESUCCESS = 0;
const WASI_EBADF = 8;
const WASI_EINVAL = 28;
const WASI_ENOSYS = 52;
const WASI_ESPIPE = 70;

const WASI_FILETYPE_CHARACTER_DEVICE = 2;
const WASI_FILETYPE_DIRECTORY = 3;
const WASI_PREOPENTYPE_DIR = 0;
const WASI_CLOCKID_REALTIME = 0;
const WASI_CLOCKID_MONOTONIC = 1;

function toSize(value, name) {
  const num = typeof value === 'bigint' ? Number(value) : value;
  if (!Number.isSafeInteger(num) || num < 0) {
    throw new Error(`${name} must be a non-negative safe integer`);
  }
  return num;
}

function asBytes(value) {
  if (value instanceof Uint8Array) {
    return new Uint8Array(value);
  }
  return textEncoder.encode(String(value));
}

function fileEntries(value = {}) {
  if (value instanceof Map) {
    return value.entries();
  }
  return Object.entries(value);
}

function directoryValues(value = []) {
  if (value == null) {
    return [];
  }
  if (typeof value[Symbol.iterator] === 'function' && typeof value !== 'string') {
    return value;
  }
  return Object.values(value);
}

function copyFileMap(value = {}) {
  const files = new Map();
  for (const [path, bytes] of fileEntries(value)) {
    files.set(String(path), asBytes(bytes));
  }
  return files;
}

function copyDirectorySet(value = []) {
  const directories = new Set(['/']);
  for (const path of directoryValues(value)) {
    directories.add(String(path));
  }
  return directories;
}

function copyHostfsSnapshot({ files = {}, directories = [] } = {}) {
  return {
    files: copyFileMap(files),
    directories: copyDirectorySet(directories),
  };
}

function arrayBufferFromBytes(bytes) {
  return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
}

function bytesFromWasmSource(source) {
  if (source instanceof Uint8Array) {
    return source;
  }
  if (source instanceof ArrayBuffer) {
    return new Uint8Array(source);
  }
  if (ArrayBuffer.isView(source)) {
    return new Uint8Array(source.buffer, source.byteOffset, source.byteLength);
  }
  return null;
}

function readCString(memory, ptr) {
  const mem = new Uint8Array(memory.buffer);
  const start = toSize(ptr, 'string pointer');
  let end = start;
  while (end < mem.length && mem[end] !== 0) {
    end++;
  }
  if (end === mem.length) {
    throw new Error('unterminated wasm string');
  }
  return textDecoder.decode(mem.subarray(start, end));
}

function copyOut(memory, ptr, bytes) {
  const mem = new Uint8Array(memory.buffer);
  const start = toSize(ptr, 'buffer pointer');
  const end = start + bytes.length;
  if (end > mem.length) {
    throw new Error('wasm write exceeds memory');
  }
  mem.set(bytes, start);
}

function copyIn(memory, ptr, len) {
  const mem = new Uint8Array(memory.buffer);
  const start = toSize(ptr, 'buffer pointer');
  const end = start + toSize(len, 'buffer length');
  if (end > mem.length) {
    throw new Error('wasm read exceeds memory');
  }
  return new Uint8Array(mem.subarray(start, end));
}

function dataView(memory) {
  return new DataView(memory.buffer);
}

function writeU32(memory, ptr, value) {
  dataView(memory).setUint32(toSize(ptr, 'u32 pointer'), value >>> 0, true);
}

function writeU64(memory, ptr, value) {
  dataView(memory).setBigUint64(
    toSize(ptr, 'u64 pointer'),
    BigInt(value),
    true,
  );
}

function zeroMemory(memory, ptr, len) {
  const start = toSize(ptr, 'zero pointer');
  new Uint8Array(memory.buffer).fill(0, start, start + len);
}

export class VereWasmExit extends Error {
  constructor(code) {
    super(`wasm exited with code ${code}`);
    this.name = 'VereWasmExit';
    this.code = code;
  }
}

export class MemoryVereWasmFileStore {
  constructor(snapshot = {}) {
    this.snapshot = copyHostfsSnapshot(snapshot);
  }

  async load() {
    return copyHostfsSnapshot(this.snapshot);
  }

  async save(snapshot) {
    this.snapshot = copyHostfsSnapshot(snapshot);
  }

  async clear() {
    this.snapshot = copyHostfsSnapshot();
  }
}

export class IndexedDBVereWasmFileStore {
  constructor({
    scope = 'default',
    dbName = 'vere-wasm-hostfs',
    storeName = 'entries',
    indexedDB = globalThis.indexedDB,
  } = {}) {
    if (!indexedDB) {
      throw new Error('IndexedDB is required for browser WASM hostfs storage');
    }
    this.scope = String(scope);
    this.dbName = dbName;
    this.storeName = storeName;
    this.indexedDB = indexedDB;
    this.dbPromise = null;
  }

  open() {
    if (!this.dbPromise) {
      this.dbPromise = new Promise((resolve, reject) => {
        const request = this.indexedDB.open(this.dbName, 1);
        request.onupgradeneeded = () => {
          const db = request.result;
          if (!db.objectStoreNames.contains(this.storeName)) {
            db.createObjectStore(this.storeName, { keyPath: 'key' });
          }
        };
        request.onsuccess = () => resolve(request.result);
        request.onerror = () => reject(request.error);
        request.onblocked = () => reject(new Error('IndexedDB open blocked'));
      });
    }
    return this.dbPromise;
  }

  recordKey(path) {
    return `${this.scope}\0${path}`;
  }

  async load() {
    const db = await this.open();
    const files = new Map();
    const directories = new Set(['/']);

    await new Promise((resolve, reject) => {
      const transaction = db.transaction(this.storeName, 'readonly');
      const store = transaction.objectStore(this.storeName);
      const request = store.openCursor();

      request.onsuccess = () => {
        const cursor = request.result;
        if (!cursor) {
          return;
        }
        const record = cursor.value;
        if (record.scope === this.scope) {
          if (record.type === 'file') {
            files.set(record.path, new Uint8Array(record.bytes ?? new ArrayBuffer(0)));
          } else if (record.type === 'dir') {
            directories.add(record.path);
          }
        }
        cursor.continue();
      };

      transaction.oncomplete = resolve;
      transaction.onerror = () => reject(transaction.error);
      transaction.onabort = () => reject(transaction.error);
    });

    return { files, directories };
  }

  async save(snapshot) {
    const { files, directories } = copyHostfsSnapshot(snapshot);
    await this.clear();

    const db = await this.open();
    await new Promise((resolve, reject) => {
      const transaction = db.transaction(this.storeName, 'readwrite');
      const store = transaction.objectStore(this.storeName);

      for (const path of directories) {
        store.put({
          key: this.recordKey(path),
          scope: this.scope,
          path,
          type: 'dir',
        });
      }
      for (const [path, bytes] of files) {
        store.put({
          key: this.recordKey(path),
          scope: this.scope,
          path,
          type: 'file',
          bytes: arrayBufferFromBytes(bytes),
        });
      }

      transaction.oncomplete = resolve;
      transaction.onerror = () => reject(transaction.error);
      transaction.onabort = () => reject(transaction.error);
    });
  }

  async clear() {
    const db = await this.open();
    await new Promise((resolve, reject) => {
      const transaction = db.transaction(this.storeName, 'readwrite');
      const store = transaction.objectStore(this.storeName);
      const request = store.openCursor();

      request.onsuccess = () => {
        const cursor = request.result;
        if (!cursor) {
          return;
        }
        if (cursor.value.scope === this.scope) {
          cursor.delete();
        }
        cursor.continue();
      };

      transaction.oncomplete = resolve;
      transaction.onerror = () => reject(transaction.error);
      transaction.onabort = () => reject(transaction.error);
    });
  }
}

export function createWasiPreview1Host({
  memory,
  args = [],
  env = {},
  preopens = {},
  onStdout = () => {},
  onStderr = () => {},
  now = () => Date.now(),
  randomFill,
} = {}) {
  const preopenByFd = new Map();
  let nextFd = 3;
  for (const path of Object.keys(preopens)) {
    preopenByFd.set(nextFd++, String(path));
  }

  const randomBytes = randomFill ?? ((bytes) => {
    const crypto = globalThis.crypto;
    if (!crypto?.getRandomValues) {
      throw new Error('crypto.getRandomValues is required for WASI random_get');
    }
    for (let i = 0; i < bytes.length; i += 65536) {
      crypto.getRandomValues(bytes.subarray(i, Math.min(i + 65536, bytes.length)));
    }
  });

  function writeBytesList(iovsPtr, iovsLen, nwrittenPtr, emit) {
    const view = dataView(memory);
    let written = 0;
    const chunks = [];
    for (let i = 0; i < iovsLen; i++) {
      const ptr = view.getUint32(iovsPtr + (i * 8), true);
      const len = view.getUint32(iovsPtr + (i * 8) + 4, true);
      const bytes = copyIn(memory, ptr, len);
      chunks.push(bytes);
      written += bytes.length;
    }
    if (chunks.length) {
      const out = new Uint8Array(written);
      let off = 0;
      for (const chunk of chunks) {
        out.set(chunk, off);
        off += chunk.length;
      }
      emit(out);
    }
    writeU32(memory, nwrittenPtr, written);
    return WASI_ESUCCESS;
  }

  function writeFdstat(fd, ptr) {
    zeroMemory(memory, ptr, 24);
    const view = dataView(memory);
    const filetype = preopenByFd.has(fd)
      ? WASI_FILETYPE_DIRECTORY
      : WASI_FILETYPE_CHARACTER_DEVICE;
    view.setUint8(ptr, filetype);
    writeU64(memory, ptr + 8, 0n);
    writeU64(memory, ptr + 16, 0n);
  }

  function writeFilestat(fd, ptr) {
    zeroMemory(memory, ptr, 64);
    const filetype = preopenByFd.has(fd)
      ? WASI_FILETYPE_DIRECTORY
      : WASI_FILETYPE_CHARACTER_DEVICE;
    dataView(memory).setUint8(ptr + 16, filetype);
  }

  const imports = {
    args_sizes_get(argcPtr, argvBufSizePtr) {
      const argv = args.map(String);
      writeU32(memory, argcPtr, argv.length);
      writeU32(
        memory,
        argvBufSizePtr,
        argv.reduce((sum, arg) => sum + textEncoder.encode(arg).length + 1, 0),
      );
      return WASI_ESUCCESS;
    },

    args_get(argvPtr, argvBufPtr) {
      const view = dataView(memory);
      let off = argvBufPtr;
      args.map(String).forEach((arg, index) => {
        const bytes = textEncoder.encode(`${arg}\0`);
        view.setUint32(argvPtr + (index * 4), off, true);
        copyOut(memory, off, bytes);
        off += bytes.length;
      });
      return WASI_ESUCCESS;
    },

    environ_sizes_get(countPtr, bufSizePtr) {
      const entries = Object.entries(env).map(([key, value]) => `${key}=${value}`);
      writeU32(memory, countPtr, entries.length);
      writeU32(
        memory,
        bufSizePtr,
        entries.reduce((sum, item) => sum + textEncoder.encode(item).length + 1, 0),
      );
      return WASI_ESUCCESS;
    },

    environ_get(environPtr, environBufPtr) {
      const view = dataView(memory);
      let off = environBufPtr;
      Object.entries(env).forEach(([key, value], index) => {
        const bytes = textEncoder.encode(`${key}=${value}\0`);
        view.setUint32(environPtr + (index * 4), off, true);
        copyOut(memory, off, bytes);
        off += bytes.length;
      });
      return WASI_ESUCCESS;
    },

    clock_time_get(clockId, _precision, timePtr) {
      if (
        clockId !== WASI_CLOCKID_REALTIME &&
        clockId !== WASI_CLOCKID_MONOTONIC
      ) {
        return WASI_EINVAL;
      }
      writeU64(memory, timePtr, BigInt(Math.floor(now())) * 1000000n);
      return WASI_ESUCCESS;
    },

    fd_close(fd) {
      return fd >= 0 && fd <= 2 ? WASI_ESUCCESS : WASI_EBADF;
    },

    fd_fdstat_get(fd, statPtr) {
      if (!(fd >= 0 && fd <= 2) && !preopenByFd.has(fd)) {
        return WASI_EBADF;
      }
      writeFdstat(fd, statPtr);
      return WASI_ESUCCESS;
    },

    fd_filestat_get(fd, statPtr) {
      if (!(fd >= 0 && fd <= 2) && !preopenByFd.has(fd)) {
        return WASI_EBADF;
      }
      writeFilestat(fd, statPtr);
      return WASI_ESUCCESS;
    },

    fd_pread() {
      return WASI_EBADF;
    },

    fd_prestat_get(fd, prestatPtr) {
      const path = preopenByFd.get(fd);
      if (path === undefined) {
        return WASI_EBADF;
      }
      zeroMemory(memory, prestatPtr, 8);
      dataView(memory).setUint8(prestatPtr, WASI_PREOPENTYPE_DIR);
      writeU32(memory, prestatPtr + 4, textEncoder.encode(path).length);
      return WASI_ESUCCESS;
    },

    fd_prestat_dir_name(fd, pathPtr, pathLen) {
      const path = preopenByFd.get(fd);
      if (path === undefined) {
        return WASI_EBADF;
      }
      const bytes = textEncoder.encode(path);
      if (bytes.length > pathLen) {
        return WASI_EINVAL;
      }
      copyOut(memory, pathPtr, bytes);
      return WASI_ESUCCESS;
    },

    fd_pwrite() {
      return WASI_EBADF;
    },

    fd_read(fd, _iovsPtr, _iovsLen, nreadPtr) {
      if (fd !== 0) {
        return WASI_EBADF;
      }
      writeU32(memory, nreadPtr, 0);
      return WASI_ESUCCESS;
    },

    fd_seek(fd, _offset, _whence, newOffsetPtr) {
      if (!(fd >= 0 && fd <= 2)) {
        return WASI_EBADF;
      }
      writeU64(memory, newOffsetPtr, 0n);
      return WASI_ESPIPE;
    },

    fd_write(fd, iovsPtr, iovsLen, nwrittenPtr) {
      if (fd === 1) {
        return writeBytesList(iovsPtr, iovsLen, nwrittenPtr, onStdout);
      }
      if (fd === 2) {
        return writeBytesList(iovsPtr, iovsLen, nwrittenPtr, onStderr);
      }
      return WASI_EBADF;
    },

    proc_exit(code) {
      throw new VereWasmExit(toSize(code, 'exit code'));
    },

    random_get(bufPtr, len) {
      randomBytes(new Uint8Array(
        memory.buffer,
        toSize(bufPtr, 'random buffer'),
        toSize(len, 'random length'),
      ));
      return WASI_ESUCCESS;
    },

    path_open() {
      return WASI_ENOSYS;
    },
  };

  return { wasi_snapshot_preview1: imports };
}

export function createVereWasmHost({
  memory,
  initialFiles = {},
  initialDirectories = [],
}) {
  const files = copyFileMap(initialFiles);
  const directories = copyDirectorySet(initialDirectories);
  const handles = new Map();
  let nextHandle = 10;

  function getPath(ptr) {
    return readCString(memory, ptr);
  }

  function getFile(path) {
    return files.get(path);
  }

  function getHandle(handle) {
    const path = handles.get(handle);
    if (path === undefined) {
      return null;
    }
    return path;
  }

  function resizeFile(path, len) {
    const old = getFile(path) ?? new Uint8Array();
    const out = new Uint8Array(len);
    out.set(old.subarray(0, Math.min(old.length, out.length)));
    files.set(path, out);
    return out;
  }

  const env = {
    memory,

    __wasm_setjmp() {
      return 0;
    },

    __wasm_setjmp_test() {
      return 0;
    },

    __wasm_longjmp() {
      throw new Error('wasm longjmp reached');
    },

    u3_wasm_file_mkdir(pathPtr) {
      directories.add(getPath(pathPtr));
      return 0;
    },

    u3_wasm_file_exists(pathPtr) {
      const path = getPath(pathPtr);
      return files.has(path) || directories.has(path) ? 0 : -1;
    },

    u3_wasm_file_open(pathPtr, flags) {
      const path = getPath(pathPtr);
      const exists = files.has(path);
      if ((flags & U3FS_O_EXCL) && (flags & U3FS_O_CREATE) && exists) {
        return -1;
      }
      if (!exists && !(flags & U3FS_O_CREATE)) {
        return -1;
      }
      if (!exists || (flags & U3FS_O_TRUNC)) {
        files.set(path, new Uint8Array());
      }
      const handle = nextHandle++;
      handles.set(handle, path);
      return handle;
    },

    u3_wasm_file_close(handle) {
      return handles.delete(handle) ? 0 : -1;
    },

    u3_wasm_file_handle_size(handle) {
      const path = getHandle(handle);
      if (path === null) {
        return -1n;
      }
      return BigInt(getFile(path)?.length ?? 0);
    },

    u3_wasm_file_resize(handle, len) {
      const path = getHandle(handle);
      if (path === null) {
        return -1;
      }
      resizeFile(path, toSize(len, 'file length'));
      return 0;
    },

    u3_wasm_file_read_at(handle, off, bufPtr, len) {
      const path = getHandle(handle);
      if (path === null) {
        return -1;
      }
      const file = getFile(path) ?? new Uint8Array();
      const start = toSize(off, 'file offset');
      const length = toSize(len, 'read length');
      if (start + length > file.length) {
        return -1;
      }
      copyOut(memory, bufPtr, file.subarray(start, start + length));
      return 0;
    },

    u3_wasm_file_write_at(handle, off, bufPtr, len) {
      const path = getHandle(handle);
      if (path === null) {
        return -1;
      }
      const start = toSize(off, 'file offset');
      const bytes = copyIn(memory, bufPtr, len);
      const file = resizeFile(
        path,
        Math.max(getFile(path)?.length ?? 0, start + bytes.length),
      );
      file.set(bytes, start);
      return 0;
    },

    u3_wasm_file_sync(handle) {
      return getHandle(handle) === null ? -1 : 0;
    },

    u3_wasm_file_unlink(pathPtr) {
      return files.delete(getPath(pathPtr)) ? 0 : -1;
    },

    u3_wasm_file_size(pathPtr) {
      const file = getFile(getPath(pathPtr));
      return file === undefined ? -1n : BigInt(file.length);
    },

    u3_wasm_file_read(pathPtr, bufPtr, len) {
      const file = getFile(getPath(pathPtr));
      const length = toSize(len, 'read length');
      if (file === undefined || length > file.length) {
        return -1;
      }
      copyOut(memory, bufPtr, file.subarray(0, length));
      return 0;
    },

    u3_wasm_file_write(pathPtr, bufPtr, len) {
      files.set(getPath(pathPtr), copyIn(memory, bufPtr, len));
      return 0;
    },
  };

  return { env, files, directories, handles };
}

export async function loadWasmBytes(source) {
  const bytes = bytesFromWasmSource(source);
  if (bytes) {
    return bytes;
  }

  if (typeof fetch === 'function') {
    try {
      const response = await fetch(source);
      if (response.ok) {
        return new Uint8Array(await response.arrayBuffer());
      }
      if (!globalThis.process?.versions?.node) {
        throw new Error(`fetch ${source} failed: ${response.status}`);
      }
    } catch (error) {
      if (!globalThis.process?.versions?.node) {
        throw error;
      }
    }
  }

  if (!globalThis.process?.versions?.node) {
    throw new Error(`cannot load wasm source: ${source}`);
  }

  const { readFile } = await import('node:fs/promises');
  return new Uint8Array(await readFile(source));
}

export async function instantiateVereWasmProbe(
  wasmSource,
  {
    args = [],
    env = {},
    preopens = {},
    initialFiles = {},
    initialDirectories = [],
    memoryOptions = {},
    onStdout = () => {},
    onStderr = () => {},
    now,
    randomFill,
  } = {},
) {
  const bytes = await loadWasmBytes(wasmSource);
  const { memory, plan } = createVereWasmMemory(memoryOptions);
  const host = createVereWasmHost({ memory, initialFiles, initialDirectories });
  const wasi = createWasiPreview1Host({
    memory,
    args,
    env,
    preopens,
    onStdout,
    onStderr,
    now,
    randomFill,
  });
  const imports = {
    ...wasi,
    env: host.env,
  };

  const { instance } = await WebAssembly.instantiate(bytes, imports);
  let exitCode = 0;
  try {
    instance.exports._start();
  } catch (error) {
    if (error instanceof VereWasmExit) {
      exitCode = error.code;
    } else {
      throw error;
    }
  }
  return { exitCode, instance, memory, plan, host };
}

export async function runVereWasmProbe(
  wasmPath = DEFAULT_BOOT_LITE_WASM_URL,
  {
    args = [],
    env = {},
    preopens = {},
    initialFiles = {},
    initialDirectories = [],
    memoryOptions = {},
    onStdout = () => {},
    onStderr = () => {},
  } = {},
) {
  return instantiateVereWasmProbe(wasmPath, {
    args: [String(wasmPath), ...args],
    env,
    preopens,
    initialFiles,
    initialDirectories,
    memoryOptions,
    onStdout,
    onStderr,
  });
}

export async function runVereWasmProbeWithFileStore(
  wasmPath = DEFAULT_BOOT_LITE_WASM_URL,
  {
    fileStore,
    args = [],
    env = {},
    preopens = {},
    initialFiles = {},
    initialDirectories = [],
    memoryOptions = {},
    onStdout = () => {},
    onStderr = () => {},
  } = {},
) {
  if (!fileStore) {
    throw new Error('fileStore is required');
  }

  const loaded = copyHostfsSnapshot(await fileStore.load());
  for (const path of directoryValues(initialDirectories)) {
    loaded.directories.add(String(path));
  }
  for (const [path, bytes] of fileEntries(initialFiles)) {
    loaded.files.set(String(path), asBytes(bytes));
  }

  const result = await instantiateVereWasmProbe(wasmPath, {
    args: [String(wasmPath), ...args],
    env,
    preopens,
    initialFiles: loaded.files,
    initialDirectories: loaded.directories,
    memoryOptions,
    onStdout,
    onStderr,
  });
  await fileStore.save({
    files: result.host.files,
    directories: result.host.directories,
  });
  return result;
}

async function isDirectNodeRun() {
  if (!globalThis.process?.argv?.[1]) {
    return false;
  }
  const { pathToFileURL } = await import('node:url');
  return import.meta.url === pathToFileURL(process.argv[1]).href;
}

if (await isDirectNodeRun()) {
  const { fileURLToPath } = await import('node:url');
  const wasmPath = process.argv[2] ?? fileURLToPath(DEFAULT_BOOT_LITE_WASM_URL);
  const { exitCode, plan } = await runVereWasmProbe(wasmPath, {
    memoryOptions: {
      loomExponent: 24,
      maximumBytes: 128 * 1024 * 1024,
    },
    onStdout: bytes => process.stdout.write(bytes),
    onStderr: bytes => process.stderr.write(bytes),
  });
  console.log(
    `wasm exited ${exitCode}; initial=${plan.initialBytes} max=${plan.maximumBytes}`,
  );
  process.exitCode = exitCode;
}
