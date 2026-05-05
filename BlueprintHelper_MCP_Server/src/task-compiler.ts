import type {
  AgentImportLink,
  AgentImportNode,
  AppendBridgePayload,
  AppendTaskPlanStep,
  BlueprintVariableTaskPlanStep,
  BlueprintLogicStatement,
  GraphWriteStructuredIrTaskPlanStep,
  TaskIssue,
  TaskPlan,
  TaskSpec,
} from './task-schemas.js';
import { TASK_PLAN_SCHEMA } from './task-schemas.js';

export class TaskSpecCompileError extends Error {
  readonly code: string;
  readonly issues: TaskIssue[];

  constructor(code: string, message: string, issues: TaskIssue[]) {
    super(message);
    this.name = 'TaskSpecCompileError';
    this.code = code;
    this.issues = issues;
  }
}

export function compileTaskSpecToTaskPlan(taskSpec: TaskSpec): TaskPlan {
  if (taskSpec.task_type === 'edit_blueprint_variables') {
    return compileBlueprintVariablesTaskSpecToTaskPlan(taskSpec);
  }
  if (taskSpec.task_type !== 'edit_blueprint_graph') {
    throw new TaskSpecCompileError('unsupported_task_type', `Unsupported TaskSpec task_type: ${taskSpec.task_type}`, [
      {
        code: 'unsupported_task_type',
        path: 'task_type',
        message: 'This TypeScript fallback compiler currently supports edit_blueprint_graph and edit_blueprint_variables only; P1 capability compilation is owned by the Python compiler.',
      },
    ]);
  }

  assertSupportedTaskSpec(taskSpec);

  return {
    schema: TASK_PLAN_SCHEMA,
    task_name: taskSpec.feature_name,
    task_type: taskSpec.task_type,
    context_id: taskSpec.context_id,
    target_assets: [taskSpec.target.asset_path],
    execution_policy: {
      dry_run_mode: taskSpec.execution_policy.dry_run_mode,
      should_compile: taskSpec.validation.should_compile,
      should_save: taskSpec.validation.should_save,
    },
    steps: [
      {
        step_id: 'step_001',
        capability: 'graph_write',
        target: {
          asset_path: taskSpec.target.asset_path,
          graph: taskSpec.scope_policy.graph_name,
        },
        write: {
          strategy: 'owned_graph_edit',
          ops: taskSpec.behavior.entries.map((entry) => ({
            op: 'ensure_entry',
            entry_type: entry.entry_type,
            name: entry.name,
            body: entry.body,
          })),
        },
        constraints: {
          allow_modify_user_nodes: taskSpec.scope_policy.allow_modify_user_nodes,
          ownership_scope: 'blueprinthelper_owned',
        },
      },
    ],
  };
}

function compileBlueprintVariablesTaskSpecToTaskPlan(taskSpec: Extract<TaskSpec, { task_type: 'edit_blueprint_variables' }>): TaskPlan {
  assertSupportedBlueprintVariablesTaskSpec(taskSpec);
  const behavior = taskSpec.behavior as Record<string, unknown>;
  const strategy = getRequiredString(behavior, 'variable_strategy', 'behavior.variable_strategy');
  const target = {
    asset_path: taskSpec.target.asset_path,
    ...(strategy === 'local_variables'
      ? { function_name: getRequiredString(behavior, 'function_name', 'behavior.function_name') }
      : {}),
  };

  return {
    schema: TASK_PLAN_SCHEMA,
    task_name: taskSpec.feature_name,
    task_type: taskSpec.task_type,
    context_id: taskSpec.context_id,
    target_assets: [taskSpec.target.asset_path],
    execution_policy: {
      dry_run_mode: taskSpec.execution_policy.dry_run_mode,
      should_compile: taskSpec.validation.should_compile,
      should_save: taskSpec.validation.should_save,
    },
    steps: [
      {
        step_id: 'step_001',
        capability: 'blueprint_variable',
        target,
        write: {
          strategy,
          ops: compileBlueprintVariableOps(behavior),
        },
        constraints: {
          allow_remove_referenced_variables: false,
        },
      },
    ],
  };
}

