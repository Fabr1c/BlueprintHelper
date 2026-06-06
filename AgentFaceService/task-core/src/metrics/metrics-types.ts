export type MetricsEventType =
  | 'tool_invoked'
  | 'tool_completed'
  | 'taskspec_preview_attempted'
  | 'taskspec_preview_completed'
  | 'taskspec_execute_attempted'
  | 'taskspec_execute_completed'
  | 'taskstep_completed'
  | 'validation_completed'
  | 'readback_completed'
  | 'cli_io_completed';

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
  fallback?: boolean;
}

export interface MetricsIssueSummary {
  code?: string;
  path?: string;
  message_digest?: string;
}

export type MetricsIoInputSource =
  | 'json'
  | 'stdin'
  | 'file'
  | 'task_file'
  | 'none';

export interface MetricsIoSummary {
  input_source?: MetricsIoInputSource;
  input_chars?: number;
  input_utf8_bytes?: number;
  output_chars?: number;
  output_utf8_bytes?: number;
  estimated_input_tokens?: number;
  estimated_output_tokens?: number;
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
  io?: MetricsIoSummary;
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
