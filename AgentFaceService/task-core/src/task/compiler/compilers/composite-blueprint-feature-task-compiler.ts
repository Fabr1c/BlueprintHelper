import type { AgentImportLink, AgentImportNode, BlueprintLogicStatement, TaskPlan, TaskSpec } from '../../schema/task-schemas.js';
import { buildTaskPlan } from '../task-plan-builder.js';
import type { TaskTypeCompiler } from '../task-type-compiler.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';
import { getRequiredString, literalValue, omitUndefined, requiredRecord, asRecord, isRecord } from '../compiler-helpers.js';
import {
  assertSupportedTaskSpec,
  compileLogicBodyToImportPayload,
  defaultFieldOwnerClassForBlueprintAsset,
  getRequiredLogicBody,
  makeGraphWriteTaskPlanSteps,
  validateSupportedStatements,
} from '../graphwrite/graphwrite-logic-body-compiler.js';
import { compileGraphWriteOps } from '../graphwrite/default-graphwrite-operation-compilers.js';

type TaskPlanStep = TaskPlan['steps'][number];

export const compositeBlueprintFeatureTaskCompiler: TaskTypeCompiler<Extract<TaskSpec, { task_type: 'create_blueprint_feature' }>> = {
  id: 'composite_feature',
  taskType: 'create_blueprint_feature',
  canCompile(taskSpec): taskSpec is Extract<TaskSpec, { task_type: 'create_blueprint_feature' }> {
    return taskSpec.task_type === 'create_blueprint_feature';
  },
  compile(taskSpec): TaskPlan {
    return compileCompositeBlueprintFeatureTaskSpecToTaskPlan(taskSpec);
  },
};

export function compileCompositeBlueprintFeatureTaskSpecToTaskPlan(
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

  return buildTaskPlan({ taskSpec, steps });
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

function normalizeComponentCollisionPolicy(value: unknown): string | undefined {
  if (value === 'reuse_existing') return 'reuse_if_exists';
  if (value === 'reuse_if_type_matches' || value === 'reuse_if_exists') return 'reuse_if_exists';
  if (value === 'fail_if_exists') return 'fail_if_exists';
  if (value === 'block_if_class_mismatch') return 'block_if_class_mismatch';
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

function throwMissingVariableType(path: string): never {
  throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Blueprint variable type is required.', [
    {
      code: 'missing_variable_type',
      path,
      message: 'Provide pin_type, variable_type, or type.',
    },
  ]);
}
