import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import { compileTaskSpecToTaskPlan, TaskSpecCompileError } from '../../task/compiler/task-compiler.js';
import { getAgentVisibleGraphWriteRoutes } from '../../task/compiler/graphwrite/graphwrite-route-registry.js';
import { TaskSpecSchema, type TaskSpec } from '../../task/schema/task-schemas.js';
import { createTaskSpecInputShapeAdapterRegistry } from '../input/taskspec-input-adapters.js';
import {
  composeTaskSpecTemplate,
  listTaskSpecTemplateClusters,
  listTaskSpecTemplateFamilies,
  listTaskSpecTemplateOperations,
  listTaskSpecTemplateQuickAccess,
  listTaskSpecTemplateWriteModes,
} from '../templates/taskspec-template-composer.js';

test('agent-facing active write templates compile or stop at contract preflight', () => {
  let compiledCount = 0;
  let preflightCount = 0;

  for (const item of activeComposableWriteTemplates()) {
    const taskSpec = composeFilledTaskSpec(item);
    try {
      const taskPlan = compileTaskSpecToTaskPlan(taskSpec);
      assert.equal(taskPlan.schema, 'BlueprintHelper.TaskPlan.v1', item.label);
      assert.ok(Array.isArray(taskPlan.steps), item.label);
      compiledCount += 1;
    } catch (error) {
      assertCompilerPreflightError(error, item.label);
      preflightCount += 1;
    }
  }

  assert.ok(compiledCount > 0, 'expected at least one active template to compile to TaskPlan');
  assert.ok(preflightCount >= 0);
});

test('agent-facing compiler contract rejects removed variable default property path shape', () => {
  const taskSpec = composeFilledTaskSpec({
    family: 'blueprint_variables',
    templateId: 'blueprint_variables.variables.ensure_member_variable',
    label: 'blueprint_variables.variables.ensure_member_variable',
  });

  assertNoRemovedDefaultValuePropertyPath(taskSpec, 'blueprint_variables.variables.ensure_member_variable');
  const taskPlan = compileTaskSpecToTaskPlan(taskSpec);
  assert.equal(taskPlan.schema, 'BlueprintHelper.TaskPlan.v1');
});

test('GraphWrite function body compose compiles to function_body target graph', () => {
  const quickAccess = listTaskSpecTemplateQuickAccess({
    family: 'graph_write',
    cluster: '',
    operation: '',
    writeMode: 'graph.replace',
  }).items;
  const route = getAgentVisibleGraphWriteRoutes()
    .find((candidate) => candidate.route_id === 'graph.replace.function_body');
  assert.ok(route);
  const routeItem = quickAccess.find((item) =>
    item.slot_type === 'route'
    && item.source_slot_id === 'graph.replace.function_body');
  assert.ok(routeItem);
  const statement = quickAccess.find((item) =>
    item.slot_type === 'statement'
    && route.allowed_slot_ids.includes(item.source_slot_id));
  assert.ok(statement);

  const taskSpec = composeFilledTaskSpec({
    family: 'graph_write',
    writeMode: 'graph.replace',
    templateId: routeItem.template_id,
    templateExpression: `${routeItem.template_id}(${statement.template_id})`,
    label: 'graph.replace.function_body',
  });
  const taskPlan = compileTaskSpecToTaskPlan(taskSpec);
  const graphWriteStep = taskPlan.steps.find((step) =>
    (step as Record<string, unknown>).capability === 'graph_write'
  ) as Record<string, unknown> | undefined;

  assert.ok(graphWriteStep);
  const target = graphWriteStep.target as Record<string, unknown> | undefined;
  assert.equal(target?.['graph'], 'ContractSmokeFunction');
});

