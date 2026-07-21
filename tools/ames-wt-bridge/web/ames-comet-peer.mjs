import { matchCometProofPath, signCometProofPage } from './ames-proof.mjs';
import { decodePact } from './mesa-pact.mjs';

function asPact(packetOrPact) {
  if (packetOrPact instanceof Uint8Array) {
    try {
      return decodePact(packetOrPact);
    }
    catch (_) {
      return null;
    }
  }
  return packetOrPact ?? null;
}

export function matchCometProofPeek(packetOrPact, { comet } = {}) {
  const pact = asPact(packetOrPact);
  if (!pact || pact.type !== 'peek') {
    return null;
  }

  if (comet !== undefined && pact.name.ship !== BigInt(comet)) {
    return null;
  }

  const pathMatch = matchCometProofPath(pact.name.path);
  if (!pathMatch) {
    return null;
  }

  return {
    request: pact,
    ...pathMatch,
  };
}

export async function proofPageForPeek(identity, packetOrPact, { subtle } = {}) {
  const sender = identity.comet ?? identity.ship;
  const match = matchCometProofPeek(packetOrPact, { comet: sender });
  if (!match) {
    return null;
  }

  return {
    match,
    page: await signCometProofPage(identity, {
      receiver: match.receiver,
      receiverPatp: match.receiverPatp,
      receiverLife: match.receiverLife,
      subtle,
    }),
  };
}

export class AmesCometProofResponder {
  constructor({
    identity,
    getIdentity,
    send,
    onPacket = () => {},
    onProof = () => {},
    onError = () => {},
    subtle,
  } = {}) {
    if (typeof send !== 'function') {
      throw new Error('send function is required');
    }

    this.identity = identity;
    this.getIdentity = getIdentity;
    this.send = send;
    this.onPacket = onPacket;
    this.onProof = onProof;
    this.onError = onError;
    this.subtle = subtle;
  }

  setIdentity(identity) {
    this.identity = identity;
  }

  async handlePacket(event) {
    this.onPacket(event);

    const identity = this.getIdentity ? this.getIdentity() : this.identity;
    if (!identity) {
      return null;
    }

    let response;
    try {
      response = await proofPageForPeek(identity, event.packet, { subtle: this.subtle });
    }
    catch (error) {
      this.onError({ type: 'proof-build', error, event });
      return null;
    }

    if (!response) {
      return null;
    }

    try {
      const mode = await this.send(response.page.pagePacket);
      this.onProof({ ...response, mode, event });
      return response;
    }
    catch (error) {
      this.onError({ type: 'proof-send', error, event, response });
      return null;
    }
  }
}
