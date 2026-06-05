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
} from '../schema/task-schemas.js';
import { BlueprintPinTypeSpecSchema } from '../schema/blueprint-pin-type-spec.js';
import {
  BLUEPRINT_VARIABLE_REPLICATION_CONDITIONS,
  BLUEPRINT_VARIABLE_REPLICATION_MODES,
  CONTAINER_ACTION_OPERATIONS_BY_KIND,
  CONTAINER_ACTION_ROLE_FIELDS,
  CONTAINER_ACTION_TYPE_FIELDS,
  TASK_PLAN_SCHEMA,
  getContainerActionResultOutputPin,
  getRequiredContainerActionRoles,
  isExpressionContainerActionOperation,
  isValueExpressionContainerActionOperation,
  isSupportedContainerActionKind,
  isSupportedContainerActionOperation,
} from '../schema/task-schemas.js';
import {
  collectGraphWriteConnectivityPreflightIssues,
} from './graphwrite-connectivity-preflight.js';

export const TASK_COMPILER_RESULT_SCHEMA = 'BlueprintHelper.TaskCompilerResult.v1';

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

export type TaskCompilerStrategyId = 'canonical_ts';

export interface TaskCompileOptions {
  dryRun: boolean;
  diagnostics?: boolean;
}

export interface TaskCompileDiagnostics {
  bridgePayload?: Record<string, unknown>;
  taskPlanSummary?: Record<string, unknown>;
  compilerOutputBytes?: number;
  [key: string]: unknown;
}

export interface CompiledTaskPlan {
  schema: typeof TASK_COMPILER_RESULT_SCHEMA;
  taskPlan: TaskPlan;
  strategyId: TaskCompilerStrategyId;
  diagnostics?: TaskCompileDiagnostics;
}

export interface TaskCompilerStrategy {
  readonly id: TaskCompilerStrategyId;
  canCompile(taskSpec: TaskSpec, options?: TaskCompileOptions): boolean;
  compile(taskSpec: TaskSpec, options: TaskCompileOptions): Promise<CompiledTaskPlan>;
}

export function createCompiledTaskPlan(input: {
  taskPlan: TaskPlan;
  strategyId: TaskCompilerStrategyId;
  diagnostics?: TaskCompileDiagnostics;
}): CompiledTaskPlan {
  return {
    schema: TASK_COMPILER_RESULT_SCHEMA,
    taskPlan: input.taskPlan,
    strategyId: input.strategyId,
    ...(input.diagnostics ? { diagnostics: input.diagnostics } : {}),
  };
}

type TaskPlanStep = TaskPlan['steps'][number];
const COMPONENT_TRANSFORM_POLICIES = ['preserve_world', 'preserve_relative', 'reset_relative'] as const;
const COMPONENT_OLD_ROOT_POLICIES = ['keep_as_child', 'remove_default_scene_root_when_empty'] as const;
const COMPONENT_DEFAULT_ROOT_POLICIES = ['require_scene_component', 'create_default_scene_root_when_needed'] as const;
const COMPONENT_DELETE_POLICIES = [
  'block_if_children',
  'promote_children',
  'delete_owned_children',
  'reattach_children_to_parent',
] as const;

export function compileTaskSpecToTaskPlan(taskSpec: TaskSpec): TaskPlan {
  if (taskSpec.task_type === 'create_asset') {
    return compileAssetFactoryTaskSpecToTaskPlan(taskSpec);
  }
  if (taskSpec.task_type === 'create_blueprint_feature') {
    return compileCompositeBlueprintFeatureTaskSpecToTaskPlan(taskSpec);
  }
  if (taskSpec.task_type === 'edit_blueprint_variables') {
    return compileBlueprintVariablesTaskSpecToTaskPlan(taskSpec);
  }
  if (taskSpec.task_type === 'edit_object_properties') {
    return compileObjectPropertiesTaskSpecToTaskPlan(taskSpec);
  }
  if (taskSpec.task_type === 'edit_blueprint_signature') {
    return compileBlueprintSignatureTaskSpecToTaskPlan(taskSpec);
  }
  if (taskSpec.task_type === 'edit_blueprint_class_settings') {
    return compileBlueprintClassSettingsTaskSpecToTaskPlan(taskSpec);
  }
  if (taskSpec.task_type === 'edit_blueprint_components') {
    return compileBlueprintComponentsTaskSpecToTaskPlan(taskSpec);
  }
  if (taskSpec.task_type === 'edit_umg_widget') {
    return compileUMGWidgetTaskSpecToTaskPlan(taskSpec);
  }
  if (taskSpec.task_type === 'edit_data_table') {
    return compileDataTableTaskSpecToTaskPlan(taskSpec);
  }
  if (taskSpec.task_type !== 'edit_blueprint_graph') {
    throw new TaskSpecCompileError('unsupported_task_type', `Unsupported TaskSpec task_type: ${taskSpec.task_type}`, [
      {
        code: 'unsupported_task_type',
        path: 'task_type',
        message: 'The canonical TypeScript compiler currently supports AssetFactory, GraphWrite, Blueprint Variables, Signature, ObjectProperty, ClassSettings, Components, UMG Widget, DataTable, and composite feature slices.',
      },
    ]);
  }

  assertSupportedTaskSpec(taskSpec);
  const behavior = taskSpec.behavior as Record<string, unknown>;
  const graphWriteOps = compileGraphWriteOps(behavior, {
    defaultFieldOwnerClass: defaultFieldOwnerClassForBlueprintAsset(taskSpec.target.asset_path),
  });

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
    steps: makeGraphWriteTaskPlanSteps(taskSpec, graphWriteOps),
  };
}

function compileAssetFactoryTaskSpecToTaskPlan(
  taskSpec: Extract<TaskSpec, { task_type: 'create_asset' }>,
): TaskPlan {
  const asset = taskSpec.behavior.asset as Record<string, unknown>;
  const op = omitUndefined({
    op: 'create_asset',
    asset_type: getRequiredString(asset, 'asset_type', 'behavior.asset.asset_type'),
    parent_class: optionalString(asset, 'parent_class'),
    value_type: optionalString(asset, 'value_type'),
    fields: Array.isArray(asset['fields']) ? asset['fields'] : undefined,
    row_struct: optionalString(asset, 'row_struct'),
    data_asset_class: optionalString(asset, 'data_asset_class'),
    collision: optionalString(asset, 'collision') ?? optionalString(asset, 'collision_policy'),
  }) as { op: string; [key: string]: unknown };

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
    steps: [{
      step_id: 'step_001',
      capability: 'asset_factory',
      target: {
        asset_path: taskSpec.target.asset_path,
      },
      write: {
        strategy: 'asset_create',
        ops: [op],
      },
    }],
  };
}

function compileCompositeBlueprintFeatureTaskSpecToTaskPlan(
  taskSpec: Extract<TaskSpec, { task_type: 'create_blueprint_feature' }>,
): TaskPlan {
  assertSupportedCompositeBlueprintFeatureTaskSpec(taskSpec);

  const steps: TaskPlanStep[] = [];
  steps.push(...compileCompositeComponentSteps(taskSpec));
  steps.push(...compileCompositeVariableSteps(taskSpec));
  steps.push(...compileCompositeClassSettingsSteps(taskSpec));
  steps.push(...compileCompositeGraphWriteSteps(taskSpec));
  steps.push(...compileCompositeIntegrationSteps(taskSpec));

  if (steps.length === 0) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'create_blueprint_feature did not produce any TaskPlan steps.', [
      {
        code: 'empty_composite_feature',
        path: 'task_spec',
        message: 'Provide components, variables, class_settings, or behavior.',
      },
    ]);
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
      review_baseline_dirty_asset_policy: taskSpec.execution_policy.review_baseline_dirty_asset_policy ?? 'block',
    },
    steps: renumberSteps(steps),
  };
}

function assertSupportedCompositeBlueprintFeatureTaskSpec(taskSpec: Extract<TaskSpec, { task_type: 'create_blueprint_feature' }>): void {
  const integration = asRecord((taskSpec as Record<string, unknown>)['integration']);
  if (integration) {
    const unsupportedKeys = Object.keys(integration).filter((key) => key !== 'interface');
    if (unsupportedKeys.length > 0) {
      throw new TaskSpecCompileError('unsupported_composite_integration', 'Unsupported composite integration fields.', [
        {
          code: 'unsupported_composite_integration',
          path: `integration.${unsupportedKeys[0]}`,
          message: 'Input binding and other integration fields need dedicated capability clusters; keep only integration.interface for this slice.',
        },
      ]);
    }
  }

  const interfaceIntegration = asRecord(integration?.['interface']);
  if (integration && !interfaceIntegration) {
    throw new TaskSpecCompileError('unsupported_composite_integration', 'Unsupported composite integration fields.', [
      {
        code: 'unsupported_composite_integration',
        path: 'integration',
        message: 'integration currently supports only an interface object.',
      },
    ]);
  }

  const scopePolicy = asRecord((taskSpec as Record<string, unknown>)['scope_policy']);
  if (scopePolicy?.['allow_create_assets'] === true) {
    throw new TaskSpecCompileError('unsupported_composite_asset_creation', 'Composite asset creation is not supported in this slice.', [
      {
        code: 'unsupported_composite_asset_creation',
        path: 'scope_policy.allow_create_assets',
        message: 'Set allow_create_assets=false and reference existing assets, or split asset creation into create_asset TaskSpecs.',
      },
    ]);
  }
}

function compileCompositeComponentSteps(taskSpec: Extract<TaskSpec, { task_type: 'create_blueprint_feature' }>): TaskPlanStep[] {
  const rawComponents = (taskSpec as Record<string, unknown>)['components'];
  if (!Array.isArray(rawComponents) || rawComponents.length === 0) return [];

  const assetPolicy = asRecord((taskSpec as Record<string, unknown>)['asset_policy']);
  const nameCollisionPolicy = normalizeComponentCollisionPolicy(assetPolicy?.['if_component_exists']);
  const steps: TaskPlanStep[] = [];
  rawComponents.forEach((rawComponent, componentIndex) => {
    if (!isRecord(rawComponent)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Component entry must be an object.', [
        {
          code: 'invalid_component',
          path: `components[${componentIndex}]`,
          message: 'Component entry must be an object.',
        },
      ]);
    }
    const component = rawComponent as Record<string, unknown>;
    const addOp = omitUndefined({
      op: 'add_component',
      component_name: getRequiredString(component, 'name', `components[${componentIndex}].name`),
      component_class: getRequiredString(component, 'class', `components[${componentIndex}].class`),
      parent_component: compositeComponentParent(component),
      socket_name: compositeComponentSocket(component),
      attach_rule: compositeComponentAttachRule(component),
      name_collision_policy: normalizeComponentCollisionPolicy(component['on_name_conflict']) ?? nameCollisionPolicy,
    });
    const addStep = makeCompositeCapabilityStep(steps.length + 1, 'blueprint_component', taskSpec.target.asset_path, 'component_tree', [addOp]);
    steps.push(addStep);

    const settings = compositeSettingsArray(component['properties'], `components[${componentIndex}].properties`, taskSpec);
    if (settings.length > 0) {
      steps.push({
        ...makeCompositeCapabilityStep(steps.length + 1, 'blueprint_component', taskSpec.target.asset_path, 'component_tree', [{
        op: 'set_component_properties',
        component_name: addOp.component_name,
        settings,
        }]),
        depends_on: [addStep.step_id],
      } as TaskPlanStep);
    }
  });
  return steps;
}

function compileCompositeVariableSteps(taskSpec: Extract<TaskSpec, { task_type: 'create_blueprint_feature' }>): TaskPlanStep[] {
  const rawVariables = (taskSpec as Record<string, unknown>)['variables'];
  if (!Array.isArray(rawVariables) || rawVariables.length === 0) return [];

  const ensureOps: Record<string, unknown>[] = [];
  const defaultOps: Record<string, unknown>[] = [];
  rawVariables.forEach((rawVariable, variableIndex) => {
    if (!isRecord(rawVariable)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Variable entry must be an object.', [
        {
          code: 'invalid_variable',
          path: `variables[${variableIndex}]`,
          message: 'Variable entry must be an object.',
        },
      ]);
    }
    const variable = rawVariable as Record<string, unknown>;
    const name = getRequiredString(variable, 'name', `variables[${variableIndex}].name`);
    ensureOps.push(omitUndefined({
      op: 'ensure_member_variable',
      name,
      pin_type: compositeVariablePinType(variable, `variables[${variableIndex}]`),
      category: variable['category'],
      tooltip: variable['tooltip'],
      flags: variable['flags'],
      metadata: variable['metadata'],
    }));
    if (Object.hasOwn(variable, 'default')) {
      defaultOps.push({
        op: 'set_member_default',
        name,
        value: literalValue(variable['default']),
      });
    }
  });

  const steps: TaskPlanStep[] = [];
  if (ensureOps.length > 0) {
    steps.push({
      step_id: 'step_001',
      capability: 'blueprint_variable',
      target: { asset_path: taskSpec.target.asset_path },
      write: { strategy: 'member_variables', ops: ensureOps },
      constraints: { allow_remove_referenced_variables: false },
    } as TaskPlanStep);
  }
  if (defaultOps.length > 0) {
    steps.push({
      step_id: 'step_001',
      capability: 'blueprint_variable',
      target: { asset_path: taskSpec.target.asset_path },
      write: { strategy: 'member_defaults', ops: defaultOps },
      constraints: { allow_remove_referenced_variables: false },
    } as TaskPlanStep);
  }
  return steps;
}

function compileCompositeClassSettingsSteps(taskSpec: Extract<TaskSpec, { task_type: 'create_blueprint_feature' }>): TaskPlanStep[] {
  const classSettings = asRecord((taskSpec as Record<string, unknown>)['class_settings']);
  if (!classSettings) return [];

  if (Object.hasOwn(classSettings, 'parent_class')) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Use class_settings.reparent.new_parent_class for Blueprint reparent operations.', [
      {
        code: 'unsupported_composite_class_settings_parent_class',
        path: 'class_settings.parent_class',
        message: 'Use class_settings.reparent.new_parent_class.',
      },
    ]);
  }

  const steps: TaskPlanStep[] = [];
  const rawInterfaces = classSettings['implemented_interfaces'];
  if (Array.isArray(rawInterfaces) && rawInterfaces.length > 0) {
    steps.push(makeCompositeCapabilityStep(steps.length + 1, 'blueprint_class_settings', taskSpec.target.asset_path, 'class_settings', [{
      op: 'add_implemented_interfaces',
      interface_paths: rawInterfaces.map((value, index) => {
        if (typeof value !== 'string' || value.length === 0) {
          throw new TaskSpecCompileError('taskspec_semantic_invalid', 'implemented_interfaces must contain path strings.', [
            {
              code: 'invalid_interface_path',
              path: `class_settings.implemented_interfaces[${index}]`,
              message: 'Provide an interface asset path string.',
            },
          ]);
        }
        return resolveCompositeReference(value, taskSpec);
      }),
    }]));
  }

  const classDefaults = compositeSettingsArray(classSettings['class_defaults'], 'class_settings.class_defaults', taskSpec);
  if (classDefaults.length > 0) {
    steps.push(makeCompositeCapabilityStep(steps.length + 1, 'blueprint_class_settings', taskSpec.target.asset_path, 'class_settings', [{
      op: 'set_class_default_properties',
      settings: classDefaults,
    }]));
  }

  const reparent = asRecord(classSettings['reparent']);
  if (reparent) {
    steps.push(makeCompositeCapabilityStep(steps.length + 1, 'blueprint_class_settings', taskSpec.target.asset_path, 'class_settings', [{
      op: 'reparent_blueprint',
      new_parent_class: resolveCompositeReference(
        getRequiredString(reparent, 'new_parent_class', 'class_settings.reparent.new_parent_class'),
        taskSpec,
      ),
    }]));
  }

  return steps;
}

function compileCompositeGraphWriteSteps(taskSpec: Extract<TaskSpec, { task_type: 'create_blueprint_feature' }>): TaskPlanStep[] {
  const behavior = asRecord((taskSpec as Record<string, unknown>)['behavior']);
  if (!behavior) return [];
  const scopePolicy = asRecord((taskSpec as Record<string, unknown>)['scope_policy']);
  if (!scopePolicy) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'create_blueprint_feature behavior requires scope_policy.', [
      {
        code: 'missing_scope_policy',
        path: 'scope_policy',
        message: 'Provide scope_policy.graph_name and allow_modify_user_nodes.',
      },
    ]);
  }
  const graphName = getRequiredString(scopePolicy, 'graph_name', 'scope_policy.graph_name');
  const graphTaskSpec = {
    ...taskSpec,
    task_type: 'edit_blueprint_graph',
    scope_policy: {
      graph_name: graphName,
      allow_modify_user_nodes: scopePolicy['allow_modify_user_nodes'] === true,
    },
    behavior,
  } as Extract<TaskSpec, { task_type: 'edit_blueprint_graph' }>;
  assertSupportedTaskSpec(graphTaskSpec);
  return makeGraphWriteTaskPlanSteps(graphTaskSpec, compileGraphWriteOps(behavior, {
    defaultFieldOwnerClass: defaultFieldOwnerClassForBlueprintAsset(taskSpec.target.asset_path),
  }));
}

function compileCompositeIntegrationSteps(taskSpec: Extract<TaskSpec, { task_type: 'create_blueprint_feature' }>): TaskPlanStep[] {
  const integration = asRecord((taskSpec as Record<string, unknown>)['integration']);
  const interfaceIntegration = asRecord(integration?.['interface']);
  if (!interfaceIntegration) return [];

  const interfacePath = resolveCompositeReference(
    getRequiredString(interfaceIntegration, 'interface_asset', 'integration.interface.interface_asset'),
    taskSpec,
  );
  const functionName = getRequiredString(interfaceIntegration, 'function', 'integration.interface.function');
  const steps: TaskPlanStep[] = [];
  let classSettingsStepId: string | undefined;

  if (!compositeClassSettingsIncludesInterface(taskSpec, interfacePath)) {
    const classSettingsStep = makeCompositeCapabilityStep(steps.length + 1, 'blueprint_class_settings', taskSpec.target.asset_path, 'class_settings', [{
      op: 'add_implemented_interfaces',
      interface_paths: [interfacePath],
    }]);
    classSettingsStepId = classSettingsStep.step_id;
    steps.push(classSettingsStep);
  }

  const signatureStep = {
    ...makeCompositeCapabilityStep(steps.length + 1, 'blueprint_signature', taskSpec.target.asset_path, 'function_signature', [{
    op: 'ensure_function',
    function_name: functionName,
    interface_path: interfacePath,
    name_collision_policy: 'reuse_if_exists',
    }]),
    ...(classSettingsStepId ? { depends_on: [classSettingsStepId] } : {}),
  } as TaskPlanStep;
  steps.push(signatureStep);

  steps.push({
    step_id: `step_${String(steps.length + 1).padStart(3, '0')}`,
    capability: 'graph_write',
    target: {
      asset_path: taskSpec.target.asset_path,
      graph: functionName,
    },
    write: {
      strategy: 'owned_graph_edit',
      ops: [
        {
          op: 'replace_body',
          replace_scope: 'function_body',
          selector: {
            function_name: functionName,
          },
          replacement: compileCompositeInterfaceImplementationReplacement(interfaceIntegration, functionName),
          options: {
            strict: true,
          },
        },
      ],
    },
    constraints: {
      allow_modify_user_nodes: false,
      ownership_scope: 'blueprinthelper_owned',
    },
    depends_on: [signatureStep.step_id],
  } as TaskPlanStep);

  return steps;
}

function makeCompositeCapabilityStep(
  index: number,
  capability: string,
  assetPath: string,
  strategy: string,
  ops: Record<string, unknown>[],
): TaskPlanStep {
  return {
    step_id: `step_${String(index).padStart(3, '0')}`,
    capability,
    target: { asset_path: assetPath },
    write: { strategy, ops },
  } as TaskPlanStep;
}

function compositeClassSettingsIncludesInterface(
  taskSpec: Extract<TaskSpec, { task_type: 'create_blueprint_feature' }>,
  interfacePath: string,
): boolean {
  const classSettings = asRecord((taskSpec as Record<string, unknown>)['class_settings']);
  const rawInterfaces = classSettings?.['implemented_interfaces'];
  return Array.isArray(rawInterfaces) && rawInterfaces.some((value) => (
    typeof value === 'string' && resolveCompositeReference(value, taskSpec) === interfacePath
  ));
}

function compileCompositeInterfaceImplementationReplacement(
  interfaceIntegration: Record<string, unknown>,
  functionName: string,
): { nodes: AgentImportNode[]; links: AgentImportLink[] } {
  const body = compositeInterfaceImplementationBody(interfaceIntegration, functionName);
  validateSupportedStatements(body.statements, 'integration.interface.implementation.body.statements');
  return compileLogicBodyToImportPayload(
    body,
    `interface_${functionName}`,
    'integration.interface.implementation.body',
  );
}

function compositeInterfaceImplementationBody(
  interfaceIntegration: Record<string, unknown>,
  functionName: string,
): { statements: BlueprintLogicStatement[] } {
  const implementation = requiredRecord(interfaceIntegration, 'implementation', 'integration.interface.implementation');
  if (isRecord(implementation['body'])) {
    return getRequiredLogicBody(implementation, 'body', 'integration.interface.implementation.body');
  }
  if (Array.isArray(implementation['statements'])) {
    return {
      statements: implementation['statements'] as BlueprintLogicStatement[],
    };
  }
  if (typeof implementation['call'] === 'string' && implementation['call'].trim().length > 0) {
    return {
      statements: [
        {
          kind: 'call',
          target: implementation['call'],
          args: implementation['args'],
        } as BlueprintLogicStatement,
      ],
    };
  }
  throw new TaskSpecCompileError('taskspec_semantic_invalid', 'integration.interface.implementation must provide call, body, or statements.', [
    {
      code: 'missing_interface_implementation',
      path: 'integration.interface.implementation',
      message: `Provide implementation.call="${functionName}" target logic or a BlueprintLogicSpec body.`,
    },
  ]);
}

function renumberSteps(steps: TaskPlanStep[]): TaskPlanStep[] {
  const oldIds = steps.map((step) => step.step_id);
  const newIds = steps.map((_, index) => `step_${String(index + 1).padStart(3, '0')}`);

  return steps.map((step, index) => ({
    ...step,
    step_id: newIds[index],
    ...('depends_on' in step && Array.isArray(step.depends_on)
      ? {
          depends_on: step.depends_on.map((id) => {
            for (let candidate = index - 1; candidate >= 0; candidate -= 1) {
              if (oldIds[candidate] === id) return newIds[candidate];
            }
            return id;
          }),
        }
      : {}),
  }));
}

