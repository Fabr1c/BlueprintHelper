# ReferenceContext 与 UE FiB 查找逻辑对齐设计

日期：2026-05-17

## 范围

`read_reference_context` 是面向 Agent 的依赖查找工具，用于高风险 Blueprint 操作前判断影响面。它的首要目标是回答：哪些资产与目标相关，以及删除、改签名、迁移时是否应该阻塞。

本工具应对齐 Unreal Engine Find-in-Blueprints 的搜索语义，但不复刻 Slate UI 行为。

第一阶段需要覆盖：

- `asset` / `blueprint` 的包级 dependency / referencer 查询。
- `function` 的调用引用查询。
- `member_variable` 的 get / set / binding 引用查询。
- 带 `graph_name` 的局部变量引用查询。
- `event` / `custom_event` 的引用查询。
- `event_dispatcher` 的基础引用查询；dispatcher 专属风险分类在基础 FiB 路径稳定后补齐。

## 非目标

- 不复制 Find-in-Blueprints 的 UI 结果树。
- 默认结果不暴露 node GUID。
- 不要求 `target_guid`；普通 Agent 不会知道 GUID。
- 不返回只重复请求入参的 `target` 或 `query` 块。
- 默认不暴露完整 pin 数据、node title、图标、UI category 等展示字段。

## UE 对齐策略

实现应尽量复用 UE 自己的搜索语义：

- `function`：先解析目标 `UFunction`，成功后使用 `FindInBlueprintsHelpers::ConstructSearchTermFromFunction`；解析失败且策略允许时，fallback 到函数名搜索。
- `member_variable`：先解析目标 `FProperty`，构造 `FMemberReference`，再使用 `FMemberReference::GetReferenceSearchString`；解析失败且策略允许时，fallback 到变量名搜索。
- `local_variable`：用 `target_name + graph_name` 构造 `Nodes(VariableReference(MemberName=... && MemberScope=...))`。
- `event` / `custom_event`：可解析事件节点时使用节点的 `GetFindReferenceSearchString`；解析失败且策略允许时，fallback 到事件名搜索。
- `event_dispatcher`：先沿用 member-reference 风格查找 bind / assign / clear / binding 相关引用，再补 dispatcher 专属 `reference_kind` 分类。

这里追求的是“搜索逻辑相同”，不是“UI 生命周期相同”。索引进度条、Index All 弹窗、当前选择态、搜索结果树都属于 UI 层，不进入 Agent 工具协议。

## 请求字段

```json
{
  "asset_path": "/Game/BP_Player.BP_Player",
  "target_type": "function",
  "target_name": "DoSomething",
  "graph_name": "EventGraph",
  "declaring_class_path": "/Script/Engine.Actor",
  "search_scope": "project",
  "resolution_policy": "ue_then_name",
  "detail": "samples",
  "max_results": 50
}
```

字段规则：

| 字段 | 是否必填 | 说明 |
| --- | --- | --- |
| `asset_path` | 是 | 目标所在资产，或资产级查询目标。 |
| `target_type` | 默认 `asset` | 第一阶段支持：`asset`、`blueprint`、`graph`、`function`、`member_variable`、`local_variable`、`event`、`custom_event`、`event_dispatcher`。已有兼容别名如果当前已经支持，可以继续接受。 |
| `target_name` | 成员级目标必填 | 函数名、变量名、事件名或 dispatcher 名。 |
| `graph_name` | 局部变量必填；事件可选 | `local_variable` 必填；`event` / `custom_event` 可用于消歧。 |
| `declaring_class_path` | 否 | 用于继承、父类、native 成员消歧，例如 `AActor::K2_DestroyActor`。 |
| `search_scope` | 默认 `project` | `asset` 只查目标资产；`project` 查项目 FiB 索引。 |
| `resolution_policy` | 默认 `ue_then_name` | `ue_then_name`、`ue_only`、`name_only`。高风险 remove / migration preview 应使用 `ue_only`。 |
| `detail` | 默认 `samples` | `summary`、`samples`、`full`。`summary` 不返回 samples；`full` 预留给修复/迁移工作流。 |
| `max_results` | 默认 50 | referencer 行或 full-detail match 的上限。 |

已明确移除：

- `target_guid`：不适合作为 Agent 调用字段。
- `include_samples`：由 `detail` 统一表达。

## 返回字段

