import type { TaskPlan, TaskSpec } from '../schema/task-schemas.js';

export type TaskTypeCompilerId =
  | 'asset_factory'
  | 'blueprint_variables'
  | 'object_properties'
  | 'blueprint_signature'
  | 'blueprint_class_settings'
  | 'blueprint_components'
  | 'umg_widget'
  | 'data_table'
  | 'graphwrite_legacy'
  | 'composite_feature_legacy';

export interface TaskTypeCompileContext {
  readonly source: 'facade' | 'strategy';
}

export interface TaskTypeCompiler<TTaskSpec extends TaskSpec = TaskSpec> {
  readonly id: TaskTypeCompilerId;
  readonly taskType: TaskSpec['task_type'];
  canCompile(taskSpec: TaskSpec): taskSpec is TTaskSpec;
  compile(taskSpec: TTaskSpec, context: TaskTypeCompileContext): TaskPlan;
}
