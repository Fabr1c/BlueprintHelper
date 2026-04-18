# BlueprintHelper 插件文档

> 版本 2.7 · 2026-04-18 · Unreal Engine 5.6

---

## 一、插件概述

BlueprintHelper 是一个 **UE5 编辑器插件**，核心能力是通过 JSON 中间格式实现蓝图的程序化读写。
插件围绕三层架构设计，最终目标是让 **IDE 中的 AI Agent 可通过 MCP 协议直接操作虚幻编辑器中的蓝图**。

```
┌─────────────────────────────────────────────────┐
│  IDE AI Agent（Copilot / Claude 等）             │
│      ↕ MCP (stdio JSON-RPC)                     │
├─────────────────────────────────────────────────┤
│  Phase 3 · MCP Server（Node.js / TypeScript）    │
│      ↕ TCP 127.0.0.1:54321                      │
├─────────────────────────────────────────────────┤
│  Phase 2 · Bridge Layer（C++ FRunnable）         │
│      ↕ GameThread Dispatch                      │
├─────────────────────────────────────────────────┤
│  Phase 1 · Service Layer（C++ 无头服务）         │
│      ↕ Direct API                               │
├─────────────────────────────────────────────────┤
│  UE5 Editor Subsystems                          │
│  (Kismet / AssetRegistry / BlueprintCompiler)   │
└─────────────────────────────────────────────────┘
```

---

## 二、当前功能范围（已实现）

### 2.1 Phase 1 — Service Layer ✅

| 服务 | 能力 | 限制 |
|------|------|------|
| **ExportService** | 导出单图表 JSON / 完整蓝图 JSON（含变量、函数签名、多图表） | 无选区级导出 |
| **ImportService** | 导入 JSON → 蓝图节点，事务包裹（支持 Undo），可选自动编译 | 部分节点未匹配时已创建的节点仍保留 |
| **ValidationService** | JSON 可解析性、版本检测（1.0–2.2）、节点 ID 唯一性、链接引用完整性 | 不校验节点类型是否存在 Handler |
| **CompileService** | 触发编译、返回 FCompilerResultsLog 结构化诊断 | 无实时进度 |
| **ContextService** | 获取当前聚焦蓝图路径 / 图表名 / 节点数 / 编译状态 | 只读快照，无事件推送 |
| **GraphResolver** | 通过资产路径解析蓝图、按名称定位图表、自动回退到当前聚焦 | 无法创建新蓝图 |

### 2.2 Phase 2 — Bridge Layer ✅

| 组件 | 能力 |
|------|------|
| **BridgeServer** | TCP 127.0.0.1:54321，4 字节大端帧 + UTF-8 JSON，单客户端顺序处理 |
| **BridgeRouter** | 路由 32 条命令：Phase 1-3 基础 6 条 + Phase 4 资产浏览 5 条 + Phase 5 蓝图结构 9 条 + Phase 6 Widget 6 条 + Phase 7 数据资产 6 条 |
| **BridgeProtocol** | 请求解析、响应序列化、9 种错误码 |

### 2.3 Phase 3 — MCP Server ✅

| 组件 | 能力 |
|------|------|
| **32 个 MCP Tools** | Phase 1-3 基础 6 个 + Phase 4 资产浏览 5 个 + Phase 5 蓝图结构 9 个 + Phase 6 Widget 6 个 + Phase 7 数据资产 6 个（详见 2.4–2.7） |
| **2 个 MCP Resources** | `blueprint://rules/json-to-blueprint`（规则文档） / `blueprint://context/active-graph`（上下文） |
| **Bridge Client** | TCP 帧协议客户端，每次请求独立连接 |

### 2.4 Phase 4 — Asset Browsing ✅

| 服务 | 能力 |
|------|------|
| **AssetBrowseService** | 资产浏览、搜索、打开、保存、信息查询 |

| Bridge 命令 | MCP Tool | 说明 |
|-------------|----------|------|
| `open_asset` | `blueprint_open_asset` | 按路径打开任意资产编辑器 |
| `list_assets` | `blueprint_list_assets` | 列出目录下资产（支持类型/名称过滤、递归、分页） |
| `search_assets` | `blueprint_search_assets` | 按关键词搜索资产 |
| `save_asset` | `blueprint_save_asset` | 保存指定资产到磁盘 |
| `get_asset_info` | `blueprint_get_asset_info` | 获取资产详情（类型、父类、磁盘大小） |