export function taskPlanToAppendBridgePayload(taskPlan: TaskPlan, dryRun: boolean): AppendBridgePayload {
  const step = taskPlan.steps[0];
  if (!step) {
    throw new TaskSpecCompileError('unsupported_taskplan_operation', 'TaskPlan does not contain an append_blueprint_graph step.', [
      {
        code: 'unsupported_taskplan_operation',
        path: 'steps[0]',
        message: 'TaskPlan requires a first GraphWrite step.',
      },
    ]);
  }

  if ('capability' in step && step.capability === 'graph_write') {
    return graphWriteTaskPlanToAppendBridgePayload(taskPlan, step as GraphWriteStructuredIrTaskPlanStep, dryRun);
  }

  if (!('operation' in step) || step.operation !== 'append_blueprint_graph') {
    throw new TaskSpecCompileError('unsupported_taskplan_operation', 'TaskPlan does not contain an append_blueprint_graph step.', [
      {
        code: 'unsupported_taskplan_operation',
        path: 'steps[0].operation',
        message: 'Only append_blueprint_graph lowering adapter TaskPlan steps are supported in the first MCP slice.',
      },
    ]);
  }

  const appendStep = step as AppendTaskPlanStep;
  return {
    target: {
      asset_path: appendStep.target.asset_path,
      graph: appendStep.target.graph,
    },
    ...(appendStep.args.feature_name ? { feature_name: appendStep.args.feature_name } : {}),
    nodes: appendStep.args.nodes,
    links: appendStep.args.links,
    dry_run: dryRun,
  };
}

function graphWriteTaskPlanToAppendBridgePayload(
  taskPlan: TaskPlan,
  step: GraphWriteStructuredIrTaskPlanStep,
  dryRun: boolean,
): AppendBridgePayload {
  if (step.write.strategy !== 'owned_graph_edit') {
    throw new TaskSpecCompileError('unsupported_graph_write_strategy', `Unsupported GraphWrite strategy: ${step.write.strategy}`, [
      {
        code: 'unsupported_graph_write_strategy',
        path: 'steps[0].write.strategy',
        message: 'Only owned_graph_edit can lower to append_blueprint_graph in the first MCP slice.',
      },
    ]);
  }

  const nodes: AgentImportNode[] = [];
  const links: AgentImportLink[] = [];
  step.write.ops.forEach((rawOp, opIndex) => {
    if (rawOp.op !== 'ensure_entry') {
      throw new TaskSpecCompileError('unsupported_graph_write_op', `Unsupported GraphWrite op for append lowering: ${rawOp.op}`, [
        {
          code: 'unsupported_graph_write_op',
          path: `steps[0].write.ops[${opIndex}].op`,
          message: 'Only ensure_entry lowers to append_blueprint_graph in the first MCP slice.',
        },
      ]);
    }

    compileEnsureEntryOpIntoAppendPayload(nodes, links, rawOp as Record<string, unknown>, `steps[0].write.ops[${opIndex}]`);
  });

  return {
    target: {
      asset_path: step.target.asset_path,
      graph: step.target.graph,
    },
    ...(taskPlan.task_name ? { feature_name: taskPlan.task_name } : {}),
    nodes,
    links,
    dry_run: dryRun,
  };
}

export function summarizeTaskPlan(taskPlan: TaskPlan) {
  return {
    schema: taskPlan.schema,
    task_name: taskPlan.task_name,
    task_type: taskPlan.task_type,
    target_assets: taskPlan.target_assets,
    steps: taskPlan.steps.map((step) => summarizeTaskPlanStep(step)),
  };
}

function summarizeTaskPlanStep(step: TaskPlan['steps'][number]) {
  if ('capability' in step) {
    return {
      step_id: step.step_id,
      capability: step.capability,
      target: step.target,
      strategy: step.write.strategy,
      ops: step.write.ops.length,
    };
  }

  const args = step.args as Record<string, unknown>;
  const replacement = isRecord(args['replacement']) ? args['replacement'] : undefined;
  const nodes = Array.isArray(args['nodes'])
    ? args['nodes'].length
    : Array.isArray(replacement?.['nodes'])
      ? replacement['nodes'].length
      : undefined;
  const links = Array.isArray(args['links'])
    ? args['links'].length
    : Array.isArray(replacement?.['links'])
      ? replacement['links'].length
      : undefined;

  return {
    step_id: step.step_id,
    operation: step.operation,
    target: step.target,
    ...(nodes !== undefined ? { nodes } : {}),
    ...(links !== undefined ? { links } : {}),
  };
}

