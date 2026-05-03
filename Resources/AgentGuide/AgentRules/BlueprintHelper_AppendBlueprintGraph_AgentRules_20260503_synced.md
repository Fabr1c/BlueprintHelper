# BlueprintHelper Agent 侧规则：AppendBlueprintGraph 使用规范

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：AppendBlueprintGraph Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释 Graph Write / AppendBlueprintGraph。UE 字段映射见独立 UE 侧文档。

---

## 1. 工具职责

`AppendBlueprintGraph` 只负责：

```text
追加新的独立 BlueprintHelper-owned 逻辑块。
```

Agent 应把它理解为：

```text
新建独立逻辑，不接入已有执行流。
```

---

## 2. 允许行为

Agent 可使用 AppendBlueprintGraph：

```text
1. 创建新的 EG_{FeatureName} 事件图表。
2. 向已有空图表追加独立逻辑块。
3. 创建唯一命名的 Custom Event。
4. 创建普通节点。
5. 创建新节点之间的连线。
6. 在同一次 Append 内让多个新 Custom Event 互相调用。
7. 调用已经存在且签名匹配的函数或 Custom Event。
```

---

## 3. 禁止行为

Agent 不得用 AppendBlueprintGraph：

```text
1. 自动连接已有节点。
2. 自动接入已有执行流。
3. 修改 BeginPlay / Tick / InputAction / Overlap 等已有事件执行链。
4. 覆盖已有节点。
5. 删除已有节点。
6. 清理旧 block。
7. 修改用户节点。
8. 创建全局事件节点。
9. 创建函数图。
10. 在 Conservative 下追加到函数图。
```

需要接入已有执行链时，必须使用：

```text
MergeBlueprintGraph
```

需要替换已有实现时，必须使用：

```text
ReplaceBlueprintGraph
```

需要精确修改节点 / Pin / 默认值 / 连线时，必须使用：

```text
PatchBlueprintGraph
```

---

## 4. 图表规则

默认推荐新建：

```text
EG_{FeatureName}
```

图表冲突规则：

```text
目标图表不存在：允许创建。
目标图表存在且为空：允许写入。
目标图表存在且非空：Append 失败。
不自动改名。
```

Agent 不得在图表非空时改用 Append 强行追加。  
这类场景需要重新计划：Merge、Replace、Patch 或向用户报告。

---

## 5. Custom Event 规则

Append 创建 Custom Event 时：

```text
事件名必须唯一。
重名直接失败。
不自动改名。
```

推荐命名：

```text
TogglePhysicsDoor
OpenPhysicsDoor
ClosePhysicsDoor
InitializePhysicsDoor
```

不推荐：

```text
DoThing
NewEvent
Event1
```

Append 不创建全局事件节点：

```text
BeginPlay
Tick
ConstructionScript
InputAction
Overlap
Hit
```

---

## 6. 同一 Append 内部调用规则

同一次 Append 内允许多个新建 Custom Event 互相调用。

示例：

```text
TogglePhysicsDoor
→ Branch bDoorOpen
  → false: OpenPhysicsDoor
  → true: ClosePhysicsDoor
```

这不等于接入已有执行流。  
如果需要从 BeginPlay、InputAction、Overlap 等入口触发这些 Custom Event，应另用 MergeBlueprintGraph。

---

## 7. 调用已有函数 / 事件规则

Append 创建的新逻辑可以调用已有函数或已有 Custom Event，但工具必须验证：

```text
目标存在。
签名匹配。
参数 / Pin 可连接。
```

目标不存在时：

```text
Append 失败。
Agent 不得假设工具会自动创建缺失函数或事件。
```

---

## 8. 成功返回解释规则

成功返回采用极简 Agent-facing 结构，只保留后续操作必需的 handle：

```text
data.append_result.graph
data.append_result.block_refs
data.write_ref
validation
```

示例：

