/**
 * Bridge TCP Client
 *
 * Connects to the UE BlueprintHelper Bridge.
 * Frame protocol: 4-byte big-endian uint32 body length + UTF-8 JSON body.
 */

import * as net from 'node:net';

export interface BridgeRequest {
  request_id: string;
  command: string;
  auth_session?: string;
  payload: Record<string, unknown>;
}

/** Optional safety/result fields returned by newer Bridge commands. */
export interface BridgeSafetyResultFields {
  effective_scope?: string;
  status?: string;
  operations_applied?: number;
  nodes_created?: number;
  links_connected?: number;
  warnings?: unknown[];
  errors?: unknown[];
  rolled_back?: boolean;
}

export type BridgeResult = Record<string, unknown> & Partial<BridgeSafetyResultFields>;

export interface BridgeResponse extends Partial<BridgeSafetyResultFields> {
  request_id: string;
  success: boolean;
  error_code?: string;
  message?: string;
  result?: BridgeResult;
}

export interface BridgeClientOptions {
  host?: string;
  port?: number;
  connectTimeoutMs?: number;
  requestTimeoutMs?: number;
}

const DEFAULT_OPTIONS: Required<BridgeClientOptions> = {
  host: '127.0.0.1',
  port: 54321,
  connectTimeoutMs: 5000,
  requestTimeoutMs: 30000,
};

const MAX_FRAME_BYTES = 16 * 1024 * 1024;

type PendingBridgeResponse = {
  resolve: (response: BridgeResponse) => void;
  reject: (err: Error) => void;
  timer: ReturnType<typeof setTimeout>;
};

/**
 * Persistent TCP client for the UE Bridge length-prefixed JSON protocol.
 * Reuses one socket across MCP tool calls and reconnects after transport failure.
 */
export class BridgeClient {
  private readonly opts: Required<BridgeClientOptions>;
  private reqCounter = 0;
  private socket: net.Socket | undefined;
  private connectPromise: Promise<net.Socket> | undefined;
  private recvBuf = Buffer.alloc(0);
  private readonly pending = new Map<string, PendingBridgeResponse>();
  private writeSessionId: string | undefined;

  constructor(options?: BridgeClientOptions) {
    this.opts = { ...DEFAULT_OPTIONS, ...options };
  }

  private nextRequestId(): string {
    return `mcp_${++this.reqCounter}_${Date.now()}`;
  }

  async sendCommand(
    command: string,
    payload: Record<string, unknown> = {},
  ): Promise<BridgeResponse> {
    const requestId = this.nextRequestId();
    const request: BridgeRequest = {
      request_id: requestId,
      command,
      payload,
      ...(this.writeSessionId ? { auth_session: this.writeSessionId } : {}),
    };
    return this.sendRaw(request);
  }

  setWriteSessionId(sessionId: string): void {
    this.writeSessionId = sessionId;
  }

  clearWriteSessionId(): void {
    this.writeSessionId = undefined;
  }

  close(): void {
    this.resetSocket(new Error('Bridge connection closed'));
  }

