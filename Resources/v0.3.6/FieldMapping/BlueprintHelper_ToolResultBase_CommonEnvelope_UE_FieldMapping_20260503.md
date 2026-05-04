# BlueprintHelper ToolResultBase / Common Envelope / Error Protocol UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：ToolResultBase / Common Envelope 字段确认稿  
本文边界：确认所有 Agent-facing MCP 工具共同遵守的 ToolResultBase 外壳、status / ok 语义、data.schema 短命名、target、validation、dry_run、error、conflicts、page、resource_ref，以及 transaction / review / safety 默认不返回规则。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

```text
1. 所有 Agent-facing 工具使用 ToolResultBase 外壳。
2. 顶层 schema 固定 BlueprintHelper.McpToolResult.v1。
3. data.schema 使用短命名。
4. operation 使用稳定短 operation 名。
5. trace_id 是日志追踪 ID，不等于 transaction_id。
6. status 第一版统一使用 completed / applied / no_op / dry_run / failed。
7. ok 表示工具调用是否成功，不等于业务是否通过。
8. 工具自身失败才 ok=false / status=failed / error。
9. diagnostics Markdown 有 Blocking 仍 ok=true / status=completed。
10. compile_result.success=false 仍 ok=true / status=completed。
11. dry_run blocked 仍 ok=true / status=dry_run。
12. 写工具 validation 只返回 should_compile / should_save。
13. validation 不返回 compiled / saved。
14. compile/save/read/runtime/diagnostics/lifecycle/debug/export/query 工具不返回 validation。
15. dry_run passed 默认只返回 result / can_execute。
16. dry_run blocked / failed 返回 blocked_by / conflicts / errors 必要摘要。
17. 成功结果不返回 write_ref / transaction_id / review / safety。
18. conflicts 只在失败/blocked 场景返回，不在成功场景返回空数组。
19. page 用于列表型只读工具，空列表不是失败。
20. resource_ref / bundle_ref / snapshot_ref 不使用本地绝对路径。
21. 写工具 target.asset_path 必须完整，不允许 %{path_filter}。
22. %{path_filter} 只限 find_assets 列表型结果。
```

---

## 1. ToolResultBase 顶层结构