```json
{
  "data": {
    "append_result": {
      "graph": {
        "graph_id": "EG_PhysicsDoor",
        "graph_name": "EG_PhysicsDoor"
      },
      "block_refs": [
        "TogglePhysicsDoor0",
        "OpenPhysicsDoor0"
      ]
    },
    "write_ref": {
      "transaction_id": "tx_20260503_1001",
      "journal_recorded": true
    }
  }
}
```

Agent 应理解：

```text
1. Append 成功写入的 block_refs 默认都是 BlueprintHelper-owned blocks。
2. 返回中不提供 ownership 字段。
3. 如果 ownership 写入失败，工具不会成功返回。
4. created_nodes / created_links / created_blocks 等计数不默认返回；这些进入 Transaction Journal / Review。
```

---

## 9. block_refs 与 block_id 规则

成功返回只给：

```text
graph_id
block_refs[]
```

`block_refs` 是 string 数组，不是 block 对象快照。

完整 `block_id` 由 Agent 反推：

```text
full_block_id = graph_id + "_" + block_ref
```

示例：

```text
graph_id = EG_PhysicsDoor
block_ref = TogglePhysicsDoor0
full_block_id = EG_PhysicsDoor_TogglePhysicsDoor0
```

Agent 不应期待以下字段默认返回：

```text
blocks[].block_id
blocks[].entry_type
blocks[].entry_name
```

`entry_name` 已包含在 `block_ref` 中；`entry_type` 如需判断，应后续读取 LogicMD / LogicJson。

---

## 10. write_ref 规则

`write_ref` 是 Graph Write 专属最小后续引用摘要，不是通用顶层 `transaction`。

第一版只包含：

```text
transaction_id
journal_recorded
```

Agent 不应期待：

```text
review_status
journal_path
rollback_data
rollback_available
```

这些属于 UE 内部 Journal / Review / rollback 系统或专用工具。

---

## 11. 成功结果不返回 summary

Append 成功结果不默认返回：

```text
summary
created_blocks
created_nodes
created_links
created_variables
called_existing_functions
called_existing_events
created_nodes 明细
created_links 明细
node_guid 列表
pin_guid 列表
```

这些信息进入：

```text
Transaction Journal
Review diff
verbose/debug 输出
```

后续精确修改应先读取 LogicJson，而不是依赖 Append 写工具回显。

---

## 12. dry_run 成功规则

dry_run 成功默认只返回：

```json
{
  "dry_run": {
    "result": "passed",
    "can_execute": true
  }
}
```

Agent 应理解：

```text
dry_run 通过，可进入正式写入阶段，前提是当前 workflow / Safety Profile 允许。
```

dry_run 成功不返回：

```text
plan
would_xxx
references
graph action
block_ref
block_id
write_ref
transaction_id
ownership
review
safety
diagnostics
next
```

---

## 13. dry_run blocked 规则

dry_run blocked 返回：

```json
{
  "dry_run": {
    "result": "blocked",
    "can_execute": false,
    "blocked_by": [
      "target_graph_not_empty"
    ],
    "conflicts": [
      {
        "code": "target_graph_not_empty",
        "message": "AppendBlueprintGraph cannot append to a non-empty existing graph."
      }
    ],
    "errors": []
  }
}
```

Agent 应理解：

```text
1. ok=true/status=dry_run 只表示 dry_run 工具运行成功。
2. can_execute=false 表示不能正式执行。
3. blocked_by 是 conflicts/errors 的 code 摘要。
4. Agent 不得继续执行正式写入。
```

---

## 14. dry_run 工具自身失败

如果返回：

```text
ok=false
status=failed
error
```

表示 dry_run 工具自身失败，例如 schema 解析失败、Bridge 异常、payload 非法。

Agent 应 stop_and_report 或修正输入后重试。

---

## 15. 正式失败解释规则

正式失败只返回：

```text
error
```

不返回：

```text
data.append_result
data.write_ref
ownership
review
safety
diagnostics
next
created_nodes
created_links
rollback_data
```

