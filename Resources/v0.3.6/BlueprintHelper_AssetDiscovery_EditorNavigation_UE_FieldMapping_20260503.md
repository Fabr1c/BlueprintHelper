# BlueprintHelper Asset Discovery / Editor Navigation UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：Asset Discovery / Editor Navigation 字段确认稿  
本文边界：确认资产查找、资产摘要读取、资产编辑器打开、编辑器上下文读取工具的 Agent-facing 返回字段、UE 侧结构体映射、路径压缩规则、空结果、失败结果和只读边界。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

Asset Discovery / Editor Navigation 工具簇采用以下字段口径：

```text
1. 增加只读查询工具 find_assets。
2. find_assets 成功返回 assets[]，每项保留 asset_path / asset_type / asset_class。
3. 当 target.path_filter 存在且结果路径以该前缀开头时，find_assets 的 assets[].asset_path 允许使用 %{path_filter} 压缩前缀。
4. Agent 后续使用压缩 asset_path 前，必须展开为完整路径。
5. find_assets 不返回 asset_name / package_path / object_path。
6. find_assets 空结果 status=completed / assets=[]，不是失败。
7. 增加 read_asset_summary。
8. read_asset_summary 只返回 asset_path / asset_type / asset_class / loaded。
9. read_asset_summary 不返回 class settings / graph list / dependencies。
10. 增加 open_asset_in_editor。
11. open_asset_in_editor 是 Editor UI 操作，modified=false。
12. open_asset_in_editor 成功返回 opened / already_open。
13. open_asset_in_editor 不返回 validation / write_ref / transaction_id。
14. get_editor_context 可作为只读诊断工具后置或保留。
15. get_editor_context 返回 open_assets / selected_assets。
16. Agent 不得用当前编辑器焦点或选中资产作为写入目标事实来源。
17. 所有 data.schema 使用短命名。
```

---

## 1. 工具簇定位

Asset Discovery / Editor Navigation 负责：

```text
查找资产
读取资产轻量摘要
在 UE 编辑器中打开 / 定位资产
读取编辑器当前上下文
```

它不负责：

```text
创建资产
修改资产
保存资产
编译资产
写 Transaction Journal
写 Review
推断写入目标
读取蓝图图表逻辑
读取 UMG WidgetTree
读取 DataTable 行内容
```

---

## 2. 通用返回原则

所有工具默认：

```text
modified=false
```

所有工具不返回：

```text
write_ref
transaction_id
journal_recorded
review
safety
validation
```

所有 `data.schema` 使用短命名。

---

# 3. find_assets

## 3.1 工具定位

`find_assets` 负责：

```text
按路径、类型、名称关键词查找 UE 资产。
```

它不读取资产内部结构。

---

## 3.2 operation

```json
"operation": "find_assets"
```

---

## 3.3 target 字段

```json
"target": {
  "query_scope": "asset_registry",
  "path_filter": "/Game/BlueprintHelperTest",
  "asset_type_filter": "Blueprint"
}
```

字段映射：

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `QueryScope` | `EBlueprintHelperAssetQueryScope` | `target.query_scope` | `string enum` | 是 | 固定或默认 `asset_registry`。 |
| `PathFilter` | `FString` | `target.path_filter` | `string` | 可选 | 路径过滤前缀。 |
| `AssetTypeFilter` | `FString` 或 enum | `target.asset_type_filter` | `string` | 可选 | 资产类型过滤，例如 `Blueprint`。 |
| `NameFilter` | `FString` | `target.name_filter` | `string` | 可选 | 名称关键词过滤。 |
| `Limit` | `int32` | `target.limit` | `number` | 可选 | 分页大小。默认建议 20。 |
| `Cursor` | `FString` | `target.cursor` | `string` | 可选 | 分页游标。 |

---

## 3.4 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "find_assets",
  "trace_id": "trace_20260503_3501",
  "status": "completed",
  "modified": false,

  "target": {
    "query_scope": "asset_registry",
    "path_filter": "/Game/BlueprintHelperTest",
    "asset_type_filter": "Blueprint"
  },

  "data": {
    "schema": "FindAssets.v1",
    "assets": [
      {
        "asset_path": "%{path_filter}/Door/BP_BH_PhysicsDoor",
        "asset_type": "Blueprint",
        "asset_class": "/Script/Engine.Blueprint"
      },
      {
        "asset_path": "%{path_filter}/UI/WBP_MainMenu",
        "asset_type": "WidgetBlueprint",
        "asset_class": "/Script/UMGEditor.WidgetBlueprint"
      }
    ],
    "page": {
      "limit": 20,
      "has_more": false
    }
  }
}
```

---

## 3.5 path_filter 压缩规则

当同时满足：

```text
target.path_filter 存在
assets[].asset_path 以 target.path_filter 为前缀
```

则 `assets[].asset_path` 允许使用：

```text
%{path_filter}
```

作为路径前缀别名。

示例：

```text
target.path_filter = /Game/BlueprintHelperTest
assets[].asset_path = %{path_filter}/Door/BP_BH_PhysicsDoor
```

展开后：

```text
/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor
```

不得使用：

```text
%{filter}
```

原因：

```text
filter 语义过宽，会和 asset_type_filter / status_filter 等字段混淆。
```

限制：

```text
1. 只限 find_assets 这类列表型只读结果。
2. 单资产工具仍返回完整 asset_path。
3. 写工具 target.asset_path 永远必须是完整路径。
4. compile / save / read_class_settings / LogicMD / LogicJson 等精确目标工具不使用 %{path_filter}。
```

---

## 3.6 assets[] 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetPath` | `FString` | `data.assets[].asset_path` | `string` | 是 | 资产路径；find_assets 可使用 `%{path_filter}` 压缩。 |
| `AssetType` | `FString` 或 enum | `data.assets[].asset_type` | `string` | 是 | 资产类型。 |
| `AssetClass` | `FString` | `data.assets[].asset_class` | `string` | 是 | 资产 class 路径。 |

