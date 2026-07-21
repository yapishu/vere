import {
  importSuiteBRing,
  isCometShip,
  shipSponsor,
} from './ames-identity.mjs';
import { bytesToHex, hexToBytes } from './mesa-pact.mjs';

const DEFAULT_DB_NAME = 'ames-wt-bridge';
const DEFAULT_STORE_NAME = 'identities';

function requestPromise(request) {
  return new Promise((resolve, reject) => {
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error ?? new Error('IndexedDB request failed'));
  });
}

function ringBytesFrom(identityOrRingBytes) {
  const ring = identityOrRingBytes?.ringBytes ?? identityOrRingBytes;
  if (!(ring instanceof Uint8Array)) {
    throw new Error('identity ring bytes are required');
  }
  return ring;
}

export class MemoryIdentityBackend {
  constructor(records = []) {
    this.records = new Map(records.map(record => [record.name, { ...record }]));
  }

  async get(name) {
    const record = this.records.get(name);
    return record === undefined ? null : { ...record };
  }

  async set(name, record) {
    this.records.set(name, { ...record, name });
  }

  async delete(name) {
    this.records.delete(name);
  }

  async keys() {
    return Array.from(this.records.keys()).sort();
  }
}

export class IndexedDBIdentityBackend {
  constructor({
    indexedDB = globalThis.indexedDB,
    dbName = DEFAULT_DB_NAME,
    storeName = DEFAULT_STORE_NAME,
  } = {}) {
    if (!indexedDB) {
      throw new Error('IndexedDB is not available');
    }
    this.indexedDB = indexedDB;
    this.dbName = dbName;
    this.storeName = storeName;
    this.dbPromise = null;
  }

  async db() {
    if (this.dbPromise) {
      return this.dbPromise;
    }

    this.dbPromise = new Promise((resolve, reject) => {
      const request = this.indexedDB.open(this.dbName, 1);
      request.onupgradeneeded = () => {
        const db = request.result;
        if (!db.objectStoreNames.contains(this.storeName)) {
          db.createObjectStore(this.storeName, { keyPath: 'name' });
        }
      };
      request.onsuccess = () => resolve(request.result);
      request.onerror = () => reject(request.error ?? new Error('IndexedDB open failed'));
    });
    return this.dbPromise;
  }

  async withStore(mode, fn) {
    const db = await this.db();
    return new Promise((resolve, reject) => {
      const tx = db.transaction(this.storeName, mode);
      const store = tx.objectStore(this.storeName);
      let result;
      tx.oncomplete = () => resolve(result);
      tx.onerror = () => reject(tx.error ?? new Error('IndexedDB transaction failed'));
      tx.onabort = () => reject(tx.error ?? new Error('IndexedDB transaction aborted'));
      try {
        result = fn(store);
      }
      catch (error) {
        tx.abort();
        reject(error);
      }
    });
  }

  async get(name) {
    return this.withStore('readonly', store => requestPromise(store.get(name)));
  }

  async set(name, record) {
    await this.withStore('readwrite', store => {
      requestPromise(store.put({ ...record, name }));
    });
  }

  async delete(name) {
    await this.withStore('readwrite', store => {
      requestPromise(store.delete(name));
    });
  }

  async keys() {
    return this.withStore('readonly', store => requestPromise(store.getAllKeys()));
  }
}

export class AmesIdentityStore {
  constructor({
    backend = new IndexedDBIdentityBackend(),
    clock = () => Date.now(),
  } = {}) {
    this.backend = backend;
    this.clock = clock;
  }

  async save(name, identityOrRingBytes) {
    const ringBytes = ringBytesFrom(identityOrRingBytes);
    const record = {
      name,
      ringHex: bytesToHex(ringBytes),
      updatedAt: this.clock(),
    };
    await this.backend.set(name, record);
    return record;
  }

  async load(name, { subtle } = {}) {
    const record = await this.backend.get(name);
    if (!record) {
      return null;
    }
    const identity = await importSuiteBRing(hexToBytes(record.ringHex), {
      retainRing: true,
      subtle,
    });
    if (isCometShip(identity.ship)) {
      return {
        ...identity,
        comet: identity.ship,
        sponsor: shipSponsor(identity.ship),
      };
    }
    return identity;
  }

  async forget(name) {
    await this.backend.delete(name);
  }

  async list() {
    return this.backend.keys();
  }
}
