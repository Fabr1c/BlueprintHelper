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

type TaskPlanStep = TaskPlan['steps'][number];

export function compileTaskSpecToTaskPlan(taskSpec: TaskSpec): TaskPlan {
  if (taskSpec.task_type === 'create_blueprint_feature') {
    return compileCompositeBlueprintFeatureTaskSpecToTaskPlan(taskSpec);
  }
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
  const behavior = taskSpec.behavior as Record<string, unknown>;
  const graphWriteOps = compileGraphWriteOps(behavior);

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
    steps: makeGraphWriteTaskPlanSteps(taskSpec, graphWriteOps),
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
  return makeGraphWriteTaskPlanSteps(graphTaskSpec, compileGraphWriteOps(behavior));
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
            preserve_layout: false,
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
          kind: 'call_function',
          name: implementation['call'],
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
  if (strategy === 'append_new_owned_graph') {
    const signatureSteps = graphWriteOps
      .filter((op) => op.op === 'ensure_entry' && op.entry_type === 'custom_event')
      .map((op, index) => ({
        step_id: `step_${String(index + 1).padStart(3, '0')}`,
        capability: 'blueprint_signature' as const,
        target: {
          asset_path: taskSpec.target.asset_path,
        },
        write: {
          strategy: 'custom_event_signature',
          ops: [
            {
              op: 'ensure_custom_event',
              event_name: op.name,
              graph_name: taskSpec.scope_policy.graph_name,
              name_collision_policy: 'reuse_if_exists',
            },
          ],
        },
      } as TaskPlanStep));

    return [
      ...signatureSteps,
      {
        step_id: `step_${String(signatureSteps.length + 1).padStart(3, '0')}`,
        capability: 'graph_write',
        target: {
          asset_path: taskSpec.target.asset_path,
          graph: taskSpec.scope_policy.graph_name,
        },
        write: {
          strategy: 'owned_graph_edit',
          ops: graphWriteOps,
        },
        constraints: {
          allow_modify_user_nodes: taskSpec.scope_policy.allow_modify_user_nodes,
          ownership_scope: 'blueprinthelper_owned',
        },
        ...(signatureSteps.length > 0
          ? { depends_on: signatureSteps.map((step) => step.step_id) }
          : {}),
      } as TaskPlanStep,
    ];
  }

  const opBatches = strategy === 'append_new_owned_graph'
    ? [graphWriteOps]
    : graphWriteOps.map((op) => [op]);

  return opBatches.map((ops, index) => ({
    step_id: `step_${String(index + 1).padStart(3, '0')}`,
    capability: 'graph_write',
    target: {
      asset_path: taskSpec.target.asset_path,
      graph: taskSpec.scope_policy.graph_name,
    },
    write: {
      strategy: 'owned_graph_edit',
      ops,
    },
    constraints: {
      allow_modify_user_nodes: taskSpec.scope_policy.allow_modify_user_nodes,
      ownership_scope: 'blueprinthelper_owned',
    },
  }));
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
  if (!['append_new_owned_graph', 'replace_owned_graph', 'patch_owned_graph', 'merge_owned_graph'].includes(strategy)) {
    throw new TaskSpecCompileError('unsupported_graph_strategy', 'Unsupported GraphWrite graph_strategy.', [
      {
        code: 'unsupported_graph_strategy',
        path: 'behavior.graph_strategy',
        message: 'Use append_new_owned_graph, replace_owned_graph, patch_owned_graph, or merge_owned_graph.',
        suggested_patch: { op: 'replace', path: '/behavior/graph_strategy', value: 'append_new_owned_graph' },
      },
    ]);
  }

  if (taskSpec.scope_policy.allow_modify_user_nodes) {
    throw new TaskSpecCompileError('unsupported_scope_policy', 'Modifying user nodes is not supported for GraphWrite owned strategies.', [
      {
        code: 'unsupported_scope_policy',
        path: 'scope_policy.allow_modify_user_nodes',
        message: 'Set allow_modify_user_nodes=false and target BlueprintHelper-owned graph logic.',
        suggested_patch: { op: 'replace', path: '/scope_policy/allow_modify_user_nodes', value: false },
      },
    ]);
  }

  compileGraphWriteOps(behavior);
}

type GraphWriteCompiledOp = Record<string, unknown> & { op: string };

function compileGraphWriteOps(behavior: Record<string, unknown>): GraphWriteCompiledOp[] {
  const strategy = getRequiredString(behavior, 'graph_strategy', 'behavior.graph_strategy');
  if (strategy === 'append_new_owned_graph') {
    return compileAppendGraphWriteOps(behavior);
  }
  if (strategy === 'replace_owned_graph') {
    return [compileReplaceGraphWriteOp(behavior)];
  }
  if (strategy === 'patch_owned_graph') {
    return compilePatchGraphWriteOps(behavior);
  }
  if (strategy === 'merge_owned_graph') {
    return compileMergeGraphWriteOps(behavior);
  }

  throw new TaskSpecCompileError('unsupported_graph_strategy', 'Unsupported GraphWrite graph_strategy.', [
    {
      code: 'unsupported_graph_strategy',
      path: 'behavior.graph_strategy',
      message: 'Use append_new_owned_graph, replace_owned_graph, patch_owned_graph, or merge_owned_graph.',
      suggested_patch: { op: 'replace', path: '/behavior/graph_strategy', value: 'append_new_owned_graph' },
    },
  ]);
}

