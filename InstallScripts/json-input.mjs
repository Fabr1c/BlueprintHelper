import { readFile } from 'node:fs/promises';

const UTF8_BOM = '\uFEFF';

export function stripJsonTextBom(text) {
  return text.startsWith(UTF8_BOM) ? text.slice(1) : text;
}

export function parseJsonText(text) {
  return JSON.parse(stripJsonTextBom(text));
}

export async function readJsonFile(filePath) {
  return parseJsonText(await readFile(filePath, 'utf8'));
}
