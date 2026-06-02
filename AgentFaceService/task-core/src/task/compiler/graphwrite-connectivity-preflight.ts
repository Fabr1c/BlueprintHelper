export interface GraphWriteConnectivityPreflightIssue {
  code: 'unconsumed_pure_data_node';
  path: string;
  message: string;
}

export function collectGraphWriteConnectivityPreflightIssues(
  statements: readonly Record<string, unknown>[],
  basePath: string,
): GraphWriteConnectivityPreflightIssue[] {
  const defined = new Map<string, { name: string; path: string }>();
  const used = new Set<string>();

  statements.forEach((statement, index) => {
    const path = `${basePath}[${index}]`;
    const kind = typeof statement.kind === 'string' ? statement.kind : '';
    const name = typeof statement.name === 'string' ? statement.name.trim() : '';
    if (kind === 'let' && name.length > 0 && isObviousPureDataProducer(statement.value)) {
      defined.set(name.toLowerCase(), { name, path });
    }
    collectGetReferences(statement, used);
  });

  const issues: GraphWriteConnectivityPreflightIssue[] = [];
  for (const [normalizedName, definition] of defined) {
    if (!used.has(normalizedName)) {
      issues.push({
        code: 'unconsumed_pure_data_node',
        path: definition.path,
        message: `Generated PureData symbol '${definition.name}' is never consumed by a later statement.`,
      });
    }
  }
  return issues;
}

function collectGetReferences(value: unknown, used: Set<string>): void {
  if (Array.isArray(value)) {
    value.forEach((item) => collectGetReferences(item, used));
    return;
  }

  if (!isRecord(value)) {
    return;
  }

  if (value.kind === 'get') {
    const name = typeof value.name === 'string'
      ? value.name.trim()
      : typeof value.target === 'string'
        ? value.target.trim()
        : '';
    if (name.length > 0) {
      used.add(name.toLowerCase());
    }
  }

  for (const entry of Object.values(value)) {
    collectGetReferences(entry, used);
  }
}

function isObviousPureDataProducer(value: unknown): boolean {
  if (!isRecord(value)) {
    return false;
  }
  return value.kind === 'call';
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}
