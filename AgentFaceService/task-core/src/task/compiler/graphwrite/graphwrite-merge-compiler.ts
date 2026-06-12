import type { GraphWriteCompiledOp, GraphWriteCompileOptions } from './graphwrite-logic-body-compiler.js';
import {
  assertAllowedString,
  assertGraphWriteConnectivityPreflight,
  compileLogicBodyToSemanticLogicSpec,
  getRequiredLogicBody,
  getRequiredString,
  normalizeExternalFlowAnchor,
  normalizeMergeAnchor,
  normalizeMergeInserted,
  normalizeMergeSequenceOrder,
  omitUndefined,
  requiredRecord,
  validateSupportedStatements,
} from './graphwrite-logic-body-compiler.js';
import { isRecord, requiredNonEmptyArray } from '../compiler-helpers.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';

export function compileMergeGraphWriteOps(behavior: Record<string, unknown>): GraphWriteCompiledOp[] {
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

export function compileExternalMergeGraphWriteOps(
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
      anchor: normalizeExternalFlowAnchor(requiredRecord(merge, 'anchor', `${path}.anchor`), insertStrategy, `${path}.anchor`),
      inserted: {
        body: compileLogicBodyToSemanticLogicSpec(body, `external_merge_${index}`, options),
      },
      sequence_order: normalizeMergeSequenceOrder(merge, insertStrategy, `${path}.sequence_order`),
    }) as GraphWriteCompiledOp;
  });
}
