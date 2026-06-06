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
  ['graphwrite bridge payload re-export', /export\s*\{\s*taskPlanToAppendBridgePayload\s*\}/],
  ['task plan summary re-export', /export\s*\{\s*summarizeTaskPlan\s*\}/],
  ['blueprint variable bridge payload re-export', /export\s*\{\s*blueprintVariableTaskPlanToBridgePayload\s*\}/],
];

const FORBIDDEN_GRAPHWRITE_LOGIC_BODY_PATTERNS: readonly [string, RegExp][] = [
  ['task plan step builder', /function makeGraphWriteTaskPlanSteps\b/],
  ['target graph hardcoded selector', /function targetGraphForGraphWriteOp\b/],
  ['legacy statement flow entrypoint', /function compileStatementFlow\b/],
  ['legacy value expression entrypoint', /function compileValueExpression\b/],
  ['legacy statement node entrypoint', /function compileStatementNode\b/],
  ['statement flow by compiler switch', /function compileStatementFlowByCompiler\b/],
  ['expression by compiler switch', /function compileValueExpressionByCompiler\b/],
  ['statement node by compiler switch', /function compileStatementNodeByCompiler\b/],
  ['function body target graph branch', /replace_scope\s*===\s*['"]function_body['"]/],
  ['macro body target graph branch', /replace_scope\s*===\s*['"]macro_body['"]/],
  ['patch payload compiler', /function compilePatchPayload\b/],
  ['patch target ref normalizer', /function normalizePatchTargetRef\b/],
  ['owned patch kind catalog', /OWNED_GRAPH_PATCH_KINDS/],
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

test('GraphWrite logic body no longer owns plan-step or compiler-id lowering switches', () => {
  const logicBodySource = readFileSync(
    path.resolve(taskCoreRoot(), 'src', 'task', 'compiler', 'graphwrite', 'graphwrite-logic-body-compiler.ts'),
    'utf8',
  );

  for (const [label, pattern] of FORBIDDEN_GRAPHWRITE_LOGIC_BODY_PATTERNS) {
    assert.equal(pattern.test(logicBodySource), false, `${label} must not remain in graphwrite-logic-body-compiler.ts`);
  }
});

test('GraphWrite patch compiler owns patch payload helpers', () => {
  const patchCompilerSource = readFileSync(
    path.resolve(taskCoreRoot(), 'src', 'task', 'compiler', 'graphwrite', 'graphwrite-patch-compiler.ts'),
    'utf8',
  );

  assert.match(patchCompilerSource, /function compilePatchPayload\b/);
  assert.match(patchCompilerSource, /function normalizePatchTargetRef\b/);
  assert.match(patchCompilerSource, /const OWNED_GRAPH_PATCH_KINDS\b/);
  assert.doesNotMatch(
    patchCompilerSource,
    /import\s*\{[^}]*compilePatchPayload[^}]*\}\s*from\s*['"]\.\/graphwrite-logic-body-compiler\.js['"]/s,
  );
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
