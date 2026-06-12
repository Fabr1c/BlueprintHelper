export const localToolNameList = [
  'blueprinthelper_read_agent_guide',
  'blueprinthelper_diagnostics',
  'blueprint_build_project',
] as const;

export type LocalToolName = (typeof localToolNameList)[number];

export const localToolNames: ReadonlySet<string> = new Set(localToolNameList);
