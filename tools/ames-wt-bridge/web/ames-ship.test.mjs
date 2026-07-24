import assert from 'node:assert/strict';
import test from 'node:test';

import {
  patp,
  patpToAtom,
  shipClass,
  shipSponsor,
} from './ames-ship.mjs';

test('patp renders Hoon scot %p vectors', () => {
  const vectors = [
    [0n, '~zod'],
    [1n, '~nec'],
    [0x100n, '~marzod'],
    [
      0x8464_c4fd_83ad_dcf0_ad56_7d1b_c5d2_0773n,
      '~tobsyr-mitnel-rabmug-rislyr--pocmut-wolhec-hobrel-hidset',
    ],
  ];

  for (const [atom, name] of vectors) {
    assert.equal(patp(atom), name);
    assert.equal(patpToAtom(name), atom);
  }
});

test('patpToAtom parses Hoon slaw %p aliases', () => {
  assert.equal(patpToAtom('~zod'), 0n);
  assert.equal(patpToAtom('~dozzod'), 0n);
  assert.equal(patpToAtom('~dozzod-dozzod'), 0n);
  assert.equal(patpToAtom('~marnec'), 257n);
});

test('shipClass and shipSponsor handle a moon entered as @p', () => {
  const moon = patpToAtom('~natnup-sigter-sitful-hatred');
  assert.equal(shipClass(moon), 'moon');
  assert.equal(shipSponsor(moon), moon & 0xffff_ffffn);
  assert.equal(patp(shipSponsor(moon)), '~sitful-hatred');
});

test('patp rejects negative atoms', () => {
  assert.throws(() => patp(-1n), /non-negative/);
});

test('patpToAtom rejects malformed names', () => {
  assert.throws(() => patpToAtom('zod'), /start with ~/);
  assert.throws(() => patpToAtom('~doz'), /stand alone/);
  assert.throws(() => patpToAtom('~mar-nec'), /word length/);
  assert.throws(() => patpToAtom('~not-a-ship'), /word length/);
  assert.throws(
    () => patpToAtom('~tobsyr-mitnel-rabmug-rislyr-pocmut-wolhec-hobrel-hidset'),
    /word group length/,
  );
});
