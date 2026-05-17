# Smoke Bug - TaskSpec Data / UMG 2026-05-10

> 2026-05-14 状态转移：本文中的未达期待、待验证项和阻塞项已迁移到 [BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md](BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md)。本文保留为历史上下文；开放项跟踪迁移完成，后续当前状态以总账为准。

来源：`BlueprintHelper_NewProject_Full_SmokeRun_20260510.md`

本文记录 TaskSpec compiler、dry-run、DataTable、UMG 跨步骤预览问题。

## SMOKE-TS-20260510-01: UMG dry_run 无法解析同一 TaskSpec 内跨步骤 Widget 依赖

**优先级**：P1

**现象**

- Smoke 中 UMG 需要拆成 3 个步骤才成功：先创建 `RootCanvas`，再创建 `SmokeText`，最后设置 `SmokeText.Text`。
- 报告记录：UMG dry_run cannot resolve cross-step widget dependencies。

**实现证据**

- P1 compiler 对 `edit_umg_widget` 的每个 change 生成独立 TaskPlan step。
- 当前 dry-run 依赖 UE 当前资产状态，不模拟前序 planned step 的结果。
- `task-contract.ts` 将 UMG lowered 到 `add_widget` / `set_widget_property` / `remove_widget`，但没有声明事务级虚拟 WidgetTree 预览模型。

**初步根因**

dry-run 逐 step 校验时没有“计划内未来状态”。同一个 TaskSpec 中先创建的 widget，后续 property update 在 dry-run 视角里仍不存在。

**建议修复**

- 为 `edit_umg_widget` dry-run 增加虚拟 WidgetTree state。
- 同一 TaskSpec 内按顺序应用 create/update/delete 到虚拟状态，再统一输出 preview。
- 对 property update 的 target 缺失场景，区分 `missing_in_current_asset_but_created_by_plan` 和真实缺失。
- 增加 regression：`UmgDryRunResolvesWidgetCreatedEarlierInSameTaskSpec`。

## SMOKE-TS-20260510-02: DataTable dry_run 不能预览未来行的 update

**优先级**：P1

**现象**

- Smoke 中 DataTable 需要第三次尝试，把 add + update 合并或改写后才成功。
- 报告记录：dry_run cannot preview update on future rows。

**实现证据**

- P1 compiler 对 `behavior.rows[]` 每一行操作生成一个独立 TaskPlan step。
- `UpdateDataTableRow()` 在 dry-run 下仍先查真实 `DT->FindRowUnchecked(RowFName)`。
- 如果前序 step 是 add 同一 row，update dry-run 仍看不到该未来 row。

**初步根因**

DataTable dry-run 缺少 TaskSpec 级 row state 模拟。它只能验证当前资产中已存在的 row，不能验证计划中刚添加的 row。

**建议修复**

- DataTable compiler 或 preview adapter 合并同一 row 的 `add` + `update`。
- 或在 dry-run 中构建虚拟 row map，按 TaskSpec 顺序应用 add/update/delete。
- 增加 regression：`DataTableDryRunUpdatesRowAddedEarlierInSameTaskSpec`。

## SMOKE-TS-20260510-03: DataTable fields 只接受 string，数字 JSON 容易被丢弃或被错误拒绝

**优先级**：P2

**现象**

- Smoke 记录：DataTable int fields reject float JSON values。
- 对 Agent 来说，JSON 数字天然会以 number 传入，要求全部预先转成字符串不符合 TaskSpec 语义。

**实现证据**

- `BlueprintHelperDataTableBridgeRoutes.cpp` 的 `ReadDataTableRouteFieldsObject()` 只在 `TryGetString(Value)` 成功时写入字段。
- `FBlueprintHelperDataTableService::ApplyFieldsToRow()` 最终调用 `FProperty::ImportText_Direct()`，其输入是字符串。
- 当前桥接层没有把 JSON number / bool 转成 UE import text。

**初步根因**

TaskSpec 允许结构化 JSON 值，但 UE DataTable bridge 把字段输入建模成 `TMap<FString, FString>`，且转换逻辑只接受 JSON string。

**建议修复**

- Bridge 层支持 JSON string / number / bool 到 UE import text 的确定性转换。
- 对 int 字段，`55.0` 这类 integral float 可规范化为 `55`，非 integral float 应返回 typed issue。
- 对字段全被丢弃的情况，不应只报 `fields must contain at least one field`，要指出被丢弃字段和类型。
- 增加 regression：`DataTableBridgeConvertsJsonNumberFieldsToImportText`。
