export const localToolNameList = [
  'blueprinthelper_read_agent_guide',
  'blueprinthelper_diagnostics',
  'blueprint_build_project',
  'blueprint_open_editor',
  'blueprint_close_editor',
] as const;

export type LocalToolName = (typeof localToolNameList)[number];

export const localToolNames: ReadonlySet<string> = new Set(localToolNameList);
