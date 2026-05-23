# BlueprintHelper GraphWrite 四簇工具完成度表

日期：2026-05-22

## 1. 判定口径

本表用于同步当前四个 GraphWrite Spawner-Oriented 工具簇的完成状态。状态只表示“当前代码是否已经按目标架构闭环”，不等同于 `UEActionContext_InputMatrix` 中的百分比。

`UEActionContext_InputMatrix` 中的旧百分比是当前 TaskSpec 能稳定获取的上下文覆盖率，用于判断每个簇的上下文输入成熟度；它不是实现完成度，也不是架构验收百分比。

| 状态 | 判定标准 |
|---|---|
| 完全完成 | 目标语义均已进入统一 context -> resolver/provider -> spawner/evidence 链路，关键成功/失败路径有测试或 smoke 证据，当前无已知架构 gap。 |
| 部分完成 | 至少一个关键语义已有可用闭环，但仍存在未实现语义、上下文 evidence 缺口、测试缺口或旧路径/临时直连需要收敛。 |
| 未完成 | 有代码或文档占位，但没有可用成功语义闭环，或主链路不可稳定使用。 |
| 未开始 | 没有可识别的当前架构实现入口。 |

## 2. 四簇完成度总表

| 工具簇 | 当前完成状态 | TaskSpec 稳定上下文覆盖率 | 已完成范围 | 剩余 gap | 下一步收敛 |
|---|---|---:|---|---|---|
| `FunctionActionCluster` | 部分完成 | 约 70%-75% | `call` 在 P6 能力行中已有 selected stable id、selected spawner、call node readback；`op` 有独立 resolver 入口；P6 regression 中的 operator/call resolver stress 与 generator 路径已通过。 | 函数类 spawner 调用仍需继续统一收敛到共享 adapter/lifecycle；Gap 2/3/4 已按文档范围关闭，但四簇整体仍保留 Gap 5 与若干语义收敛缺口。 | 继续把函数类 spawner 调用收敛到共享 adapter/lifecycle，并保持 call query 以 target/stable id 为主的 evidence contract。 |
| `FieldVariableActionCluster` | 部分完成 | 约 45%-55% | P6 readback 覆盖 variable get/set by field evidence；基础 get/set resolver/spawner evidence 可用。 | `get_property` / `set_property` 仍未完全按 property path、pin evidence、field metadata 建立独立闭环；field 名称回退推导仍需收敛。 | 把 `field_name`、`property_path`、owner、pin/type evidence 设为 resolver 的显式输入契约；补 property 正向和缺失 evidence 测试；收敛变量 spawner 到共享 adapter。 |
| `EventDelegateActionCluster` | 部分完成 | 约 20%-30% | P6 readback 覆盖 custom event by event name；P5/P6 明确 component-bound event / delegate bind complete evidence 当前返回 `unsupported_intent`，不产生 fake selected spawner；declared-capability contract 已同步当前 P5 语义。 | Gap 5 仍开放；component-bound/bind 尚无安全 UE spawner-family 正向路径。 | 先解决安全 UE spawner-family 路径；只有 complete projected evidence + `SelectedSpawner != null` 正向测试存在后才可标记完成。 |
| `GenericAssetStructControlActionCluster` | 部分完成 | 约 45%-55% | P6 readback 覆盖 select/control singleton stable id、struct make/break by struct type；P4 provider boundary 证据可用。 | Gap 2/3/4 closed for their documented scopes: GraphStatementBuilder demand/projection is owned by ActionContext, canonical singleton direct spawn is fixed behind `FBlueprintHelperSingletonControlFlowEvidenceProvider`, and mutation branch-fork sequence creation reuses provider evidence plus shared spawner adapter. Broad create/convert/schedule semantics remain open. | 保留一级分发到簇、簇内二级语义映射；继续让 broad create/convert/schedule 走完整 context -> provider/resolver -> spawner/evidence。 |

## 3. 分类汇总

| 状态 | 工具簇 |
|---|---|
| 完全完成 | 无 |
| 部分完成 | `FunctionActionCluster`、`FieldVariableActionCluster`、`EventDelegateActionCluster`、`GenericAssetStructControlActionCluster` |
| 未完成 | 无 |
| 未开始 | 无 |

## 4. 审计依据

| 输入 | 用途 |
|---|---|
| `BlueprintHelper_UEActionContext_InputMatrix_20260522_CN.md` | 只用于引用 TaskSpec 稳定上下文覆盖率，不用于判定实现完成度。 |
| `BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md` | 校验四簇分工、context pipeline、direct spawn 例外口径是否与总设计一致。 |
| `BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md` | 校验当前剩余 gap 与完成度表是否一致。 |
| `BlueprintHelper_ActionResolution_P0P1_FieldVariable_ImplementationPlan_20260521_CN.md` | 校验 Field/Variable 计划项和未完成项。 |
| `BlueprintHelper_ActionResolution_P2P3_FunctionGeneric_ImplementationPlan_20260521_CN.md` | 校验 Function/Generic 后续 adapter、construct/deconstruct、direct spawn 和生命周期计划项。 |
| `BlueprintHelper_FourSpawnerClusters_ContextConsumption_ImplementationPlan_20260522_CN.md` | 校验四簇是否已消费 `FBlueprintHelperActionClusterContextView` 以及是否仍存在语义覆盖缺口。 |
| 四簇 `ActionResolution` C++ 实现与 GraphWrite 相关测试 | 校验当前代码是否已有实际 success/error/test 证据。 |

## 5. 当前结论

当前四个工具簇都不能标记为“完全完成”。它们已经不属于“未开始”，也不是完全不可用的“未完成”：每个簇至少有一个当前架构路径已经形成基础闭环。

下一步收敛重点不是追求所有节点都完整走 `context -> ActionDatabase 查询 -> NodeSpawner` 链路，而是按语义范围区分：

1. `callfunction`、field/property、delegate/bind、create/convert/schedule 等宽范围或高歧义语义必须走完整 context / resolver / evidence 链路。
2. `branch`、`sequence`、`select` 等唯一控制流可以 direct spawn，但仍必须保留统一的一级分发规则和簇内次级语义映射路径。
3. direct spawn 也应有明确 evidence provider / adapter 边界，不能重新变成 mutation helper 或 builder 内的散落硬编码。

P6 已重新运行能力行、`BlueprintHelper.GraphWrite` 全量 regression 与 BuildPlugin，三者均通过；Gap 2/3/4 已按各自文档范围关闭。四簇仍全部保持“部分完成”，原因是 Gap 5 的 EventDelegate use-site 正向 spawner-family 路径仍开放，且 broad `create/convert/schedule` 等语义仍需继续完整走 context / resolver / evidence 链路。

## 6. P6 同步记录

| 验证 | 结果 | 影响 |
|---|---|---|
| `BlueprintHelper.GraphWrite.Capability80` | PASS: 5 succeeded, 0 failed | P6 能力行、readback、DebugBundle 分类和最终指标可作为 80% 能力记录。 |
| `BlueprintHelper.GraphWrite` | PASS: 116 tests successful (`107` succeeded + `9` succeeded with warnings), 0 failed, 0 not run | P6 regression blocker closed in `Saved/Automation/GraphWrite80_P6_GraphWrite_Regression_002/index.json`; four clusters still stay `部分完成` because residual architecture gaps, especially Gap 5, remain open. |
| UE 5.6 BuildPlugin | PASS | 当前代码可打包；`Saved/BlueprintHelperBuildTest_GraphWrite80_P6_002` 已通过，P6 regression 不再阻塞 DONE。 |
