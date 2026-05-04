# 07 Safety Profile / dry_run 设计文档（已同步确认 Diff）

日期：2026-05-03  
工具簇：Safety Profile / dry_run 规则工具簇  
状态：同步确认 Diff 后的修正版  
同步范围：runtime_profile 为 safety_profile 唯一 Agent 来源、普通工具不返回 safety、移除 parent_class 修改能力、dry_run 字段位置收敛、Setup / runtime profile 权限边界。

---

## 0. 本次同步结论

本文件替换旧版中以下过期口径：

```text
1. Agent 不从单次工具结果读取 safety_profile。
2. safety_profile 只从 runtime_profile.active_profile 读取。
3. 普通成功工具结果不默认返回 safety。
4. dry_run 数据只在 status=dry_run 时位于 data.dry_run。
5. Blueprint Class Settings 第一版不支持修改 Parent Class，因此不再把 parent_class 解析不明确列为第一版高风险写入项。
6. runtime_profile.tool_capabilities 是 unavailable_only 负向稀疏列表，不是完整工具索引。
7. SetupProfile / Safety Profile 不由 Agent 临时覆盖。
```

---

## 1. Profile 档位

Safety Profile 使用五档：

```text
ReadOnly
Conservative
Standard
AutoRepair
Expert
```

Profile 是运行时安全策略，不是 Agent 可随意指定的工具参数。

Agent 必须通过：

```text
runtime_profile.active_profile.safety_profile
```

读取当前生效档位。

---

## 2. ReadOnly

只允许：

```text
读取
搜索
诊断
导出
预览
dry_run
Review 历史查看
```

禁止：

```text
创建资产
修改组件
修改类设置
Graph Write
Cleanup
Rollback
Save
```

ReadOnly 允许 dry_run，但必须：

```text
execution_allowed=false
profile="ReadOnly"
reason="profile_is_read_only"
```

ReadOnly 下 dry_run 不允许转为正式写入，不生成审阅用 transaction_id。

---

## 3. Conservative

默认安全写入档。

规则：

```text
允许写操作。
高风险操作必须 dry_run。
dry_run 无 error / conflict / blocker 后才可执行。
warning / info 不阻断正式写入。
error / conflict / blocker 必须阻断正式写入。
永不自动 save。
不自动 cleanup 旧 owned block。
不自动修改用户节点 / 用户组件 / 用户类设置。
```

Conservative 下普通低风险新建可直接写，但工具仍必须支持 dry_run。

---

## 4. Standard

面向常规 Agent 自动化开发。

规则：

```text
允许更多低风险写操作直接执行。
允许自动 diagnostics。
默认不自动 save。
可在显式参数或 workflow 下自动 save。
对 owned 内容可做更主动的 replace_owned / cleanup。
高风险操作仍建议或要求 dry_run。
```

---

## 5. AutoRepair

面向自动修复 BlueprintHelper-owned 问题。

AutoRepair 不等于无限制自动修改。

默认只自动修复 BlueprintHelper-owned 内容，例如：

```text
owned block
owned component_group
owned event_entry
owned metadata inconsistency
owned Journal / Review 状态不一致
owned block 残留
owned component group 残留
owned entry 后方 orphan owned nodes
```

可读取 Diagnostics 结果后自动调用：

```text
CleanupBlueprintHelperBlock
CleanupBlueprintHelperFeature
cleanup_blueprint_helper_component_group
cleanup_blueprint_helper_override_entry
PatchBlueprintGraph
ReplaceBlueprintGraph
MergeBlueprintGraph
```

修复用户内容不是默认允许行为。用户明确指定目标并授权后，通常进入 Expert / 高风险流程。

---

## 6. Expert

Expert 表示用户显式授权低层、高风险、高自由度操作。

适用：

```text
低层 factory_class / asset_class
通用属性路径高级修复
复杂 Class Defaults
高风险用户资产修改
高级 Debug / Migration
```

Expert 不表示自动修复。Expert 不允许绕过 Journal / Review。

禁止：

```text
no_review=true
no_journal=true
静默修改 UE 资产
```

所有真实 UE 写操作仍必须在 UE 插件内部：

```text
生成 transaction_id
记录 before / after diff
写入 rollback_data
进入 Review UI
```

Expert 不是免审模式。

---

## 7. Agent-facing safety 字段规则

普通工具成功结果不默认返回：

```text
safety
safety_profile
```

Agent 只能从 runtime_profile 读取当前 safety profile：

```json
{
  "active_profile": {
    "safety_profile": "conservative",
    "missing_capability_policy": "stop_and_report"
  }
}
```

单次工具结果中的安全阻断应通过：

