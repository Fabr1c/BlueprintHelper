import * as fs from 'node:fs';

const UTF8_BOM = '\uFEFF';

export function stripJsonTextBom(text: string): string {
  return text.startsWith(UTF8_BOM) ? text.slice(1) : text;
}

export function parseJsonText(text: string): unknown {
  return JSON.parse(stripJsonTextBom(text));
}

export function readJsonFileText(filePath: string): string {
  return stripJsonTextBom(fs.readFileSync(filePath, 'utf8'));
}

export function readJsonFile(filePath: string): unknown {
  return JSON.parse(readJsonFileText(filePath));
}
