export interface EditorBlockedErrorClassification {
  readonly code: 'transport_connect_timeout' | 'unknown_mutation_state' | 'task_internal_error';
  readonly category: 'bridge_unavailable' | 'editor_command_blocked' | 'internal_error';
  readonly recommended_action: string;
}

export function classifyEditorBlockedError(error: unknown): EditorBlockedErrorClassification {
  const message = error instanceof Error ? error.message : String(error);

  if (/Bridge connection timed out/u.test(message)) {
    return {
      code: 'transport_connect_timeout',
      category: 'bridge_unavailable',
      recommended_action: 'Verify the Editor Bridge is running before retrying.',
    };
  }

  if (/Bridge request timed out/u.test(message)) {
    return {
      code: 'unknown_mutation_state',
      category: 'editor_command_blocked',
      recommended_action: 'Do not retry blindly. Let the editor settle, then run read_context and source-control status for target assets before deciding whether another write is required.',
    };
  }

  return {
    code: 'task_internal_error',
    category: 'internal_error',
    recommended_action: 'Inspect the task error and Bridge diagnostics.',
  };
}