### 2.5 Phase 5 — Blueprint Structure Operations ✅

| 服务 | 能力 |
|------|------|
| **BlueprintStructureService** | 蓝图结构查询与精细操作，委托现有 OperationHandler 执行 |

| Bridge 命令 | MCP Tool | 说明 |
|-------------|----------|------|
| `list_graphs` | `blueprint_list_graphs` | 列出蓝图所有图表（EventGraph/Function/Macro）及节点数 |
| `list_variables` | `blueprint_list_variables` | 列出蓝图成员变量（类型、默认值、分类） |
| `list_event_dispatchers` | `blueprint_list_event_dispatchers` | 列出事件分发器及参数签名 |
| `add_variable` | `blueprint_add_variable` | 添加成员变量（支持类型、默认值、标记） |
| `remove_variable` | `blueprint_remove_variable` | 删除成员变量（幂等） |
| `add_graph` | `blueprint_add_graph` | 添加函数/宏图表（支持输入输出参数） |
| `remove_graph` | `blueprint_remove_graph` | 删除图表（禁止删除 EventGraph，幂等） |
| `add_event_dispatcher` | `blueprint_add_event_dispatcher` | 添加事件分发器（支持带类型参数） |
| `delete_nodes` | `blueprint_delete_nodes` | 按 ID 删除图表中的节点（保护 Entry/Result） |

### 2.6 Phase 6 — UMG Widget Operations ✅

| 服务 | 能力 |
|------|------|
| **WidgetService** | UMG Widget Tree 查询与操作，通过 FProperty 反射实现通用属性读写 |

| Bridge 命令 | MCP Tool | 说明 |
|-------------|----------|------|
| `get_widget_tree` | `blueprint_get_widget_tree` | 获取 WidgetBlueprint 完整 Widget 层级树（名称、类型、父子、Slot、深度） |
| `add_widget` | `blueprint_add_widget` | 向面板添加子 Widget（支持所有非抽象 UWidget 子类，按类名创建） |
| `remove_widget` | `blueprint_remove_widget` | 移除 Widget 及其子树 |
| `move_widget` | `blueprint_move_widget` | 跨面板移动 Widget（支持指定插入位置，防止循环引用） |
| `get_widget_properties` | `blueprint_get_widget_properties` | 通过 FProperty 反射获取 Widget 所有可编辑属性及当前值 |
| `set_widget_property` | `blueprint_set_widget_property` | 通过 `FProperty::ImportText_Direct` 设置任意属性值 |

### 2.7 Phase 7 — DataAsset & DataTable Operations ✅

| 服务 | 能力 |
|------|------|
| **PropertyReflectionService** | 通用 UObject 属性反射读写，支持所有 FProperty 子类型（Struct/Array/Map/Enum/Object/SoftObject） |
| **DataTableService** | DataTable 行 CRUD 操作，通过 FDataTableEditorUtils 实现行添加/删除/更新 |

| Bridge 命令 | MCP Tool | 说明 |
|-------------|----------|------|
| `get_object_properties` | `blueprint_get_object_properties` | 通过 FProperty 反射获取任意 UObject（含 DataAsset）的所有可编辑属性及当前值、类型、分类 |
| `set_object_property` | `blueprint_set_object_property` | 通过 `FProperty::ImportText_Direct` 设置任意 UObject 属性值 |
| `get_datatable_rows` | `blueprint_get_datatable_rows` | 获取 DataTable 行数据（含列 Schema），支持行名过滤 |
| `add_datatable_row` | `blueprint_add_datatable_row` | 添加 DataTable 新行（自动初始化 + 字段赋值，失败时回滚） |
| `update_datatable_row` | `blueprint_update_datatable_row` | 更新已有行的指定字段（部分更新） |
| `delete_datatable_row` | `blueprint_delete_datatable_row` | 删除 DataTable 行 |

### 2.8 支持的节点类型（27 个 Handler）

