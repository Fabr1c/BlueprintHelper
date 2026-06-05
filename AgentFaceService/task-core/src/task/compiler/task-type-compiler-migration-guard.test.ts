import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const forbiddenLegacyTaskTypeBranches = [
  'create_asset',
  'edit_blueprint_variables',
  'edit_object_properties',
  'edit_blueprint_signature',
  'edit_blueprint_class_settings',
  'edit_blueprint_components',
  'edit_umg_widget',
  'edit_data_table',
];

test('P3 non-GraphWrite task types do not branch in legacy task-compiler facade', () => {
  const source = fs.readFileSync(sourcePath('task-compiler.ts'), 'utf8');

  for (const taskType of forbiddenLegacyTaskTypeBranches) {
    assert.doesNotMatch(
      source,
      new RegExp(`['"]${taskType}['"]`),
      `${taskType} must be registered in TaskTypeCompilerRegistry, not named in task-compiler.ts`,
    );
  }
});

test('P3 legacy compiler facade does not reintroduce alternate non-GraphWrite routing forms', () => {
  const source = fs.readFileSync(sourcePath('task-compiler.ts'), 'utf8');

  assert.doesNotMatch(source, /switch\s*\([^)]*task_type[^)]*\)/);
  assert.doesNotMatch(source, /case\s+['"][^'"]+['"]\s*:/);
  assert.doesNotMatch(source, /includes\s*\([^)]*task_type[^)]*\)/);
});

test('legacy facade only names GraphWrite and composite feature task types during P3', () => {
  const source = fs.readFileSync(sourcePath('task-compiler.ts'), 'utf8');

  assert.match(source, /edit_blueprint_graph/);
  assert.match(source, /create_blueprint_feature/);
});

function sourcePath(fileName: string): string {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', '..', 'src', 'task', 'compiler', fileName);
}
