import * as fs from 'node:fs';
import * as path from 'node:path';
import { fileURLToPath } from 'node:url';
import { ancestorDirs } from './path-utils.js';

const agentGuideIndexFileName = '00_Agent_Onboarding_Index.md';
const agentFaceServiceAgentGuideRelativePath = path.join(
  'AgentFaceService',
  'agent-guide',
  agentGuideIndexFileName,
);
const localAgentGuideRelativePath = path.join('agent-guide', agentGuideIndexFileName);
const moduleAgentGuidePath = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  '..',
  '..',
  '..',
  '..',
  'agent-guide',
  agentGuideIndexFileName,
);

export function resolveAgentGuidePath(cwd: string): string | undefined {
  const candidateRoots = ancestorDirs(path.resolve(cwd));
  for (const root of candidateRoots) {
    const candidates = [
      path.join(root, agentFaceServiceAgentGuideRelativePath),
      path.join(root, localAgentGuideRelativePath),
    ];
    for (const candidate of candidates) {
      if (fs.existsSync(candidate)) {
        return candidate;
      }
    }
  }
  if (fs.existsSync(moduleAgentGuidePath)) {
    return moduleAgentGuidePath;
  }
  return undefined;
}