```text
status=failed
error.code
error.stage
```

表达。

单次工具 dry_run 结果通过：

```text
status=dry_run
data.dry_run
```

表达。

---

## 8. Conservative 强制 dry_run 高风险表

Conservative 下不是所有写操作都强制 dry_run。

高风险写操作必须 dry_run，低风险新建可直接写。

高风险写操作包括：

```text
修改用户已有节点
修改用户已有组件
修改用户已有 Class Settings / Class Defaults
Merge 到已有执行流
Replace 用户函数体 / 事件体
删除节点 / 删除组件 / 删除事件入口 / 删除资产
Cleanup 操作
replace_owned
修改 Root Component
修改组件 parent / attach 关系
修改 PhysicsConstraint
修改 Collision
修改 SimulatePhysics
修改 Mobility
修改 Tick / Replication / Spawn / Input 类设置
迁移普通函数到接口函数实现
使用低层 factory_class / asset_class
路径冲突
复用已有资产
factory_options 复杂配置
```

已移除第一版高风险项：

```text
parent_class 解析不明确
```

原因：Blueprint Class Settings 第一版不支持修改 Parent Class。如果用户任务要求修改 Parent Class，Agent 应 stop_and_report，而不是进入 dry_run 写入路径。

低风险新建操作包括：

```text
在不存在路径创建白名单资产
在空蓝图 append_only 添加 BlueprintHelper-owned 组件
创建全新 Interface 资产
创建全新 Override / Event 入口且不接入已有执行流（未来能力）
创建全新 owned graph block 到空图表 / 新图表
```

低风险新建可直接写入，但 UE 插件内部仍必须：

```text
记录 transaction_id
写入 Journal / Review
返回 validation.should_compile / validation.should_save
支持 dry_run
```

Agent-facing 普通结果不因此默认返回 transaction / review。

---

## 9. warning 阻断规则

Conservative 下：

```text
info / warning 不阻断正式写入。
error / conflict / blocker 阻断正式写入。
```

如果某个问题实际应该阻断，就不应标记为 warning，而应升级为 error / conflict / blocker。

warning / info 应进入 Journal / Review / validation_result。

---

## 10. 自动保存

普通写工具默认返回：

```text
validation.should_save
validation.saved
```

复杂工作流工具可按需返回：

```text
recommended_next_tool
recommended_validation_workflow
```

规则：

```text
Conservative：永不自动 save。
Standard：默认不自动 save，可在显式参数或 workflow 下 save。
AutoRepair：修复成功后可按参数或 workflow save。
Expert：用户显式授权后可自动 save。
```

自动 save 必须记录到 Journal 或 transaction validation_result / save_result。

Save 是落盘动作，不等同于 Accept。Accept 是审查动作，Save 是资产持久化动作。

---

## 11. Runtime Profile 能力规则

Agent 必须从 runtime_profile 获取当前运行时事实：

```text
bridge_status
config_status
write_permission
risk_command
active_profile
tool_capabilities
```

`tool_capabilities` 使用负向稀疏模式：

```json
{
  "tool_capabilities": {
    "mode": "unavailable_only",
    "unavailable": []
  }
}
```

语义：

```text
1. runtime_profile 只列出 unavailable / disabled / degraded / blocked 的能力。
2. 未列出的能力不等于 runtime_profile 已完整确认 schema。
3. runtime_profile 不是工具索引，也不是 MCP tool schema 文档。
4. Agent 应从 AgentGuide / tools 索引理解工具簇，从 MCP schema 获取具体参数。
```

unavailable item 只包含：

```text
cluster
capability
status
reason
```

不返回：

```text
severity
stop_and_report
message
required_tool
```

stop_and_report 由 Agent 根据当前任务、missing_capability_policy、不可用能力和是否存在安全替代路径判断。

---

## 12. Profile 配置来源

Safety Profile 不由 Agent 在每次 MCP 工具调用中自由传入。

Profile 应通过插件 Setup 流程生成：

```text
/blueprinthelper-setup
Setup Wizard
settings.json
runtime profile
```

SetupProfile 应保存在配置文件中，由 MCP / UE runtime 读取后形成当前 active_profile。

Agent 不允许自行提升 Profile。

---

## 13. SetupProfile 修改生效

两种修改场景：

### 插件命令 / Setup Wizard 修改

```text
受控修改路径。
用户在插件 UI 或 Setup 流程中完成确认。
修改后写入 Settings。
立刻生效或通过明确 reload 生效。
记录 Setup log / Settings change log。
```

### 直接修改 Settings 文件

