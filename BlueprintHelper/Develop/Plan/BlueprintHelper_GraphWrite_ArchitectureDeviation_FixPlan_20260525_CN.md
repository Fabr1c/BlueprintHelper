# BlueprintHelper GraphWrite ArchitectureDeviation FixPlan 2026-05-25

## Summary

本文档承接 `BlueprintHelper_GraphWrite_ArchitectureDeviation_Audit_20260525_CN.md` 的第一批修复分发，只执行审计项 4 和 6。审计项 1、2、3、5 进入讨论门禁 backlog，不在本批修改代码行为。

本批约束：

- 不回写污染原审计文档；本文件记录执行范围、延期原因和验证计划。
- `Patch ConnectPins` 与审计项 1 有部分重叠，等 Merge/Patch ownership、Merge callable、EventDelegate handler scan 完成讨论后再决定。
- 本批不做旧 Agent、旧字段、旧工具兼容。

## Immediate Implementation

### 4. GraphWrite runtime Review evidence

状态：已完成。

目标：

- `FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence` 产出 cluster-owned evidence。
- 不启用 runtime fallback evidence。
- 成功 GraphWrite step 且 payload 同时有 asset 和 graph 时，产出 1 个 Graph surface atomic target。

目标 evidence：

- `TargetKind=graph_block`
- `TargetKey=graph_block:<graph_name>`
- `GraphName=<graph_name>`
- `OperationKind=<adapter_operation>`
- `TaskStepIndex=<step_index>`
- `AssetPath=<asset_path>`

失败条件：

- step result 失败时返回 `false`。
- payload 缺少 asset 或 graph 时返回 `false`。

验证：

- `BlueprintHelper.TaskRuntime.ClusterHub` 覆盖成功 GraphWrite step 的 Review evidence。
- 同一测试覆盖失败 step、缺 asset、缺 graph 不产出 evidence。

### 6. Public parsed DTO cleanup

状态：已完成。

目标：

- `BlueprintGraphWriteFacade.h` 不再暴露 parsed DTO、legacy JSON generation facade API、local variable parsed API。
- parsed DTO 收敛到 GraphWrite private pipeline 边界。
- 非 legacy result/display 类型拆到明确 public result header。
- UI、ImportService、SignatureService、测试调用点直接依赖当前 pipeline/service 边界，不通过 facade 暗示旧 parsed API 是主线。

必须移除的 public facade/API 残留：

- `EParsedBlueprintNodeType`
- `FParsedPinType`
- `FParsedLocalVariableDeclaration`
- `FParsed*` DTO
- `GenerateBlueprintFromJson`
- `GenerateMultiGraphFromJson`
- `EnsureLocalVariableExists(FParsed...)`
- `ConvertToEdGraphPinType(FParsed...)`

验证：

- 加强 `BlueprintHelper.GraphWrite.LegacyMainline.NoPublicParsedNodeGraphWriteApi`。
- `rg -n "EParsedBlueprintNodeType|FParsedPinType|FParsedLocalVariableDeclaration" BlueprintHelper\Source\BlueprintHelper\Public` 预期无命中。

## Discussion-gated Backlog

### 1. Merge / Patch ownership and Patch ConnectPins boundary

风险：

- `Patch ConnectPins` 可能与 Merge/Patch ownership 的语义归属重叠。
- 如果先做局部修复，可能把 Patch 语义硬编码进 GraphWrite mutation 或 runtime 层，破坏统一 ownership。

当前证据：

- 审计项 5 与此项存在交叉，Patch ConnectPins 处理时需要先决定 Merge/Patch ownership 的归属边界。

先决策问题：

- Patch 是否拥有独立 mutation owner，还是复用 Merge callable 的受控子路径。
- ConnectPins 是否属于 Patch handler 自身语义，还是应下沉为统一 mutation intent。

暂缓原因：

- 用户已明确要求 `Patch ConnectPins` 等 1、2、3 讨论后再决定。

### 2. Merge callable convergence

风险：