function makeGraphWriteTaskPlanSteps(
  taskSpec: Extract<TaskSpec, { task_type: 'edit_blueprint_graph' }>,
  graphWriteOps: GraphWriteCompiledOp[],
): TaskPlanStep[] {
  const behavior = taskSpec.behavior as Record<string, unknown>;
  const strategy = String(behavior['graph_strategy']);
  const autoSearchPolicy = graphWriteAutoSearchPolicyForTaskSpec(taskSpec);
  if (strategy === 'append_new_owned_graph') {
    const signatureOps = graphWriteOps.filter((op) => graphWriteEnsureEntryEventKind(op) === 'custom_event');
    const signatureSteps = signatureOps.map((op, index) => ({
      step_id: `step_${String(index + 1).padStart(3, '0')}`,
      capability: 'blueprint_signature',
      target: {
        asset_path: taskSpec.target.asset_path,
      },
      write: {
        strategy: 'custom_event_signature',
        ops: [
          omitUndefined({
            op: 'ensure_custom_event',
            event_name: String(op['name']),
            graph_name: taskSpec.scope_policy.graph_name,
            inputs: (op as GraphWriteCompiledOp & { __signature_split?: GraphWriteSignatureSplit }).__signature_split?.inputs,
            name_collision_policy: 'reuse_if_exists',
          }),
        ],
      },
    } as TaskPlanStep));
    const signatureStepIds = signatureSteps.map((step) => step.step_id);
    const graphWriteSteps = graphWriteOps.map((op, index) => {
      const stepId = `step_${String(signatureSteps.length + index + 1).padStart(3, '0')}`;
      const previousGraphWriteStepId = index > 0
        ? `step_${String(signatureSteps.length + index).padStart(3, '0')}`
        : undefined;
      return {
        step_id: stepId,
        capability: 'graph_write',
        target: {
          asset_path: taskSpec.target.asset_path,
          graph: targetGraphForGraphWriteOp(taskSpec, op),
        },
        write: {
          strategy: 'owned_graph_edit',
          ...graphWriteAutoSearchPolicyWriteField(autoSearchPolicy),
          ops: [withSignatureEvidence(stripGraphWriteCompilerMetadata(op))],
        },
        constraints: {
          allow_modify_user_nodes: taskSpec.scope_policy.allow_modify_user_nodes,
          ownership_scope: 'blueprinthelper_owned',
        },
        depends_on: previousGraphWriteStepId
          ? [...signatureStepIds, previousGraphWriteStepId]
          : signatureStepIds,
      } as TaskPlanStep;
    });
    return [...signatureSteps, ...graphWriteSteps];
  }

  if (strategy === 'replace_owned_graph' && graphWriteOps.length === 1) {
    const op = graphWriteOps[0] as GraphWriteCompiledOp & { __signature_split?: GraphWriteSignatureSplit };
    if (op.__signature_split) {
      const signatureOp = op.__signature_split;
      return [
        {
          step_id: 'step_001',
          capability: 'blueprint_signature',
          target: {
            asset_path: taskSpec.target.asset_path,
          },
          write: {
            strategy: 'custom_event_signature',
            ops: [
              omitUndefined({
                op: signatureOp.op,
                event_name: signatureOp.event_name,
                graph_name: taskSpec.scope_policy.graph_name,
                inputs: signatureOp.inputs,
                name_collision_policy: signatureOp.name_collision_policy,
              }),
            ],
          },
        } as TaskPlanStep,
        {
          step_id: 'step_002',
          capability: 'graph_write',
          target: {
            asset_path: taskSpec.target.asset_path,
            graph: targetGraphForGraphWriteOp(taskSpec, op),
          },
          write: {
            strategy: 'owned_graph_edit',
            ...graphWriteAutoSearchPolicyWriteField(autoSearchPolicy),
            ops: [withSignatureEvidence(stripGraphWriteCompilerMetadata(op))],
          },
          constraints: {
            allow_modify_user_nodes: taskSpec.scope_policy.allow_modify_user_nodes,
            ownership_scope: 'blueprinthelper_owned',
          },
          depends_on: ['step_001'],
        } as TaskPlanStep,
      ];
    }
  }

  if (strategy === 'merge_external_flow' || strategy === 'patch_external_graph' || strategy === 'replace_external_body') {
    const mutationPolicyByStrategy: Record<string, string[]> = {
      merge_external_flow: ['exec_boundary_link'],
      patch_external_graph: ['pin_default', 'node_comment'],
      replace_external_body: ['body_replace'],
    };
    return graphWriteOps.map((op, index) => ({
      step_id: `step_${String(index + 1).padStart(3, '0')}`,
      capability: 'graph_write',
      target: {
        asset_path: taskSpec.target.asset_path,
        graph: targetGraphForGraphWriteOp(taskSpec, op),
      },
      write: {
        strategy: 'external_graph_edit',
        ...graphWriteAutoSearchPolicyWriteField(autoSearchPolicy),
        ops: [stripGraphWriteCompilerMetadata(op)],
      },
      constraints: {
        allow_modify_user_nodes: false,
        ownership_scope: 'external_user_authored',
        external_mutation_policy: {
          strategy,
          allowed_mutations: mutationPolicyByStrategy[strategy],
        },
      },
    } as TaskPlanStep));
  }

  const opBatches = strategy === 'append_new_owned_graph'
    ? [graphWriteOps]
    : graphWriteOps.map((op) => [stripGraphWriteCompilerMetadata(op)]);

  return opBatches.map((ops, index) => ({
    step_id: `step_${String(index + 1).padStart(3, '0')}`,
    capability: 'graph_write',
    target: {
      asset_path: taskSpec.target.asset_path,
      graph: ops.length === 1 ? targetGraphForGraphWriteOp(taskSpec, ops[0]) : taskSpec.scope_policy.graph_name,
    },
    write: {
      strategy: 'owned_graph_edit',
      ...graphWriteAutoSearchPolicyWriteField(autoSearchPolicy),
      ops,
    },
    constraints: {
      allow_modify_user_nodes: taskSpec.scope_policy.allow_modify_user_nodes,
      ownership_scope: 'blueprinthelper_owned',
    },
  }));
}

function graphWriteAutoSearchPolicyForTaskSpec(
  taskSpec: Extract<TaskSpec, { task_type: 'edit_blueprint_graph' }>,
): Record<string, unknown> | undefined {
  const policy = (taskSpec as Record<string, unknown>)['graph_write_policy'];
  if (!isRecord(policy) || !isRecord(policy['auto_search'])) {
    return undefined;
  }
  return policy['auto_search'];
}

function graphWriteAutoSearchPolicyWriteField(
  autoSearchPolicy: Record<string, unknown> | undefined,
): Record<string, unknown> {
  if (!autoSearchPolicy) {
    return {};
  }
  const sanitized = omitUndefined({
    mode: typeof autoSearchPolicy['mode'] === 'string' ? autoSearchPolicy['mode'] : undefined,
    max_candidates_per_statement: typeof autoSearchPolicy['max_candidates_per_statement'] === 'number'
      ? autoSearchPolicy['max_candidates_per_statement']
      : undefined,
    max_auto_search_statements: typeof autoSearchPolicy['max_auto_search_statements'] === 'number'
      ? autoSearchPolicy['max_auto_search_statements']
      : undefined,
    max_total_auto_search_ms: typeof autoSearchPolicy['max_total_auto_search_ms'] === 'number'
      ? autoSearchPolicy['max_total_auto_search_ms']
      : undefined,
    detail_level: typeof autoSearchPolicy['detail_level'] === 'string' ? autoSearchPolicy['detail_level'] : undefined,
  });
  return Object.keys(sanitized).length > 0 ? { auto_search_policy: sanitized } : {};
}

function stripGraphWriteCompilerMetadata(op: GraphWriteCompiledOp): GraphWriteCompiledOp {
  const { __signature_split: _signatureSplit, ...cleanOp } = op as GraphWriteCompiledOp & { __signature_split?: unknown };
  return cleanOp as GraphWriteCompiledOp;
}

function targetGraphForGraphWriteOp(taskSpec: TaskSpec, op: GraphWriteCompiledOp): string {
  if (op.op !== 'replace_body' || !isRecord(op.selector)) {
    return taskSpec.scope_policy.graph_name;
  }

  if (op.replace_scope === 'function_body' && typeof op.selector.function_name === 'string' && op.selector.function_name.trim().length > 0) {
    return op.selector.function_name.trim();
  }

  if (typeof op.selector.graph_id === 'string' && op.selector.graph_id.trim().length > 0) {
    return op.selector.graph_id.trim();
  }

  return taskSpec.scope_policy.graph_name;
}

function withSignatureEvidence(op: GraphWriteCompiledOp): GraphWriteCompiledOp {
  if (op.op !== 'ensure_entry' || typeof op.name !== 'string' || op.name.trim().length === 0) {
    return op;
  }
  if (typeof op.signature_evidence_id === 'string' && op.signature_evidence_id.trim().length > 0) {
    return {
      ...op,
      signature_evidence_id: op.signature_evidence_id.trim(),
    };
  }
  if (graphWriteEnsureEntryEventKind(op) !== 'custom_event') {
    return op;
  }
  return {
    ...op,
    signature_evidence_id: makeCustomEventSignatureEvidenceId(op.name.trim()),
  };
}

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

function compileObjectPropertiesTaskSpecToTaskPlan(taskSpec: Extract<TaskSpec, { task_type: 'edit_object_properties' }>): TaskPlan {
  const behavior = taskSpec.behavior as Record<string, unknown>;
  assertExactString(
    behavior,
    'property_strategy',
    'property_edit',
    'behavior.property_strategy',
    'Use property_strategy="property_edit".',
  );

  const changes = requiredArray(behavior, 'changes', 'behavior.changes');
  const settings = changes.map((rawChange, index) => {
    const path = `behavior.changes[${index}]`;
    if (!isRecord(rawChange)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object.`, [
        { code: 'invalid_property_change', path, message: 'Use { property_path, value }.' },
      ]);
    }
    const change = rawChange as Record<string, unknown>;
    if (!Object.hasOwn(change, 'value')) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}.value is required.`, [
        { code: 'missing_property_value', path: `${path}.value`, message: 'Provide value.' },
      ]);
    }
    return {
      property_path: getRequiredString(change, 'property_path', `${path}.property_path`),
      value: literalValue(change['value']),
    };
  });

  const op = settings.length === 1
    ? {
        op: 'set_object_property',
        property_path: settings[0].property_path,
        value: settings[0].value,
      }
    : {
        op: 'set_object_properties',
        settings,
      };

  return makeSingleCapabilityTaskPlan(
    taskSpec,
    'object_property',
    'property_edit',
    [op],
    { property_scope: 'uobject' },
  );
}

function compileBlueprintComponentsTaskSpecToTaskPlan(
  taskSpec: Extract<TaskSpec, { task_type: 'edit_blueprint_components' }>,
): TaskPlan {
  const behavior = taskSpec.behavior as Record<string, unknown>;
  assertExactString(
    behavior,
    'component_strategy',
    'component_tree',
    'behavior.component_strategy',
    'Use component_strategy="component_tree".',
  );

  const changes = requiredArray(behavior, 'changes', 'behavior.changes');
  const steps: TaskPlanStep[] = [];

  changes.forEach((rawChange, changeIndex) => {
    const path = `behavior.changes[${changeIndex}]`;
    if (!isRecord(rawChange)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object.`, [
        { code: 'invalid_component_change', path, message: 'Provide a component change object.' },
      ]);
    }

    const change = rawChange as Record<string, unknown>;
    const kind = getRequiredString(change, 'kind', `${path}.kind`);
    if (kind === 'ensure_component_present') {
      const addOp = omitUndefined({
        op: 'add_component',
        component_name: getRequiredString(change, 'name', `${path}.name`),
        component_class: getRequiredString(change, 'class', `${path}.class`),
        parent_component: componentParent(change),
        socket_name: componentSocket(change),
        attach_rule: componentAttachRule(change),
        name_collision_policy: normalizeComponentCollisionPolicy(change['name_collision_policy'])
          ?? normalizeComponentCollisionPolicy(change['on_name_conflict']),
      });
      const addStep = makeCompositeCapabilityStep(
        steps.length + 1,
        'blueprint_component',
        taskSpec.target.asset_path,
        'component_tree',
        [addOp],
      );
      steps.push(addStep);

      const settings = propertySettingsArray(change['properties'], `${path}.properties`, false, 'component');
      if (settings.length > 0) {
        steps.push({
          ...makeCompositeCapabilityStep(
            steps.length + 1,
            'blueprint_component',
            taskSpec.target.asset_path,
            'component_tree',
            [{
              op: 'set_component_properties',
              component_name: addOp.component_name,
              settings,
            }],
          ),
          depends_on: [addStep.step_id],
        } as TaskPlanStep);
      }
      return;
    }

    if (kind === 'configure_component') {
      const settings = propertySettingsArray(change['properties'], `${path}.properties`, true, 'component');
      steps.push(makeCompositeCapabilityStep(
        steps.length + 1,
        'blueprint_component',
        taskSpec.target.asset_path,
        'component_tree',
        [{
          op: 'set_component_properties',
          component_name: getRequiredString(change, 'name', `${path}.name`),
          settings,
        }],
      ));
      return;
    }

    if (kind === 'rename_component') {
      steps.push(makeCompositeCapabilityStep(
        steps.length + 1,
        'blueprint_component',
        taskSpec.target.asset_path,
        'component_tree',
        [omitUndefined({
          op: 'rename_component',
          component_name: getRequiredString(change, 'name', `${path}.name`),
          new_component_name: getRequiredString(change, 'new_name', `${path}.new_name`),
        })],
      ));
      return;
    }

    if (kind === 'reparent_component') {
      steps.push(makeCompositeCapabilityStep(
        steps.length + 1,
        'blueprint_component',
        taskSpec.target.asset_path,
        'component_tree',
        [omitUndefined({
          op: 'reparent_component',
          component_name: getRequiredString(change, 'name', `${path}.name`),
          new_parent_component: requiredComponentHierarchyParent(change, `${path}.new_parent`, ['new_parent']),
          socket_name: componentSocket(change),
          attach_rule: componentAttachRule(change),
          transform_policy: optionalComponentPolicyValue(change, 'transform_policy', COMPONENT_TRANSFORM_POLICIES, `${path}.transform_policy`, 'unsupported_transform_policy'),
        })],
      ));
      return;
    }

    if (kind === 'attach_component') {
      steps.push(makeCompositeCapabilityStep(
        steps.length + 1,
        'blueprint_component',
        taskSpec.target.asset_path,
        'component_tree',
        [omitUndefined({
          op: 'attach_component',
          component_name: getRequiredString(change, 'name', `${path}.name`),
          parent_component: requiredComponentHierarchyParent(change, `${path}.parent`, ['parent']),
          socket_name: componentSocket(change),
          attach_rule: componentAttachRule(change),
          transform_policy: optionalComponentPolicyValue(change, 'transform_policy', COMPONENT_TRANSFORM_POLICIES, `${path}.transform_policy`, 'unsupported_transform_policy'),
        })],
      ));
      return;
    }

    if (kind === 'detach_component') {
      steps.push(makeCompositeCapabilityStep(
        steps.length + 1,
        'blueprint_component',
        taskSpec.target.asset_path,
        'component_tree',
        [omitUndefined({
          op: 'detach_component',
          component_name: getRequiredString(change, 'name', `${path}.name`),
          transform_policy: optionalComponentPolicyValue(change, 'transform_policy', COMPONENT_TRANSFORM_POLICIES, `${path}.transform_policy`, 'unsupported_transform_policy'),
          default_root_policy: optionalComponentPolicyValue(change, 'default_root_policy', COMPONENT_DEFAULT_ROOT_POLICIES, `${path}.default_root_policy`, 'unsupported_default_root_policy'),
        })],
      ));
      return;
    }

    if (kind === 'set_root_component') {
      steps.push(makeCompositeCapabilityStep(
        steps.length + 1,
        'blueprint_component',
        taskSpec.target.asset_path,
        'component_tree',
        [omitUndefined({
          op: 'set_root_component',
          component_name: getRequiredString(change, 'name', `${path}.name`),
          old_root_policy: optionalComponentPolicyValue(change, 'old_root_policy', COMPONENT_OLD_ROOT_POLICIES, `${path}.old_root_policy`, 'unsupported_old_root_policy'),
          default_root_policy: optionalComponentPolicyValue(change, 'default_root_policy', COMPONENT_DEFAULT_ROOT_POLICIES, `${path}.default_root_policy`, 'unsupported_default_root_policy'),
        })],
      ));
      return;
    }

    if (kind === 'remove_component') {
      steps.push(makeCompositeCapabilityStep(
        steps.length + 1,
        'blueprint_component',
        taskSpec.target.asset_path,
        'component_tree',
        [omitUndefined({
          op: 'remove_component',
          component_name: getRequiredString(change, 'name', `${path}.name`),
          delete_policy: optionalComponentPolicyValue(change, 'delete_policy', COMPONENT_DELETE_POLICIES, `${path}.delete_policy`, 'unsupported_delete_policy'),
        })],
      ));
      return;
    }

    throw new TaskSpecCompileError('taskspec_semantic_invalid', `Unsupported component change kind: ${kind}`, [
      {
        code: 'unsupported_component_change_kind',
        path: `${path}.kind`,
        message: 'Use ensure_component_present, configure_component, rename_component, reparent_component, attach_component, detach_component, set_root_component, or remove_component.',
      },
    ]);
  });

  return makeTaskPlanWithSteps(taskSpec, steps);
}

function compileUMGWidgetTaskSpecToTaskPlan(
  taskSpec: Extract<TaskSpec, { task_type: 'edit_umg_widget' }>,
): TaskPlan {
  const behavior = taskSpec.behavior as Record<string, unknown>;
  assertExactString(
    behavior,
    'widget_strategy',
    'widget_blueprint_edit',
    'behavior.widget_strategy',
    'Use widget_strategy="widget_blueprint_edit".',
  );

  const changes = requiredArray(behavior, 'changes', 'behavior.changes');
  const steps = changes.map((rawChange, index) => {
    const path = `behavior.changes[${index}]`;
    if (!isRecord(rawChange)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object.`, [
        { code: 'invalid_umg_widget_change', path, message: 'Provide a UMG widget change object.' },
      ]);
    }

    const change = rawChange as Record<string, unknown>;
    const kind = getRequiredString(change, 'kind', `${path}.kind`);
    if (kind === 'create_widget') {
      return makeCompositeCapabilityStep(
        index + 1,
        'umg_widget',
        taskSpec.target.asset_path,
        'widget_tree_edit',
        [omitUndefined({
          op: 'add_widget',
          widget_name: getRequiredString(change, 'widget_name', `${path}.widget_name`),
          widget_class: getRequiredString(change, 'widget_class', `${path}.widget_class`),
          parent_widget_name: optionalString(change, 'parent_widget_name'),
          parent_name: optionalString(change, 'parent_name'),
        })],
      );
    }

    if (kind === 'update_widget_property') {
      if (!Object.hasOwn(change, 'value')) {
        throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}.value is required.`, [
          {
            code: 'missing_umg_widget_property_value',
            path: `${path}.value`,
            message: 'Provide value for update_widget_property.',
          },
        ]);
      }
      const propertyPath = optionalString(change, 'property_path');
      const propertyName = optionalString(change, 'property_name');
      if (!propertyPath && !propertyName) {
        throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}.property_path is required.`, [
          {
            code: 'missing_umg_widget_property_path',
            path: `${path}.property_path`,
            message: 'Provide property_path or property_name.',
          },
        ]);
      }
      return makeCompositeCapabilityStep(
        index + 1,
        'umg_widget',
        taskSpec.target.asset_path,
        'widget_property_edit',
        [omitUndefined({
          op: 'set_widget_property',
          widget_name: getRequiredString(change, 'widget_name', `${path}.widget_name`),
          property_path: propertyPath,
          property_name: propertyName,
          value: literalValue(change['value']),
        })],
      );
    }

    if (kind === 'delete_widget') {
      return makeCompositeCapabilityStep(
        index + 1,
        'umg_widget',
        taskSpec.target.asset_path,
        'widget_tree_edit',
        [{
          op: 'remove_widget',
          widget_name: getRequiredString(change, 'widget_name', `${path}.widget_name`),
        }],
      );
    }

    throw new TaskSpecCompileError('taskspec_semantic_invalid', `Unsupported UMG widget change kind: ${kind}`, [
      {
        code: 'unsupported_umg_widget_change_kind',
        path: `${path}.kind`,
        message: 'Use create_widget, update_widget_property, or delete_widget.',
      },
    ]);
  });

  return makeTaskPlanWithSteps(taskSpec, steps);
}

function compileDataTableTaskSpecToTaskPlan(
  taskSpec: Extract<TaskSpec, { task_type: 'edit_data_table' }>,
): TaskPlan {
  const behavior = taskSpec.behavior as Record<string, unknown>;
  assertExactString(
    behavior,
    'row_strategy',
    'row_edit',
    'behavior.row_strategy',
    'Use row_strategy="row_edit".',
  );

  const rows = requiredArray(behavior, 'rows', 'behavior.rows');
  const steps = rows.map((rawRow, index) => {
    const path = `behavior.rows[${index}]`;
    if (!isRecord(rawRow)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object.`, [
        { code: 'invalid_data_table_row', path, message: 'Provide a DataTable row object.' },
      ]);
    }

    const row = rawRow as Record<string, unknown>;
    const action = getRequiredString(row, 'action', `${path}.action`);
    const rowName = getRequiredString(row, 'row_name', `${path}.row_name`);

    if (action === 'add') {
      return makeCompositeCapabilityStep(
        index + 1,
        'data_table',
        taskSpec.target.asset_path,
        'row_edit',
        [omitUndefined({
          op: 'add_row',
          row_name: rowName,
          fields: optionalFieldsObject(row['fields'], `${path}.fields`, false),
        })],
      );
    }

    if (action === 'update') {
      return makeCompositeCapabilityStep(
        index + 1,
        'data_table',
        taskSpec.target.asset_path,
        'row_edit',
        [{
          op: 'update_row',
          row_name: rowName,
          fields: optionalFieldsObject(row['fields'], `${path}.fields`, true),
        }],
      );
    }

    if (action === 'delete') {
      return makeCompositeCapabilityStep(
        index + 1,
        'data_table',
        taskSpec.target.asset_path,
        'row_edit',
        [{
          op: 'delete_row',
          row_name: rowName,
        }],
      );
    }

    throw new TaskSpecCompileError('taskspec_semantic_invalid', `Unsupported DataTable row action: ${action}`, [
      {
        code: 'unsupported_data_table_row_action',
        path: `${path}.action`,
        message: 'Use add, update, or delete.',
      },
    ]);
  });

  return makeTaskPlanWithSteps(taskSpec, steps);
}

function compileBlueprintClassSettingsTaskSpecToTaskPlan(
  taskSpec: Extract<TaskSpec, { task_type: 'edit_blueprint_class_settings' }>,
): TaskPlan {
  const behavior = taskSpec.behavior as Record<string, unknown>;
  assertExactString(
    behavior,
    'class_settings_strategy',
    'class_settings',
    'behavior.class_settings_strategy',
    'Use class_settings_strategy="class_settings".',
  );

  if (Object.hasOwn(behavior, 'parent_class')) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Use behavior.reparent.new_parent_class for Blueprint reparent operations.', [
      {
        code: 'legacy_parent_class_field',
        path: 'behavior.parent_class',
        message: 'Use behavior.reparent.new_parent_class.',
      },
    ]);
  }

  const ops: Record<string, unknown>[] = [];
  const interfaces = asRecord(behavior['interfaces']);
  const ensurePresent = stringArrayOrEmpty(interfaces?.['ensure_present'], 'behavior.interfaces.ensure_present');
  if (ensurePresent.length > 0) {
    ops.push({
      op: 'add_implemented_interfaces',
      interface_paths: ensurePresent,
    });
  }

  const ensureAbsent = stringArrayOrEmpty(interfaces?.['ensure_absent'], 'behavior.interfaces.ensure_absent');
  if (ensureAbsent.length > 0) {
    ops.push({
      op: 'remove_implemented_interfaces',
      interface_paths: ensureAbsent,
    });
  }

  const classDefaults = classSettingsDefaultArray(behavior['class_defaults'], 'behavior.class_defaults');
  if (classDefaults.length > 0) {
    ops.push({
      op: 'set_class_default_properties',
      settings: classDefaults,
    });
  }

  const reparent = asRecord(behavior['reparent']);
  if (reparent) {
    ops.push({
      op: 'reparent_blueprint',
      new_parent_class: getRequiredString(reparent, 'new_parent_class', 'behavior.reparent.new_parent_class'),
    });
  }

  if (ops.length === 0) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'edit_blueprint_class_settings requires at least one class settings change.', [
      {
        code: 'missing_class_settings_change',
        path: 'behavior',
        message: 'Provide interfaces, class_defaults, or reparent.new_parent_class.',
      },
    ]);
  }

  return makeTaskPlanWithSteps(
    taskSpec,
    ops.map((op, index) => makeCompositeCapabilityStep(
      index + 1,
      'blueprint_class_settings',
      taskSpec.target.asset_path,
      'class_settings',
      [op],
    )),
  );
}

function compileBlueprintSignatureTaskSpecToTaskPlan(
  taskSpec: Extract<TaskSpec, { task_type: 'edit_blueprint_signature' }>,
): TaskPlan {
  const behavior = taskSpec.behavior as Record<string, unknown>;
  assertExactString(
    behavior,
    'signature_strategy',
    'signature_edit',
    'behavior.signature_strategy',
    'Use signature_strategy="signature_edit".',
  );

  const changes = requiredArray(behavior, 'changes', 'behavior.changes');
  const steps = changes.map((rawChange, index) => {
    const path = `behavior.changes[${index}]`;
    if (!isRecord(rawChange)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object.`, [
        { code: 'invalid_signature_change', path, message: 'Provide a signature change object.' },
      ]);
    }
    const op = compileBlueprintSignatureOp(rawChange, path);
    return makeCompositeCapabilityStep(
      index + 1,
      'blueprint_signature',
      taskSpec.target.asset_path,
      blueprintSignatureStrategyForOp(op),
      [op],
    );
  });

  return makeTaskPlanWithSteps(taskSpec, steps);
}

