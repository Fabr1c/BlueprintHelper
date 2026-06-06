import type { GraphWriteCompiledOp, GraphWriteCompileOptions } from './graphwrite-logic-body-compiler.js';
import {
  assertAllowedString,
  assertGraphWriteConnectivityPreflight,
  compileLogicBodyToSemanticLogicSpec,
  getRequiredLogicBody,
  getRequiredString,
  normalizeExternalBodyEntryAnchor,
  normalizeReplaceSelector,
  omitUndefined,
  optionalString,
  requiredRecord,
  validateSupportedStatements,
} from './graphwrite-logic-body-compiler.js';
import { isRecord } from '../compiler-helpers.js';
import { requireGraphWriteRouteByScope } from './graphwrite-route-registry.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';

export function compileReplaceGraphWriteOp(
  behavior: Record<string, unknown>,
  options: GraphWriteCompileOptions,
): GraphWriteCompiledOp {
  const replace = requiredRecord(behavior, 'replace', 'behavior.replace');
  const replaceScope = getRequiredString(replace, 'scope', 'behavior.replace.scope');
  const graphWriteReplaceScope = replaceScope === 'custom_event_definition'
    ? 'custom_event_body'
    : replaceScope;
  const route = requireGraphWriteRouteByScope('replace_owned_graph', graphWriteReplaceScope);
  if (!route.selector) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'GraphWrite replace route is missing selector metadata.', [
      {
        code: 'missing_graphwrite_selector_descriptor',
        path: 'behavior.replace.scope',
        message: `Descriptor ${route.route_id} must define selector metadata.`,
      },
    ]);
  }
  const selector = normalizeReplaceSelector(
    graphWriteReplaceScope,
    requiredRecord(replace, 'selector', 'behavior.replace.selector'),
    route.selector,
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

export function compileExternalReplaceBodyGraphWriteOp(
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
