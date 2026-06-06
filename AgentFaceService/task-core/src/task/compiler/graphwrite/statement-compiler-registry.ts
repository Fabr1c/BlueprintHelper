import { getAllGraphWriteSlotDescriptors } from './graphwrite-slot-registry.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';
import type {
  GraphWriteStatementFlowCompileHandler,
  GraphWriteStatementNodeCompileHandler,
} from './graphwrite-compiler-types.js';

export interface GraphWriteStatementCompilerMetadata {
  compiler_id: string;
  public_kinds: readonly string[];
  slot_ids: readonly string[];
}

export interface GraphWriteStatementCompilerRegistration {
  compiler_id: string;
  compile_flow: GraphWriteStatementFlowCompileHandler;
  compile_node: GraphWriteStatementNodeCompileHandler;
}

export interface GraphWriteStatementCompilerDescriptor extends GraphWriteStatementCompilerMetadata {
  compile_flow: GraphWriteStatementFlowCompileHandler;
  compile_node: GraphWriteStatementNodeCompileHandler;
}

export interface ResolveGraphWriteStatementCompilerInput {
  kind: string;
  path: string;
  controlKind?: string;
  delegateOperation?: string;
}

const PUBLIC_DELEGATE_KINDS = new Set([
  'delegate.bind',
  'delegate.assign',
  'delegate.unbind',
  'delegate.unbind_all',
  'delegate.call',
]);

const COMPATIBILITY_STATEMENT_COMPILERS: readonly GraphWriteStatementCompilerMetadata[] = [
  {
    compiler_id: 'statement.control.sequence',
    public_kinds: ['sequence'],
    slot_ids: [],
  },
  {
    compiler_id: 'statement.delegate',
    public_kinds: ['delegate.assign', 'delegate.unbind', 'delegate.unbind_all', 'delegate.call', 'delegate'],
    slot_ids: [],
  },
];

export class StatementCompilerRegistry {
  private readonly descriptorsById: Map<string, GraphWriteStatementCompilerDescriptor>;

  constructor(
    descriptors: readonly GraphWriteStatementCompilerMetadata[],
    registrations: readonly GraphWriteStatementCompilerRegistration[] = [],
  ) {
    this.descriptorsById = mergeStatementCompilerDescriptors(descriptors, registrations);
  }

  getByCompilerId(compilerId: string): GraphWriteStatementCompilerDescriptor | undefined {
    return this.descriptorsById.get(compilerId);
  }

  requireByCompilerId(compilerId: string): GraphWriteStatementCompilerDescriptor {
    const descriptor = this.getByCompilerId(compilerId);
    if (!descriptor) {
      throw new Error(`Unknown GraphWrite statement compiler id: ${compilerId}`);
    }
    return descriptor;
  }

  requireForStatement(input: ResolveGraphWriteStatementCompilerInput): GraphWriteStatementCompilerDescriptor {
    return this.requireByCompilerId(resolveStatementCompilerId(input));
  }

  getAll(): readonly GraphWriteStatementCompilerDescriptor[] {
    return [...this.descriptorsById.values()];
  }
}

export function createDefaultStatementCompilerRegistry(
  registrations: readonly GraphWriteStatementCompilerRegistration[] = [],
): StatementCompilerRegistry {
  return new StatementCompilerRegistry(
    [
      ...statementCompilerDescriptorsFromSlots(),
      ...COMPATIBILITY_STATEMENT_COMPILERS,
    ],
    registrations,
  );
}

export function requireGraphWriteStatementCompiler(
  input: ResolveGraphWriteStatementCompilerInput,
): GraphWriteStatementCompilerDescriptor {
  return defaultStatementCompilerRegistry.requireForStatement(input);
}

export const defaultStatementCompilerRegistry = createDefaultStatementCompilerRegistry();