不返回：

```text
asset_name
package_path
object_path
thumbnail
tags 全量
dependencies
referencers
loaded object pointer
```

---

## 3.7 page 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Limit` | `int32` | `data.page.limit` | `number` | 是 | 当前页大小。 |
| `bHasMore` | `bool` | `data.page.has_more` | `boolean` | 是 | 是否还有下一页。 |
| `NextCursor` | `FString` | `data.page.next_cursor` | `string` | 有下一页时 | 下一页游标。 |

---

## 3.8 空结果

空结果不是失败：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "find_assets",
  "trace_id": "trace_20260503_3502",
  "status": "completed",
  "modified": false,

  "target": {
    "query_scope": "asset_registry",
    "path_filter": "/Game/Missing",
    "asset_type_filter": "Blueprint"
  },

  "data": {
    "schema": "FindAssets.v1",
    "assets": [],
    "page": {
      "limit": 20,
      "has_more": false
    }
  }
}
```

---

## 3.9 查询失败

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "find_assets",
  "trace_id": "trace_20260503_3503",
  "status": "failed",
  "modified": false,

  "target": {
    "query_scope": "asset_registry"
  },

  "error": {
    "code": "asset_registry_unavailable",
    "stage": "query_asset_registry",
    "message": "The UE Asset Registry is unavailable.",
    "retryable": true
  }
}
```

---

# 4. read_asset_summary

## 4.1 工具定位

`read_asset_summary` 负责：

```text
读取一个资产的轻量摘要。
```

它不读取资产内部业务结构。

---

## 4.2 operation

```json
"operation": "read_asset_summary"
```

---

## 4.3 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_asset_summary",
  "trace_id": "trace_20260503_3601",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "read_scope": "asset_summary"
  },

  "data": {
    "schema": "ReadAssetSummary.v1",
    "asset": {
      "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
      "asset_type": "Blueprint",
      "asset_class": "/Script/Engine.Blueprint",
      "loaded": true
    }
  }
}
```

---

## 4.4 asset 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetPath` | `FString` | `data.asset.asset_path` | `string` | 是 | 完整资产路径。 |
| `AssetType` | `FString` 或 enum | `data.asset.asset_type` | `string` | 是 | 资产类型。 |
| `AssetClass` | `FString` | `data.asset.asset_class` | `string` | 是 | 资产 class。 |
| `bLoaded` | `bool` | `data.asset.loaded` | `boolean` | 是 | 资产是否已加载。 |

不返回：

```text
package_path
asset_name
generated_class
implemented_interfaces
class defaults
graph list
function list
full tags
dependencies
referencers
```

如果要读这些：

```text
generated_class / interfaces / class defaults -> read_class_settings
graph / function 逻辑 -> LogicMD / LogicJson
dependencies / referencers -> 依赖查询工具，后置
```

---

## 4.5 资产不存在

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_asset_summary",
  "trace_id": "trace_20260503_3602",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_Missing",
    "read_scope": "asset_summary"
  },

  "error": {
    "code": "asset_not_found",
    "stage": "resolve_asset",
    "message": "The requested asset was not found.",
    "retryable": false
  }
}
```

---

# 5. open_asset_in_editor

## 5.1 工具定位

`open_asset_in_editor` 负责：

```text
在 UE 编辑器中打开或聚焦一个资产。
```

它是 Editor UI 操作，不是资产写操作。

---

## 5.2 operation

```json
"operation": "open_asset_in_editor"
```

---

## 5.3 成功打开

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "open_asset_in_editor",
  "trace_id": "trace_20260503_3701",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "editor_scope": "asset_editor"
  },

  "data": {
    "schema": "OpenAssetInEditor.v1",
    "open_result": {
      "opened": true,
      "already_open": false
    }
  }
}
```

---

