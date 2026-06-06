import type {
  GraphWriteExpressionCompilerRegistration,
} from './expression-compiler-registry.js';
import type {
  GraphWriteExpressionCompileHandler,
} from './graphwrite-compiler-types.js';

const DEFAULT_EXPRESSION_COMPILER_IDS = [
  'expression.call',
  'expression.construct',
  'expression.container_action',
  'expression.convert',
  'expression.create',
  'expression.deconstruct',
  'expression.field',
  'expression.get',
  'expression.get_function_param',
  'expression.get_property',
  'expression.literal',
  'expression.op',
  'expression.schedule',
  'expression.select',
] as const;

export interface GraphWriteExpressionCompilerHandlers {
  compileExpression: GraphWriteExpressionCompileHandler;
}

export function createGraphWriteExpressionCompilerRegistrations(
  handlers: GraphWriteExpressionCompilerHandlers,
): GraphWriteExpressionCompilerRegistration[] {
  return DEFAULT_EXPRESSION_COMPILER_IDS.map((compilerId) => ({
    compiler_id: compilerId,
    compile: handlers.compileExpression,
  }));
}

export function getDefaultGraphWriteExpressionCompilerIds(): readonly string[] {
  return DEFAULT_EXPRESSION_COMPILER_IDS;
}