  private async sendRaw(request: BridgeRequest): Promise<BridgeResponse> {
    const socket = await this.ensureConnected();

    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(request.request_id);
        const err = new Error(`Bridge request timed out after ${this.opts.requestTimeoutMs}ms`);
        reject(err);
        this.resetSocket(err);
      }, this.opts.requestTimeoutMs);

      this.pending.set(request.request_id, { resolve, reject, timer });

      const failWrite = (err: Error) => {
        const pending = this.pending.get(request.request_id);
        if (!pending) return;
        clearTimeout(pending.timer);
        this.pending.delete(request.request_id);
        pending.reject(err);
        this.resetSocket(err);
      };

      try {
        socket.write(this.encodeRequest(request), (err?: Error | null) => {
          if (err) {
            failWrite(new Error(`Bridge connection error: ${err.message}`));
          }
        });
      } catch (err) {
        failWrite(err instanceof Error ? err : new Error(String(err)));
      }
    });
  }

  private ensureConnected(): Promise<net.Socket> {
    if (this.socket && this.isSocketUsable(this.socket)) {
      return Promise.resolve(this.socket);
    }
    if (this.socket) {
      this.resetSocket(new Error('Bridge connection is no longer writable'));
    }
    if (this.connectPromise) {
      return this.connectPromise;
    }

    this.connectPromise = new Promise<net.Socket>((resolve, reject) => {
      const socket = new net.Socket();
      let settled = false;

      const cleanupConnectHandlers = () => {
        clearTimeout(timer);
        socket.off('connect', onConnect);
        socket.off('error', onError);
      };

      const fail = (err: Error) => {
        if (settled) return;
        settled = true;
        cleanupConnectHandlers();
        socket.destroy();
        reject(err);
      };

      const onError = (err: Error) => {
        fail(new Error(`Bridge connection error: ${err.message}`));
      };

      const onConnect = () => {
        if (settled) return;
        settled = true;
        cleanupConnectHandlers();
        socket.setNoDelay(true);
        socket.setKeepAlive(true);
        this.socket = socket;
        this.recvBuf = Buffer.alloc(0);
        this.attachSocketHandlers(socket);
        resolve(socket);
      };

      const timer = setTimeout(() => {
        fail(new Error(`Bridge connection timed out after ${this.opts.connectTimeoutMs}ms`));
      }, this.opts.connectTimeoutMs);

      socket.once('connect', onConnect);
      socket.once('error', onError);
      socket.connect(this.opts.port, this.opts.host);
    }).finally(() => {
      this.connectPromise = undefined;
    });

    return this.connectPromise;
  }

  private attachSocketHandlers(socket: net.Socket): void {
    socket.on('data', (chunk: Buffer) => {
      if (this.socket !== socket) return;
      this.handleData(chunk);
    });

    socket.on('error', (err: Error) => {
      if (this.socket !== socket) return;
      this.resetSocket(new Error(`Bridge connection error: ${err.message}`));
    });

    socket.on('end', () => {
      if (this.socket !== socket) return;
      this.resetSocket(new Error('Bridge connection ended'));
    });

    socket.on('close', () => {
      if (this.socket !== socket) return;
      this.resetSocket(new Error('Bridge connection closed'));
    });
  }

  private isSocketUsable(socket: net.Socket): boolean {
    return !socket.destroyed
      && socket.writable
      && !socket.writableEnded
      && socket.readyState === 'open';
  }

  private handleData(chunk: Buffer): void {
    this.recvBuf = Buffer.concat([this.recvBuf, chunk]);

    while (this.recvBuf.length >= 4) {
      const bodyLen = this.recvBuf.readUInt32BE(0);
      if (bodyLen === 0 || bodyLen > MAX_FRAME_BYTES) {
        this.resetSocket(new Error(`Invalid Bridge response frame length: ${bodyLen}`));
        return;
      }

      if (this.recvBuf.length < 4 + bodyLen) {
        return;
      }

      const bodyStr = this.recvBuf.subarray(4, 4 + bodyLen).toString('utf-8');
      this.recvBuf = this.recvBuf.subarray(4 + bodyLen);

      let resp: BridgeResponse;
      try {
        resp = JSON.parse(bodyStr) as BridgeResponse;
      } catch {
        this.resetSocket(new Error(`Failed to parse Bridge response: ${bodyStr.slice(0, 200)}`));
        return;
      }

      const pending = this.pending.get(resp.request_id);
      if (!pending) {
        continue;
      }

      clearTimeout(pending.timer);
      this.pending.delete(resp.request_id);
      pending.resolve(resp);
    }
  }

  private encodeRequest(request: BridgeRequest): Buffer {
    const bodyBuf = Buffer.from(JSON.stringify(request), 'utf-8');
    const header = Buffer.alloc(4);
    header.writeUInt32BE(bodyBuf.length, 0);
    return Buffer.concat([header, bodyBuf]);
  }

  private resetSocket(err: Error): void {
    const socket = this.socket;
    this.socket = undefined;
    this.recvBuf = Buffer.alloc(0);

    if (socket && !socket.destroyed) {
      socket.destroy();
    }

    for (const pending of this.pending.values()) {
      clearTimeout(pending.timer);
      pending.reject(err);
    }
    this.pending.clear();
  }

  async ping(): Promise<boolean> {
    try {
      const resp = await this.sendCommand('get_editor_context');
      return resp.success;
    } catch {
      return false;
    }
  }
}
