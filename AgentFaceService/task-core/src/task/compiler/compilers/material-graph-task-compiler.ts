import type { GraphWriteStructuredIrTaskPlanStep, TaskPlan, TaskSpec } from '../../schema/task-schemas.js';
import { buildTaskPlan } from '../task-plan-builder.js';
import type { TaskTypeCompiler } from '../task-type-compiler.js';

type MaterialGraphTaskSpec = Extract<TaskSpec, { task_type: 'edit_material_graph' }>;

type MaterialGraphBehavior = {
  readonly graph_strategy: string;
  readonly entries?: readonly MaterialGraphBlock[];
  readonly replace?: MaterialGraphBlock;
  readonly patches?: readonly MaterialGraphPatchBlock[];
  readonly merges?: readonly MaterialGraphMergeBlock[];
};

type MaterialGraphBlock = {
  readonly block_id: string;
  readonly nodes?: readonly MaterialGraphNode[];
  readonly links?: readonly MaterialGraphLink[];
};

type MaterialGraphPatchBlock = MaterialGraphBlock & {
  readonly deletes?: readonly MaterialGraphDelete[];
};

type MaterialGraphMergeBlock = MaterialGraphBlock;

type MaterialGraphNode = {
  readonly node_key: string;
  readonly selector: string | Record<string, unknown>;
  readonly properties?: Record<string, unknown>;
};

type MaterialGraphLink = {
  readonly from: MaterialGraphEndpoint;
  readonly to: MaterialGraphEndpoint;
};

type MaterialGraphEndpoint = {
  readonly node_key: string;
  readonly pin: string;
};

type MaterialGraphDelete = {
  readonly node_key: string;
};

type MaterialGraphOp = {
  readonly op: string;
  readonly [key: string]: unknown;
};

export const materialGraphTaskCompiler: TaskTypeCompiler<MaterialGraphTaskSpec> = {
  id: 'material_graph',
  taskType: 'edit_material_graph',
  canCompile(taskSpec): taskSpec is MaterialGraphTaskSpec {
    return taskSpec.task_type === 'edit_material_graph';
  },
  compile(taskSpec): TaskPlan {
    return buildTaskPlan({
      taskSpec,
      steps: [makeMaterialGraphTaskPlanStep(taskSpec)],
    });
  },
};

function makeMaterialGraphTaskPlanStep(taskSpec: MaterialGraphTaskSpec): GraphWriteStructuredIrTaskPlanStep {
  const behavior = taskSpec.behavior as MaterialGraphBehavior;
  return {
    step_id: 'step_material_graph',
    capability: 'graph_write',
    target: {
      asset_path: taskSpec.target.asset_path,
      graph: 'MaterialGraph',
      target_type: taskSpec.target.target_type ?? 'material_graph',
    },
    write: {
      strategy: 'owned_graph_edit',
      graph_domain: 'material_graph',
      material_strategy: behavior.graph_strategy,
      ops: compileMaterialGraphOps(behavior),
    },
    constraints: {
      allow_modify_user_nodes: false,
      ownership_scope: 'blueprinthelper_owned',
      graph_domain: 'material_graph',
    },
  };
}

function compileMaterialGraphOps(behavior: MaterialGraphBehavior): MaterialGraphOp[] {
  const ops: MaterialGraphOp[] = [{
    op: 'begin_material_graph_edit',
    graph_domain: 'material_graph',
    material_strategy: behavior.graph_strategy,
  }];

  if (behavior.graph_strategy === 'append_new_owned_graph') {
    for (const entry of behavior.entries ?? []) appendBlockOps(ops, 'append_new_owned_graph', entry);
  } else if (behavior.graph_strategy === 'replace_owned_graph' && behavior.replace) {
    appendBlockOps(ops, 'replace_owned_graph', behavior.replace);
  } else if (behavior.graph_strategy === 'patch_owned_graph') {
    for (const patch of behavior.patches ?? []) appendPatchOps(ops, patch);
  } else if (behavior.graph_strategy === 'merge_owned_graph') {
    for (const merge of behavior.merges ?? []) appendBlockOps(ops, 'merge_owned_graph', merge);
  }

  ops.push({ op: 'compile_material' });
  return ops;
}

function appendPatchOps(ops: MaterialGraphOp[], patch: MaterialGraphPatchBlock): void {
  ops.push({
    op: 'begin_material_block',
    block_id: patch.block_id,
    material_strategy: 'patch_owned_graph',
  });
  for (const deletion of patch.deletes ?? []) {
    ops.push({
      op: 'delete_owned_material_expression',
      block_id: patch.block_id,
      node_key: deletion.node_key,
    });
  }
  appendNodeOps(ops, patch.block_id, patch.nodes ?? []);
  appendLinkOps(ops, patch.block_id, patch.links ?? []);
}

function appendBlockOps(
  ops: MaterialGraphOp[],
  materialStrategy: string,
  block: MaterialGraphBlock,
): void {
  ops.push({
    op: 'begin_material_block',
    block_id: block.block_id,
    material_strategy: materialStrategy,
  });
  appendNodeOps(ops, block.block_id, block.nodes ?? []);
  appendLinkOps(ops, block.block_id, block.links ?? []);
}

function appendNodeOps(
  ops: MaterialGraphOp[],
  blockId: string,
  nodes: readonly MaterialGraphNode[],
): void {
  for (const node of nodes) {
    ops.push({
      op: selectorRequiresCandidateSearch(node.selector)
        ? 'resolve_material_expression'
        : 'spawn_material_expression',
      block_id: blockId,
      node_key: node.node_key,
      selector: node.selector,
    });
    if (node.properties && Object.keys(node.properties).length > 0) {
      ops.push({
        op: 'set_material_expression_properties',
        block_id: blockId,
        node_key: node.node_key,
        properties: node.properties,
      });
    }
  }
}

function appendLinkOps(
  ops: MaterialGraphOp[],
  blockId: string,
  links: readonly MaterialGraphLink[],
): void {
  for (const link of links) {
    ops.push({
      op: link.to.node_key === '$material_output'
        ? 'connect_material_property'
        : 'connect_material_expression',
      block_id: blockId,
      from: link.from,
      to: link.to,
    });
  }
}

function selectorRequiresCandidateSearch(selector: string | Record<string, unknown>): boolean {
  return typeof selector === 'object'
    && selector !== null
    && typeof selector['query'] === 'string';
}