Agent 应根据：

```text
error.code
error.stage
error.retryable
error.rollback_result
```

判断是否可修正计划、重试或 stop_and_report。

---

## 16. rollback_result 解释规则

| rollback_result | Agent 解释 |
|---|---|
| `not_needed` | preflight 阶段失败，没有写入，无需回滚。 |
| `rolled_back` | 写入中失败，但工具已回滚，最终 modified=false。 |
| `blocked` | 写入失败且回滚被阻断，可能残留修改，必须 stop_and_report。 |
| `failed` | 回滚失败，可能残留修改，必须 stop_and_report。 |

如果：

```text
rollback_result=blocked 或 failed
```

Agent 不得继续：

```text
compile
save
patch
merge
replace
cleanup
```

除非用户明确要求进入恢复流程，并且 Agent 先读取当前资产状态。

---

## 17. ownership / Journal 失败规则

### ownership_write_failed

Agent 应理解：

```text
Append 成功 = blocks 默认 owned。
ownership 写入失败 = 工具整体失败。
```

不要期待成功结果中有 ownership 字段。

### journal_write_failed

Agent 应理解：

```text
Graph Write 的 Journal 写入失败时，工具不能报告成功。
```

不要期待：

```text
write_ref.journal_recorded=false
```

如果 Journal 写入失败，应是：

```text
ok=false
status=failed
error.code=journal_write_failed
```

---

## 18. validation 使用规则

Append 正式成功通常返回：

```json
{
  "validation": {
    "should_compile": true,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

Agent 应根据 validation 继续 compile/save 闭环。

规则：

```text
1. should_compile=true：后续应执行编译，除非当前 workflow 明确延后。
2. should_save=true：后续应保存，除非当前 Profile / workflow 禁止。
3. compiled/saved 表示本工具调用中是否已执行。
4. Conservative 默认不自动 save。
```

---

## 19. runtime_profile / safety 规则

Agent 不从 Append 结果读取：

```text
safety
safety_profile
```

Agent 必须从：

```text
runtime_profile.active_profile.safety_profile
runtime_profile.write_permission
runtime_profile.tool_capabilities
```

判断写入权限和安全策略。

dry_run 信息只在：

```text
status=dry_run
data.dry_run
```

出现。

---

## 20. Agent 禁止行为

Agent 不得：

```text
1. 用 Append 接入已有执行流。
2. 用 Append 修改非空已有图表。
3. 用 Append 创建 BeginPlay / Tick / InputAction / Overlap 等全局事件。
4. 期待 dry_run 返回完整计划。
5. 期待 dry_run 返回 block_ref / block_id。
6. 期待成功返回 ownership。
7. 期待成功返回 summary / created_nodes / created_links。
8. 在 rollback blocked / failed 后继续后续写入。
9. 在 Journal 写入失败时把结果当作成功。
10. 在最终报告中默认输出 transaction_id / review_status / Journal 路径。
```

---

## 21. 最终报告规则

正常完成时，Agent 可报告：

```text
1. 是否创建或使用目标图表。
2. 如需要后续引用，可使用返回的 block_refs。
3. 是否需要 compile/save。
```

不默认报告：

```text
transaction_id
review_status
journal_path
rollback_data
node_guid
pin_guid
created_nodes 明细
created_links 明细
```

---

## 22. Agent 侧验收标准

```text
1. Agent 能区分 Append / Merge / Replace / Patch。
2. Agent 不用 Append 接入已有执行流。
3. Agent 能根据 graph_id + block_refs[] 反推 full block_id。
4. Agent 理解 Append 成功 blocks 默认 owned。
5. Agent 不期待 ownership 字段。
6. Agent 不期待 dry_run 完整计划。
7. Agent 能处理 dry_run result=passed / blocked。
8. Agent 能处理正式失败 error.rollback_result。
9. Agent 能根据 validation 执行后续 compile/save。
10. Agent 不默认输出 transaction_id / review_status。
```