export function blueprintVariableTaskPlanToBridgePayload(
  taskPlan: TaskPlan,
  step: BlueprintVariableTaskPlanStep,
  dryRun: boolean,
): Record<string, unknown> {
  const lowerableAsBatchAdd = step.write.strategy === 'member_variables' &&
    step.write.ops.every((op) => op.op === 'ensure_member_variable');
  if (!lowerableAsBatchAdd) {
    return { task_plan: taskPlan };
  }

  if (step.write.strategy !== 'member_variables') {
    throw new TaskSpecCompileError('unsupported_variable_strategy', `Unsupported Blueprint Variable strategy: ${step.write.strategy}`, [
      {
        code: 'unsupported_variable_strategy',
        path: 'steps[0].write.strategy',
        message: 'Only member_variables can lower to add_blueprint_member_variables in this slice.',
      },
    ]);
  }

  return {
    asset_path: step.target.asset_path,
    variables: step.write.ops.map((op, index) => {
      if (op.op !== 'ensure_member_variable') {
        throw new TaskSpecCompileError('unsupported_variable_op', `Unsupported Blueprint Variable op: ${op.op}`, [
          {
            code: 'unsupported_variable_op',
            path: `steps[0].write.ops[${index}].op`,
            message: 'Only ensure_member_variable is supported in this slice.',
          },
        ]);
      }

      const { op: _op, ...payload } = op as Record<string, unknown>;
      return payload;
    }),
    dry_run: dryRun,
  };
}

function assertSupportedTaskSpec(taskSpec: TaskSpec) {
  if (taskSpec.task_type !== 'edit_blueprint_graph') {
    throw new TaskSpecCompileError('unsupported_task_type', `Unsupported TaskSpec task_type: ${taskSpec.task_type}`, [
      {
        code: 'unsupported_task_type',
        path: 'task_type',
        message: 'This compiler path only supports edit_blueprint_graph.',
      },
    ]);
  }

  if (taskSpec.behavior.graph_strategy !== 'append_new_owned_graph') {
    throw new TaskSpecCompileError('unsupported_graph_strategy', 'Only append_new_owned_graph is supported in the first MCP slice.', [
      {
        code: 'unsupported_graph_strategy',
        path: 'behavior.graph_strategy',
        message: 'Use behavior.graph_strategy="append_new_owned_graph" for this milestone.',
        suggested_patch: { op: 'replace', path: '/behavior/graph_strategy', value: 'append_new_owned_graph' },
      },
    ]);
  }

  if (taskSpec.scope_policy.allow_modify_user_nodes) {
    throw new TaskSpecCompileError('unsupported_scope_policy', 'Modifying user nodes is not supported for append_new_owned_graph.', [
      {
        code: 'unsupported_scope_policy',
        path: 'scope_policy.allow_modify_user_nodes',
        message: 'Set allow_modify_user_nodes=false and target a new or empty graph.',
        suggested_patch: { op: 'replace', path: '/scope_policy/allow_modify_user_nodes', value: false },
      },
    ]);
  }

  for (const [entryIndex, entry] of taskSpec.behavior.entries.entries()) {
    if (entry.entry_type !== 'custom_event') {
      throw new TaskSpecCompileError('unsupported_entry_type', 'Only custom_event entries are supported in the first MCP slice.', [
        {
          code: 'unsupported_entry_type',
          path: `behavior.entries[${entryIndex}].entry_type`,
          message: 'Use entry_type="custom_event". Function/Event signature management is a later capability cluster.',
          suggested_patch: { op: 'replace', path: `/behavior/entries/${entryIndex}/entry_type`, value: 'custom_event' },
        },
      ]);
    }

    for (const [statementIndex, statement] of entry.body.statements.entries()) {
      if (statement.kind !== 'call_function' && statement.kind !== 'set_member_variable') {
        throw new TaskSpecCompileError('unsupported_statement_kind', 'Only call_function and set_member_variable statements are supported in the first MCP slice.', [
          {
            code: 'unsupported_statement_kind',
            path: `behavior.entries[${entryIndex}].body.statements[${statementIndex}].kind`,
            message: 'Use call_function or set_member_variable, or split this work into a later GraphWrite capability.',
          },
        ]);
      }
    }
  }
}

