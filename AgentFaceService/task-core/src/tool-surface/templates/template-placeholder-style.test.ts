import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const PLUGIN_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../../..');
const TEMPLATE_ROOT = path.join(PLUGIN_ROOT, 'AgentFaceService/agent-guide/Templates');
const PLACEHOLDER_STYLE_PATTERN = /^__(REQUIRED|OPTIONAL)_[A-Z0-9_]+__$/;

test('agent-facing JSON templates use required or optional placeholder style markers', () => {
	const hits: string[] = [];
	let requiredCount = 0;
	let optionalCount = 0;

	for (const filePath of listJsonFiles(TEMPLATE_ROOT)) {
		const relativePath = normalizePath(path.relative(PLUGIN_ROOT, filePath));
		collectPlaceholderStyleViolations(JSON.parse(fs.readFileSync(filePath, 'utf8')), relativePath, '', hits, {
			onRequired: () => { requiredCount += 1; },
			onOptional: () => { optionalCount += 1; },
		});
	}

	assert.deepEqual(hits, []);
	assert.ok(requiredCount > 0, 'expected at least one required placeholder marker');
	assert.ok(optionalCount > 0, 'expected at least one optional placeholder marker');
});

function collectPlaceholderStyleViolations(
	value: unknown,
	filePath: string,
	pointer: string,
	hits: string[],
	counters: {
		onRequired: () => void;
		onOptional: () => void;
	},
): void {
	if (Array.isArray(value)) {
		value.forEach((item, index) =>
			collectPlaceholderStyleViolations(item, filePath, `${pointer}/${index}`, hits, counters));
		return;
	}
	if (value && typeof value === 'object') {
		for (const [key, child] of Object.entries(value)) {
			if (key.includes('__')) {
				recordPlaceholder(key, `${filePath}${pointer}/${key}`, hits, counters);
			}
			collectPlaceholderStyleViolations(child, filePath, `${pointer}/${key}`, hits, counters);
		}
		return;
	}
	if (typeof value === 'string' && value.includes('__')) {
		recordPlaceholder(value, `${filePath}${pointer}`, hits, counters);
	}
}

function recordPlaceholder(
	placeholder: string,
	location: string,
	hits: string[],
	counters: {
		onRequired: () => void;
		onOptional: () => void;
	},
): void {
	if (PLACEHOLDER_STYLE_PATTERN.test(placeholder)) {
		if (placeholder.startsWith('__REQUIRED_')) {
			counters.onRequired();
		} else {
			counters.onOptional();
		}
		return;
	}
	hits.push(`${location}=${placeholder}`);
}

function listJsonFiles(root: string): string[] {
	const entries = fs.readdirSync(root, { withFileTypes: true });
	return entries.flatMap((entry) => {
		const fullPath = path.join(root, entry.name);
		if (entry.isDirectory()) {
			return listJsonFiles(fullPath);
		}
		return entry.isFile() && entry.name.endsWith('.json') ? [fullPath] : [];
	});
}

function normalizePath(filePath: string): string {
	return filePath.replaceAll('\\', '/');
}
