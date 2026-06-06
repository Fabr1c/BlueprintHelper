import type { BlueprintVariableTaskPlanStep, TaskPlan } from '../../schema/task-schemas.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';

export function blueprintVariableTaskPlanToBridgePayload(
  taskPlan: TaskPlan,
  step: BlueprintVariableTaskPlanStep,
  dryRun: boolean,
): Record<string, unknown> {
  const lowerableAsBatchAdd = step.write.strategy === 'member_variables' &&
    step.write.ops.every((op) => op.op === 'ensure_member_variable');
  if (!lowerableAsBatchAdd) {
    return { task_plan: taskPlan };
  }

  if (step.write.strategy !== 'member_variables') {
    throw new TaskSpecCompileError('unsupported_variable_strategy', `Unsupported Blueprint Variable strategy: ${step.write.strategy}`, [
      {
        code: 'unsupported_variable_strategy',
        path: 'steps[0].write.strategy',
        message: 'Only member_variables can lower to add_blueprint_member_variables in this slice.',
      },
    ]);
  }

  return {
    asset_path: step.target.asset_path,
    variables: step.write.ops.map((op, index) => {
      if (op.op !== 'ensure_member_variable') {
        throw new TaskSpecCompileError('unsupported_variable_op', `Unsupported Blueprint Variable op: ${op.op}`, [
          {
            code: 'unsupported_variable_op',
            path: `steps[0].write.ops[${index}].op`,
            message: 'Only ensure_member_variable is supported in this slice.',
          },
        ]);
      }

      const { op: _op, ...payload } = op as Record<string, unknown>;
      return payload;
    }),
    dry_run: dryRun,
  };
}