function statementCompilerDescriptorsFromSlots(): GraphWriteStatementCompilerMetadata[] {
  const byCompilerId = new Map<string, { publicKinds: Set<string>; slotIds: string[] }>();
  for (const slot of getAllGraphWriteSlotDescriptors()) {
    if (slot.slot_type !== 'statement') continue;
    const entry = byCompilerId.get(slot.compiler_id) ?? { publicKinds: new Set<string>(), slotIds: [] };
    entry.publicKinds.add(slot.kind);
    entry.slotIds.push(slot.slot_id);
    byCompilerId.set(slot.compiler_id, entry);
  }
  return [...byCompilerId.entries()].map(([compilerId, entry]) => ({
    compiler_id: compilerId,
    public_kinds: [...entry.publicKinds].sort(),
    slot_ids: [...entry.slotIds].sort(),
  }));
}

function mergeStatementCompilerDescriptors(
  descriptors: readonly GraphWriteStatementCompilerMetadata[],
  registrations: readonly GraphWriteStatementCompilerRegistration[],
): Map<string, GraphWriteStatementCompilerDescriptor> {
  const merged = new Map<string, { publicKinds: Set<string>; slotIds: Set<string> }>();
  for (const descriptor of descriptors) {
    const entry = merged.get(descriptor.compiler_id) ?? { publicKinds: new Set<string>(), slotIds: new Set<string>() };
    descriptor.public_kinds.forEach((kind) => entry.publicKinds.add(kind));
    descriptor.slot_ids.forEach((slotId) => entry.slotIds.add(slotId));
    merged.set(descriptor.compiler_id, entry);
  }
  const registrationsById = new Map(registrations.map((registration) => [registration.compiler_id, registration]));
  return new Map([...merged.entries()].map(([compilerId, entry]) => [compilerId, {
    compiler_id: compilerId,
    public_kinds: [...entry.publicKinds].sort(),
    slot_ids: [...entry.slotIds].sort(),
    compile_flow: registrationsById.get(compilerId)?.compile_flow ?? unboundStatementFlowCompiler,
    compile_node: registrationsById.get(compilerId)?.compile_node ?? unboundStatementNodeCompiler,
  }]));
}

function unboundStatementFlowCompiler(
  input: Parameters<GraphWriteStatementFlowCompileHandler>[0],
): ReturnType<GraphWriteStatementFlowCompileHandler> {
  throw new Error(`GraphWrite statement compiler ${input.compilerId} has no registered flow handler.`);
}

function unboundStatementNodeCompiler(
  input: Parameters<GraphWriteStatementNodeCompileHandler>[0],
): ReturnType<GraphWriteStatementNodeCompileHandler> {
  throw new Error(`GraphWrite statement compiler ${input.compilerId} has no registered node handler.`);
}

function resolveStatementCompilerId(input: ResolveGraphWriteStatementCompilerInput): string {
  const kind = input.kind;
  if (kind === 'control') {
    if (input.controlKind === 'branch') return 'statement.control.branch';
    if (input.controlKind === 'return') return 'statement.control.return';
    if (input.controlKind === 'sequence') return 'statement.control.sequence';
    return 'statement.control.generic';
  }
  if (kind === 'branch') return 'statement.control.branch';
  if (kind === 'return') return 'statement.control.return';
  if (kind === 'sequence') return 'statement.control.sequence';
  if (kind === 'component_bound_event') return 'statement.component_bound_event';
  if (PUBLIC_DELEGATE_KINDS.has(kind) || (kind === 'delegate' && input.delegateOperation)) {
    return 'statement.delegate';
  }
  if (kind === 'container_action') return 'statement.container_action';
  if (kind === 'call') return 'statement.call';
  if (kind === 'set') return 'statement.set';
  if (kind === 'set_property') return 'statement.set_property';
  if (kind === 'let') return 'statement.let';
  if (kind === 'create') return 'statement.create';
  if (kind === 'convert') return 'statement.convert';
  if (kind === 'schedule') return 'statement.schedule';
  if (kind === 'field') return 'statement.field';

  throw new TaskSpecCompileError('unsupported_statement_kind', 'Unsupported GraphWrite statement kind.', [
    {
      code: 'unsupported_statement_kind',
      path: `${input.path}.kind`,
      message: 'Use call, field, create, convert, schedule, set, set_property, let, control, container_action, component_bound_event, or delegate.*.',
    },
  ]);
}