test('custom event route compiles without function parameter slots', () => {
  const quickAccess = listTaskSpecTemplateQuickAccess({
    family: 'graph_write',
    cluster: '',
    operation: '',
    writeMode: 'graph.replace',
  }).items;
  const routeItem = quickAccess.find((item) =>
    item.slot_type === 'route'
    && item.source_slot_id === 'graph.replace.event_body');
  assert.ok(routeItem);
  const statement = quickAccess.find((item) =>
    item.slot_type === 'statement'
    && item.source_slot_id !== 'graph.statement.control.return');
  assert.ok(statement);

  const taskSpec = composeFilledTaskSpec({
    family: 'graph_write',
    writeMode: 'graph.replace',
    templateId: routeItem.template_id,
    templateExpression: `${routeItem.template_id}(${statement.template_id})`,
    label: 'graph.replace.event_body',
  });
  const taskPlan = compileTaskSpecToTaskPlan(taskSpec);
  assert.doesNotMatch(JSON.stringify(taskPlan), /field\.function_param_get|function_param_get/);
});

interface CompilerContractTemplate {
  readonly family: string;
  readonly writeMode?: string;
  readonly templateId: string;
  readonly templateExpression?: string;
  readonly label: string;
}

function activeComposableWriteTemplates(): CompilerContractTemplate[] {
  const output: CompilerContractTemplate[] = [];
  for (const family of listTaskSpecTemplateFamilies({ workflow: 'preview_execute' }).items) {
    if (family.family !== 'graph_write') {
      const clusterIds = family.navigation.levels.includes('cluster')
        ? listTaskSpecTemplateClusters({ family: family.family }).items.map((cluster) => cluster.cluster_id)
        : [''];
      for (const clusterId of clusterIds) {
        for (const operation of listTaskSpecTemplateOperations({
          family: family.family,
          cluster: clusterId,
        }).items) {
          for (const item of listTaskSpecTemplateQuickAccess({
            family: family.family,
            cluster: clusterId,
            operation: operation.operation_id,
          }).items) {
            output.push({
              family: family.family,
              templateId: item.template_id,
              label: `${family.family}.${item.template_id}`,
            });
          }
        }
      }
      continue;
    }

    for (const writeMode of listTaskSpecTemplateWriteModes({ family: family.family }).items) {
      for (const cluster of listTaskSpecTemplateClusters({ family: family.family }).items) {
        if (cluster.unsupported_write_modes.includes(writeMode.write_mode)) {
          continue;
        }
        for (const operation of listTaskSpecTemplateOperations({
          family: family.family,
          cluster: cluster.cluster_id,
          writeMode: writeMode.write_mode,
        }).items) {
          const quickAccess = listTaskSpecTemplateQuickAccess({
            family: family.family,
            cluster: cluster.cluster_id,
            operation: operation.operation_id,
            writeMode: writeMode.write_mode,
          }).items;
          for (const item of quickAccess) {
            if (item.slot_type === 'expression') {
              continue;
            }
            output.push({
              family: family.family,
              writeMode: writeMode.write_mode,
              templateId: item.template_id,
              templateExpression: templateExpressionForCoverage(item, quickAccess),
              label: `${family.family}.${writeMode.write_mode}.${item.template_id}`,
            });
          }
        }
      }
    }
  }
  return output;
}

function composeFilledTaskSpec(item: CompilerContractTemplate): TaskSpec {
  const outputPath = path.join(
    fs.mkdtempSync(path.join(os.tmpdir(), 'bh-compiler-contract-')),
    `${item.templateId.replaceAll('.', '-')}.taskspec.json`,
  );
  const result = composeTaskSpecTemplate({
    ...(item.family === 'graph_write'
      ? {
        family: item.family,
        writeMode: requiredString(item.writeMode, `${item.label} write mode`),
        templateIds: [requiredString(item.templateExpression, `${item.label} template expression`)],
      }
      : {
        templateId: item.templateId,
      }),
    outputPath,
  });
  assert.equal(result.status, 'ok', `${item.label}: ${JSON.stringify(result)}`);
  const taskSpec = JSON.parse(fs.readFileSync(outputPath, 'utf8'));
  const coverageTaskSpec = fillTaskSpecCoveragePlaceholders(taskSpec);
  const adapted = createTaskSpecInputShapeAdapterRegistry()
    .require('bare_taskspec')
    .adapt(coverageTaskSpec) as { task_spec?: unknown };
  const parseResult = TaskSpecSchema.safeParse(adapted.task_spec);
  assert.equal(parseResult.success, true, parseResult.success ? undefined : `${item.label}: ${parseResult.error.message}`);
  return parseResult.data;
}

