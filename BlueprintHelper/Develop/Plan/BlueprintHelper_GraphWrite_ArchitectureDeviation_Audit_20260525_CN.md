# BlueprintHelper GraphWrite 架构偏离审计

审计日期：2026-05-25

审计对象：
- 设计基线：`BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
- 实现范围：GraphWrite bridge routes、TaskRuntime cluster、GraphStatement、ActionResolution、Merge/Patch/MutationCoordinator、EventDelegate、Review evidence 集成路径

结论：现有 GraphWrite 主路径已经部分具备 `SemanticIR -> ActionContextPipeline -> ActionResolutionCore -> GraphStatement fragment -> UE mutator` 的新架构形态，但 `merge_blueprint_graph`、`patch_blueprint_graph`、EventDelegate handler 解析、Review evidence 接入仍存在明确偏离。其中 Merge/Patch 独立执行链路是最高风险偏离，因为它绕开了设计文档要求的一致 GraphStatement/ActionContext/ActionResolution 主线。

## 2026-05-25 状态同步

本节记录后续修复计划执行后的当前状态；下方 P0/P1/P2/P3 原审计段落保留为历史问题来源，不再全部代表当前实现状态。

| 原审计项 | 当前状态 | 当前证据 | 剩余判断 |
|---|---|---|---|
| 1. Merge/Patch ownership | CLOSED | `BlueprintHelper_GraphWrite_PatchMergeOwnership_ConnectPinsPlan_20260525_CN.md` 已记录 Execution/Verification Result；`Patch/Merge` 通过 mutation intent + `FBlueprintHelperGraphWriteMutationCoordinator::ExecuteIntents` 收敛所有 pin mutation owner。 | 不再作为当前 P0 阻塞项；后续新增 Patch/Merge 子能力仍必须复用该 ownership。 |
| 2. Merge callable convergence | CLOSED_WITH_TEST_NAME_NOTE | `BlueprintHelper_GraphWrite_MergeCallableConvergencePlan_20260525_CN.md` 已记录 Merge callable 创建收敛到 `FBlueprintHelperMergeCallableFragmentService`，并通过 `BuildCallFunctionFragment` / `ValidateCallFunctionFragment` 进入 GraphStatement callable fragment builder；`NoLocalMergeCallableSpawner` contract 已落地。 | 计划正文中的 `ToolResultBase.MergeCallableConvergence` 名称没有成为最终测试名，实际验收由 `BlockScopedAnchors.Merge` + source contract 承担。 |
| 3. EventDelegate handler scan / event taxonomy | CLOSED | `BlueprintHelper_GraphWrite_EventTaxonomy_FiveBugFixPlan_20260525_CN.md` 已标记 completed；focused automation、legacy guards、source scans、final read-only review 均记录通过。 | 当前不再作为架构偏离项；后续 event 能力扩展仍保持 Signature owns declaration、GraphWrite owns body/use-site、EventDelegate owns delegate/component-bound use-site。 |
| 4. GraphWrite runtime Review evidence | CLOSED | `BlueprintHelper_GraphWrite_ArchitectureDeviation_FixPlan_20260525_CN.md` 已记录 GraphWrite TaskRuntime cluster 自建 Review evidence，不走 runtime fallback，并通过 `BlueprintHelper.TaskRuntime.Cluster` / Review producer tests。 | 最小 graph surface atomic target 已接入；更细粒度 fragment/pin/action evidence 属于后续增强，不阻塞当前闭环。 |
| 5. Patch ConnectPins | CLOSED | `BlueprintHelper_GraphWrite_PatchMergeOwnership_ConnectPinsPlan_20260525_CN.md` 已记录 `patch.source_node_ref/source_pin_ref` 与 path 形态解析、dry-run 端点校验、execute path 构造 `ConnectPins` intent 并走 coordinator。 | 不再作为功能失败项；TaskSpec agent-facing contract 是否公开 `connect_pins` 是独立能力边界问题。 |
| 6. Public parsed DTO cleanup | CLOSED_FOR_FACADE | `BlueprintHelper_GraphWrite_ArchitectureDeviation_FixPlan_20260525_CN.md` 已记录 `BlueprintGraphWriteFacade.h` parsed DTO / legacy facade API 清理，public parsed DTO guard 通过。 | Public pipeline headers 仍暴露 `Generate*FromJson` pipeline API；这不是 Facade parsed DTO residue，但若目标是完全隐藏 JSON pipeline，应单独开 API boundary cleanup。 |

## 当前距离“完全稳定 / 能力齐全”的剩余门禁

1. `BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md` 仍是未执行计划；该测试应在 GraphWrite 当前稳定性收敛后作为统一最终验收执行，而不是当前修复闭环的前置阻塞。
2. Agent-facing `TaskSpec` public contract 仍是 first-slice 子集；它描述的是 Agent 可直接编写的稳定语义子集，不等同于 GraphWrite 内部/Bridge/runtime 的完整能力面。当前已新增 `BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md` 作为待实现的全局能力契约入口。
3. `asset_action` positive proof 当前主要是 C++ ActionResolution 证据；CLI/TaskSpec positive smoke 仍依赖能诚实生成 `asset_action_stable_id` 的 ActionDatabase projection fixture。
4. legacy parsed-plan 路径仍是“执行层禁用”而非完全移除：`BlueprintGraphMutationPlanBuilder.cpp` 仍解析 legacy `nodes/links`，`BlueprintGraphMutationPlanExecutor.cpp` 以 `parsed_node_plan_unsupported` 阻断。当前决策是该类 parsed residue 必须全部移除，不能只保留阻断路径。
5. Review evidence 仍缺少更上层的 TaskRuntime -> Review model 端到端消费断言；当前已有 cluster-owned evidence 构建证据，但 Merge/Patch/ConnectPins 的 Review metadata、`EvidenceId`/`ChangeId` 父子关系仍建议补链路级测试。
6. GraphWrite 稳定性修复完成后，需要再跑一次统一最终 E2E：TaskSpec build/node tests、GraphWrite generality preflight、四簇/GraphWrite automation、UE 5.6 build、source hygiene scans。

## 严重等级

- P0：主架构路径偏离，导致同一类 GraphWrite 写入存在两套事实上的执行模型。
- P1：局部核心能力偏离 projected ActionContext、ActionResolution、Review/Debug evidence 等硬约束，可能破坏可解释性、一致预览、回滚或诊断。
- P2：局部分支实现不完整或特殊化，短期可控但会扩大架构债。

## P0：Merge/Patch 仍是独立 mutation 主路径，绕开 GraphStatement/ActionContext/ActionResolution

设计要求：
- 宽表面 node/action 选择必须经过 `BlueprintActionResolutionCore`，并携带 projected ActionContext、ActionDatabase、ActionFilter、NodeSpawner evidence。
- 新能力不能绕过 `SpawnerClusterKind -> cluster -> semantic constraint`，也不能走旧 handler registry、parsed-node mutation fallback、手写 `UK2Node_*` 快捷路径。
- Preview/Execute 应共享同一 pipeline；DebugBundle、Review evidence、UI overlay、AcceptReject 应消费同一 Review/Action 数据模型。

代码证据：
- `Source/BlueprintHelper/Private/Entry/Bridge/Routes/BlueprintHelperGraphWriteBridgeRoutes.cpp:52` 将 `merge_blueprint_graph`、`patch_blueprint_graph` 与 append/replace 同列为 GraphWrite 命令。
- `Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.cpp:62` 在 runtime cluster 中直接分发 append/replace/patch/merge service。
- `Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.cpp:1025` 将 merge 结果转成 `FBlueprintHelperGraphWriteMutationIntent` 并调用 mutation coordinator。
- `Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.cpp:530` 将 connect/disconnect/replace 等操作转成 mutation intent 并执行。
- `Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.cpp:220` 之后直接执行 append/insert/branch/pin default/link mutation。

偏离说明：
Merge/Patch 现在是 GraphWrite 对外能力的一部分，但不是 GraphStatement pipeline 的一部分。它们以 service + mutation intent + coordinator 的方式直接写图，缺少统一的 ActionContext demand/snapshot/inference/bundle、ActionResolutionResult、fragment evidence、preview/execute 同源数据。这样会形成两套 GraphWrite 行为：append/replace 走新架构，merge/patch 走独立 mutation 架构。

影响：
- 同一个 GraphWrite 写入能力无法保证同源 preview、execute、Review evidence、DebugBundle。
- resolver/cluster/fragment 层的约束、诊断和 evidence 无法覆盖 merge/patch。
- 后续新增 patch/merge 子能力容易继续堆在 mutation coordinator 中，扩大特殊路径。

建议收敛：
1. 明确 merge/patch 是否属于 GraphStatement 主架构。如果属于，应迁移为 SemanticIR/GraphStatement fragment，再由统一 ActionContext/ActionResolution/fragment coordinator 执行。
2. 如果确实需要保留 GraphMutation 层，应把它定义为 GraphWrite 下的正式 action cluster，并补齐 projected request、Action evidence、Review evidence、preview/execute 同源模型，而不是 service 私有分支。
3. 删除或隔离旧 mutation-only 入口，避免新能力继续直接调用 mutation coordinator。

## P1：Merge 插入 callable 节点绕过 projected ActionContext 与 ActionResolutionCore

设计要求：
- 一级 action resolution 应以 `EBlueprintHelperSpawnerClusterKind` 为调度边界。
- Cluster 只能消费 projected `FBlueprintHelperActionResolutionRequest` 或 `FBlueprintHelperActionClusterContextView`，不能自己重建 demand/snapshot/inference/bundle/scope。
- 共享 resolve/invoke/pin metadata/semantic tag 应由 `FBlueprintHelperActionFragmentSpawnCoordinator` 负责。

代码证据：
- `Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.cpp:48` 的 `ResolveMergeCallableFunction` 本地调用 `FBlueprintHelperCallFunctionResolver`。
- `Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.cpp:120` 的 `CreateMergeCallableNode` 本地选择 `NodeSpawner` 或创建 `UBlueprintFunctionNodeSpawner`。
- `Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.cpp:884` 的 inserted logic 路径在 merge service 内解析 owned block/custom event/function call。

偏离说明：
Merge callable 插入没有通过 GraphStatementBuilder 构造 projected action request，也没有通过 `FBlueprintHelperActionFragmentSpawnCoordinator::BuildResolvedActionFragment` 获取统一的 selected spawner、pin metadata、semantic tag、Action evidence。它局部复用 resolver/spawner adapter，但绕过了架构要求的投影边界与 action cluster ownership。

影响：
- CallFunction、CustomEvent、OwnedBlock 的选择依据与 GraphStatement 主路径不一致。
- 失败诊断不能稳定复用 ActionResolutionCore 的候选、过滤、evidence 模型。
- 后续 merge 子能力容易继续复制 resolver/spawner 逻辑。

建议收敛：
1. Merge service 不应直接解析 callable；应构造 `FBlueprintHelperActionResolutionRequest` 并委托 GraphStatement/fragment coordinator。
2. 将 inserted logic 表达为 GraphStatement fragment 或正式 Action cluster 输入。
3. 保留 architecture-neutral helper 可以接受，但不能保留独立 resolver ownership。

## P1：EventDelegate handler 解析仍扫描资产图来修补缺失 projected evidence

设计要求：
- EventDelegate resolver 必须消费 projected evidence；GraphWrite/EventDelegate 不应通过扫描资产来修补缺失上下文。
- Signature 边界负责声明，GraphWrite/EventDelegate 只负责 use-site 写入。
- Resolver 成功不能凭空生成缺失的 projected values。

代码证据：
- `Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.cpp:15` 从 `Request.ContextEvidence` 读取 use-site evidence。
- `Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.cpp:153` 的 `ResolveHandlerFunction` 先 `FindFunctionByName`。
- `Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.cpp:168` 在缺少函数时扫描 `Blueprint->UbergraphPages` 与 `Graph->Nodes` 查找 `UK2Node_CustomEvent`。

偏离说明：
该路径表面上消费 `ContextEvidence`，但 handler function 缺失时会扫描 Blueprint 图节点并把已有 custom event 当作成功 evidence。这与“resolver 只消费 projected evidence，不扫描资产修补上下文”的设计要求冲突。

影响：
- Resolver 成功不再完全代表 projector 已提供充分上下文。
- Preview/execute 与 DebugBundle 中的 evidence 可能无法解释 handler 来源。
- Signature 与 use-site 边界变模糊，后续容易出现重复声明、误绑定或不可复现解析。

建议收敛：
1. 移除 EventDelegate resolver 内的 graph scan 修补逻辑。
2. 缺少 handler/signature projected evidence 时返回确定性失败，例如 `needs_more_semantic_context`。
3. 将 handler 声明、查找、签名匹配前移到 Signature/ActionContext projection 边界，并把 evidence 显式投影给 EventDelegate cluster。

## P1：GraphWrite runtime 未产出 Review evidence，未接入统一 Review/Action 数据模型

设计要求：
- Preview/Execute 应共享同一 pipeline。
- DebugBundle、Review evidence、UI overlay、AcceptReject 状态必须消费同一套 Review/Action 数据模型。

代码证据：
- `Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.cpp:48` 的 `BuildReviewEvidence` 直接返回 `false`。
- `Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp:5788` 只在 `bHasReviewEvidence` 为真时写入 `PostIoBatch.AddReviewEvidence`。
- `Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimeClusterHubTests.cpp:284` 测试明确断言 GraphWrite cluster 不使用 runtime fallback evidence。

偏离说明：
GraphWrite runtime step 成功后不会产出 cluster-owned Review evidence。现有测试确认它不走 fallback evidence，但也意味着 GraphWrite 没有把 GraphStatement/ActionResolution/MutationResult 的证据统一投递到 Review 模型中。

影响：
- Review panel、DebugBundle、AcceptReject 很难基于同一套 GraphWrite action evidence 解释写入结果。
- GraphWrite 执行和 Review v2 的状态模型之间存在断点。
- Merge/Patch 这类 mutation-only 路径更难被统一审计和回滚解释。

建议收敛：
1. 定义 GraphWrite Review evidence 的最小模型：target graph、node/action refs、fragment evidence、mutation result、resolver candidates、selected spawner、pin changes。
2. Runtime cluster 不应使用 fallback evidence，但应从 GraphWrite pipeline 的正式结果中构建 Review evidence。
3. Review UI、DebugBundle、AcceptReject 只消费这一套模型，不再各自解释 GraphWrite 输出。

## P2：Patch ConnectPins 分支存在局部特殊化且源端 ref 未从 payload 填充

设计要求：
- GraphWrite 写图应通过统一 statement/action pipeline 表达。
- 特殊分支必须复用 typed target/pin/candidate reporting/Review/Debug evidence 模型。

代码证据：
- `Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.cpp:510` 的 ConnectPins 分支声明 `FromNodeRef`、`FromPinRef`。
- 同一分支后续直接使用这两个 ref 查找 source node/source pin，但代码片段未从 payload 读取并填充源端 ref。
- `Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.cpp:530` 之后直接执行 mutation intent。

偏离说明：
ConnectPins 是 patch service 的本地特殊分支，既没有进入统一 action pipeline，也存在明显 payload 到内部 ref 映射不完整的问题。

影响：
- ConnectPins 可能稳定失败，或只能依赖未初始化/空 ref 进入错误路径。
- 该分支继续扩大 patch service 的私有语义，而不是复用 typed pin evidence。

建议收敛：
1. 短期修正 payload 字段读取，避免隐藏的功能失败。
2. 中期将 ConnectPins 表达为统一 pin-action/GraphStatement fragment，并复用 typed pin evidence 与 Review evidence。
3. 如果无法立即迁移，应将该分支标记为临时兼容并限制新增子能力。

## P3 观察项：公开 parsed DTO 表面仍残留，但主线已拒绝 legacy nodes/links

代码证据：
- `Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h:17` 仍暴露 `EParsedBlueprintNodeType`。
- `Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h:480` 仍暴露 `GenerateBlueprintFromJson`、`GenerateMultiGraphFromJson`、`EnsureLocalVariableExists(FParsedLocalVariableDeclaration)`、`ConvertToEdGraphPinType(FParsedPinType)`。
- `Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp:1060` 已拒绝无 `logic_spec` 的 legacy nodes/links 创建。

判断：
这不是当前最高风险偏离，因为主执行路径已经拒绝 legacy nodes/links。但公开 facade 仍保留 parsed DTO，会继续给外部调用者暗示旧数据模型可用，和 GraphStatement/SemanticIR 主线存在语义漂移风险。

建议：
在确认无外部调用后删除或降级这些公开接口；如果必须暂存，应在 API 层明确标记为非主线遗留接口，并禁止新调用点接入。

## 推荐修复顺序

1. P0：先收敛 Merge/Patch ownership，决定迁移到 GraphStatement 主线，或正式定义 GraphMutation action cluster；不要继续以 service 私有 mutation path 扩展能力。
2. P1：把 Merge callable 插入迁移到 projected ActionContext + ActionResolutionCore + fragment coordinator。
3. P1：移除 EventDelegate resolver 的资产图扫描修补逻辑，缺失 projected evidence 时确定性失败。
4. P1：补齐 GraphWrite runtime Review evidence，使 DebugBundle、Review UI、AcceptReject 共享同一 GraphWrite action evidence。
5. P2：修复 Patch ConnectPins payload 映射，并规划迁移到统一 typed pin/action pipeline。
6. P3：清理或降级 public parsed DTO 遗留表面。

## 本次审计未执行项

- 未修改 C++ 实现。
- 未运行 Unreal 编译或 BlueprintHelper CLI 验证。
- 本文档基于静态代码审计和现有设计文档对照生成。
