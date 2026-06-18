import assert from 'node:assert/strict';
import test from 'node:test';

import { compileTaskSpecToTaskPlan } from './task-compiler.js';

const externalSourcePin = {
  anchor_type: 'external_pin',
  anchor_ref: 'xpin:v1:e:aaaaaaaa.then#sourcefp',
};

const externalTargetPin = {
  anchor_type: 'external_pin',
  anchor_ref: 'xpin:v1:e:bbbbbbbb.execute#targetfp',
};

const externalLink = {
  anchor_type: 'external_link',
  anchor_ref: 'xlink:v1:e:aaaaaaaa.then>bbbbbbbb.execute#linkfp',
};

function makePatchExternalLinksSpec(overrides: {
  scopePolicy?: Record<string, unknown>;
  linkPatch?: Record<string, unknown>;
  behavior?: Record<string, unknown>;
} = {}) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_patch_external_links_ts',
    task_type: 'edit_blueprint_graph',
    feature_name: 'PatchExternalLinksTs',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
      external_mutation_policy: {
        strategy: 'patch_external_links',
        allowed_mutations: ['link_connect', 'link_disconnect', 'link_replace'],
      },
      ...overrides.scopePolicy,
    },
    behavior: {
      graph_strategy: 'patch_external_links',
      external_link_patches: [{
        kind: 'replace_link',
        anchor: externalLink,
        replacement: externalTargetPin,
        ...overrides.linkPatch,
      }],
      ...overrides.behavior,
    },
  };
}

function compilePatchExternalLinksStep(overrides?: Parameters<typeof makePatchExternalLinksSpec>[0]) {
  const taskPlan = compileTaskSpecToTaskPlan(makePatchExternalLinksSpec(overrides) as never);
  const step = taskPlan.steps.find((candidate) => (
    (candidate as Record<string, unknown>).capability === 'graph_write'
  )) as Record<string, unknown> | undefined;
  assert.ok(step);
  return step;
}

test('patch_external_links lowers replace_link to external graph edit with exact mutation policy', () => {
  const step = compilePatchExternalLinksStep();
  const write = step.write as { strategy: string; ops: Array<Record<string, unknown>> };

  assert.equal(write.strategy, 'external_graph_edit');
  assert.deepEqual(write.ops[0], {
    op: 'replace_external_link',
    link_anchor: externalLink,
    replacement_anchor: externalTargetPin,
  });
  assert.deepEqual(step.constraints, {
    allow_modify_user_nodes: false,
    ownership_scope: 'external_user_authored',
    external_mutation_policy: {
      strategy: 'patch_external_links',
      allowed_mutations: ['link_connect', 'link_disconnect', 'link_replace'],
    },
  });
});

test('patch_external_links lowers connect and disconnect compact anchors', () => {
  const connectStep = compilePatchExternalLinksStep({
    behavior: {
      external_link_patches: [{
        kind: 'connect_pins',
        source: externalSourcePin,
        target: externalTargetPin,
      }],
    },
  });
  const connectWrite = connectStep.write as { ops: Array<Record<string, unknown>> };
  assert.deepEqual(connectWrite.ops[0], {
    op: 'connect_external_pins',
    source_anchor: externalSourcePin,
    target_anchor: externalTargetPin,
  });

  const disconnectStep = compilePatchExternalLinksStep({
    behavior: {
      external_link_patches: [{
        kind: 'disconnect_link',
        anchor: externalLink,
      }],
    },
  });
  const disconnectWrite = disconnectStep.write as { ops: Array<Record<string, unknown>> };
  assert.deepEqual(disconnectWrite.ops[0], {
    op: 'disconnect_external_link',
    link_anchor: externalLink,
  });
});

test('patch_external_links rejects broad policy, wrong channel, and display-only refs', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makePatchExternalLinksSpec({
      scopePolicy: {
        allow_modify_user_nodes: true,
      },
    }) as never),
    /unsupported_scope_policy/,
  );

  assert.throws(
    () => compileTaskSpecToTaskPlan(makePatchExternalLinksSpec({
      behavior: {
        external_patches: [{
          kind: 'set_external_node_comment',
          anchor: {},
          value: 'wrong channel',
          expected_old_state: { value: '' },
        }],
      },
    }) as never),
    /external_patches does not belong|external_link_patches/,
  );

  assert.throws(
    () => compileTaskSpecToTaskPlan(makePatchExternalLinksSpec({
      linkPatch: {
        anchor: {
          anchor_type: 'external_link',
          anchor_ref: 'links[5]',
        },
      },
    }) as never),
    /links\[n\]|read-view|compact anchor_ref/,
  );
});

test('patch_external_links rejects compact anchor type and prefix mismatches', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makePatchExternalLinksSpec({
      linkPatch: {
        anchor: externalSourcePin,
      },
    }) as never),
    /external graph compact anchor type|External GraphWrite compact anchor type/i,
  );

  assert.throws(
    () => compileTaskSpecToTaskPlan(makePatchExternalLinksSpec({
      linkPatch: {
        replacement: {
          anchor_type: 'external_pin',
          anchor_ref: 'xlink:v1:e:aaaaaaaa.then>bbbbbbbb.execute#linkfp',
        },
      },
    }) as never),
    /wrong prefix/,
  );
});
