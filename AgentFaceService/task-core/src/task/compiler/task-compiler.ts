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
import { TASK_PLAN_SCHEMA } from '../schema/task-schemas.js';

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
  if (taskSpec.task_type === 'edit_object_properties') {
    return compileObjectPropertiesTaskSpecToTaskPlan(taskSpec);
  }
  if (taskSpec.task_type === 'manage_blueprinthelper_ownership') {
    return compileGraphCleanupOwnershipTaskSpecToTaskPlan(taskSpec);
  }
  if (taskSpec.task_type === 'edit_blueprint_signature') {
    return compileBlueprintSignatureTaskSpecToTaskPlan(taskSpec);
  }
  if (taskSpec.task_type !== 'edit_blueprint_graph') {
    throw new TaskSpecCompileError('unsupported_task_type', `Unsupported TaskSpec task_type: ${taskSpec.task_type}`, [
      {
        code: 'unsupported_task_type',
        path: 'task_type',
        message: 'This TypeScript fallback compiler currently supports GraphWrite, Blueprint Variables, Signature, ObjectProperty, Cleanup/Ownership, and composite feature slices; other capability compilation is owned by the Python compiler.',
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
    return graphWriteOps.map((op, index) => ({
      step_id: `step_${String(index + 1).padStart(3, '0')}`,
      capability: 'graph_write',
      target: {
        asset_path: taskSpec.target.asset_path,
        graph: taskSpec.scope_policy.graph_name,
      },
      write: {
        strategy: 'owned_graph_edit',
        ops: [stripGraphWriteCompilerMetadata(op)],
      },
      constraints: {
        allow_modify_user_nodes: taskSpec.scope_policy.allow_modify_user_nodes,
        ownership_scope: 'blueprinthelper_owned',
      },
    } as TaskPlanStep));
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
            graph: taskSpec.scope_policy.graph_name,
          },
          write: {
            strategy: 'owned_graph_edit',
            ops: [stripGraphWriteCompilerMetadata(op)],
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

  const opBatches = strategy === 'append_new_owned_graph'
    ? [graphWriteOps]
    : graphWriteOps.map((op) => [stripGraphWriteCompilerMetadata(op)]);

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

function stripGraphWriteCompilerMetadata(op: GraphWriteCompiledOp): GraphWriteCompiledOp {
  const { __signature_split: _signatureSplit, ...cleanOp } = op as GraphWriteCompiledOp & { __signature_split?: unknown };
  return cleanOp as GraphWriteCompiledOp;
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

function compileGraphCleanupOwnershipTaskSpecToTaskPlan(
  taskSpec: Extract<TaskSpec, { task_type: 'manage_blueprinthelper_ownership' }>,
): TaskPlan {
  const behavior = taskSpec.behavior as Record<string, unknown>;
  assertExactString(
    behavior,
    'ownership_strategy',
    'owned_block_lifecycle',
    'behavior.ownership_strategy',
    'Use ownership_strategy="owned_block_lifecycle".',
  );

  const changes = requiredArray(behavior, 'changes', 'behavior.changes');
  const steps = changes.map((rawChange, index) => {
    const path = `behavior.changes[${index}]`;
    if (!isRecord(rawChange)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object.`, [
        { code: 'invalid_ownership_change', path, message: 'Use an owned block lifecycle change object.' },
      ]);
    }
    const change = rawChange as Record<string, unknown>;
    const op = compileGraphCleanupOwnershipOp(change, path);
    return makeCompositeCapabilityStep(
      index + 1,
      'graph_cleanup_ownership',
      taskSpec.target.asset_path,
      'owned_block_lifecycle',
      [op],
    );
  });

  return makeTaskPlanWithSteps(taskSpec, steps);
}

function compileGraphCleanupOwnershipOp(change: Record<string, unknown>, path: string): Record<string, unknown> {
  const kind = getRequiredString(change, 'kind', `${path}.kind`);
  const opByKind: Record<string, string> = {
    cleanup_block: 'cleanup_blueprint_helper_block',
    cleanup_blueprint_helper_block: 'cleanup_blueprint_helper_block',
    convert_block_to_user_owned: 'convert_blueprint_helper_block_to_user_owned',
    convert_blueprint_helper_block_to_user_owned: 'convert_blueprint_helper_block_to_user_owned',
    rollback_cleanup_transaction: 'rollback_cleanup_transaction',
  };
  const opName = opByKind[kind];
  if (!opName) {
    throw new TaskSpecCompileError('unsupported_ownership_op', `Unsupported ownership change kind: ${kind}.`, [
      {
        code: 'unsupported_ownership_op',
        path: `${path}.kind`,
        message: 'Use cleanup_block, convert_block_to_user_owned, or rollback_cleanup_transaction.',
      },
    ]);
  }

  if (opName === 'rollback_cleanup_transaction') {
    return omitUndefined({
      op: opName,
      transaction_id: getRequiredString(change, 'transaction_id', `${path}.transaction_id`),
      asset_path: change['asset_path'],
      rollback_scope: change['rollback_scope'] ?? 'cleanup_transaction',
      already_rolled_back_policy: change['already_rolled_back_policy'],
    });
  }

  const blockId = typeof change['block_id'] === 'string' ? change['block_id'] : undefined;
  const graphId = typeof change['graph_id'] === 'string' ? change['graph_id'] : undefined;
  const blockRef = typeof change['block_ref'] === 'string' ? change['block_ref'] : undefined;
  if (!blockId && !(graphId && blockRef)) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Owned block operation requires block_id or graph_id + block_ref.', [
      {
        code: 'missing_owned_block_ref',
        path,
        message: 'Provide block_id or graph_id + block_ref.',
      },
    ]);
  }

  return omitUndefined({
    op: opName,
    graph: change['graph_name'],
    graph_id: graphId,
    block_ref: blockRef,
    block_id: blockId,
    missing_policy: change['missing_policy'],
    already_user_owned_policy: change['already_user_owned_policy'],
  });
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
    const op = omitUndefined({
      op: 'ensure_function',
      function_name: getRequiredString(change, 'function_name', `${path}.function_name`),
      interface_path: kind === 'ensure_interface_function'
        ? getRequiredString(change, 'interface_path', `${path}.interface_path`)
        : optionalString(change, 'interface_path'),
      interface_entry_kind: kind === 'ensure_interface_function' ? 'function' : undefined,
      inputs: change['inputs'],
      outputs: change['outputs'],
      is_pure: change['is_pure'],
      name_collision_policy: optionalString(change, 'name_collision_policy') ?? 'reuse_if_exists',
    });
    return op;
  }

  if (kind === 'ensure_custom_event' || kind === 'ensure_interface_event') {
    return omitUndefined({
      op: 'ensure_custom_event',
      event_name: getRequiredString(change, 'event_name', `${path}.event_name`),
      graph_name: getRequiredString(change, 'graph_name', `${path}.graph_name`),
      interface_path: kind === 'ensure_interface_event'
        ? getRequiredString(change, 'interface_path', `${path}.interface_path`)
        : optionalString(change, 'interface_path'),
      interface_entry_kind: kind === 'ensure_interface_event' ? 'event' : undefined,
      inputs: change['inputs'],
      name_collision_policy: optionalString(change, 'name_collision_policy') ?? 'reuse_if_exists',
    });
  }

  if (kind === 'ensure_event_dispatcher') {
    return omitUndefined({
      op: 'ensure_event_dispatcher',
      dispatcher_name: getRequiredString(change, 'dispatcher_name', `${path}.dispatcher_name`),
      inputs: change['inputs'],
      name_collision_policy: optionalString(change, 'name_collision_policy') ?? 'reuse_if_exists',
      signature_mismatch_policy: optionalString(change, 'signature_mismatch_policy') ?? 'block',
    });
  }

  if (kind === 'ensure_override_event') {
    return omitUndefined({
      op: 'ensure_override_event',
      event_name: getRequiredString(change, 'event_name', `${path}.event_name`),
      event_kind: optionalString(change, 'event_kind') ?? 'native_event',
      graph_name: optionalString(change, 'graph_name'),
      inputs: change['inputs'],
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

    logicStatements.push(...compileEnsureEntryOpIntoAppendPayload(nodes, links, rawOp as Record<string, unknown>, `steps[0].write.ops[${opIndex}]`));
    if (!logicEntry && isRecord(rawOp) && rawOp.entry_type === 'custom_event' && typeof rawOp.name === 'string') {
      logicEntry = {
        kind: 'custom_event',
        name: rawOp.name,
        id: `${toIdSegment(rawOp.name)}_entry`,
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
type GraphWriteSignatureSplit = {
  op: 'ensure_custom_event';
  event_name: string;
  inputs?: unknown;
  name_collision_policy: string;
};

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
  const replaceScope = getRequiredString(replace, 'scope', 'behavior.replace.scope');
  assertAllowedString(
    replaceScope,
    'behavior.replace.scope',
    ['custom_event_definition', 'custom_event_body', 'function_body', 'event_body', 'block_implementation'],
    'Use custom_event_definition, custom_event_body, function_body, event_body, or block_implementation.',
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
    logic_spec: compileLogicBodyToSemanticLogicSpec(body, 'replace'),
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
    if (!['set_pin_default', 'set_node_comment', 'set_node_position'].includes(kind)) {
      throw new TaskSpecCompileError('unsupported_graph_write_patch', `Unsupported GraphWrite patch kind: ${kind}`, [
        {
          code: 'unsupported_graph_write_patch',
          path: `${path}.kind`,
          message: 'Use set_pin_default, set_node_comment, or set_node_position.',
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

    return omitUndefined({
      op: kind,
      patch_scope: patchScope,
      patched_ref: normalizePatchTargetRef(kind, requiredRecord(patch, 'target_ref', `${path}.target_ref`), `${path}.target_ref`),
      patch: compilePatchPayload(kind, patch, path),
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

function validateSupportedStatements(statements: BlueprintLogicStatement[], path: string): void {
  statements.forEach((statement, statementIndex) => {
    if (!['call', 'set', 'branch', 'let', 'return', 'call_function', 'set_member_variable'].includes(statement.kind)) {
      throw new TaskSpecCompileError('unsupported_statement_kind', 'Only call and set statements are supported in this GraphWrite slice.', [
        {
          code: 'unsupported_statement_kind',
          path: `${path}[${statementIndex}].kind`,
          message: 'Use call, set, branch, let, or return, or split this work into a later GraphWrite capability.',
        },
      ]);
    }
    if (statement.kind === 'branch') {
      const branchStatement = statement as BlueprintLogicStatement & { then?: unknown; else?: unknown };
      const thenStatements = Array.isArray(branchStatement.then)
        ? (branchStatement.then as BlueprintLogicStatement[])
        : [];
      const elseStatements = Array.isArray(branchStatement.else)
        ? (branchStatement.else as BlueprintLogicStatement[])
        : [];
      validateSupportedStatements(thenStatements, `${path}[${statementIndex}].then`);
      validateSupportedStatements(elseStatements, `${path}[${statementIndex}].else`);
    }
  });
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
): Record<string, unknown> {
  return {
    schema: 'BlueprintLogicSpec.v2',
    statements: cloneLogicStatementSequenceWithCompiledIds(body.statements, `${toIdSegment(prefix)}_stmt`),
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

function makeCompileFlowContext(parent?: CompileFlowContext): CompileFlowContext {
  return {
    symbols: new Map(parent ? parent.symbols : []),
  };
}

function cloneLogicExpressionWithCompiledIds(expression: unknown, nodeId: string): unknown {
  if (!isRecord(expression)) {
    return expression;
  }

  const kind = typeof expression.kind === 'string' ? expression.kind : 'literal';
  const out: Record<string, unknown> = { ...expression, id: nodeId };

  if (kind === 'compare') {
    out.left = cloneLogicExpressionWithCompiledIds(expression.left, `${nodeId}_left`);
    out.right = cloneLogicExpressionWithCompiledIds(expression.right, `${nodeId}_right`);
  } else if (kind === 'select') {
    out.condition = cloneLogicExpressionWithCompiledIds(expression.condition, `${nodeId}_index`);
    if (Array.isArray(expression.options)) {
      out.options = expression.options.map((option, index) => cloneLogicExpressionWithCompiledIds(option, `${nodeId}_option_${index}`));
    }
  } else if (kind === 'make_struct' && isRecord(expression.args)) {
    out.args = Object.fromEntries(
      Object.entries(expression.args).map(([argName, argValue]) => [
        argName,
        cloneLogicExpressionWithCompiledIds(argValue, `${nodeId}_${toIdSegment(argName)}`),
      ]),
    );
  } else if (isRecord(expression.args)) {
    out.args = Object.fromEntries(
      Object.entries(expression.args).map(([argName, argValue]) => [
        argName,
        cloneLogicExpressionWithCompiledIds(argValue, `${nodeId}_${toIdSegment(argName)}`),
      ]),
    );
  }

  return out;
}

function cloneLogicStatementWithCompiledIds(statement: BlueprintLogicStatement, statementId: string): BlueprintLogicStatement {
  const out: Record<string, unknown> = { ...(statement as Record<string, unknown>), id: statementId };

  if (statement.kind === 'call_function') {
    out.kind = 'call';
    out.target = (statement as Record<string, unknown>).name;
    delete out.name;
  } else if (statement.kind === 'set_member_variable') {
    out.kind = 'set';
    out.target = (statement as Record<string, unknown>).name;
    delete out.name;
  }

  if (statement.kind === 'branch') {
    out.condition = cloneLogicExpressionWithCompiledIds((statement as Record<string, unknown>).condition, `${statementId}_condition`);
    const thenStatements = Array.isArray((statement as Record<string, unknown>).then)
      ? ((statement as Record<string, unknown>).then as BlueprintLogicStatement[])
      : [];
    const elseStatements = Array.isArray((statement as Record<string, unknown>).else)
      ? ((statement as Record<string, unknown>).else as BlueprintLogicStatement[])
      : [];
    out.then = cloneLogicStatementSequenceWithCompiledIds(thenStatements, `${statementId}_then`);
    out.else = cloneLogicStatementSequenceWithCompiledIds(elseStatements, `${statementId}_else`);
  } else if (statement.kind === 'let') {
    out.value = cloneLogicExpressionWithCompiledIds((statement as Record<string, unknown>).value, `${statementId}_value`);
  } else if (statement.kind === 'set' || statement.kind === 'set_member_variable') {
    out.value = cloneLogicExpressionWithCompiledIds((statement as Record<string, unknown>).value, `${statementId}_value`);
  } else if ((statement.kind === 'call' || statement.kind === 'call_function') && isRecord((statement as Record<string, unknown>).args)) {
    const args = (statement as Record<string, unknown>).args as Record<string, unknown>;
    out.args = Object.fromEntries(
      Object.entries(args).map(([argName, argValue]) => [
        argName,
        cloneLogicExpressionWithCompiledIds(argValue, `${statementId}_arg_${toIdSegment(argName)}`),
      ]),
    );
  }

  return out as BlueprintLogicStatement;
}

function cloneLogicStatementSequenceWithCompiledIds(statements: BlueprintLogicStatement[], idPrefix: string): BlueprintLogicStatement[] {
  return statements.map((statement, statementIndex) => cloneLogicStatementWithCompiledIds(statement, `${idPrefix}_${statementIndex + 1}`));
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

function compileStatementFlow(statement: BlueprintLogicStatement, nodeId: string, path: string, context: CompileFlowContext): CompiledStatementFlow {
  if (statement.kind === 'branch') {
    return compileBranchStatementFlow(statement, nodeId, path, context);
  }
  if (statement.kind === 'let') {
    const name = getRequiredString(statement, 'name', `${path}.name`);
    const valueFlow = compileValueExpression(statement['value'], `${nodeId}_value`, `${path}.value`, context);
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
  if (statement.kind === 'return') {
    return {
      nodes: [],
      links: [],
      exits: [],
    };
  }

  const node = compileStatementNode(statement, nodeId, path);
  const nodes: AgentImportNode[] = [node];
  const links: AgentImportLink[] = [];
  if (statement.kind === 'call' || statement.kind === 'call_function') {
    const inputValues: Record<string, unknown> = {};
    if (isRecord(statement['args'])) {
      for (const [argName, argValue] of Object.entries(statement['args'])) {
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
  if (statement.kind === 'set' || statement.kind === 'set_member_variable') {
    const variableName = statement.kind === 'set'
      ? getRequiredString(statement, 'target', `${path}.target`)
      : getRequiredString(statement, 'name', `${path}.name`);
    const valueFlow = compileValueExpression(statement['value'], `${nodeId}_value`, `${path}.value`, context);
    nodes.push(...valueFlow.nodes);
    links.push(...valueFlow.links);
    if (valueFlow.output) {
      links.push({ kind: 'data', from: valueFlow.output, to: `${nodeId}.${variableName}` });
      delete node.value;
    } else {
      node.value = valueExprToString(valueFlow.defaultValue);
    }
  }
  return {
    nodes,
    links,
    entry: `${nodeId}.execute`,
    exits: [`${nodeId}.then`],
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

  if (kind === 'ref') {
    const symbolName = typeof expression.name === 'string'
      ? expression.name
      : typeof expression.target === 'string'
        ? expression.target
        : undefined;
    if (!symbolName) {
      throw new TaskSpecCompileError('invalid_ref_expression', 'ref expression requires name or target.', [
        { code: 'invalid_ref_expression', path, message: 'Provide ref.name or ref.target.' },
      ]);
    }
    const symbol = context.symbols.get(symbolName.toLowerCase());
    if (!symbol) {
      throw new TaskSpecCompileError('ref_symbol_not_found', `Temporary symbol not found: ${symbolName}.`, [
        { code: 'ref_symbol_not_found', path, message: `Define let.name="${symbolName}" before this ref.` },
      ]);
    }
    return { nodes: [], links: [], output: symbol.output, defaultValue: symbol.defaultValue };
  }

  if (kind === 'get' || kind === 'get_property') {
    const target = typeof expression.target === 'string'
      ? expression.target
      : typeof expression.name === 'string'
        ? expression.name
        : typeof expression.var === 'string'
          ? expression.var
          : undefined;
    if (!target) {
      throw new TaskSpecCompileError('invalid_get_expression', 'get expression requires target, name, or var.', [
        { code: 'invalid_get_expression', path, message: 'Provide get.target, get.name, or get.var.' },
      ]);
    }
    const outputPin = kind === 'get_property' ? 'value' : target;
    return {
      nodes: [{ id: nodeId, kind, var: target, target }],
      links: [],
      output: `${nodeId}.${outputPin}`,
    };
  }

  const nodes: AgentImportNode[] = [];
  const links: AgentImportLink[] = [];
  const node: AgentImportNode = {
    id: nodeId,
    kind,
    inputs: {},
  };
  if (kind === 'call') {
    node.function = getRequiredString(expression, 'target', `${path}.target`);
  }
  if (kind === 'compare') {
    node.function = typeof expression.op === 'string' ? expression.op : undefined;
    compileExpressionInput(expression['left'], 'A', `${nodeId}_left`, `${path}.left`, node, nodes, links, context);
    compileExpressionInput(expression['right'], 'B', `${nodeId}_right`, `${path}.right`, node, nodes, links, context);
  } else if (kind === 'select') {
    if (Array.isArray(expression.options)) {
      expression.options.forEach((option, index) => {
        compileExpressionInput(option, `Option${index}`, `${nodeId}_option_${index}`, `${path}.options[${index}]`, node, nodes, links, context);
      });
    }
    compileExpressionInput(expression['condition'], 'Index', `${nodeId}_index`, `${path}.condition`, node, nodes, links, context);
  } else if (kind === 'make_struct') {
    const structType = typeof expression.type === 'string' ? expression.type : undefined;
    (node as Record<string, unknown>).type = structType;
    (node as Record<string, unknown>).struct_path = structType;
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

  const outputPin = kind === 'make_struct' || kind === 'select' ? 'value' : 'ReturnValue';
  return { nodes, links, output: `${nodeId}.${outputPin}` };
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
  if (kind === 'set_node_position') return 'node_position';
  return 'pin_default';
}

function normalizeReplaceSelector(
  replaceScope: string,
  selector: Record<string, unknown>,
): Record<string, unknown> {
  const kind = getRequiredString(selector, 'kind', 'behavior.replace.selector.kind');
  const out: Record<string, unknown> = {};
  copyOptionalStringFields(selector, out, ['graph_id', 'node_ref', 'node_path']);

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
  if (kind === 'set_pin_default') {
    getRequiredString(targetRef, 'pin_ref', `${path}.pin_ref`);
  }
  return out;
}

function compilePatchPayload(kind: string, patch: Record<string, unknown>, path: string): Record<string, unknown> {
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
  if (kind === 'set_node_position') {
    const payload = requiredRecord(patch, 'patch', `${path}.patch`);
    if (typeof payload['x'] !== 'number' && typeof payload['y'] !== 'number') {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'set_node_position requires patch.x or patch.y.', [
        {
          code: 'missing_node_position',
          path: `${path}.patch`,
          message: 'Provide patch.x and/or patch.y as numbers.',
        },
      ]);
    }
    return literalRecordValues(payload);
  }

  throw new TaskSpecCompileError('unsupported_graph_write_patch', `Unsupported GraphWrite patch kind: ${kind}`, [
    {
      code: 'unsupported_graph_write_patch',
      path: `${path}.kind`,
      message: 'Use set_pin_default, set_node_comment, or set_node_position.',
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

function assertBlockScopedGraphWriteRef(ref: Record<string, unknown>, path: string): void {
  const hasBlockId = typeof ref['block_id'] === 'string' && ref['block_id'].trim().length > 0;
  if (hasBlockId) return;

  for (const field of ['node_ref', 'pin_ref', 'link_ref']) {
    const value = ref[field];
    if (typeof value === 'string' && isRawLogicJsonArrayRef(value)) {
      throwUnsupportedGraphWriteAnchor(
        `${path}.${field}`,
        `${path}.${field} uses a read-view array index. Use block_id with group-local node_ref/pin_ref/link_ref.`,
      );
    }
  }

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
  if (statement.kind === 'call' || statement.kind === 'call_function') {
    const functionName = statement.kind === 'call'
      ? getRequiredString(statement, 'target', `${path}.target`)
      : getRequiredString(statement, 'name', `${path}.name`);
    return {
      id: nodeId,
      kind: 'call',
      function: functionName,
      inputs: compileArgs(statement['args']),
    };
  }

  if (statement.kind === 'set' || statement.kind === 'set_member_variable') {
    const variableName = statement.kind === 'set'
      ? getRequiredString(statement, 'target', `${path}.target`)
      : getRequiredString(statement, 'name', `${path}.name`);
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
  return cloneLogicStatementSequenceWithCompiledIds(body.statements, `${toIdSegment(entryName)}_stmt`);
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
