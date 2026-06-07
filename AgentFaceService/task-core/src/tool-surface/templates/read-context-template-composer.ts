import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import {
  listReadContextTemplateClusters,
  listReadContextTemplateDomains,
  listReadContextTemplateQuickAccess,
  listReadContextTemplateTargets,
  listReadContextTemplateViews,
} from './read-context-template-index.js';
import { getActiveReadContextRouteDescriptors } from './read-context-template-registry.js';
import type {
  ComposeReadContextTemplateInput,
  ReadContextRouteDescriptor,
  ReadContextTemplateCompositionResult,
  ReadContextTemplateDiagnostic,
} from './read-context-template-types.js';

export {
  listReadContextTemplateClusters,
  listReadContextTemplateDomains,
  listReadContextTemplateQuickAccess,
  listReadContextTemplateTargets,
  listReadContextTemplateViews,
};

export function composeReadContextTemplate(input: ComposeReadContextTemplateInput): ReadContextTemplateCompositionResult {
  if (input.templateIds.length > 1) {
    return failed(input, input.templateIds.map((templateId) => ({
      code: 'unsupported_template_set',
      domain: input.domain,
      read_cluster: input.readCluster,
      target_kind: input.targetKind,
      view_template: input.viewTemplate,
      template_id: templateId,
    })));
  }

  const route = findRoute(input);
  if (!route) {
    return failed(input, [{
      code: 'unsupported_read_context_template',
      domain: input.domain,
      read_cluster: input.readCluster,
      target_kind: input.targetKind,
      view_template: input.viewTemplate,
    }]);
  }

  const requestedTemplateId = input.templateIds[0];
  if (requestedTemplateId !== undefined && requestedTemplateId !== route.route_id) {
    return failed(input, [{
      code: 'unknown_quick_access_template',
      domain: input.domain,
      read_cluster: input.readCluster,
      target_kind: input.targetKind,
      view_template: input.viewTemplate,
      template_id: requestedTemplateId,
    }]);
  }

  const readSpec = composeReadSpec(route);
  writeJson(input.outputPath, readSpec);
  return ok(input, route);
}

function findRoute(input: ComposeReadContextTemplateInput): ReadContextRouteDescriptor | undefined {
  return getActiveReadContextRouteDescriptors().find((route) =>
    route.domain === input.domain
    && route.read_cluster === input.readCluster
    && route.target_kind === input.targetKind
    && route.view_template === input.viewTemplate);
}

function composeReadSpec(route: ReadContextRouteDescriptor): Record<string, unknown> {
  const template = readJson(pluginPath(route.base_template_path)) as Record<string, unknown>;
  const target: Record<string, unknown> = {
    ...readRecord(template['target']),
    asset_path: '__REQUIRED_ASSET_PATH__',
  };
  if (route.target_type) {
    target['target_type'] = route.target_type;
  }
  if (route.required_target_fields.includes('target_name')) {
    target['target_name'] = '__REQUIRED_TARGET_NAME__';
  }
  if (route.required_target_fields.includes('block_id')) {
    target['block_id'] = '__REQUIRED_BLOCK_ID__';
    delete target['target_name'];
  }

  const readSpec: Record<string, unknown> = {
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: route.read_type,
    target,
  };
  if (route.format) {
    readSpec['view'] = {
      format: toReadSpecFormat(route),
    };
  }
  return readSpec;
}

function toReadSpecFormat(route: ReadContextRouteDescriptor): string {
  if (route.view_template === 'tree_json' && route.format === 'logic_json') {
    return 'logic_json';
  }
  return route.format ?? route.view_template;
}

function ok(
  input: ComposeReadContextTemplateInput,
  route: ReadContextRouteDescriptor,
): ReadContextTemplateCompositionResult {
  const outputPath = normalizePath(path.resolve(input.outputPath));
  return {
    schema: 'BlueprintHelper.ReadContextTemplateComposition.v1',
    status: 'ok',
    domain: input.domain,
    read_cluster: input.readCluster,
    target_kind: input.targetKind,
    view_template: input.viewTemplate,
    template_id: route.route_id,
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
    domain: input.domain,
    read_cluster: input.readCluster,
    target_kind: input.targetKind,
    view_template: input.viewTemplate,
    diagnostics,
  };
}

function readJson(filePath: string): unknown {
  return JSON.parse(fs.readFileSync(filePath, 'utf8'));
}

function writeJson(filePath: string, value: unknown): void {
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
  fs.writeFileSync(filePath, `${JSON.stringify(value, null, 2)}\n`, 'utf8');
}

function pluginPath(relativePath: string): string {
  return path.resolve(pluginRoot(), relativePath);
}

function pluginRoot(): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../../..');
}

function readRecord(value: unknown): Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value)
    ? { ...(value as Record<string, unknown>) }
    : {};
}

function normalizePath(filePath: string): string {
  return filePath.replaceAll('\\', '/');
}
