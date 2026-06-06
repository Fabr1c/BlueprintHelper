import type {
  GraphWriteExpressionCompilerRegistration,
} from './expression-compiler-registry.js';
import type {
  CompiledConditionFlow,
  GraphWriteExpressionCompileInput,
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

export interface GraphWriteExpressionCompilerServices {
  compileLiteral(input: GraphWriteExpressionCompileInput): CompiledConditionFlow;
  compileContainerAction(input: GraphWriteExpressionCompileInput): CompiledConditionFlow;
  compileFieldGet(input: GraphWriteExpressionCompileInput): CompiledConditionFlow;
  compileGeneral(input: GraphWriteExpressionCompileInput): CompiledConditionFlow;
}

export function createGraphWriteExpressionCompilerRegistrations(
  services: GraphWriteExpressionCompilerServices,
): GraphWriteExpressionCompilerRegistration[] {
  return [
    registration('expression.call', services.compileGeneral),
    registration('expression.construct', services.compileGeneral),
    registration('expression.container_action', services.compileContainerAction),
    registration('expression.convert', services.compileGeneral),
    registration('expression.create', services.compileGeneral),
    registration('expression.deconstruct', services.compileGeneral),
    registration('expression.field', services.compileFieldGet),
    registration('expression.get', services.compileFieldGet),
    registration('expression.get_function_param', services.compileFieldGet),
    registration('expression.get_property', services.compileFieldGet),
    registration('expression.literal', services.compileLiteral),
    registration('expression.op', services.compileGeneral),
    registration('expression.schedule', services.compileGeneral),
    registration('expression.select', services.compileGeneral),
  ];
}

export function getDefaultGraphWriteExpressionCompilerIds(): readonly string[] {
  return DEFAULT_EXPRESSION_COMPILER_IDS;
}

function registration(
  compilerId: (typeof DEFAULT_EXPRESSION_COMPILER_IDS)[number],
  compile: GraphWriteExpressionCompilerRegistration['compile'],
): GraphWriteExpressionCompilerRegistration {
  return {
    compiler_id: compilerId,
    compile: (input) => compile({ ...input, compilerId }),
  };
}
