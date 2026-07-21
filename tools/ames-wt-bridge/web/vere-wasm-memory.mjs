export const WASM_PAGE_SIZE = 64 * 1024;
export const WASM_MAX_PAGES = 65536;
export const WASM_MAX_BYTES = WASM_PAGE_SIZE * WASM_MAX_PAGES;

export const VERE_WASM_MIN_LOOM_EXPONENT = 20;
export const VERE_WASM_DEFAULT_LOOM_EXPONENT = 28;
export const VERE_WASM_MAX_LOOM_EXPONENT = 31;

export const VERE_WASM_DEFAULT_INITIAL_BYTES = 16 * 1024 * 1024;
export const VERE_WASM_DEFAULT_OVERHEAD_BYTES = 64 * 1024 * 1024;
export const VERE_WASM_DEFAULT_MAX_MEMORY_BYTES = 512 * 1024 * 1024;

function assertSafeBytes(value, name) {
  if (!Number.isSafeInteger(value) || value < 0) {
    throw new Error(`${name} must be a non-negative safe integer byte count`);
  }
  if (value > WASM_MAX_BYTES) {
    throw new Error(`${name} exceeds wasm32 linear memory`);
  }
  return value;
}

function roundToWasmPage(bytes) {
  return Math.ceil(bytes / WASM_PAGE_SIZE) * WASM_PAGE_SIZE;
}

export function wasmPagesForBytes(bytes) {
  const pages = Math.ceil(assertSafeBytes(bytes, 'bytes') / WASM_PAGE_SIZE);
  if (pages > WASM_MAX_PAGES) {
    throw new Error('byte count exceeds wasm32 page limit');
  }
  return pages;
}

export function loomBytesFromExponent(exponent) {
  if (!Number.isInteger(exponent)) {
    throw new Error('loom exponent must be an integer');
  }
  if (
    exponent < VERE_WASM_MIN_LOOM_EXPONENT ||
    exponent > VERE_WASM_MAX_LOOM_EXPONENT
  ) {
    throw new Error(
      `loom exponent must be between ${VERE_WASM_MIN_LOOM_EXPONENT} and ` +
      `${VERE_WASM_MAX_LOOM_EXPONENT}`,
    );
  }
  return 2 ** exponent;
}

export function planVereWasmMemory({
  loomExponent = VERE_WASM_DEFAULT_LOOM_EXPONENT,
  overheadBytes = VERE_WASM_DEFAULT_OVERHEAD_BYTES,
  maximumBytes = VERE_WASM_DEFAULT_MAX_MEMORY_BYTES,
  minimumInitialBytes = VERE_WASM_DEFAULT_INITIAL_BYTES,
  initialBytes,
} = {}) {
  const loomBytes = loomBytesFromExponent(loomExponent);
  const overhead = assertSafeBytes(overheadBytes, 'overheadBytes');
  const maximum = roundToWasmPage(assertSafeBytes(maximumBytes, 'maximumBytes'));
  const minimumInitial = roundToWasmPage(
    assertSafeBytes(minimumInitialBytes, 'minimumInitialBytes'),
  );
  const requiredBytes = roundToWasmPage(loomBytes + overhead);

  if (requiredBytes > maximum) {
    throw new Error(
      `--loom ${loomExponent} needs at least ${requiredBytes} bytes of ` +
      `wasm memory including browser headroom, but maximum is ${maximum}`,
    );
  }

  const requestedInitial = initialBytes === undefined
    ? Math.max(requiredBytes, minimumInitial)
    : assertSafeBytes(initialBytes, 'initialBytes');
  const initial = roundToWasmPage(requestedInitial);
  if (initial < requiredBytes) {
    throw new Error(
      `initial wasm memory ${initial} is smaller than required ` +
      `${requiredBytes} for --loom ${loomExponent}`,
    );
  }
  if (initial > maximum) {
    throw new Error(`initial wasm memory ${initial} exceeds maximum ${maximum}`);
  }

  return {
    loomExponent,
    loomBytes,
    overheadBytes: overhead,
    requiredBytes,
    initialBytes: initial,
    maximumBytes: maximum,
    initialPages: wasmPagesForBytes(initial),
    maximumPages: wasmPagesForBytes(maximum),
    args: ['--loom', String(loomExponent)],
    descriptor: {
      initial: wasmPagesForBytes(initial),
      maximum: wasmPagesForBytes(maximum),
    },
  };
}

export function createVereWasmMemory(options = {}) {
  const plan = planVereWasmMemory(options);
  return {
    plan,
    memory: new WebAssembly.Memory(plan.descriptor),
  };
}
