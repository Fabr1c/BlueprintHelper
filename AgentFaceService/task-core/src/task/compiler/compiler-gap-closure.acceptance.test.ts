import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { TaskSpecSchema } from '../schema/task-schemas.js';
import { compileTaskSpecToTaskPlan } from './task-compiler.js';

const FORBIDDEN_TASK_COMPILER_PATTERNS: readonly [string, RegExp][] = [
  ['legacy fallback function', /compileLegacyGraphWriteOrCompositeTaskSpecToTaskPlan/],
  ['inline graphwrite operation registry', /graphWriteOperationCompilerRegistry/],
  ['inline statement flow compiler', /function compileStatementFlow\b/],
  ['inline value expression compiler', /function compileValueExpression\b/],
  ['inline statement node compiler', /function compileStatementNode\b/],
  ['inline composite feature compiler', /function compileCompositeBlueprintFeatureTaskSpecToTaskPlan\b/],
  ['owned patch payload switch', /function compilePatchPayload\b/],
];

const GRAPHWRITE_FIXTURE_PATHS = [
  'AgentFaceService/cli/test-fixtures/graphwrite-slots/replace-function-body-with-param-return.taskspec.json',
  'AgentFaceService/cli/test-fixtures/graphwrite-slots/append-custom-event-with-call.taskspec.json',
] as const;

test('task-compiler facade contains no legacy GraphWrite or composite compiler bodies', () => {
  const taskCompilerSource = readFileSync(
    path.resolve(taskCoreRoot(), 'src', 'task', 'compiler', 'task-compiler.ts'),
    'utf8',
  );

  for (const [label, pattern] of FORBIDDEN_TASK_COMPILER_PATTERNS) {
    assert.equal(pattern.test(taskCompilerSource), false, `${label} must not remain in task-compiler.ts`);
  }
});

test('GraphWrite fixtures still compile through the public compiler facade', () => {
  for (const fixturePath of GRAPHWRITE_FIXTURE_PATHS) {
    const fixture = JSON.parse(readFileSync(path.resolve(pluginRoot(), fixturePath), 'utf8'));
    const taskSpec = TaskSpecSchema.parse(fixture);
    const plan = compileTaskSpecToTaskPlan(taskSpec);

    assert.equal(plan.schema, 'BlueprintHelper.TaskPlan.v1', `${fixturePath} schema`);
    assert.equal(plan.task_type, 'edit_blueprint_graph', `${fixturePath} task_type`);
    assert.equal(plan.steps.length > 0, true, `${fixturePath} must produce TaskPlan steps`);
  }
});

function taskCoreRoot(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
}

function pluginRoot(): string {
  return path.resolve(taskCoreRoot(), '../..');
}