| 类别 | 节点 |
|------|------|
| **函数调用** | `K2Node_CallFunction`（含 Pure） |
| **变量** | `K2Node_VariableGet`、`K2Node_VariableSet` |
| **流程控制** | `K2Node_IfThenElse`、`K2Node_ExecutionSequence` |
| **宏** | `K2Node_MacroInstance`（ForLoop / ForEachLoop / WhileLoop / Sequence / Branch / Gate / DoOnce / DoN / FlipFlop / IsValid） |
| **事件** | `K2Node_Event`（BeginPlay / Tick 等）、`K2Node_CustomEvent`、`K2Node_ComponentBoundEvent` |
| **委托** | `K2Node_CallDelegate` / `AddDelegate` / `RemoveDelegate` / `ClearDelegate` / `AssignDelegate` / `CreateDelegate` |
| **容器** | `K2Node_MakeArray` / `MakeSet` / `MakeMap`、`K2Node_GetArrayItem` |
| **结构体** | `K2Node_MakeStruct`、`K2Node_BreakStruct` |
| **其他** | `K2Node_Self`、`K2Node_DynamicCast`、`K2Node_SpawnActorFromClass`、`K2Node_FormatText`、`K2Node_Timeline`、`K2Node_Literal`、`K2Node_Knot` |

### 2.9 蓝图级操作（JSON `blueprint_operations`，Import 路径可用）

| 操作 | 说明 |
|------|------|
| `add_member_variable` | 添加成员变量 |
| `add_function_graph` | 添加函数图表 |
| `add_macro_graph` | 添加宏图表 |
| `add_event_dispatcher` | 添加事件分发器 |
| `remove_member_variable` | 删除成员变量 |
| `remove_graph` | 删除图表 |

---

## 三、"全覆盖编辑器操作"差距分析

**目标愿景**：打开 UE 编辑器后，AI Agent 通过 MCP 可完成以下全部操作——

```
从 Content Browser 打开蓝图 / UMG Widget / DataAsset / DataTable →
浏览和修改内容 → 保存 → 编译 → 运行验证
```

下表列出当前状态与目标之间的差距：

### 3.1 资产级操作

| 操作 | 当前 | 差距 | 优先级 |
|------|------|------|--------|
| 按路径打开蓝图并聚焦编辑器 | ✅ Phase 4 `open_asset` | — | — |
| 列出 Content Browser 资产 | ✅ Phase 4 `list_assets` | — | — |
| 创建新蓝图资产 | ❌ 无 | 需 `create_blueprint` 命令（FKismetEditorUtilities） | 🟡 中 |
| 保存蓝图 | ✅ Phase 4 `save_asset` | — | — |
| 删除资产 | ❌ 无 | 需 `delete_asset` 命令 | 🟢 低 |
| 复制/重命名资产 | ❌ 无 | 需 `rename_asset` / `duplicate_asset` | 🟢 低 |

### 3.2 蓝图图表操作

| 操作 | 当前 | 差距 | 优先级 |
|------|------|------|--------|
| 导出蓝图图表 → JSON | ✅ 已实现 | — | — |
| 导入 JSON → 蓝图图表 | ✅ 已实现 | — | — |
| 编译蓝图 | ✅ 已实现 | — | — |
| 读取编辑器上下文 | ✅ 已实现 | — | — |
| 列出蓝图中的所有图表 | ✅ Phase 5 `list_graphs` | — | — |
| 列出蓝图成员变量 | ✅ Phase 5 `list_variables` | — | — |
| 添加/删除变量和图表 | ✅ Phase 5（6 个独立命令） | — | — |
| 读取单个节点详情 | ❌ 无 | 需 `get_node_details` 命令 | 🟢 低 |
| 删除选中节点 | ✅ Phase 5 `delete_nodes` | — | — |

### 3.3 UMG Widget Blueprint

| 操作 | 当前 | 差距 | 优先级 |
|------|------|------|--------|
| 打开 Widget Blueprint | ✅ Phase 4 `open_asset` 支持所有资产类型 | — | — |
| 读取 Widget 层级树 | ✅ Phase 6 `get_widget_tree` | — | — |
| 添加/删除/移动 Widget 组件 | ✅ Phase 6 `add_widget` / `remove_widget` / `move_widget` | — | — |
| 修改 Widget 属性（位置、大小、样式） | ✅ Phase 6 `set_widget_property` / `get_widget_properties` | — | — |
| 编辑 Widget 事件图表（逻辑） | ⚠️ 理论上可用（WidgetBP 也有 EventGraph） | 需验证 GraphResolver 兼容性 | 🟡 中 |
| 导出 Widget 层级为 JSON | ❌ 无 | 需 Widget-specific 导出器 | 🟡 中 |
| 编辑 Widget 绑定（Binding） | ❌ 无 | 需 `set_widget_binding` 命令 | 🟡 中 |