- Merge callable 若继续保留散落调用点，会让 Function/Action adapter 生命周期继续分叉。
- 若直接删除或重路由，可能影响现有 merge graph 生成成功路径。

当前证据：

- 先前审计指出 Function 仍有共享 adapter/lifecycle 的收敛债。

先决策问题：

- Merge callable 是否应只暴露统一 semantic pipeline 入口。
- 现有 callable 成功路径是否需要保留为迁移中间层，还是应直接改写到当前 adapter boundary。

暂缓原因：

- 该项属于行为收敛，超出第一批只修 4、6 的范围。

### 3. EventDelegate handler scan

风险：

- custom event / override / native taxonomy 与已有非 GraphWrite 工具可能冲突。
- EventDelegate handler 若提前改动，可能掩盖 taxonomy 决策问题。

当前证据：

- 先前 smoke 和审计均提示 Event custom_event / override / native taxonomy 需要先做冲突排查。

先决策问题：

- GraphWrite event taxonomy 是否以 GraphWrite statement 为唯一入口。
- 非 GraphWrite 工具现存 event/native 处理是否需要迁移或删除。

暂缓原因：

- 用户要求先做 scan 和讨论，本批不改代码。

### 5. Patch ConnectPins

风险：

- ConnectPins 是 graph mutation 语义，若本批单独修会和项 1 的 ownership 决策冲突。
- 局部 patch 可能继续扩大旧 facade/legacy API 的使用面。

当前证据：

- 用户已指出 `Patch ConnectPins` 与 1 有部分重叠。

先决策问题：

- ConnectPins 的合法入口、review target、rollback evidence 是否由 Patch 还是统一 GraphWrite mutation coordinator 拥有。
- Merge callable 和 EventDelegate scan 结论是否影响 ConnectPins 入口形态。

暂缓原因：

- 等 1、2、3 完成讨论后再决定，不在第一批实现。

## Test Plan

执行目标：

- `UnrealEditor-Cmd.exe ... Automation RunTests BlueprintHelper.TaskRuntime.ClusterHub;BlueprintHelper.GraphWrite.LegacyMainline.NoPublicParsedNodeGraphWriteApi`
- `Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload`
- `rg -n "EParsedBlueprintNodeType|FParsedPinType|FParsedLocalVariableDeclaration" BlueprintHelper\Source\BlueprintHelper\Public`
- `git diff --check`

验收标准：

- GraphWrite success step 会构建 graph surface atomic target。
- GraphWrite failed/missing payload step 不产生 Review evidence。
- Public GraphWrite headers 不再暴露 parsed DTO 或 legacy facade generation/local-variable API。
- 编译与聚焦自动化测试通过。

## Execution Result

本批实际执行项：

- 已实现审计项 4：GraphWrite TaskRuntime cluster 自建 Review evidence，不走 runtime fallback。
- 已实现审计项 6：public parsed DTO 从 `BlueprintGraphWriteFacade.h` 清出，parsed DTO 收敛到 private pipeline，公共结果类型拆到 `BlueprintGraphWriteResultTypes.h`。
- 审计项 1、2、3、5 未改代码，仅保留在讨论门禁 backlog。

验证结果：

- `Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload` 通过。
- `Automation RunTests BlueprintHelper.TaskRuntime.Cluster` 通过 6 个测试。
- `Automation RunTests BlueprintHelper.Review.Producer.ClusterBuildsProducerOwnedEvidence` 通过。
- `Automation RunTests BlueprintHelper.GraphWrite.LegacyMainline.NoPublicParsedNodeGraphWriteApi` 通过。
- `rg -n "EParsedBlueprintNodeType|FParsedPinType|FParsedLocalVariableDeclaration" BlueprintHelper\Source\BlueprintHelper\Public` 无命中。
- `git diff --check` 通过；仅有既有 Windows 换行提示。

验证备注：

- `Automation RunTests A;B;C` 会被 UE console 按分号拆成多条命令，本批已改为分别执行精确测试。
- `LogFileManager` 的 JetBrains Rider ports move warning、EOS/HTTP 无网络 warning 不属于插件行为失败；自动化测试最终 exit code 为 0。
