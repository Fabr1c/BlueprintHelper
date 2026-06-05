import type { TaskSpec } from '../schema/task-schemas.js';
import { TaskSpecCompileError } from './task-compiler-errors.js';
import type { TaskTypeCompiler } from './task-type-compiler.js';

export class TaskTypeCompilerRegistry {
  private readonly compilers = new Map<TaskSpec['task_type'], TaskTypeCompiler>();

  register(compiler: TaskTypeCompiler): this {
    if (this.compilers.has(compiler.taskType)) {
      throw new Error(`Task type compiler is already registered: ${compiler.taskType}`);
    }
    this.compilers.set(compiler.taskType, compiler);
    return this;
  }

  has(taskType: string): boolean {
    return this.compilers.has(taskType as TaskSpec['task_type']);
  }

  get(taskType: string): TaskTypeCompiler | undefined {
    return this.compilers.get(taskType as TaskSpec['task_type']);
  }

  requireForTaskSpec(taskSpec: TaskSpec): TaskTypeCompiler {
    const compiler = this.get(taskSpec.task_type);
    if (compiler?.canCompile(taskSpec)) {
      return compiler;
    }
    throw new TaskSpecCompileError('unsupported_task_type', `Unsupported TaskSpec task_type: ${taskSpec.task_type}`, [
      {
        code: 'unsupported_task_type',
        path: 'task_type',
        message: `No TaskTypeCompiler is registered for ${taskSpec.task_type}.`,
      },
    ]);
  }

  list(): TaskTypeCompiler[] {
    return Array.from(this.compilers.values());
  }
}
