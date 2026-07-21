function asBytes(packet) {
  return packet instanceof Uint8Array ? packet : Uint8Array.from(packet);
}

async function sendDatagram(wt, packet) {
  const writer = wt.datagrams.writable.getWriter();
  try {
    await writer.write(packet);
  }
  finally {
    writer.releaseLock();
  }
}

async function sendStream(wt, packet) {
  const stream = await wt.createUnidirectionalStream();
  const writer = stream.getWriter();
  try {
    await writer.write(packet);
    await writer.close();
  }
  finally {
    writer.releaseLock();
  }
}

export async function sendPacket(wt, packet) {
  const bytes = asBytes(packet);
  const maxDatagramSize = wt.datagrams?.maxDatagramSize;

  if ((typeof maxDatagramSize !== 'number') || (bytes.length <= maxDatagramSize)) {
    try {
      await sendDatagram(wt, bytes);
      return 'datagram';
    }
    catch (err) {
      if ((typeof maxDatagramSize === 'number') && (bytes.length <= maxDatagramSize)) {
        throw err;
      }
    }
  }

  await sendStream(wt, bytes);
  return 'stream';
}