function assertSupportedBlueprintVariablesTaskSpec(taskSpec: Extract<TaskSpec, { task_type: 'edit_blueprint_variables' }>) {
  const behavior = taskSpec.behavior as Record<string, unknown>;
  const strategy = getRequiredString(behavior, 'variable_strategy', 'behavior.variable_strategy');
  if (!['member_variables', 'member_defaults', 'local_variables'].includes(strategy)) {
    throw new TaskSpecCompileError('unsupported_variable_strategy', 'Only member_variables is supported in the Blueprint Variables slice.', [
      {
        code: 'unsupported_variable_strategy',
        path: 'behavior.variable_strategy',
        message: 'Use member_variables, member_defaults, or local_variables.',
        suggested_patch: { op: 'replace', path: '/behavior/variable_strategy', value: 'member_variables' },
      },
    ]);
  }

  if (strategy === 'local_variables') {
    getRequiredString(behavior, 'function_name', 'behavior.function_name');
  }

  compileBlueprintVariableOps(behavior);
}

type BlueprintVariableCompiledOp = Record<string, unknown> & { op: string };

function compileBlueprintVariableOps(behavior: Record<string, unknown>): BlueprintVariableCompiledOp[] {
  const strategy = getRequiredString(behavior, 'variable_strategy', 'behavior.variable_strategy');
  if (strategy === 'member_variables') {
    const entries = Array.isArray(behavior['changes'])
      ? behavior['changes']
      : Array.isArray(behavior['variables'])
        ? behavior['variables']
        : undefined;
    if (!entries || entries.length === 0) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'behavior.changes must contain at least one variable change.', [
        {
          code: 'missing_variables',
          path: Array.isArray(behavior['variables']) ? 'behavior.variables' : 'behavior.changes',
          message: 'Provide at least one variable change.',
        },
      ]);
    }
    return entries.map((entry, index) => compileMemberVariableChange(entry, `behavior.${Array.isArray(behavior['changes']) ? 'changes' : 'variables'}[${index}]`));
  }

  if (strategy === 'member_defaults') {
    const defaults = behavior['defaults'];
    if (!Array.isArray(defaults) || defaults.length === 0) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'behavior.defaults must contain at least one default change.', [
        {
          code: 'missing_variables',
          path: 'behavior.defaults',
          message: 'Provide at least one member default change.',
        },
      ]);
    }
    return defaults.map((entry, index) => compileMemberDefaultChange(entry, `behavior.defaults[${index}]`));
  }

  const changes = behavior['changes'];
  if (!Array.isArray(changes) || changes.length === 0) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'behavior.changes must contain at least one local variable change.', [
      {
        code: 'missing_variables',
        path: 'behavior.changes',
        message: 'Provide at least one local variable change.',
      },
    ]);
  }
  const functionName = getRequiredString(behavior, 'function_name', 'behavior.function_name');
  return changes.map((entry, index) => compileLocalVariableChange(entry, functionName, `behavior.changes[${index}]`));
}

function compileMemberVariableChange(rawEntry: unknown, path: string): BlueprintVariableCompiledOp {
  if (!isRecord(rawEntry)) {
    throwInvalidVariable(path);
  }
  const entry = rawEntry as Record<string, unknown>;
  if ('op' in entry && !('kind' in entry)) {
    if (entry['op'] !== 'ensure_member_variable') {
      throw new TaskSpecCompileError('unsupported_variable_op', 'Only ensure_member_variable is supported in the Blueprint Variables slice.', [
        {
          code: 'unsupported_variable_op',
          path: `${path}.op`,
          message: 'Replace adapter-style op with a semantic kind.',
        },
      ]);
    }
    const out = { ...entry };
    if (!isRecord(out['pin_type'])) {
      if (isRecord(out['variable_type'])) {
        out['pin_type'] = out['variable_type'];
        delete out['variable_type'];
      } else {
        throwMissingVariableType(`${path}.pin_type`);
      }
    }
    return out as BlueprintVariableCompiledOp;
  }

  const kind = getRequiredString(entry, 'kind', `${path}.kind`);
  if (kind === 'ensure_member_variable') {
    return omitUndefined({
      op: 'ensure_member_variable',
      name: getRequiredString(entry, 'name', `${path}.name`),
      pin_type: variablePinType(entry, path),
      category: entry['category'],
      tooltip: entry['tooltip'],
      flags: entry['flags'],
      metadata: entry['metadata'],
      name_collision: entry['name_collision'],
    }) as BlueprintVariableCompiledOp;
  }
  if (kind === 'configure_member_variable') {
    return {
      op: 'set_member_variable_properties',
      name: getRequiredString(entry, 'name', `${path}.name`),
      settings: requiredNonEmptyArray(entry, 'properties', `${path}.properties`),
    };
  }
  if (kind === 'remove_member_variable') {
    return {
      op: 'remove_member_variable',
      name: getRequiredString(entry, 'name', `${path}.name`),
    };
  }
  throwUnsupportedVariableKind(kind, `${path}.kind`, ['ensure_member_variable', 'configure_member_variable', 'remove_member_variable']);
}