function requiredString(value: string | undefined, label: string): string {
  assert.equal(typeof value, 'string', label);
  return value ?? '';
}

function assertCompilerPreflightError(error: unknown, label: string): void {
  assert.ok(error instanceof TaskSpecCompileError, `${label}: expected TaskSpecCompileError, got ${(error as Error).message}`);
  assert.ok(
    isAllowedPreflightCode(error.code),
    `${label}: unexpected compiler error code ${error.code}`,
  );
  for (const issue of error.issues) {
    assert.equal(typeof issue.code, 'string', `${label}: issue code`);
    assert.equal(typeof issue.message, 'string', `${label}: issue message`);
  }
}

function isAllowedPreflightCode(code: string): boolean {
  return code === 'taskspec_semantic_invalid'
    || code === 'unsupported_task_type'
    || code === 'unsupported_composite_integration'
    || code === 'unsupported_composite_asset_creation'
    || code === 'unsupported_variable_strategy'
    || code === 'unsupported_variable_op'
    || code === 'unsupported_signature_change'
    || code === 'invalid_signature_remove_policy'
    || code === 'unsupported_statement_kind'
    || code === 'unsupported_container_kind'
    || code === 'legacy_pin_type_token_unsupported'
    || code === 'unsupported_entry_type'
    || code === 'unsupported_graph_write_anchor'
    || code === 'unsupported_external_graph_anchor'
    || code === 'unsupported_field_operation'
    || code === 'unsupported_control_shape'
    || code === 'unsupported_control_continuation'
    || code === 'unsupported_control_kind'
    || code === 'impure_expression_requires_statement'
    || code === 'return_value_shape_removed'
    || code === 'return_outputs_required';
}

function templateExpressionForCoverage(
  item: ReturnType<typeof listTaskSpecTemplateQuickAccess>['items'][number],
  quickAccess: ReturnType<typeof listTaskSpecTemplateQuickAccess>['items'],
): string {
  if (item.slot_type !== 'route' || !item.arg_slots.some((slot) => slot.includes('statement[]'))) {
    return item.template_id;
  }
  const itemWriteMode = item.write_mode;
  if (typeof itemWriteMode !== 'string') {
    assert.fail(`${item.template_id} route has write_mode`);
  }
  const statement = quickAccess.find((candidate) =>
    candidate.slot_type === 'statement'
    && candidate.write_mode === itemWriteMode
    && !candidate.unsupported_write_modes.includes(itemWriteMode));
  assert.notEqual(statement, undefined, `${item.template_id} route has statement child`);
  return `${item.template_id}(${statement?.template_id ?? ''})`;
}

function fillTaskSpecCoveragePlaceholders(value: unknown): unknown {
  if (Array.isArray(value)) {
    return value.map((item) => fillTaskSpecCoveragePlaceholders(item));
  }
  if (!value || typeof value !== 'object') {
    return fillScalarPlaceholder(value, '');
  }
  const record = value as Record<string, unknown>;
  const output: Record<string, unknown> = {};
  for (const [key, child] of Object.entries(record)) {
    if (key === 'feature_name' && isRequiredPlaceholder(child)) {
      output[key] = 'TemplateContractSmoke';
      continue;
    }
    if (key === 'asset_path' && isRequiredPlaceholder(child)) {
      output[key] = '/Game/BlueprintHelperCliSmoke/BP_TemplateContractSmoke';
      continue;
    }
    if (key === 'graph_name' && isRequiredPlaceholder(child)) {
      output[key] = 'EventGraph';
      continue;
    }
    if (key === 'function_name' && isRequiredPlaceholder(child)) {
      output[key] = 'ContractSmokeFunction';
      continue;
    }
    if (key === 'event_name' && isRequiredPlaceholder(child)) {
      output[key] = 'ContractSmokeEvent';
      continue;
    }
    if (key === 'variable_name' && isRequiredPlaceholder(child)) {
      output[key] = 'ContractSmokeFlag';
      continue;
    }
    if (key === 'property_descriptor_id' && isRequiredPlaceholder(child)) {
      output[key] = 'k2.node.comment';
      continue;
    }
    if (key === 'pin_direction' && isRequiredPlaceholder(child)) {
      output[key] = record['semantic_role'] === 'exec_boundary' ? 'output' : 'input';
      continue;
    }
    if (key === 'category' && typeof child === 'string' && child.includes('PIN_CATEGORY__')) {
      output[key] = 'bool';
      continue;
    }
    if (key.endsWith('_mapping') && isPlaceholder(child)) {
      output[key] = {};
      continue;
    }
    if (key === 'replacement_policy' && isPlaceholder(child)) {
      output[key] = 'replace_with_empty_root';
      continue;
    }
    if (key === 'is_variable' && isPlaceholder(child)) {
      output[key] = true;
      continue;
    }
    if (key === 'anchor_ref' && isRequiredPlaceholder(child)) {
      output[key] = schemaCoverageAnchorRef(record);
      continue;
    }
    output[key] = fillTaskSpecCoveragePlaceholders(child);
  }
  return output;
}

