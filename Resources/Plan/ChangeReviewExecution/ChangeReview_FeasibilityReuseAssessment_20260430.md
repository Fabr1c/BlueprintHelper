# BlueprintHelper Change Review Feasibility And Reuse Assessment

> Date: 2026-04-30

## 结论

计划可行，收益极高，建议实施。

当前实现已经具备 Change Review 第一版所需的关键基础：Bridge Router 有集中命令入口，MCPServer 已经把 Bridge 命令暴露为工具，服务层已经覆盖 Blueprint、UMG、UObject、DataTable、资产浏览、编译、Undo/Redo、创建蓝图和保存资产，LogicProcessor 也已经接入 `export_logic`。

需要调整的是实施顺序：不要一开始就把所有写命令默认切到 pending review。应先建立命令清单、审阅数据模型、持久化和最小面板，再分批把写命令接入。默认 pending 可以在 MVP 验收后打开。

## 复用评估

| 领域 | 当前实现 | 可复用性 | 结论 |
|---|---|---:|---|
| Bridge 命令入口 | `FBlueprintHelperBridgeRouter::HandleRequest` 以集中 if 链分发所有命令 | 高 | 适合增加命令分类器和 review 包裹层 |
| MCP 工具层 | `MCPServer/src/tools.ts` 逐个注册 `blueprint_*` 工具，直接转发 Bridge 响应 | 高 | 新增 review 工具成本低，旧响应兼容性好 |
| 写操作能力 | Blueprint 变量、图表、节点、UMG、UObject、DataTable、创建资产均已有服务 | 高 | Change Review 不需要重写写工具 |
| 编译诊断 | `FBlueprintHelperCompileService` 已能编译并提取诊断 | 高 | 可直接记录到 session diagnostics |
| 逻辑摘要 | `FBlueprintHelperLogicProcessor` 和 `export_logic` 已落地 | 高 | 原计划中的 Logic Diff 可以提前纳入 MVP+ |
| Undo/Redo | `FBlueprintHelperEditorCommandService::Undo` 已调用 `GEditor->UndoTransaction` | 中 | 可复用，但不能作为唯一回滚手段 |
| 事务 | Import 和 Blueprint Structure 有 `FScopedTransaction`，Widget/DataTable/UObject 主要是 `Modify`/dirty | 中 | 需要统一事务边界或记录回滚能力 |
| 快照 | 目前没有 Review snapshot 管理器 | 低 | 需要新增，优先从逻辑 JSON/属性值快照开始 |
| UE Asset Diff | 当前代码未见 Diff 服务 | 低 | 需要新增，第一版允许退化到 summary/logic diff |
| Widget 面板 | `SHelperMainWidget` 是现有工具面板，无审阅列表和详情页 | 中 | 可扩展，但需要明显改 UI 结构 |
| 持久化 | Bridge 响应和 JSON 工具齐全，但无 session 保存目录 | 中 | 新增 Saved JSON 持久化即可 |

## 主要证据

- `Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp` 已集中处理 `import_json`、`export_logic`、`add_variable`、`delete_nodes`、UMG、UObject、DataTable、Undo、创建蓝图等命令。
- `MCPServer/src/tools.ts` 已有 43 个工具注册模式，新增 review 工具可以沿用 `bridge.sendCommand` 和 `toToolResult`。
- `Source/BlueprintHelper/Private/BlueprintHelper.cpp` 已集中构造服务并注入 Router，适合继续注入 `FBlueprintHelperChangeReviewManager` 和相关服务。
- `Source/BlueprintHelper/Private/Services/BlueprintHelperCompileService.cpp` 已能调用 `FKismetEditorUtilities::CompileBlueprint` 并收集诊断。
- `Source/BlueprintHelper/Private/Services/BlueprintHelperLogicProcessor.cpp` 已提供 raw JSON 到 logic JSON/Markdown 的处理，适合生成审阅摘要。
- `Source/BlueprintHelper/Private/Services/BlueprintHelperBlueprintStructureService.cpp` 的 Blueprint 结构操作已有 `FScopedTransaction`。
- `Source/BlueprintHelper/Private/Services/BlueprintHelperWidgetService.cpp`、`BlueprintHelperDataTableService.cpp`、`BlueprintHelperPropertyReflectionService.cpp` 会修改对象并标记 dirty，但事务覆盖不一致。

## 对原计划的修正

### 1. 默认 pending 延后打开

原计划建议所有写命令默认 `review_policy=pending`。这方向正确，但应分两步：

1. 第一批实现支持 `review_policy=pending | bypass | auto_approve | auto_approve_and_save`，默认仍可由项目设置或临时常量控制。
2. MVP 验收后，把默认值切到 `pending`。

原因是当前写工具很多，面板和回滚未完成前直接默认 pending 会改变所有 Agent 写入体验。

### 2. 回滚必须分级

当前事务覆盖不一致，不能承诺所有写操作都能一键 Undo。第一版应记录每个 asset record 的 `RollbackMode`：

- `Transaction`: Blueprint import、变量、图表、节点删除等已有事务路径。
- `ValueSnapshot`: UObject 属性、DataTable 行值。
- `AssetSnapshot`: Blueprint/WidgetBlueprint 临时资产或 raw JSON 快照。
- `DeleteCreatedAsset`: 新建资产且无引用。
- `ManualOnly`: 已保存、混入用户编辑、快照缺失或资产移动。

### 3. Logic Diff 应提前复用

原计划把 Logic Diff 作为可选能力。当前 LogicProcessor 已存在，Change Review 应直接复用：

- before: `export_to_json` 或属性/行快照。
- after: 写操作后再次导出。
- summary: `FBlueprintHelperLogicProcessor::ProcessRawJson` 生成 Markdown 或 logic JSON。

### 4. 命令清单必须先落地

Bridge Router 当前没有命令元数据。必须先生成命令清单，明确每个命令是 read/write/ui/editor-process/build，以及目标资产字段来自哪里。否则无法可靠包裹写命令和决定 capture 策略。

## 风险等级

| 风险 | 当前等级 | 降级策略 |
|---|---:|---|
| 回滚误伤用户手工修改 | 高 | 增加 dirty/revision/mixed changes 检查，命中后 ManualOnly |
| 事务嵌套或 Undo 栈混乱 | 高 | 不强行给所有服务加外层 `FScopedTransaction`，先记录操作和快照 |
| 大资产 snapshot 或 Diff 过慢 | 中 | MVP 使用摘要和懒加载，raw JSON 不作为默认 UI |
| 旧 MCP 工具行为变化 | 中 | `review` 字段只追加，不改变 `success/result/error` |
| UE Diff API 接入成本 | 中 | 第一版允许 Open UE Diff 只覆盖 Blueprint，其他资产退化摘要 |
| 面板改造冲突 | 中 | 单独 Worker，先列表和详情，不重做整个主面板 |

## 推荐实施判断

继续执行，并把优先级放在“审阅闭环”而不是“最完整 Diff”：

1. 命令清单和 Review 契约。
2. ReviewManager 持久化和 Bridge review 命令。
3. 三类最小写操作进入 pending：`add_variable`、`import_json` 或 `delete_nodes`、`set_object_property`。
4. 面板显示 pending session，支持 approve/reject。
5. 事务或快照能恢复至少一条 Blueprint 变量用例。
6. 再扩展 DataTable、UMG、UE Diff、MCP review 工具和报告导出。

