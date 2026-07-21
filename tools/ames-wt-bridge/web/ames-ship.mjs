import { murmurHash3x86_32 } from './mesa-pact.mjs';

const PREFIXES = `
dozmarbinwansamlitsighidfidlissogdirwacsabwissib
rigsoldopmodfoglidhopdardorlorhodfolrintogsilmir
holpaslacrovlivdalsatlibtabhanticpidtorbolfosdot
losdilforpilramtirwintadbicdifrocwidbisdasmidlop
rilnardapmolsanlocnovsitnidtipsicropwitnatpanmin
ritpodmottamtolsavposnapnopsomfinfonbanmorworsip
ronnorbotwicsocwatdolmagpicdavbidbaltimtasmallig
sivtagpadsaldivdactansidfabtarmonranniswolmispal
lasdismaprabtobrollatlonnodnavfignomnibpagsopral
bilhaddocridmocpacravripfaltodtiltinhapmicfanpat
taclabmogsimsonpinlomrictapfirhasbosbatpochactid
havsaplindibhosdabbitbarracparloddosbortochilmac
tomdigfilfasmithobharmighinradmashalraglagfadtop
mophabnilnosmilfopfamdatnoldinhatnacrisfotribhoc
nimlarfitwalrapsarnalmoslandondanladdovrivbacpol
laptalpitnambonrostonfodponsovnocsorlavmatmipfip
`.replace(/\s+/g, '').match(/.{1,3}/g);

const SUFFIXES = `
zodnecbudwessevpersutletfulpensytdurwepserwylsun
rypsyxdyrnuphebpeglupdepdysputlughecryttyvsydnex
lunmeplutseppesdelsulpedtemledtulmetwenbynhexfeb
pyldulhetmevruttylwydtepbesdexsefwycburderneppur
rysrebdennutsubpetrulsynregtydsupsemwynrecmegnet
secmulnymtevwebsummutnyxrextebfushepbenmuswyxsym
selrucdecwexsyrwetdylmynmesdetbetbeltuxtugmyrpel
syptermebsetdutdegtexsurfeltudnuxruxrenwytnubmed
lytdusnebrumtynseglyxpunresredfunrevrefmectedrus
bexlebduxrynnumpyxrygryxfeptyrtustyclegnemfermer
tenlusnussyltecmexpubrymtucfyllepdebbermughuttun
bylsudpemdevlurdefbusbeprunmelpexdytbyttyplevmyl
wedducfurfexnulluclennerlexrupnedlecrydlydfenwel
nydhusrelrudneshesfetdesretdunlernyrsebhulryllud
remlysfynwerrycsugnysnyllyndyndemluxfedsedbecmun
lyrtesmudnytbyrsenwegfyrmurtelreptegpecnelnevfes
`.replace(/\s+/g, '').match(/.{1,3}/g);

if (PREFIXES.length !== 256 || SUFFIXES.length !== 256) {
  throw new Error('invalid @p syllable table');
}

const MASK_32 = 0xffff_ffffn;
const MASK_64 = 0xffff_ffff_ffff_ffffn;
const HI_64 = 0xffff_ffff_0000_0000n;

function asAtom(value) {
  const atom = BigInt(value);
  if (atom < 0n) {
    throw new Error('ship atom must be non-negative');
  }
  return atom;
}

function bex(n) {
  return 1n << BigInt(n);
}

function rsh(blockBitsExponent, blocks, value) {
  return value >> (bex(blockBitsExponent) * BigInt(blocks));
}

function end(blockBitsExponent, blocks, value) {
  return value & ((1n << (bex(blockBitsExponent) * BigInt(blocks))) - 1n);
}

function met(blockBitsExponent, value) {
  let atom = asAtom(value);
  if (atom === 0n) {
    return 0n;
  }

  const shift = bex(blockBitsExponent);
  let out = 0n;
  while (atom > 0n) {
    out++;
    atom >>= shift;
  }
  return out;
}

function muk(seed, atom) {
  const lo = Number(atom & 0xffn);
  const hi = Number((atom >> 8n) & 0xffn);
  return BigInt(murmurHash3x86_32(Uint8Array.of(lo, hi), seed));
}

function feistelF(round, atom) {
  const seeds = [0xb76d5eed, 0xee281300, 0x85bcae01, 0x4b387af7];
  return muk(seeds[round], atom);
}

function fe(r, a, b, f, m) {
  function loop(j, left, right) {
    if (j > r) {
      if ((r % 2) !== 0) {
        return (a * right) + left;
      }
      return right === a ? (a * right) + left : (a * left) + right;
    }

    const eff = f(j - 1, right);
    const tmp = (j % 2) !== 0
      ? (left + eff) % a
      : (left + eff) % b;
    return loop(j + 1, right, tmp);
  }

  return loop(1, m % a, m / a);
}

function feis(atom) {
  const out = fe(4, 65_535n, 65_536n, feistelF, atom);
  return out < MASK_32 ? out : fe(4, 65_535n, 65_536n, feistelF, out);
}