function compileBlueprintSignatureOp(change: Record<string, unknown>, path: string): Record<string, unknown> {
  const kind = getRequiredString(change, 'kind', `${path}.kind`);
  if (kind === 'ensure_function' || kind === 'ensure_interface_function') {
    const inputs = optionalSignaturePinSpecs(change['inputs'], `${path}.inputs`);
    const outputs = optionalSignaturePinSpecs(change['outputs'], `${path}.outputs`);
    const op = omitUndefined({
      op: 'ensure_function',
      function_name: getRequiredString(change, 'function_name', `${path}.function_name`),
      interface_path: kind === 'ensure_interface_function'
        ? getRequiredString(change, 'interface_path', `${path}.interface_path`)
        : optionalString(change, 'interface_path'),
      interface_entry_kind: kind === 'ensure_interface_function' ? 'function' : undefined,
      inputs,
      outputs,
      is_pure: change['is_pure'],
      name_collision_policy: optionalString(change, 'name_collision_policy') ?? 'reuse_if_exists',
    });
    return op;
  }

  if (kind === 'ensure_custom_event' || kind === 'ensure_interface_event') {
    const inputs = optionalSignaturePinSpecs(change['inputs'], `${path}.inputs`);
    return omitUndefined({
      op: 'ensure_custom_event',
      event_name: getRequiredString(change, 'event_name', `${path}.event_name`),
      graph_name: getRequiredString(change, 'graph_name', `${path}.graph_name`),
      interface_path: kind === 'ensure_interface_event'
        ? getRequiredString(change, 'interface_path', `${path}.interface_path`)
        : optionalString(change, 'interface_path'),
      interface_entry_kind: kind === 'ensure_interface_event' ? 'event' : undefined,
      inputs,
      name_collision_policy: optionalString(change, 'name_collision_policy') ?? 'reuse_if_exists',
    });
  }

  if (kind === 'ensure_event_dispatcher') {
    const inputs = optionalSignaturePinSpecs(change['inputs'], `${path}.inputs`);
    return omitUndefined({
      op: 'ensure_event_dispatcher',
      dispatcher_name: getRequiredString(change, 'dispatcher_name', `${path}.dispatcher_name`),
      inputs,
      name_collision_policy: optionalString(change, 'name_collision_policy') ?? 'reuse_if_exists',
      signature_mismatch_policy: optionalString(change, 'signature_mismatch_policy') ?? 'block',
    });
  }

  if (kind === 'ensure_override_event') {
    const inputs = optionalSignaturePinSpecs(change['inputs'], `${path}.inputs`);
    return omitUndefined({
      op: 'ensure_override_event',
      event_name: getRequiredString(change, 'event_name', `${path}.event_name`),
      event_kind: optionalString(change, 'event_kind') ?? 'native_event',
      graph_name: optionalString(change, 'graph_name'),
      inputs,
      execute_policy: optionalString(change, 'execute_policy') ?? 'blocked_preflight',
    });
  }

  if (kind === 'remove_signature') {
    const signatureKind = optionalString(change, 'signature_kind') ?? inferRemoveSignatureKind(change);
    if (change['require_reference_context'] === false) {
      throw new TaskSpecCompileError('invalid_signature_remove_policy', `${path}.require_reference_context must be true.`, [
        {
          code: 'invalid_signature_remove_policy',
          path: `${path}.require_reference_context`,
          message: 'Signature removal requires reference context in this slice.',
        },
      ]);
    }
    return omitUndefined({
      op: 'remove_signature',
      signature_kind: signatureKind,
      signature_name: removeSignatureName(change, signatureKind, path),
      graph_name: change['graph_name'],
      execute_policy: optionalString(change, 'execute_policy') ?? 'blocked_preflight',
      require_reference_context: typeof change['require_reference_context'] === 'boolean'
        ? change['require_reference_context']
        : true,
    });
  }

  throw new TaskSpecCompileError('unsupported_signature_change', `Unsupported signature change kind: ${kind}.`, [
    {
      code: 'unsupported_signature_change',
      path: `${path}.kind`,
      message: 'Use ensure_function, ensure_interface_function, ensure_custom_event, ensure_interface_event, ensure_event_dispatcher, ensure_override_event, or remove_signature.',
    },
  ]);
}

function optionalSignaturePinSpecs(value: unknown, path: string): Array<Record<string, unknown>> | undefined {
  if (value === undefined) {
    return undefined;
  }
  if (!Array.isArray(value)) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an array.`, [
      {
        code: 'invalid_signature_pins',
        path,
        message: 'Provide signature pins as an array of objects.',
      },
    ]);
  }

  return value.map((rawPin, index) => {
    const pinPath = `${path}[${index}]`;
    if (!isRecord(rawPin)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${pinPath} must be an object.`, [
        {
          code: 'invalid_signature_pin',
          path: pinPath,
          message: 'Provide a signature pin object.',
        },
      ]);
    }

    return {
      ...rawPin,
      name: getRequiredString(rawPin, 'name', `${pinPath}.name`),
      pin_type: requireStructuredPinType(rawPin['pin_type'], `${pinPath}.pin_type`),
    };
  });
}

function blueprintSignatureStrategyForOp(op: Record<string, unknown>): string {
  if (op['op'] === 'ensure_event_dispatcher') return 'event_dispatcher_signature';
  if (op['op'] === 'ensure_override_event') return 'override_event_signature';
  if (op['op'] === 'ensure_custom_event') return 'custom_event_signature';
  if (op['op'] === 'remove_signature') {
    const kind = String(op['signature_kind'] ?? 'function');
    if (kind === 'event_dispatcher') return 'event_dispatcher_signature';
    if (kind === 'override_event' || kind === 'native_event') return 'override_event_signature';
    if (kind === 'custom_event' || kind === 'interface_event') return 'custom_event_signature';
  }
  return 'function_signature';
}

function inferRemoveSignatureKind(change: Record<string, unknown>): string {
  if (typeof change['dispatcher_name'] === 'string') return 'event_dispatcher';
  if (typeof change['event_name'] === 'string') return 'custom_event';
  return 'function';
}

function removeSignatureName(change: Record<string, unknown>, signatureKind: string, path: string): string {
  if (typeof change['signature_name'] === 'string' && change['signature_name'].length > 0) {
    return change['signature_name'];
  }
  if ((signatureKind === 'function' || signatureKind === 'interface_function') && typeof change['function_name'] === 'string') {
    return change['function_name'];
  }
  if ((signatureKind === 'custom_event' || signatureKind === 'interface_event' || signatureKind === 'override_event' || signatureKind === 'native_event') && typeof change['event_name'] === 'string') {
    return change['event_name'];
  }
  if (signatureKind === 'event_dispatcher' && typeof change['dispatcher_name'] === 'string') {
    return change['dispatcher_name'];
  }
  return getRequiredString(change, 'signature_name', `${path}.signature_name`);
}

function makeSingleCapabilityTaskPlan(
  taskSpec: TaskSpec,
  capability: string,
  strategy: string,
  ops: Record<string, unknown>[],
  constraints?: Record<string, unknown>,
): TaskPlan {
  return makeTaskPlanWithSteps(taskSpec, [
    {
      ...makeCompositeCapabilityStep(1, capability, taskSpec.target.asset_path, strategy, ops),
      ...(constraints ? { constraints } : {}),
    } as TaskPlanStep,
  ]);
}

function makeTaskPlanWithSteps(taskSpec: TaskSpec, steps: TaskPlanStep[]): TaskPlan {
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
    steps: renumberSteps(steps),
  };
}

