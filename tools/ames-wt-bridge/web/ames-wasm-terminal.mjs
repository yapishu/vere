import {
  atomFromBytesLE,
  bytesFromAtomLE,
  cell,
  cue,
  jamBytes,
  list,
  termAtom,
  tuple,
} from './urbit-noun.mjs';

const textDecoder = new TextDecoder();

export const DEFAULT_TERMINAL_ID = '1';

const TERMS = Object.freeze({
  bel: termAtom('bel'),
  belt: termAtom('belt'),
  blew: termAtom('blew'),
  blit: termAtom('blit'),
  born: termAtom('born'),
  clr: termAtom('clr'),
  give: termAtom('give'),
  hail: termAtom('hail'),
  hop: termAtom('hop'),
  klr: termAtom('klr'),
  logo: termAtom('logo'),
  mor: termAtom('mor'),
  nel: termAtom('nel'),
  put: termAtom('put'),
  qit: termAtom('qit'),
  ret: termAtom('ret'),
  term: termAtom('term'),
  txt: termAtom('txt'),
  url: termAtom('url'),
  wyp: termAtom('wyp'),
});

function isCell(noun) {
  return Array.isArray(noun) && noun.length === 2;
}

function decodeEffectsInput(input) {
  if (input instanceof Uint8Array) {
    return cue(atomFromBytesLE(input));
  }
  if (input instanceof ArrayBuffer) {
    return cue(atomFromBytesLE(new Uint8Array(input)));
  }
  if (ArrayBuffer.isView(input)) {
    return cue(atomFromBytesLE(
      new Uint8Array(input.buffer, input.byteOffset, input.byteLength),
    ));
  }
  return input;
}

function listItems(noun, name) {
  const out = [];
  let cur = noun;
  while (cur !== 0n) {
    if (!isCell(cur)) {
      throw new Error(`${name} must be a Hoon list`);
    }
    out.push(cur[0]);
    cur = cur[1];
  }
  return out;
}

function tupleItems(noun, count) {
  const out = [];
  let cur = noun;
  for (let i = 0; i < count - 1; i++) {
    if (!isCell(cur)) {
      return null;
    }
    out.push(cur[0]);
    cur = cur[1];
  }
  out.push(cur);
  return out;
}

function atomText(atom) {
  return textDecoder.decode(bytesFromAtomLE(atom));
}

function terminalIdAtom(id) {
  return atomFromBytesLE(new TextEncoder().encode(String(id)));
}

function charList(text) {
  return list(...[...String(text)].map(char => BigInt(char.codePointAt(0))));
}

function textFromChars(noun) {
  return String.fromCodePoint(...listItems(noun, 'terminal characters').map(Number));
}

function textFromStub(noun) {
  let out = '';
  for (const item of listItems(noun, 'terminal styled text')) {
    if (!isCell(item)) {
      continue;
    }
    out += textFromChars(item[1]);
  }
  return out;
}

function runtimeOvum({ card, id = DEFAULT_TERMINAL_ID }) {
  return cell(
    cell(termAtom('d'), termWire(id)),
    card,
  );
}

function unitList(noun) {
  return noun === 0n ? [] : listItems(noun, 'terminal list');
}

function terminalWireParts(wire) {
  const payload = isCell(wire) && wire[0] === 0n ? wire[1] : wire;
  if (!isCell(payload) || payload[0] !== TERMS.term) {
    return null;
  }
  const tail = listItems(payload[1], 'terminal wire');
  if (tail.length !== 1) {
    return null;
  }
  return {
    id: atomText(tail[0]),
  };
}

function cardInner(card) {
  const give = tupleItems(card, 2);
  if (give?.[0] === TERMS.give) {
    return give[1];
  }
  return card;
}

function decodeBlit(noun, out) {
  if (!isCell(noun)) {
    out.unknown.push({ blit: noun, reason: 'terminal blit is not a cell' });
    return;
  }

  switch (noun[0]) {
    case TERMS.bel:
      out.events.push({ type: 'bell' });
      break;
    case TERMS.clr:
      out.events.push({ type: 'clear' });
      break;
    case TERMS.hop:
      if (typeof noun[1] === 'bigint') {
        out.events.push({ type: 'cursor', col: Number(noun[1]), row: null });
      }
      else if (isCell(noun[1])) {
        out.events.push({ type: 'cursor', col: Number(noun[1][0]), row: Number(noun[1][1]) });
      }
      break;
    case TERMS.klr:
      out.events.push({ type: 'write', text: textFromStub(noun[1]) });
      break;
    case TERMS.mor:
      for (const inner of unitList(noun[1])) {
        decodeBlit(inner, out);
      }
      break;
    case TERMS.nel:
      out.events.push({ type: 'write', text: '\r\n' });
      break;
    case TERMS.put:
      out.events.push({ type: 'write', text: textFromChars(noun[1]) });
      break;
    case TERMS.qit:
      out.events.push({ type: 'quit' });
      break;
    case TERMS.url:
      out.events.push({ type: 'url', url: atomText(noun[1]) });
      break;
    case TERMS.wyp:
      out.events.push({ type: 'write', text: '\r\x1b[K' });
      break;
    default:
      out.unknown.push({ blit: noun, reason: 'unknown terminal blit' });
  }
}

