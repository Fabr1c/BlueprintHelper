const fs = require('node:fs');
const path = require('node:path');

const LEDGER_SCHEMA = 'BlueprintHelper.HookLedger.v1';
const MAX_ATTEMPTS = 3;

function classifyCommand(command) {
  const tokens = tokenize(command);
  if (tokens.length < 3 || !isBhCommand(tokens[0])) {
    return undefined;
  }

  if (tokens[1] === 'task' && tokens[2] === 'preview') {
    const file = readFlag(tokens, '--file');
    return file ? { kind: 'preview', file } : undefined;
  }

  if (tokens[1] === 'task' && tokens[2] === 'execute') {
    const file = readFlag(tokens, '--file');
    const previewToken = readFlag(tokens, '--preview-token');
    return file ? compact({ kind: 'execute', file, previewToken }) : undefined;
  }

  if (tokens[1] === 'task' && tokens[2] === 'result') {
    const id = readFlag(tokens, '--id');
    return id ? { kind: 'result', id } : undefined;
  }

  if (tokens[1] === 'context' && tokens[2] === 'read') {
    const file = readFlag(tokens, '--file');
    return file ? { kind: 'context_read', file } : undefined;
  }

  if (tokens[1] === 'blueprinthelper_read_reference_context') {
    const file = readFlag(tokens, '--file');
    return file ? { kind: 'context_read', file } : undefined;
  }

  return undefined;
}

function tokenize(command) {
  if (typeof command !== 'string' || command.trim().length === 0) {
    return [];
  }

  const tokens = [];
  let current = '';
  let quote = '';
  for (let index = 0; index < command.length; index += 1) {
    const char = command[index];
    if (quote) {
      if (char === quote) {
        quote = '';
      } else {
        current += char;
      }
      continue;
    }
    if (char === '"' || char === "'") {
      quote = char;
      continue;
    }
    if (/\s/.test(char)) {
      if (current.length > 0) {
        tokens.push(current);
        current = '';
      }
      continue;
    }
    current += char;
  }
  if (current.length > 0) {
    tokens.push(current);
  }
  return tokens;
}

function isBhCommand(token) {
  const name = path.basename(token).toLowerCase();
  return name === 'bh' || name === 'bh.cmd' || name === 'bh.exe';
}

function readFlag(tokens, flag) {
  const index = tokens.indexOf(flag);
  if (index === -1) {
    return undefined;
  }
  const value = tokens[index + 1];
  return typeof value === 'string' && value.length > 0 ? value : undefined;
}

function resolveLedgerRoot(input = {}) {
  const projectDir = normalizeProjectDir(
    input.projectDir
      ?? readProjectDirFromPackage(input.packageData)
      ?? findNearestProjectRoot(input.cwd ?? process.cwd()),
  );

  if (!projectDir) {
    const error = new Error('hook_ledger_project_path_unresolved');
    error.code = 'hook_ledger_project_path_unresolved';
    throw error;
  }

  return path.join(projectDir, 'Saved', 'BlueprintHelper', 'HookLedger');
}

function readProjectDirFromPackage(packageData) {
  if (!isRecord(packageData)) {
    return undefined;
  }
  const projectDir = readString(packageData.project_dir)
    ?? readString(packageData.projectDir)
    ?? readString(packageData.project_root)
    ?? readString(packageData.projectRoot);
  if (projectDir) {
    return projectDir;
  }
  const projectFile = readString(packageData.project_file) ?? readString(packageData.projectFile);
  return projectFile ? path.dirname(projectFile) : undefined;
}

function normalizeProjectDir(candidate) {
  if (!candidate) {
    return undefined;
  }
  const resolved = path.resolve(candidate);
  if (path.extname(resolved).toLowerCase() === '.uproject') {
    return path.dirname(resolved);
  }
  return resolved;
}

function findNearestProjectRoot(cwd) {
  let current = path.resolve(cwd);
  while (true) {
    if (isProjectRoot(current)) {
      return current;
    }
    const parent = path.dirname(current);
    if (parent === current) {
      return undefined;
    }
    current = parent;
  }
}