export function taskPlanToAppendBridgePayload(taskPlan: TaskPlan, dryRun: boolean): AppendBridgePayload {
  const step = taskPlan.steps.find((candidate) => (
    ('capability' in candidate && candidate.capability === 'graph_write') ||
    ('operation' in candidate && candidate.operation === 'append_blueprint_graph')
  ));
  if (!step) {
    throw new TaskSpecCompileError('unsupported_taskplan_operation', 'TaskPlan does not contain an append_blueprint_graph step.', [
      {
        code: 'unsupported_taskplan_operation',
        path: 'steps',
        message: 'TaskPlan requires a GraphWrite append step.',
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
  const appendArgs = appendStep.args as Record<string, unknown>;
  const logicSpec = appendArgs['logic_spec'];
  if (!isRecord(logicSpec)) {
    throw new TaskSpecCompileError('logic_spec_required', 'append_blueprint_graph requires args.logic_spec/SemanticIR.', [
      {
        code: 'logic_spec_required',
        path: 'steps[0].args.logic_spec',
        message: 'Legacy nodes/links append payloads are disabled. Provide BlueprintLogicSpec.v2 logic_spec.',
      },
    ]);
  }

  return {
    target: {
      asset_path: appendStep.target.asset_path,
      graph: appendStep.target.graph,
    },
    ...(appendStep.args.feature_name ? { feature_name: appendStep.args.feature_name } : {}),
    logic_spec: logicSpec as AppendBridgePayload['logic_spec'],
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
  const logicStatements: BlueprintLogicStatement[] = [];
  let logicEntry: Record<string, unknown> | undefined;
  const cloneOptions: LogicCloneOptions = {
    defaultFieldOwnerClass: defaultFieldOwnerClassForBlueprintAsset(step.target.asset_path),
  };
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

    logicStatements.push(...compileEnsureEntryOpIntoAppendPayload(nodes, links, rawOp as Record<string, unknown>, `steps[0].write.ops[${opIndex}]`, cloneOptions));
    if (!logicEntry && isRecord(rawOp) && rawOp.entry_type === 'custom_event' && typeof rawOp.name === 'string') {
      const eventKind = graphWriteEnsureEntryEventKind(rawOp);
      const catalogEvidence = graphWriteCatalogEvidence(rawOp['catalog_evidence']);
      logicEntry = {
        kind: eventKind,
        name: rawOp.name,
        id: `${toIdSegment(rawOp.name)}_entry`,
        ...(catalogEvidence ? { catalog_evidence: catalogEvidence } : {}),
        ...(typeof rawOp.signature_evidence_id === 'string' && rawOp.signature_evidence_id.trim().length > 0
          ? {
              source_cluster: 'blueprint_signature',
              signature_evidence_id: rawOp.signature_evidence_id.trim(),
            }
          : {}),
      };
    }
  });

  return {
    target: {
      asset_path: step.target.asset_path,
      graph: step.target.graph,
    },
    ...(taskPlan.task_name ? { feature_name: taskPlan.task_name } : {}),
    logic_spec: {
      schema: 'BlueprintLogicSpec.v2',
      ...(logicEntry ? { entry: logicEntry } : {}),
      statements: logicStatements,
    },
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
      ...('depends_on' in step && Array.isArray(step.depends_on) ? { depends_on: step.depends_on } : {}),
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

  const behavior = taskSpec.behavior as Record<string, unknown>;
  const strategy = getRequiredString(behavior, 'graph_strategy', 'behavior.graph_strategy');
  if (!['append_new_owned_graph', 'replace_owned_graph', 'patch_owned_graph', 'merge_owned_graph', 'merge_external_flow', 'patch_external_graph', 'replace_external_body'].includes(strategy)) {
    throw new TaskSpecCompileError('unsupported_graph_strategy', 'Unsupported GraphWrite graph_strategy.', [
      {
        code: 'unsupported_graph_strategy',
        path: 'behavior.graph_strategy',
        message: 'Use append_new_owned_graph, replace_owned_graph, patch_owned_graph, merge_owned_graph, merge_external_flow, patch_external_graph, or replace_external_body.',
        suggested_patch: { op: 'replace', path: '/behavior/graph_strategy', value: 'append_new_owned_graph' },
      },
    ]);
  }
  const requiredFieldByStrategy: Record<string, string> = {
    append_new_owned_graph: 'entries',
    replace_owned_graph: 'replace',
    patch_owned_graph: 'patches',
    merge_owned_graph: 'merges',
    merge_external_flow: 'external_merges',
    patch_external_graph: 'external_patches',
    replace_external_body: 'external_replace',
  };
  const requiredField = requiredFieldByStrategy[strategy];
  for (const field of ['entries', 'replace', 'patches', 'merges', 'external_merges', 'external_patches', 'external_replace']) {
    if (field !== requiredField && behavior[field] !== undefined) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${field} does not belong to graph_strategy ${strategy}.`, [
        {
          code: 'graph_write_strategy_field_mismatch',
          path: `behavior.${field}`,
          message: `Use behavior.${requiredField} for ${strategy}.`,
        },
      ]);
    }
  }

  if (taskSpec.scope_policy.allow_modify_user_nodes) {
    throw new TaskSpecCompileError('unsupported_scope_policy', 'unsupported_scope_policy: Modifying user nodes is not supported for GraphWrite owned strategies.', [
      {
        code: 'unsupported_scope_policy',
        path: 'scope_policy.allow_modify_user_nodes',
        message: 'Set allow_modify_user_nodes=false and target BlueprintHelper-owned graph logic.',
        suggested_patch: { op: 'replace', path: '/scope_policy/allow_modify_user_nodes', value: false },
      },
    ]);
  }
  if (strategy === 'merge_external_flow') {
    validateExternalGraphWriteScopePolicy(taskSpec, strategy, ['exec_boundary_link']);
  }
  if (strategy === 'patch_external_graph') {
    validateExternalGraphWriteScopePolicy(taskSpec, strategy, ['pin_default', 'node_comment']);
  }
  if (strategy === 'replace_external_body') {
    if (taskSpec.execution_policy.dry_run_mode !== 'full') {
      throw new TaskSpecCompileError('unsupported_execution_policy', 'replace_external_body requires execution_policy.dry_run_mode="full".', [
        {
          code: 'replace_external_body_requires_full_dry_run',
          path: 'execution_policy.dry_run_mode',
          message: 'Set dry_run_mode="full" for replace_external_body.',
          suggested_patch: { op: 'replace', path: '/execution_policy/dry_run_mode', value: 'full' },
        },
      ]);
    }
    validateExternalGraphWriteScopePolicy(taskSpec, strategy, ['body_replace']);
  }

  compileGraphWriteOps(behavior);
}

function validateExternalGraphWriteScopePolicy(
  taskSpec: Extract<TaskSpec, { task_type: 'edit_blueprint_graph' }>,
  strategy: string,
  requiredMutations: string[],
): void {
  const policy = asRecord(taskSpec.scope_policy['external_mutation_policy']);
  if (!policy) {
    throw new TaskSpecCompileError('unsupported_scope_policy', 'unsupported_scope_policy: External graph writes require scope_policy.external_mutation_policy.', [
      {
        code: 'missing_external_mutation_policy',
        path: 'scope_policy.external_mutation_policy',
        message: `Set external_mutation_policy.strategy="${strategy}" with the required allowed_mutations.`,
      },
    ]);
  }

  const policyStrategy = typeof policy['strategy'] === 'string' ? policy['strategy'] : '';
  if (policyStrategy !== strategy) {
    throw new TaskSpecCompileError('unsupported_scope_policy', 'unsupported_scope_policy: External graph mutation policy strategy does not match graph_strategy.', [
      {
        code: 'external_mutation_policy_strategy_mismatch',
        path: 'scope_policy.external_mutation_policy.strategy',
        message: `Use strategy="${strategy}".`,
        suggested_patch: { op: 'replace', path: '/scope_policy/external_mutation_policy/strategy', value: strategy },
      },
    ]);
  }

  const allowedMutations = Array.isArray(policy['allowed_mutations'])
    ? policy['allowed_mutations'].filter((value): value is string => typeof value === 'string')
    : [];
  const exactMatch = allowedMutations.length === requiredMutations.length
    && requiredMutations.every((mutation) => allowedMutations.includes(mutation))
    && allowedMutations.every((mutation) => requiredMutations.includes(mutation));
  if (!exactMatch) {
    throw new TaskSpecCompileError(
      'unsupported_scope_policy',
      `unsupported_scope_policy: External graph mutation policy allowlist must be exactly: ${requiredMutations.join(', ')}.`,
      [
      {
        code: 'external_mutation_policy_exact_allowlist_required',
        path: 'scope_policy.external_mutation_policy.allowed_mutations',
        message: `Use exactly: ${requiredMutations.join(', ')}.`,
      },
      ],
    );
  }
}

type GraphWriteCompiledOp = Record<string, unknown> & { op: string };
type GraphWriteAppendEventKind =
  | 'custom_event'
  | 'override_event'
  | 'component_bound_event'
  | 'input_action_event'
  | 'dispatcher_event';
type GraphWriteCatalogEvidence = {
  source: 'signature' | 'graph_action_catalog';
  signature_evidence_id?: string;
  action_stable_id?: string;
  context_fingerprint?: string;
};
const OWNED_GRAPH_PATCH_KINDS = [
  'set_pin_default',
  'set_node_comment',
  'connect_pins',
  'disconnect_link',
  'replace_link',
  'delete_owned_node',
] as const;
type GraphWriteSignatureSplit = {
  op: 'ensure_custom_event';
  event_name: string;
  inputs?: unknown;
  name_collision_policy: string;
};

interface GraphWriteCompileOptions {
  defaultFieldOwnerClass?: string;
}

interface LogicCloneOptions {
  defaultFieldOwnerClass?: string;
  graphLocalSymbols?: Set<string>;
}

function makeCustomEventSignatureEvidenceId(eventName: string): string {
  return `signature:custom_event:${eventName}`;
}

function compileGraphWriteOps(
  behavior: Record<string, unknown>,
  options: GraphWriteCompileOptions = {},
): GraphWriteCompiledOp[] {
  const strategy = getRequiredString(behavior, 'graph_strategy', 'behavior.graph_strategy');
  if (strategy === 'append_new_owned_graph') {
    return compileAppendGraphWriteOps(behavior, options);
  }
  if (strategy === 'replace_owned_graph') {
    return [compileReplaceGraphWriteOp(behavior, options)];
  }
  if (strategy === 'patch_owned_graph') {
    return compilePatchGraphWriteOps(behavior);
  }
  if (strategy === 'merge_owned_graph') {
    return compileMergeGraphWriteOps(behavior);
  }
  if (strategy === 'merge_external_flow') {
    return compileExternalMergeGraphWriteOps(behavior, options);
  }
  if (strategy === 'patch_external_graph') {
    return compileExternalPatchGraphWriteOps(behavior);
  }
  if (strategy === 'replace_external_body') {
    return [compileExternalReplaceBodyGraphWriteOp(behavior, options)];
  }

  throw new TaskSpecCompileError('unsupported_graph_strategy', 'Unsupported GraphWrite graph_strategy.', [
    {
      code: 'unsupported_graph_strategy',
      path: 'behavior.graph_strategy',
      message: 'Use append_new_owned_graph, replace_owned_graph, patch_owned_graph, merge_owned_graph, merge_external_flow, patch_external_graph, or replace_external_body.',
      suggested_patch: { op: 'replace', path: '/behavior/graph_strategy', value: 'append_new_owned_graph' },
    },
  ]);
}

function assertGraphWriteConnectivityPreflight(
  statements: BlueprintLogicStatement[],
  basePath: string,
): void {
  const issues = collectGraphWriteConnectivityPreflightIssues(
    statements as unknown as Record<string, unknown>[],
    basePath,
  );
  if (issues.length === 0) {
    return;
  }

  const taskIssues: TaskIssue[] = issues.map((issue) => ({
    code: issue.code,
    path: issue.path,
    message: issue.message,
  }));
  throw new TaskSpecCompileError(
    'taskspec_semantic_invalid',
    `GraphWrite connectivity static preflight failed: ${taskIssues[0]?.code ?? 'unknown_issue'}.`,
    taskIssues,
  );
}

function compileAppendGraphWriteOps(
  behavior: Record<string, unknown>,
  options: GraphWriteCompileOptions,
): GraphWriteCompiledOp[] {
  const entries = requiredNonEmptyArray(behavior, 'entries', 'behavior.entries');
  return entries.map((rawEntry, entryIndex) => {
    if (!isRecord(rawEntry)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'GraphWrite entry must be an object.', [
        {
          code: 'invalid_graph_write_entry',
          path: `behavior.entries[${entryIndex}]`,
          message: 'GraphWrite entry must be an object.',
        },
      ]);
    }
    const entry = rawEntry as Record<string, unknown>;
    const entryType = getRequiredString(entry, 'entry_type', `behavior.entries[${entryIndex}].entry_type`);
    if (entryType !== 'custom_event') {
      throw new TaskSpecCompileError('unsupported_entry_type', 'Only custom_event entries are supported in this GraphWrite slice.', [
        {
          code: 'unsupported_entry_type',
          path: `behavior.entries[${entryIndex}].entry_type`,
          message: 'Use entry_type="custom_event". Function/Event signature management is a later capability cluster.',
          suggested_patch: { op: 'replace', path: `/behavior/entries/${entryIndex}/entry_type`, value: 'custom_event' },
        },
      ]);
    }

    const body = getRequiredLogicBody(entry, 'body', `behavior.entries[${entryIndex}].body`);
    validateSupportedStatements(body.statements, `behavior.entries[${entryIndex}].body.statements`);
    assertGraphWriteConnectivityPreflight(
      body.statements,
      `behavior.entries[${entryIndex}].body.statements`,
    );
    const entryName = getRequiredString(entry, 'name', `behavior.entries[${entryIndex}].name`);
    const rawEventKind = optionalString(entry, 'event_kind');
    const eventKind = graphWriteAppendEventKind(entry);
    const entryInputs = Array.isArray(entry['inputs']) ? entry['inputs'] : undefined;
    const catalogEvidence = graphWriteCatalogEvidence(entry['catalog_evidence']);
    const signatureEvidenceId = catalogEvidence?.signature_evidence_id;
    return {
      op: 'ensure_entry',
      entry_type: entryType,
      name: entryName,
      ...(rawEventKind ? { event_kind: eventKind } : {}),
      ...(catalogEvidence ? { catalog_evidence: catalogEvidence } : {}),
      ...(signatureEvidenceId
        ? { signature_evidence_id: signatureEvidenceId }
        : eventKind === 'custom_event'
          ? { signature_evidence_id: makeCustomEventSignatureEvidenceId(entryName) }
          : {}),
      body: compileLogicBodyToSemanticLogicSpec(body, entryName, options),
      ...(entryInputs && eventKind === 'custom_event'
        ? {
            __signature_split: {
              op: 'ensure_custom_event',
              event_name: entryName,
              inputs: entryInputs,
              name_collision_policy: 'reuse_if_exists',
            } satisfies GraphWriteSignatureSplit,
          }
        : {}),
    };
  });
}

function compileReplaceGraphWriteOp(
  behavior: Record<string, unknown>,
  options: GraphWriteCompileOptions,
): GraphWriteCompiledOp {
  const replace = requiredRecord(behavior, 'replace', 'behavior.replace');
  const replaceScope = getRequiredString(replace, 'scope', 'behavior.replace.scope');
  assertAllowedString(
    replaceScope,
    'behavior.replace.scope',
    ['graph', 'custom_event_definition', 'custom_event_body', 'function_body', 'event_body', 'block_implementation'],
    'Use graph, custom_event_definition, custom_event_body, function_body, event_body, or block_implementation.',
  );
  const graphWriteReplaceScope = replaceScope === 'custom_event_definition'
    ? 'custom_event_body'
    : replaceScope;
  const selector = normalizeReplaceSelector(
    graphWriteReplaceScope,
    requiredRecord(replace, 'selector', 'behavior.replace.selector'),
  );
  const body = getRequiredLogicBody(replace, 'body', 'behavior.replace.body');
  validateSupportedStatements(body.statements, 'behavior.replace.body.statements');
  assertGraphWriteConnectivityPreflight(body.statements, 'behavior.replace.body.statements');
  if (body.statements.length === 0) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'replace_owned_graph requires at least one replacement statement.', [
      {
        code: 'empty_replacement',
        path: 'behavior.replace.body.statements',
        message: 'Provide at least one replacement statement.',
      },
    ]);
  }

  return omitUndefined({
    op: 'replace_body',
    replace_scope: graphWriteReplaceScope,
    selector,
    logic_spec: compileLogicBodyToSemanticLogicSpec(body, 'replace', options),
    options: isRecord(replace['options']) ? replace['options'] : undefined,
    __signature_split: replaceScope === 'custom_event_definition'
      ? {
          op: 'ensure_custom_event',
          event_name: String(selector['entry_name']),
          inputs: replace['inputs'],
          name_collision_policy: optionalString(replace, 'name_collision_policy') ?? 'reuse_if_exists',
        }
      : undefined,
  }) as GraphWriteCompiledOp;
}

function compilePatchGraphWriteOps(behavior: Record<string, unknown>): GraphWriteCompiledOp[] {
  const patches = requiredNonEmptyArray(behavior, 'patches', 'behavior.patches');
  return patches.map((rawPatch, index) => {
    const path = `behavior.patches[${index}]`;
    if (!isRecord(rawPatch)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'GraphWrite patch must be an object.', [
        {
          code: 'invalid_graph_write_patch',
          path,
          message: 'GraphWrite patch must be an object.',
        },
      ]);
    }
    const patch = rawPatch as Record<string, unknown>;
    const kind = getRequiredString(patch, 'kind', `${path}.kind`);
    if (!isOwnedGraphPatchKind(kind)) {
      throw new TaskSpecCompileError('unsupported_graph_write_patch', `Unsupported GraphWrite patch kind: ${kind}`, [
        {
          code: 'unsupported_graph_write_patch',
          path: `${path}.kind`,
          message: `Use ${OWNED_GRAPH_PATCH_KINDS.join(', ')}.`,
        },
      ]);
    }
    const patchScope = typeof patch['scope'] === 'string' && patch['scope'].length > 0
      ? patch['scope']
      : defaultPatchScope(kind);
    const expectedScope = defaultPatchScope(kind);
    if (patchScope !== expectedScope) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `GraphWrite patch scope must match ${kind}.`, [
        {
          code: 'patch_scope_mismatch',
          path: `${path}.scope`,
          message: `${kind} uses scope ${expectedScope}. Omit scope or set it to ${expectedScope}.`,
        },
      ]);
    }

    const patchedRef = normalizePatchTargetRef(kind, requiredRecord(patch, 'target_ref', `${path}.target_ref`), `${path}.target_ref`);
    const targetBlockId = getRequiredString(patchedRef, 'block_id', `${path}.target_ref.block_id`);
    rejectRedundantOwnedPatchExpectedOldState(kind, patch, path);

    return omitUndefined({
      op: kind,
      patch_scope: patchScope,
      patched_ref: patchedRef,
      patch: compilePatchPayload(kind, patch, path, targetBlockId),
      expected_old_state: isRecord(patch['expected_old_state'])
        ? normalizeExpectedOldState(patch['expected_old_state'])
        : undefined,
    }) as GraphWriteCompiledOp;
  });
}

function compileMergeGraphWriteOps(behavior: Record<string, unknown>): GraphWriteCompiledOp[] {
  const merges = requiredNonEmptyArray(behavior, 'merges', 'behavior.merges');
  return merges.map((rawMerge, index) => {
    const path = `behavior.merges[${index}]`;
    if (!isRecord(rawMerge)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'GraphWrite merge must be an object.', [
        {
          code: 'invalid_graph_write_merge',
          path,
          message: 'GraphWrite merge must be an object.',
        },
      ]);
    }
    const merge = rawMerge as Record<string, unknown>;
    const kind = getRequiredString(merge, 'kind', `${path}.kind`);
    if (kind !== 'insert_flow') {
      throw new TaskSpecCompileError('unsupported_graph_write_merge', `Unsupported GraphWrite merge kind: ${kind}`, [
        {
          code: 'unsupported_graph_write_merge',
          path: `${path}.kind`,
          message: 'Use insert_flow.',
        },
      ]);
    }
    const mergeScope = getRequiredString(merge, 'scope', `${path}.scope`);
    assertAllowedString(
      mergeScope,
      `${path}.scope`,
      ['owned_block_call', 'custom_event_call', 'function_call'],
      'Use owned_block_call, custom_event_call, or function_call.',
    );
    const insertStrategy = getRequiredString(merge, 'insert_strategy', `${path}.insert_strategy`);
    assertAllowedString(
      insertStrategy,
      `${path}.insert_strategy`,
      ['append_after', 'insert_between', 'branch_fork'],
      'Use append_after, insert_between, or branch_fork.',
    );
    const sequenceOrder = normalizeMergeSequenceOrder(merge, insertStrategy, `${path}.sequence_order`);

    return omitUndefined({
      op: 'insert_flow',
      merge_scope: mergeScope,
      insert_strategy: insertStrategy,
      anchor: normalizeMergeAnchor(requiredRecord(merge, 'anchor', `${path}.anchor`), `${path}.anchor`),
      inserted: normalizeMergeInserted(mergeScope, requiredRecord(merge, 'inserted', `${path}.inserted`), `${path}.inserted`),
      sequence_order: sequenceOrder,
    }) as GraphWriteCompiledOp;
  });
}

function compileExternalMergeGraphWriteOps(
  behavior: Record<string, unknown>,
  options: GraphWriteCompileOptions,
): GraphWriteCompiledOp[] {
  const merges = requiredNonEmptyArray(behavior, 'external_merges', 'behavior.external_merges');
  return merges.map((rawMerge, index) => {
    const path = `behavior.external_merges[${index}]`;
    if (!isRecord(rawMerge)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'External GraphWrite merge must be an object.', [
        {
          code: 'invalid_external_graph_write_merge',
          path,
          message: 'External GraphWrite merge must be an object.',
        },
      ]);
    }

    const merge = rawMerge as Record<string, unknown>;
    const kind = getRequiredString(merge, 'kind', `${path}.kind`);
    if (kind !== 'insert_external_flow') {
      throw new TaskSpecCompileError('unsupported_external_graph_write_merge', `Unsupported external GraphWrite merge kind: ${kind}`, [
        {
          code: 'unsupported_external_graph_write_merge',
          path: `${path}.kind`,
          message: 'Use insert_external_flow.',
        },
      ]);
    }

    const insertStrategy = getRequiredString(merge, 'insert_strategy', `${path}.insert_strategy`);
    assertAllowedString(
      insertStrategy,
      `${path}.insert_strategy`,
      ['append_after', 'insert_between', 'branch_fork'],
      'Use append_after, insert_between, or branch_fork.',
    );
    const inserted = requiredRecord(merge, 'inserted', `${path}.inserted`);
    const body = getRequiredLogicBody(inserted, 'body', `${path}.inserted.body`);
    validateSupportedStatements(body.statements, `${path}.inserted.body.statements`);
    assertGraphWriteConnectivityPreflight(body.statements, `${path}.inserted.body.statements`);

    return omitUndefined({
      op: 'insert_external_flow',
      insert_strategy: insertStrategy,
      anchor: normalizeExternalExecBoundaryAnchor(requiredRecord(merge, 'anchor', `${path}.anchor`), `${path}.anchor`),
      inserted: {
        body: compileLogicBodyToSemanticLogicSpec(body, `external_merge_${index}`, options),
      },
      sequence_order: normalizeMergeSequenceOrder(merge, insertStrategy, `${path}.sequence_order`),
    }) as GraphWriteCompiledOp;
  });
}

function compileExternalPatchGraphWriteOps(behavior: Record<string, unknown>): GraphWriteCompiledOp[] {
  const patches = requiredNonEmptyArray(behavior, 'external_patches', 'behavior.external_patches');
  return patches.map((rawPatch, index) => {
    const path = `behavior.external_patches[${index}]`;
    if (!isRecord(rawPatch)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'External GraphWrite patch must be an object.', [
        {
          code: 'invalid_external_graph_write_patch',
          path,
          message: 'External GraphWrite patch must be an object.',
        },
      ]);
    }

    const patch = rawPatch as Record<string, unknown>;
    const kind = getRequiredString(patch, 'kind', `${path}.kind`);
    assertAllowedString(
      kind,
      `${path}.kind`,
      ['set_external_pin_default', 'set_external_node_comment'],
      'Use set_external_pin_default or set_external_node_comment.',
    );
    if (!Object.hasOwn(patch, 'value')) {
      throwMissingPatchValue(path, `${kind} requires value.`);
    }
    const expectedOldState = requiredRecord(patch, 'expected_old_state', `${path}.expected_old_state`);

    return {
      op: kind,
      anchor: normalizeExternalNodeAnchor(requiredRecord(patch, 'anchor', `${path}.anchor`), `${path}.anchor`, kind),
      value: patchValueToString(literalValue(patch['value'])),
      expected_old_state: normalizeExpectedOldState(expectedOldState),
    } as GraphWriteCompiledOp;
  });
}

function compileExternalReplaceBodyGraphWriteOp(
  behavior: Record<string, unknown>,
  options: GraphWriteCompileOptions,
): GraphWriteCompiledOp {
  const replace = requiredRecord(behavior, 'external_replace', 'behavior.external_replace');
  const scope = getRequiredString(replace, 'scope', 'behavior.external_replace.scope');
  assertAllowedString(
    scope,
    'behavior.external_replace.scope',
    ['custom_event_body', 'event_body', 'function_body'],
    'Use custom_event_body, event_body, or function_body.',
  );
  if (replace['require_full_dry_run'] !== true) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'replace_external_body requires require_full_dry_run=true.', [
      {
        code: 'replace_external_body_requires_full_dry_run',
        path: 'behavior.external_replace.require_full_dry_run',
        message: 'Set require_full_dry_run=true.',
      },
    ]);
  }

  const body = getRequiredLogicBody(replace, 'body', 'behavior.external_replace.body');
  validateSupportedStatements(body.statements, 'behavior.external_replace.body.statements');
  assertGraphWriteConnectivityPreflight(body.statements, 'behavior.external_replace.body.statements');
  if (body.statements.length === 0) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'replace_external_body requires at least one replacement statement.', [
      {
        code: 'empty_external_body_replacement',
        path: 'behavior.external_replace.body.statements',
        message: 'Provide at least one replacement statement.',
      },
    ]);
  }

  return {
    op: 'replace_external_body',
    replace_scope: scope,
    anchor: normalizeExternalBodyEntryAnchor(
      requiredRecord(replace, 'anchor', 'behavior.external_replace.anchor'),
      'behavior.external_replace.anchor',
    ),
    logic_spec: compileLogicBodyToSemanticLogicSpec(body, 'external_body_replace', options),
    expected_body_fingerprint: getRequiredString(
      replace,
      'expected_body_fingerprint',
      'behavior.external_replace.expected_body_fingerprint',
    ),
    require_full_dry_run: true,
  } as GraphWriteCompiledOp;
}

const PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES = new Map<string, string>([
  ['component_bound_event', 'component_bound_event'],
  ['delegate.bind', 'bind'],
  ['delegate.assign', 'assign'],
  ['delegate.unbind', 'unbind'],
  ['delegate.unbind_all', 'clear'],
  ['delegate.call', 'call'],
]);
const INTERNAL_DELEGATE_STATEMENT_KIND = 'delegate';
const DELEGATE_STATEMENT_OPERATION_KINDS = new Set(['bind', 'assign', 'unbind', 'clear', 'call']);
const CONTAINER_ACTION_KIND = 'container_action';
const GRAPH_CONTAINER_ACTION_FIELDS = [
  'container_kind',
  'container_operation',
  'element_type',
  'key_type',
  'value_type',
  'target',
  'item',
  'items',
  'key',
  'value',
  'index',
  'result_symbol',
  'context_evidence',
] as const;
const SUPPORTED_CONTAINER_KINDS = new Set(Object.keys(CONTAINER_ACTION_OPERATIONS_BY_KIND));
const SUPPORTED_CONTAINER_OPERATIONS = new Map<string, ReadonlySet<string>>(
  Object.entries(CONTAINER_ACTION_OPERATIONS_BY_KIND).map(([containerKind, operations]) => [
    containerKind,
    new Set(operations),
  ]),
);
const FORBIDDEN_AGENT_DELEGATE_INTERNAL_KINDS = new Set([
  'delegate',
  'bind',
  'assign',
  'unbind',
  'unbind_all',
  'delegate_call',
  'delegate_clear',
]);
const SUPPORTED_GRAPH_BODY_STATEMENT_KINDS = new Set([
  'call',
  'field',
  'set',
  'set_property',
  'let',
  'control',
  'create',
  'convert',
  'schedule',
  CONTAINER_ACTION_KIND,
  ...PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES.keys(),
]);
const VALUE_PRODUCING_STATEMENT_KINDS = new Set([
  'call',
  'create',
  'convert',
  'schedule',
  CONTAINER_ACTION_KIND,
]);
const GRAPH_BODY_SINGLETON_CONTROL_KINDS = new Set(['branch', 'sequence', 'return']);
const GRAPH_BODY_SWITCH_CONTROL_KINDS = new Set(['switch_int', 'switch_string', 'switch_name', 'switch_enum']);
const GRAPH_BODY_DYNAMIC_CONTROL_KINDS = new Set(['multi_gate']);
const GRAPH_BODY_MACRO_CONTROL_KINDS = new Set([
  'do_once',
  'do_n',
  'gate',
  'flip_flop',
  'for_loop',
  'for_loop_with_break',
  'foreach_loop',
  'foreach_loop_with_break',
  'while_loop',
]);
const SUPPORTED_GRAPH_BODY_CONTROL_KINDS = new Set([
  ...GRAPH_BODY_SINGLETON_CONTROL_KINDS,
  ...GRAPH_BODY_SWITCH_CONTROL_KINDS,
  ...GRAPH_BODY_DYNAMIC_CONTROL_KINDS,
  ...GRAPH_BODY_MACRO_CONTROL_KINDS,
]);
const SUPPORTED_GRAPH_BODY_EXPRESSION_KINDS = new Set([
  'literal',
  'field',
  'get',
  'get_property',
  'call',
  'op',
  'construct',
  'deconstruct',
  'select',
  'create',
  'convert',
  'schedule',
  CONTAINER_ACTION_KIND,
]);
const GRAPH_CONVERT_SCHEDULE_FIELDS = [
  'function_operation',
  'transform_operation',
  'schedule_operation',
  'target_class_path',
  'graph_latent_allowed',
] as const;
const GENERIC_SCHEDULE_OPERATIONS = new Set(['timer_delegate_node', 'latent_or_async_node']);
const FUNCTION_BACKED_CREATE_OPERATIONS = new Set([
  'async_action',
  'function_backed_create',
  'function_backed_spawn',
  'function_backed_construct',
]);
const GENERIC_CREATE_OPERATIONS = new Set([
  'spawn_actor',
  'create_widget',
  'construct_object',
  'make_array',
  'make_map',
  'make_set',
  'asset_action',
]);
const FIELD_STATEMENT_KIND_MAP = new Map([
  ['set', { operation: 'set', scope: 'variable' }],
  ['set_property', { operation: 'set', scope: 'property_path' }],
]);
const FIELD_EXPRESSION_KIND_MAP = new Map([
  ['get', { operation: 'get', scope: 'variable' }],
  ['get_property', { operation: 'get', scope: 'property_path' }],
]);
const SUPPORTED_FIELD_SCOPES = new Set(['variable', 'property_path', 'component_ref', 'field_access']);
const FIELD_SCOPES_WITH_PROPERTY_PATH = new Set(['property_path', 'field_access']);
const GRAPHWRITE_STRUCTURED_PIN_TYPE_FIELDS = [
  'pin_type',
  'key_pin_type',
  'value_pin_type',
  'result_pin_type',
  'return_pin_type',
] as const;
const GRAPHWRITE_STRUCTURED_PIN_TYPE_EVIDENCE_FIELDS = [
  'pin_type',
  'result_pin_type',
  'return_pin_type',
  'output_pin_type',
  'source_pin_type',
  'target_pin_type',
  'schedule_result_pin_type',
  'schedule_output_pin_type',
  'type_promotion_source_pin_type',
  'type_promotion_target_pin_type',
  'type_promotion_result_pin_type',
  'generic.transform.source_pin_type',
  'generic.transform.target_pin_type',
] as const;
const GRAPHWRITE_RESULT_TYPE_PROOF_EVIDENCE_KEY = 'generic.select.result_type_proof';

function applyFieldTaxonomy(record: Record<string, unknown>, operation: string, scope: string): void {
  record.kind = 'field';
  record.field_operation = operation;
  record.field_scope = scope;
}

function copyConvertScheduleSemanticFields(source: Record<string, unknown>, target: Record<string, unknown>): void {
  GRAPH_CONVERT_SCHEDULE_FIELDS.forEach((field) => {
    if (Object.hasOwn(source, field)) {
      target[field] = source[field];
    }
  });
}

function normalizeSemanticToken(value: unknown): string {
  return typeof value === 'string' ? value.trim().toLowerCase() : '';
}

function normalizeEvidenceValue(value: unknown): string {
  if (Array.isArray(value)) {
    return value.map((entry) => normalizeEvidenceValue(entry)).filter((entry) => entry.length > 0).join(',');
  }
  if (typeof value === 'string') return value.trim();
  if (typeof value === 'number' || typeof value === 'boolean') return String(value);
  return '';
}

function contextEvidenceValue(record: Record<string, unknown>, key: string): string {
  const evidence = record.context_evidence;
  if (!isRecord(evidence) || !Object.hasOwn(evidence, key)) {
    return '';
  }
  return normalizeEvidenceValue(evidence[key]);
}

function firstEvidenceValue(record: Record<string, unknown>, evidenceKey: string, fields: readonly string[]): string {
  const fromEvidence = contextEvidenceValue(record, evidenceKey);
  if (fromEvidence.length > 0) {
    return fromEvidence;
  }
  for (const field of fields) {
    if (Object.hasOwn(record, field)) {
      const value = normalizeEvidenceValue(record[field]);
      if (value.length > 0) {
        return value;
      }
    }
  }
  return '';
}

function isGenericControlKind(controlKind: string): boolean {
  return SUPPORTED_GRAPH_BODY_CONTROL_KINDS.has(controlKind) && !GRAPH_BODY_SINGLETON_CONTROL_KINDS.has(controlKind);
}

function requireGenericControlEvidence(value: string, path: string, evidenceKey: string): string {
  if (value.length > 0) {
    return value;
  }
  throw new TaskSpecCompileError('missing_required_evidence', `Generic control requires ${evidenceKey}.`, [
    {
      code: 'missing_required_evidence',
      path,
      message: `Generic control requires ${evidenceKey}.`,
    },
  ]);
}

function genericControlContextEvidence(
  record: Record<string, unknown>,
  controlKind: string,
  path: string,
): Record<string, unknown> {
  const evidence = isRecord(record.context_evidence) ? { ...record.context_evidence } : {};
  evidence['generic.control.operation'] = controlKind;

  if (GRAPH_BODY_SWITCH_CONTROL_KINDS.has(controlKind)) {
    evidence['generic.control.case_values'] = requireGenericControlEvidence(
      firstEvidenceValue(record, 'generic.control.case_values', ['case_values']),
      `${path}.case_values`,
      'generic.control.case_values',
    );
    const defaultPolicy = firstEvidenceValue(record, 'generic.control.default_policy', ['default_policy']);
    if (defaultPolicy.length > 0) {
      evidence['generic.control.default_policy'] = defaultPolicy;
    }
    if (controlKind === 'switch_enum') {
      evidence['generic.control.enum_path'] = requireGenericControlEvidence(
        firstEvidenceValue(record, 'generic.control.enum_path', ['enum_path']),
        `${path}.enum_path`,
        'generic.control.enum_path',
      );
    }
  } else if (GRAPH_BODY_DYNAMIC_CONTROL_KINDS.has(controlKind)) {
    evidence['generic.control.dynamic_output_count'] = requireGenericControlEvidence(
      firstEvidenceValue(record, 'generic.control.dynamic_output_count', ['dynamic_output_count']),
      `${path}.dynamic_output_count`,
      'generic.control.dynamic_output_count',
    );
  } else if (GRAPH_BODY_MACRO_CONTROL_KINDS.has(controlKind)) {
    evidence['generic.macro.graph_path'] = requireGenericControlEvidence(
      firstEvidenceValue(record, 'generic.macro.graph_path', ['macro_graph_path']),
      `${path}.macro_graph_path`,
      'generic.macro.graph_path',
    );
    evidence['generic.macro.pin_shape_snapshot'] = requireGenericControlEvidence(
      firstEvidenceValue(record, 'generic.macro.pin_shape_snapshot', ['macro_pin_shape_snapshot']),
      `${path}.macro_pin_shape_snapshot`,
      'generic.macro.pin_shape_snapshot',
    );
    const worldContextPolicy = firstEvidenceValue(record, 'generic.macro.world_context_policy', ['macro_world_context_policy']);
    if (worldContextPolicy.length > 0) {
      evidence['generic.macro.world_context_policy'] = worldContextPolicy;
    }
  }

  return evidence;
}

function applyGenericControlSemanticFields(source: Record<string, unknown>, target: Record<string, unknown>, controlKind: string, path: string): void {
  target.kind = 'control';
  target.control = controlKind;
  target.control_operation = controlKind;
  target.context_evidence = genericControlContextEvidence(source, controlKind, path);
  delete target.case_values;
  delete target.enum_path;
  delete target.default_policy;
  delete target.dynamic_output_count;
  delete target.macro_graph_path;
  delete target.macro_pin_shape_snapshot;
  delete target.macro_world_context_policy;
}

function isFunctionBackedCreateOperation(createOperation: string): boolean {
  return FUNCTION_BACKED_CREATE_OPERATIONS.has(createOperation);
}

function validateCreateOwnership(record: Record<string, unknown>, path: string): { createOperation: string; functionOperation: string } {
  const createOperation = getRequiredString(record, 'create_operation', `${path}.create_operation`).trim().toLowerCase();
  const functionOperation = normalizeSemanticToken(record.function_operation);
  if (isFunctionBackedCreateOperation(createOperation)) {
    const callableTarget = optionalString(record, 'target') ?? optionalString(record, 'name');
    if (!callableTarget) {
      throw new TaskSpecCompileError('missing_create_function_target', 'missing_create_function_target: Function-backed create operations require a callable target.', [
        {
          code: 'missing_create_function_target',
          path: `${path}.target`,
          message: 'Provide the callable factory function name in target for function-backed create operations.',
        },
      ]);
    }
    if (functionOperation.length > 0 && functionOperation !== 'create_function') {
      throw new TaskSpecCompileError('unsupported_create_owner_mix', 'unsupported_create_owner_mix: Function-backed create operations require function_operation=create_function.', [
        {
          code: 'unsupported_create_owner_mix',
          path: `${path}.function_operation`,
          message: 'Use function_operation=create_function for function-backed create operations.',
        },
      ]);
    }
    return { createOperation, functionOperation: 'create_function' };
  }
  if (functionOperation.length > 0 || GENERIC_CREATE_OPERATIONS.has(createOperation) && functionOperation.length > 0) {
    throw new TaskSpecCompileError('unsupported_create_owner_mix', 'unsupported_create_owner_mix: Generic create operations must not specify function_operation.', [
      {
        code: 'unsupported_create_owner_mix',
        path: `${path}.function_operation`,
        message: 'Remove function_operation for Generic create operations. Use it only for function-backed create factories.',
      },
    ]);
  }
  return { createOperation, functionOperation: '' };
}

function copyCreateSemanticFields(source: Record<string, unknown>, target: Record<string, unknown>, path: string): void {
  const { createOperation, functionOperation } = validateCreateOwnership(source, path);
  target.create_operation = createOperation;
  if (functionOperation.length > 0) {
    target.function_operation = functionOperation;
  } else {
    delete target.function_operation;
  }
}

function validateConvertScheduleOwnership(record: Record<string, unknown>, path: string): void {
  const kind = typeof record.kind === 'string' ? record.kind : '';
  if (kind !== 'schedule') {
    return;
  }

  const functionOperation = typeof record.function_operation === 'string' ? record.function_operation.trim() : '';
  const scheduleOperation = typeof record.schedule_operation === 'string' ? record.schedule_operation.trim().toLowerCase() : '';
  if (functionOperation.length > 0 && GENERIC_SCHEDULE_OPERATIONS.has(scheduleOperation)) {
    throw new TaskSpecCompileError('unsupported_schedule_owner_mix', 'unsupported_schedule_owner_mix: Generic schedule operations must not specify function_operation.', [
      {
        code: 'unsupported_schedule_owner_mix',
        path: `${path}.function_operation`,
        message: 'Remove the FunctionAction operation field for Generic Schedule timer or latent nodes. Use it only for FunctionAction-owned schedule functions.',
      },
    ]);
  }
}

function invalidPinTypeMessage(path: string): string {
  return `${path} must be a structured BlueprintPinTypeSpec object.`;
}

function invalidPinTypeEvidenceMessage(path: string): string {
  return `${path} must be structured pin-type evidence.`;
}

function throwLegacyPinTypeTokenUnsupported(path: string): never {
  throw new TaskSpecCompileError('legacy_pin_type_token_unsupported', invalidPinTypeMessage(path), [
    {
      code: 'legacy_pin_type_token_unsupported',
      path,
      message: 'Use a structured BlueprintPinTypeSpec object instead of a legacy string token.',
    },
  ]);
}

function throwInvalidStructuredPinType(
  code: 'invalid_pin_type' | 'invalid_pin_type_evidence',
  path: string,
  message: string,
): never {
  throw new TaskSpecCompileError(code, message, [
    {
      code,
      path,
      message,
    },
  ]);
}

function joinIssuePath(basePath: string, issuePath: readonly (string | number)[]): string {
  if (issuePath.length === 0) return basePath;
  return `${basePath}.${issuePath.join('.')}`;
}

function requireStructuredPinType(
  value: unknown,
  path: string,
  code: 'invalid_pin_type' | 'invalid_pin_type_evidence' = 'invalid_pin_type',
): Record<string, unknown> {
  if (typeof value === 'string') {
    throwLegacyPinTypeTokenUnsupported(path);
  }
  if (!isRecord(value)) {
    throwInvalidStructuredPinType(
      code,
      path,
      code === 'invalid_pin_type' ? invalidPinTypeMessage(path) : invalidPinTypeEvidenceMessage(path),
    );
  }

  const parsed = BlueprintPinTypeSpecSchema.safeParse(value);
  if (!parsed.success) {
    const firstIssue = parsed.error.issues[0];
    throw new TaskSpecCompileError(
      code,
      code === 'invalid_pin_type' ? invalidPinTypeMessage(path) : invalidPinTypeEvidenceMessage(path),
      [{
        code,
        path: joinIssuePath(path, firstIssue?.path ?? []),
        message: firstIssue?.message ?? (code === 'invalid_pin_type'
          ? invalidPinTypeMessage(path)
          : invalidPinTypeEvidenceMessage(path)),
      }],
    );
  }
  return parsed.data as Record<string, unknown>;
}

function requireStructuredResultTypeProofEvidence(value: unknown, path: string): Record<string, unknown> {
  if (typeof value === 'string') {
    throwLegacyPinTypeTokenUnsupported(path);
  }
  if (!isRecord(value)) {
    throwInvalidStructuredPinType('invalid_pin_type_evidence', path, `${path} must be an object with pin_type evidence.`);
  }
  if (!Object.hasOwn(value, 'pin_type')) {
    throwInvalidStructuredPinType('invalid_pin_type_evidence', `${path}.pin_type`, `${path}.pin_type is required.`);
  }
  return {
    ...value,
    pin_type: requireStructuredPinType(value.pin_type, `${path}.pin_type`, 'invalid_pin_type_evidence'),
  };
}

function normalizeStructuredPinTypeEvidence(
  evidence: Record<string, unknown>,
  path: string,
): Record<string, unknown> {
  const normalized: Record<string, unknown> = { ...evidence };
  GRAPHWRITE_STRUCTURED_PIN_TYPE_EVIDENCE_FIELDS.forEach((field) => {
    if (Object.hasOwn(normalized, field)) {
      normalized[field] = requireStructuredPinType(normalized[field], `${path}.${field}`, 'invalid_pin_type_evidence');
    }
  });
  if (Object.hasOwn(normalized, GRAPHWRITE_RESULT_TYPE_PROOF_EVIDENCE_KEY)) {
    normalized[GRAPHWRITE_RESULT_TYPE_PROOF_EVIDENCE_KEY] = requireStructuredResultTypeProofEvidence(
      normalized[GRAPHWRITE_RESULT_TYPE_PROOF_EVIDENCE_KEY],
      `${path}.${GRAPHWRITE_RESULT_TYPE_PROOF_EVIDENCE_KEY}`,
    );
  }
  return normalized;
}

function validateStructuredGraphWritePinTypeUsage(record: Record<string, unknown>, path: string): void {
  GRAPHWRITE_STRUCTURED_PIN_TYPE_FIELDS.forEach((field) => {
    if (Object.hasOwn(record, field)) {
      requireStructuredPinType(record[field], `${path}.${field}`);
    }
  });
  const evidence = record.context_evidence;
  if (isRecord(evidence)) {
    normalizeStructuredPinTypeEvidence(evidence, `${path}.context_evidence`);
  }
}

function copyStructuredPinTypeFields(source: Record<string, unknown>, target: Record<string, unknown>, path = ''): void {
  GRAPHWRITE_STRUCTURED_PIN_TYPE_FIELDS.forEach((field) => {
    if (Object.hasOwn(source, field)) {
      target[field] = requireStructuredPinType(source[field], path ? `${path}.${field}` : field);
    }
  });
}

function copyContainerActionSemanticFields(source: Record<string, unknown>, target: Record<string, unknown>): void {
  GRAPH_CONTAINER_ACTION_FIELDS.forEach((field) => {
    if (Object.hasOwn(source, field)) {
      target[field] = source[field];
    }
  });
}

function copyContextEvidence(source: Record<string, unknown>, target: Record<string, unknown>, path = 'context_evidence'): void {
  const evidence = source['context_evidence'];
  if (isRecord(evidence)) {
    target['context_evidence'] = normalizeStructuredPinTypeEvidence(evidence, path);
  }
}

function defaultFieldOwnerClassForBlueprintAsset(assetPath: string): string | undefined {
  const normalizedAssetPath = assetPath.trim();
  if (normalizedAssetPath.length === 0) return undefined;
  if (/\/[^/]+\.[^/.]+_C$/.test(normalizedAssetPath)) {
    return normalizedAssetPath;
  }

  if (normalizedAssetPath.includes('.')) {
    return `${normalizedAssetPath}_C`;
  }

  const assetName = normalizedAssetPath.split('/').filter((segment) => segment.length > 0).at(-1);
  return assetName ? `${normalizedAssetPath}.${assetName}_C` : undefined;
}

function applyDefaultFieldOwnerEvidence(
  record: Record<string, unknown>,
  operation: string,
  scope: string,
  options: LogicCloneOptions,
): void {
  if (operation !== 'get' || scope !== 'variable' || !options.defaultFieldOwnerClass) {
    return;
  }
  const graphLocalName = fieldGetSymbolName(record);
  if (graphLocalName && options.graphLocalSymbols?.has(graphLocalName.toLowerCase())) {
    return;
  }

  const evidence = isRecord(record.context_evidence) ? { ...record.context_evidence } : {};
  if (!Object.hasOwn(evidence, 'field_owner_class')) {
    evidence.field_owner_class = options.defaultFieldOwnerClass;
  }
  record.context_evidence = evidence;
}

function fieldGetSymbolName(record: Record<string, unknown>): string | undefined {
  return optionalString(record, 'name') ?? optionalString(record, 'target');
}

function registerGraphLocalSymbols(statement: BlueprintLogicStatement, options: LogicCloneOptions): void {
  const statementRecord = statement as Record<string, unknown>;
  if (!options.graphLocalSymbols) {
    options.graphLocalSymbols = new Set<string>();
  }

  const kind = typeof statementRecord.kind === 'string' ? statementRecord.kind : '';
  if (kind === 'let') {
    const name = optionalString(statementRecord, 'name');
    if (name) {
      options.graphLocalSymbols.add(name.toLowerCase());
    }
  }

  const resultSymbol = optionalString(statementRecord, 'result_symbol');
  if (resultSymbol) {
    options.graphLocalSymbols.add(resultSymbol.toLowerCase());
  }
}

function fieldScopeUsesPropertyPath(scope: string): boolean {
  return FIELD_SCOPES_WITH_PROPERTY_PATH.has(scope);
}

function fieldOperationScope(record: Record<string, unknown>, path: string): { operation: string; scope: string } {
  const operation = getRequiredString(record, 'field_operation', `${path}.field_operation`).trim().toLowerCase();
  const scope = getRequiredString(record, 'field_scope', `${path}.field_scope`).trim().toLowerCase();
  if (operation !== 'get' && operation !== 'set') {
    throw new TaskSpecCompileError('unsupported_field_operation', `Unsupported field_operation: ${operation}`, [
      {
        code: 'unsupported_field_operation',
        path: `${path}.field_operation`,
        message: 'Use get or set.',
      },
    ]);
  }
  if (!SUPPORTED_FIELD_SCOPES.has(scope)) {
    throw new TaskSpecCompileError('unsupported_field_scope', `Unsupported field_scope: ${scope}`, [
      {
        code: 'unsupported_field_scope',
        path: `${path}.field_scope`,
        message: 'Use variable, property_path, component_ref, or field_access.',
      },
    ]);
  }
  return { operation, scope };
}

function delegateStatementOperation(statement: Record<string, unknown>): string | undefined {
  const kind = typeof statement.kind === 'string' ? statement.kind : '';
  if (kind === INTERNAL_DELEGATE_STATEMENT_KIND) {
    const operation = typeof statement.delegate_operation === 'string' ? statement.delegate_operation : '';
    return DELEGATE_STATEMENT_OPERATION_KINDS.has(operation) ? operation : undefined;
  }
  const operation = PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES.get(kind);
  return operation && operation !== 'component_bound_event' ? operation : undefined;
}

function validateDelegateStatementShape(statement: Record<string, unknown>, path: string): void {
  const kind = typeof statement.kind === 'string' ? statement.kind : '';
  if (kind === 'component_bound_event') {
    getRequiredString(statement, 'component', `${path}.component`);
    getRequiredString(statement, 'delegate', `${path}.delegate`);
    getRequiredString(statement, 'handler', `${path}.handler`);
    return;
  }

  const operation = delegateStatementOperation(statement);
  if (!operation) {
    return;
  }

  getRequiredString(statement, 'target', `${path}.target`);
  getRequiredString(statement, 'delegate', `${path}.delegate`);
  if (operation === 'bind' || operation === 'assign' || operation === 'unbind') {
    getRequiredString(statement, 'handler', `${path}.handler`);
  }
  if (operation === 'call') {
    validateExpressionMap(statement.args, `${path}.args`);
  }
}

function normalizeContainerActionKind(value: unknown, path: string): string {
  const containerKind = getRequiredString({ value }, 'value', path).trim().toLowerCase();
  if (!SUPPORTED_CONTAINER_KINDS.has(containerKind) || !isSupportedContainerActionKind(containerKind)) {
    throw new TaskSpecCompileError('unsupported_container_kind', `Unsupported container_kind: ${containerKind}`, [
      {
        code: 'unsupported_container_kind',
        path,
        message: 'Use array, map, or set.',
      },
    ]);
  }
  return containerKind;
}

function normalizeContainerActionOperation(containerKind: string, value: unknown, path: string): string {
  const containerOperation = getRequiredString({ value }, 'value', path).trim().toLowerCase();
  if (!SUPPORTED_CONTAINER_OPERATIONS.get(containerKind)?.has(containerOperation) || !isSupportedContainerActionOperation(containerKind, containerOperation)) {
    throw new TaskSpecCompileError('unsupported_container_operation', `Unsupported container_operation: ${containerKind}.${containerOperation}`, [
      {
        code: 'unsupported_container_operation',
        path,
        message: 'Unsupported container_operation in first-class V1 container_action.',
      },
    ]);
  }
  return containerOperation;
}

function validateContainerActionShape(
  record: Record<string, unknown>,
  path: string,
  usage: 'statement' | 'expression',
): { containerKind: string; containerOperation: string } {
  const containerKind = normalizeContainerActionKind(record.container_kind, `${path}.container_kind`);
  const containerOperation = normalizeContainerActionOperation(containerKind, record.container_operation, `${path}.container_operation`);
  if (usage === 'expression' && !isValueExpressionContainerActionOperation(containerKind, containerOperation)) {
    throw new TaskSpecCompileError('unsupported_container_operation', `Unsupported container_operation: ${containerKind}.${containerOperation}`, [
      {
        code: 'unsupported_container_operation',
        path: `${path}.container_operation`,
        message: 'Unsupported container_operation for expression container_action with a single result output.',
      },
    ]);
  }
  if (!Object.hasOwn(record, 'target')) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'container_action requires target.', [
      {
        code: 'taskspec_semantic_invalid',
        path: `${path}.target`,
        message: 'Provide the target container expression.',
      },
    ]);
  }
  getRequiredContainerActionRoles(containerKind, containerOperation).forEach((role) => {
    if (!Object.hasOwn(record, role)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `container_action ${containerKind}.${containerOperation} requires ${role}.`, [
        {
          code: 'taskspec_semantic_invalid',
          path: `${path}.${role}`,
          message: `container_action ${containerKind}.${containerOperation} requires ${role}.`,
        },
      ]);
    }
  });
  CONTAINER_ACTION_TYPE_FIELDS.forEach((field) => {
    if (!Object.hasOwn(record, field)) return;
    const value = record[field];
    if (typeof value !== 'string' || value.trim().length === 0) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}.${field} must be a non-empty string.`, [
        {
          code: 'taskspec_semantic_invalid',
          path: `${path}.${field}`,
          message: `${path}.${field} must be a non-empty string.`,
        },
      ]);
    }
  });
  if (Object.hasOwn(record, 'result_symbol')) {
    getRequiredString(record, 'result_symbol', `${path}.result_symbol`);
    if (!isValueExpressionContainerActionOperation(containerKind, containerOperation)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'result_symbol is only supported for query container_action operations with a single result output.', [
        {
          code: 'taskspec_semantic_invalid',
          path: `${path}.result_symbol`,
          message: 'result_symbol is only supported for query container_action operations with a single result output.',
        },
      ]);
    }
  }
  return { containerKind, containerOperation };
}

