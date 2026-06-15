#!/usr/bin/env node

const { runWorkflowHook } = require('./workflow-hook-core.cjs');
const process = require('node:process');

async function runCodexHook(input = {}) {
  const payload = isRecord(input.payload) ? input.payload : {};
  const command = readCommand(payload);
  const toolResult = readToolResult(payload);
  const metadata = readMetadata(payload);

  const result = await runWorkflowHook({
    event: input.event,
    command,
    cwd: input.cwd ?? readString(payload.cwd) ?? process.cwd(),
    toolResult,
    metadata,
  });

  return formatHookResult(result);
}

function readCommand(payload) {
  return readString(payload.tool_input?.command)
    ?? readString(payload.toolInput?.command)
    ?? readString(payload.input?.command)
    ?? readString(payload.command);
}

function readToolResult(payload) {
  const toolResult = payload.tool_response
    ?? payload.toolResponse
    ?? payload.tool_output
    ?? payload.toolOutput
    ?? payload.output;
  return isRecord(toolResult) ? toolResult : undefined;
}

function readMetadata(payload) {
  return mergeRecords(
    payload.metadata,
    payload.tool_input?.metadata,
    payload.toolInput?.metadata,
    payload.tool_response?.metadata,
    payload.toolResponse?.metadata,
    payload.tool_output?.metadata,
    payload.toolOutput?.metadata,
  );
}

function formatHookResult(result) {
  if (!isRecord(result) || result.action === 'allow') {
    return { exitCode: 0, stdout: '', stderr: '' };
  }

  if (result.action === 'block') {
    return {
      exitCode: 2,
      stdout: `${JSON.stringify({
        action: 'block',
        reason: result.reason,
        message: result.message,
      })}\n`,
      stderr: `${result.message ?? result.reason ?? 'BlueprintHelper workflow hook blocked the command.'}\n`,
    };
  }

  if (result.action === 'remind') {
    return {
      exitCode: 0,
      stdout: '',
      stderr: `${result.message ?? result.reason ?? 'BlueprintHelper workflow reminder.'}\n`,
    };
  }

  return { exitCode: 0, stdout: '', stderr: '' };
}

async function main(args = process.argv.slice(2), io = {}) {
  const event = readArg(args, '--event') ?? 'PreToolUse';
  const stdin = await readStream(io.stdin ?? process.stdin);
  const payload = parsePayload(stdin);
  const result = await runCodexHook({
    event,
    cwd: io.cwd ?? process.cwd(),
    payload,
  });

  const stdout = io.stdout ?? process.stdout;
  const stderr = io.stderr ?? process.stderr;
  if (result.stdout) {
    stdout.write(result.stdout);
  }
  if (result.stderr) {
    stderr.write(result.stderr);
  }
  process.exitCode = result.exitCode;
  return result;
}

function readArg(args, name) {
  const index = args.indexOf(name);
  if (index === -1) {
    return undefined;
  }
  return args[index + 1];
}

async function readStream(stream) {
  let content = '';
  for await (const chunk of stream) {
    content += chunk.toString('utf8');
  }
  return content;
}

function parsePayload(stdin) {
  if (typeof stdin !== 'string' || stdin.trim().length === 0) {
    return undefined;
  }
  try {
    return JSON.parse(stdin);
  } catch {
    return undefined;
  }
}

function mergeRecords(...records) {
  const result = {};
  for (const record of records) {
    if (isRecord(record)) {
      Object.assign(result, record);
    }
  }
  return Object.keys(result).length > 0 ? result : undefined;
}

function readString(value) {
  return typeof value === 'string' && value.trim().length > 0 ? value.trim() : undefined;
}

function isRecord(value) {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

if (require.main === module) {
  main().catch((error) => {
    const reason = error?.code ?? 'codex_hook_adapter_error';
    const message = error instanceof Error ? error.message : String(error);
    process.stdout.write(`${JSON.stringify({ action: 'block', reason, message })}\n`);
    process.stderr.write(`${message}\n`);
    process.exitCode = 2;
  });
}

module.exports = {
  main,
  runCodexHook,
};
