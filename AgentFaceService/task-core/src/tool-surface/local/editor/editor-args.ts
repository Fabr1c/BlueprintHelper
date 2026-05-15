import * as fs from 'node:fs';
import * as path from 'node:path';
import { toUnrealPath } from '../path-utils.js';

export function buildDefaultEditorArgs(uprojectFile: string, explicitArgs: string[]): string[] {
  const projectDir = path.dirname(uprojectFile);
  const shaderWorkingDir = path.join(projectDir, 'Intermediate', 'Shaders', 'WorkingDirectory', 'BlueprintHelperCli');
  fs.mkdirSync(shaderWorkingDir, { recursive: true });

  const args = [...explicitArgs];
  if (!hasSwitch(args, '-DDC-ForceMemoryCache') && !hasSwitchPrefix(args, '-ddc=')) {
    args.unshift('-DDC-ForceMemoryCache');
  }
  if (!hasSwitchPrefix(args, '-ShaderWorkingDir=')) {
    args.push(`-ShaderWorkingDir=${toUnrealPath(shaderWorkingDir)}/`);
  }
  if (!hasSwitch(args, '-NoSplash')) {
    args.push('-NoSplash');
  }
  return args;
}

function hasSwitch(args: string[], flag: string): boolean {
  return args.some((arg) => arg.toLowerCase() === flag.toLowerCase());
}

function hasSwitchPrefix(args: string[], prefix: string): boolean {
  return args.some((arg) => arg.toLowerCase().startsWith(prefix.toLowerCase()));
}