function normalizeContainerActionRoleValue(role: (typeof CONTAINER_ACTION_ROLE_FIELDS)[number], value: unknown): unknown {
  if (role === 'target' && typeof value === 'string' && value.trim().length > 0) {
    return { kind: 'get', name: value.trim() };
  }
  return value;
}

function normalizeContainerActionRoleValueForFlow(role: (typeof CONTAINER_ACTION_ROLE_FIELDS)[number], value: unknown): unknown {
  const normalizedValue = normalizeContainerActionRoleValue(role, value);
  if (
    isRecord(normalizedValue)
    && (normalizedValue.kind === 'get' || normalizedValue.kind === 'field')
    && typeof normalizedValue.name === 'string'
    && normalizedValue.name.trim().length > 0
    && !Object.hasOwn(normalizedValue, 'target')
  ) {
    return { ...normalizedValue, target: normalizedValue.name.trim() };
  }
  return normalizedValue;
}

function validateContainerActionRoleExpressions(record: Record<string, unknown>, path: string): void {
  CONTAINER_ACTION_ROLE_FIELDS.forEach((role) => {
    if (!Object.hasOwn(record, role)) return;
    const value = normalizeContainerActionRoleValue(role, record[role]);
    if (role === 'items' && Array.isArray(value)) {
      validateExpressionList(value, `${path}.${role}`);
      return;
    }
    validateSupportedExpression(value, `${path}.${role}`);
  });
}

function statementKindSupportsResultSymbol(kind: string): boolean {
  return VALUE_PRODUCING_STATEMENT_KINDS.has(kind);
}

function statementResultSymbolRequiresOutputEvidence(kind: string): boolean {
  return kind === 'call' || kind === 'schedule';
}

function hasExplicitResultOutputEvidence(record: Record<string, unknown>): boolean {
  const topLevelTypeFields = ['value_type', 'result_type', 'output_type', 'return_type'];
  if (topLevelTypeFields.some((field) => optionalString(record, field))) {
    return true;
  }
  if (Object.hasOwn(record, 'pin_type') || Object.hasOwn(record, 'result_pin_type') || Object.hasOwn(record, 'return_pin_type')) {
    return true;
  }
  const evidence = record.context_evidence;
  if (isRecord(evidence)) {
    const evidenceTypeFields = [
      'value_type',
      'result_type',
      'output_type',
      'return_type',
    ];
    return evidenceTypeFields.some((field) => optionalString(evidence, field))
      || GRAPHWRITE_STRUCTURED_PIN_TYPE_EVIDENCE_FIELDS.some((field) => Object.hasOwn(evidence, field))
      || Object.hasOwn(evidence, 'result_pin')
      || Object.hasOwn(evidence, 'return_pin')
      || Object.hasOwn(evidence, 'output_pin');
  }
  return false;
}

function validateStatementResultSymbol(record: Record<string, unknown>, path: string): void {
  if (!Object.hasOwn(record, 'result_symbol')) return;

  getRequiredString(record, 'result_symbol', `${path}.result_symbol`);
  const kind = typeof record.kind === 'string' ? record.kind : '';
  if (!statementKindSupportsResultSymbol(kind)) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'result_symbol requires a value-producing statement.', [
      {
        code: 'taskspec_semantic_invalid',
        path: `${path}.result_symbol`,
        message: 'Use result_symbol only on call, create, convert, schedule, or query container_action statements.',
      },
    ]);
  }
  if (statementResultSymbolRequiresOutputEvidence(kind) && !hasExplicitResultOutputEvidence(record)) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'result_symbol requires explicit result output evidence.', [
      {
        code: 'taskspec_semantic_invalid',
        path: `${path}.result_symbol`,
        message: 'call and schedule result_symbol require value_type, pin_type, or context_evidence that identifies a data output.',
      },
    ]);
  }
}

function validateSupportedStatements(statements: BlueprintLogicStatement[], path: string): void {
  statements.forEach((statement, statementIndex) => {
    const statementRecord = statement as Record<string, unknown>;
    const kind = typeof statementRecord.kind === 'string' ? statementRecord.kind : '';
    const statementPath = `${path}[${statementIndex}]`;
    validateStructuredGraphWritePinTypeUsage(statementRecord, statementPath);
    if (FORBIDDEN_AGENT_DELEGATE_INTERNAL_KINDS.has(kind)) {
      throw new TaskSpecCompileError('unsupported_statement_kind', 'Unsupported GraphWrite statement kind.', [
        {
          code: 'unsupported_statement_kind',
          path: `${statementPath}.kind`,
          message: 'Use component_bound_event or delegate.bind/delegate.assign/delegate.unbind/delegate.unbind_all/delegate.call in Agent-facing TaskSpec. The compiler owns kind=delegate + delegate_operation lowering.',
        },
      ]);
    }
    if (!SUPPORTED_GRAPH_BODY_STATEMENT_KINDS.has(kind)) {
      throw new TaskSpecCompileError('unsupported_statement_kind', 'Unsupported GraphWrite statement kind.', [
        {
          code: 'unsupported_statement_kind',
          path: `${statementPath}.kind`,
          message: 'Use call, field, create, convert, schedule, set, set_property, let, control, container_action, component_bound_event, or delegate.*.',
        },
      ]);
    }
    validateStatementResultSymbol(statementRecord, statementPath);
    if (PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES.has(kind)) {
      validateDelegateStatementShape(statementRecord, statementPath);
    } else if (kind === CONTAINER_ACTION_KIND) {
      validateContainerActionShape(statementRecord, statementPath, 'statement');
      validateContainerActionRoleExpressions(statementRecord, statementPath);
    } else if (kind === 'call') {
      validateExpressionMap(statementRecord.args, `${statementPath}.args`);
    } else if (kind === 'create') {
      validateCreateShape(statementRecord, statementPath);
      validateExpressionMap(statementRecord.args, `${statementPath}.args`);
    } else if (kind === 'convert' || kind === 'schedule') {
      validateConvertScheduleOwnership(statementRecord, statementPath);
      validateExpressionMap(statementRecord.args, `${statementPath}.args`);
    } else if (kind === 'field') {
      const { operation } = fieldOperationScope(statementRecord, statementPath);
      if (operation !== 'set') {
        throw new TaskSpecCompileError('unsupported_field_operation', 'Field statements require field_operation=set.', [
          {
            code: 'unsupported_field_operation',
            path: `${statementPath}.field_operation`,
            message: 'Field statements require field_operation=set.',
          },
        ]);
      }
      validateSupportedExpression(statementRecord.value, `${statementPath}.value`);
    } else if (kind === 'let' || kind === 'set' || kind === 'set_property') {
      validateSupportedExpression(statementRecord.value, `${statementPath}.value`);
    } else if (kind === 'control') {
      const controlKind = getControlStatementKind(statementRecord, statementPath);
      if (controlKind === 'branch') {
        validateSupportedExpression(statementRecord.condition, `${statementPath}.condition`);
        validateSupportedStatements(Array.isArray(statementRecord.then) ? statementRecord.then as BlueprintLogicStatement[] : [], `${statementPath}.then`);
        validateSupportedStatements(Array.isArray(statementRecord['else']) ? statementRecord['else'] as BlueprintLogicStatement[] : [], `${statementPath}.else`);
      } else if (controlKind === 'sequence') {
        if (Array.isArray(statementRecord.statements) && statementRecord.statements.length > 0) {
          throw new TaskSpecCompileError('unsupported_control_shape', 'Unsupported GraphWrite control shape.', [
            {
              code: 'unsupported_control_shape',
              path: `${statementPath}.statements`,
              message: 'Sequence control is an execution-flow node; place following statements after it.',
            },
          ]);
        }
      } else if (isGenericControlKind(controlKind)) {
        if (statementIndex < statements.length - 1) {
          throw new TaskSpecCompileError('unsupported_control_continuation', 'unsupported_control_continuation: Generic control statements do not provide an implicit linear continuation.', [
            {
              code: 'unsupported_control_continuation',
              path: statementPath,
              message: 'Place generic control statements as terminal statements, or use a dedicated control shape with explicit branch/body semantics.',
            },
          ]);
        }
        genericControlContextEvidence(statementRecord, controlKind, statementPath);
        validateExpressionMap(statementRecord.args, `${statementPath}.args`);
        if (Object.hasOwn(statementRecord, 'value')) {
          validateSupportedExpression(statementRecord.value, `${statementPath}.value`);
        }
      } else if (Object.hasOwn(statementRecord, 'value')) {
        validateSupportedExpression(statementRecord.value, `${statementPath}.value`);
      }
    }
  });
}

function getControlStatementKind(statementRecord: Record<string, unknown>, path: string): string {
  const controlKind = normalizeSemanticToken(
    typeof statementRecord.control === 'string'
      ? statementRecord.control
      : statementRecord.control_operation,
  );
  if (SUPPORTED_GRAPH_BODY_CONTROL_KINDS.has(controlKind)) {
    return controlKind;
  }

  throw new TaskSpecCompileError('unsupported_control_kind', 'Unsupported GraphWrite control kind.', [
    {
      code: 'unsupported_control_kind',
      path: `${path}.control`,
      message: 'Use branch, sequence, return, switch_int, switch_string, switch_name, switch_enum, multi_gate, or a supported StandardMacros control operation.',
    },
  ]);
}

function validateExpressionMap(value: unknown, path: string): void {
  if (!isRecord(value)) return;
  for (const [key, expression] of Object.entries(value)) {
    validateSupportedExpression(expression, `${path}.${key}`);
  }
}

function validateExpressionList(value: unknown, path: string): void {
  if (!Array.isArray(value)) return;
  value.forEach((expression, index) => validateSupportedExpression(expression, `${path}[${index}]`));
}

function isExplicitlyImpureCallExpression(expression: Record<string, unknown>): boolean {
  if (expression.is_pure === false || expression.pure === false || expression.is_impure === true) {
    return true;
  }
  const purity = (optionalString(expression, 'purity') ?? optionalString(expression, 'function_purity') ?? '').toLowerCase();
  return purity === 'impure' || purity === 'not_pure' || purity === 'exec';
}

function validateSupportedExpression(expression: unknown, path: string): void {
  if (!isRecord(expression)) return;
  validateStructuredGraphWritePinTypeUsage(expression, path);
  const kind = typeof expression.kind === 'string' ? expression.kind : 'literal';
  if (!SUPPORTED_GRAPH_BODY_EXPRESSION_KINDS.has(kind)) {
    throw new TaskSpecCompileError('unsupported_expression_kind', 'Unsupported GraphWrite expression kind.', [
      {
        code: 'unsupported_expression_kind',
        path: `${path}.kind`,
        message: 'Use literal, field, get, get_property, call, op, construct, deconstruct, select, create, convert, schedule, or container_action.',
      },
    ]);
  }
  if (kind === CONTAINER_ACTION_KIND) {
    validateContainerActionShape(expression, path, 'expression');
    validateContainerActionRoleExpressions(expression, path);
    return;
  }
  if (kind === 'field') {
    const { operation } = fieldOperationScope(expression, path);
    if (operation !== 'get') {
      throw new TaskSpecCompileError('unsupported_field_operation', 'Field expressions require field_operation=get.', [
        {
          code: 'unsupported_field_operation',
          path: `${path}.field_operation`,
          message: 'Field expressions require field_operation=get.',
        },
      ]);
    }
  }
  if (kind === 'create') {
    validateCreateShape(expression, path);
  }
  if (kind === 'convert' || kind === 'schedule') {
    validateConvertScheduleOwnership(expression, path);
  }
  if (kind === 'schedule' || (kind === 'call' && isExplicitlyImpureCallExpression(expression))) {
    throw new TaskSpecCompileError('impure_expression_requires_statement', `impure ${kind} expressions require a statement result_symbol.`, [
      {
        code: 'impure_expression_requires_statement',
        path: `${path}.kind`,
        message: `Use a ${kind} statement with result_symbol, then read the symbol with kind=get.`,
      },
    ]);
  }
  if (kind === 'call' || kind === 'op' || kind === 'construct' || kind === 'deconstruct' || kind === 'create' || kind === 'convert' || kind === 'schedule') {
    validateExpressionMap(expression.args, `${path}.args`);
  }
  if (kind === 'op') {
    if (Object.hasOwn(expression, 'left')) validateSupportedExpression(expression.left, `${path}.left`);
    if (Object.hasOwn(expression, 'right')) validateSupportedExpression(expression.right, `${path}.right`);
  }
  if (kind === 'deconstruct') {
    if (Object.hasOwn(expression, 'source')) validateSupportedExpression(expression.source, `${path}.source`);
    if (Object.hasOwn(expression, 'value')) validateSupportedExpression(expression.value, `${path}.value`);
  }
  if (kind === 'select') {
    validateSupportedExpression(expression.condition, `${path}.condition`);
    validateExpressionList(expression.options, `${path}.options`);
  }
}

function validateCreateShape(record: Record<string, unknown>, path: string): void {
  validateCreateOwnership(record, path);
}

function compileLogicBodyToImportPayload(
  body: { statements: BlueprintLogicStatement[] },
  prefix: string,
  path: string,
): { nodes: AgentImportNode[]; links: AgentImportLink[] } {
  const flow = compileStatementSequence(body.statements, `${toIdSegment(prefix)}_stmt`, `${path}.statements`, makeCompileFlowContext());
  return { nodes: flow.nodes, links: flow.links };
}

function compileLogicBodyToSemanticLogicSpec(
  body: { statements: BlueprintLogicStatement[] },
  prefix: string,
  options: LogicCloneOptions = {},
): Record<string, unknown> {
  return {
    schema: 'BlueprintLogicSpec.v2',
    statements: cloneLogicStatementSequenceWithCompiledIds(body.statements, `${toIdSegment(prefix)}_stmt`, options),
  };
}

interface CompiledSymbolValue {
  output?: string;
  defaultValue?: unknown;
}

interface CompileFlowContext {
  symbols: Map<string, CompiledSymbolValue>;
}

interface CompiledStatementFlow {
  nodes: AgentImportNode[];
  links: AgentImportLink[];
  entry?: string;
  exits: string[];
  preservePreviousExits?: boolean;
}

function statementResultOutputPin(kind: string): string | undefined {
  if (kind === 'create' || kind === 'convert' || kind === 'schedule') {
    return 'value';
  }
  if (kind === 'call') {
    return 'ReturnValue';
  }
  return undefined;
}

function containerActionResultOutputPin(containerKind: string, containerOperation: string): string {
  const outputPin = getContainerActionResultOutputPin(containerKind, containerOperation);
  if (!outputPin) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `container_action ${containerKind}.${containerOperation} has no single result output.`, [
      {
        code: 'container_action_no_single_result_output',
        path: 'container_operation',
        message: 'Use result_symbol only when the container_action has a single result output.',
      },
    ]);
  }
  return outputPin;
}

function makeCompileFlowContext(parent?: CompileFlowContext): CompileFlowContext {
  return {
    symbols: new Map(parent ? parent.symbols : []),
  };
}

function cloneContainerActionRoleExpressionWithCompiledIds(
  expression: unknown,
  nodeId: string,
  options: LogicCloneOptions,
): unknown {
  if (!isRecord(expression)) {
    return expression;
  }
  if (expression.kind === 'get') {
    const out: Record<string, unknown> = { ...expression, id: nodeId };
    copyContextEvidence(expression, out, `${nodeId}.context_evidence`);
    return out;
  }
  return cloneLogicExpressionWithCompiledIds(expression, nodeId, { ...options, defaultFieldOwnerClass: undefined });
}

function cloneContainerActionRoleValue(
  role: (typeof CONTAINER_ACTION_ROLE_FIELDS)[number],
  value: unknown,
  nodeId: string,
  options: LogicCloneOptions,
): unknown {
  const normalizedValue = normalizeContainerActionRoleValue(role, value);
  if (Array.isArray(normalizedValue)) {
    return normalizedValue.map((entry, index) => cloneContainerActionRoleExpressionWithCompiledIds(entry, `${nodeId}_${index + 1}`, options));
  }
  return cloneContainerActionRoleExpressionWithCompiledIds(normalizedValue, nodeId, options);
}