```json
{
  "schema": "ReferenceContextPack.v1",
  "context_id": "refctx_xxx",
  "summary": {
    "asset_count": 2,
    "reference_count": 5,
    "blocking_count": 2,
    "warning_count": 0,
    "partial": false,
    "truncated": false
  },
  "index_status": {
    "unindexed_count": 0,
    "out_of_date_count": 0,
    "failed_count": 0
  },
  "referencers": [
    {
      "asset_path": "/Game/BP_Enemy.BP_Enemy",
      "asset_type": "Blueprint",
      "match_count": 3,
      "reference_kinds": ["function_call"],
      "safety": "blocking",
      "samples": [
        {
          "graph_name": "EventGraph",
          "reference_kind": "function_call"
        }
      ]
    }
  ],
  "agent_hints": {
    "can_edit_safely": false,
    "requires_preview": true,
    "blockers": ["2 assets reference this function."]
  },
  "unsupported_checks": []
}
```

返回规则：

- `schema` 固定为 `ReferenceContextPack.v1`，不使用 BlueprintHelper 前缀。
- 不返回 `target`，因为请求中已经有目标上下文。
- 不返回 `query`，普通 Agent 不需要搜索词内部细节。
- `summary.partial=true` 表示 FiB 索引状态、unsupported target、fallback 策略或解析失败导致结果不完整。
- `summary.truncated=true` 表示 `max_results` 截断了 referencer。
- `index_status` 只表达 FiB 索引完整度，不替代引用计数。
- `referencers` 是资产级聚合，每行汇总该资产里的所有匹配。
- `detail=summary` 时不返回 samples；`detail=samples` 时每个资产只返回少量位置样本；`detail=full` 只用于后续修复/迁移需要。
- 默认 sample 不包含 node GUID，只包含 `graph_name` 和 `reference_kind` 等 Agent 可理解信息。

## Reference Kinds

第一阶段词表：

- `asset_reference`
- `function_call`
- `variable_get`
- `variable_set`
- `variable_binding`
- `local_variable_get`
- `local_variable_set`
- `event_reference`
- `custom_event_reference`
- `dispatcher_bind`
- `dispatcher_assign`
- `dispatcher_clear`
- `dispatcher_broadcast`
- `unknown`

词表应描述“为什么该目标删除或改动有风险”，而不是复刻 UE UI 怎么展示搜索结果。

## 安全语义

`safety` 取值：

- `blocking`：删除或修改目标很可能破坏该资产。
- `warning`：相关但不一定阻塞，或结果来自 fallback / name-only 搜索。
- `info`：仅上下文相关。

高风险操作必须保守处理：

- `remove_signature`：应使用 `resolution_policy=ue_only`；只要存在 `blocking` referencer、索引不完整、解析失败或结果截断，就应阻塞。
- dispatcher signature migration：在 dispatcher 专属 `reference_kind` 和迁移期望完成前，继续保持 block-first。
- asset 删除 / cleanup：即使成员级查找没有命中，包级 referencer 仍然需要纳入风险判断。

## 实现边界

推荐边界：

- 保持 Agent-facing 命令名为 `read_reference_context`。
- 扩展 `FBlueprintHelperDependencyAnalysisService`，或在其后增加一个小型 member-level analyzer。
- AssetRegistry 继续负责包级 dependencies / referencers 和 FiB 候选/索引状态。
- 函数、变量、事件目标尽量复用 UE search term 构造逻辑。
- 普通命令执行不直接依赖 Slate UI widget。
- destructive repair / migration 执行前可对候选资产做 loaded graph scan 复核，但这不是默认 Agent-facing 结果格式。

## 未定事项

- `local_variable` 是否作为新的公开 `target_type`，还是用 `member_variable + graph_name` 表达。
- 非交互 Agent 调用遇到未索引资产时的刷新策略。第一版可以返回 `partial=true`，不主动加载/重存资产。
- dispatcher 分类测试完成前，不开放 dispatcher signature migration beyond block。
- `detail=full` 是否开放给普通 Agent，还是仅作为内部修复/迁移模式。

## 2026-05-17 落地状态

- 已落地 Agent-facing 请求合同：`asset_path`、`target_type`、`target_name`、`graph_name`、`declaring_class_path`、`search_scope`、`resolution_policy`、`detail`、`max_results`。
- 已拒绝冗余/不可用请求字段：`target_guid`、旧 `scope`、旧 `include_samples`。
- 已落地返回合同：`schema=ReferenceContextPack.v1`，返回 `summary`、`index_status`、`dependencies`、`referencers`、`agent_hints`、`unsupported_checks`；不再回显 `target` / `query`，不返回默认 node GUID。
- 已实现成员级 loaded graph scan：`function`、`member_variable`、`local_variable`、`event`、`custom_event`、`event_dispatcher`，并用 `reference_kinds` 聚合到资产级 referencer 行。
- 已保留 AssetRegistry 包级 dependencies / referencers；FiB 索引状态通过 `index_status` 暴露，当前不复制 Slate UI 查询生命周期。
- `remove_signature` blocked preflight 已附带 `reference_context_request`，并固定使用 `resolution_policy=ue_only` 作为后续真实删除前置。
- 已知保守项：`member_variable` 的 UMG widget property binding 深扫当前记录为 `unsupported_checks=widget_property_binding_scan`；dispatcher signature migration beyond block 仍保持未开放。

