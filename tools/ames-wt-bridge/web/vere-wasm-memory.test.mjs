import assert from 'node:assert/strict';
import test from 'node:test';

import {
  VERE_WASM_DEFAULT_LOOM_EXPONENT,
  VERE_WASM_DEFAULT_MAX_MEMORY_BYTES,
  VERE_WASM_DEFAULT_OVERHEAD_BYTES,
  WASM_PAGE_SIZE,
  createVereWasmMemory,
  loomBytesFromExponent,
  planVereWasmMemory,
  wasmPagesForBytes,
} from './vere-wasm-memory.mjs';

test('loomBytesFromExponent follows Vere byte-exponent semantics', () => {
  assert.equal(loomBytesFromExponent(20), 1024 * 1024);
  assert.equal(loomBytesFromExponent(28), 256 * 1024 * 1024);
  assert.equal(loomBytesFromExponent(31), 2 * 1024 * 1024 * 1024);
});

test('loomBytesFromExponent rejects exponents outside the wasm Vere range', () => {
  assert.throws(() => loomBytesFromExponent(19), /between 20 and 31/);
  assert.throws(() => loomBytesFromExponent(32), /between 20 and 31/);
  assert.throws(() => loomBytesFromExponent(28.5), /integer/);
});

test('wasmPagesForBytes rounds up to 64KB pages', () => {
  assert.equal(wasmPagesForBytes(0), 0);
  assert.equal(wasmPagesForBytes(1), 1);
  assert.equal(wasmPagesForBytes(WASM_PAGE_SIZE), 1);
  assert.equal(wasmPagesForBytes(WASM_PAGE_SIZE + 1), 2);
});

test('default browser memory plan preallocates the wasm loom plus headroom', () => {
  const plan = planVereWasmMemory();

  assert.equal(plan.loomExponent, VERE_WASM_DEFAULT_LOOM_EXPONENT);
  assert.equal(plan.loomBytes, 256 * 1024 * 1024);
  assert.equal(plan.overheadBytes, VERE_WASM_DEFAULT_OVERHEAD_BYTES);
  assert.equal(plan.initialBytes, 320 * 1024 * 1024);
  assert.equal(plan.maximumBytes, VERE_WASM_DEFAULT_MAX_MEMORY_BYTES);
  assert.deepEqual(plan.args, ['--loom', '28']);
  assert.deepEqual(plan.descriptor, { initial: 5120, maximum: 8192 });
});

test('default browser maximum rejects native Vere default --loom 31', () => {
  assert.throws(
    () => planVereWasmMemory({ loomExponent: 31 }),
    /--loom 31 needs at least/,
  );
});

test('larger explicit maximum can support a larger loom', () => {
  const plan = planVereWasmMemory({
    loomExponent: 29,
    maximumBytes: 768 * 1024 * 1024,
  });

  assert.equal(plan.loomBytes, 512 * 1024 * 1024);
  assert.equal(plan.initialBytes, 576 * 1024 * 1024);
  assert.equal(plan.maximumPages, 12288);
});

test('initial memory cannot be smaller than the loom plus headroom', () => {
  assert.throws(
    () => planVereWasmMemory({
      loomExponent: 28,
      initialBytes: 300 * 1024 * 1024,
    }),
    /smaller than required/,
  );
});

test('createVereWasmMemory instantiates a planned bounded memory', () => {
  const { memory, plan } = createVereWasmMemory({
    loomExponent: 20,
    overheadBytes: 64 * 1024,
    maximumBytes: 2 * 1024 * 1024,
    minimumInitialBytes: 0,
  });

  assert.equal(plan.initialPages, 17);
  assert.equal(memory.buffer.byteLength, 17 * WASM_PAGE_SIZE);
});
