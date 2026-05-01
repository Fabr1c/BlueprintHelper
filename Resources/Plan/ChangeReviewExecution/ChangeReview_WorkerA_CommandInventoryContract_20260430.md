# Worker A Command Inventory And Review Contract Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** 建立 Bridge 命令清单、命令风险分类和 review 契约，供后续 Router 包裹与 MCP 文档复用。

**Architecture:** 先用文档列出当前 43 个 MCP 工具和对应 Bridge 命令，再在 C++ 层增加轻量命令分类辅助，不改变任何写命令行为。

**Tech Stack:** UE5 C++、Bridge Router、Markdown planning docs。

---

## 写入边界

允许新增：

```text
Resources/Plan/ChangeReviewExecution/ChangeReview_WriteCommandInventory_20260430.md
Source/BlueprintHelper/Public/Services/BlueprintHelperChangeReviewCommandClassifier.h
Source/BlueprintHelper/Private/Services/BlueprintHelperChangeReviewCommandClassifier.cpp
```

允许修改：

```text
Source/BlueprintHelper/BlueprintHelper.Build.cs
```

不允许修改：

```text
Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp
MCPServer/src/tools.ts
```

## 命令分类规则

分类值：

```cpp
enum class EBlueprintHelperReviewCommandKind : uint8
{
	Read,
	Write,
	UI,
	EditorProcess,
	Build,
	Unknown
};
```

风险值：

```cpp
enum class EBlueprintHelperReviewCommandRisk : uint8
{
	Low,
	Medium,
	High
};
```

目标资产来源：

```cpp
struct FBlueprintHelperReviewCommandSpec
{
	FString CommandName;
	EBlueprintHelperReviewCommandKind Kind = EBlueprintHelperReviewCommandKind::Unknown;
	EBlueprintHelperReviewCommandRisk Risk = EBlueprintHelperReviewCommandRisk::Medium;
	TArray<FString> TargetAssetFields;
	bool bCanUseActiveContext = false;
	bool bAffectsSavedFiles = false;
	bool bRequiresExplicitUserApproval = false;
};
```

## 预期分类

Read：

```text
get_rule_markdown
get_editor_context
validate_json
export_to_json
export_logic
list_assets
search_assets
get_asset_info
list_graphs
list_variables
list_event_dispatchers
get_widget_tree
get_widget_properties
get_object_properties
get_datatable_rows
```

Write：

```text
import_json
save_asset
add_variable
remove_variable
add_graph
remove_graph
add_event_dispatcher
delete_nodes
add_widget
remove_widget
move_widget
set_widget_property
set_object_property
add_datatable_row
update_datatable_row
delete_datatable_row
create_blueprint
```

UI：

```text
open_asset
```

EditorProcess：

```text
undo
redo
play_in_editor
stop_pie
exec_console_command
close_editor
```

Build：

```text
blueprint_build_project
blueprint_open_editor
```

注意：`blueprint_build_project` 和 `blueprint_open_editor` 是 MCPServer 本地工具，不是 Bridge 命令。

## 任务

### Task A1: 生成命令清单文档

**Files:**

- Create: `Resources/Plan/ChangeReviewExecution/ChangeReview_WriteCommandInventory_20260430.md`

- [ ] 列出每个 MCP 工具、Bridge 命令、分类、目标资产字段、风险、是否进入 review。
- [ ] 标记目标资产不明确的命令为 high risk。
- [ ] 明确 `save_asset`、`close_editor bSaveAll`、`undo/redo` 不应自动进入普通 pending review。

验收：

- 文档包含所有当前工具。
- 文档明确 read/write/ui/editor-process/build。
- 文档能直接指导 Worker D 的包裹逻辑。

### Task A2: 新增命令分类器接口

**Files:**

- Create: `Source/BlueprintHelper/Public/Services/BlueprintHelperChangeReviewCommandClassifier.h`
- Create: `Source/BlueprintHelper/Private/Services/BlueprintHelperChangeReviewCommandClassifier.cpp`

- [ ] 定义 `EBlueprintHelperReviewCommandKind`。
- [ ] 定义 `EBlueprintHelperReviewCommandRisk`。
- [ ] 定义 `FBlueprintHelperReviewCommandSpec`。
- [ ] 实现 `static FBlueprintHelperReviewCommandSpec GetCommandSpec(const FString& CommandName)`。
- [ ] 实现 `static bool IsWriteCommand(const FString& CommandName)`。

验收：

- 所有 Bridge 命令都返回非 Unknown spec。
- 未识别命令返回 Unknown 和 High risk。
- 不触碰 Router 行为。

### Task A3: 编译依赖检查

**Files:**

- Modify only if needed: `Source/BlueprintHelper/BlueprintHelper.Build.cs`

- [ ] 如果新增文件只依赖 Core/Json，则不修改 Build.cs。
- [ ] 若编译缺依赖，只添加最小模块，不添加未使用的大模块。

验收命令：

```powershell
& 'G:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

预期：编译通过。