function isProjectRoot(dir) {
  if (fs.existsSync(path.join(dir, '.blueprinthelper', 'project-profile.json'))) {
    return true;
  }
  try {
    return fs
      .readdirSync(dir, { withFileTypes: true })
      .some((entry) => entry.isFile() && entry.name.toLowerCase().endsWith('.uproject'));
  } catch {
    return false;
  }
}

async function runWorkflowHook(input) {
  const event = input.event;
  if (event === 'PreToolUse') {
    return runPreToolUse(input);
  }
  if (event === 'PostToolUse') {
    return runPostToolUse(input);
  }
  if (event === 'Stop' || event === 'SubagentStop') {
    return runStop(input);
  }
  return allow();
}

async function runPreToolUse(input) {
  const command = classifyCommand(input.command);
  if (!command || command.kind !== 'execute') {
    return allow();
  }

  const packageData = await loadPackageForCommand(command, input.cwd, input.metadata);
  const taskPackageId = readTaskPackageId(packageData, input.metadata);
  if (!taskPackageId) {
    return block('task_package_id_missing', 'BlueprintHelper execute is missing task_package_id.');
  }

  const gateProblem = validatePrewriteGates(packageData);
  if (gateProblem) {
    return block(gateProblem, `BlueprintHelper execute blocked: ${gateProblem}.`);
  }

  const ledgerRootResult = resolveLedgerRootResult({
    cwd: input.cwd,
    packageData,
  });
  if (ledgerRootResult.error) {
    return block(ledgerRootResult.error, 'BlueprintHelper hook cannot resolve project Saved/BlueprintHelper/HookLedger.');
  }

  const ledger = await readLedger({
    ledgerRoot: ledgerRootResult.ledgerRoot,
    taskPackageId,
  });
  if (!ledger || !ledger.last_preview || ledger.last_preview.status !== 'passed') {
    return block('preview_required_before_execute', 'BlueprintHelper execute requires a passed preview in the hook ledger.');
  }

  if (attemptsExceeded(ledger)) {
    return block(
      'retry_budget_exceeded',
      'BlueprintHelper retry budget exceeded after 3 attempts. Report a capability boundary instead of executing again.',
    );
  }

  return allow();
}

async function runPostToolUse(input) {
  const command = classifyCommand(input.command);
  if (!command) {
    return allow();
  }

  if (command.kind === 'preview') {
    return recordPreview(input, command);
  }
  if (command.kind === 'execute') {
    return recordExecute(input, command);
  }
  if (command.kind === 'context_read') {
    return recordReadback(input);
  }
  if (command.kind === 'result') {
    return recordResult(input, command);
  }

  return allow();
}

async function recordPreview(input, command) {
  const packageData = await loadPackageForCommand(command, input.cwd, input.metadata);
  const taskPackageId = readTaskPackageId(packageData, input.metadata);
  if (!taskPackageId) {
    return block('task_package_id_missing', 'BlueprintHelper preview is missing task_package_id.');
  }

  const ledgerRootResult = resolveLedgerRootResult({ cwd: input.cwd, packageData });
  if (ledgerRootResult.error) {
    return block(ledgerRootResult.error, 'BlueprintHelper hook cannot resolve project Saved/BlueprintHelper/HookLedger.');
  }

  const ledger = await readOrCreateLedger({
    ledgerRoot: ledgerRootResult.ledgerRoot,
    taskPackageId,
    packageData,
  });
  ledger.attempts.preview += 1;
  const parsed = parseToolResult(input.toolResult);
  ledger.last_preview = {
    status: isSuccessfulToolResult(input.toolResult, parsed) ? 'passed' : 'failed',
    preview_token: readFirstString(
      parsed.preview_token,
      parsed.previewToken,
      parsed.extra?.previewToken,
      parsed.extra?.preview_token,
      parsed.tool_result?.data?.preview_token,
      parsed.tool_result?.data?.previewToken,
    ),
  };
  await writeLedger({ ledgerRoot: ledgerRootResult.ledgerRoot, ledger });
  return allow();
}

