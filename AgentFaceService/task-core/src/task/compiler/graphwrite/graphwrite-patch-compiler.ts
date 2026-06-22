import type { GraphWriteCompiledOp } from './graphwrite-logic-body-compiler.js';
import {
  getRequiredString,
  isRecord,
  literalValue,
  omitUndefined,
  optionalString,
  requiredNonEmptyArray,
  requiredRecord,
} from '../compiler-helpers.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';

const OWNED_GRAPH_PATCH_KINDS = [
  'set_pin_default',
  'set_node_comment',
  'connect_pins',
  'disconnect_link',
  'replace_link',
  'delete_owned_node',
] as const;

type OwnedGraphPatchKind = (typeof OWNED_GRAPH_PATCH_KINDS)[number];

const EXTERNAL_NODE_PROPERTY_DESCRIPTOR_IDS = [
  'k2.node.comment',
  'k2.call.function_target',
  'k2.field.member_reference',
] as const;

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

function defaultPatchScope(kind: string): string {
  if (kind === 'set_node_comment') return 'node_comment';
  if (kind === 'connect_pins') return 'connect_pins';
  if (kind === 'disconnect_link') return 'disconnect_link';
  if (kind === 'replace_link') return 'replace_link';
  if (kind === 'delete_owned_node') return 'node_delete';
  return 'pin_default';
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

function isOwnedGraphPatchKind(kind: string): kind is OwnedGraphPatchKind {
  return OWNED_GRAPH_PATCH_KINDS.includes(kind as OwnedGraphPatchKind);
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

function normalizeExternalNodeAnchor(anchor: Record<string, unknown>, path: string, kind: string): Record<string, unknown> {
  if (typeof anchor['anchor_type'] === 'string' || typeof anchor['anchor_ref'] === 'string') {
    if (kind === 'set_external_pin_default') {
      throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'set_external_pin_default requires a node external anchor with pin_name.', [
        {
          code: 'unsupported_external_anchor_type',
          path,
          message: 'Use a BlueprintHelper.ExternalGraphAnchor.v1 node anchor with pin_name for pin default patches.',
        },
      ]);
    }
    return normalizeCompactExternalAnchor(anchor, path, 'external_node');
  }

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
  for (const field of ['stable_name', 'entry_kind', 'member_name', 'function_name', 'display_name'] as const) {
    const value = optionalString(anchor, field);
    if (value) {
      out[field] = value;
    }
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
  throw new TaskSpecCompileError('unsupported_graph_write_anchor', message, [
    {
      code: 'unsupported_graph_write_anchor',
      path,
      message,
    },
  ]);
}

function literalRecordValues(record: Record<string, unknown>): Record<string, unknown> {
  return Object.fromEntries(
    Object.entries(record).map(([key, value]) => [key, literalValue(value)]),
  );
}

function patchValueToString(value: unknown): string {
  if (typeof value === 'string') return value;
  if (typeof value === 'number' || typeof value === 'boolean') return String(value);
  if (value === null || value === undefined) return '';
  return JSON.stringify(value);
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
    if (
      kind !== 'set_external_pin_default'
      && kind !== 'set_external_node_comment'
      && kind !== 'set_external_node_property'
    ) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'External GraphWrite patch kind is not supported.', [
        {
          code: 'invalid_literal',
          path: `${path}.kind`,
          message: 'Use set_external_pin_default, set_external_node_comment, or set_external_node_property.',
        },
      ]);
    }
    if (!Object.hasOwn(patch, 'value')) {
      throwMissingPatchValue(path, `${kind} requires value.`);
    }
    const expectedOldState = requiredRecord(patch, 'expected_old_state', `${path}.expected_old_state`);
    const propertyDescriptorId = kind === 'set_external_node_property'
      ? getRequiredString(patch, 'property_descriptor_id', `${path}.property_descriptor_id`)
      : undefined;
    if (propertyDescriptorId !== undefined) {
      assertAllowedString(
        propertyDescriptorId,
        `${path}.property_descriptor_id`,
        [...EXTERNAL_NODE_PROPERTY_DESCRIPTOR_IDS],
        'Use a registered external node property descriptor id.',
      );
    }
    if (kind !== 'set_external_node_property' && Object.hasOwn(patch, 'property_descriptor_id')) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'property_descriptor_id is only valid for set_external_node_property.', [
        {
          code: 'unsupported_field_value',
          path: `${path}.property_descriptor_id`,
          message: 'Remove property_descriptor_id or use set_external_node_property.',
        },
      ]);
    }

    return {
      op: kind,
      anchor: normalizeExternalNodeAnchor(requiredRecord(patch, 'anchor', `${path}.anchor`), `${path}.anchor`, kind),
      ...(propertyDescriptorId ? { property_descriptor_id: propertyDescriptorId } : {}),
      value: patchValueToString(literalValue(patch['value'])),
      expected_old_state: normalizeExpectedOldState(expectedOldState),
    } as GraphWriteCompiledOp;
  });
}

