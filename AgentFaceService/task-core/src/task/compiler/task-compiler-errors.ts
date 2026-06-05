import type { TaskIssue } from '../schema/task-schemas.js';

export class TaskSpecCompileError extends Error {
  readonly code: string;
  readonly issues: TaskIssue[];

  constructor(code: string, message: string, issues: TaskIssue[]) {
    super(message);
    this.name = 'TaskSpecCompileError';
    this.code = code;
    this.issues = issues;
  }
}
