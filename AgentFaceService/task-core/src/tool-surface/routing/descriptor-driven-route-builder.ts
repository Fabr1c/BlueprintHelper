import {
  CapabilityDescriptorSchema,
  RuntimeCapabilityStateSchema,
  type CapabilityDescriptor,
  type RuntimeCapabilityState,
} from '../capabilities/capability-descriptor.schema.js';

export type DescriptorDrivenRouteMode = 'preview' | 'execute' | 'bridge';

export interface DescriptorDrivenRouteBuilderInput {
  readonly descriptor: CapabilityDescriptor;
  readonly runtime: RuntimeCapabilityState;
  readonly command: Record<string, unknown>;
  readonly mode: DescriptorDrivenRouteMode;
}

export interface DescriptorDrivenRouteDescriptorRefs {
  readonly review_evidence_adapter?: string;
  readonly review_restore_adapter?: string;
  readonly review_surface_adapter?: string;
  readonly debug_projection_adapter?: string;
  readonly debug_export_projection?: string;
  readonly read_context_projection_adapter?: string;
  readonly read_context_route_refs: readonly string[];
  readonly ui_presenter_adapter?: string;
  readonly ui_surface?: string;
}

export interface DescriptorDrivenRoutePlan {
  readonly capability_id: string;
  readonly family: string;
  readonly operation: string;
  readonly handler_id: string;
  readonly cli_command: string;
  readonly bridge_command?: string;
  readonly runtime_adapter_id: string;
  readonly mode: DescriptorDrivenRouteMode;
  readonly preview_supported: boolean;
  readonly preview_required_for_execute: boolean;
  readonly payload: Record<string, unknown>;
  readonly descriptor_refs: DescriptorDrivenRouteDescriptorRefs;
}

export type DescriptorDrivenRouteUnavailableReason =
  | 'capability_reserved'
  | 'runtime_status_unavailable'
  | 'runtime_adapter_unregistered'
  | 'write_capability_disabled'
  | 'high_risk_capability_disabled';

export type DescriptorDrivenRouteBuildResult =
  | { readonly ok: true; readonly plan: DescriptorDrivenRoutePlan }
  | {
      readonly ok: false;
      readonly status: 'capability_unavailable';
      readonly error_code: 'capability_unavailable';
      readonly reason: DescriptorDrivenRouteUnavailableReason;
      readonly capability_id: string;
      readonly runtime_adapter_id: string;
      readonly runtime_status: CapabilityDescriptor['runtime']['status'];
      readonly message: string;
    };

export function buildDescriptorDrivenRoute(
  input: DescriptorDrivenRouteBuilderInput,
): DescriptorDrivenRouteBuildResult {
  const descriptor = CapabilityDescriptorSchema.parse(input.descriptor);
  const runtime = RuntimeCapabilityStateSchema.parse(input.runtime);
  const unavailable = getUnavailableResult(descriptor, runtime);
  if (unavailable) {
    return unavailable;
  }

  return {
    ok: true,
    plan: {
      capability_id: descriptor.id,
      family: descriptor.family,
      operation: descriptor.operation,
      handler_id: descriptor.routing.handler_id,
      cli_command: descriptor.routing.cli_command,
      bridge_command: descriptor.routing.bridge_command,
      runtime_adapter_id: descriptor.runtime.adapter_id,
      mode: input.mode,
      preview_supported: descriptor.preview.supported,
      preview_required_for_execute: descriptor.preview.required_for_execute,
      payload: buildRoutePayload(input.command),
      descriptor_refs: {
        review_evidence_adapter: descriptor.review.evidence_adapter,
        review_restore_adapter: descriptor.review.restore_adapter,
        review_surface_adapter: descriptor.review.surface_adapter,
        debug_projection_adapter: descriptor.debug.projection_adapter,
        debug_export_projection: descriptor.debug.export_projection,
        read_context_projection_adapter: descriptor.read_context.projection_adapter,
        read_context_route_refs: descriptor.read_context.route_refs,
        ui_presenter_adapter: descriptor.ui.presenter_adapter,
        ui_surface: descriptor.ui.surface,
      },
    },
  };
}

function getUnavailableResult(
  descriptor: CapabilityDescriptor,
  runtime: RuntimeCapabilityState,
): Extract<DescriptorDrivenRouteBuildResult, { ok: false }> | undefined {
  if (descriptor.safety.reserved_only) {
    return unavailable(descriptor, 'capability_reserved');
  }
  if (descriptor.runtime.status !== 'active') {
    return unavailable(descriptor, 'runtime_status_unavailable');
  }
  if (!runtime.registered_runtime_adapter_ids.includes(descriptor.runtime.adapter_id)) {
    return unavailable(descriptor, 'runtime_adapter_unregistered');
  }
  if (descriptor.safety.write_approval_required && !runtime.allow_write_capabilities) {
    return unavailable(descriptor, 'write_capability_disabled');
  }
  if (descriptor.safety.risk === 'high' && !runtime.allow_high_risk_capabilities) {
    return unavailable(descriptor, 'high_risk_capability_disabled');
  }
  return undefined;
}

function unavailable(
  descriptor: CapabilityDescriptor,
  reason: DescriptorDrivenRouteUnavailableReason,
): Extract<DescriptorDrivenRouteBuildResult, { ok: false }> {
  return {
    ok: false,
    status: 'capability_unavailable',
    error_code: 'capability_unavailable',
    reason,
    capability_id: descriptor.id,
    runtime_adapter_id: descriptor.runtime.adapter_id,
    runtime_status: descriptor.runtime.status,
    message: `BlueprintHelper capability unavailable: ${descriptor.id} (${reason}).`,
  };
}

function buildRoutePayload(command: Record<string, unknown>): Record<string, unknown> {
  const payload: Record<string, unknown> = {};
  for (const [key, value] of Object.entries(command)) {
    if (value !== undefined) {
      payload[normalizePayloadKey(key)] = value;
    }
  }
  return payload;
}

function normalizePayloadKey(key: string): string {
  if (key === 'previewToken') {
    return 'preview_token';
  }
  if (key === 'bridgeCommand') {
    return 'bridge_command';
  }
  return key;
}