export function compileExternalLinkPatchGraphWriteOps(behavior: Record<string, unknown>): GraphWriteCompiledOp[] {
  const patches = requiredNonEmptyArray(behavior, 'external_link_patches', 'behavior.external_link_patches');
  return patches.map((rawPatch, index) => {
    const path = `behavior.external_link_patches[${index}]`;
    if (!isRecord(rawPatch)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'External link patch must be an object.', [
        {
          code: 'invalid_external_link_patch',
          path,
          message: 'External link patch must be an object.',
        },
      ]);
    }

    const patch = rawPatch as Record<string, unknown>;
    const kind = getRequiredString(patch, 'kind', `${path}.kind`);
    if (kind === 'connect_pins') {
      return {
        op: 'connect_external_pins',
        source_anchor: normalizeCompactExternalAnchor(
          requiredRecord(patch, 'source', `${path}.source`),
          `${path}.source`,
          'external_pin',
        ),
        target_anchor: normalizeCompactExternalAnchor(
          requiredRecord(patch, 'target', `${path}.target`),
          `${path}.target`,
          'external_pin',
        ),
      } as GraphWriteCompiledOp;
    }
    if (kind === 'disconnect_link') {
      return {
        op: 'disconnect_external_link',
        link_anchor: normalizeCompactExternalAnchor(
          requiredRecord(patch, 'anchor', `${path}.anchor`),
          `${path}.anchor`,
          'external_link',
        ),
      } as GraphWriteCompiledOp;
    }
    if (kind === 'replace_link') {
      return {
        op: 'replace_external_link',
        link_anchor: normalizeCompactExternalAnchor(
          requiredRecord(patch, 'anchor', `${path}.anchor`),
          `${path}.anchor`,
          'external_link',
        ),
        replacement_anchor: normalizeCompactExternalAnchor(
          requiredRecord(patch, 'replacement', `${path}.replacement`),
          `${path}.replacement`,
          'external_pin',
        ),
      } as GraphWriteCompiledOp;
    }

    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'External link patch kind is not supported.', [
      {
        code: 'invalid_literal',
        path: `${path}.kind`,
        message: 'Use connect_pins, disconnect_link, or replace_link.',
      },
    ]);
  });
}

function normalizeCompactExternalAnchor(
  anchor: Record<string, unknown>,
  path: string,
  expectedAnchorType: 'external_pin' | 'external_link' | 'external_node' | 'external_body',
): Record<string, unknown> {
  const anchorType = getRequiredString(anchor, 'anchor_type', `${path}.anchor_type`);
  if (anchorType !== expectedAnchorType) {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'External GraphWrite compact anchor type is not supported here.', [
      {
        code: 'unsupported_external_anchor_type',
        path: `${path}.anchor_type`,
        message: `Use anchor_type="${expectedAnchorType}".`,
      },
    ]);
  }
  const anchorRef = getRequiredString(anchor, 'anchor_ref', `${path}.anchor_ref`);
  if (isRawLogicJsonArrayRef(anchorRef)) {
    throwUnsupportedGraphWriteAnchor(
      `${path}.anchor_ref`,
      `${path}.anchor_ref uses a read-view array index. Use compact anchor_ref from read_context instead.`,
    );
  }
  const expectedPrefixByType: Record<string, string> = {
    external_node: 'xnode:v1:',
    external_pin: 'xpin:v1:',
    external_link: 'xlink:v1:',
    external_body: 'xbody:v1:',
  };
  const expectedPrefix = expectedPrefixByType[expectedAnchorType];
  if (!anchorRef.startsWith(expectedPrefix)) {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'External GraphWrite compact anchor_ref has the wrong prefix.', [
      {
        code: 'unsupported_external_anchor_ref',
        path: `${path}.anchor_ref`,
        message: `Use compact ${expectedAnchorType} anchor_ref emitted by read_context (${expectedPrefix}...).`,
      },
    ]);
  }
  return {
    anchor_type: anchorType,
    anchor_ref: anchorRef,
  };
}
