import * as fs from 'node:fs';
import * as path from 'node:path';
import { ancestorDirs } from './path-utils.js';

const agentGuideRelativePath = path.join(
  'BlueprintHelper',
  'Resources',
  'AgentGuide',
  '00_Agent_Onboarding_Index_20260504.md',
);
const agentGuidePackagedRelativePath = path.join(
  'Resources',
  'AgentGuide',
  '00_Agent_Onboarding_Index_20260504.md',
);
const agentGuideProjectPluginRelativePaths = [
  path.join('Plugins', 'BlueprintHelper', 'BlueprintHelper', agentGuidePackagedRelativePath),
  path.join('Plugins', 'BlueprintHelper', agentGuidePackagedRelativePath),
];

export function resolveAgentGuidePath(cwd: string): string | undefined {
  const candidateRoots = ancestorDirs(path.resolve(cwd));
  for (const root of candidateRoots) {
    const candidates = [
      path.join(root, agentGuideRelativePath),
      path.join(root, agentGuidePackagedRelativePath),
      ...agentGuideProjectPluginRelativePaths.map((relativePath) => path.join(root, relativePath)),
    ];
    for (const candidate of candidates) {
      if (fs.existsSync(candidate)) {
        return candidate;
      }
    }
  }
  return undefined;
}
