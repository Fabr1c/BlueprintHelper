import assert from 'node:assert/strict';
import * as net from 'node:net';
import test from 'node:test';
import { BridgeClient } from '../../bridge/bridge-client.js';
import { TaskTimingTrace } from '../../task/service/task-timing.js';

type ParsedBridgeRequest = {
  request_id?: string;
  command?: string;
  auth_session?: string;
};

type TestBridgeServer = {
  port: number;
  connectionCount: () => number;
  commands: () => string[];
  requests: () => ParsedBridgeRequest[];
  close: () => Promise<void>;
};

type TestBridgeServerOptions = {
  closeSocketAfterResponse?: (command: string) => boolean;
  endSocketAfterResponse?: (command: string) => boolean;
};

function encodeFrame(payload: Record<string, unknown>): Buffer {
  const body = Buffer.from(JSON.stringify(payload), 'utf8');
  const header = Buffer.alloc(4);
  header.writeUInt32BE(body.length, 0);
  return Buffer.concat([header, body]);
}

async function startTestBridgeServer(options: TestBridgeServerOptions = {}): Promise<TestBridgeServer> {
  const sockets = new Set<net.Socket>();
  const seenCommands: string[] = [];
  const seenRequests: ParsedBridgeRequest[] = [];
  let connections = 0;

  const server = net.createServer((socket) => {
    connections += 1;
    sockets.add(socket);
    let recvBuf = Buffer.alloc(0);

    socket.on('data', (chunk: Buffer) => {
      recvBuf = Buffer.concat([recvBuf, chunk]);

      while (recvBuf.length >= 4) {
        const bodyLen = recvBuf.readUInt32BE(0);
        if (recvBuf.length < 4 + bodyLen) {
          return;
        }

        const body = recvBuf.subarray(4, 4 + bodyLen).toString('utf8');
        recvBuf = recvBuf.subarray(4 + bodyLen);
        const request = JSON.parse(body) as ParsedBridgeRequest;
        seenRequests.push(request);
        seenCommands.push(request.command ?? '');
        socket.write(encodeFrame({
          request_id: request.request_id,
          success: true,
          result: { command: request.command },
        }), () => {
          const command = request.command ?? '';
          if (options.closeSocketAfterResponse?.(command)) {
            socket.destroy();
          } else if (options.endSocketAfterResponse?.(command)) {
            socket.end();
          }
        });
      }
    });

    socket.on('close', () => {
      sockets.delete(socket);
    });
  });

  await new Promise<void>((resolve) => {
    server.listen(0, '127.0.0.1', resolve);
  });

  const address = server.address();
  assert.ok(address && typeof address === 'object');

  return {
    port: address.port,
    connectionCount: () => connections,
    commands: () => [...seenCommands],
    requests: () => [...seenRequests],
    close: async () => {
      for (const socket of sockets) {
        socket.destroy();
      }
      await new Promise<void>((resolve, reject) => {
        server.close((err) => {
          if (err) reject(err);
          else resolve();
        });
      });
    },
  };
}

test('BridgeClient reuses one TCP connection for sequential commands', async () => {
  const server = await startTestBridgeServer();
  const client = new BridgeClient({
    host: '127.0.0.1',
    port: server.port,
    connectTimeoutMs: 1000,
    requestTimeoutMs: 1000,
  });

  try {
    const first = await client.sendCommand('first');
    const second = await client.sendCommand('second');

    assert.equal(first.success, true);
    assert.equal(second.success, true);
    assert.deepEqual(server.commands(), ['first', 'second']);
    assert.equal(server.connectionCount(), 1);
  } finally {
    client.close();
    await server.close();
  }
});

test('BridgeClient attaches approved write session to subsequent Bridge requests', async () => {
  const server = await startTestBridgeServer();
  const client = new BridgeClient({
    host: '127.0.0.1',
    port: server.port,
    connectTimeoutMs: 1000,
    requestTimeoutMs: 1000,
  });

  try {
    client.setWriteSessionId('session-test-123');
    const response = await client.sendCommand('import_json');

    assert.equal(response.success, true);
    assert.equal(server.requests()[0]?.auth_session, 'session-test-123');
  } finally {
    client.close();
    await server.close();
  }
});

test('BridgeClient shares one TCP connection for concurrent commands', async () => {
  const server = await startTestBridgeServer();
  const client = new BridgeClient({
    host: '127.0.0.1',
    port: server.port,
    connectTimeoutMs: 1000,
    requestTimeoutMs: 1000,
  });

  try {
    const [first, second] = await Promise.all([
      client.sendCommand('first'),
      client.sendCommand('second'),
    ]);

    assert.equal(first.success, true);
    assert.equal(second.success, true);
    assert.deepEqual(server.commands().sort(), ['first', 'second']);
    assert.equal(server.connectionCount(), 1);
  } finally {
    client.close();
    await server.close();
  }
});

test('BridgeClient reconnects after the Bridge closes the persistent connection', async () => {
  const server = await startTestBridgeServer({
    closeSocketAfterResponse: (command) => command === 'first',
  });
  const client = new BridgeClient({
    host: '127.0.0.1',
    port: server.port,
    connectTimeoutMs: 1000,
    requestTimeoutMs: 1000,
  });

  try {
    const first = await client.sendCommand('first');
    await new Promise((resolve) => setTimeout(resolve, 20));
    const second = await client.sendCommand('second');

    assert.equal(first.success, true);
    assert.equal(second.success, true);
    assert.deepEqual(server.commands(), ['first', 'second']);
    assert.equal(server.connectionCount(), 2);
  } finally {
    client.close();
    await server.close();
  }
});

test('BridgeClient reconnects after the Bridge half-closes an idle persistent connection', async () => {
  const server = await startTestBridgeServer({
    endSocketAfterResponse: (command) => command === 'first',
  });
  const client = new BridgeClient({
    host: '127.0.0.1',
    port: server.port,
    connectTimeoutMs: 1000,
    requestTimeoutMs: 1000,
  });

  try {
    const first = await client.sendCommand('first');
    await new Promise((resolve) => setTimeout(resolve, 20));
    const second = await client.sendCommand('second');

    assert.equal(first.success, true);
    assert.equal(second.success, true);
    assert.deepEqual(server.commands(), ['first', 'second']);
    assert.equal(server.connectionCount(), 2);
  } finally {
    client.close();
    await server.close();
  }
});

test('BridgeClient records transport timing stages when a timing trace is supplied', async () => {
  const server = await startTestBridgeServer();
  const client = new BridgeClient({
    host: '127.0.0.1',
    port: server.port,
    connectTimeoutMs: 1000,
    requestTimeoutMs: 1000,
  });
  const timing = TaskTimingTrace.start('bridge_client_transport_test', 'agentface_test');

  try {
    const response = await client.sendCommand('timed_command', {}, {
      timing,
      timingPrefix: 'read_context.bridge_transport',
    });

    assert.equal(response.success, true);
    const stageNames = timing.snapshot().stages.map((stage) => stage.name);
    assert.ok(stageNames.includes('read_context.bridge_transport.connect'));
    assert.ok(stageNames.includes('read_context.bridge_transport.write'));
    assert.ok(stageNames.includes('read_context.bridge_transport.client_parse'));
  } finally {
    client.close();
    await server.close();
  }
});
