import {
  createGraphWriteOperationCompilerRegistry,
  type GraphWriteOperationCompilerRegistry,
} from './graphwrite-operation-compiler-registry.js';
import {
  type GraphWriteCompiledOp,
  type GraphWriteCompileOptions,
} from './graphwrite-logic-body-compiler.js';
import { compileAppendGraphWriteOps } from './graphwrite-append-compiler.js';
import {
  compileExternalMergeGraphWriteOps,
  compileMergeGraphWriteOps,
} from './graphwrite-merge-compiler.js';
import {
  compileExternalLinkPatchGraphWriteOps,
  compileExternalPatchGraphWriteOps,
  compilePatchGraphWriteOps,
} from './graphwrite-patch-compiler.js';
import {
  compileExternalReplaceBodyGraphWriteOp,
  compileReplaceGraphWriteOp,
} from './graphwrite-replace-compiler.js';

export function createDefaultGraphWriteOperationCompilerRegistry(): GraphWriteOperationCompilerRegistry {
  return createGraphWriteOperationCompilerRegistry([
    {
      compilerId: 'append_new_owned_graph',
      compile: (behavior, options) => compileAppendGraphWriteOps(behavior, options),
    },
    {
      compilerId: 'replace_body',
      compile: (behavior, options) => [compileReplaceGraphWriteOp(behavior, options)],
    },
    {
      compilerId: 'patch_owned_graph',
      compile: (behavior) => compilePatchGraphWriteOps(behavior),
    },
    {
      compilerId: 'merge_owned_graph',
      compile: (behavior) => compileMergeGraphWriteOps(behavior),
    },
    {
      compilerId: 'merge_external_flow',
      compile: (behavior, options) => compileExternalMergeGraphWriteOps(behavior, options),
    },
    {
      compilerId: 'patch_external_graph',
      compile: (behavior) => compileExternalPatchGraphWriteOps(behavior),
    },
    {
      compilerId: 'patch_external_links',
      compile: (behavior) => compileExternalLinkPatchGraphWriteOps(behavior),
    },
    {
      compilerId: 'replace_external_body',
      compile: (behavior, options) => [compileExternalReplaceBodyGraphWriteOp(behavior, options)],
    },
  ]);
}

export function compileGraphWriteOps(
  behavior: Record<string, unknown>,
  options: GraphWriteCompileOptions = {},
): GraphWriteCompiledOp[] {
  return createDefaultGraphWriteOperationCompilerRegistry().compile(behavior, options) as GraphWriteCompiledOp[];
}
