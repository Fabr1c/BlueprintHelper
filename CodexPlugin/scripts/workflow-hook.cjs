#!/usr/bin/env node

const path = require('node:path');

const adapterPath = path.resolve(__dirname, '..', '..', 'AgentFaceService', 'hooks', 'codex-hook-adapter.cjs');
const { main } = require(adapterPath);

if (require.main === module) {
  main(process.argv.slice(2)).catch((error) => {
    const reason = error?.code ?? 'workflow_hook_wrapper_error';
    const message = error instanceof Error ? error.message : String(error);
    process.stdout.write(`${JSON.stringify({ action: 'block', reason, message })}\n`);
    process.stderr.write(`${message}\n`);
    process.exitCode = 2;
  });
}

module.exports = {
  adapterPath,
};