function compileAppendGraphWriteOps(behavior: Record<string, unknown>): GraphWriteCompiledOp[] {
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
    return {
      op: 'ensure_entry',
      entry_type: entryType,
      name: getRequiredString(entry, 'name', `behavior.entries[${entryIndex}].name`),
      body: entry['body'],
    };
  });
}

function compileReplaceGraphWriteOp(behavior: Record<string, unknown>): GraphWriteCompiledOp {
  const replace = requiredRecord(behavior, 'replace', 'behavior.replace');
  const body = getRequiredLogicBody(replace, 'body', 'behavior.replace.body');
  validateSupportedStatements(body.statements, 'behavior.replace.body.statements');
  const replacement = compileLogicBodyToImportPayload(body, 'replace', 'behavior.replace.body');
  if (replacement.nodes.length === 0) {
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
    replace_scope: getRequiredString(replace, 'scope', 'behavior.replace.scope'),
    selector: requiredRecord(replace, 'selector', 'behavior.replace.selector'),
    replacement,
    options: isRecord(replace['options']) ? replace['options'] : undefined,
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
    if (!['set_pin_default', 'set_node_comment', 'set_node_position'].includes(kind)) {
      throw new TaskSpecCompileError('unsupported_graph_write_patch', `Unsupported GraphWrite patch kind: ${kind}`, [
        {
          code: 'unsupported_graph_write_patch',
          path: `${path}.kind`,
          message: 'Use set_pin_default, set_node_comment, or set_node_position.',
        },
      ]);
    }

    return omitUndefined({
      op: kind,
      patch_scope: typeof patch['scope'] === 'string' && patch['scope'].length > 0
        ? patch['scope']
        : defaultPatchScope(kind),
      patched_ref: requiredRecord(patch, 'target_ref', `${path}.target_ref`),
      patch: compilePatchPayload(patch, path),
      expected_old_state: isRecord(patch['expected_old_state'])
        ? literalRecordValues(patch['expected_old_state'])
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

    return omitUndefined({
      op: 'insert_flow',
      merge_scope: getRequiredString(merge, 'scope', `${path}.scope`),
      insert_strategy: getRequiredString(merge, 'insert_strategy', `${path}.insert_strategy`),
      anchor: requiredRecord(merge, 'anchor', `${path}.anchor`),
      inserted: requiredRecord(merge, 'inserted', `${path}.inserted`),
      sequence_order: Array.isArray(merge['sequence_order']) ? merge['sequence_order'] : undefined,
    }) as GraphWriteCompiledOp;
  });
}

function validateSupportedStatements(statements: BlueprintLogicStatement[], path: string): void {
  statements.forEach((statement, statementIndex) => {
    if (statement.kind !== 'call_function' && statement.kind !== 'set_member_variable') {
      throw new TaskSpecCompileError('unsupported_statement_kind', 'Only call_function and set_member_variable statements are supported in this GraphWrite slice.', [
        {
          code: 'unsupported_statement_kind',
          path: `${path}[${statementIndex}].kind`,
          message: 'Use call_function or set_member_variable, or split this work into a later GraphWrite capability.',
        },
      ]);
    }
  });
}

function compileLogicBodyToImportPayload(
  body: { statements: BlueprintLogicStatement[] },
  prefix: string,
  path: string,
): { nodes: AgentImportNode[]; links: AgentImportLink[] } {
  const nodes: AgentImportNode[] = [];
  const links: AgentImportLink[] = [];
  let previousNodeId: string | undefined;
  body.statements.forEach((statement, statementIndex) => {
    const nodeId = `${toIdSegment(prefix)}_stmt_${statementIndex + 1}`;
    const node = compileStatementNode(statement, nodeId, `${path}.statements[${statementIndex}]`);
    nodes.push(node);
    if (previousNodeId) {
      links.push({ kind: 'exec', from: `${previousNodeId}.then`, to: `${nodeId}.execute` });
    }
    previousNodeId = nodeId;
  });
  return { nodes, links };
}

function defaultPatchScope(kind: string): string {
  if (kind === 'set_node_comment') return 'node_comment';
  if (kind === 'set_node_position') return 'node_position';
  return 'pin_default';
}

function compilePatchPayload(patch: Record<string, unknown>, path: string): Record<string, unknown> {
  if (isRecord(patch['patch'])) {
    return literalRecordValues(patch['patch']);
  }
  if ('value' in patch) {
    return {
      value: literalValue(patch['value']),
    };
  }
  throw new TaskSpecCompileError('taskspec_semantic_invalid', 'GraphWrite patch requires patch or value.', [
    {
      code: 'missing_patch_payload',
      path: `${path}.patch`,
      message: 'Provide patch or value.',
    },
  ]);
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

function compositeComponentParent(component: Record<string, unknown>): unknown {
  if (typeof component['attach_to'] === 'string' && component['attach_to'].length > 0) {
    return component['attach_to'];
  }
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

function normalizeComponentCollisionPolicy(value: unknown): string | undefined {
  if (value === 'reuse_if_type_matches' || value === 'reuse_if_exists') return 'reuse_if_exists';
  if (value === 'fail_if_exists') return 'fail_if_exists';
  return undefined;
}

function compositeVariablePinType(record: Record<string, unknown>, path: string): Record<string, unknown> {
  if (isRecord(record['pin_type'])) return record['pin_type'];
  if (isRecord(record['variable_type'])) return record['variable_type'];
  if (typeof record['type'] === 'string' && record['type'].trim().length > 0) {
    return { category: record['type'] };
  }
  throwMissingVariableType(`${path}.type`);
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
