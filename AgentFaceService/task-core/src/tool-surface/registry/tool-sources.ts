import { bridgeToolSource } from './bridge-tool-source.js';
import { localToolSource } from './local-tool-source.js';
import { taskToolSource } from './task-tool-source.js';
import type { ToolSource } from './tool-source.js';

export const toolSources: readonly ToolSource[] = [
  taskToolSource,
  localToolSource,
  bridgeToolSource,
];
