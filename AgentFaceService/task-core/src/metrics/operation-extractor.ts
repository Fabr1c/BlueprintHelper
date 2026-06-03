import type { MetricsOperationIdentity } from './metrics-types.js';

const GRAPH_WRITE_ADAPTER_CAPABILITY = 'graph_write';

const CAPABILITY_BY_ADAPTER_OPERATION = new Map<string, string>([
  ['append_blueprint_graph', GRAPH_WRITE_ADAPTER_CAPABILITY],
  ['replace_blueprint_graph', GRAPH_WRITE_ADAPTER_CAPABILITY],
  ['patch_blueprint_graph', GRAPH_WRITE_ADAPTER_CAPABILITY],
  ['merge_blueprint_graph', GRAPH_WRITE_ADAPTER_CAPABILITY],
  ['merge_external_flow', GRAPH_WRITE_ADAPTER_CAPABILITY],
  ['patch_external_graph', GRAPH_WRITE_ADAPTER_CAPABILITY],
  ['replace_external_body', GRAPH_WRITE_ADAPTER_CAPABILITY],
]);

export function extractTaskPlanMetricOperations(taskPlanLike: unknown): MetricsOperationIdentity[] {
  const raw = asRecord(taskPlanLike);
  const steps = Array.isArray(raw?.['steps']) ? raw['steps'] : [];
  const operations: MetricsOperationIdentity[] = [];

  for (const stepValue of steps) {
    const step = asRecord(stepValue);
    if (!step) {
      continue;
    }

    const adapterOperation = readString(step['operation']);
    if (adapterOperation) {
      operations.push({
        capability: CAPABILITY_BY_ADAPTER_OPERATION.get(adapterOperation) ?? adapterOperation,
        semantic_operation: buildAdapterSemanticOperation(adapterOperation, step),
      });
      continue;
    }

    const capability = readString(step['capability']);
    if (!capability) {
      continue;
    }

    const stepOperations = collectStructuredStepOperations(step['write'] ?? step['action']);
    if (stepOperations.length === 0) {
      operations.push({ capability, semantic_operation: `${capability}.unknown` });
      continue;
    }

    for (const semanticOperation of stepOperations) {
      operations.push({
        capability,
        semantic_operation: semanticOperation,
      });
    }
  }

  return operations;
}

export function extractReadToolOperation(input: unknown): MetricsOperationIdentity {
  const raw = asRecord(input);
  const readType = readString(raw?.['read_type']) ?? 'read';
  const view = asRecord(raw?.['view']);
  const format = readString(view?.['format'])
    ?? readString(raw?.['format'])
    ?? defaultReadContextFormat(readType)
    ?? 'unknown';

  return {
    capability: 'read_context',
    semantic_operation: `${readType}.${format}`,
  };
}

function defaultReadContextFormat(readType: string): string | undefined {
  if (readType === 'graph_context') {
    return 'logic_json';
  }
  if (readType === 'blueprint_logic') {
    return 'logic_flow';
  }

  return undefined;
}

function collectStructuredStepOperations(value: unknown): string[] {
  const config = asRecord(value);
  const ops = Array.isArray(config?.['ops']) ? config['ops'] : [];
  const operationNames = ops
    .map((entry) => readString(asRecord(entry)?.['op']))
    .filter((entry): entry is string => entry !== undefined);

  if (operationNames.length > 0) {
    return operationNames;
  }

  const strategy = readString(config?.['strategy']);
  return strategy ? [strategy] : [];
}

function buildAdapterSemanticOperation(operation: string, step: Record<string, unknown>): string {
  const target = asRecord(step['target']);
  const args = asRecord(step['args']);
  const parts = [operation];

  const scope = readString(
    target?.['patch_scope']
    ?? target?.['replace_scope']
    ?? target?.['merge_scope'],
  );
  const variant = readString(
    args?.['patch_type']
    ?? args?.['kind']
    ?? args?.['insert_strategy']
    ?? args?.['scope']
    ?? target?.['insert_strategy']
    ?? args?.['strategy'],
  );

  if (scope) {
    parts.push(scope);
  }
  if (variant) {
    parts.push(variant);
  }

  return parts.join('.');
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return value !== null && typeof value === 'object' && !Array.isArray(value)
    ? value as Record<string, unknown>
    : undefined;
}

function readString(value: unknown): string | undefined {
  return typeof value === 'string' && value.trim().length > 0 ? value.trim() : undefined;
}