function fillScalarPlaceholder(value: unknown, key: string): unknown {
  if (!isPlaceholder(value)) {
    return value;
  }
  if (key.endsWith('_class') || String(value).includes('CLASS')) {
    return '/Script/Engine.Actor';
  }
  if (String(value).includes('VALUE_TYPE')) {
    return 'bool';
  }
  if (String(value).includes('VALUE')) {
    return true;
  }
  return placeholderStringValue(String(value));
}

function placeholderStringValue(value: string): string {
  if (value.includes('ASSET_PATH')) {
    return '/Game/BlueprintHelperCliSmoke/BP_TemplateContractSmoke';
  }
  if (value.includes('GRAPH')) {
    return 'EventGraph';
  }
  if (value.includes('FUNCTION')) {
    return 'ContractSmokeFunction';
  }
  if (value.includes('EVENT')) {
    return 'ContractSmokeEvent';
  }
  if (value.includes('VARIABLE')) {
    return 'ContractSmokeFlag';
  }
  if (value.includes('BLOCK_ID')) {
    return 'ContractSmokeBlock';
  }
  if (value.includes('NODE_KEY')) {
    return 'ContractSmokeNode';
  }
  if (value.includes('PIN')) {
    return 'ContractSmokePin';
  }
  if (value.includes('CLASS')) {
    return '/Script/Engine.Actor';
  }
  return 'ContractSmokeValue';
}

function schemaCoverageAnchorRef(anchor: Record<string, unknown>): string {
  const anchorType = typeof anchor['anchor_type'] === 'string' ? anchor['anchor_type'] : '';
  const current = typeof anchor['anchor_ref'] === 'string' ? anchor['anchor_ref'] : '';
  switch (anchorType) {
    case 'external_link':
      return current.includes('EXEC') ? 'xlink:v1:e:coverage-link' : 'xlink:v1:d:coverage-link';
    case 'external_pin':
      return 'xpin:v1:d:coverage-pin';
    case 'external_node':
      return 'xnode:v1:coverage-node#coverage-fingerprint';
    case 'external_body':
      return 'xbody:v1:coverage-body#coverage-fingerprint';
    default:
      return current;
  }
}

function assertNoRemovedDefaultValuePropertyPath(value: unknown, label: string): void {
  const hits: string[] = [];
  collectDefaultValuePropertyPathHits(value, '', hits);
  assert.deepEqual(hits, [], label);
}

function collectDefaultValuePropertyPathHits(value: unknown, pointer: string, hits: string[]): void {
  if (!value || typeof value !== 'object') {
    return;
  }
  if (Array.isArray(value)) {
    value.forEach((item, index) => collectDefaultValuePropertyPathHits(item, `${pointer}/${index}`, hits));
    return;
  }
  const record = value as Record<string, unknown>;
  if (record['property_path'] === 'default_value') {
    hits.push(`${pointer}/property_path=default_value`);
  }
  for (const [key, child] of Object.entries(record)) {
    collectDefaultValuePropertyPathHits(child, `${pointer}/${key}`, hits);
  }
}

function isRequiredPlaceholder(value: unknown): value is string {
  return typeof value === 'string' && value.startsWith('__REQUIRED_');
}

function isPlaceholder(value: unknown): value is string {
  return typeof value === 'string' && value.startsWith('__') && value.endsWith('__');
}
