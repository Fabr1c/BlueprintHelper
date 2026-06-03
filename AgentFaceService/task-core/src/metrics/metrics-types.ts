export type MetricsEventType =
  | 'tool_invoked'
  | 'tool_completed'
  | 'taskspec_preview_attempted'
  | 'taskspec_preview_completed'
  | 'taskspec_execute_attempted'
  | 'taskspec_execute_completed'
  | 'taskstep_completed'
  | 'validation_completed'
  | 'readback_completed';

export type MetricsErrorCategory =
  | 'capability_boundary'
  | 'parameter_error'
  | 'context_error'
  | 'runtime_state_error'
  | 'unknown';

export type MetricsStatus =
  | 'success'
  | 'failed'
  | 'blocked'
  | 'pending_confirmation';

export interface MetricsTaskKey {
  task_type: string;
  feature_name?: string;
  target_type: string;
  target_ref_hash: string;
  target_ref_label?: string;
}

export interface MetricsOperationIdentity {
  capability?: string;
  semantic_operation?: string;
}

export interface MetricsIssueSummary {
  code?: string;
  path?: string;
  message_digest?: string;
}

export interface MetricsEvent extends MetricsOperationIdentity {
  schema: 'BlueprintHelper.MetricsEvent.v1';
  timestamp: string;
  event_type: MetricsEventType;
  tool_name?: string;
  task_key?: MetricsTaskKey;
  task_spec_hash?: string;
  status: MetricsStatus;
  error_category?: MetricsErrorCategory;
  error_code?: string;
  issue?: MetricsIssueSummary;
  duration_ms?: number;
  correctness_basis?: 'validation_readback' | 'pending_confirmation' | 'not_applicable';
}

export interface MetricsEventSink {
  record(event: MetricsEvent): void | Promise<void>;
}

export const NOOP_METRICS_SINK: MetricsEventSink = {
  record() {
    return undefined;
  },
};
