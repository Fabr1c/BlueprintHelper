import type { GraphWriteCompiledOp, GraphWriteCompileOptions, GraphWriteSignatureSplit } from './graphwrite-logic-body-compiler.js';
import {
  assertGraphWriteConnectivityPreflight,
  compileLogicBodyToSemanticLogicSpec,
  getRequiredLogicBody,
  getRequiredString,
  graphWriteAppendEventKind,
  graphWriteCatalogEvidence,
  makeCustomEventSignatureEvidenceId,
  optionalString,
  validateSupportedStatements,
} from './graphwrite-logic-body-compiler.js';
import { isRecord, requiredNonEmptyArray } from '../compiler-helpers.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';

export function compileAppendGraphWriteOps(
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
