import type { TaskPlan, TaskSpec } from '../../schema/task-schemas.js';
import {
  assertExactString,
  componentAttachRule,
  componentParent,
  componentSocket,
  getRequiredString,
  isRecord,
  makeCompositeCapabilityStep,
  makeTaskPlanWithSteps,
  normalizeComponentCollisionPolicy,
  omitUndefined,
  optionalComponentPolicyValue,
  propertySettingsArray,
  requiredArray,
  requiredComponentHierarchyParent,
  type TaskPlanStep,
} from '../compiler-helpers.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';
import type { TaskTypeCompiler } from '../task-type-compiler.js';

const COMPONENT_TRANSFORM_POLICIES = ['preserve_world', 'preserve_relative', 'reset_relative'] as const;
const COMPONENT_OLD_ROOT_POLICIES = ['keep_as_child', 'remove_default_scene_root_when_empty'] as const;
const COMPONENT_DEFAULT_ROOT_POLICIES = ['require_scene_component', 'create_default_scene_root_when_needed'] as const;
const COMPONENT_DELETE_POLICIES = [
  'block_if_children',
  'promote_children',
  'delete_owned_children',
  'reattach_children_to_parent',
] as const;

export const blueprintComponentsTaskCompiler: TaskTypeCompiler<Extract<TaskSpec, { task_type: 'edit_blueprint_components' }>> = {
  id: 'blueprint_components',
  taskType: 'edit_blueprint_components',
  canCompile(taskSpec): taskSpec is Extract<TaskSpec, { task_type: 'edit_blueprint_components' }> {
    return taskSpec.task_type === 'edit_blueprint_components';
  },
  compile(taskSpec) {
    return compileBlueprintComponentsTaskSpecToTaskPlan(taskSpec);
  },
};

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
