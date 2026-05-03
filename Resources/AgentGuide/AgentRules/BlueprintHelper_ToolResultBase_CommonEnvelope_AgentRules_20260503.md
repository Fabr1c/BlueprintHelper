# BlueprintHelper Agent 侧规则：ToolResultBase / Common Envelope / Error Protocol

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：ToolResultBase / Common Envelope Agent 侧规则确认稿  
本文边界：规定 Agent 如何解析所有 BlueprintHelper Agent-facing MCP 工具的公共返回外壳，包括 ok / status、data.schema、target、validation、dry_run、error、conflicts、page、resource_ref，以及 transaction / review / safety 默认不返回规则。UE 字段映射见独立文档。

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

## 1. ToolResultBase 解析规则

所有 Agent-facing 工具都返回同一外壳：

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

Agent 应先看：

```text
ok
status
operation
target
data.schema
```

再看具体 `data.*`。

---

## 2. ok 不等于业务通过

`ok` 只表示工具调用本身是否成功。

以下场景仍然可以是：

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

Agent 不得把业务 blocked 等同于工具失败。

---

## 3. status 解释

第一版 status 枚举：

```text
completed
applied
no_op
dry_run
failed
```

### completed

用于读、查询、诊断、runtime_profile、compile/save/lifecycle/debug/export 成功。

注意：

```text
completed 不代表业务一定成功。
compile completed 但 compile_result.success 可以是 false。
diagnostics completed 但 Markdown 可以有 Blocking。
```

### applied

用于写工具实际应用成功。

### no_op

工具执行成功，但无需修改。

典型：

```text
save_asset：asset_not_dirty
add_component：reuse_if_exists
add_widget_to_tree：reuse_if_exists
add_data_table_row：reuse_if_exists
stop_pie_session：pie_not_running
open_asset_in_editor：already_open
```

### dry_run

高风险操作预检结果。

dry_run blocked 仍然：

```text
ok=true
status=dry_run
```

### failed

仅工具自身失败。

Agent 应读取：

```text
error.code
error.stage
error.message
error.retryable
```

---

## 4. data.schema 短命名

Agent 应期待：

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

Agent 不应期待：

```text
BlueprintHelper.RuntimeProfile.v1
BlueprintHelper.Tools.UMG.AddWidgetToTree.v1
BlueprintHelper.MCP.SaveAsset.v1
```

---

## 5. trace_id 不是 transaction_id

`trace_id` 用于日志排查。

Agent 不得把 `trace_id` 用作：

```text
rollback target
Review target
Journal query target
block ownership identifier
transaction identifier
```

普通写工具成功也不返回 transaction_id。

---

## 6. target 规则

写工具 target 必须完整明确：

```text
asset_path
graph / function / event / custom_event / block / node / pin
```

Agent 不得依赖：

```text
当前编辑器焦点
selected_assets
open_assets
模糊目标
压缩 asset_path
```

写工具 `target.asset_path` 必须是完整路径。

---

## 7. `%{path_filter}` 限制

`%{path_filter}` 只允许出现在：

```text
find_assets.assets[].asset_path
```

用于列表型只读结果压缩。

Agent 后续调用任何精确目标工具前必须展开为完整路径。

禁止直接传入：

```text
写工具 target.asset_path
compile/save target.asset_path
read_class_settings target.asset_path
LogicMD / LogicJson target.asset_path
```

---

## 8. validation 规则

validation 只在写工具成功 / no_op 时按需返回。

只包含：

```json
{
  "should_compile": true,
  "should_save": true
}
```

不包含：

```text
compiled
saved
```

原因：

```text
compile/save 是独立闭环工具。
写工具只告诉 Agent 后续是否需要 compile/save。
```

Agent 不应期待以下工具返回 validation：

```text
compile_blueprint_asset
save_asset
read tools
runtime_profile
diagnostics
Transaction Journal Query
Editor Lifecycle
Debug / Export
Project Context
Asset Discovery
```

---

## 9. dry_run 规则

