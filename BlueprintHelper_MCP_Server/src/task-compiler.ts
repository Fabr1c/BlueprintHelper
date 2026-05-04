import type {
  AgentImportLink,
  AgentImportNode,
  AppendBridgePayload,
  BlueprintLogicStatement,
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
  assertSupportedTaskSpec(taskSpec);

  const nodes: AgentImportNode[] = [];
  const links: AgentImportLink[] = [];

  for (const entry of taskSpec.behavior.entries) {
    const entryId = `${toIdSegment(entry.name)}_entry`;
    nodes.push({ id: entryId, kind: 'custom_event', name: entry.name });

    let previousExecEndpoint = `${entryId}.then`;
    entry.body.statements.forEach((statement, index) => {
      const nodeId = `${toIdSegment(entry.name)}_stmt_${index + 1}`;
      const node = compileStatementNode(statement, nodeId, `behavior.entries[${index}].body.statements[${index}]`);
      nodes.push(node);
      links.push({ kind: 'exec', from: previousExecEndpoint, to: `${nodeId}.execute` });
      previousExecEndpoint = `${nodeId}.then`;
    });
  }

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
        operation: 'append_blueprint_graph',
        target: {
          asset_path: taskSpec.target.asset_path,
          graph: taskSpec.scope_policy.graph_name,
        },
        args: {
          feature_name: taskSpec.feature_name,
          nodes,
          links,
        },
      },
    ],
  };
}

export function taskPlanToAppendBridgePayload(taskPlan: TaskPlan, dryRun: boolean): AppendBridgePayload {
  const step = taskPlan.steps[0];
  if (!step || step.operation !== 'append_blueprint_graph') {
    throw new TaskSpecCompileError('unsupported_taskplan_operation', 'TaskPlan does not contain an append_blueprint_graph step.', [
      {
        code: 'unsupported_taskplan_operation',
        path: 'steps[0].operation',
        message: 'Only append_blueprint_graph TaskPlan steps are supported in the first MCP slice.',
      },
    ]);
  }

  return {
    target: {
      asset_path: step.target.asset_path,
      graph: step.target.graph,
    },
    ...(step.args.feature_name ? { feature_name: step.args.feature_name } : {}),
    nodes: step.args.nodes,
    links: step.args.links,
    dry_run: dryRun,
  };
}

export function summarizeTaskPlan(taskPlan: TaskPlan) {
  return {
    schema: taskPlan.schema,
    task_name: taskPlan.task_name,
    task_type: taskPlan.task_type,
    target_assets: taskPlan.target_assets,
    steps: taskPlan.steps.map((step) => ({
      step_id: step.step_id,
      operation: step.operation,
      target: step.target,
      nodes: step.args.nodes.length,
      links: step.args.links.length,
    })),
  };
}

function assertSupportedTaskSpec(taskSpec: TaskSpec) {
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

function toIdSegment(value: string): string {
  const normalized = value.replace(/[^A-Za-z0-9_]/g, '_');
  return normalized.length > 0 ? normalized : 'entry';
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}
