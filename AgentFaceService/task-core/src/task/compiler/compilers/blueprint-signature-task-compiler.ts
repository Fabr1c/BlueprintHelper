import type { TaskPlan, TaskSpec } from '../../schema/task-schemas.js';
import {
  assertExactString,
  getRequiredString,
  isRecord,
  makeCompositeCapabilityStep,
  makeTaskPlanWithSteps,
  omitUndefined,
  optionalString,
  requiredArray,
  requireStructuredPinType,
} from '../compiler-helpers.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';
import type { TaskTypeCompiler } from '../task-type-compiler.js';

export const blueprintSignatureTaskCompiler: TaskTypeCompiler<Extract<TaskSpec, { task_type: 'edit_blueprint_signature' }>> = {
  id: 'blueprint_signature',
  taskType: 'edit_blueprint_signature',
  canCompile(taskSpec): taskSpec is Extract<TaskSpec, { task_type: 'edit_blueprint_signature' }> {
    return taskSpec.task_type === 'edit_blueprint_signature';
  },
  compile(taskSpec) {
    return compileBlueprintSignatureTaskSpecToTaskPlan(taskSpec);
  },
};

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