function fen(r, a, b, f, m) {
  function loop(j, left, right) {
    if (j < 1) {
      return (a * right) + left;
    }

    const eff = f(j - 1, left);
    const tmp = (j % 2) !== 0
      ? (right + a - (eff % a)) % a
      : (right + b - (eff % b)) % b;
    return loop(j - 1, tmp, left);
  }

  const high = (r % 2) !== 0 ? m / a : m % a;
  const low = (r % 2) !== 0 ? m % a : m / a;
  const left = low === a ? high : low;
  const right = low === a ? low : high;
  return loop(r, left, right);
}

function tail(atom) {
  const out = fen(4, 65_535n, 65_536n, feistelF, atom);
  return out < MASK_32 ? out : fen(4, 65_535n, 65_536n, feistelF, out);
}

function fein(value) {
  const atom = asAtom(value);
  const lo = atom & MASK_32;
  const hi = atom & HI_64;

  if (atom >= 0x1_0000n && atom <= MASK_32) {
    return 0x1_0000n + feis(atom - 0x1_0000n);
  }
  if (atom >= 0x1_0000_0000n && atom <= MASK_64) {
    return hi | fein(lo);
  }
  return atom;
}

function fynd(value) {
  const atom = asAtom(value);
  const lo = atom & MASK_32;
  const hi = atom & HI_64;

  if (atom >= 0x1_0000n && atom <= MASK_32) {
    return 0x1_0000n + tail(atom - 0x1_0000n);
  }
  if (atom >= 0x1_0000_0000n && atom <= MASK_64) {
    return hi | fynd(lo);
  }
  return atom;
}

export function patp(value) {
  const concealed = fein(value);
  const wordCount = met(4, concealed);

  if (met(3, concealed) <= 1n) {
    return `~${SUFFIXES[Number(concealed)]}`;
  }

  function loop(rest, index, rendered) {
    const syllable = end(4, 1, rest);
    const prefix = PREFIXES[Number(rsh(3, 1, syllable))];
    const suffix = SUFFIXES[Number(end(3, 1, syllable))];
    const sep = (index % 4n) === 0n
      ? (index === 0n ? '' : '--')
      : '-';
    const next = `${prefix}${suffix}${sep}${rendered}`;

    return index === wordCount ? rendered : loop(rsh(4, 1, rest), index + 1n, next);
  }

  return `~${loop(concealed, 0n, '')}`;
}

function syllableIndex(table, syllable) {
  return table.indexOf(syllable);
}

function parsePatpWord(word) {
  if (word.length !== 6) {
    throw new Error('invalid patp word length');
  }

  const prefix = syllableIndex(PREFIXES, word.slice(0, 3));
  const suffix = syllableIndex(SUFFIXES, word.slice(3, 6));
  if (prefix < 0 || suffix < 0) {
    throw new Error('invalid patp syllable');
  }
  return (BigInt(prefix) << 8n) | BigInt(suffix);
}

function parsePatpWordGroup(group, {
  allowSuffixOnly = false,
  exactWords,
} = {}) {
  if (group.length === 0) {
    throw new Error('invalid patp separator');
  }

  if (allowSuffixOnly && !group.includes('-') && group.length === 3) {
    const suffix = syllableIndex(SUFFIXES, group);
    if (suffix >= 0) {
      return { suffixOnly: true, value: BigInt(suffix) };
    }
    if (syllableIndex(PREFIXES, group) >= 0) {
      throw new Error('patp prefix syllable cannot stand alone');
    }
    throw new Error('invalid patp syllable');
  }

  const words = group.split('-');
  if (words.some(word => word.length === 0)) {
    throw new Error('invalid patp separator');
  }
  if (exactWords !== undefined && words.length !== exactWords) {
    throw new Error('invalid patp word group length');
  }
  if (exactWords === undefined && (words.length < 1 || words.length > 4)) {
    throw new Error('invalid patp word group length');
  }

  return {
    suffixOnly: false,
    words: words.map(parsePatpWord),
  };
}

function wordsToAtom(words) {
  let out = 0n;
  for (const word of words) {
    out = (out << 16n) | word;
  }
  return out;
}

export function patpToAtom(name) {
  if (typeof name !== 'string' || !name.startsWith('~')) {
    throw new Error('patp must start with ~');
  }

  const text = name.slice(1);
  if (text.length === 0 || !/^[a-z-]+$/.test(text)) {
    throw new Error('invalid patp text');
  }

  const doubleGroups = text.split('--');
  if (doubleGroups.some(group => group.length === 0)) {
    throw new Error('invalid patp separator');
  }

  let concealed;
  if (doubleGroups.length === 1) {
    const group = parsePatpWordGroup(doubleGroups[0], { allowSuffixOnly: true });
    concealed = group.suffixOnly ? group.value : wordsToAtom(group.words);
  }
  else {
    const words = [];
    for (let i = 0; i < doubleGroups.length; i++) {
      const group = parsePatpWordGroup(doubleGroups[i], {
        exactWords: i === 0 ? undefined : 4,
      });
      if (group.suffixOnly) {
        throw new Error('invalid patp word group length');
      }
      words.push(...group.words);
    }
    concealed = wordsToAtom(words);
  }

  return fynd(concealed);
}

export function decimalScot(value) {
  return asAtom(value).toString(10);
}