function cloneContainerActionWithCompiledIds(
  record: Record<string, unknown>,
  nodeId: string,
  options: LogicCloneOptions,
): Record<string, unknown> {
  const { containerKind, containerOperation } = validateContainerActionShape(
    record,
    record.kind === CONTAINER_ACTION_KIND ? nodeId : `${nodeId}.container_action`,
    'statement',
  );
  const out: Record<string, unknown> = { kind: CONTAINER_ACTION_KIND, id: nodeId };
  copyContainerActionSemanticFields(record, out);
  out.container_kind = containerKind;
  out.container_operation = containerOperation;
  CONTAINER_ACTION_ROLE_FIELDS.forEach((role) => {
    if (Object.hasOwn(record, role)) {
      out[role] = cloneContainerActionRoleValue(role, record[role], `${nodeId}_${role}`, options);
    }
  });
  copyContextEvidence(record, out, `${nodeId}.context_evidence`);
  return out;
}

function cloneLogicExpressionWithCompiledIds(
  expression: unknown,
  nodeId: string,
  options: LogicCloneOptions = {},
): unknown {
  if (!isRecord(expression)) {
    return expression;
  }

  const kind = typeof expression.kind === 'string' ? expression.kind : 'literal';
  if (kind === 'get') {
    const out: Record<string, unknown> = { ...expression, id: nodeId };
    copyContextEvidence(expression, out, `${nodeId}.context_evidence`);
    applyDefaultFieldOwnerEvidence(out, 'get', 'variable', options);
    return out;
  }
  if (kind === CONTAINER_ACTION_KIND) {
    const { containerKind, containerOperation } = validateContainerActionShape(expression, nodeId, 'expression');
    const out = cloneContainerActionWithCompiledIds(expression, nodeId, options);
    out.container_kind = containerKind;
    out.container_operation = containerOperation;
    return out;
  }
  const out: Record<string, unknown> = { ...expression, id: nodeId };
  copyContextEvidence(expression, out, `${nodeId}.context_evidence`);

  const fieldExpression = FIELD_EXPRESSION_KIND_MAP.get(kind);
  if (fieldExpression) {
    applyFieldTaxonomy(out, fieldExpression.operation, fieldExpression.scope);
    applyDefaultFieldOwnerEvidence(out, fieldExpression.operation, fieldExpression.scope, options);
    if (kind === 'get' && !Object.hasOwn(out, 'target') && typeof out.name === 'string' && out.name.trim().length > 0) {
      out.target = out.name.trim();
    }
    if (kind === 'get_property') {
      const propertyPath = requiredGraphBodyPropertyPath(expression, `${nodeId}.property_path`);
      out.property_path = propertyPath;
      out.property = propertyPath;
    }
  } else if (kind === 'field') {
    const { operation, scope } = fieldOperationScope(expression, nodeId);
    applyFieldTaxonomy(out, operation, scope);
    applyDefaultFieldOwnerEvidence(out, operation, scope, options);
    if (fieldScopeUsesPropertyPath(scope)) {
      const propertyPath = requiredGraphBodyPropertyPath(expression, `${nodeId}.property_path`);
      out.property_path = propertyPath;
      out.property = propertyPath;
    }
  } else if (kind === 'select') {
    out.condition = cloneLogicExpressionWithCompiledIds(expression.condition, `${nodeId}_index`, options);
    if (Array.isArray(expression.options)) {
      out.options = expression.options.map((option, index) => cloneLogicExpressionWithCompiledIds(option, `${nodeId}_option_${index}`, options));
    }
  } else if (kind === 'op') {
    if (Object.hasOwn(expression, 'left')) {
      out.left = cloneLogicExpressionWithCompiledIds(expression.left, `${nodeId}_left`, options);
    }
    if (Object.hasOwn(expression, 'right')) {
      out.right = cloneLogicExpressionWithCompiledIds(expression.right, `${nodeId}_right`, options);
    }
    if (isRecord(expression.args)) {
      out.args = Object.fromEntries(
        Object.entries(expression.args).map(([argName, argValue]) => [
          argName,
          cloneLogicExpressionWithCompiledIds(argValue, `${nodeId}_${toIdSegment(argName)}`, options),
        ]),
      );
    }
  } else if (kind === 'construct' && isRecord(expression.args)) {
    out.args = Object.fromEntries(
      Object.entries(expression.args).map(([argName, argValue]) => [
        argName,
        cloneLogicExpressionWithCompiledIds(argValue, `${nodeId}_${toIdSegment(argName)}`, options),
      ]),
    );
  } else if (kind === 'deconstruct') {
    if (Object.hasOwn(expression, 'source')) {
      out.source = cloneLogicExpressionWithCompiledIds(expression.source, `${nodeId}_source`, options);
    }
    if (Object.hasOwn(expression, 'value')) {
      out.value = cloneLogicExpressionWithCompiledIds(expression.value, `${nodeId}_value`, options);
    }
    if (isRecord(expression.args)) {
      out.args = Object.fromEntries(
        Object.entries(expression.args).map(([argName, argValue]) => [
          argName,
          cloneLogicExpressionWithCompiledIds(argValue, `${nodeId}_${toIdSegment(argName)}`, options),
        ]),
      );
    }
  } else if (isRecord(expression.args)) {
    out.args = Object.fromEntries(
      Object.entries(expression.args).map(([argName, argValue]) => [
        argName,
        cloneLogicExpressionWithCompiledIds(argValue, `${nodeId}_${toIdSegment(argName)}`, options),
      ]),
    );
  }
  if (kind === 'create') {
    copyCreateSemanticFields(expression, out, nodeId);
    copyStructuredPinTypeFields(expression, out, nodeId);
  }

  return out;
}

function cloneLogicStatementWithCompiledIds(
  statement: BlueprintLogicStatement,
  statementId: string,
  options: LogicCloneOptions = {},
): BlueprintLogicStatement {
  const statementRecord = statement as Record<string, unknown>;
  const kind = typeof statementRecord.kind === 'string'
    ? statementRecord.kind
    : '';
  if (kind === CONTAINER_ACTION_KIND) {
    return cloneContainerActionWithCompiledIds(statementRecord, statementId, options) as BlueprintLogicStatement;
  }
  const out: Record<string, unknown> = { ...statementRecord, id: statementId };
  copyContextEvidence(statementRecord, out, `${statementId}.context_evidence`);
  const delegateOperation = delegateStatementOperation(statementRecord);

  const fieldStatement = FIELD_STATEMENT_KIND_MAP.get(kind);
  if (fieldStatement) {
    applyFieldTaxonomy(out, fieldStatement.operation, fieldStatement.scope);
    if (kind === 'set_property') {
      const propertyPath = requiredGraphBodyPropertyPath(statementRecord, `${statementId}.property_path`);
      out.property_path = propertyPath;
      out.property = propertyPath;
    }
    out.value = cloneLogicExpressionWithCompiledIds(statementRecord.value, `${statementId}_value`, options);
  } else if (kind === 'field') {
    const { operation, scope } = fieldOperationScope(statementRecord, statementId);
    applyFieldTaxonomy(out, operation, scope);
    if (fieldScopeUsesPropertyPath(scope)) {
      const propertyPath = requiredGraphBodyPropertyPath(statementRecord, `${statementId}.property_path`);
      out.property_path = propertyPath;
      out.property = propertyPath;
    }
    if (Object.hasOwn(statementRecord, 'value')) {
      out.value = cloneLogicExpressionWithCompiledIds(statementRecord.value, `${statementId}_value`, options);
    }
  } else if (kind === 'component_bound_event') {
    out.kind = 'component_bound_event';
  } else if (delegateOperation) {
    out.kind = 'delegate';
    out.delegate_operation = delegateOperation;
    if (delegateOperation === 'unbind') {
      out.unbind_mode = 'single';
    } else if (delegateOperation === 'clear') {
      out.unbind_mode = 'all';
    } else if (delegateOperation === 'call' && isRecord(statementRecord.args)) {
      out.args = Object.fromEntries(
        Object.entries(statementRecord.args).map(([argName, argValue]) => [
          argName,
          cloneLogicExpressionWithCompiledIds(argValue, `${statementId}_arg_${toIdSegment(argName)}`, options),
        ]),
      );
    }
  } else if (kind === 'let') {
    out.value = cloneLogicExpressionWithCompiledIds(statementRecord.value, `${statementId}_value`, options);
  } else if ((kind === 'call' || kind === 'create' || kind === 'convert' || kind === 'schedule') && isRecord(statementRecord.args)) {
    const args = statementRecord.args as Record<string, unknown>;
    out.args = Object.fromEntries(
      Object.entries(args).map(([argName, argValue]) => [
        argName,
        cloneLogicExpressionWithCompiledIds(argValue, `${statementId}_arg_${toIdSegment(argName)}`, options),
      ]),
    );
    if (kind === 'create') {
      copyCreateSemanticFields(statementRecord, out, statementId);
      copyStructuredPinTypeFields(statementRecord, out, statementId);
    }
  } else if (kind === 'control') {
    const normalizedControlKind = getControlStatementKind(statementRecord, statementId);

    if (normalizedControlKind === 'branch') {
      out.kind = normalizedControlKind;
      delete out.control;
      out.condition = cloneLogicExpressionWithCompiledIds(statementRecord.condition, `${statementId}_condition`, options);
      if (Array.isArray(statementRecord.then)) {
        out.then = cloneLogicStatementSequenceWithCompiledIds(statementRecord.then as BlueprintLogicStatement[], `${statementId}_then`, options);
      }
      if (Array.isArray(statementRecord['else'])) {
        out.else = cloneLogicStatementSequenceWithCompiledIds(statementRecord['else'] as BlueprintLogicStatement[], `${statementId}_else`, options);
      }
    } else if (normalizedControlKind === 'sequence') {
      out.kind = normalizedControlKind;
      delete out.control;
      delete out.statements;
    } else if (normalizedControlKind === 'return') {
      out.kind = normalizedControlKind;
      delete out.control;
      if (Object.hasOwn(statementRecord, 'value')) {
        out.value = cloneLogicExpressionWithCompiledIds(statementRecord.value, `${statementId}_value`, options);
      }
    } else {
      applyGenericControlSemanticFields(statementRecord, out, normalizedControlKind, statementId);
      if (isRecord(statementRecord.args)) {
        out.args = Object.fromEntries(
          Object.entries(statementRecord.args).map(([argName, argValue]) => [
            argName,
            cloneLogicExpressionWithCompiledIds(argValue, `${statementId}_arg_${toIdSegment(argName)}`, options),
          ]),
        );
      }
    }
  }
  if (kind === 'create') {
    copyCreateSemanticFields(statementRecord, out, statementId);
    copyStructuredPinTypeFields(statementRecord, out, statementId);
  }

  return out as BlueprintLogicStatement;
}

function cloneLogicStatementSequenceWithCompiledIds(
  statements: BlueprintLogicStatement[],
  idPrefix: string,
  options: LogicCloneOptions = {},
): BlueprintLogicStatement[] {
  const sequenceOptions: LogicCloneOptions = {
    ...options,
    graphLocalSymbols: new Set(options.graphLocalSymbols ?? []),
  };
  return statements.map((statement, statementIndex) => {
    const cloned = cloneLogicStatementWithCompiledIds(statement, `${idPrefix}_${statementIndex + 1}`, sequenceOptions);
    registerGraphLocalSymbols(statement, sequenceOptions);
    return cloned;
  });
}

function compileStatementSequence(
  statements: BlueprintLogicStatement[],
  idPrefix: string,
  path: string,
  context: CompileFlowContext,
): CompiledStatementFlow {
  const nodes: AgentImportNode[] = [];
  const links: AgentImportLink[] = [];
  let entry: string | undefined;
  let previousExits: string[] = [];

  statements.forEach((statement, statementIndex) => {
    const statementId = `${idPrefix}_${statementIndex + 1}`;
    const statementPath = `${path}[${statementIndex}]`;
    const flow = compileStatementFlow(statement, statementId, statementPath, context);
    nodes.push(...flow.nodes);
    links.push(...flow.links);
    if (!entry) {
      entry = flow.entry;
    }
    const flowEntry = flow.entry;
    if (flowEntry) {
      previousExits.forEach((exit) => {
        links.push({ kind: 'exec', from: exit, to: flowEntry });
      });
      previousExits = flow.exits;
    } else if (!flow.preservePreviousExits) {
      previousExits = flow.exits;
    }
  });

  return { nodes, links, entry, exits: previousExits };
}

function compileContainerActionRoleInputs(
  statementRecord: Record<string, unknown>,
  nodeId: string,
  path: string,
  node: AgentImportNode,
  nodes: AgentImportNode[],
  links: AgentImportLink[],
  context: CompileFlowContext,
): void {
  const inputValues: Record<string, unknown> = {};
  CONTAINER_ACTION_ROLE_FIELDS.forEach((role) => {
    if (!Object.hasOwn(statementRecord, role)) return;
    const rawValue = normalizeContainerActionRoleValueForFlow(role, statementRecord[role]);
    if (role === 'items' && Array.isArray(rawValue)) {
      inputValues[role] = rawValue.map((entry) => literalValue(normalizeContainerActionRoleValueForFlow(role, entry)));
      return;
    }
    const roleFlow = compileValueExpression(rawValue, `${nodeId}_${role}`, `${path}.${role}`, context);
    nodes.push(...roleFlow.nodes);
    links.push(...roleFlow.links);
    if (roleFlow.output) {
      links.push({ kind: 'data', from: roleFlow.output, to: `${nodeId}.${role}` });
    } else {
      inputValues[role] = roleFlow.defaultValue;
    }
  });
  node.inputs = {
    ...(node.inputs ?? {}),
    ...inputValues,
  };
}

function isContainerActionPureOperation(containerKind: string, containerOperation: string): boolean {
  return isExpressionContainerActionOperation(containerKind, containerOperation);
}

function compileStatementFlow(statement: BlueprintLogicStatement, nodeId: string, path: string, context: CompileFlowContext): CompiledStatementFlow {
  const statementRecord = statement as Record<string, unknown>;
  const kind = typeof statementRecord.kind === 'string' ? statementRecord.kind : '';
  const delegateOperation = delegateStatementOperation(statementRecord);
  if (kind === 'control') {
    const controlKind = getControlStatementKind(statementRecord, path);
    if (controlKind === 'branch') {
      return compileBranchStatementFlow({ ...statementRecord, kind: 'branch' } as BlueprintLogicStatement, nodeId, path, context);
    }
    if (controlKind === 'return') {
      return compileReturnStatementFlow(statementRecord, nodeId, path, context);
    }
    if (isGenericControlKind(controlKind)) {
      const node = compileStatementNode(statement, nodeId, path);
      return {
        nodes: [node],
        links: [],
        entry: `${nodeId}.execute`,
        exits: [`${nodeId}.then`],
      };
    }
    return compileSequenceControlStatementFlow(statementRecord, nodeId, path, context);
  }
  if (kind === 'branch') {
    return compileBranchStatementFlow(statement, nodeId, path, context);
  }
  if (kind === 'return') {
    return compileReturnStatementFlow(statementRecord, nodeId, path, context);
  }
  if (kind === 'sequence') {
    return compileSequenceControlStatementFlow(statementRecord, nodeId, path, context);
  }
  if (kind === 'let') {
    const name = getRequiredString(statementRecord, 'name', `${path}.name`);
    const valueFlow = compileValueExpression(statementRecord['value'], `${nodeId}_value`, `${path}.value`, context);
    context.symbols.set(name.toLowerCase(), {
      output: valueFlow.output,
      defaultValue: valueFlow.defaultValue,
    });
    return {
      nodes: valueFlow.nodes,
      links: valueFlow.links,
      exits: [],
      preservePreviousExits: true,
    };
  }
  if (kind === CONTAINER_ACTION_KIND) {
    const { containerKind, containerOperation } = validateContainerActionShape(statementRecord, path, 'statement');
    const node = compileStatementNode(statement, nodeId, path);
    const nodes: AgentImportNode[] = [node];
    const links: AgentImportLink[] = [];
    compileContainerActionRoleInputs(statementRecord, nodeId, path, node, nodes, links, context);
    const resultSymbol = optionalString(statementRecord, 'result_symbol');
    if (resultSymbol) {
      context.symbols.set(resultSymbol.toLowerCase(), {
        output: `${nodeId}.${containerActionResultOutputPin(containerKind, containerOperation)}`,
      });
    }
    if (isContainerActionPureOperation(containerKind, containerOperation)) {
      return {
        nodes,
        links,
        exits: [],
        preservePreviousExits: true,
      };
    }
    return {
      nodes,
      links,
      entry: `${nodeId}.execute`,
      exits: [`${nodeId}.then`],
    };
  }

  const node = compileStatementNode(statement, nodeId, path);
  const nodes: AgentImportNode[] = [node];
  const links: AgentImportLink[] = [];
  if (kind === 'call' || kind === 'create' || kind === 'convert' || kind === 'schedule' || delegateOperation === 'call') {
    const inputValues: Record<string, unknown> = {};
    if (isRecord(statementRecord['args'])) {
      for (const [argName, argValue] of Object.entries(statementRecord['args'])) {
        const argFlow = compileValueExpression(argValue, `${nodeId}_arg_${toIdSegment(argName)}`, `${path}.args.${argName}`, context);
        nodes.push(...argFlow.nodes);
        links.push(...argFlow.links);
        if (argFlow.output) {
          links.push({ kind: 'data', from: argFlow.output, to: `${nodeId}.${argName}` });
        } else {
          inputValues[argName] = argFlow.defaultValue;
        }
      }
    }
    node.inputs = inputValues;
  }
  if (kind === 'set' || kind === 'set_property' || kind === 'field') {
    let valuePinName = kind === 'set'
      ? getRequiredString(statementRecord, 'target', `${path}.target`)
      : 'value';
    if (kind === 'field') {
      const { operation, scope } = fieldOperationScope(statementRecord, path);
      if (operation !== 'set') {
        throw new TaskSpecCompileError('unsupported_field_operation', 'Field statements require field_operation=set.', [
          {
            code: 'unsupported_field_operation',
            path: `${path}.field_operation`,
            message: 'Field statements require field_operation=set.',
          },
        ]);
      }
      valuePinName = scope === 'variable'
        ? getRequiredString(statementRecord, 'target', `${path}.target`)
        : 'value';
    }
    const valueFlow = compileValueExpression(statementRecord['value'], `${nodeId}_value`, `${path}.value`, context);
    nodes.push(...valueFlow.nodes);
    links.push(...valueFlow.links);
    if (valueFlow.output) {
      links.push({ kind: 'data', from: valueFlow.output, to: `${nodeId}.${valuePinName}` });
      delete node.value;
    } else {
      node.value = valueExprToString(valueFlow.defaultValue);
    }
  }
  const resultSymbol = optionalString(statementRecord, 'result_symbol');
  const outputPin = statementResultOutputPin(kind);
  if (resultSymbol && outputPin && statementKindSupportsResultSymbol(kind) && (!statementResultSymbolRequiresOutputEvidence(kind) || hasExplicitResultOutputEvidence(statementRecord))) {
    context.symbols.set(resultSymbol.toLowerCase(), { output: `${nodeId}.${outputPin}` });
  }
  return {
    nodes,
    links,
    entry: `${nodeId}.execute`,
    exits: [`${nodeId}.then`],
  };
}

function compileReturnStatementFlow(statementRecord: Record<string, unknown>, nodeId: string, path: string, context: CompileFlowContext): CompiledStatementFlow {
  const node: AgentImportNode = { id: nodeId, kind: 'return' } as AgentImportNode;
  const nodes: AgentImportNode[] = [node];
  const links: AgentImportLink[] = [];
  if (Object.hasOwn(statementRecord, 'value')) {
    const valueFlow = compileValueExpression(statementRecord.value, `${nodeId}_value`, `${path}.value`, context);
    nodes.push(...valueFlow.nodes);
    links.push(...valueFlow.links);
    if (valueFlow.output) {
      links.push({ kind: 'data', from: valueFlow.output, to: `${nodeId}.value` });
    } else {
      node.value = valueExprToString(valueFlow.defaultValue);
    }
  }
  return {
    nodes,
    links,
    entry: `${nodeId}.execute`,
    exits: [],
  };
}

function compileSequenceControlStatementFlow(statementRecord: Record<string, unknown>, nodeId: string, path: string, context: CompileFlowContext): CompiledStatementFlow {
  const sequenceNode: AgentImportNode = { id: nodeId, kind: 'sequence' } as AgentImportNode;
  const nodes: AgentImportNode[] = [sequenceNode];
  const links: AgentImportLink[] = [];
  const nestedStatements = Array.isArray(statementRecord.statements)
    ? statementRecord.statements as BlueprintLogicStatement[]
    : [];
  const nestedFlow = compileStatementSequence(nestedStatements, `${nodeId}_sequence`, `${path}.statements`, makeCompileFlowContext(context));
  nodes.push(...nestedFlow.nodes);
  links.push(...nestedFlow.links);
  if (nestedFlow.entry) {
    links.push({ kind: 'exec', from: `${nodeId}.then`, to: nestedFlow.entry });
  }
  return {
    nodes,
    links,
    entry: `${nodeId}.execute`,
    exits: nestedFlow.entry ? nestedFlow.exits : [`${nodeId}.then`],
  };
}

function compileBranchStatementFlow(statement: BlueprintLogicStatement, nodeId: string, path: string, context: CompileFlowContext): CompiledStatementFlow {
  const branchStatement = statement as BlueprintLogicStatement & {
    condition?: unknown;
    then?: unknown;
    else?: unknown;
  };
  const branchNode: AgentImportNode = { id: nodeId, kind: 'branch' };
  const nodes: AgentImportNode[] = [branchNode];
  const links: AgentImportLink[] = [];
  const conditionFlow = compileBranchCondition(branchStatement.condition, `${nodeId}_condition`, `${path}.condition`, context);
  nodes.push(...conditionFlow.nodes);
  links.push(...conditionFlow.links);
  if (conditionFlow.output) {
    links.push({ kind: 'data', from: conditionFlow.output, to: `${nodeId}.Condition` });
  }
  if (conditionFlow.defaultValue !== undefined) {
    branchNode.inputs = { Condition: conditionFlow.defaultValue };
  }

  const thenStatements = Array.isArray(branchStatement.then)
    ? (branchStatement.then as BlueprintLogicStatement[])
    : [];
  const elseStatements = Array.isArray(branchStatement.else)
    ? (branchStatement.else as BlueprintLogicStatement[])
    : [];
  const thenFlow = compileStatementSequence(thenStatements, `${nodeId}_then`, `${path}.then`, makeCompileFlowContext(context));
  const elseFlow = compileStatementSequence(elseStatements, `${nodeId}_else`, `${path}.else`, makeCompileFlowContext(context));
  nodes.push(...thenFlow.nodes, ...elseFlow.nodes);
  links.push(...thenFlow.links, ...elseFlow.links);

  const exits: string[] = [];
  if (thenFlow.entry) {
    links.push({ kind: 'exec', from: `${nodeId}.then`, to: thenFlow.entry });
    exits.push(...thenFlow.exits);
  } else {
    exits.push(`${nodeId}.then`);
  }
  if (elseFlow.entry) {
    links.push({ kind: 'exec', from: `${nodeId}.else`, to: elseFlow.entry });
    exits.push(...elseFlow.exits);
  } else {
    exits.push(`${nodeId}.else`);
  }

  return {
    nodes,
    links,
    entry: `${nodeId}.execute`,
    exits,
  };
}

interface CompiledConditionFlow {
  nodes: AgentImportNode[];
  links: AgentImportLink[];
  output?: string;
  defaultValue?: unknown;
}

function compileBranchCondition(condition: unknown, nodeId: string, path: string, context: CompileFlowContext): CompiledConditionFlow {
  return compileValueExpression(condition, nodeId, path, context);
}