## 5.4 no_op：资产已经打开

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "open_asset_in_editor",
  "trace_id": "trace_20260503_3702",
  "status": "no_op",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "editor_scope": "asset_editor"
  },

  "data": {
    "schema": "OpenAssetInEditor.v1",
    "open_result": {
      "opened": false,
      "already_open": true
    }
  }
}
```

---

## 5.5 open_result 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `bOpened` | `bool` | `data.open_result.opened` | `boolean` | 是 | 本次是否打开资产。 |
| `bAlreadyOpen` | `bool` | `data.open_result.already_open` | `boolean` | 是 | 资产是否原本已打开。 |

---

## 5.6 打开失败

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "open_asset_in_editor",
  "trace_id": "trace_20260503_3703",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_Missing",
    "editor_scope": "asset_editor"
  },

  "error": {
    "code": "asset_not_found",
    "stage": "resolve_asset",
    "message": "The requested asset was not found.",
    "retryable": false
  }
}
```

---

# 6. get_editor_context

## 6.1 工具定位

`get_editor_context` 负责：

```text
读取当前 UE 编辑器上下文，例如当前打开资产、当前选中资产。
```

它不允许作为写入目标事实来源。

---

## 6.2 operation

```json
"operation": "get_editor_context"
```

---

## 6.3 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "get_editor_context",
  "trace_id": "trace_20260503_3801",
  "status": "completed",
  "modified": false,

  "target": {
    "read_scope": "editor_context"
  },

  "data": {
    "schema": "GetEditorContext.v1",
    "editor_context": {
      "open_assets": [
        "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
      ],
      "selected_assets": [
        "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
      ]
    }
  }
}
```

---

## 6.4 editor_context 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `OpenAssets` | `TArray<FString>` | `data.editor_context.open_assets` | `array<string>` | 是 | 当前打开资产路径。 |
| `SelectedAssets` | `TArray<FString>` | `data.editor_context.selected_assets` | `array<string>` | 是 | 当前选中资产路径。 |

关键规则：

```text
Agent 不得仅凭 selected_assets / open_assets 执行写入。
写工具仍必须显式指定 asset_path 和目标图表 / 函数 / 事件。
```

---

# 7. error 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Code` | `EBlueprintHelperAssetDiscoveryErrorCode` | `error.code` | `string enum` | 是 | 稳定错误码。 |
| `Stage` | `EBlueprintHelperAssetDiscoveryStage` | `error.stage` | `string enum` | 是 | 失败阶段。 |
| `Message` | `FString` | `error.message` | `string` | 是 | 简短人类可读信息。 |
| `bRetryable` | `bool` | `error.retryable` | `boolean` | 是 | Agent 是否可直接重试。 |
| `Conflicts` | `TArray<FBlueprintHelperConflictItem>` | `error.conflicts` | `array<object>` | 可选 | 冲突列表。 |

---

# 8. UE 侧建议结构体

```cpp
struct FBlueprintHelperFindAssetsResultData
{
    FString Schema; // FindAssets.v1
    TArray<FBlueprintHelperAssetListItem> Assets;
    FBlueprintHelperPageInfo Page;
};

struct FBlueprintHelperAssetListItem
{
    FString AssetPath; // May contain %{path_filter} only in find_assets list result.
    FString AssetType;
    FString AssetClass;
};

struct FBlueprintHelperReadAssetSummaryResultData
{
    FString Schema; // ReadAssetSummary.v1
    FBlueprintHelperAssetSummary Asset;
};

struct FBlueprintHelperAssetSummary
{
    FString AssetPath;
    FString AssetType;
    FString AssetClass;
    bool bLoaded = false;
};

struct FBlueprintHelperOpenAssetInEditorResultData
{
    FString Schema; // OpenAssetInEditor.v1
    FBlueprintHelperOpenAssetResult OpenResult;
};

struct FBlueprintHelperOpenAssetResult
{
    bool bOpened = false;
    bool bAlreadyOpen = false;
};

struct FBlueprintHelperGetEditorContextResultData
{
    FString Schema; // GetEditorContext.v1
    FBlueprintHelperEditorContext EditorContext;
};

struct FBlueprintHelperEditorContext
{
    TArray<FString> OpenAssets;
    TArray<FString> SelectedAssets;
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
FBlueprintHelperWriteRef
FString TransactionId
FBlueprintHelperValidationResult
FString PackagePath
FString ObjectPath
FString AssetName
```

---

# 9. 验收标准

```text
1. find_assets 是只读工具，modified=false。
2. find_assets 返回 asset_path / asset_type / asset_class。
3. find_assets 不返回 asset_name / package_path / object_path。
4. find_assets 可在 assets[].asset_path 中使用 %{path_filter} 压缩。
5. %{path_filter} 只限 find_assets 列表型结果。
6. 单资产工具返回完整 asset_path。
7. 写工具 target.asset_path 必须是完整路径。
8. find_assets 空结果不是失败。
9. read_asset_summary 只返回轻量 asset 摘要。
10. open_asset_in_editor 是 UI 操作，modified=false。
11. open_asset_in_editor 不返回 validation / transaction。
12. get_editor_context 不可作为写入目标事实来源。
13. 所有 data.schema 使用短命名。