function compileMemberDefaultChange(rawEntry: unknown, path: string): BlueprintVariableCompiledOp {
  if (!isRecord(rawEntry)) {
    throwInvalidVariable(path);
  }
  if (!('value' in rawEntry)) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'set_member_default requires value.', [
      {
        code: 'missing_variable_default_value',
        path: `${path}.value`,
        message: 'Provide a default value.',
      },
    ]);
  }
  return {
    op: 'set_member_default',
    name: getRequiredString(rawEntry, 'name', `${path}.name`),
    value: literalValue(rawEntry['value']),
  };
}

function compileLocalVariableChange(rawEntry: unknown, functionName: string, path: string): BlueprintVariableCompiledOp {
  if (!isRecord(rawEntry)) {
    throwInvalidVariable(path);
  }
  const kind = getRequiredString(rawEntry, 'kind', `${path}.kind`);
  if (kind === 'ensure_local_variable') {
    return {
      op: 'ensure_local_variable',
      function_name: functionName,
      name: getRequiredString(rawEntry, 'name', `${path}.name`),
      pin_type: variablePinType(rawEntry, path),
    };
  }
  if (kind === 'configure_local_variable') {
    return {
      op: 'set_local_variable_properties',
      function_name: functionName,
      name: getRequiredString(rawEntry, 'name', `${path}.name`),
      settings: requiredNonEmptyArray(rawEntry, 'properties', `${path}.properties`),
    };
  }
  if (kind === 'remove_local_variable') {
    return {
      op: 'remove_local_variable',
      function_name: functionName,
      name: getRequiredString(rawEntry, 'name', `${path}.name`),
    };
  }
  throwUnsupportedVariableKind(kind, `${path}.kind`, ['ensure_local_variable', 'configure_local_variable', 'remove_local_variable']);
}

function variablePinType(entry: Record<string, unknown>, path: string): Record<string, unknown> {
  if (isRecord(entry['pin_type'])) return entry['pin_type'];
  if (isRecord(entry['variable_type'])) return entry['variable_type'];
  throwMissingVariableType(`${path}.variable_type`);
}

