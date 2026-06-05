import { TaskSpecCompileError } from '../task-compiler-errors.js';
import {
  getGraphWriteCompilerIdForStrategy,
  getSupportedGraphWriteStrategies,
} from './graphwrite-route-registry.js';

export type GraphWriteCompiledOperation = Record<string, unknown> & { op: string };

export interface GraphWriteOperationCompileOptions {
  defaultFieldOwnerClass?: string;
}

export type GraphWriteOperationCompileHandler = (
  behavior: Record<string, unknown>,
  options: GraphWriteOperationCompileOptions,
) => GraphWriteCompiledOperation[];

export interface GraphWriteOperationCompilerRegistration {
  compilerId: string;
  compile: GraphWriteOperationCompileHandler;
}

export class GraphWriteOperationCompilerRegistry {
  private readonly compilers = new Map<string, GraphWriteOperationCompileHandler>();

  register(registration: GraphWriteOperationCompilerRegistration): void {
    if (this.compilers.has(registration.compilerId)) {
      throw new Error(`Duplicate GraphWrite operation compiler id: ${registration.compilerId}`);
    }
    this.compilers.set(registration.compilerId, registration.compile);
  }

  compile(
    behavior: Record<string, unknown>,
    options: GraphWriteOperationCompileOptions = {},
  ): GraphWriteCompiledOperation[] {
    const strategy = readGraphStrategy(behavior);
    const compilerId = getGraphWriteCompilerIdForStrategy(strategy);
    if (!compilerId) {
      throwUnsupportedGraphStrategy();
    }
    const compiler = this.compilers.get(compilerId);
    if (!compiler) {
      throw new TaskSpecCompileError('unsupported_graph_strategy', 'GraphWrite operation compiler is not registered.', [
        {
          code: 'unsupported_graphwrite_operation_compiler',
          path: 'behavior.graph_strategy',
          message: `GraphWrite compiler_id="${compilerId}" is not registered for graph_strategy="${strategy}".`,
        },
      ]);
    }
    return compiler(behavior, options);
  }
}

export function createGraphWriteOperationCompilerRegistry(
  registrations: readonly GraphWriteOperationCompilerRegistration[],
): GraphWriteOperationCompilerRegistry {
  const registry = new GraphWriteOperationCompilerRegistry();
  for (const registration of registrations) {
    registry.register(registration);
  }
  return registry;
}

function readGraphStrategy(behavior: Record<string, unknown>): string {
  const value = behavior['graph_strategy'];
  if (typeof value === 'string' && value.trim().length > 0) {
    return value;
  }
  throw new TaskSpecCompileError('taskspec_semantic_invalid', 'behavior.graph_strategy must be a non-empty string.', [
    {
      code: 'missing_required_string',
      path: 'behavior.graph_strategy',
      message: 'behavior.graph_strategy must be a non-empty string.',
    },
  ]);
}

function throwUnsupportedGraphStrategy(): never {
  const strategies = getSupportedGraphWriteStrategies();
  throw new TaskSpecCompileError('unsupported_graph_strategy', 'Unsupported GraphWrite graph_strategy.', [
    {
      code: 'unsupported_graph_strategy',
      path: 'behavior.graph_strategy',
      message: `Use ${strategies.join(', ')}.`,
      suggested_patch: { op: 'replace', path: '/behavior/graph_strategy', value: strategies[0] ?? 'append_new_owned_graph' },
    },
  ]);
}
