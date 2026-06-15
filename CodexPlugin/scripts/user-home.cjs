const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

function getWindowsUsersRoot() {
  const systemDrive = (process.env.SystemDrive || 'C:').replace(/[\\/]$/u, '');
  return `${systemDrive}\\Users`;
}

function addCandidate(candidates, candidate) {
  if (typeof candidate === 'string' && candidate.trim()) {
    candidates.push(candidate.trim());
  }
}

function isUnderDirectory(candidate, directory) {
  const relative = path.relative(directory, candidate);
  return Boolean(relative) && !relative.startsWith('..') && !path.isAbsolute(relative);
}

function resolveBlueprintHelperUserHome() {
  const candidates = [];
  const usersRoot = getWindowsUsersRoot();
  if (fs.existsSync(usersRoot)) {
    addCandidate(candidates, path.join(usersRoot, process.env.USERNAME || ''));
  }
  addCandidate(candidates, process.env.USERPROFILE);
  addCandidate(candidates, process.env.HOME);
  addCandidate(candidates, os.homedir());

  const seen = new Set();
  for (const candidate of candidates) {
    const resolved = path.resolve(candidate);
    const key = resolved.toLowerCase();
    if (seen.has(key)) {
      continue;
    }
    seen.add(key);

    if (!fs.existsSync(resolved) || !fs.statSync(resolved).isDirectory()) {
      continue;
    }

    if (process.platform === 'win32' && fs.existsSync(usersRoot)) {
      if (!isUnderDirectory(resolved, usersRoot)) {
        continue;
      }

      if (process.env.USERNAME && path.basename(resolved).toLowerCase() !== process.env.USERNAME.toLowerCase()) {
        continue;
      }
    }

    return resolved;
  }

  throw new Error(`Unable to resolve Windows user home directory under ${usersRoot} for BlueprintHelper Codex/Claude config.`);
}

module.exports = {
  getWindowsUsersRoot,
  resolveBlueprintHelperUserHome,
};
