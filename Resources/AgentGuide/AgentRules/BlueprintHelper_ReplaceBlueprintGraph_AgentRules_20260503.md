# BlueprintHelper Agent 侧规则：ReplaceBlueprintGraph 使用规范

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：ReplaceBlueprintGraph Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释 ReplaceBlueprintGraph，包括 replace_scope、成功极简返回、replaced_ref、dry_run、失败诊断和 rollback 处理。UE 字段映射见独立文档。

---

## 1. 工具职责

`ReplaceBlueprintGraph` 只负责：

```text
替换一个明确目标的完整实现。
```

Agent 应把 Replace 理解为：

```text
目标已存在。
用户或上下文已明确指定目标。
本次操作会替换该目标的完整实现。
```

---

## 2. 适用目标

Replace 可用于：

```text
BlueprintHelper-owned block
function_body
event_body
custom_event_body
function_definition
event_definition
graph
```

Replace 不用于：

```text
1. 追加新逻辑。使用 AppendBlueprintGraph。
2. 接入已有执行流。使用 MergeBlueprintGraph。
3. 修改单个节点 / Pin / 连接。使用 PatchBlueprintGraph。
4. 清理旧 block。使用 Cleanup 工具。
5. 模糊查找和替换。
```

---

## 3. replace_scope 规则

Agent 必须明确 `replace_scope`。

允许值：

```text
block_implementation
function_body
event_body
custom_event_body
function_definition
event_definition
graph
```

解释：

| replace_scope | Agent 解释 |
|---|---|
| `block_implementation` | 替换 owned block 实现，保留 block_id。 |
| `function_body` | 保留函数入口和签名，只替换内部逻辑。 |
| `event_body` | 保留事件入口身份，只替换内部逻辑。 |
| `custom_event_body` | 保留 Custom Event 入口，只替换后方逻辑。 |
| `function_definition` | 替换函数定义，高风险。 |
| `event_definition` | 替换事件定义，高风险。 |
| `graph` | 替换整个明确图表范围，高风险。 |

---

## 4. 成功返回极简规则

Replace 成功返回不是 diff，也不是 Journal 摘要。

Agent 只读取：

```text
data.replace_result.replaced_ref
data.write_ref.transaction_id
validation
```

示例：

```json
{
  "data": {
    "schema": "ReplaceBlueprintGraph.v1",
    "replace_result": {
      "replaced_ref": {
        "graph_id": "EG_PhysicsDoor",
        "target_ref": "TogglePhysicsDoor0"
      }
    },
    "write_ref": {
      "transaction_id": "tx_20260503_1301",
      "journal_recorded": true
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

Agent 不应期待：

```text
summary
deleted_nodes / created_nodes / modified_nodes 计数
before / after
full_diff
ownership
review
safety
diagnostics
next
```

---

## 5. replaced_ref 规则

`replaced_ref` 表示本次正式命中的替换目标。

字段：

```text
graph_id
target_ref
```

owned block 替换时：

```text
target_ref = block_ref
full_block_id = graph_id + "_" + target_ref
```

函数体 / 事件体替换时：

```text
target_ref = function ref / event ref / custom event ref
```

Agent 不应期待：

```text
replace_result.target
target_kind
entry_type
entry_name
```

目标语义由：

```text
target.replace_scope
```

判断。

---

## 6. owned block 替换规则

替换 BlueprintHelper-owned block 时：

```text
保留原 block_id。
```

原因：

```text
Replace 表示同一逻辑块的新版本，不是创建新逻辑块。
```

变化通过：

```text
新的 transaction_id
Journal / Review 中的 before-after diff
```

表达。

---

## 7. 用户手写目标规则

替换用户手写目标时：

```text
1. 用户必须明确指定目标。
2. 默认不生成 block_ref。
3. 默认不接管 ownership。
4. 仍记录 transaction_id / Journal / Review。
```

只有用户明确要求“以后交给 BlueprintHelper 管理”，才可以考虑接管 ownership；该场景必须 dry_run 并提示 ownership 变化。

---

## 8. dry_run 规则

Replace 替换任何已有目标前都必须 dry_run。

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
      "external_dependents_may_break"
    ],
    "conflicts": [
      {
        "code": "external_dependents_may_break",
        "message": "Function definition replacement may break external callers."
      }
    ],
    "errors": []
  }
}
```

Agent 规则：

```text
1. result=passed + can_execute=true：可进入正式写入。
2. result=blocked + can_execute=false：不得正式写入。
3. ok=true/status=dry_run 只表示 dry_run 工具运行成功。
4. dry_run 工具自身失败才是 ok=false/status=failed/error。
```

dry_run 不返回 replace_plan / would_xxx。完整 plan 进入 Journal / Review / verbose/debug。

---

## 9. external_dependents 规则

如果替换的是：

```text
function_definition
event_definition
graph
```

遇到 external_dependents 默认阻断。

如果替换的是：

```text
function_body
event_body
custom_event_body
```

只有在入口身份、签名、调用 Pin、外部可调用身份保持稳定时，external_dependents 不直接阻断；否则阻断。

---

## 10. 正式失败规则

正式失败不返回：

```text
replace_result
write_ref
ownership
review
safety
diagnostics
next
```

但必须读取：

```text
error.code
error.stage
error.message
error.retryable
error.rollback_result
error.failed_item
error.conflicts
```

最小错误字段：

```text
code
stage
message
retryable
rollback_result
```

---

## 11. rollback_result 规则

| rollback_result | Agent 解释 |
|---|---|
| `not_needed` | preflight / resolve_target 阶段失败，未修改资产。 |
| `rolled_back` | 写入中失败但已回滚，通常 modified=false。 |
| `blocked` | 回滚被阻断，可能残留修改，必须 stop_and_report。 |
| `failed` | 回滚失败，可能残留修改，必须 stop_and_report。 |

如果 rollback blocked / failed：

```text
Agent 不得继续 compile / save / patch / merge / replace。
```

---

## 12. validation 规则

Replace 成功通常返回：

```text
validation.should_compile=true
validation.should_save=true
```

Agent 应根据 validation 继续 compile/save 闭环。

---

## 13. Agent 禁止行为

Agent 不得：

```text
1. 用 Replace 处理追加新逻辑。
2. 用 Replace 接入已有执行流。
3. 在目标不明确时执行 Replace。
4. 期待 Replace 成功返回 summary。
5. 期待 Replace 成功返回 before / after / full_diff。
6. 期待 target_kind。
7. 在 rollback blocked / failed 后继续写入。
8. 把用户手写目标默认接管为 owned。
```

---

## 14. 最终报告规则

正常完成时，Agent 可报告：

```text
1. 哪个资产 / 图表 / 函数被替换。
2. 替换的大致目标。
3. 编译 / 保存结果。
4. 异常或未完成项。
```

不默认报告：

```text
transaction_id
review_status
journal_path
rollback_data
node_guid
pin_guid
summary 计数
```

---

## 15. 验收标准

```text
1. Agent 能识别 Replace 只替换明确目标。
2. Agent 能使用 replace_scope 判断替换语义。
3. Agent 能解析 replace_result.replaced_ref。
4. Agent 不期待 target_kind。
5. Agent 不期待 summary。
6. Agent 理解 owned block 替换保留 block_id。
7. Agent 理解用户手写目标默认不接管 ownership。
8. Agent 能处理 dry_run passed / blocked。
9. Agent 能处理 error.rollback_result。
