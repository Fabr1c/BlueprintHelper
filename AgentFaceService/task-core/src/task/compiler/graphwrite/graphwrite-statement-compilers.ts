import type {
  BlueprintLogicStatement,
} from '../../schema/task-schemas.js';
import type {
  GraphWriteStatementCompilerRegistration,
} from './statement-compiler-registry.js';
import type {
  CompiledStatementFlow,
  GraphWriteStatementCompileInput,
  GraphWriteStatementNodeCompileInput,
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

export interface GraphWriteStatementCompilerServices {
  compileBranchFlow(input: GraphWriteStatementCompileInput): CompiledStatementFlow;
  compileReturnFlow(input: GraphWriteStatementCompileInput): CompiledStatementFlow;
  compileSequenceFlow(input: GraphWriteStatementCompileInput): CompiledStatementFlow;
  compileGenericControlFlow(input: GraphWriteStatementCompileInput): CompiledStatementFlow;
  compileLetFlow(input: GraphWriteStatementCompileInput): CompiledStatementFlow;
  compileContainerActionFlow(input: GraphWriteStatementCompileInput): CompiledStatementFlow;
  compileDefaultExecFlow(input: GraphWriteStatementCompileInput): CompiledStatementFlow;
  compileCallNode(input: GraphWriteStatementNodeCompileInput): ReturnType<GraphWriteStatementCompilerRegistration['compile_node']>;
  compileComponentBoundEventNode(input: GraphWriteStatementNodeCompileInput): ReturnType<GraphWriteStatementCompilerRegistration['compile_node']>;
  compileContainerActionNode(input: GraphWriteStatementNodeCompileInput): ReturnType<GraphWriteStatementCompilerRegistration['compile_node']>;
  compileGenericControlNode(input: GraphWriteStatementNodeCompileInput): ReturnType<GraphWriteStatementCompilerRegistration['compile_node']>;
  compileConvertOrScheduleNode(input: GraphWriteStatementNodeCompileInput): ReturnType<GraphWriteStatementCompilerRegistration['compile_node']>;
  compileCreateNode(input: GraphWriteStatementNodeCompileInput): ReturnType<GraphWriteStatementCompilerRegistration['compile_node']>;
  compileDelegateNode(input: GraphWriteStatementNodeCompileInput): ReturnType<GraphWriteStatementCompilerRegistration['compile_node']>;
  compileFieldNode(input: GraphWriteStatementNodeCompileInput): ReturnType<GraphWriteStatementCompilerRegistration['compile_node']>;
  compileSetNode(input: GraphWriteStatementNodeCompileInput): ReturnType<GraphWriteStatementCompilerRegistration['compile_node']>;
  compileSetPropertyNode(input: GraphWriteStatementNodeCompileInput): ReturnType<GraphWriteStatementCompilerRegistration['compile_node']>;
  compileUnsupportedNode(input: GraphWriteStatementNodeCompileInput): never;
}

export function createGraphWriteStatementCompilerRegistrations(
  services: GraphWriteStatementCompilerServices,
): GraphWriteStatementCompilerRegistration[] {
  return [
    registration('statement.call', services.compileDefaultExecFlow, services.compileCallNode),
    registration('statement.component_bound_event', services.compileDefaultExecFlow, services.compileComponentBoundEventNode),
    registration('statement.container_action', services.compileContainerActionFlow, services.compileContainerActionNode),
    registration('statement.control.branch', normalizeControlBranchFlow(services), services.compileUnsupportedNode),
    registration('statement.control.generic', services.compileGenericControlFlow, services.compileGenericControlNode),
    registration('statement.control.return', services.compileReturnFlow, services.compileUnsupportedNode),
    registration('statement.control.sequence', services.compileSequenceFlow, services.compileUnsupportedNode),
    registration('statement.convert', services.compileDefaultExecFlow, services.compileConvertOrScheduleNode),
    registration('statement.create', services.compileDefaultExecFlow, services.compileCreateNode),
    registration('statement.delegate', services.compileDefaultExecFlow, services.compileDelegateNode),
    registration('statement.field', services.compileDefaultExecFlow, services.compileFieldNode),
    registration('statement.let', services.compileLetFlow, services.compileUnsupportedNode),
    registration('statement.schedule', services.compileDefaultExecFlow, services.compileConvertOrScheduleNode),
    registration('statement.set', services.compileDefaultExecFlow, services.compileSetNode),
    registration('statement.set_property', services.compileDefaultExecFlow, services.compileSetPropertyNode),
  ];
}

export function getDefaultGraphWriteStatementCompilerIds(): readonly string[] {
  return DEFAULT_STATEMENT_COMPILER_IDS;
}

function registration(
  compilerId: (typeof DEFAULT_STATEMENT_COMPILER_IDS)[number],
  compileFlow: GraphWriteStatementCompilerRegistration['compile_flow'],
  compileNode: GraphWriteStatementCompilerRegistration['compile_node'],
): GraphWriteStatementCompilerRegistration {
  return {
    compiler_id: compilerId,
    compile_flow: (input) => compileFlow({ ...input, compilerId }),
    compile_node: (input) => compileNode({ ...input, compilerId }),
  };
}

function normalizeControlBranchFlow(
  services: GraphWriteStatementCompilerServices,
): GraphWriteStatementCompilerRegistration['compile_flow'] {
  return (input) => {
    const statementRecord = input.statement as Record<string, unknown>;
    const kind = typeof statementRecord.kind === 'string' ? statementRecord.kind : '';
    if (kind !== 'control') {
      return services.compileBranchFlow(input);
    }
    return services.compileBranchFlow({
      ...input,
      statement: { ...statementRecord, kind: 'branch' } as BlueprintLogicStatement,
    });
  };
}
