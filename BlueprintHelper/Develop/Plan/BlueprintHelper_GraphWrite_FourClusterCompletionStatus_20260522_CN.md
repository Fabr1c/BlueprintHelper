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
| `FunctionActionCluster` | 部分完成 | 约 70%-75% | `call` 已经经由 `CallFunctionResolver` 消费 ActionDatabase / ActionFilter / NodeSpawner 候选；`op` 已经通过 `OperatorActionResolver` 映射 UE promotable operator spawner，并有候选、歧义、缺失等基础测试。 | `CallFunctionResolverUtils` 仍保留 `TFieldIterator<UFunction>` 补充扫描路径，需要明确只能作为补充候选而不能绕过 ActionDatabase 主证据；construct/deconstruct、统一 spawn adapter、post-spawn defaults 等仍在 P2/P3 计划中未完全闭环。 | 锁定 `call` 主链路为 ActionDatabase 候选优先；补测试证明 `TFieldIterator` 不能单独形成 selected success evidence；把函数类 spawner 调用统一收敛到共享 adapter/lifecycle。 |
| `FieldVariableActionCluster` | 部分完成 | 约 45%-55% | `get` / `set` 已经通过 `FieldVariableActionResolver` 和 `UBlueprintVariableNodeSpawner::CreateFromMemberOrParam` 形成基础 spawner evidence；簇入口已经转发到 resolver，并有基础成功、歧义、缺失测试。 | `get_property` / `set_property` 仍未按 property path、pin evidence、field metadata 建立独立闭环；field 名称仍可从 `Semantic.TargetPath` / `Query` 回退推导，缺少“投影上下文缺失即失败”的硬约束；P2/P3 计划中仍有“get/set spawn layer 不走直接变量节点捷径”的未完成项。 | 把 `field_name`、`property_path`、owner、pin/type evidence 设为 resolver 的显式输入契约；补 `get_property` / `set_property` 正向和缺失 evidence 测试；收敛变量 spawner 到共享 adapter。 |
| `EventDelegateActionCluster` | 部分完成 | 约 20%-30% | `event` 自定义事件语义已有成功路径，可提取事件名并用 `UBlueprintEventNodeSpawner::Create` 返回 resolved spawner；能力边界只认领 `Event`，合约测试固定该簇当前不误承诺 `ComponentBoundEvent` / `Bind`。 | `component_bound_event`、`bind`、`unbind`、`assign`、`delegate_call`、`delegate_clear` 等委托/绑定语义尚无成功闭环；缺少 binding object、component、delegate signature 等 projected evidence；失败诊断还未细分为可执行的 `needs_more_semantic_context`。 | 先投影 component/delegate/signature/binding object evidence；缺失时返回细分诊断；evidence 完整后再接入 `UBlueprintBoundEventNodeSpawner` / `UBlueprintDelegateNodeSpawner` 并补正向与失败测试。 |
| `GenericAssetStructControlActionCluster` | 部分完成 | 约 45%-55% | `construct` / `deconstruct` / `select` / `control` 已有 provider boundary 和 resolver 入口；select/branch/return/sequence 等唯一控制流可以 direct spawn，符合“唯一控制流不必完整走通用 ActionDatabase 查询链路”的取舍。 | `create` / `convert` / `schedule` 等被簇枚举认领但 provider boundary 仍默认 unsupported；construct/deconstruct 仍需要把 struct/action payload、wildcard promotion、post-link evidence 统一；sequence 等 direct spawn 还应保持一级分发规则 + 次级语义映射路径，而不是散落在 mutation helper。 | 把唯一控制流 direct spawn 抽成显式 singleton/control-flow evidence provider；保留一级分发到簇、簇内二级语义映射；把 broad create/convert/schedule 继续走完整 context -> provider/resolver -> spawner/evidence 链路。 |

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

本次是静态代码与文档审计，未重新运行 UE 编译、编辑器 smoke 或 BlueprintHelper CLI 执行链路。
