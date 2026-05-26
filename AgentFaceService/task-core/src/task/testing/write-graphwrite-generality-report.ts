#!/usr/bin/env node
import { writeGraphWriteGeneralityReport } from './graphwrite-generality-report.js';

const args = new Map<string, string>();
for (let index = 2; index < process.argv.length; index += 2) {
  args.set(process.argv[index], process.argv[index + 1]);
}

writeGraphWriteGeneralityReport({
  runRoot: args.get('--run') ?? '',
  reportDir: args.get('--report') ?? 'BlueprintHelper/Develop/Report',
  dateStamp: args.get('--date'),
});
