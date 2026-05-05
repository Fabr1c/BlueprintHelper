# 04 - TaskSpec 修改蓝图工作流

标准流程：

```text
1. get_runtime_profile
2. read_context / read_reference_context as needed
3. build TaskSpec
4. preview_task
5. 如果 context_required/context_stale：重新 read_context / read_reference_context
6. 如果 TaskSpec error：按 suggested_patch 修正
7. 如果 preview_blocked：stop_and_report 或修改 TaskSpec
8. execute_task
9. get_task_result if needed
10. report summary
```

TaskSpec 必须描述：目标资产、feature_name、scope_policy、asset_policy、resources、components、variables、class_settings、behavior、validation。

execute_task 成功后，普通报告只输出任务摘要、目标资产、主要变更、编译/保存/未完成项，不展开完整 TaskPlan、child transaction、Journal 路径或底层 Bridge JSON。