验证：

- `npm.cmd --prefix AgentFaceService\task-core run build`：通过。
- `npm.cmd --prefix AgentFaceService\task-core run test:node`：106/106 通过。
- `npm.cmd --prefix AgentFaceService\mcp run build:mcp`：通过。
- `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload`：通过。
- UE Automation：`BlueprintHelper.ObjectFirst.Contract.ReadReferenceContextPayload`、`BlueprintHelper.ObjectFirst.Contract.ReferenceContextPackShape`、`BlueprintHelper.DependencyAnalysis.Service.FindsFunctionCallReference`、`BlueprintHelper.Signature.Service.RemoveSignatureDryRunBlocked` 均通过。

非本轮阻塞：

- MCP 回归测试文件 `task-tools.regression.test.ts` 当前通过 `registerTools` 取不到普通 task tools，原因是工作区已有 lifecycle-only / developer lifecycle 注册改动；本轮只验证了 MCP 编译和 task-core 普通工具合同，未修改该注册策略。后续按工具边界修正：MCP 只保留 `open`、`close`、`exec command`，历史 MCP task-tools 注册与回归测试视为废弃项目保留项，不再作为普通 Agent-facing 工具验证范围。

## 2026-05-17 边界项完成记录

本轮按新的工具边界收敛验证范围：MCP 只保留 `open`、`close`、`exec command` 三个入口，历史 MCP task-tools 注册与回归测试视为废弃项目保留项。普通功能继续由 CLI / task-core / UE service 与 Automation 覆盖。

已关闭的三个边界项：

- UMG WidgetTree / property binding deep scan：`UWidgetBlueprint::Bindings` 已纳入 `FBlueprintHelperDependencyAnalysisService` 的成员与函数级扫描。变量目标可识别 `widget_property_binding` / `widget_binding_target`，函数、事件和自定义事件目标可识别 `widget_property_binding_function`。`SourceProperty`、`SourcePath.Segments`、`ObjectName` 以及绑定函数 `MemberGuid` 均进入解析路径。
- Real remove execution：`remove_signature` 现在在 `execute_policy=execute_if_unreferenced` 且 `require_reference_context=true` 时执行真实删除。删除前固定构造 `read_reference_context` 请求，使用 `resolution_policy=ue_only`；只要 reference context 不安全、结果不完整、存在 blocking referencer 或 unsupported check，执行会阻断。成功路径覆盖 function 与 custom_event。
- Dispatcher signature migration：`ensure_event_dispatcher` 的 `signature_mismatch_policy=migrate_if_unreferenced` 已开放，仍受 reference context 门禁约束。安全时按 UE 语义移除旧 dispatcher variable / delegate signature graph，再按目标签名重建；`block` 策略仍保持默认保守行为。

实现细节修正：

- 自定义事件 / override event 的声明节点在依赖分析里被标为 `info`，不再把“自身声明”误判为删除 blocker。
- transient Automation 蓝图使用 asset scope，避免未保存测试资产触发全项目引用扫描。
- 执行型签名单元测试只屏蔽外部历史 `BlueprintHelperCliSmoke` 资产的编译错误日志，当前瞬态测试资产自身错误仍会失败；这是测试隔离，不改变生产路径。

验证结果：

- `npm.cmd --prefix AgentFaceService\task-core run build`：通过。
- `npm.cmd --prefix AgentFaceService\task-core run test:node`：106/106 通过。
- `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload`：通过。
- UE Automation 通过：
  - `BlueprintHelper.DependencyAnalysis.Service.FindsWidgetPropertyBindingReference`
  - `BlueprintHelper.DependencyAnalysis.Service.FindsWidgetFunctionBindingReference`
  - `BlueprintHelper.Signature.Service.RemoveSignatureDryRunBlocked`
  - `BlueprintHelper.Signature.Service.RemoveEventDispatcherDryRunBlocked`
  - `BlueprintHelper.Signature.Service.RemoveNativeEventDryRunBlocked`
  - `BlueprintHelper.Signature.Service.RemoveFunctionExecuteIfUnreferenced`
  - `BlueprintHelper.Signature.Service.RemoveCustomEventExecuteIfUnreferenced`
  - `BlueprintHelper.Signature.Service.MigratesEventDispatcherWhenUnreferenced`
