import fs from 'node:fs';
import path from 'node:path';

import {
  listReadContextTemplateClusters,
  listReadContextTemplateFamilies,
  listReadContextTemplates,
} from './read-context-template-index.js';
import { getReadContextRouteDescriptor } from './read-context-template-registry.js';
import type {
  ComposeReadContextTemplateInput,
  ReadContextRouteDescriptor,
  ReadContextTemplateCompositionResult,
  ReadContextTemplateDiagnostic,
} from './read-context-template-types.js';

export {
  listReadContextTemplateClusters,
  listReadContextTemplateFamilies,
  listReadContextTemplates,
};

export function composeReadContextTemplate(input: ComposeReadContextTemplateInput): ReadContextTemplateCompositionResult {
  const route = getReadContextRouteDescriptor(input.templateId);
  if (!route || route.status !== 'active') {
    return failed(input, [{
      code: 'unknown_template_id',
      template_id: input.templateId,
    }]);
  }

  const readSpec = cloneJson(route.read_spec) as Record<string, unknown>;
  writeJson(input.outputPath, readSpec);
  return ok(input, route);
}

function ok(
  input: ComposeReadContextTemplateInput,
  route: ReadContextRouteDescriptor,
): ReadContextTemplateCompositionResult {
  const outputPath = normalizePath(path.resolve(input.outputPath));
  return {
    schema: 'BlueprintHelper.ReadContextTemplateComposition.v1',
    status: 'ok',
    template_id: route.template_id,
    family: route.family,
    cluster: route.cluster,
    output_path: outputPath,
    next: {
      read_command: `bh context read --file ${outputPath} --format json`,
    },
  };
}

function failed(
  input: ComposeReadContextTemplateInput,
  diagnostics: ReadContextTemplateDiagnostic[],
): ReadContextTemplateCompositionResult {
  return {
    schema: 'BlueprintHelper.ReadContextTemplateComposition.v1',
    status: 'failed',
    template_id: input.templateId,
    diagnostics,
  };
}

function writeJson(filePath: string, value: unknown): void {
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
  fs.writeFileSync(filePath, `${JSON.stringify(value, null, 2)}\n`, 'utf8');
}

function cloneJson(value: unknown): unknown {
  return JSON.parse(JSON.stringify(value)) as unknown;
}

function normalizePath(filePath: string): string {
  return filePath.replaceAll('\\', '/');
}
