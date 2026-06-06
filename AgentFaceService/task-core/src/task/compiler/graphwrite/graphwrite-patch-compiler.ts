import type { GraphWriteCompiledOp } from './graphwrite-logic-body-compiler.js';
import {
  compilePatchPayload,
  defaultPatchScope,
  getRequiredString,
  isOwnedGraphPatchKind,
  literalValue,
  normalizeExpectedOldState,
  normalizeExternalNodeAnchor,
  normalizePatchTargetRef,
  omitUndefined,
  patchValueToString,
  rejectRedundantOwnedPatchExpectedOldState,
  requiredRecord,
  throwMissingPatchValue,
  OWNED_GRAPH_PATCH_KINDS,
} from './graphwrite-logic-body-compiler.js';
import { isRecord, requiredNonEmptyArray } from '../compiler-helpers.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';

export function compilePatchGraphWriteOps(behavior: Record<string, unknown>): GraphWriteCompiledOp[] {
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

export function compileExternalPatchGraphWriteOps(behavior: Record<string, unknown>): GraphWriteCompiledOp[] {
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
    if (kind !== 'set_external_pin_default' && kind !== 'set_external_node_comment') {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'External GraphWrite patch kind is not supported.', [
        {
          code: 'invalid_literal',
          path: `${path}.kind`,
          message: 'Use set_external_pin_default or set_external_node_comment.',
        },
      ]);
    }
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
