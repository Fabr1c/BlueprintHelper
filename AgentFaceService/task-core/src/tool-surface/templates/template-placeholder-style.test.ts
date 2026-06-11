import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const PLUGIN_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../../..');
const TEMPLATE_ROOT = path.join(PLUGIN_ROOT, 'AgentFaceService/agent-guide/Templates');
const PLACEHOLDER_STYLE_PATTERN = /^__(REQUIRED|OPTIONAL)_[A-Z0-9_]+__$/;
const REFERENCE_CONTEXT_SCHEMA = 'BlueprintHelper.ReferenceContextRequest.v1';

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

test('enum-like placeholders expose allowed values or an authoritative source hint', () => {
	const expectations: Array<{
		file: string;
		placeholder: string;
		requiredEvidenceKeys: string[];
	}> = [
		{
			file: 'write/slots/graph_statement_container_action_template.json',
			placeholder: '__REQUIRED_CONTAINER_KIND__',
			requiredEvidenceKeys: ['container.kind.allowed_values'],
		},
		{
			file: 'write/slots/graph_statement_container_action_template.json',
			placeholder: '__REQUIRED_OPERATION__',
			requiredEvidenceKeys: [
				'container.operation.allowed_values.array',
				'container.operation.allowed_values.map',
				'container.operation.allowed_values.set',
			],
		},
		{
			file: 'write/slots/graph_statement_control_switch_template.json',
			placeholder: '__REQUIRED_SWITCH_OPERATION__',
			requiredEvidenceKeys: ['generic.control.operation.allowed_values'],
		},
		{
			file: 'write/slots/graph_statement_create_template.json',
			placeholder: '__REQUIRED_CREATE_OPERATION__',
			requiredEvidenceKeys: ['generic.create.operation.allowed_values'],
		},
		{
			file: 'write/slots/graph_statement_convert_template.json',
			placeholder: '__REQUIRED_TRANSFORM_OPERATION__',
			requiredEvidenceKeys: ['generic.transform.operation.allowed_values'],
		},
		{
			file: 'write/slots/graph_statement_schedule_template.json',
			placeholder: '__REQUIRED_SCHEDULE_OPERATION__',
			requiredEvidenceKeys: ['generic.schedule.operation.allowed_values'],
		},
		{
			file: 'write/slots/graph_expression_op_template.json',
			placeholder: '__REQUIRED_OPERATION_ID__',
			requiredEvidenceKeys: ['op.operation_id.source'],
		},
		{
			file: 'write/slots/graph_statement_delegate_bind_template.json',
			placeholder: '__REQUIRED_BINDING_OBJECT_KIND__',
			requiredEvidenceKeys: ['event_delegate.binding_object_kind.allowed_values'],
		},
	];

	for (const expectation of expectations) {
		const templatePath = path.join(TEMPLATE_ROOT, ...expectation.file.split('/'));
		const template = JSON.parse(fs.readFileSync(templatePath, 'utf8'));
		const serialized = JSON.stringify(template);
		assert.equal(
			serialized.includes(expectation.placeholder),
			true,
			`${expectation.file} should keep ${expectation.placeholder}`,
		);
		const contextEvidence = readContextEvidence(template);
		for (const key of expectation.requiredEvidenceKeys) {
			assert.equal(
				Object.prototype.hasOwnProperty.call(contextEvidence, key),
				true,
				`${expectation.file} should expose ${key}`,
			);
			assert.equal(
				typeof contextEvidence[key],
				'string',
				`${expectation.file} ${key} should be agent-readable text`,
			);
		}
	}
});

test('ReferenceContext request templates declare the schema-rooted request contract', () => {
  const referenceTemplatePaths = listJsonFiles(path.join(TEMPLATE_ROOT, 'read'))
    .filter((filePath) => path.basename(filePath).startsWith('blueprinthelper_read_reference_context_'));

  assert.ok(referenceTemplatePaths.length > 0, 'expected ReferenceContext templates to exist');
  for (const templatePath of referenceTemplatePaths) {
    const relativePath = normalizePath(path.relative(TEMPLATE_ROOT, templatePath));
    const template = JSON.parse(fs.readFileSync(templatePath, 'utf8')) as Record<string, unknown>;
    assert.equal(
      template.schema,
      REFERENCE_CONTEXT_SCHEMA,
      `${relativePath} should expose ${REFERENCE_CONTEXT_SCHEMA}`,
    );
  }
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

function readContextEvidence(template: unknown): Record<string, unknown> {
	if (template && typeof template === 'object' && !Array.isArray(template)) {
		const contextEvidence = (template as Record<string, unknown>).context_evidence;
		if (contextEvidence && typeof contextEvidence === 'object' && !Array.isArray(contextEvidence)) {
			return contextEvidence as Record<string, unknown>;
		}
	}
	return {};
}
