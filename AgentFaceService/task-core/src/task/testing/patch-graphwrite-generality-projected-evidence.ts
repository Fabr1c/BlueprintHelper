#!/usr/bin/env node
import { readFileSync, writeFileSync } from 'node:fs';

import { BridgeClient, DEFAULT_BRIDGE_HOST, DEFAULT_BRIDGE_PORT, type BridgeResponse } from '../../bridge/bridge-client.js';

type JsonRecord = Record<string, unknown>;

export interface GraphWriteProjectedEvidenceRequest {
  request_id: string;
  operation_id: string;
  projection_kind: 'asset_action' | 'schedule';
  queries: string[];
  graph_latent_allowed?: string;
}

interface ProjectedEvidenceItem {
  request_id?: string;
  operation_id?: string;
  projection_kind?: string;
  status?: string;
  message?: string;
  evidence?: JsonRecord;
}

const PROJECTION_COMMAND = 'project_graphwrite_spawner_evidence';

export function collectProjectedEvidenceRequests(taskSpec: JsonRecord): GraphWriteProjectedEvidenceRequest[] {
  const requests: GraphWriteProjectedEvidenceRequest[] = [];
  for (const statement of graphWriteStatements(taskSpec)) {
    const contextEvidence = asRecord(statement['context_evidence']);
    const operationId = stringValue(contextEvidence['graphwrite_generality.operation_id']);
    if (!operationId) continue;

    const requestId = stringValue(contextEvidence['graphwrite_generality.variant_name'])
      || stringValue(statement['id'])
      || operationId;
    if (operationId === 'create.asset_action') {
      requests.push({
        request_id: requestId,
        operation_id: operationId,
        projection_kind: 'asset_action',
        queries: ['Make Array'],
      });
    } else if (operationId === 'schedule.latent_or_async_node') {
      requests.push({
        request_id: requestId,
        operation_id: operationId,
        projection_kind: 'schedule',
        queries: ['Async Load Primary Asset'],
        graph_latent_allowed: 'true',
      });
    } else if (operationId === 'schedule.timer_delegate_node') {
      requests.push({
        request_id: requestId,
        operation_id: operationId,
        projection_kind: 'schedule',
        queries: ['Set Timer by Event', 'Set Timer by Delegate', 'Set Timer'],
      });
    }
  }
  return requests;
}

export function applyProjectedEvidence(taskSpec: JsonRecord, items: readonly ProjectedEvidenceItem[]): number {
  const evidenceByRequest = new Map<string, JsonRecord>();
  for (const item of items) {
    if (item.status === 'resolved' && item.request_id && isRecord(item.evidence)) {
      evidenceByRequest.set(item.request_id, item.evidence);
    }
  }

  let patched = 0;
  for (const statement of graphWriteStatements(taskSpec)) {
    const contextEvidence = asRecord(statement['context_evidence']);
    const operationId = stringValue(contextEvidence['graphwrite_generality.operation_id']);
    const requestId = stringValue(contextEvidence['graphwrite_generality.variant_name'])
      || stringValue(statement['id'])
      || operationId;
    const projectedEvidence = evidenceByRequest.get(requestId);
    if (!projectedEvidence) continue;

    Object.assign(contextEvidence, projectedEvidence);
    statement['context_evidence'] = contextEvidence;
    patched += 1;
  }
  return patched;
}

export function unresolvedProjectedEvidenceItems(items: readonly ProjectedEvidenceItem[]): ProjectedEvidenceItem[] {
  return items.filter((item) => item.status !== 'resolved');
}

async function patchSpecWithBridgeEvidence(input: {
  specPath: string;
  assetPath: string;
  graphName: string;
  host?: string;
  port?: number;
}): Promise<number> {
  const taskSpec = JSON.parse(readFileSync(input.specPath, 'utf8')) as JsonRecord;
  const requests = collectProjectedEvidenceRequests(taskSpec);
  if (requests.length === 0) return 0;

  const bridge = new BridgeClient({
    host: input.host ?? process.env['BRIDGE_HOST'] ?? DEFAULT_BRIDGE_HOST,
    port: input.port ?? Number(process.env['BRIDGE_PORT'] ?? DEFAULT_BRIDGE_PORT),
    requestTimeoutMs: readPositiveEnvInt('BPH_GRAPHWRITE_EVIDENCE_TIMEOUT_MS', 120000),
  });
  let response: BridgeResponse;
  try {
    response = await bridge.sendCommand(PROJECTION_COMMAND, {
      asset_path: input.assetPath,
      graph_name: input.graphName,
      requests,
    });
  } finally {
    await bridge.close();
  }

  if (!response.success) {
    const error = asRecord((response as unknown as JsonRecord)['error']);
    throw new Error(`Projected evidence bridge command failed: ${
      response.message
        ?? response.error_code
        ?? stringValue(error['message'])
        ?? stringValue(error['code'])
        ?? 'unknown'
    }`);
  }
  const data = asRecord(asRecord(response.result)['data']);
  const items = asArray(data['items']).filter(isRecord) as ProjectedEvidenceItem[];
  const unresolved = unresolvedProjectedEvidenceItems(items);
  if (unresolved.length > 0 || data['all_resolved'] !== true) {
    const message = unresolved
      .map((item) => `${item.operation_id ?? item.request_id ?? 'unknown'}: ${item.message ?? item.status ?? 'unresolved'}`)
      .join('; ');
    throw new Error(`Projected evidence unresolved: ${message}`);
  }

  const patched = applyProjectedEvidence(taskSpec, items);
  writeFileSync(input.specPath, `${JSON.stringify(taskSpec, null, 2)}\n`, 'utf8');
  return patched;
}

function graphWriteStatements(taskSpec: JsonRecord): JsonRecord[] {
  const behavior = asRecord(taskSpec['behavior']);
  const entries = asArray(behavior['entries']);
  return entries.flatMap((entry) => {
    const body = asRecord(asRecord(entry)['body']);
    return asArray(body['statements']).filter(isRecord) as JsonRecord[];
  });
}

function asRecord(value: unknown): JsonRecord {
  return isRecord(value) ? value : {};
}

function isRecord(value: unknown): value is JsonRecord {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function asArray(value: unknown): unknown[] {
  return Array.isArray(value) ? value : [];
}

function stringValue(value: unknown): string {
  return typeof value === 'string' ? value.trim() : '';
}

function readPositiveEnvInt(name: string, fallback: number): number {
  const raw = process.env[name];
  if (!raw) return fallback;
  const parsed = Number(raw);
  return Number.isInteger(parsed) && parsed > 0 ? parsed : fallback;
}

function parseArgs(argv: readonly string[]): Map<string, string> {
  const args = new Map<string, string>();
  for (let index = 2; index < argv.length; index += 2) {
    args.set(argv[index], argv[index + 1]);
  }
  return args;
}

const invokedPath = process.argv[1]?.replace(/\\/g, '/');
if (invokedPath?.endsWith('/patch-graphwrite-generality-projected-evidence.js')) {
  const args = parseArgs(process.argv);
  const specPath = args.get('--spec');
  const assetPath = args.get('--asset');
  const graphName = args.get('--graph');
  if (!specPath || !assetPath || !graphName) {
    throw new Error('Usage: patch-graphwrite-generality-projected-evidence --spec <graph_write.json> --asset <asset_path> --graph <graph_name>');
  }
  const patched = await patchSpecWithBridgeEvidence({ specPath, assetPath, graphName });
  process.stdout.write(JSON.stringify({ ok: true, patched }) + '\n');
}
