import type {
  GraphWriteStatementCompilerRegistration,
} from './statement-compiler-registry.js';
import type {
  GraphWriteStatementFlowCompileHandler,
  GraphWriteStatementNodeCompileHandler,
} from './graphwrite-compiler-types.js';

const DEFAULT_STATEMENT_COMPILER_IDS = [
  'statement.call',
  'statement.component_bound_event',
  'statement.container_action',
  'statement.control.branch',
  'statement.control.generic',
  'statement.control.return',
  'statement.control.sequence',
  'statement.convert',
  'statement.create',
  'statement.delegate',
  'statement.field',
  'statement.let',
  'statement.schedule',
  'statement.set',
  'statement.set_property',
] as const;

export interface GraphWriteStatementCompilerHandlers {
  compileFlow: GraphWriteStatementFlowCompileHandler;
  compileNode: GraphWriteStatementNodeCompileHandler;
}

export function createGraphWriteStatementCompilerRegistrations(
  handlers: GraphWriteStatementCompilerHandlers,
): GraphWriteStatementCompilerRegistration[] {
  return DEFAULT_STATEMENT_COMPILER_IDS.map((compilerId) => ({
    compiler_id: compilerId,
    compile_flow: handlers.compileFlow,
    compile_node: handlers.compileNode,
  }));
}

export function getDefaultGraphWriteStatementCompilerIds(): readonly string[] {
  return DEFAULT_STATEMENT_COMPILER_IDS;
}
