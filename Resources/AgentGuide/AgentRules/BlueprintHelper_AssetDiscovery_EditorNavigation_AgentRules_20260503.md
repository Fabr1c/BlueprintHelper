# BlueprintHelper Agent 侧规则：Asset Discovery / Editor Navigation 使用规范

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：Asset Discovery / Editor Navigation Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释资产查找、资产摘要读取、资产编辑器打开、编辑器上下文读取工具，包括 `%{path_filter}` 压缩路径、只读边界、无 validation / transaction 返回规则。UE 字段映射见独立文档。

---

## 1. 工具簇职责

Asset Discovery / Editor Navigation 负责：

```text
查找资产
读取资产轻量摘要
打开或聚焦 UE 资产编辑器
读取当前编辑器上下文
```

它不负责：

```text
创建资产
修改资产
保存资产
编译资产
读取蓝图图表逻辑
读取 UMG WidgetTree
读取 DataTable 行内容
写 Transaction Journal
写 Review
```

---

## 2. 工具列表

第一版包含：

```text
find_assets
read_asset_summary
open_asset_in_editor
get_editor_context
```

---

## 3. 通用规则

这些工具都是只读或 UI 操作。

Agent 应期待：

```text
modified=false
```

Agent 不应期待：

```text
validation
write_ref
transaction_id
journal_recorded
review
safety
```

所有 `data.schema` 使用短命名。

---

# 4. find_assets

## 4.1 职责

`find_assets` 用于：

```text
按路径、类型、名称关键词查找 UE 资产。
```

它不读取资产内部结构。

---

## 4.2 成功返回解释

示例：

```json
{
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
      }
    ],
    "page": {
      "limit": 20,
      "has_more": false
    }
  }
}
```

Agent 可读取：

```text
asset_path
asset_type
asset_class
page
```

Agent 不应期待：

```text
asset_name
package_path
object_path
thumbnail
dependencies
referencers
tags 全量
```

---

## 4.3 `%{path_filter}` 路径压缩规则

当 `target.path_filter` 存在，且结果路径以前缀开头时，`find_assets` 允许返回：

```text
%{path_filter}
```

示例：

```text
target.path_filter = /Game/BlueprintHelperTest
assets[].asset_path = %{path_filter}/Door/BP_BH_PhysicsDoor
```

Agent 后续使用前必须展开为：

```text
/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor
```

---

## 4.4 不允许的压缩写法

不使用：

```text
%{filter}
```

原因：

```text
filter 语义过宽，容易和 asset_type_filter / status_filter 等字段混淆。
```

---

## 4.5 压缩限制

`%{path_filter}` 只限：

```text
find_assets 这类列表型只读结果。
```

以下场景必须使用完整 asset_path：

```text
read_asset_summary
open_asset_in_editor
compile_blueprint_asset
save_asset
read_class_settings
LogicMD / LogicJson
所有写工具 target.asset_path
```

Agent 不得把压缩路径直接传给写工具。

---

## 4.6 空结果

如果：

```json
"assets": []
```

且：

```text
ok=true
status=completed
```

Agent 应理解为空结果，不是失败。

---

# 5. read_asset_summary

## 5.1 职责

`read_asset_summary` 读取一个资产的轻量摘要。

返回：

```text
asset_path
asset_type
asset_class
loaded
```

示例：

```json
{
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

## 5.2 不读取的内容

Agent 不应期待：

```text
class settings
generated_class
implemented_interfaces
class defaults
graph list
function list
dependencies
referencers
```

对应专用工具：

```text
Class Settings -> read_class_settings
Blueprint Logic -> LogicMD / LogicJson
Dependencies -> 后续依赖查询工具
```

---

# 6. open_asset_in_editor

## 6.1 职责

`open_asset_in_editor` 用于：

```text
在 UE 编辑器中打开或聚焦资产。
```

它是 Editor UI 操作，不是资产写操作。

---

## 6.2 成功返回

```json
{
  "data": {
    "schema": "OpenAssetInEditor.v1",
    "open_result": {
      "opened": true,
      "already_open": false
    }
  }
}
```

no_op：

```json
{
  "status": "no_op",
  "data": {
    "open_result": {
      "opened": false,
      "already_open": true
    }
  }
}
```

Agent 不应期待 validation / transaction。

---

# 7. get_editor_context

## 7.1 职责

`get_editor_context` 读取当前 UE 编辑器上下文：

```text
open_assets
selected_assets
```

示例：

```json
{
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

## 7.2 写入目标禁止规则

Agent 不得仅凭：

```text
open_assets
selected_assets
当前编辑器焦点
```

作为写入目标事实来源。

写工具仍必须显式指定：

```text
asset_path
graph / function / event / target
```

且 asset_path 必须是完整路径。

---

# 8. error 处理

常见失败：

```text
asset_registry_unavailable
asset_not_found
invalid_asset_path
editor_open_failed
```

Agent 根据 error.code / retryable 判断是否重试或 stop_and_report。

---

# 9. Agent 禁止行为

Agent 不得：

```text
1. 把 find_assets 当成读取资产内部结构的工具。
2. 期待 find_assets 返回 package_path / object_path。
3. 把 %{path_filter} 压缩路径直接传给写工具。
4. 用 selected_assets / open_assets 作为写目标事实来源。
5. 期待 open_asset_in_editor 返回 validation。
6. 期待任何本簇工具返回 transaction_id。
```

---

# 10. 最终报告规则

正常完成时，Agent 可报告：

```text
1. 找到多少资产。
2. 资产路径、类型、class。
3. 是否已打开资产。
4. 读取到的轻量摘要。
```

不默认报告：

```text
package_path
object_path
transaction_id
Review 状态
```

---

# 11. 验收标准

```text
1. Agent 能使用 find_assets 查找资产。
2. Agent 能展开 %{path_filter}。
3. Agent 不把压缩路径传给写工具。
4. Agent 能处理 assets=[]。
5. Agent 能使用 read_asset_summary 获取轻量摘要。
6. Agent 能使用 open_asset_in_editor 打开资产。
7. Agent 不用 editor context 作为写目标依据。
8. Agent 不期待 validation / transaction。
