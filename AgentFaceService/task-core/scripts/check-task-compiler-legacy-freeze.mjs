import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

export const LEGACY_COMPILER_FROZEN_ALLOWLIST = {
  task_types: [
    'create_blueprint_feature',
    'edit_blueprint_graph',
  ],
  graph_strategies: [
    'append_new_owned_graph',
    'replace_owned_graph',
    'patch_owned_graph',
    'merge_owned_graph',
    'merge_external_flow',
    'patch_external_graph',
    'replace_external_body',
  ],
  adapter_operations: [
    'append_blueprint_graph',
    'replace_blueprint_graph',
    'patch_blueprint_graph',
    'merge_blueprint_graph',
    'merge_external_flow',
    'patch_external_graph',
    'replace_external_body',
  ],
};

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const DEFAULT_ROOT = path.resolve(__dirname, '..');
const COMPILER_RELATIVE_PATH = 'src/task/compiler/task-compiler.ts';

export function runLegacyFreezeCheck(root = DEFAULT_ROOT) {
  const compilerPath = path.resolve(root, COMPILER_RELATIVE_PATH);
  const source = stripComments(fs.readFileSync(compilerPath, 'utf8'));
  const allowedTaskTypes = new Set(LEGACY_COMPILER_FROZEN_ALLOWLIST.task_types);
  const allowedGraphStrategies = new Set(LEGACY_COMPILER_FROZEN_ALLOWLIST.graph_strategies);
  const allowedAdapterOperations = new Set(LEGACY_COMPILER_FROZEN_ALLOWLIST.adapter_operations);
  const matches = [];

  for (const taskType of extractTaskTypeBranches(source)) {
    if (!allowedTaskTypes.has(taskType)) {
      matches.push({
        kind: 'task_type',
        value: taskType,
        reason: 'Task type branches in task-compiler.ts are frozen.',
      });
    }
  }

  for (const strategy of extractGraphStrategyBranches(source)) {
    if (!allowedGraphStrategies.has(strategy)) {
      matches.push({
        kind: 'graph_strategy',
        value: strategy,
        reason: 'GraphWrite strategies must enter descriptors/registries, not new task-compiler.ts branches.',
      });
    }
  }

  for (const operation of extractAdapterOperationBranches(source)) {
    if (!allowedAdapterOperations.has(operation)) {
      matches.push({
        kind: 'adapter_operation',
        value: operation,
        reason: 'Adapter operation handling in task-compiler.ts is frozen.',
      });
    }
  }

  return {
    ok: matches.length === 0,
    code: matches.length === 0 ? undefined : 'legacy_task_compiler_branch_added',
    file: 'AgentFaceService/task-core/src/task/compiler/task-compiler.ts',
    matches,
  };
}

function extractTaskTypeBranches(source) {
  const branches = [];
  for (const match of source.matchAll(/taskSpec\.task_type\s*={2,3}\s*['"]([^'"]+)['"]/g)) {
    if (match[1]) {
      branches.push(match[1]);
    }
  }
  for (const match of source.matchAll(/case\s+['"]([^'"]+)['"]\s*:/g)) {
    if (match[1]) {
      branches.push(match[1]);
    }
  }
  return [...new Set(branches)];
}

function extractAdapterOperationBranches(source) {
  const operations = [];
  for (const match of source.matchAll(/operation\s+={2,3}\s*['"]([^'"]+)['"]/g)) {
    if (match[1] && isGraphWriteAdapterOperationLiteral(match[1])) {
      operations.push(match[1]);
    }
  }
  for (const match of source.matchAll(/case\s+['"]([^'"]+)['"]\s*:/g)) {
    if (match[1] && isGraphWriteAdapterOperationLiteral(match[1])) {
      operations.push(match[1]);
    }
  }
  return [...new Set(operations)];
}

function isGraphWriteAdapterOperationLiteral(value) {
  return value.includes('_blueprint_graph')
    || value === 'merge_external_flow'
    || value === 'patch_external_graph'
    || value === 'replace_external_body';
}

function extractGraphStrategyBranches(source) {
  const strategies = [];
  for (const match of source.matchAll(/(?<![\w.])strategy\s*(?:={2,3}|!==?)\s*['"]([^'"]+)['"]/g)) {
    if (match[1]) {
      strategies.push(match[1]);
    }
  }
  for (const match of source.matchAll(/requireGraphWriteRouteByScope\(\s*['"]([^'"]+)['"]/g)) {
    if (match[1]) {
      strategies.push(match[1]);
    }
  }
  return [...new Set(strategies)];
}

function stripComments(source) {
  return source
    .replace(/\/\*[\s\S]*?\*\//g, '')
    .replace(/(^|[^:])\/\/.*$/gm, '$1');
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const result = runLegacyFreezeCheck();
  console.log(JSON.stringify(result.ok ? { ok: true } : result, null, 2));
  process.exit(result.ok ? 0 : 1);
}
