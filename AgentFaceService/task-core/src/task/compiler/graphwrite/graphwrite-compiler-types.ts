import type {
  AgentImportLink,
  AgentImportNode,
  BlueprintLogicStatement,
} from '../../schema/task-schemas.js';

export interface CompiledSymbolValue {
  output?: string;
  defaultValue?: unknown;
}

export interface CompileFlowContext {
  symbols: Map<string, CompiledSymbolValue>;
}

export interface CompiledStatementFlow {
  nodes: AgentImportNode[];
  links: AgentImportLink[];
  entry?: string;
  exits: string[];
  preservePreviousExits?: boolean;
}

export interface CompiledConditionFlow {
  nodes: AgentImportNode[];
  links: AgentImportLink[];
  output?: string;
  defaultValue?: unknown;
}

export interface GraphWriteStatementCompileInput {
  statement: BlueprintLogicStatement;
  nodeId: string;
  path: string;
  context: CompileFlowContext;
  compilerId: string;
}

export interface GraphWriteStatementNodeCompileInput {
  statement: BlueprintLogicStatement;
  nodeId: string;
  path: string;
  compilerId: string;
}

export interface GraphWriteExpressionCompileInput {
  expression: unknown;
  nodeId: string;
  path: string;
  context: CompileFlowContext;
  compilerId: string;
}

export type GraphWriteStatementFlowCompileHandler = (
  input: GraphWriteStatementCompileInput,
) => CompiledStatementFlow;

export type GraphWriteStatementNodeCompileHandler = (
  input: GraphWriteStatementNodeCompileInput,
) => AgentImportNode;

export type GraphWriteExpressionCompileHandler = (
  input: GraphWriteExpressionCompileInput,
) => CompiledConditionFlow;