async function recordExecute(input, command) {
  const packageData = await loadPackageForCommand(command, input.cwd, input.metadata);
  const taskPackageId = readTaskPackageId(packageData, input.metadata);
  if (!taskPackageId) {
    return block('task_package_id_missing', 'BlueprintHelper execute is missing task_package_id.');
  }

  const ledgerRootResult = resolveLedgerRootResult({ cwd: input.cwd, packageData });
  if (ledgerRootResult.error) {
    return block(ledgerRootResult.error, 'BlueprintHelper hook cannot resolve project Saved/BlueprintHelper/HookLedger.');
  }

  const ledger = await readOrCreateLedger({
    ledgerRoot: ledgerRootResult.ledgerRoot,
    taskPackageId,
    packageData,
  });
  ledger.attempts.execute += 1;
  const parsed = parseToolResult(input.toolResult);
  const succeeded = isSuccessfulToolResult(input.toolResult, parsed);
  ledger.last_execute = {
    status: succeeded ? 'succeeded' : 'failed',
    task_run_id: readFirstString(
      parsed.task_run_id,
      parsed.taskRunId,
      parsed.tool_result?.data?.task_run_id,
      parsed.tool_result?.data?.taskRunId,
      command.id,
    ),
  };
  if (ledger.readback.required) {
    ledger.readback.completed = false;
    ledger.readback.status = 'not_run';
  }
  await writeLedger({ ledgerRoot: ledgerRootResult.ledgerRoot, ledger });

  if (succeeded && ledger.readback.required) {
    return {
      action: 'remind',
      reason: 'readback_required_after_execute',
      message: 'BlueprintHelper execute succeeded. Run the required readback with bh context read before reporting success.',
    };
  }
  return allow();
}

async function recordReadback(input) {
  const taskPackageId = readTaskPackageId(undefined, input.metadata);
  const ledgerRootResult = resolveLedgerRootResult({ cwd: input.cwd, packageData: input.metadata });
  if (ledgerRootResult.error) {
    return allow();
  }

  const targetLedger = taskPackageId
    ? await readLedger({ ledgerRoot: ledgerRootResult.ledgerRoot, taskPackageId })
    : await findLatestActiveLedger(ledgerRootResult.ledgerRoot);
  if (!targetLedger) {
    return allow();
  }

  targetLedger.attempts.readback += 1;
  const parsed = parseToolResult(input.toolResult);
  targetLedger.readback.completed = isSuccessfulToolResult(input.toolResult, parsed);
  targetLedger.readback.status = targetLedger.readback.completed ? 'completed' : 'failed';
  await writeLedger({ ledgerRoot: ledgerRootResult.ledgerRoot, ledger: targetLedger });
  return allow();
}

async function recordResult(input, command) {
  const ledgerRootResult = resolveLedgerRootResult({ cwd: input.cwd, packageData: input.metadata });
  if (ledgerRootResult.error) {
    return allow();
  }
  const ledger = await findLedgerByRunId(ledgerRootResult.ledgerRoot, command.id);
  if (!ledger) {
    return allow();
  }
  const parsed = parseToolResult(input.toolResult);
  ledger.last_result = {
    status: isSuccessfulToolResult(input.toolResult, parsed) ? 'succeeded' : 'failed',
    task_run_id: command.id,
  };
  await writeLedger({ ledgerRoot: ledgerRootResult.ledgerRoot, ledger });
  return allow();
}

async function runStop(input) {
  const ledgerRootResult = resolveLedgerRootResult({ cwd: input.cwd, packageData: input.metadata });
  if (ledgerRootResult.error || !fs.existsSync(ledgerRootResult.ledgerRoot)) {
    return allow();
  }

  const ledgers = await readAllLedgers(ledgerRootResult.ledgerRoot);
  const missing = ledgers.find((ledger) => {
    return ledger.last_execute?.status === 'succeeded'
      && ledger.readback?.required === true
      && ledger.readback?.completed !== true;
  });

  if (!missing) {
    return allow();
  }

  return block(
    'readback_missing_before_success',
    `BlueprintHelper task ${missing.task_package_id} executed successfully but required readback is missing. Run bh context read before reporting success.`,
  );
}

