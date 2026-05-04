# TaskSpec-first Workflow

1. 读取 runtime_profile。
2. 读取 TaskContextPack。
3. 生成 BlueprintHelper.TaskSpec.v1。
4. 调用 preview_task。
5. 使用 suggested_patch 修正 TaskSpec，或 stop_and_report。
6. preview passed 后调用 execute_task。
7. 最终报告任务级摘要。

Agent 不生成 transaction_id、block_id 或 TaskPlan step 的底层细节。