function compileValueExpression(expression: unknown, nodeId: string, path: string, context: CompileFlowContext): CompiledConditionFlow {
  if (!isRecord(expression)) {
    return { nodes: [], links: [], defaultValue: literalValue(expression) };
  }

  const kind = typeof expression.kind === 'string' ? expression.kind : 'literal';
  if (kind === 'literal') {
    return { nodes: [], links: [], defaultValue: literalValue(expression) };
  }

  if (!SUPPORTED_GRAPH_BODY_EXPRESSION_KINDS.has(kind)) {
    throw new TaskSpecCompileError('unsupported_expression_kind', `Unsupported expression kind: ${kind}`, [
      {
        code: 'unsupported_expression_kind',
        path: `${path}.kind`,
        message: 'Use literal, field, get, get_property, call, op, construct, deconstruct, select, create, convert, schedule, or container_action.',
      },
    ]);
  }

  if (kind === CONTAINER_ACTION_KIND) {
    const { containerKind, containerOperation } = validateContainerActionShape(expression, path, 'expression');
    const node: AgentImportNode = {
      id: nodeId,
      kind: CONTAINER_ACTION_KIND,
      inputs: {},
    };
    copyContainerActionSemanticFields(expression, node as Record<string, unknown>);
    (node as Record<string, unknown>).container_kind = containerKind;
    (node as Record<string, unknown>).container_operation = containerOperation;
    copyContextEvidence(expression, node as Record<string, unknown>, `${path}.context_evidence`);
    const nodes: AgentImportNode[] = [node];
    const links: AgentImportLink[] = [];
    compileContainerActionRoleInputs(expression, nodeId, path, node, nodes, links, context);
    return { nodes, links, output: `${nodeId}.${containerActionResultOutputPin(containerKind, containerOperation)}` };
  }

  if (kind === 'get' || kind === 'get_property' || kind === 'field') {
    const target = kind === 'get'
      ? (optionalString(expression, 'target') ?? getRequiredString(expression, 'name', `${path}.name`))
      : getRequiredString(expression, 'target', `${path}.target`);
    const fieldExpression = kind === 'field'
      ? fieldOperationScope(expression, path)
      : FIELD_EXPRESSION_KIND_MAP.get(kind);
    if (!fieldExpression || fieldExpression.operation !== 'get') {
      throw new TaskSpecCompileError('unsupported_field_operation', 'Field expressions require field_operation=get.', [
        {
          code: 'unsupported_field_operation',
          path: `${path}.field_operation`,
          message: 'Field expressions require field_operation=get.',
        },
      ]);
    }
    if (fieldExpression.scope === 'variable') {
      const symbol = context.symbols.get(target.toLowerCase());
      if (symbol) {
        return { nodes: [], links: [], output: symbol.output, defaultValue: symbol.defaultValue };
      }
    }
    const outputPin = fieldScopeUsesPropertyPath(fieldExpression.scope) ? 'value' : target;
    const node = { id: nodeId, kind: 'field', var: target, target } as AgentImportNode;
    copyContextEvidence(expression, node as Record<string, unknown>, `${path}.context_evidence`);
    applyFieldTaxonomy(node as Record<string, unknown>, fieldExpression.operation, fieldExpression.scope);
    if (fieldScopeUsesPropertyPath(fieldExpression.scope)) {
      const propertyPath = requiredGraphBodyPropertyPath(expression, path);
      (node as Record<string, unknown>).property_path = propertyPath;
      (node as Record<string, unknown>).property = propertyPath;
    }
    return { nodes: [node], links: [], output: `${nodeId}.${outputPin}` };
  }

  const nodes: AgentImportNode[] = [];
  const links: AgentImportLink[] = [];
  const node: AgentImportNode = {
    id: nodeId,
    kind,
    inputs: {},
  };
  copyContextEvidence(expression, node as Record<string, unknown>, `${path}.context_evidence`);
  if (kind === 'call') {
    node.function = getRequiredString(expression, 'target', `${path}.target`);
  }
  if (kind === 'op') {
    node.function = getRequiredString(expression, 'op', `${path}.op`);
    if (Object.hasOwn(expression, 'left')) {
    compileExpressionInput(expression['left'], 'A', `${nodeId}_left`, `${path}.left`, node, nodes, links, context);
    }
    if (Object.hasOwn(expression, 'right')) {
    compileExpressionInput(expression['right'], 'B', `${nodeId}_right`, `${path}.right`, node, nodes, links, context);
    }
    if (isRecord(expression.args)) {
      for (const [argName, argValue] of Object.entries(expression.args)) {
        compileExpressionInput(argValue, argName, `${nodeId}_${toIdSegment(argName)}`, `${path}.args.${argName}`, node, nodes, links, context);
      }
    }
  } else if (kind === 'select') {
    if (Array.isArray(expression.options)) {
      expression.options.forEach((option, index) => {
        compileExpressionInput(option, `Option${index}`, `${nodeId}_option_${index}`, `${path}.options[${index}]`, node, nodes, links, context);
      });
    }
    compileExpressionInput(expression['condition'], 'Index', `${nodeId}_index`, `${path}.condition`, node, nodes, links, context);
  } else if (kind === 'construct') {
    const structType = requiredConstructType(expression, path);
    (node as Record<string, unknown>).type = structType;
    (node as Record<string, unknown>).struct_path = structType;
    if (isRecord(expression.args)) {
      for (const [argName, argValue] of Object.entries(expression.args)) {
        compileExpressionInput(argValue, argName, `${nodeId}_${toIdSegment(argName)}`, `${path}.args.${argName}`, node, nodes, links, context);
      }
    }
  } else if (kind === 'deconstruct') {
    const structType = optionalString(expression, 'type') ?? optionalString(expression, 'struct_path');
    if (structType) {
      (node as Record<string, unknown>).type = structType;
      (node as Record<string, unknown>).struct_path = structType;
    }
    const propertyPath = optionalGraphBodyPropertyPath(expression);
    if (propertyPath) {
      (node as Record<string, unknown>).property_path = propertyPath;
      (node as Record<string, unknown>).property = propertyPath;
    }
    if (Object.hasOwn(expression, 'source')) {
      compileExpressionInput(expression['source'], 'Input', `${nodeId}_source`, `${path}.source`, node, nodes, links, context);
    } else if (Object.hasOwn(expression, 'value')) {
      compileExpressionInput(expression['value'], 'Input', `${nodeId}_value`, `${path}.value`, node, nodes, links, context);
    }
  } else if (kind === 'create') {
    node.create_operation = getRequiredString(expression, 'create_operation', `${path}.create_operation`);
    copyCreateSemanticFields(expression, node as Record<string, unknown>, path);
    const target = optionalString(expression, 'target');
    const classPath = optionalString(expression, 'class_path');
    const assetPath = optionalString(expression, 'asset_path');
    if (target) node.target = target;
    if (classPath) node.class_path = classPath;
    if (assetPath) node.asset_path = assetPath;
    copyStructuredPinTypeFields(expression, node as Record<string, unknown>, path);
    if (isRecord(expression.args)) {
      for (const [argName, argValue] of Object.entries(expression.args)) {
        compileExpressionInput(argValue, argName, `${nodeId}_${toIdSegment(argName)}`, `${path}.args.${argName}`, node, nodes, links, context);
      }
    }
  } else if (kind === 'convert' || kind === 'schedule') {
    copyConvertScheduleSemanticFields(expression, node as Record<string, unknown>);
    const target = optionalString(expression, 'target');
    if (target) node.target = target;
    if (isRecord(expression.args)) {
      for (const [argName, argValue] of Object.entries(expression.args)) {
        compileExpressionInput(argValue, argName, `${nodeId}_${toIdSegment(argName)}`, `${path}.args.${argName}`, node, nodes, links, context);
      }
    }
  } else if (isRecord(expression.args)) {
    for (const [argName, argValue] of Object.entries(expression.args)) {
      compileExpressionInput(argValue, argName, `${nodeId}_${toIdSegment(argName)}`, `${path}.args.${argName}`, node, nodes, links, context);
    }
  }
  nodes.unshift(node);

  const outputPin = kind === 'construct' || kind === 'deconstruct' || kind === 'select' || kind === 'create' || kind === 'convert' || kind === 'schedule' ? 'value' : 'ReturnValue';
  return { nodes, links, output: `${nodeId}.${outputPin}` };
}

function optionalGraphBodyPropertyPath(record: Record<string, unknown>): string | undefined {
  return optionalString(record, 'property_path') ?? optionalString(record, 'property');
}

function requiredGraphBodyPropertyPath(record: Record<string, unknown>, path: string): string {
  const propertyPath = optionalGraphBodyPropertyPath(record);
  if (propertyPath) return propertyPath;
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}.property_path must be a non-empty string.`, [
    {
      code: 'missing_property_path',
      path: `${path}.property_path`,
      message: 'Provide property_path for graph-body property access.',
    },
  ]);
}

function requiredConstructType(record: Record<string, unknown>, path: string): string {
  const structType = optionalString(record, 'type') ?? optionalString(record, 'struct_path');
  if (structType) return structType;
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}.type must be a non-empty string.`, [
    {
      code: 'missing_construct_type',
      path: `${path}.type`,
      message: 'Provide type for construct expressions.',
    },
  ]);
}

function compileExpressionInput(
  expression: unknown,
  pinName: string,
  nodeId: string,
  path: string,
  targetNode: AgentImportNode,
  nodes: AgentImportNode[],
  links: AgentImportLink[],
  context: CompileFlowContext,
): void {
  const valueFlow = compileValueExpression(expression, nodeId, path, context);
  nodes.push(...valueFlow.nodes);
  links.push(...valueFlow.links);
  if (valueFlow.output) {
    links.push({ kind: 'data', from: valueFlow.output, to: `${targetNode.id}.${pinName}` });
  } else {
    targetNode.inputs = targetNode.inputs ?? {};
    targetNode.inputs[pinName] = valueFlow.defaultValue;
  }
}

function defaultPatchScope(kind: string): string {
  if (kind === 'set_node_comment') return 'node_comment';
  if (kind === 'connect_pins') return 'connect_pins';
  if (kind === 'disconnect_link') return 'disconnect_link';
  if (kind === 'replace_link') return 'replace_link';
  if (kind === 'delete_owned_node') return 'node_delete';
  return 'pin_default';
}

function normalizeReplaceSelector(
  replaceScope: string,
  selector: Record<string, unknown>,
): Record<string, unknown> {
  const kind = getRequiredString(selector, 'kind', 'behavior.replace.selector.kind');
  const out: Record<string, unknown> = {};
  copyOptionalStringFields(selector, out, ['graph_id', 'node_ref', 'node_path']);

  if (replaceScope === 'graph') {
    requireSelectorKind(kind, 'graph', replaceScope);
    return out;
  }
  if (replaceScope === 'custom_event_body') {
    requireSelectorKind(kind, 'custom_event', replaceScope);
    out['entry_name'] = getRequiredString(selector, 'name', 'behavior.replace.selector.name');
    return out;
  }
  if (replaceScope === 'event_body') {
    requireSelectorKind(kind, 'event', replaceScope);
    out['entry_name'] = getRequiredString(selector, 'name', 'behavior.replace.selector.name');
    return out;
  }
  if (replaceScope === 'function_body') {
    requireSelectorKind(kind, 'function', replaceScope);
    out['function_name'] = getRequiredString(selector, 'name', 'behavior.replace.selector.name');
    return out;
  }

  requireSelectorKind(kind, 'block', replaceScope);
  out['block_id'] = getRequiredString(selector, 'block_id', 'behavior.replace.selector.block_id');
  copyOptionalStringFields(selector, out, ['target_ref', 'block_ref']);
  return out;
}

function requireSelectorKind(actual: string, expected: string, replaceScope: string): void {
  if (actual === expected) return;
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `replace selector kind must match ${replaceScope}.`, [
    {
      code: 'replace_selector_scope_mismatch',
      path: 'behavior.replace.selector.kind',
      message: `${replaceScope} requires selector.kind="${expected}".`,
    },
  ]);
}

function normalizePatchTargetRef(kind: string, targetRef: Record<string, unknown>, path: string): Record<string, unknown> {
  const out = { ...targetRef };
  assertBlockScopedGraphWriteRef(targetRef, path);
  getRequiredString(targetRef, 'node_ref', `${path}.node_ref`);
  if (kind === 'set_pin_default' || kind === 'connect_pins' || kind === 'disconnect_link' || kind === 'replace_link') {
    getRequiredString(targetRef, 'pin_ref', `${path}.pin_ref`);
  }
  if (kind === 'disconnect_link' || kind === 'replace_link') {
    getRequiredString(targetRef, 'link_ref', `${path}.link_ref`);
  }
  return out;
}

function rejectRedundantOwnedPatchExpectedOldState(kind: string, patch: Record<string, unknown>, path: string): void {
  if (
    ['connect_pins', 'disconnect_link', 'replace_link', 'delete_owned_node'].includes(kind) &&
    Object.hasOwn(patch, 'expected_old_state')
  ) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${kind} does not support expected_old_state.`, [
      {
        code: 'redundant_owned_patch_expected_old_state',
        path: `${path}.expected_old_state`,
        message: 'Use read_context refs directly; P0-D owned link/delete patches do not accept redundant expected_old_state.',
      },
    ]);
  }
}

function compilePatchPayload(
  kind: string,
  patch: Record<string, unknown>,
  path: string,
  targetBlockId: string,
): Record<string, unknown> {
  if (kind === 'set_pin_default') {
    if (!Object.hasOwn(patch, 'value')) {
      throwMissingPatchValue(path, 'set_pin_default requires value.');
    }
    return {
      value: patchValueToString(literalValue(patch['value'])),
    };
  }
  if (kind === 'set_node_comment') {
    if (!Object.hasOwn(patch, 'value')) {
      throwMissingPatchValue(path, 'set_node_comment requires value.');
    }
    return {
      comment: patchValueToString(literalValue(patch['value'])),
    };
  }
  if (kind === 'connect_pins') {
    const sourceRef = normalizePatchEndpointRef(patch, 'source_ref', path, targetBlockId);
    return {
      source_block_id: targetBlockId,
      source_node_ref: sourceRef.nodeRef,
      source_pin_ref: sourceRef.pinRef,
    };
  }
  if (kind === 'disconnect_link') {
    return {};
  }
  if (kind === 'replace_link') {
    const replacementRef = normalizePatchEndpointRef(patch, 'replacement_ref', path, targetBlockId);
    return {
      replacement_block_id: targetBlockId,
      replacement_node_ref: replacementRef.nodeRef,
      replacement_pin_ref: replacementRef.pinRef,
    };
  }
  if (kind === 'delete_owned_node') {
    return normalizeDeleteOwnedNodePolicy(patch, path);
  }
  throw new TaskSpecCompileError('unsupported_graph_write_patch', `Unsupported GraphWrite patch kind: ${kind}`, [
    {
      code: 'unsupported_graph_write_patch',
      path: `${path}.kind`,
      message: `Use ${OWNED_GRAPH_PATCH_KINDS.join(', ')}.`,
    },
  ]);
}

function isOwnedGraphPatchKind(kind: string): boolean {
  return OWNED_GRAPH_PATCH_KINDS.includes(kind as (typeof OWNED_GRAPH_PATCH_KINDS)[number]);
}

function normalizePatchEndpointRef(
  patch: Record<string, unknown>,
  field: 'source_ref' | 'replacement_ref',
  path: string,
  targetBlockId: string,
): { nodeRef: string; pinRef: string } {
  const ref = requiredRecord(patch, field, `${path}.${field}`);
  if (Object.hasOwn(ref, 'block_id')) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${field}.block_id is redundant.`, [
      {
        code: 'redundant_patch_endpoint_block_id',
        path: `${path}.${field}.block_id`,
        message: `${field}.block_id is redundant; the compiler derives it from target_ref.block_id.`,
      },
    ]);
  }
  assertBlockScopedGraphWriteRef({ ...ref, block_id: targetBlockId }, `${path}.${field}`);
  return {
    nodeRef: getRequiredString(ref, 'node_ref', `${path}.${field}.node_ref`),
    pinRef: getRequiredString(ref, 'pin_ref', `${path}.${field}.pin_ref`),
  };
}

function normalizeDeleteOwnedNodePolicy(patch: Record<string, unknown>, path: string): Record<string, unknown> {
  const rawPolicy = patch['delete_policy'];
  const policy = rawPolicy === undefined ? {} : requiredRecord(patch, 'delete_policy', `${path}.delete_policy`);
  const breakLinks = optionalGraphWritePatchBoolean(policy, 'break_links', true, `${path}.delete_policy.break_links`);
  const allowEntryNode = optionalGraphWritePatchBoolean(policy, 'allow_entry_node', false, `${path}.delete_policy.allow_entry_node`);
  const allowLifecycleRoot = optionalGraphWritePatchBoolean(policy, 'allow_lifecycle_root', false, `${path}.delete_policy.allow_lifecycle_root`);

  if (!breakLinks) {
    throwUnsafeDeleteOwnedNodePolicy(`${path}.delete_policy.break_links`, 'delete_owned_node requires delete_policy.break_links=true.');
  }
  if (allowEntryNode) {
    throwUnsafeDeleteOwnedNodePolicy(`${path}.delete_policy.allow_entry_node`, 'delete_owned_node does not allow delete_policy.allow_entry_node=true.');
  }
  if (allowLifecycleRoot) {
    throwUnsafeDeleteOwnedNodePolicy(`${path}.delete_policy.allow_lifecycle_root`, 'delete_owned_node does not allow delete_policy.allow_lifecycle_root=true.');
  }

  return {
    break_links: breakLinks,
    allow_entry_node: allowEntryNode,
    allow_lifecycle_root: allowLifecycleRoot,
  };
}

function optionalGraphWritePatchBoolean(
  record: Record<string, unknown>,
  field: string,
  fallback: boolean,
  path: string,
): boolean {
  const value = record[field];
  if (value === undefined || value === null) {
    return fallback;
  }
  if (typeof value === 'boolean') {
    return value;
  }
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${field} must be a boolean.`, [
    {
      code: 'invalid_graph_write_patch_delete_policy',
      path,
      message: `${field} must be a boolean.`,
    },
  ]);
}

function throwUnsafeDeleteOwnedNodePolicy(path: string, message: string): never {
  throw new TaskSpecCompileError('taskspec_semantic_invalid', message, [
    {
      code: 'owned_delete_policy_disallowed',
      path,
      message,
    },
  ]);
}

function throwMissingPatchValue(path: string, message: string): never {
  throw new TaskSpecCompileError('taskspec_semantic_invalid', message, [
    {
      code: 'missing_patch_payload',
      path: `${path}.value`,
      message: 'Provide value.',
    },
  ]);
}

function normalizeExpectedOldState(record: Record<string, unknown>): Record<string, unknown> {
  const out = literalRecordValues(record);
  if (Object.hasOwn(record, 'value')) {
    out['value'] = patchValueToString(literalValue(record['value']));
  }
  return out;
}

function normalizeMergeAnchor(anchor: Record<string, unknown>, path: string): Record<string, unknown> {
  assertBlockScopedGraphWriteRef(anchor, path);
  getRequiredString(anchor, 'node_ref', `${path}.node_ref`);
  getRequiredString(anchor, 'pin_ref', `${path}.pin_ref`);
  return { ...anchor };
}

function normalizeExternalGraphAnchorBase(anchor: Record<string, unknown>, path: string): Record<string, unknown> {
  const schema = getRequiredString(anchor, 'schema', `${path}.schema`);
  if (schema !== 'BlueprintHelper.ExternalGraphAnchor.v1') {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'External GraphWrite requires BlueprintHelper.ExternalGraphAnchor.v1.', [
      {
        code: 'unsupported_external_graph_anchor',
        path: `${path}.schema`,
        message: 'Use an external_anchor emitted by blueprinthelper_read_context.',
      },
    ]);
  }

  const semanticRole = getRequiredString(anchor, 'semantic_role', `${path}.semantic_role`);
  assertAllowedString(
    semanticRole,
    `${path}.semantic_role`,
    ['exec_boundary', 'node', 'body_entry'],
    'Use exec_boundary, node, or body_entry.',
  );

  const nodeGuid = getRequiredString(anchor, 'node_guid', `${path}.node_guid`);
  if (!/^[0-9a-fA-F]{32}$/u.test(nodeGuid)) {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'External GraphWrite anchor node_guid must be a stable UE GUID.', [
      {
        code: 'unsupported_external_graph_anchor_node_guid',
        path: `${path}.node_guid`,
        message: 'Do not use nodes[index], display names, or JSONPath selectors for external graph writes.',
      },
    ]);
  }

  const out = {
    schema,
    asset_path: getRequiredString(anchor, 'asset_path', `${path}.asset_path`),
    graph_name: getRequiredString(anchor, 'graph_name', `${path}.graph_name`),
    node_guid: nodeGuid,
    node_class: getRequiredString(anchor, 'node_class', `${path}.node_class`),
    semantic_role: semanticRole,
    fingerprint: getRequiredString(anchor, 'fingerprint', `${path}.fingerprint`),
  } as Record<string, unknown>;
  if (typeof anchor['pin_name'] === 'string' && anchor['pin_name'].trim().length > 0) {
    out['pin_name'] = anchor['pin_name'].trim();
  }
  if (typeof anchor['pin_direction'] === 'string' && anchor['pin_direction'].trim().length > 0) {
    const pinDirection = anchor['pin_direction'].trim();
    assertAllowedString(pinDirection, `${path}.pin_direction`, ['input', 'output'], 'Use input or output.');
    out['pin_direction'] = pinDirection;
  }
  return out;
}

function normalizeLogicJsonAnchorSelector(anchor: Record<string, unknown>, path: string): Record<string, unknown> {
  const schema = getRequiredString(anchor, 'schema', `${path}.schema`);
  if (schema !== 'BlueprintHelper.LogicJsonAnchorSelector.v1') {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'External GraphWrite requires BlueprintHelper.ExternalGraphAnchor.v1 or BlueprintHelper.LogicJsonAnchorSelector.v1.', [
      {
        code: 'unsupported_external_graph_anchor',
        path: `${path}.schema`,
        message: 'Use an external_anchor or LogicJson anchor selector emitted by blueprinthelper_read_context.',
      },
    ]);
  }

  const nodeRef = optionalString(anchor, 'node_ref');
  const linkRef = optionalString(anchor, 'link_ref');
  if (Boolean(nodeRef) === Boolean(linkRef)) {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'LogicJson anchor selector requires exactly one of node_ref or link_ref.', [
      {
        code: 'unsupported_logic_json_anchor_selector',
        path,
        message: 'Set exactly one of node_ref or link_ref.',
      },
    ]);
  }
  if (nodeRef && !optionalString(anchor, 'pin_ref')) {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'LogicJson node_ref selector requires pin_ref for merge_external_flow.', [
      {
        code: 'unsupported_logic_json_anchor_selector',
        path: `${path}.pin_ref`,
        message: 'Set pin_ref to identify the exec output boundary on the selected node.',
      },
    ]);
  }
  const graphNameField = optionalString(anchor, 'graph_name');
  const graphAliasField = optionalString(anchor, 'graph');
  if (graphNameField && graphAliasField && graphNameField !== graphAliasField) {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'LogicJson anchor selector graph and graph_name must match when both are provided.', [
      {
        code: 'unsupported_logic_json_anchor_selector',
        path: `${path}.graph`,
        message: 'Remove one field or make graph and graph_name match.',
      },
    ]);
  }
  const graphName = graphNameField ?? graphAliasField;
  if (!graphName) {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'LogicJson anchor selector requires graph_name or graph.', [
      {
        code: 'unsupported_logic_json_anchor_selector',
        path: `${path}.graph_name`,
        message: 'Set graph_name, or pass the graph field from logic_json.',
      },
    ]);
  }

  return omitUndefined({
    schema,
    asset_path: getRequiredString(anchor, 'asset_path', `${path}.asset_path`),
    graph_name: graphName,
    entry_name: optionalString(anchor, 'entry_name'),
    node_ref: nodeRef,
    link_ref: linkRef,
    pin_ref: optionalString(anchor, 'pin_ref'),
  });
}

function normalizeExternalExecBoundaryAnchor(anchor: Record<string, unknown>, path: string): Record<string, unknown> {
  if (anchor['schema'] === 'BlueprintHelper.LogicJsonAnchorSelector.v1') {
    return normalizeLogicJsonAnchorSelector(anchor, path);
  }

  const out = normalizeExternalGraphAnchorBase(anchor, path);
  if (out['semantic_role'] !== 'exec_boundary') {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'merge_external_flow requires an exec_boundary external anchor.', [
      {
        code: 'unsupported_external_graph_anchor_role',
        path: `${path}.semantic_role`,
        message: 'Use semantic_role="exec_boundary".',
      },
    ]);
  }

  const pinDirection = getRequiredString(out, 'pin_direction', `${path}.pin_direction`);
  if (pinDirection !== 'output') {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'merge_external_flow requires an output exec boundary anchor.', [
      {
        code: 'unsupported_external_graph_anchor_pin_direction',
        path: `${path}.pin_direction`,
        message: 'Use pin_direction="output".',
      },
    ]);
  }

  return {
    ...out,
    pin_name: getRequiredString(out, 'pin_name', `${path}.pin_name`),
    pin_direction: pinDirection,
  };
}

function normalizeExternalNodeAnchor(anchor: Record<string, unknown>, path: string, kind: string): Record<string, unknown> {
  const out = normalizeExternalGraphAnchorBase(anchor, path);
  if (out['semantic_role'] !== 'node') {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'patch_external_graph requires a node external anchor.', [
      {
        code: 'unsupported_external_graph_anchor_role',
        path: `${path}.semantic_role`,
        message: 'Use semantic_role="node".',
      },
    ]);
  }
  if (kind === 'set_external_pin_default') {
    out['pin_name'] = getRequiredString(out, 'pin_name', `${path}.pin_name`);
  }
  return out;
}

function normalizeExternalBodyEntryAnchor(anchor: Record<string, unknown>, path: string): Record<string, unknown> {
  const out = normalizeExternalGraphAnchorBase(anchor, path);
  if (out['semantic_role'] !== 'body_entry') {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'replace_external_body requires a body_entry external anchor.', [
      {
        code: 'unsupported_external_graph_anchor_role',
        path: `${path}.semantic_role`,
        message: 'Use semantic_role="body_entry".',
      },
    ]);
  }
  return out;
}

function assertBlockScopedGraphWriteRef(ref: Record<string, unknown>, path: string): void {
  const blockId = ref['block_id'];
  if (typeof blockId === 'string' && isRawLogicJsonArrayRef(blockId)) {
    throwUnsupportedGraphWriteAnchor(
      `${path}.block_id`,
      `${path}.block_id uses a read-view array index. Use a stable BlueprintHelper-owned block_id.`,
    );
  }

  for (const field of ['node_ref', 'pin_ref', 'link_ref']) {
    const value = ref[field];
    if (typeof value === 'string' && isRawLogicJsonArrayRef(value)) {
      throwUnsupportedGraphWriteAnchor(
        `${path}.${field}`,
        `${path}.${field} uses a read-view array index. Use block_id with group-local node_ref/pin_ref/link_ref.`,
      );
    }
  }

  const hasBlockId = typeof ref['block_id'] === 'string' && ref['block_id'].trim().length > 0;
  if (hasBlockId) return;

  throwUnsupportedGraphWriteAnchor(
    path,
    `${path} must identify a BlueprintHelper-owned block with block_id.`,
  );
}

function isRawLogicJsonArrayRef(value: string): boolean {
  return /^(nodes|pins|links)\[\d+\]$/u.test(value.trim());
}

function throwUnsupportedGraphWriteAnchor(path: string, message: string): never {
  throw new TaskSpecCompileError('unsupported_graph_write_anchor', 'GraphWrite patch/merge requires a block-scoped anchor.', [
    {
      code: 'unsupported_graph_write_anchor',
      path,
      message,
    },
  ]);
}

function normalizeMergeInserted(mergeScope: string, inserted: Record<string, unknown>, path: string): Record<string, unknown> {
  const expectedCallKind = mergeScope;
  const callKind = getRequiredString(inserted, 'call_kind', `${path}.call_kind`);
  if (callKind !== expectedCallKind) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'merge inserted.call_kind must match merge scope.', [
      {
        code: 'merge_inserted_scope_mismatch',
        path: `${path}.call_kind`,
        message: `${mergeScope} requires inserted.call_kind="${expectedCallKind}".`,
      },
    ]);
  }
  if (mergeScope === 'function_call') {
    return { function: getRequiredString(inserted, 'name', `${path}.name`) };
  }
  if (mergeScope === 'custom_event_call') {
    return { custom_event: getRequiredString(inserted, 'name', `${path}.name`) };
  }
  return omitUndefined({
    block_id: getRequiredString(inserted, 'block_id', `${path}.block_id`),
    block_ref: typeof inserted['block_ref'] === 'string' && inserted['block_ref'].length > 0 ? inserted['block_ref'] : undefined,
  });
}

function normalizeMergeSequenceOrder(record: Record<string, unknown>, insertStrategy: string, path: string): string[] | undefined {
  const raw = record['sequence_order'];
  if (insertStrategy !== 'branch_fork') {
    if (raw !== undefined) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'sequence_order is only valid for branch_fork.', [
        {
          code: 'sequence_order_not_allowed',
          path,
          message: 'Remove sequence_order unless insert_strategy is branch_fork.',
        },
      ]);
    }
    return undefined;
  }
  if (!Array.isArray(raw) || raw.length === 0) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'branch_fork requires sequence_order.', [
      {
        code: 'sequence_order_required',
        path,
        message: 'Provide sequence_order using inserted_logic and original_successor.',
      },
    ]);
  }
  const sequenceOrder = raw.map((value, index) => {
    if (typeof value === 'string' && (value === 'inserted_logic' || value === 'original_successor')) {
      return value;
    }
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Invalid branch_fork sequence_order entry.', [
      {
        code: 'sequence_order_invalid',
        path: `${path}[${index}]`,
        message: 'Use inserted_logic or original_successor.',
      },
    ]);
  });
  const uniqueEntries = new Set(sequenceOrder);
  if (sequenceOrder.length > 2 || uniqueEntries.size !== sequenceOrder.length) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'branch_fork sequence_order entries must be unique.', [
      {
        code: 'sequence_order_invalid',
        path,
        message: 'Provide each branch_fork sequence_order entry at most once.',
      },
    ]);
  }
  if (!sequenceOrder.includes('inserted_logic')) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'branch_fork sequence_order must include inserted_logic.', [
      {
        code: 'sequence_order_invalid',
        path,
        message: 'Include inserted_logic.',
      },
    ]);
  }
  return sequenceOrder;
}

function patchValueToString(value: unknown): string {
  if (typeof value === 'string') return value;
  if (typeof value === 'number' || typeof value === 'boolean') return String(value);
  if (value === null || value === undefined) return '';
  return JSON.stringify(value);
}

function copyOptionalStringFields(source: Record<string, unknown>, target: Record<string, unknown>, fields: string[]): void {
  fields.forEach((field) => {
    if (typeof source[field] === 'string' && source[field].length > 0) {
      target[field] = source[field];
    }
  });
}

function assertAllowedString(value: string, path: string, allowed: string[], message: string): void {
  if (allowed.includes(value)) return;
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} is not supported.`, [
    {
      code: 'unsupported_field_value',
      path,
      message,
    },
  ]);
}