dry_run passed：

```json
{
  "dry_run": {
    "result": "passed",
    "can_execute": true
  }
}
```

dry_run blocked：

```json
{
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
}
```

Agent 规则：

```text
1. dry_run passed 默认极简。
2. 不期待 would_create / would_delete / would_modify 全量列表。
3. dry_run blocked / failed 只返回必要冲突摘要。
4. blocked 时不得继续正式写入。
```

---

## 10. error 规则

工具自身失败时返回：

```json
{
  "ok": false,
  "status": "failed",
  "error": {
    "code": "asset_not_found",
    "stage": "resolve_asset",
    "message": "The requested asset was not found.",
    "retryable": false
  }
}
```

Agent 应使用：

```text
error.code
error.stage
retryable
```

判断是否重试、降级、stop_and_report。

Agent 不应期待：

```text
stack_trace
本地绝对路径
完整 payload
完整 settings.json
Token / secret
```

---

## 11. conflicts 规则

conflicts 只在失败、blocked、dry_run blocked 或高风险冲突场景返回。

成功场景不返回：

```json
"conflicts": []
```

Agent 不应期待成功结果携带空 conflicts。

conflicts 只包含定位问题所需字段，例如：

```text
property
row_name
widget_name
slot_type
block_id
message
```

不包含完整 before / after。

---

## 12. page 规则

列表型只读工具可返回：

```json
"page": {
  "limit": 20,
  "has_more": false
}
```

空列表不是失败：

```text
ok=true
status=completed
items=[]
```

Agent 不应把空列表当成工具失败。

---

## 13. resource/ref 规则

大 payload 不内联。

Agent 应使用：

```text
resource_ref
bundle_ref
snapshot_ref
```

不应期待：

```text
本地绝对路径
完整大 payload
bundle bytes
```

统一 URI：

```text
resource://blueprinthelper/...
```

---

## 14. transaction / review / safety 默认不返回

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

Agent 规则：

```text
1. 普通任务不依赖 transaction_id。
2. transaction_id 只通过 Journal Query / Debug / Rollback 相关专用路径出现。
3. Review 是 UE 侧审计系统，不是普通 Agent 执行闭环。
4. safety_profile 不在单个工具结果中重复返回。
```

---

## 15. Agent 禁止行为

Agent 不得：

```text
1. 把 ok=true 的业务阻断当工具失败。
2. 把 trace_id 当 transaction_id。
3. 期待成功结果返回 transaction_id。
4. 期待 validation.compiled / validation.saved。
5. 期待 dry_run passed 返回完整 would_* 列表。
6. 在 dry_run blocked 后继续正式写入。
7. 把 conflicts=[] 作为成功必要字段。
8. 把空列表当工具失败。
9. 把 %{path_filter} 传给写工具。
10. 期待 resource_ref 对应本地绝对路径。
```

---

## 16. 最终报告规则

Agent 对用户最终报告默认只输出：

```text
任务是否完成
修改资产摘要
主要新增/修改逻辑
编译/验证结果
保存结果
异常或未完成项
```

不默认输出：

```text
ToolResultBase 原始 JSON
trace_id
transaction_id
review_status
完整 runtime profile
完整 dry_run details
schema/docs 读取细节
```

除非用户要求调试、失败定位、rollback、消歧或 CLI 无 UE 插件窗口。

---

## 17. 验收标准

```text
1. Agent 能解析 ToolResultBase 外壳。
2. Agent 能区分 ok 和业务结果。
3. Agent 能正确解释 status。
4. Agent 知道 data.schema 使用短命名。
5. Agent 知道 trace_id 不是 transaction_id。
6. Agent 知道 validation 只含 should_compile / should_save。
7. Agent 能处理 dry_run passed / blocked。
8. Agent 能处理 error / conflicts。
9. Agent 能处理 page 空列表。
10. Agent 能处理 resource_ref / bundle_ref / snapshot_ref。
11. Agent 不期待普通成功结果返回 transaction/review/safety。
