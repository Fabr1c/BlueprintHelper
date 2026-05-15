export function readRequiredString(input: Record<string, unknown>, field: string): string {
  const value = input[field];
  if (typeof value !== 'string' || value.length === 0) {
    throw new Error(`${field} is required.`);
  }
  return value;
}

export function readOptionalString(input: Record<string, unknown>, field: string): string | undefined {
  const value = input[field];
  return typeof value === 'string' && value.length > 0 ? value : undefined;
}

export function readOptionalBoolean(input: Record<string, unknown>, field: string): boolean | undefined {
  const value = input[field];
  return typeof value === 'boolean' ? value : undefined;
}

export function readOptionalNumber(input: Record<string, unknown>, field: string): number | undefined {
  const value = input[field];
  return typeof value === 'number' && Number.isFinite(value) ? value : undefined;
}

export function readOptionalStringArray(input: Record<string, unknown>, field: string): string[] {
  const value = input[field];
  if (value === undefined || value === null) {
    return [];
  }
  if (typeof value === 'string') {
    const trimmed = value.trim();
    return trimmed.length > 0 ? trimmed.split(/\s+/) : [];
  }
  if (!Array.isArray(value)) {
    throw new Error(`${field} must be a string array.`);
  }
  return value.map((item, index) => {
    if (typeof item !== 'string') {
      throw new Error(`${field}[${index}] must be a string.`);
    }
    return item;
  });
}
