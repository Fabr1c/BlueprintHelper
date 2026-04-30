/**
 * Bridge TCP Client
 *
 * 连接 UE5 BlueprintHelper Bridge（127.0.0.1:54321）。
 * 帧协议：4-byte big-endian uint32 body length + UTF-8 JSON body
 */

import * as net from 'node:net';

/** Bridge 请求 */
export interface BridgeRequest {
  request_id: string;
  command: string;
  auth_token?: string;
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

/** Bridge 响应 */
export interface BridgeResponse extends Partial<BridgeSafetyResultFields> {
  request_id: string;
  success: boolean;
  error_code?: string;
  message?: string;
  result?: BridgeResult;
}

/** 连接配置 */
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

/**
 * 单次连接 TCP 客户端。
 * 每次 sendCommand 建立新 TCP 连接、发送请求、等待响应后关闭。
 * 简单可靠，适配 MCP tool handler 的 request-response 模式。
 */
export class BridgeClient {
  private readonly opts: Required<BridgeClientOptions>;

  constructor(options?: BridgeClientOptions) {
    this.opts = { ...DEFAULT_OPTIONS, ...options };
  }

  /** 生成递增的 request_id */
  private reqCounter = 0;
  private nextRequestId(): string {
    return `mcp_${++this.reqCounter}_${Date.now()}`;
  }

  /** 发送命令并返回响应 */
  async sendCommand(
    command: string,
    payload: Record<string, unknown> = {},
  ): Promise<BridgeResponse> {
    const requestId = this.nextRequestId();
    const token = process.env.BLUEPRINTHELPER_BRIDGE_TOKEN;
    const request: BridgeRequest = {
      request_id: requestId,
      command,
      payload,
      ...(token ? { auth_token: token } : {}),
    };
    return this.sendRaw(request);
  }

  /** 底层发送：连接 → 写帧 → 读帧 → 关闭 */
  private sendRaw(request: BridgeRequest): Promise<BridgeResponse> {
    return new Promise((resolve, reject) => {
      const socket = new net.Socket();
      let settled = false;

      const settle = (err?: Error, resp?: BridgeResponse) => {
        if (settled) return;
        settled = true;
        socket.destroy();
        if (err) reject(err);
        else resolve(resp!);
      };

      // — 超时 —
      const timer = setTimeout(() => {
        settle(new Error(`Bridge request timed out after ${this.opts.requestTimeoutMs}ms`));
      }, this.opts.requestTimeoutMs);

      socket.setTimeout(this.opts.connectTimeoutMs);

      socket.on('timeout', () => {
        settle(new Error(`Bridge connection timed out after ${this.opts.connectTimeoutMs}ms`));
      });

      socket.on('error', (err: Error) => {
        settle(new Error(`Bridge connection error: ${err.message}`));
      });

      // — 接收缓冲 —
      let recvBuf = Buffer.alloc(0);

      socket.on('data', (chunk: Buffer) => {
        recvBuf = Buffer.concat([recvBuf, chunk]);
        // 尝试读取完整帧
        if (recvBuf.length >= 4) {
          const bodyLen = recvBuf.readUInt32BE(0);
          if (recvBuf.length >= 4 + bodyLen) {
            clearTimeout(timer);
            const bodyStr = recvBuf.subarray(4, 4 + bodyLen).toString('utf-8');
            try {
              const resp = JSON.parse(bodyStr) as BridgeResponse;
              settle(undefined, resp);
            } catch {
              settle(new Error(`Failed to parse Bridge response: ${bodyStr.slice(0, 200)}`));
            }
          }
        }
      });

      // — 连接并写入 —
      socket.connect(this.opts.port, this.opts.host, () => {
        // 连接成功，清除连接超时，切换到请求超时
        socket.setTimeout(0);
        const bodyStr = JSON.stringify(request);
        const bodyBuf = Buffer.from(bodyStr, 'utf-8');
        const header = Buffer.alloc(4);
        header.writeUInt32BE(bodyBuf.length, 0);
        socket.write(Buffer.concat([header, bodyBuf]));
      });
    });
  }

  /** 快速检查 Bridge 是否可达 */
  async ping(): Promise<boolean> {
    try {
      const resp = await this.sendCommand('get_editor_context');
      return resp.success;
    } catch {
      return false;
    }
  }
}
