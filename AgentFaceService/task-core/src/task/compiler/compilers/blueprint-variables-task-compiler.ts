import type { TaskPlan, TaskSpec } from '../../schema/task-schemas.js';
import {
  BLUEPRINT_VARIABLE_REPLICATION_CONDITIONS,
  BLUEPRINT_VARIABLE_REPLICATION_MODES,
  TASK_PLAN_SCHEMA,
} from '../../schema/task-schemas.js';
import {
  getRequiredString,
  isRecord,
  literalValue,
  omitUndefined,
  requiredNonEmptyArray,
  type TaskPlanStep,
} from '../compiler-helpers.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';
import type { TaskTypeCompiler } from '../task-type-compiler.js';

export const blueprintVariablesTaskCompiler: TaskTypeCompiler<Extract<TaskSpec, { task_type: 'edit_blueprint_variables' }>> = {
  id: 'blueprint_variables',
  taskType: 'edit_blueprint_variables',
  canCompile(taskSpec): taskSpec is Extract<TaskSpec, { task_type: 'edit_blueprint_variables' }> {
    return taskSpec.task_type === 'edit_blueprint_variables';
  },
  compile(taskSpec) {
    return compileBlueprintVariablesTaskSpecToTaskPlan(taskSpec);
  },
};

function compileBlueprintVariablesTaskSpecToTaskPlan(taskSpec: Extract<TaskSpec, { task_type: 'edit_blueprint_variables' }>): TaskPlan {
  assertSupportedBlueprintVariablesTaskSpec(taskSpec);
  const behavior = taskSpec.behavior as Record<string, unknown>;
  const strategy = getRequiredString(behavior, 'variable_strategy', 'behavior.variable_strategy');

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
      review_baseline_dirty_asset_policy: taskSpec.execution_policy.review_baseline_dirty_asset_policy ?? 'block',
    },
    steps: compileBlueprintVariableSteps(taskSpec.target.asset_path, behavior, strategy),
  };
}