### 3.4 DataAsset

| 操作 | 当前 | 差距 | 优先级 |
|------|------|------|--------|
| 打开 DataAsset | ✅ Phase 4 `open_asset` | — | — |
| 读取 DataAsset 属性 | ✅ Phase 7 `get_object_properties` | — | — |
| 修改 DataAsset 属性 | ✅ Phase 7 `set_object_property` | — | — |
| 列出特定类型的所有 DataAsset | ✅ Phase 4 `list_assets` + 类型过滤 | — | — |

### 3.5 DataTable

| 操作 | 当前 | 差距 | 优先级 |
|------|------|------|--------|
| 打开 DataTable | ✅ Phase 4 `open_asset` | — | — |
| 读取 DataTable 行 | ✅ Phase 7 `get_datatable_rows` | — | — |
| 添加/修改/删除行 | ✅ Phase 7 `add/update/delete_datatable_row` | — | — |
| 导出 DataTable 为 JSON/CSV | ❌ 无 | UE 有内建导出，需包装为命令 | 🟡 中 |
| 导入 JSON/CSV 到 DataTable | ❌ 无 | UE 有内建导入，需包装为命令 | 🟡 中 |

### 3.6 通用编辑器操作

| 操作 | 当前 | 差距 | 优先级 |
|------|------|------|--------|
| 搜索资产（名称/类型/标签） | ✅ Phase 4 `search_assets` | — | — |
| 获取资产依赖/引用关系 | ❌ 无 | 需 `get_references`（AssetRegistry） | 🟢 低 |
| Undo / Redo | ⚠️ Import 有事务包裹 | 需暴露全局 `undo` / `redo` 命令 | 🟡 中 |
| PIE 启动/停止 | ❌ 无 | 需 `play_in_editor` / `stop_pie` | 🟢 低 |
| 执行控制台命令 | ❌ 无 | 需 `exec_console_command` | 🟢 低 |

### 3.7 Bridge/MCP 基础设施

| 项目 | 当前 | 差距 | 优先级 |
|------|------|------|--------|
| `blueprint_operations` 独立暴露 | ✅ Phase 5 已暴露为 9 个独立 Bridge 命令 | — | — |
| 事件推送（资产切换、编译完成等） | ❌ 无 | 需在 Bridge 层实现 Server→Client 推送 | 🟡 中 |
| 多客户端支持 | ❌ 单连接 | 需升级为多客户端或长连接 | 🟢 低 |
| 任务池与异步结果查询 | ⚠️ 架构预留但未启用 | 需实现 `get_task_result` 命令 | 🟡 中 |

---

## 四、差距量化总结

```
当前已覆盖                             目标全覆盖
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
██████████████████████████████████████████░░░░░░░░░░░░░░
~75%                                                100%

已有：32 个 Bridge 命令 / 32 个 MCP Tool / 2 个 MCP Resource
目标：~42 个 Bridge 命令 / ~42 个 MCP Tool / ~8 个 MCP Resource
```

| 领域 | 已有命令 | 缺少命令 | 覆盖率 |
|------|----------|----------|--------|
| 蓝图图表操作 | 15 | 1 | 94% |
| 资产浏览与管理 | 5 | 3 | 63% |
| UMG Widget | 6 | 3 | 67% |
| DataAsset | 2 | 0 | 100% |
| DataTable | 4 | 2 | 67% |
| 通用编辑器 | 0 | 4 | 0% |
| 基础设施升级 | 1 项 | 3 项 | — |

---

## 五、实施计划

按价值/成本排序，分 5 个 Phase 递进实施。每个 Phase 完成后系统可独立使用。

### Phase 4 — 资产浏览与通用操作基座 ✅ 已完成

**目标**：AI 能发现和打开编辑器中的任意资产。

| ID | 命令 | 实现要点 | 工作量 |
|----|------|----------|--------|
| 4.1 | `open_asset` | `GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset()`，支持 Blueprint / WidgetBP / DataAsset / DataTable | 小 |
| 4.2 | `list_assets` | `AssetRegistry.GetAssetsByPath()` + 类型/名称过滤 + 分页 | 小 |
| 4.3 | `search_assets` | `AssetRegistry.GetAssetsByTagValues()` / 全文搜索 | 小 |
| 4.4 | `save_asset` | `UEditorLoadingAndSavingUtils::SavePackages()` | 小 |
| 4.5 | `get_asset_info` | 反射读取 UObject 基础信息（类型、路径、父类、大小） | 小 |

