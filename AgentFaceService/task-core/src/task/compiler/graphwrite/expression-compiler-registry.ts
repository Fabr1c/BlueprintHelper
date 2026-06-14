import { getAllGraphWriteSlotDescriptors } from './graphwrite-slot-registry.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';
import type { GraphWriteExpressionCompileHandler } from './graphwrite-compiler-types.js';

export interface GraphWriteExpressionCompilerMetadata {
  compiler_id: string;
  public_kinds: readonly string[];
  compatibility_kinds?: readonly string[];
  slot_ids: readonly string[];
}

export interface GraphWriteExpressionCompilerRegistration {
  compiler_id: string;
  compile: GraphWriteExpressionCompileHandler;
}

export interface GraphWriteExpressionCompilerDescriptor extends GraphWriteExpressionCompilerMetadata {
  compatibility_kinds: readonly string[];
  compile: GraphWriteExpressionCompileHandler;
}

export interface ResolveGraphWriteExpressionCompilerInput {
  kind: string;
  path: string;
  capabilityId?: string;
}

const COMPATIBILITY_EXPRESSION_COMPILERS: readonly GraphWriteExpressionCompilerMetadata[] = [
  {
    compiler_id: 'expression.get_property',
    public_kinds: [],
    compatibility_kinds: ['get_property'],
    slot_ids: [],
  },
  {
    compiler_id: 'expression.field',
    public_kinds: [],
    compatibility_kinds: ['field'],
    slot_ids: [],
  },
  {
    compiler_id: 'expression.call',
    public_kinds: [],
    compatibility_kinds: ['call'],
    slot_ids: [],
  },
  {
    compiler_id: 'expression.deconstruct',
    public_kinds: [],
    compatibility_kinds: ['deconstruct'],
    slot_ids: [],
  },
  {
    compiler_id: 'expression.create',
    public_kinds: [],
    compatibility_kinds: ['create'],
    slot_ids: [],
  },
  {
    compiler_id: 'expression.convert',
    public_kinds: [],
    compatibility_kinds: ['convert'],
    slot_ids: [],
  },
  {
    compiler_id: 'expression.schedule',
    public_kinds: [],
    compatibility_kinds: ['schedule'],
    slot_ids: [],
  },
  {
    compiler_id: 'expression.container_action',
    public_kinds: [],
    compatibility_kinds: ['container_action'],
    slot_ids: [],
  },
];

export class ExpressionCompilerRegistry {
  private readonly descriptorsById: Map<string, GraphWriteExpressionCompilerDescriptor>;

  constructor(
    descriptors: readonly GraphWriteExpressionCompilerMetadata[],
    registrations: readonly GraphWriteExpressionCompilerRegistration[] = [],
  ) {
    this.descriptorsById = mergeExpressionCompilerDescriptors(descriptors, registrations);
  }

  getByCompilerId(compilerId: string): GraphWriteExpressionCompilerDescriptor | undefined {
    return this.descriptorsById.get(compilerId);
  }

  requireByCompilerId(compilerId: string): GraphWriteExpressionCompilerDescriptor {
    const descriptor = this.getByCompilerId(compilerId);
    if (!descriptor) {
      throw new Error(`Unknown GraphWrite expression compiler id: ${compilerId}`);
    }
    return descriptor;
  }

  requireForExpression(input: ResolveGraphWriteExpressionCompilerInput): GraphWriteExpressionCompilerDescriptor {
    return this.requireByCompilerId(resolveExpressionCompilerId(input));
  }

  getAll(): readonly GraphWriteExpressionCompilerDescriptor[] {
    return [...this.descriptorsById.values()];
  }
}

export function createDefaultExpressionCompilerRegistry(
  registrations: readonly GraphWriteExpressionCompilerRegistration[] = [],
): ExpressionCompilerRegistry {
  return new ExpressionCompilerRegistry(
    [
      ...expressionCompilerDescriptorsFromSlots(),
      ...COMPATIBILITY_EXPRESSION_COMPILERS,
    ],
    registrations,
  );
}