export function termWire(id = DEFAULT_TERMINAL_ID) {
  return cell(TERMS.term, list(terminalIdAtom(id)));
}

export function terminalBornOvumJam({ id = DEFAULT_TERMINAL_ID } = {}) {
  return jamBytes(runtimeOvum({
    id,
    card: cell(TERMS.born, 0n),
  }));
}

export function terminalBlewOvumJam({
  id = DEFAULT_TERMINAL_ID,
  cols = 80,
  rows = 24,
} = {}) {
  return jamBytes(runtimeOvum({
    id,
    card: cell(TERMS.blew, cell(BigInt(cols), BigInt(rows))),
  }));
}

export function terminalHailOvumJam({ id = DEFAULT_TERMINAL_ID } = {}) {
  return jamBytes(runtimeOvum({
    id,
    card: cell(TERMS.hail, 0n),
  }));
}

export function terminalTextOvumJam({
  id = DEFAULT_TERMINAL_ID,
  text,
} = {}) {
  return jamBytes(runtimeOvum({
    id,
    card: cell(TERMS.belt, cell(TERMS.txt, charList(text))),
  }));
}

export function terminalRetOvumJam({ id = DEFAULT_TERMINAL_ID } = {}) {
  return jamBytes(runtimeOvum({
    id,
    card: cell(TERMS.belt, cell(TERMS.ret, 0n)),
  }));
}

export function extractTerminalEffects(input) {
  const effects = decodeEffectsInput(input);
  const out = {
    events: [],
    blits: 0,
    logos: 0,
    unknown: [],
  };

  for (const effect of listItems(effects, 'effects')) {
    if (!isCell(effect)) {
      out.unknown.push({ effect, reason: 'effect is not a cell' });
      continue;
    }
    const wire = terminalWireParts(effect[0]);
    if (!wire) {
      out.unknown.push({ effect, reason: 'not a terminal wire' });
      continue;
    }

    const card = cardInner(effect[1]);
    if (!isCell(card)) {
      out.unknown.push({ effect, wire, reason: 'terminal card is not a cell' });
      continue;
    }

    if (card[0] === TERMS.blit) {
      out.blits++;
      for (const blit of listItems(card[1], 'terminal blits')) {
        decodeBlit(blit, out);
      }
      continue;
    }

    if (card[0] === TERMS.logo) {
      out.logos++;
      out.events.push({ type: 'logo' });
      continue;
    }

    out.unknown.push({ effect, wire, reason: 'not a terminal card' });
  }

  return out;
}

export class BrowserTerminalHost {
  constructor({
    id = DEFAULT_TERMINAL_ID,
    cols = 80,
    rows = 24,
    maxBufferChars = 1_000_000,
    onTerminal = () => {},
  } = {}) {
    this.id = id;
    this.cols = cols;
    this.rows = rows;
    this.maxBufferChars = maxBufferChars;
    this.onTerminal = onTerminal;
    this.started = false;
    this.buffer = '';
    this.blits = 0;
    this.unknown = 0;
  }

  snapshot() {
    return {
      id: this.id,
      started: this.started,
      cols: this.cols,
      rows: this.rows,
      bufferChars: this.buffer.length,
      blits: this.blits,
      unknown: this.unknown,
    };
  }

  bornOvumJam() {
    return terminalBornOvumJam({ id: this.id });
  }

  blewOvumJam({ cols = this.cols, rows = this.rows } = {}) {
    this.cols = Number(cols);
    this.rows = Number(rows);
    return terminalBlewOvumJam({ id: this.id, cols: this.cols, rows: this.rows });
  }

  hailOvumJam() {
    return terminalHailOvumJam({ id: this.id });
  }

  textOvumJam({ text }) {
    return terminalTextOvumJam({ id: this.id, text });
  }

  retOvumJam() {
    return terminalRetOvumJam({ id: this.id });
  }

  routeEffects(input) {
    const effects = extractTerminalEffects(input);
    this.blits += effects.blits;
    this.unknown += effects.unknown.length;

    for (const event of effects.events) {
      if (event.type === 'clear') {
        this.buffer = '';
      }
      else if (event.type === 'write') {
        this.buffer += event.text;
        if (this.buffer.length > this.maxBufferChars) {
          this.buffer = this.buffer.slice(-this.maxBufferChars);
        }
      }

      this.onTerminal(event);
    }

    return {
      events: effects.events.length,
      blits: effects.blits,
      logos: effects.logos,
      unknown: effects.unknown.length,
      bufferChars: this.buffer.length,
    };
  }
}
