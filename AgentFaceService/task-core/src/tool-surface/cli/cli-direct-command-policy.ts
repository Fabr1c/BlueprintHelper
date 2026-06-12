export interface RemovedDirectCliToolCommand {
  readonly tool_name: string;
  readonly replacement_command: string;
  readonly reason: string;
}

const REMOVED_DIRECT_CLI_TOOL_COMMANDS: readonly RemovedDirectCliToolCommand[] = [
  removed('blueprinthelper_preview_task', 'bh task preview --file <task-spec.json>'),
  removed('blueprinthelper_execute_task', 'bh task execute --file <task-spec.json>'),
  removed('blueprinthelper_get_task_result', 'bh task result --id <task_run_id>'),
  removed('blueprinthelper_read_context', 'bh context read --file <read-spec.json>'),
  removed('blueprint_open_editor', 'mcp__blueprint_helper__blueprint_open_editor'),
  removed('open_editor', 'mcp__blueprint_helper__blueprint_open_editor'),
  removed('blueprint_close_editor', 'mcp__blueprint_helper__blueprint_close_editor'),
  removed('close_editor', 'mcp__blueprint_helper__blueprint_close_editor'),
];

export function getRemovedDirectCliToolCommand(toolName: string): RemovedDirectCliToolCommand | undefined {
  return REMOVED_DIRECT_CLI_TOOL_COMMANDS.find((entry) => entry.tool_name === toolName);
}

export function listRemovedDirectCliToolCommands(): readonly RemovedDirectCliToolCommand[] {
  return REMOVED_DIRECT_CLI_TOOL_COMMANDS;
}

function removed(toolName: string, replacementCommand: string): RemovedDirectCliToolCommand {
  return {
    tool_name: toolName,
    replacement_command: replacementCommand,
    reason: 'Direct tool-name CLI entry was removed from Agent-facing workflows; use the grouped command.',
  };
}
