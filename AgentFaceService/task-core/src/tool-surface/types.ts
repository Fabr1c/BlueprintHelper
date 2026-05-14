import type { BridgeClient } from '../bridge/bridge-client.js';
import type { ToolResultBase } from '../result/tool-result.js';
import type { TaskSpecRunner } from '../task/service/task-spec-runner.js';
import type { z } from 'zod';

export type ToolAudience = 'default' | 'compat' | 'expert';
export type ToolRisk = 'none' | 'low' | 'medium' | 'high' | 'critical';

export interface LocalProcessResult {
  exitCode: number;
  stdout: string;
  stderr: string;
}

export interface BlueprintHelperToolContext {
  cwd: string;
  bridge: BridgeClient;
  taskRunner: TaskSpecRunner;
  ueEngineDir?: string;
  runLocalProcess?: (command: string, args: string[], options?: {
    timeoutMs?: number;
    detached?: boolean;
    env?: NodeJS.ProcessEnv;
  }) => Promise<LocalProcessResult>;
  sleep?: (ms: number) => Promise<void>;
}

export interface BlueprintHelperToolDefinition {
  name: string;
  description: string;
  inputSchema: z.ZodTypeAny;
  audience: ToolAudience;
  risk: ToolRisk;
  requiresExpert?: boolean;
  execute(input: Record<string, unknown>, context: BlueprintHelperToolContext): Promise<ToolResultBase>;
}
