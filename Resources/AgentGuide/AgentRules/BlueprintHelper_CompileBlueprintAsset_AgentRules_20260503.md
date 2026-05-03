# BlueprintHelper Agent 侧规则：compile_blueprint_asset 使用规范

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：compile_blueprint_asset Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释 compile_blueprint_asset，包括工具执行成功、蓝图编译失败、warning_count、Markdown 错误信息和无 validation / transaction 返回规则。UE 字段映射见独立文档。

---

## 1. 工具职责

`compile_blueprint_asset` 负责：

```text
编译一个明确 Blueprint 资产，并返回编译结果。
```

它不负责：

```text
修复错误
保存资产
修改蓝图
生成 transaction_id
进入 Review
```

---

## 2. 成功执行与编译成功的区别

Agent 必须区分两层状态：

```text
ok/status：工具是否成功执行。
compile_result.success：蓝图是否编译通过。
```

如果：

```text
ok=true
status=completed
compile_result.success=false
```

含义是：

```text
工具成功调用了 UE 编译流程，但蓝图本身编译失败。
```

这不是工具自身失败。

---

## 3. 编译通过

示例：

```json
{
  "ok": true,
  "status": "completed",
  "modified": false,
  "data": {
    "schema": "CompileBlueprintAsset.v1",
    "compile_result": {
      "success": true,
      "status": "succeeded",
      "warning_count": 0
    }
  }
}
```

Agent 应理解：

```text
蓝图编译通过。
```

---

## 4. warning_count 规则

`warning_count` 保留。

规则：

```text
warning_count 不影响 compile_result.success。
```

即：

```text
success=true
warning_count>0
```

仍表示蓝图编译通过。

Agent 可在最终报告中简短说明存在 warning，但不应把 warning 当成编译失败。

---

## 5. 编译失败

示例：

```json
{
  "ok": true,
  "status": "completed",
  "modified": false,
  "data": {
    "schema": "CompileBlueprintAsset.v1",
    "compile_result": {
      "success": false,
      "status": "failed",
      "warning_count": 1,
      "format": "markdown",
      "markdown": "## Compile Errors\n\n- `EG_PhysicsDoor_TogglePhysicsDoor0`: Cannot connect Object Reference pin to Boolean pin."
    }
  }
}
```

Agent 应理解：

```text
编译流程执行成功，但蓝图有编译错误。
```

---

## 6. 编译错误 Markdown

编译失败错误信息通过：

```text
data.compile_result.markdown
```

返回。

Markdown 只包含：

```text
block_id + message
```

Agent 不应期待：

```text
messages[]
node_ref
pin_ref
graph
severity
error_count
```

无法映射到 block_id 的错误使用：

```md
- `unmapped`: <message>
```

---

## 7. 工具自身失败

工具自身失败示例：

```json
{
  "ok": false,
  "status": "failed",
  "modified": false,
  "error": {
    "code": "asset_not_found",
    "stage": "resolve_asset",
    "message": "The requested Blueprint asset was not found.",
    "retryable": false
  }
}
```

Agent 应理解：

```text
未能执行编译流程。
```

此时应根据 error 决策，不读取 compile_result。

---

## 8. 不返回 validation

compile 工具不返回：

```text
validation
```

原因：

```text
compile_blueprint_asset 本身就是验证闭环动作。
```

Agent 不应期待：

```text
validation.compiled
validation.compile_success
validation.should_save
```

是否继续保存，应由 Agent 根据：

```text
compile_result.success
用户工作流
Safety Profile
前序写工具的 should_save
```

自行判断。

---

## 9. 不返回事务信息

compile 工具不返回：

```text
write_ref
transaction_id
journal_recorded
review
safety
rollback_data
```

Agent 不应把 compile 当成写事务。

---

## 10. modified 规则

compile 默认：

```text
modified=false
```

即使 UE 内部可能刷新编译状态，Agent-facing 结果不把它视为 BlueprintHelper 写事务修改。

---

## 11. Agent 禁止行为

Agent 不得：

```text
1. 把 compile_result.success=false 误判为工具调用失败。
2. 把 warning_count>0 判定为编译失败。
3. 期待 error_count。
4. 期待 messages[] 结构化列表。
5. 期待 node_ref / pin_ref 级错误定位。
6. 期待 validation。
7. 期待 transaction_id / write_ref。
```

---

## 12. 最终报告规则

正常完成时，Agent 可报告：

```text
1. 编译是否通过。
2. warning 数量。
3. 若失败，按 Markdown 中 block_id + message 摘要说明。
```

不默认报告：

```text
node_ref
pin_ref
transaction_id
review_status
```

---

## 13. 验收标准

```text
1. Agent 能区分工具执行成功和蓝图编译成功。
2. Agent 能解析 compile_result.success。
3. Agent 知道 warning 不影响 success。
4. Agent 不期待 error_count。
5. Agent 能读取 compile_result.markdown。
6. Agent 不期待 validation。
7. Agent 不期待 write_ref / transaction_id。
