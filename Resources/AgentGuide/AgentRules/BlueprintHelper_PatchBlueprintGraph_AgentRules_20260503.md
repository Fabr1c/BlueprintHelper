# BlueprintHelper Agent 侧规则：PatchBlueprintGraph 使用规范

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：PatchBlueprintGraph Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释 PatchBlueprintGraph，包括精确定位、patched_ref、patch_type、expected_old_state、成功极简返回、dry_run、失败诊断和 rollback 处理。UE 字段映射见独立文档。

---

## 1. 工具职责

`PatchBlueprintGraph` 只负责：

```text
精确修改一个明确目标点。
```

Agent 应把 Patch 理解为：

```text
已经定位到具体节点 / Pin / Link / 属性。
只修改一个明确点或一组明确点。
不重建完整实现。
不接入执行流。
```

---

## 2. 可修改目标

Patch 可修改：

```text
1. Pin 默认值。
2. 节点属性。
3. 节点注释。
4. 节点位置。
5. Pin 连接。
6. 单条 Link。
7. 调用目标。
8. 局部变量引用。
```

Patch 不用于：

```text
1. 新增完整独立逻辑。使用 AppendBlueprintGraph。
2. 替换完整实现。使用 ReplaceBlueprintGraph。
3. 接入已有执行流。使用 MergeBlueprintGraph。
4. 模糊查找节点。
5. 根据自然语言猜测目标。
```

---

## 3. 精确定位规则

Patch 前 Agent 必须有明确定位。

优先使用：

```text
LogicJson node_ref / pin_ref / link_ref
或可从 LogicJson group 反推的完整 node_path / pin_path / link_path
```

规则：

```text
1. target_graph / blueprint / multi_target 使用 logic.groups[]。
2. node_ref / link_ref 是 group 内局部引用。
3. pin_ref 也只在对应 node/group 上下文内有效。
4. 需要完整路径时，从 group.entry.node_path 反推。
5. 不得跨 group 使用局部引用。
6. 仅靠显示名 / Pin 名不允许执行 Patch。
```

---

## 4. patch_scope / patch_type

Agent 必须区分：

```text
target.patch_scope
data.patch_result.patch.patch_type
```

示例：

```text
patch_scope=pin_default
patch_type=set_pin_default
```

`patch_scope` 表达工具执行范围。  
`patch_type` 表达本次实际修改类型。

目标类型由 `patch_scope + patch_type` 推导，不再依赖：

```text
target_type
target_kind
```

---

## 5. 成功返回极简规则

Patch 成功返回不是 diff，也不是 Journal 摘要。

Agent 只读取：

```text
data.patch_result.patched_ref
data.patch_result.patch
data.write_ref.transaction_id
validation
```

示例：

```json
{
  "data": {
    "schema": "PatchBlueprintGraph.v1",
    "patch_result": {
      "patched_ref": {
        "graph_id": "EG_PhysicsDoor",
        "node_ref": "Branch0",
        "pin_ref": "Condition"
      },
      "patch": {
        "patch_type": "set_pin_default",
        "expected_old_state_provided": true,
        "changed": true
      }
    },
    "write_ref": {
      "transaction_id": "tx_20260503_1401",
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
modified_nodes / modified_pins 计数
created_links / deleted_links 计数
before / after
old_value / new_value
full_diff
ownership
review
safety
diagnostics
next
```

---

## 6. patched_ref 规则

`patched_ref` 表示本次正式命中的修改目标引用。

默认字段：

```text
graph_id
node_ref
pin_ref
link_ref
```

完整 path 只在必要时返回：

```text
node_path
pin_path
link_path
```

Agent 规则：

```text
1. node_ref / pin_ref / link_ref 是执行确认，不是新的全局定位事实。
2. 后续若还要精确修改，应重新读取 LogicJson 或确认引用仍然有效。
3. 如果工具返回完整 path，Agent 可优先使用完整 path。
```

---

## 7. patch 字段规则

`patch` 只返回：

```text
patch_type
expected_old_state_provided
changed
```

字段解释：

| 字段 | Agent 解释 |
|---|---|
| `patch_type` | 本次修改类型。 |
| `expected_old_state_provided` | Agent 是否提供 expected_old_state / expected_old_value。 |
| `changed` | 本次是否产生实际修改。 |

如果：

```text
changed=false
modified=false
```

Agent 应按 no-op 处理，并根据 validation 判断是否需要 compile/save。

---

## 8. expected_old_state 规则

Patch 不强制所有场景都携带 expected_old_state / expected_old_value。

必须或建议携带：

```text
用户手写节点
高风险修改
连接关系修改
影响执行流的 Pin
目标存在多义性
```

可省略：

```text
BlueprintHelper-owned 节点
目标定位明确
低风险默认值修改
old/new value 是长文本，重复传输 Token 成本高
```

即使省略，UE 侧仍会在 Journal / Review 中记录 before / after。

Agent 不应期待成功返回 before / after。

---

## 9. dry_run 规则

Patch dry_run 沿用极简口径。

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
      "expected_old_state_mismatch"
    ],
    "conflicts": [
      {
        "code": "expected_old_state_mismatch",
        "message": "Target state no longer matches expected_old_state."
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

dry_run 不返回 patch_plan / would_xxx。

---

## 10. 正式失败规则

正式失败不返回：

```text
patch_result
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

Patch 成功通常返回：

```text
validation.should_compile=true
validation.should_save=true
```

Agent 应根据 validation 继续 compile/save 闭环。

---

## 13. Agent 禁止行为

Agent 不得：

```text
1. 用 Patch 替换完整实现。
2. 用 Patch 接入已有执行流。
3. 仅凭显示名 / Pin 名执行 Patch。
4. 跨 LogicJson group 使用 node_ref / link_ref。
5. 期待 Patch 成功返回 summary。
6. 期待 Patch 成功返回 before / after / old_value / new_value。
7. 期待 target_kind。
8. 在 rollback blocked / failed 后继续写入。
```

---

## 14. 最终报告规则

正常完成时，Agent 可报告：

```text
1. 哪个资产 / 图表被修改。
2. 修改的大致目标。
3. 修改类型。
4. 编译 / 保存结果。
5. 异常或未完成项。
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
before / after
```

---

## 15. 验收标准

```text
1. Agent 能识别 Patch 只修改明确目标点。
2. Agent 能使用 LogicJson 做 Patch 前定位。
3. Agent 不仅凭显示名 / Pin 名执行 Patch。
4. Agent 能解析 patch_result.patched_ref。
5. Agent 不期待 target_kind。
6. Agent 不期待 summary。
7. Agent 能解析 patch_type / expected_old_state_provided / changed。
8. Agent 理解 expected_old_state 不是所有场景都强制。
9. Agent 能处理 dry_run passed / blocked。
10. Agent 能处理 error.rollback_result。