**完成标志**：AI 可执行 `search_assets` → `open_asset` → `get_editor_context` → 完成工作 → `save_asset` 闭环。

**预估**：5 个命令，新增 1 个 Service（`AssetBrowseService`），扩展 BridgeRouter + MCP Tools。

---

### Phase 5 — 蓝图操作补全 ✅ 已完成

**目标**：蓝图操作从“导入/导出整图”升级为精细化操作。

**实际实现**：9 个命令（超过原计划 7 个，新增 `list_event_dispatchers`、`add_event_dispatcher`、`remove_graph` 独立命令）。
新建 `BlueprintStructureService`，委托现有 OperationHandler 执行增删操作。

**目标**：蓝图操作从"导入/导出整图"升级为精细化操作。

| ID | 命令 | 实现要点 | 工作量 |
|----|------|----------|--------|
| 5.1 | `list_graphs` | 遍历 `UBlueprint::UbergraphPages` + `FunctionGraphs` + `MacroGraphs` | 小 |
| 5.2 | `list_variables` | 遍历 `UBlueprint::NewVariables` | 小 |
| 5.3 | `add_variable` | 现有 `AddMemberVariableHandler` 包装为独立命令 | 小 |
| 5.4 | `add_graph` | 现有 `AddFunctionGraphHandler` / `AddMacroGraphHandler` 包装 | 小 |
| 5.5 | `delete_nodes` | `FBlueprintEditorUtils::RemoveNode()` | 中 |
| 5.6 | `add_event_dispatcher` | 现有 Handler 包装 | 小 |
| 5.7 | `remove_variable` / `remove_graph` | 现有 Handler 包装 | 小 |

**完成标志**：AI 可不依赖完整 JSON 导入，按需增删蓝图成员和图表。

**预估**：7 个命令，复用已有 Operation Handler，主要工作在 BridgeRouter 路由新增。

---

### Phase 6 — UMG Widget 操作 ✅ 已完成

**目标**：AI 可读写 Widget Blueprint 的可视化层级（WidgetTree）和事件图表。

**实际实现**：6 个命令，新建 `WidgetService`。
核心技术：
- `UWidgetTree::ConstructWidget` / `FindWidget` / `RemoveWidget` 用于树操作
- `UPanelWidget::AddChild` / `InsertChildAt` / `RemoveChild` 用于父子关系
- `FProperty::ExportText_Direct` / `ImportText_Direct` 用于通用属性反射读写
- 循环引用保护：移动前检查目标不在源子树中

| ID | 命令 | 实现要点 | 工作量 |
|----|------|----------|--------|
| 6.1 | `get_widget_tree` | 遍历 `UWidgetBlueprint::WidgetTree`，输出 JSON 层级 | 中 |
| 6.2 | `add_widget` | `UWidgetTree::FindWidgetChild` + `UWidget::AddChild`，支持按类名创建 | 大 |
| 6.3 | `remove_widget` | `UWidgetTree::RemoveWidget()` | 小 |
| 6.4 | `move_widget` | 修改 Slot 父子关系 | 中 |
| 6.5 | `set_widget_property` | 通用反射属性写入（`FProperty::ImportText`） | 大 |
| 6.6 | `get_widget_property` | 通用反射属性读取 | 中 |

**前置**：Phase 4 的 `open_asset` 已支持 WidgetBlueprint。

**完成标志**：AI 可创建一个完整 UI 页面——添加 Canvas/VerticalBox/Button/TextBlock，设置属性，绑定事件。

**预估**：6 个命令，需新建 `WidgetService`（~600 行），核心难点在通用属性反射。

---

### Phase 7 — DataAsset 与 DataTable 操作 ✅ 已完成

**目标**：AI 可读写数据驱动资产。

**实际实现**：6 个命令，新建 `PropertyReflectionService` + `DataTableService`。
核心技术：
- `TFieldIterator<FProperty>` 遍历 UObject 所有可编辑属性（Edit | BlueprintVisible）
- `FProperty::ExportTextItem_Direct` / `ImportText_Direct` 用于通用属性读写
- `FDataTableEditorUtils::AddRow` / `RemoveRow` 用于 DataTable 行 CRUD
- 添加行失败时自动回滚（删除已创建的行）
- `HandleDataTableChanged` + `BroadcastPostChange` 确保编辑器 UI 同步

