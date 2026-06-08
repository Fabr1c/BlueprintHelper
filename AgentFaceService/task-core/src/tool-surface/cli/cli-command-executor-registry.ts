import { listCliCommandDescriptors } from './cli-command-descriptor.js';

export interface CliCommandExecutorDescriptor<TKind extends string = string> {
  readonly id: string;
  readonly kinds: readonly TKind[];
}

export function listCliCommandKindsByExecutorId(executorId: string): string[] {
  return Array.from(new Set(listCliCommandDescriptors()
    .filter((descriptor) => descriptor.executor_id === executorId)
    .map((descriptor) => descriptor.kind)));
}

export function resolveCliCommandExecutorDescriptor<
  TKind extends string,
  TExecutor extends CliCommandExecutorDescriptor<TKind>,
>(executors: readonly TExecutor[], commandKind: TKind): TExecutor | undefined {
  return executors.find((executor) => executor.kinds.includes(commandKind));
}
