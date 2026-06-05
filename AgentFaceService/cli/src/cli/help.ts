import { createHelpBuilder } from './help-builder.js';

export function buildHelpText(target: string[] = []): string {
  return createHelpBuilder().build(target);
}