function assertSupportedBlueprintVariablesTaskSpec(taskSpec: Extract<TaskSpec, { task_type: 'edit_blueprint_variables' }>) {
  const behavior = taskSpec.behavior as Record<string, unknown>;
  const strategy = getRequiredString(behavior, 'variable_strategy', 'behavior.variable_strategy');
  if (!['member_variables', 'member_defaults', 'local_variables'].includes(strategy)) {
    throw new TaskSpecCompileError('unsupported_variable_strategy', 'Unsupported Blueprint Variables strategy.', [
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

export function compileBlueprintVariableSteps(
  assetPath: string,
  behavior: Record<string, unknown>,
  strategy: string,
): TaskPlanStep[] {
  if (strategy !== 'member_variables') {
    return [
      blueprintVariableStep(
        'step_001',
        {
          asset_path: assetPath,
          ...(strategy === 'local_variables'
            ? { function_name: getRequiredString(behavior, 'function_name', 'behavior.function_name') }
            : {}),
        },
        strategy,
        compileBlueprintVariableOps(behavior),
      ),
    ];
  }

  const entries = Array.isArray(behavior['changes'])
    ? behavior['changes']
    : Array.isArray(behavior['variables'])
      ? behavior['variables']
      : [];
  const pathPrefix = Array.isArray(behavior['changes']) ? 'behavior.changes' : 'behavior.variables';
  const target = { asset_path: assetPath };
  const steps = [
    blueprintVariableStep(
      'step_001',
      target,
      'member_variables',
      entries.map((entry, index) => compileMemberVariableChange(entry, `${pathPrefix}[${index}]`)),
    ),
  ];
  const defaultOps = entries
    .map((entry, index) => compileMemberDefaultFromVariableEntry(entry, `${pathPrefix}[${index}]`))
    .filter((op): op is BlueprintVariableCompiledOp => op !== undefined);
  if (defaultOps.length > 0) {
    steps.push({
      ...blueprintVariableStep('step_002', target, 'member_defaults', defaultOps),
      depends_on: ['step_001'],
    });
  }
  return steps;
}

function blueprintVariableStep(
  stepId: string,
  target: Record<string, unknown>,
  strategy: string,
  ops: BlueprintVariableCompiledOp[],
): TaskPlanStep {
  return {
    step_id: stepId,
    capability: 'blueprint_variable',
    target,
    write: {
      strategy,
      ops,
    },
    constraints: {
      allow_remove_referenced_variables: false,
    },
  } as TaskPlanStep;
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

  if (!('kind' in entry)) {
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
    const name = getRequiredString(entry, 'name', `${path}.name`);
    return {
      op: 'set_member_variable_properties',
      name,
      settings: normalizeMemberVariablePropertySettings(entry, 'properties', `${path}.properties`, name),
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

function normalizeMemberVariablePropertySettings(
  record: Record<string, unknown>,
  field: string,
  path: string,
  variableName: string,
): unknown[] {
  return requiredNonEmptyArray(record, field, path)
    .map((rawSetting, index) => normalizeMemberVariablePropertySetting(rawSetting, `${path}[${index}]`, variableName));
}

function normalizeMemberVariablePropertySetting(rawSetting: unknown, path: string, variableName: string): unknown {
  if (!isRecord(rawSetting)) {
    return rawSetting;
  }

  const propertyPath = rawSetting['property_path'];
  if (propertyPath !== 'replication') {
    return rawSetting;
  }

  const rawValue = literalValue(rawSetting['value']);
  if (!isRecord(rawValue)) {
    throwReplicationCompileError(
      'invalid_replication_setting',
      `${path}.value`,
      'Replication setting value must be an object.',
    );
  }

  const mode = replicationStringValue(
    rawValue,
    'mode',
    `${path}.value.mode`,
    BLUEPRINT_VARIABLE_REPLICATION_MODES,
    'invalid_replication_mode',
    'Use one of: none, replicated, rep_notify.',
  );
  const condition = rawValue['condition'] === undefined
    ? 'none'
    : replicationStringValue(
        rawValue,
        'condition',
        `${path}.value.condition`,
        BLUEPRINT_VARIABLE_REPLICATION_CONDITIONS,
        'invalid_replication_condition',
        'Use a public UE editor-facing replication condition.',
      );

  if (mode === 'none' && condition !== 'none') {
    throwReplicationCompileError(
      'replication_condition_requires_networked_mode',
      `${path}.value.condition`,
      'Replication condition is accepted only for replicated and rep_notify modes.',
    );
  }

  return {
    ...rawSetting,
    property_path: 'replication',
    value: omitUndefined({
      mode,
      condition,
      notify_function: mode === 'rep_notify'
        ? optionalNonEmptyString(rawValue, 'notify_function', `${path}.value.notify_function`) ?? `OnRep_${variableName}`
        : undefined,
      create_notify_function: optionalBoolean(rawValue, 'create_notify_function', true, `${path}.value.create_notify_function`),
      reuse_existing_notify_function: optionalBoolean(rawValue, 'reuse_existing_notify_function', false, `${path}.value.reuse_existing_notify_function`),
    }),
  };
}

function replicationStringValue(
  record: Record<string, unknown>,
  field: string,
  path: string,
  allowedValues: readonly string[],
  code: string,
  message: string,
): string {
  const value = record[field];
  if (typeof value === 'string' && allowedValues.includes(value)) {
    return value;
  }
  throwReplicationCompileError(code, path, message);
}

function optionalNonEmptyString(record: Record<string, unknown>, field: string, path: string): string | undefined {
  const value = record[field];
  if (value === undefined || value === null) {
    return undefined;
  }
  if (typeof value === 'string' && value.trim().length > 0) {
    return value.trim();
  }
  throwReplicationCompileError(
    'rep_notify_function_missing',
    path,
    'RepNotify function name must be a non-empty string when provided.',
  );
}

function optionalBoolean(record: Record<string, unknown>, field: string, fallback: boolean, path: string): boolean {
  const value = record[field];
  if (value === undefined || value === null) {
    return fallback;
  }
  if (typeof value === 'boolean') {
    return value;
  }
  throwReplicationCompileError(
    'invalid_replication_setting',
    path,
    `${field} must be a boolean.`,
  );
}

function throwReplicationCompileError(code: string, path: string, message: string): never {
  throw new TaskSpecCompileError('taskspec_semantic_invalid', message, [
    {
      code,
      path,
      message,
    },
  ]);
}

function compileMemberDefaultFromVariableEntry(rawEntry: unknown, path: string): BlueprintVariableCompiledOp | undefined {
  if (!isRecord(rawEntry) || !Object.hasOwn(rawEntry, 'default')) {
    return undefined;
  }
  const kind = rawEntry['kind'];
  const op = rawEntry['op'];
  if (
    (kind !== undefined && kind !== 'ensure_member_variable') ||
    (op !== undefined && op !== 'ensure_member_variable')
  ) {
    return undefined;
  }
  return {
    op: 'set_member_default',
    name: getRequiredString(rawEntry, 'name', `${path}.name`),
    value: literalValue(rawEntry['default']),
  };
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
      settings: normalizeLocalVariablePropertySettings(rawEntry, 'properties', `${path}.properties`),
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

function normalizeLocalVariablePropertySettings(
  record: Record<string, unknown>,
  field: string,
  path: string,
): unknown[] {
  return requiredNonEmptyArray(record, field, path)
    .map((rawSetting, index) => normalizeLocalVariablePropertySetting(rawSetting, `${path}[${index}]`));
}

function normalizeLocalVariablePropertySetting(rawSetting: unknown, path: string): unknown {
  if (!isRecord(rawSetting) || rawSetting['property_path'] !== 'replication') {
    return rawSetting;
  }

  throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Local variable replication is unsupported; use member_variables.', [
    {
      code: 'local_variable_replication_unsupported',
      path: `${path}.property_path`,
      message: 'Replication is only supported for member variables.',
      suggested_patch: { op: 'replace', path: '/behavior/variable_strategy', value: 'member_variables' },
    },
  ]);
}

function variablePinType(entry: Record<string, unknown>, path: string): Record<string, unknown> {
  if (isRecord(entry['pin_type'])) return entry['pin_type'];
  if (isRecord(entry['variable_type'])) return entry['variable_type'];
  if (typeof entry['type'] === 'string' && entry['type'].trim().length > 0) {
    return { category: entry['type'] };
  }
  throwMissingVariableType(`${path}.type`);
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
      message: 'Provide type or variable_type, for example {"category":"bool"}.',
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
