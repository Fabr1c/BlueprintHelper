#!/usr/bin/env node
import { writeGraphWriteGeneralitySpecs } from './graphwrite-generality-spec-factory.js';

const args = new Map<string, string>();
for (let index = 2; index < process.argv.length; index += 2) {
  args.set(process.argv[index], process.argv[index + 1]);
}

writeGraphWriteGeneralitySpecs({
  assetPath: args.get('--asset') ?? '/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality',
  graphName: args.get('--graph') ?? 'EG_GraphWriteGenerality',
  outDir: args.get('--out') ?? 'Saved/Automation/GraphWriteGenerality/specs',
  operationIds: (args.get('--operations') ?? '')
    .split(',')
    .map((operationId) => operationId.trim())
    .filter((operationId) => operationId.length > 0),
});
