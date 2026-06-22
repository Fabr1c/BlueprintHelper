import assert from 'node:assert/strict';
import { describe, it } from 'node:test';

import { GraphWriteTaskSpecSchema } from './task-schemas.js';

const externalBodyAnchorWithStableIdentity = {
  schema: 'BlueprintHelper.ExternalGraphAnchor.v1',
  asset_path: '/Game/BP/BP_Door',
  graph_name: 'EventGraph',
  node_guid: 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb',
  node_class: '/Script/BlueprintGraph.K2Node_Event',
  stable_name: 'ReceiveBeginPlay',
  entry_kind: 'event',
  member_name: 'ReceiveBeginPlay',
  function_name: 'ReceiveBeginPlay',
  display_name: 'Event ReceiveBeginPlay',
  semantic_role: 'body_entry',
  fingerprint: 'body_entry_fp',
};

function makeReplaceExternalBodySpec() {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_replace_external_body_stable_identity_schema',
    task_type: 'edit_blueprint_graph',
    feature_name: 'ReplaceExternalBodyStableIdentitySchema',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
      external_mutation_policy: {
        strategy: 'replace_external_body',
        allowed_mutations: ['body_replace'],
      },
    },
    behavior: {
      graph_strategy: 'replace_external_body',
      external_replace: {
        scope: 'event_body',
        anchor: externalBodyAnchorWithStableIdentity,
        expected_body_fingerprint: 'body_fp_before',
        require_full_dry_run: true,
        body: {
          schema: 'BlueprintLogicSpec.v2',
          statements: [{ kind: 'call', target: 'PrintString' }],
        },
      },
    },
  };
}

describe('GraphWrite replace_external_body task schema', () => {
  it('accepts stable body-entry identity emitted by read_context', () => {
    const result = GraphWriteTaskSpecSchema.safeParse(makeReplaceExternalBodySpec());
    assert.equal(result.success, true, result.success ? undefined : result.error.message);
  });

  it('rejects body-entry anchors without stable identity', () => {
    const spec = makeReplaceExternalBodySpec();
    const anchorWithoutStableName = {
      ...externalBodyAnchorWithStableIdentity,
    };
    delete (anchorWithoutStableName as Partial<typeof externalBodyAnchorWithStableIdentity>).stable_name;
    spec.behavior.external_replace.anchor = anchorWithoutStableName;

    const result = GraphWriteTaskSpecSchema.safeParse(spec);
    assert.equal(result.success, false);
    assert.match(JSON.stringify(result.error.issues), /external_body_entry_stable_name_required/);
  });
});