async function loadPackageForCommand(command, cwd, metadata) {
  const fromFile = command.file
    ? await readJsonIfPresent(path.resolve(cwd ?? process.cwd(), command.file))
    : undefined;
  return mergeRecords(fromFile, metadata);
}

function readTaskPackageId(packageData, metadata) {
  return readString(metadata?.task_package_id)
    ?? readString(metadata?.taskPackageId)
    ?? readString(packageData?.task_package_id)
    ?? readString(packageData?.taskPackageId)
    ?? readString(packageData?.package?.task_package_id);
}

function validatePrewriteGates(packageData) {
  const gates = isRecord(packageData?.prewrite_gates)
    ? packageData.prewrite_gates
    : packageData?.package?.prewrite_gates;
  if (!isRecord(gates)) {
    return 'source_control_gate_missing_or_failed';
  }

  const sourceControl = readGateStatus(gates.source_control);
  if (sourceControl !== 'passed' && sourceControl !== 'not_required') {
    return 'source_control_gate_missing_or_failed';
  }

  const writeSession = readGateStatus(gates.write_session);
  if (writeSession !== 'approved' && writeSession !== 'not_required') {
    return 'write_session_gate_missing_or_failed';
  }

  return undefined;
}

function readGateStatus(value) {
  if (typeof value === 'string') {
    return value;
  }
  if (isRecord(value)) {
    return readString(value.status);
  }
  return undefined;
}

async function readOrCreateLedger(input) {
  const existing = await readLedger(input);
  if (existing) {
    return existing;
  }
  return createLedger(input);
}

function createLedger(input) {
  const projectDir = normalizeProjectDir(readProjectDirFromPackage(input.packageData) ?? path.resolve(input.ledgerRoot, '..', '..', '..'));
  return {
    schema: LEDGER_SCHEMA,
    task_package_id: input.taskPackageId,
    project_dir: projectDir,
    attempts: {
      preview: 0,
      execute: 0,
      readback: 0,
    },
    prewrite_gates: normalizePrewriteGates(input.packageData),
    last_preview: undefined,
    last_execute: undefined,
    readback: {
      required: readbackRequired(input.packageData),
      completed: false,
      status: 'not_run',
    },
  };
}

function normalizePrewriteGates(packageData) {
  const gates = isRecord(packageData?.prewrite_gates)
    ? packageData.prewrite_gates
    : packageData?.package?.prewrite_gates;
  return {
    source_control: readGateStatus(gates?.source_control) ?? 'missing',
    write_session: readGateStatus(gates?.write_session) ?? 'missing',
  };
}

function readbackRequired(packageData) {
  if (typeof packageData?.readback_required === 'boolean') {
    return packageData.readback_required;
  }
  if (typeof packageData?.readbackRequired === 'boolean') {
    return packageData.readbackRequired;
  }
  return true;
}

async function readLedger(input) {
  const filePath = ledgerPath(input.ledgerRoot, input.taskPackageId);
  const parsed = await readJsonIfPresent(filePath);
  return isRecord(parsed) ? parsed : undefined;
}

async function writeLedger(input) {
  const filePath = ledgerPath(input.ledgerRoot, input.ledger.task_package_id);
  await fs.promises.mkdir(path.dirname(filePath), { recursive: true });
  await fs.promises.writeFile(filePath, `${JSON.stringify(cleanForJson(input.ledger), null, 2)}\n`, 'utf8');
}

function ledgerPath(ledgerRoot, taskPackageId) {
  return path.join(ledgerRoot, `${safeSegment(taskPackageId)}.json`);
}

async function readAllLedgers(ledgerRoot) {
  let entries;
  try {
    entries = await fs.promises.readdir(ledgerRoot, { withFileTypes: true });
  } catch {
    return [];
  }
  const ledgers = [];
  for (const entry of entries) {
    if (!entry.isFile() || !entry.name.endsWith('.json')) {
      continue;
    }
    const parsed = await readJsonIfPresent(path.join(ledgerRoot, entry.name));
    if (isRecord(parsed) && parsed.schema === LEDGER_SCHEMA) {
      ledgers.push(parsed);
    }
  }
  return ledgers;
}