```text
非受控外部修改路径。
默认需要重启 UE Editor / MCP Server 后生效。
运行中检测到 Settings 文件变化，可提示用户重启或执行 reload，但不静默切换高权限 Profile。
```

UE 插件 UI 应显示当前运行时生效 Profile，而不是仅显示文件中的 Profile。

---

## 14. 工具调用临时覆盖

工具调用不允许临时覆盖 SetupProfile。

不支持：

```text
temporary_profile
safety_profile override
per_call_profile
one-shot Expert
```

所有安全策略以当前运行时 SetupProfile 为准。

工具仍可支持业务级 dry_run 参数，但其行为受 SetupProfile 约束。

如果 SetupProfile 要求某类操作必须 dry_run，则工具调用不能跳过。

如果 SetupProfile 不允许某类写操作，则工具调用不能通过参数开启。

---

## 15. 工具参数与 SetupProfile 冲突

SetupProfile 是安全策略权威来源。

工具参数与 SetupProfile 冲突时，直接返回 error。

推荐错误码：

```text
ProfilePolicyViolation
```

不自动降级执行，不静默忽略冲突参数。

工具必须返回：

```text
violated_policy
requested_behavior
allowed_behavior
current_profile
recommended_action
```

示例：

```text
ReadOnly 下调用写工具 -> ProfilePolicyViolation
Conservative 下高风险写入但未 dry_run -> ProfilePolicyViolation
Profile 禁止 auto_save 但工具请求 save_after_write -> ProfilePolicyViolation
Profile 禁止低层 factory_class 但工具传入 factory_class -> ProfilePolicyViolation
```

Agent 收到错误后应停止当前写入，并报告用户或建议通过 Setup Wizard 修改 Profile。

---

## 16. Parent Class 修改边界

Blueprint Class Settings 第一版不支持：

```text
set_parent_class
blueprint_reparent
```

因此：

```text
1. parent_class 只作为 read_class_settings 的只读字段。
2. Agent 不应计划通过 Class Settings 修改 Parent Class。
3. 如果任务要求改变 Parent Class，Agent 应 stop_and_report。
4. 不存在 parent_class dry_run / confirmed_after_dry_run / parent_class_result 第一版字段。
```

---

## 17. 验收标准

```text
1. Agent 只从 runtime_profile.active_profile 读取 safety_profile。
2. 普通工具成功结果不默认返回 safety。
3. dry_run 信息只在 status=dry_run 时位于 data.dry_run。
4. Conservative 下 info/warning 不阻断，error/conflict/blocker 阻断。
5. 低风险新建可直接写，但必须支持 dry_run。
6. UE 插件内部仍记录 Journal / Review，但 Agent-facing 普通结果不默认返回 transaction/review。
7. runtime_profile.tool_capabilities 使用 unavailable_only。
8. Agent 不把 runtime_profile 当工具索引或 schema 文档。
9. 工具调用不允许临时覆盖 SetupProfile。
10. 第一版不支持修改 Parent Class；相关任务 stop_and_report。
```
---

# 2026-05-04 TaskSpec / TaskPlan 安全策略同步

## 同步结论

Safety Profile 仍是运行时安全策略权威来源，不由 Agent 在单次工具调用中覆盖。

新增任务级执行口径：

```text
TaskSpec 由 Agent 提交。
Task Compiler 校验 TaskSpec 与 Safety Profile 是否冲突。
UE Task Runtime 执行 TaskPlan 前再次检查当前 Safety Profile / write_permission / context stale。
```

## 新增 Agent-facing 工具

普通写入流程固定为：

```text
read_task_context
preview_task
execute_task
```

ReadOnly：

```text
允许 read_task_context / preview_task。
禁止 execute_task 真实写入。
```

Conservative：

```text
允许 execute_task，但 TaskPlan 内高风险 step 必须 dry_run / preflight 通过。
```

## TaskSpec 与 Profile 冲突

如果 TaskSpec 请求扩大权限，例如：

```json
{
  "scope_policy": {
    "allow_edit_input_mapping": true
  }
}
```

但当前 profile 或 runtime capability 不允许，应返回：

```text
ProfilePolicyViolation / capability_unavailable
agent_action = remove_scope_or_stop_and_report
```

不允许 Python / MCP / UE Task Runtime 静默降级执行。

## Context stale

TaskSpec 可以引用 context_id。execute 前 UE Task Runtime 必须重新检查：

```text
目标资产是否仍存在
图表 empty/non-empty 状态是否变化
资源候选是否仍唯一
组件是否已被用户改动
write_permission 是否变化
safety_profile 是否变化
```

如果过期：

```text
status=context_stale
agent_action=refresh_context_and_retry
```