function requiredNonEmptyArray(record: Record<string, unknown>, field: string, path: string): unknown[] {
  const value = record[field];
  if (Array.isArray(value) && value.length > 0) return value;
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be a non-empty list.`, [
    {
      code: 'missing_required_list',
      path,
      message: `${path} must be a non-empty list.`,
    },
  ]);
}

function throwInvalidVariable(path: string): never {
  throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Blueprint variable entry must be an object.', [
    {
      code: 'invalid_variable',
      path,
      message: 'Blueprint variable entry must be an object.',
    },
  ]);
}

function throwMissingVariableType(path: string): never {
  throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Blueprint variable type is required.', [
    {
      code: 'missing_variable_pin_type',
      path,
      message: 'Provide variable_type, for example {"category":"bool"}.',
    },
  ]);
}

function throwUnsupportedVariableKind(kind: string, path: string, allowed: string[]): never {
  throw new TaskSpecCompileError('unsupported_variable_op', `Unsupported Blueprint variable change kind: ${kind}`, [
    {
      code: 'unsupported_variable_op',
      path,
      message: `Use one of: ${allowed.join(', ')}.`,
    },
  ]);
}

function omitUndefined(record: Record<string, unknown>): Record<string, unknown> {
  return Object.fromEntries(Object.entries(record).filter(([, value]) => value !== undefined));
}

function compileStatementNode(statement: BlueprintLogicStatement, nodeId: string, path: string): AgentImportNode {
  if (statement.kind === 'call_function') {
    const functionName = getRequiredString(statement, 'name', `${path}.name`);
    return {
      id: nodeId,
      kind: 'call',
      function: functionName,
      inputs: compileArgs(statement['args']),
    };
  }

  if (statement.kind === 'set_member_variable') {
    const variableName = getRequiredString(statement, 'name', `${path}.name`);
    return {
      id: nodeId,
      kind: 'set',
      var: variableName,
      value: valueExprToString(statement['value']),
    };
  }

  throw new TaskSpecCompileError('unsupported_statement_kind', `Unsupported statement kind: ${statement.kind}`, [
    {
      code: 'unsupported_statement_kind',
      path: `${path}.kind`,
      message: `Unsupported statement kind: ${statement.kind}`,
    },
  ]);
}

function compileEnsureEntryOpIntoAppendPayload(
  nodes: AgentImportNode[],
  links: AgentImportLink[],
  op: Record<string, unknown>,
  path: string,
) {
  const entryType = getRequiredString(op, 'entry_type', `${path}.entry_type`);
  if (entryType !== 'custom_event') {
    throw new TaskSpecCompileError('unsupported_entry_type', 'Only custom_event entries are supported in the first MCP slice.', [
      {
        code: 'unsupported_entry_type',
        path: `${path}.entry_type`,
        message: 'Use entry_type="custom_event". Function/Event signature management is a later capability cluster.',
      },
    ]);
  }

  const entryName = getRequiredString(op, 'name', `${path}.name`);
  const body = getRequiredLogicBody(op, 'body', `${path}.body`);
  const entryId = `${toIdSegment(entryName)}_entry`;
  nodes.push({ id: entryId, kind: 'custom_event', name: entryName });

  let previousExecEndpoint = `${entryId}.then`;
  body.statements.forEach((statement, statementIndex) => {
    const nodeId = `${toIdSegment(entryName)}_stmt_${statementIndex + 1}`;
    const node = compileStatementNode(statement, nodeId, `${path}.body.statements[${statementIndex}]`);
    nodes.push(node);
    links.push({ kind: 'exec', from: previousExecEndpoint, to: `${nodeId}.execute` });
    previousExecEndpoint = `${nodeId}.then`;
  });
}

function compileArgs(args: unknown): Record<string, unknown> {
  if (!isRecord(args)) return {};
  const out: Record<string, unknown> = {};
  for (const [key, value] of Object.entries(args)) {
    out[key] = literalValue(value);
  }
  return out;
}

function literalValue(value: unknown): unknown {
  if (isRecord(value) && value['kind'] === 'literal') {
    return value['value'];
  }
  return value;
}

function valueExprToString(value: unknown): string {
  const literal = literalValue(value);
  if (typeof literal === 'string') return literal;
  if (typeof literal === 'number' || typeof literal === 'boolean') return String(literal);
  if (literal === null || literal === undefined) return '';
  return JSON.stringify(literal);
}

function getRequiredString(record: Record<string, unknown>, field: string, path: string): string {
  const value = record[field];
  if (typeof value === 'string' && value.trim().length > 0) {
    return value;
  }

  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be a non-empty string.`, [
    {
      code: 'missing_required_string',
      path,
      message: `${path} must be a non-empty string.`,
    },
  ]);
}

function getRequiredLogicBody(record: Record<string, unknown>, field: string, path: string): { statements: BlueprintLogicStatement[] } {
  const value = record[field];
  if (isRecord(value) && Array.isArray(value['statements'])) {
    return {
      statements: value['statements'] as BlueprintLogicStatement[],
    };
  }

  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be a BlueprintLogicSpec body.`, [
    {
      code: 'missing_required_logic_body',
      path,
      message: `${path} must be a BlueprintLogicSpec body.`,
    },
  ]);
}

function toIdSegment(value: string): string {
  const normalized = value.replace(/[^A-Za-z0-9_]/g, '_');
  return normalized.length > 0 ? normalized : 'entry';
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}
