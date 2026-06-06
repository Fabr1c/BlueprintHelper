import {
  createGraphWriteOperationCompilerRegistry,
  type GraphWriteOperationCompilerRegistry,
} from './graphwrite-operation-compiler-registry.js';
import {
  compileAppendGraphWriteOps,
  compileExternalMergeGraphWriteOps,
  compileExternalReplaceBodyGraphWriteOp,
  compileMergeGraphWriteOps,
  type GraphWriteCompiledOp,
  type GraphWriteCompileOptions,
} from './graphwrite-logic-body-compiler.js';
import {
  compileExternalPatchGraphWriteOps,
  compilePatchGraphWriteOps,
} from './graphwrite-patch-compiler.js';
import { compileReplaceGraphWriteOp } from './graphwrite-replace-compiler.js';

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
