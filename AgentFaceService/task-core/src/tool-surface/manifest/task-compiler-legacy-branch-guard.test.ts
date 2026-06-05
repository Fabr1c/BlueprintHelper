import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const BASELINE_TASK_TYPES = [
  'create_asset',
  'create_blueprint_feature',
  'edit_blueprint_graph',
  'edit_blueprint_variables',
  'edit_object_properties',
  'edit_blueprint_signature',
  'edit_blueprint_class_settings',
  'edit_blueprint_components',
  'edit_umg_widget',
  'edit_data_table',
] as const;

const MIGRATION_MESSAGE = 'New public compiler capability must enter descriptor/registry plans, not the legacy task-compiler.ts branch.';

test('legacy task compiler branch does not grow new public task types during descriptor migration', () => {
  const compilerRegistrySource = readTaskCoreSource('task/compiler/task-compiler-registry.ts');
  for (const taskType of BASELINE_TASK_TYPES) {
    assert.match(
      compilerRegistrySource,
      new RegExp(`['"]${escapeRegExp(taskType)}['"]`),
      `${taskType} is missing from CANONICAL_TS_TASK_TYPES. ${MIGRATION_MESSAGE}`,
    );
  }

  const compilerSource = readTaskCoreSource('task/compiler/task-compiler.ts');
  const branchTaskTypes = new Set<string>();
  for (const match of compilerSource.matchAll(/taskSpec\.task_type\s*={2,3}\s*['"]([^'"]+)['"]/g)) {
    branchTaskTypes.add(match[1] ?? '');
  }
  for (const match of compilerSource.matchAll(/case\s+['"]([^'"]+)['"]/g)) {
    branchTaskTypes.add(match[1] ?? '');
  }

  for (const taskType of branchTaskTypes) {
    assert.ok(
      BASELINE_TASK_TYPES.includes(taskType as typeof BASELINE_TASK_TYPES[number]),
      `${taskType} is not in the migration baseline. ${MIGRATION_MESSAGE}`,
    );
  }
});

function readTaskCoreSource(relativePath: string): string {
  return fs.readFileSync(path.resolve(taskCoreRoot(), 'src', relativePath), 'utf8');
}

function escapeRegExp(value: string): string {
  return value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function taskCoreRoot(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
}