export function requireGraphWriteExpressionCompiler(
  input: ResolveGraphWriteExpressionCompilerInput,
): GraphWriteExpressionCompilerDescriptor {
  return defaultExpressionCompilerRegistry.requireForExpression(input);
}

export const defaultExpressionCompilerRegistry = createDefaultExpressionCompilerRegistry();

function expressionCompilerDescriptorsFromSlots(): GraphWriteExpressionCompilerMetadata[] {
  const byCompilerId = new Map<string, { publicKinds: Set<string>; slotIds: string[] }>();
  for (const slot of getAllGraphWriteSlotDescriptors()) {
    if (slot.slot_type !== 'expression') continue;
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

function mergeExpressionCompilerDescriptors(
  descriptors: readonly GraphWriteExpressionCompilerMetadata[],
  registrations: readonly GraphWriteExpressionCompilerRegistration[],
): Map<string, GraphWriteExpressionCompilerDescriptor> {
  const merged = new Map<string, { publicKinds: Set<string>; compatibilityKinds: Set<string>; slotIds: Set<string> }>();
  for (const descriptor of descriptors) {
    const entry = merged.get(descriptor.compiler_id) ?? {
      publicKinds: new Set<string>(),
      compatibilityKinds: new Set<string>(),
      slotIds: new Set<string>(),
    };
    descriptor.public_kinds.forEach((kind) => entry.publicKinds.add(kind));
    (descriptor.compatibility_kinds ?? []).forEach((kind) => entry.compatibilityKinds.add(kind));
    descriptor.slot_ids.forEach((slotId) => entry.slotIds.add(slotId));
    merged.set(descriptor.compiler_id, entry);
  }
  const registrationsById = new Map(registrations.map((registration) => [registration.compiler_id, registration]));
  return new Map([...merged.entries()].map(([compilerId, entry]) => [compilerId, {
    compiler_id: compilerId,
    public_kinds: [...entry.publicKinds].sort(),
    compatibility_kinds: [...entry.compatibilityKinds].sort(),
    slot_ids: [...entry.slotIds].sort(),
    compile: registrationsById.get(compilerId)?.compile ?? unboundExpressionCompiler,
  }]));
}

function unboundExpressionCompiler(
  input: Parameters<GraphWriteExpressionCompileHandler>[0],
): ReturnType<GraphWriteExpressionCompileHandler> {
  throw new Error(`GraphWrite expression compiler ${input.compilerId} has no registered handler.`);
}

function resolveExpressionCompilerId(input: ResolveGraphWriteExpressionCompilerInput): string {
  switch (input.kind) {
    case 'literal':
      return 'expression.literal';
    case 'get':
      return 'expression.get';
    case 'get_property':
      return 'expression.get_property';
    case 'field':
      return input.capabilityId === 'field.function_param_get'
        ? 'expression.get_function_param'
        : 'expression.field';
    case 'call':
      return 'expression.call';
    case 'op':
      return 'expression.op';
    case 'construct':
      return 'expression.construct';
    case 'deconstruct':
      return 'expression.deconstruct';
    case 'select':
      return 'expression.select';
    case 'create':
      return 'expression.create';
    case 'convert':
      return 'expression.convert';
    case 'schedule':
      return 'expression.schedule';
    case 'container_action':
      return 'expression.container_action';
    default:
      throw new TaskSpecCompileError(
        'unsupported_expression_kind',
        'Unsupported GraphWrite expression kind. Use {"kind":"get","target":"self"} for self.',
        [
        {
          code: 'unsupported_expression_kind',
          path: `${input.path}.kind`,
          message: 'Use literal, field, get, get_property, call, op, construct, deconstruct, select, create, convert, schedule, or container_action. For self, use {"kind":"get","target":"self"}.',
        },
        ],
      );
  }
}