| ID | 命令 | 实现要点 | 工作量 |
|----|------|----------|--------|
| 7.1 | `get_object_properties` | 通用 UObject 反射属性读取（FProperty 遍历） | 大 |
| 7.2 | `set_object_property` | 通用 UObject 反射属性写入 | 大 |
| 7.3 | `get_datatable_rows` | `UDataTable::GetRowMap()` + 反射序列化 | 中 |
| 7.4 | `add_datatable_row` | `FDataTableEditorUtils::AddRow()` | 中 |
| 7.5 | `update_datatable_row` | 定位行 + 反射修改 | 中 |
| 7.6 | `delete_datatable_row` | `FDataTableEditorUtils::RemoveRow()` | 小 |

---

### Phase 8 — 事件推送与高级基础设施

**目标**：从请求-响应升级为实时协作。

| ID | 项目 | 实现要点 | 工作量 |
|----|------|----------|--------|
| 8.1 | 事件推送（Server→Client） | Bridge 长连接 + 事件帧；触发：资产切换、编译完成、选区变化 | 大 |
| 8.2 | MCP `resources/updated` 通知 | 将 UE 事件转换为 MCP 资源变更通知 | 中 |
| 8.3 | 任务池与异步结果 | `get_task_result` 命令，长任务进度查询 | 中 |
| 8.4 | 多客户端支持 | BridgeServer 升级为连接池 | 中 |
| 8.5 | Undo / Redo 命令 | `GEditor->Trans->Undo()` / `Redo()` | 小 |
| 8.6 | PIE 控制 | `GEditor->PlayInEditor()` / `EndPlayMap()` | 小 |

**完成标志**：AI 可感知编辑器状态变化，不再需要轮询。

---

## 六、里程碑路线图

```
Phase 4 ─── Phase 5 ─── Phase 6 ─── Phase 7 ─── Phase 8
资产浏览     蓝图补全     UMG Widget   数据资产     事件推送
  ✅           ✅           ✅           ✅
  ├── 5 命令    ├── 9 命令    ├── 6 命令    ├── 6 命令    ├── 6 项
  ├── 1 Service ├── 1 Service ├── 1 Service ├── 2 Service ├── 基础设施
  │             │             │             │             │
  ▼             ▼             ▼             ▼             ▼
"AI 能找到     "AI 能精细   "AI 能搭建   "AI 能改     "AI 实时
 并打开任何     编辑蓝图     UI 界面"     数据表和     感知编辑器
 资产"         逻辑"                     数据资产"    变化"
```

| Phase | 新增命令 | 新增 Service | 核心技术 | 相对工作量 | 状态 |
|-------|----------|-------------|----------|-----------|------|
| **4 — 资产浏览** | 5 | AssetBrowseService | AssetRegistry API | ★☆☆☆☆ | ✅ |
| **5 — 蓝图补全** | 9 | BlueprintStructureService | BridgeRouter + OperationHandler 委托 | ★★☆☆☆ | ✅ |
| **6 — UMG Widget** | 6 | WidgetService | UWidgetTree + 属性反射 | ★★★★☆ | ✅ |
| **7 — 数据资产** | 6 | PropertyReflectionService + DataTableService | 通用 FProperty 反射 | ★★★☆☆ | ✅ |
| **8 — 事件推送** | 6 项 | 基础设施改造 | 长连接 + 事件分发 | ★★★☆☆ | ⬜ 下一步 |

**关键路径**：Phase 4 → Phase 5 → Phase 6 → Phase 7 已全部完成；Phase 8 为基础设施升级，可独立启动。

---

## 七、推荐起步顺序

**已完成**：Phase 4 + Phase 5 + Phase 6 + Phase 7。AI 已具备资产浏览、打开、保存能力，蓝图增量编辑能力，UMG Widget 层级操作能力，以及 DataAsset/DataTable 数据读写能力。

**下一步**：Phase 8（事件推送与高级基础设施）。完成后 AI 可感知编辑器状态变化，不再需要轮询。

**完整路径**：~~Phase 4 → 5 → 6 → 7~~ → 8，剩余约 **8 个新命令 + 基础设施改造**，覆盖率从当前 75% 提升至约 95%。
