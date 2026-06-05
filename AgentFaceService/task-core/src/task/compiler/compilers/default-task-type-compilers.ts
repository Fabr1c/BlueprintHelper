import { TaskTypeCompilerRegistry } from '../task-type-compiler-registry.js';
import { assetFactoryTaskCompiler } from './asset-factory-task-compiler.js';
import { blueprintClassSettingsTaskCompiler } from './blueprint-class-settings-task-compiler.js';
import { blueprintComponentsTaskCompiler } from './blueprint-components-task-compiler.js';
import { blueprintSignatureTaskCompiler } from './blueprint-signature-task-compiler.js';
import { blueprintVariablesTaskCompiler } from './blueprint-variables-task-compiler.js';
import { dataTableTaskCompiler } from './data-table-task-compiler.js';
import { objectPropertiesTaskCompiler } from './object-properties-task-compiler.js';
import { umgWidgetTaskCompiler } from './umg-widget-task-compiler.js';

export function createDefaultTaskTypeCompilerRegistry(): TaskTypeCompilerRegistry {
  return new TaskTypeCompilerRegistry()
    .register(assetFactoryTaskCompiler)
    .register(blueprintVariablesTaskCompiler)
    .register(objectPropertiesTaskCompiler)
    .register(blueprintSignatureTaskCompiler)
    .register(blueprintClassSettingsTaskCompiler)
    .register(blueprintComponentsTaskCompiler)
    .register(umgWidgetTaskCompiler)
    .register(dataTableTaskCompiler);
}
