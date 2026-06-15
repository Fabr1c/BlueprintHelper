#!/usr/bin/env node

const path = require('node:path');

const adapterPath = path.resolve(__dirname, '..', '..', 'AgentFaceService', 'hooks', 'claude-hook-adapter.cjs');
const { main } = require(adapterPath);

if (require.main === module) {
  main(process.argv.slice(2)).catch((error) => {
    const message = error instanceof Error ? error.message : String(error);
    process.stdout.write(`${JSON.stringify({ decision: 'block', reason: message })}\n`);
    process.exitCode = 0;
  });
}

module.exports = {
  adapterPath,
};