function assertExactString(
  record: Record<string, unknown>,
  field: string,
  expected: string,
  path: string,
  message: string,
): void {
  const actual = getRequiredString(record, field, path);
  if (actual === expected) return;
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be ${expected}.`, [
    {
      code: 'unsupported_field_value',
      path,
      message,
    },
  ]);
}

function requiredArray(record: Record<string, unknown>, field: string, path: string): unknown[] {
  const value = record[field];
  if (Array.isArray(value) && value.length > 0) return value;
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be a non-empty array.`, [
    {
      code: 'missing_required_array',
      path,
      message: `Provide at least one item in ${path}.`,
    },
  ]);
}

function stringArrayOrEmpty(value: unknown, path: string): string[] {
  if (value === undefined || value === null) return [];
  if (!Array.isArray(value)) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an array.`, [
      {
        code: 'invalid_string_array',
        path,
        message: `${path} must contain path strings.`,
      },
    ]);
  }
  return value.map((item, index) => {
    if (typeof item !== 'string' || item.length === 0) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}[${index}] must be a non-empty string.`, [
        {
          code: 'invalid_string_array_item',
          path: `${path}[${index}]`,
          message: `${path}[${index}] must be a non-empty string.`,
        },
      ]);
    }
    return item;
  });
}

function classSettingsDefaultArray(rawSettings: unknown, path: string): Record<string, unknown>[] {
  if (rawSettings === undefined || rawSettings === null) return [];
  if (!Array.isArray(rawSettings)) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an array.`, [
      {
        code: 'invalid_property_settings',
        path,
        message: 'Use an array of { property_path, value } settings.',
      },
    ]);
  }
  return rawSettings.map((rawSetting, index) => {
    if (!isRecord(rawSetting)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}[${index}] must be an object.`, [
        {
          code: 'invalid_property_setting',
          path: `${path}[${index}]`,
          message: 'Use { property_path, value }.',
        },
      ]);
    }
    const setting = rawSetting as Record<string, unknown>;
    if (!Object.hasOwn(setting, 'value')) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}[${index}].value is required.`, [
        {
          code: 'missing_property_value',
          path: `${path}[${index}].value`,
          message: 'Provide value.',
        },
      ]);
    }
    return {
      ...setting,
      property_path: getRequiredString(setting, 'property_path', `${path}[${index}].property_path`),
      value: literalValue(setting['value']),
    };
  });
}

function literalRecordValues(record: Record<string, unknown>): Record<string, unknown> {
  return Object.fromEntries(
    Object.entries(record).map(([key, value]) => [key, literalValue(value)]),
  );
}

function requiredRecord(record: Record<string, unknown>, field: string, path: string): Record<string, unknown> {
  const value = record[field];
  if (isRecord(value)) return value;
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object.`, [
    {
      code: 'missing_required_object',
      path,
      message: `${path} must be an object.`,
    },
  ]);
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

function compileBlueprintVariableSteps(
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

function omitUndefined(record: Record<string, unknown>): Record<string, unknown> {
  return Object.fromEntries(Object.entries(record).filter(([, value]) => value !== undefined));
}

function compileStatementNode(statement: BlueprintLogicStatement, nodeId: string, path: string): AgentImportNode {
  const statementRecord = statement as Record<string, unknown>;
  const kind = typeof statementRecord.kind === 'string' ? statementRecord.kind : '';
  if (kind === 'call') {
    const functionName = getRequiredString(statementRecord, 'target', `${path}.target`);
    const node: Record<string, unknown> = {
      id: nodeId,
      kind: 'call',
      function: functionName,
      inputs: compileArgs(statement['args']),
    };
    copyContextEvidence(statementRecord, node, `${path}.context_evidence`);
    return node as AgentImportNode;
  }

  if (kind === 'create') {
    const node: Record<string, unknown> = {
      id: nodeId,
      kind: 'create',
      create_operation: getRequiredString(statementRecord, 'create_operation', `${path}.create_operation`),
      target: optionalString(statementRecord, 'target'),
      class_path: optionalString(statementRecord, 'class_path'),
      asset_path: optionalString(statementRecord, 'asset_path'),
      inputs: compileArgs(statement['args']),
    };
    copyStructuredPinTypeFields(statementRecord, node, path);
    copyCreateSemanticFields(statementRecord, node, path);
    copyContextEvidence(statementRecord, node, `${path}.context_evidence`);
    return omitUndefined(node) as AgentImportNode;
  }

  if (kind === 'convert' || kind === 'schedule') {
    const node: Record<string, unknown> = {
      id: nodeId,
      kind,
      target: optionalString(statementRecord, 'target'),
      inputs: compileArgs(statement['args']),
    };
    copyConvertScheduleSemanticFields(statementRecord, node);
    copyContextEvidence(statementRecord, node, `${path}.context_evidence`);
    return omitUndefined(node) as AgentImportNode;
  }

  if (kind === 'set') {
    const variableName = getRequiredString(statementRecord, 'target', `${path}.target`);
    const node = {
      id: nodeId,
      kind: 'field',
      var: variableName,
      target: variableName,
      value: valueExprToString(statement['value']),
    } as AgentImportNode;
    const fieldStatement = FIELD_STATEMENT_KIND_MAP.get(kind);
    if (fieldStatement) {
      applyFieldTaxonomy(node as Record<string, unknown>, fieldStatement.operation, fieldStatement.scope);
    }
    copyContextEvidence(statementRecord, node as Record<string, unknown>, `${path}.context_evidence`);
    return node;
  }

  if (kind === 'set_property') {
    const target = getRequiredString(statementRecord, 'target', `${path}.target`);
    const propertyPath = requiredGraphBodyPropertyPath(statementRecord, path);
    const node = {
      id: nodeId,
      kind: 'field',
      target,
      property_path: propertyPath,
      property: propertyPath,
      value: valueExprToString(statementRecord['value']),
    } as AgentImportNode;
    const fieldStatement = FIELD_STATEMENT_KIND_MAP.get(kind);
    if (fieldStatement) {
      applyFieldTaxonomy(node as Record<string, unknown>, fieldStatement.operation, fieldStatement.scope);
    }
    copyContextEvidence(statementRecord, node as Record<string, unknown>, `${path}.context_evidence`);
    return node;
  }

  if (kind === 'field') {
    const { operation, scope } = fieldOperationScope(statementRecord, path);
    if (operation !== 'set') {
      throw new TaskSpecCompileError('unsupported_field_operation', 'Field statements require field_operation=set.', [
        {
          code: 'unsupported_field_operation',
          path: `${path}.field_operation`,
          message: 'Field statements require field_operation=set.',
        },
      ]);
    }
    const target = getRequiredString(statementRecord, 'target', `${path}.target`);
    const node = {
      id: nodeId,
      kind: 'field',
      var: target,
      target,
      value: valueExprToString(statementRecord['value']),
    } as AgentImportNode;
    if (fieldScopeUsesPropertyPath(scope)) {
      const propertyPath = requiredGraphBodyPropertyPath(statementRecord, path);
      (node as Record<string, unknown>).property_path = propertyPath;
      (node as Record<string, unknown>).property = propertyPath;
    }
    applyFieldTaxonomy(node as Record<string, unknown>, operation, scope);
    copyContextEvidence(statementRecord, node as Record<string, unknown>, `${path}.context_evidence`);
    return node;
  }

  if (kind === CONTAINER_ACTION_KIND) {
    const { containerKind, containerOperation } = validateContainerActionShape(statementRecord, path, 'statement');
    const node: Record<string, unknown> = {
      id: nodeId,
      kind: CONTAINER_ACTION_KIND,
      inputs: {},
    };
    copyContainerActionSemanticFields(statementRecord, node);
    node.container_kind = containerKind;
    node.container_operation = containerOperation;
    copyContextEvidence(statementRecord, node, `${path}.context_evidence`);
    return node as AgentImportNode;
  }

  if (kind === 'control' && isGenericControlKind(getControlStatementKind(statementRecord, path))) {
    const controlKind = getControlStatementKind(statementRecord, path);
    const node: Record<string, unknown> = {
      id: nodeId,
      kind: 'control',
      control: controlKind,
      control_operation: controlKind,
      inputs: compileArgs(statementRecord.args),
    };
    applyGenericControlSemanticFields(statementRecord, node, controlKind, path);
    return omitUndefined(node) as AgentImportNode;
  }

  if (kind === 'component_bound_event') {
    const node: Record<string, unknown> = {
      id: nodeId,
      kind: 'component_bound_event',
      component: getRequiredString(statementRecord, 'component', `${path}.component`),
      delegate: getRequiredString(statementRecord, 'delegate', `${path}.delegate`),
      handler: getRequiredString(statementRecord, 'handler', `${path}.handler`),
    };
    copyContextEvidence(statementRecord, node, `${path}.context_evidence`);
    return omitUndefined(node) as AgentImportNode;
  }

  const delegateOperation = delegateStatementOperation(statementRecord);
  if (delegateOperation) {
    const node: Record<string, unknown> = {
      id: nodeId,
      kind: 'delegate',
      target: getRequiredString(statementRecord, 'target', `${path}.target`),
      delegate: getRequiredString(statementRecord, 'delegate', `${path}.delegate`),
      handler: typeof statementRecord.handler === 'string' ? statementRecord.handler : undefined,
      delegate_operation: delegateOperation,
      unbind_mode: delegateOperation === 'unbind' ? 'single' : (delegateOperation === 'clear' ? 'all' : undefined),
      inputs: delegateOperation === 'call' ? compileArgs(statementRecord.args) : undefined,
    };
    copyContextEvidence(statementRecord, node, `${path}.context_evidence`);
    return omitUndefined(node) as AgentImportNode;
  }

  throw new TaskSpecCompileError('unsupported_statement_kind', `Unsupported statement kind: ${kind}`, [
    {
      code: 'unsupported_statement_kind',
      path: `${path}.kind`,
      message: `Unsupported statement kind: ${kind}`,
    },
  ]);
}

function compileEnsureEntryOpIntoAppendPayload(
  nodes: AgentImportNode[],
  links: AgentImportLink[],
  op: Record<string, unknown>,
  path: string,
  options: LogicCloneOptions = {},
): BlueprintLogicStatement[] {
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

  const flow = compileStatementSequence(body.statements, `${toIdSegment(entryName)}_stmt`, `${path}.body.statements`, makeCompileFlowContext());
  nodes.push(...flow.nodes);
  links.push(...flow.links);
  if (flow.entry) {
    links.push({ kind: 'exec', from: `${entryId}.then`, to: flow.entry });
  }
  return cloneLogicStatementSequenceWithCompiledIds(body.statements, `${toIdSegment(entryName)}_stmt`, options);
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

function optionalString(record: Record<string, unknown>, field: string): string | undefined {
  const value = record[field];
  return typeof value === 'string' && value.trim().length > 0 ? value : undefined;
}

function graphWriteAppendEventKind(record: Record<string, unknown>): GraphWriteAppendEventKind {
  const eventKind = optionalString(record, 'event_kind');
  if (
    eventKind === 'custom_event'
    || eventKind === 'override_event'
    || eventKind === 'component_bound_event'
    || eventKind === 'input_action_event'
    || eventKind === 'dispatcher_event'
  ) {
    return eventKind;
  }
  return 'custom_event';
}

function graphWriteEnsureEntryEventKind(record: Record<string, unknown>): GraphWriteAppendEventKind {
  return graphWriteAppendEventKind(record);
}

function graphWriteCatalogEvidence(value: unknown): GraphWriteCatalogEvidence | undefined {
  if (!isRecord(value)) {
    return undefined;
  }

  const source = optionalString(value, 'source');
  if (source !== 'signature' && source !== 'graph_action_catalog') {
    return undefined;
  }

  return omitUndefined({
    source,
    signature_evidence_id: optionalString(value, 'signature_evidence_id'),
    action_stable_id: optionalString(value, 'action_stable_id'),
    context_fingerprint: optionalString(value, 'context_fingerprint'),
  }) as GraphWriteCatalogEvidence;
}

function getRequiredLogicBody(record: Record<string, unknown>, field: string, path: string): { statements: BlueprintLogicStatement[] } {
  const value = record[field];
  const logicBodySchema = isRecord(value) ? value['schema'] : undefined;
  if (
    isRecord(value)
    && (logicBodySchema === 'BlueprintLogicSpec.v1' || logicBodySchema === 'BlueprintLogicSpec.v2')
    && Array.isArray(value['statements'])
  ) {
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

function compositeComponentParent(component: Record<string, unknown>): unknown {
  const attach = asRecord(component['attach']);
  return typeof attach?.['parent'] === 'string' && attach['parent'].length > 0 ? attach['parent'] : undefined;
}

function compositeComponentSocket(component: Record<string, unknown>): unknown {
  const attach = asRecord(component['attach']);
  return typeof attach?.['socket'] === 'string' && attach['socket'].length > 0 ? attach['socket'] : undefined;
}

function compositeComponentAttachRule(component: Record<string, unknown>): unknown {
  if (typeof component['attach_rule'] === 'string' && component['attach_rule'].length > 0) {
    return component['attach_rule'];
  }
  const attach = asRecord(component['attach']);
  return typeof attach?.['rule'] === 'string' && attach['rule'].length > 0 ? attach['rule'] : undefined;
}

function componentParent(component: Record<string, unknown>): unknown {
  if (typeof component['parent'] === 'string' && component['parent'].length > 0) {
    return component['parent'];
  }
  return compositeComponentParent(component);
}

function componentSocket(component: Record<string, unknown>): unknown {
  if (typeof component['socket'] === 'string' && component['socket'].length > 0) {
    return component['socket'];
  }
  return compositeComponentSocket(component);
}

function componentAttachRule(component: Record<string, unknown>): unknown {
  return compositeComponentAttachRule(component);
}

function normalizeComponentCollisionPolicy(value: unknown): string | undefined {
  if (value === 'reuse_existing') return 'reuse_if_exists';
  if (value === 'reuse_if_type_matches' || value === 'reuse_if_exists') return 'reuse_if_exists';
  if (value === 'fail_if_exists') return 'fail_if_exists';
  if (value === 'block_if_class_mismatch') return 'block_if_class_mismatch';
  return undefined;
}

function optionalComponentPolicyValue<T extends readonly string[]>(
  change: Record<string, unknown>,
  field: string,
  allowedValues: T,
  path: string,
  errorCode: string,
): T[number] | undefined {
  const value = change[field];
  if (value === undefined || value === null) {
    return undefined;
  }
  if (typeof value === 'string' && allowedValues.includes(value as T[number])) {
    return value as T[number];
  }
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} is not supported.`, [
    {
      code: errorCode,
      path,
      message: `Supported values: ${allowedValues.join(', ')}.`,
    },
  ]);
}

function requiredComponentHierarchyParent(
  change: Record<string, unknown>,
  path: string,
  fieldCandidates: readonly string[],
): string {
  for (const field of fieldCandidates) {
    if (typeof change[field] === 'string' && change[field].length > 0) {
      return change[field] as string;
    }
  }

  const fallbackParent = componentParent(change);
  if (typeof fallbackParent === 'string' && fallbackParent.length > 0) {
    return fallbackParent;
  }

  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} is required.`, [
    {
      code: 'parent_component_not_found',
      path,
      message: 'Provide a parent component before lowering hierarchy mutations.',
    },
  ]);
}

function compositeVariablePinType(record: Record<string, unknown>, path: string): Record<string, unknown> {
  if (isRecord(record['pin_type'])) return record['pin_type'];
  if (isRecord(record['variable_type'])) return record['variable_type'];
  if (typeof record['type'] === 'string' && record['type'].trim().length > 0) {
    return { category: record['type'] };
  }
  throwMissingVariableType(`${path}.type`);
}

function propertySettingsArray(
  rawSettings: unknown,
  path: string,
  requireNonEmpty: boolean,
  owner: 'component' | 'property' = 'property',
): Record<string, unknown>[] {
  if (rawSettings === undefined || rawSettings === null) {
    if (requireNonEmpty) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} is required.`, [
        {
          code: owner === 'component' ? 'missing_component_properties' : 'missing_property_settings',
          path,
          message: 'Provide at least one property setting.',
        },
      ]);
    }
    return [];
  }

  const settings = Array.isArray(rawSettings)
    ? rawSettings.map((rawSetting, index) => {
        if (!isRecord(rawSetting)) {
          throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Property setting must be an object.', [
            {
              code: 'invalid_property_setting',
              path: `${path}[${index}]`,
              message: 'Use { "property_path": "...", "value": ... }.',
            },
          ]);
        }
        const setting = rawSetting as Record<string, unknown>;
        if (!Object.hasOwn(setting, 'value')) {
          throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Property setting requires value.', [
            {
              code: 'missing_property_value',
              path: `${path}[${index}].value`,
              message: 'Provide value.',
            },
          ]);
        }
        return {
          property_path: getRequiredString(setting, 'property_path', `${path}[${index}].property_path`),
          value: literalValue(setting['value']),
        };
      })
    : isRecord(rawSettings)
      ? Object.entries(rawSettings).map(([propertyPath, value]) => ({
          property_path: propertyPath,
          value: literalValue(value),
        }))
      : undefined;

  if (!settings) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object or array.`, [
      {
        code: 'invalid_property_settings',
        path,
        message: 'Use an object map or an array of { property_path, value } settings.',
      },
    ]);
  }

  if (requireNonEmpty && settings.length === 0) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must not be empty.`, [
      {
        code: owner === 'component' ? 'missing_component_properties' : 'missing_property_settings',
        path,
        message: 'Provide at least one property setting.',
      },
    ]);
  }

  return settings;
}

function optionalFieldsObject(value: unknown, path: string, requireNonEmpty: boolean): Record<string, unknown> | undefined {
  if (value === undefined || value === null) {
    if (requireNonEmpty) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} is required.`, [
        {
          code: 'missing_data_table_row_fields',
          path,
          message: 'DataTable update rows require a non-empty fields object.',
        },
      ]);
    }
    return undefined;
  }

  if (!isRecord(value) || Object.keys(value).length === 0) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be a non-empty object.`, [
      {
        code: requireNonEmpty ? 'missing_data_table_row_fields' : 'invalid_data_table_row_fields',
        path,
        message: 'Use a non-empty object keyed by row field name.',
      },
    ]);
  }

  return value;
}

function compositeSettingsArray(
  rawSettings: unknown,
  path: string,
  taskSpec: Extract<TaskSpec, { task_type: 'create_blueprint_feature' }>,
): Record<string, unknown>[] {
  if (rawSettings === undefined || rawSettings === null) return [];
  if (Array.isArray(rawSettings)) {
    return rawSettings.map((rawSetting, index) => {
      if (!isRecord(rawSetting)) {
        throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Property setting must be an object.', [
          {
            code: 'invalid_property_setting',
            path: `${path}[${index}]`,
            message: 'Use { "property_path": "...", "value": ... }.',
          },
        ]);
      }
      const setting = rawSetting as Record<string, unknown>;
      if (!Object.hasOwn(setting, 'value')) {
        throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Property setting requires value.', [
          {
            code: 'missing_property_value',
            path: `${path}[${index}].value`,
            message: 'Provide value.',
          },
        ]);
      }
      return {
        ...setting,
        property_path: getRequiredString(setting, 'property_path', `${path}[${index}].property_path`),
        value: resolveCompositeValue(setting['value'], taskSpec),
      };
    });
  }
  if (isRecord(rawSettings)) {
    return Object.entries(rawSettings).map(([propertyPath, value]) => ({
      property_path: propertyPath,
      value: resolveCompositeValue(value, taskSpec),
    }));
  }
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object or array.`, [
    {
      code: 'invalid_property_settings',
      path,
      message: 'Use an object map or a settings array.',
    },
  ]);
}

function resolveCompositeValue(
  value: unknown,
  taskSpec: Extract<TaskSpec, { task_type: 'create_blueprint_feature' }>,
): unknown {
  if (typeof value === 'string') {
    return resolveCompositeReference(value, taskSpec);
  }
  if (Array.isArray(value)) {
    return value.map((item) => resolveCompositeValue(item, taskSpec));
  }
  if (isRecord(value)) {
    if (value['kind'] === 'literal') {
      return literalValue(value);
    }
    return Object.fromEntries(
      Object.entries(value).map(([key, item]) => [key, resolveCompositeValue(item, taskSpec)]),
    );
  }
  return value;
}

function resolveCompositeReference(
  value: string,
  taskSpec: Extract<TaskSpec, { task_type: 'create_blueprint_feature' }>,
): string {
  if (!value.startsWith('$resources.')) return value;
  const resources = asRecord((taskSpec as Record<string, unknown>)['resources']);
  if (!resources) return value;

  const segments = value.slice('$resources.'.length).split('.');
  let cursor: unknown = resources;
  for (const segment of segments) {
    if (!isRecord(cursor) || !(segment in cursor)) return value;
    cursor = cursor[segment];
  }
  if (typeof cursor === 'string') return cursor;
  if (isRecord(cursor) && typeof cursor['asset_path'] === 'string') return cursor['asset_path'];
  return value;
}

function asRecord(value: unknown): Record<string, unknown> | undefined {
  return isRecord(value) ? value : undefined;
}

function isNonEmptyRecord(value: unknown): value is Record<string, unknown> {
  return isRecord(value) && Object.keys(value).length > 0;
}

function toIdSegment(value: string): string {
  const normalized = value.replace(/[^A-Za-z0-9_]/g, '_');
  return normalized.length > 0 ? normalized : 'entry';
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}