async function findLatestActiveLedger(ledgerRoot) {
  const ledgers = await readAllLedgers(ledgerRoot);
  return ledgers
    .filter((ledger) => ledger.last_execute?.status === 'succeeded' && ledger.readback?.completed !== true)
    .sort((left, right) => String(right.last_execute?.task_run_id ?? '').localeCompare(String(left.last_execute?.task_run_id ?? '')))[0];
}

async function findLedgerByRunId(ledgerRoot, taskRunId) {
  const ledgers = await readAllLedgers(ledgerRoot);
  return ledgers.find((ledger) => ledger.last_execute?.task_run_id === taskRunId);
}

function attemptsExceeded(ledger) {
  const attempts = ledger.attempts ?? {};
  const maxAttempts = Number(ledger.retry_budget?.max_attempts) || MAX_ATTEMPTS;
  return Math.max(
    Number(attempts.preview) || 0,
    Number(attempts.execute) || 0,
  ) >= maxAttempts;
}

function parseToolResult(toolResult) {
  if (!isRecord(toolResult)) {
    return {};
  }
  for (const key of ['stdout', 'output', 'result']) {
    const value = toolResult[key];
    if (isRecord(value)) {
      return value;
    }
    if (typeof value === 'string') {
      const parsed = parseJson(value);
      if (isRecord(parsed)) {
        return parsed;
      }
    }
  }
  return {};
}

function isSuccessfulToolResult(toolResult, parsed) {
  if (isRecord(toolResult)) {
    const exitCode = toolResult.exit_code ?? toolResult.exitCode;
    if (typeof exitCode === 'number' && exitCode !== 0) {
      return false;
    }
  }

  const status = readString(parsed.status) ?? readString(parsed.result);
  if (parsed.ok === true && !/fail|blocked|error/i.test(status ?? '')) {
    return true;
  }
  if (!status) {
    return true;
  }
  return /^(success|succeeded|passed|completed|ok)$/i.test(status) || /(?:^|_)(passed|succeeded|completed)$/i.test(status);
}

function resolveLedgerRootResult(input) {
  try {
    return { ledgerRoot: resolveLedgerRoot(input) };
  } catch (error) {
    return { error: error?.code ?? 'hook_ledger_project_path_unresolved' };
  }
}

async function readJsonIfPresent(filePath) {
  try {
    const content = await fs.promises.readFile(filePath, 'utf8');
    return parseJson(content.replace(/^\uFEFF/, ''));
  } catch (error) {
    if (error?.code === 'ENOENT') {
      return undefined;
    }
    throw error;
  }
}

function parseJson(content) {
  if (typeof content !== 'string' || content.trim().length === 0) {
    return undefined;
  }
  try {
    return JSON.parse(content);
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
  return result;
}

function compact(record) {
  return Object.fromEntries(Object.entries(record).filter(([, value]) => value !== undefined));
}

function cleanForJson(value) {
  if (Array.isArray(value)) {
    return value.map(cleanForJson);
  }
  if (!isRecord(value)) {
    return value;
  }
  return Object.fromEntries(
    Object.entries(value)
      .filter(([, entryValue]) => entryValue !== undefined)
      .map(([key, entryValue]) => [key, cleanForJson(entryValue)]),
  );
}

function safeSegment(value) {
  return String(value ?? '').replace(/[^A-Za-z0-9_.-]/g, '_') || 'unknown';
}

function allow() {
  return { action: 'allow' };
}

function block(reason, message) {
  return { action: 'block', reason, message };
}

function readString(value) {
  return typeof value === 'string' && value.trim().length > 0 ? value.trim() : undefined;
}

function readFirstString(...values) {
  for (const value of values) {
    const text = readString(value);
    if (text) {
      return text;
    }
  }
  return undefined;
}

function isRecord(value) {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

module.exports = {
  classifyCommand,
  readLedger,
  resolveLedgerRoot,
  runWorkflowHook,
};