建议所有 Agent-facing 工具外壳统一为：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_widget_property",
  "trace_id": "trace_20260503_2501",
  "status": "applied",
  "modified": true,

  "target": {},

  "data": {
    "schema": "SetWidgetProperty.v1"
  },

  "validation": {
    "should_compile": false,
    "should_save": true
  }
}
```

---

## 2. 顶层字段映射

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `bOk` | `bool` | `ok` | `boolean` | 是 | MCP 工具调用本身是否成功执行。 |
| `Schema` | `FString` | `schema` | `string` | 是 | 顶层固定 `BlueprintHelper.McpToolResult.v1`。 |
| `Operation` | `FString` 或 enum | `operation` | `string` | 是 | 稳定短 operation 名。 |
| `TraceId` | `FString` | `trace_id` | `string` | 是 | 日志追踪 ID，不是 transaction_id。 |
| `Status` | `EBlueprintHelperToolStatus` | `status` | `string enum` | 是 | `completed` / `applied` / `no_op` / `dry_run` / `failed`。 |
| `bModified` | `bool` | `modified` | `boolean` | 是 | 本次工具是否修改 UE 资产或目标状态。 |
| `Target` | `FJsonObject` | `target` | `object` | 按工具 | 操作目标或查询范围。 |
| `Data` | `FJsonObject` | `data` | `object` | 成功/业务结果时 | 工具业务结果。 |
| `Validation` | `FBlueprintHelperValidationHint` | `validation` | `object` | 写工具按需 | 仅 `should_compile` / `should_save`。 |
| `Error` | `FBlueprintHelperToolError` | `error` | `object` | 失败时 | 工具自身失败信息。 |

---

## 3. ok 语义

`ok` 表示 MCP 工具调用本身是否成功执行。

它不等于业务检查是否通过。

以下场景仍可返回：

```text
ok=true
```

```text
diagnostics Markdown 中有 Blocking
compile_result.success=false
dry_run.result=blocked
runtime_profile.status=blocked
check_setup_state.setup_state.status=blocked
```

只有工具自身失败时才返回：

```text
ok=false
status=failed
error
```

---

## 4. status 枚举

第一版统一使用：

```text
completed
applied
no_op
dry_run
failed
```

### 4.1 completed

用于：

```text
读工具成功
诊断工具成功
runtime_profile 成功
compile/save/lifecycle/debug/export 成功
查询工具成功
```

注意：

```text
completed 不代表业务无 Blocking。
diagnostics completed 但 Markdown 可以有 Blocking。
compile completed 但 compile_result.success 可以为 false。
```

### 4.2 applied

用于：

```text
写工具实际应用成功。
```

```json
{
  "ok": true,
  "status": "applied",
  "modified": true
}
```

### 4.3 no_op

用于：

```text
工具执行成功，但无需修改或目标已满足。
```

```json
{
  "ok": true,
  "status": "no_op",
  "modified": false
}
```

常见场景：

```text
save_asset：asset_not_dirty
add_component：reuse_if_exists
add_widget_to_tree：reuse_if_exists
add_data_table_row：reuse_if_exists
stop_pie_session：pie_not_running
open_asset_in_editor：already_open
```

### 4.4 dry_run

用于高风险操作的预执行检查：

```json
{
  "ok": true,
  "status": "dry_run",
  "modified": false,
  "data": {
    "schema": "RemoveWidgetFromTreeDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

dry_run blocked 也仍然是：

```text
ok=true
status=dry_run
modified=false
```

因为工具成功执行了预检。

### 4.5 failed

仅用于工具自身失败，或请求无法执行到业务结果阶段：

```json
{
  "ok": false,
  "status": "failed",
  "modified": false,
  "error": {
    "code": "asset_not_found",
    "stage": "resolve_asset",
    "message": "The requested asset was not found.",
    "retryable": false
  }
}
```

---

## 5. data.schema 短命名规则

`data.schema` 全部使用短命名。

使用：

```text
RuntimeProfile.v1
Diagnostics.v1
FindAssets.v1
AppendBlueprintGraph.v1
AddWidgetToTree.v1
SetDataAssetProperty.v1
CompileBlueprintAsset.v1
SaveAsset.v1
```

不使用：

```text
BlueprintHelper.RuntimeProfile.v1
BlueprintHelper.Tools.UMG.AddWidgetToTree.v1
BlueprintHelper.MCP.SaveAsset.v1
```

---

## 6. target 公共规则

### 6.1 写工具 target 必须完整明确

写工具必须包含明确目标：

```text
asset_path
graph / function / event / custom_event / block / node / pin
```

规则：

```text
1. 不依赖当前编辑器焦点。
2. 不依赖 selected_assets。
3. 不使用压缩 asset_path。
4. 不使用模糊目标。
5. 不默认扩大作用域。
```

写工具示例：

```json
"target": {
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
  "graph": "EG_PhysicsDoor"
}
```

### 6.2 读工具 target 可表达 read_scope

```json
"target": {
  "asset_path": "/Game/UI/WBP_MainMenu",
  "read_scope": "widget_tree"
}
```

### 6.3 查询工具 target 可表达 query_scope/filter/page

```json
"target": {
  "query_scope": "asset_registry",
  "path_filter": "/Game/BlueprintHelperTest",
  "asset_type_filter": "Blueprint",
  "limit": 20
}
```

### 6.4 `%{path_filter}` 限制

只允许 `find_assets.assets[].asset_path` 在列表型只读结果中使用：

```text
%{path_filter}
```

不允许用于：

```text
写工具 target.asset_path
单资产 read 工具 asset_path
compile/save target.asset_path
LogicMD / LogicJson target.asset_path
```

---

## 7. validation 公共规则

只在写工具成功 / no_op 时返回。

```json
"validation": {
  "should_compile": true,
  "should_save": true
}
```

或：

```json
"validation": {
  "should_compile": false,
  "should_save": true
}
```

no_op：

```json
"validation": {
  "should_compile": false,
  "should_save": false
}
```

不返回：

```text
compiled
saved
```

原因：

```text
compile/save 是独立闭环工具。
写工具只指向后续是否需要 compile/save。
```

### 7.1 validation 字段映射

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `bShouldCompile` | `bool` | `validation.should_compile` | `boolean` | 是 | 后续是否需要 compile 工具。 |
| `bShouldSave` | `bool` | `validation.should_save` | `boolean` | 是 | 后续是否需要 save 工具。 |

不返回：

```text
validation.compiled
validation.saved
```

### 7.2 不返回 validation 的工具

```text
compile_blueprint_asset
save_asset
read 工具
runtime_profile
diagnostics
Transaction Journal Query
Editor Lifecycle
Debug / Export
Project Context
Asset Discovery
```

---

## 8. dry_run 公共规则

dry_run 结果结构统一：

```json
"dry_run": {
  "result": "passed",
  "can_execute": true
}
```

或：

```json
"dry_run": {
  "result": "blocked",
  "can_execute": false,
  "blocked_by": [
    "external_dependents_exist"
  ],
  "conflicts": [
    {
      "code": "external_dependents_exist",
      "message": "External dependents exist for the requested target."
    }
  ],
  "errors": []
}
```

### 8.1 dry_run.result 枚举

```text
passed
blocked
failed
```

含义：

```text
passed：
- 预检通过，可执行。

blocked：
- 工具成功完成预检，但安全策略 / 依赖 / 冲突阻止执行。

failed：
- 预检逻辑无法完成，但仍属于 dry_run 业务结果。
```

如果 dry_run 工具自身失败，则：

```text
ok=false
status=failed
error
```

### 8.2 dry_run 成功极简规则

```text
dry_run passed 默认只返回 result / can_execute。
不返回 would_create / would_delete / would_modify 全量列表。
```

blocked / failed 时返回：

```text
blocked_by
conflicts
errors
```

且只返回必要摘要。

---

## 9. error 公共规则

工具自身失败时返回：

```json
"error": {
  "code": "asset_not_found",
  "stage": "resolve_asset",
  "message": "The requested asset was not found.",
  "retryable": false
}
```

### 9.1 error 字段映射

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Code` | `FString` 或 enum | `error.code` | `string` | 是 | 稳定错误码。 |
| `Stage` | `FString` 或 enum | `error.stage` | `string` | 是 | 失败阶段。 |
| `Message` | `FString` | `error.message` | `string` | 是 | 简短人类可读信息。 |
| `bRetryable` | `bool` | `error.retryable` | `boolean` | 是 | 是否可直接重试。 |
| `Conflicts` | `TArray<FBlueprintHelperConflictItem>` | `error.conflicts` | `array<object>` | 可选 | 冲突列表。 |
| `FailedItem` | `FBlueprintHelperFailedItem` | `error.failed_item` | `object` | 可选 | 失败对象摘要。 |

不返回：

```text
stack_trace
本地绝对路径
完整 payload
完整 settings.json
Token / secret
```

---

## 10. conflicts 公共规则

conflicts 只在失败、blocked、dry_run blocked 或高风险冲突场景返回。

示例：

```json
"conflicts": [
  {
    "code": "property_not_found",
    "property": "DamageWrong"
  }
]
```

或：

```json
"conflicts": [
  {
    "code": "slot_type_mismatch",
    "widget_name": "StartButton",
    "slot_type": "VerticalBoxSlot",
    "property": "Anchors"
  }
]
```

规则：

```text
1. conflicts 只返回定位问题所需字段。
2. 成功结果不返回 conflicts=[]。
3. 不返回完整 before / after。
4. 不返回全量扫描结果。
```

---

## 11. page 公共规则

列表型读工具可返回：

```json
"page": {
  "limit": 20,
  "has_more": false
}
```

有下一页时：

```json
"page": {
  "limit": 20,
  "has_more": true,
  "next_cursor": "cursor_abc"
}
```

适用：

```text
find_assets
list_blueprint_helper_transactions
其他列表型只读查询
```

空列表：

```text
ok=true
status=completed
items=[]
```

不是失败。

---

## 12. resource/ref 公共规则

大 payload 不内联。

使用：

```text
resource_ref
bundle_ref
snapshot_ref
```

不返回：

```text
本地绝对路径
完整大 payload
bundle bytes
```

统一 URI 建议：

```text
resource://blueprinthelper/...
```

---

## 13. transaction / review / safety 默认不返回

普通成功结果不返回：

```text
write_ref
transaction_id
journal_recorded
review
safety
safety_profile
rollback_data
```

说明：

```text
1. transaction / Journal / Review 是 UE 侧审计系统。
2. 普通 Agent 执行不依赖 transaction_id。
3. 查询 / Debug / Rollback 专用场景可通过专门工具或内部数据使用 transaction_id。
4. safety_profile 只从 runtime_profile 异常或配置上下文理解，不在单个工具结果中重复返回。
```

---

## 14. UE/MCP 建议结构体

```cpp
struct FBlueprintHelperToolResultBase
{
    bool bOk = false;
    FString Schema; // BlueprintHelper.McpToolResult.v1
    FString Operation;
    FString TraceId;
    FString Status; // completed | applied | no_op | dry_run | failed
    bool bModified = false;

    TSharedPtr<FJsonObject> Target;
    TSharedPtr<FJsonObject> Data;

    TOptional<FBlueprintHelperValidationHint> Validation;
    TOptional<FBlueprintHelperToolError> Error;
};

struct FBlueprintHelperValidationHint
{
    bool bShouldCompile = false;
    bool bShouldSave = false;
};

struct FBlueprintHelperToolError
{
    FString Code;
    FString Stage;
    FString Message;
    bool bRetryable = false;

    TArray<FBlueprintHelperConflictItem> Conflicts;
    TOptional<FBlueprintHelperFailedItem> FailedItem;
};

struct FBlueprintHelperConflictItem
{
    FString Code;
    TSharedPtr<FJsonObject> Fields;
};

struct FBlueprintHelperPageInfo
{
    int32 Limit = 20;
    bool bHasMore = false;
    FString NextCursor;
};
```

明确不包含：

```cpp
FString TransactionId;
FString WriteRef;
FString ReviewStatus;
FString SafetyProfile;
bool bCompiled;
bool bSaved;
FString LocalAbsolutePath;
```

---

## 15. 验收标准

```text
1. 所有 Agent-facing 工具返回 ToolResultBase。
2. 顶层 schema 固定 BlueprintHelper.McpToolResult.v1。
3. data.schema 使用短命名。
4. operation 使用稳定短名。
5. trace_id 不等于 transaction_id。
6. status 只使用 completed / applied / no_op / dry_run / failed。
7. ok=false 只表示工具自身失败。
8. 业务 blocked / compile failed / diagnostics Blocking 不等同于工具失败。
9. 写工具 validation 只返回 should_compile / should_save。
10. validation 不返回 compiled / saved。
11. dry_run passed 默认极简。
12. dry_run blocked / failed 只返回必要冲突摘要。
13. 成功结果不返回 transaction / review / safety。
14. conflicts 不在成功场景返回空数组。
15. page 支持空列表成功。
16. resource/ref 不使用本地绝对路径。
17. 写工具 target.asset_path 必须完整。
18. %{path_filter} 只限 find_assets 列表型结果。
