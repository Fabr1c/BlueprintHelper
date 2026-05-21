# BlueprintHelper SpawnerCluster Skeleton Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` to execute follow-up implementation tasks.

**Current canonical pipeline:**
`AgentFace statement -> Semantic Resolver -> FBlueprintHelperActionResolutionRequest { ClusterKind, SemanticConstraints, GraphContext, TypedPins } -> BlueprintActionResolutionCore -> SpawnerClusterResolver.SelectCluster(ClusterKind) -> Cluster.Resolve(Request) -> UBlueprintNodeSpawner candidate -> NodeFragment adapter`.

## 0. 执行状态（2026-05-21）

- [x] `ActionResolutionCore` 已以 `ClusterKind` 作为唯一一级分发键。
- [x] `SpawnerClusterResolver` 已只按 `Request.ClusterKind` 分发，不再按语义 kind 选择簇。
- [x] `call/get/set/get_property/set_property/op/construct/deconstruct/select/control` 已收敛为 `FBlueprintHelperActionSemanticConstraints.Kind`。
- [x] `FunctionActionCluster` 已接入现有 call resolver。
- [x] `FieldVariableActionCluster` 已接入 get/set provider 解析链路。
- [x] 旧变量 direct spawn 入口已从 GraphWrite facade / node spawner 公开路径移除。
- [ ] FieldVariableAction 的 NodeFragment adapter 尚未完成，因此 get/set 当前 P1 范围只保证候选解析，不虚标为写入成功。
- [ ] get_property/set_property、GenericAssetStructControl、EventDelegate 等簇仍需要后续 provider/adapter 迁移。

距离期望差距：当前完成的是 P0 契约固化与 P1 get/set provider 候选解析；距离完整 GraphStatement Framework 仍差非 call 的 NodeFragment adapter、更多 spawner family provider、FragmentDAG emission 与端到端 CLI 覆盖测试。

## 1. 一级分发硬规则

1. `FBlueprintHelperActionResolutionRequest.ClusterKind` 是唯一一级分发键。
2. AgentFace 语义字段只能写入 `FBlueprintHelperActionSemanticConstraints`。
3. `SpawnerClusterResolver` 不允许根据 semantic kind、query、target、type 做一级簇选择。
4. 新增能力必须扩展对应 SpawnerCluster / Resolver / Adapter 边界，不允许恢复旧 node handler、旧 direct spawn、旧 fallback。

## 2. 当前四类簇

| ClusterKind | 当前状态 | 差距 |
|---|---|---|
| `FunctionAction` | `call` 已接入 call resolver | 后续把 call 专项缓存泛化到 ActionResolution 缓存 |
| `FieldVariableAction` | `get/set` 已接入 provider 候选解析 | 缺 NodeFragment adapter；`get_property/set_property` 未迁移 |
| `EventDelegateAction` | skeleton 已存在 | 缺真实 provider / adapter |
| `GenericAssetStructControlAction` | skeleton 已存在 | 缺 construct/deconstruct/select/control 等 provider / adapter |

## 3. 后续执行顺序

1. 完成 FieldVariableAction get/set NodeFragment adapter。
2. 迁移 get_property/set_property provider。
3. 迁移 GenericAssetStructControl provider。
4. 接入统一 candidate action cache。
5. 跑 CLI preview/execute 覆盖测试并同步现实能力。