# BlueprintHelper ReplaceBlueprintGraph TaskPlan 能力协议

日期：2026-05-04  
范围：Graph Write / ReplaceBlueprintGraph / TaskPlan step args  
状态：架构变更后同步稿  
依赖：公共协议、BlueprintLogicSpec.v1

架构同步说明：`ReplaceBlueprintGraph` 不是普通 Agent 默认直调入口。Agent 默认提交 TaskSpec；Python / MCP Task Compiler 在确认目标明确、替换范围安全后生成 TaskPlan step，UE Task Runtime 再调用本 capability。本文字段同时保留给 debug / expert / 测试入口。

---

## 1. 工具职责

`ReplaceBlueprintGraph` 第一版负责替换一个明确目标的完整实现。

支持目标：

```text
1. BlueprintHelper-owned block implementation
2. Function body
3. Custom Event body
4. Custom Event definition（同名同签名重建）
5. Event body
6. Event definition（同名同签名重建）
7. Graph
```

不支持：

```text
1. Function definition。
2. Function 签名修改。
3. Custom Event / Event 签名修改。
4. 创建新 Function / Custom Event / Event。
5. 创建成员变量 / Local Variable。
6. 修改成员变量默认值。
7. 保存资产。
8. 编译资产。
9. 布局控制。
10. 注释保留策略字段。
```

Function definition / 函数头参数变化归属 Function / Event Signature Management。

---

## 2. TaskPlan step 输入结构

```ts
type ReplaceBlueprintGraphRequest = {
  asset_path: string
  dry_run_mode: "none" | "quick" | "full"

  target: ReplaceTarget
  replace_scope: ReplaceScope

  body: BlueprintLogicSpec

  verbose?: boolean
}
```

---

## 3. target

```ts
type ReplaceTarget =
  | { target_type: "block"; block_id: string }
  | { target_type: "function"; target_name: string }
  | { target_type: "custom_event"; target_name: string }
  | { target_type: "event"; target_name: string }
  | { target_type: "graph"; target_graph: string }
```

### 3.1 block

```ts
target: {
  target_type: "block",
  block_id: "EG_PhysicsDoor_TogglePhysicsDoor0"
}
```

规则：

```text
1. 只接受完整 block_id。
2. 不接受 graph_id + block_ref。
3. Append 返回的 graph_id + block_ref 只是压缩返回。
4. Task Compiler / Task Runtime 后续引用 block 时自行反推完整 block_id。
```

反推规则：

```text
full_block_id = graph_id + "_" + block_ref
```

### 3.2 function

```ts
target: {
  target_type: "function",
  target_name: "OpenDoor"
}
```

规则：

```text
1. asset_path + target_name 定位函数。
2. 函数名在单个 Blueprint 内唯一。
3. 不需要 target_graph。
```

### 3.3 custom_event

```ts
target: {
  target_type: "custom_event",
  target_name: "OpenDoor"
}
```

规则：

```text
1. Custom Event 名称在单个 Blueprint 内唯一。
2. Custom Event 与 Function / Event 之间也不能同名。
3. 不需要 target_graph。
```

### 3.4 event

```ts
target: {
  target_type: "event",
  target_name: "ReceiveBeginPlay"
}
```

规则：

```text
1. Event 名称在单个 Blueprint 内唯一。
2. Event 与 Function / Custom Event 之间也不能同名。
3. 不需要 event_kind / input_action_asset_path / override_source。
```

### 3.5 graph

```ts
target: {
  target_type: "graph",
  target_graph: "EG_PhysicsDoor"
}
```

规则：

```text
1. asset_path + target_graph 定位图表。
2. target_graph 必须存在。
3. 不自动创建缺失图表。
```

---

## 4. replace_scope

```ts
type ReplaceScope =
  | "block_implementation"
  | "function_body"
  | "custom_event_body"
  | "custom_event_definition"
  | "event_body"
  | "event_definition"
  | "graph"
```

移除并禁止：

```text
function_definition
```

规则：

```text
1. replace_scope 必填。
2. 不由 target_type 自动推断。
3. 工具必须校验 target_type 与 replace_scope 是否匹配。
4. 不匹配返回 replace_scope_mismatch。
```

合法匹配：

```text
target_type=block
→ replace_scope=block_implementation

target_type=function
→ replace_scope=function_body

target_type=custom_event
→ replace_scope=custom_event_body
→ replace_scope=custom_event_definition

target_type=event
→ replace_scope=event_body
→ replace_scope=event_definition

target_type=graph
→ replace_scope=graph
```

---

## 5. dry_run_mode 与风险规则

```ts
dry_run_mode: "none" | "quick" | "full"
```

### 5.1 始终 full 的 scope

以下 scope 始终要求 full dry_run / full preflight：

```text
custom_event_definition
event_definition
graph
```

规则：

```text
1. dry_run_mode="quick" 不允许。
2. dry_run_mode="full" 用于非写入预演。
3. dry_run_mode="none" 用于正式写入请求。
4. dry_run_mode="none" 下工具内部仍必须执行 full preflight。
5. 必须检查 external_dependents。
6. 存在外部调用方则 blocked。
7. 依赖检查不可用则 blocked。
```

### 5.2 按 ownership 决定的 scope

以下 scope 根据目标内容 ownership 决定是否 full：

```text
function_body
custom_event_body
event_body
```

规则：

```text
owned-only：
- dry_run_mode="quick" 可用。
- dry_run_mode="full" 可用。
- dry_run_mode="none" 正式写入前普通 preflight。

user-owned / mixed / unknown：
- dry_run_mode="quick" 不允许。
- dry_run_mode="full" 可用于预演。
- dry_run_mode="none" 正式写入前必须 full preflight。
```

### 5.3 block_implementation

```text
block_implementation 目标必须是 BlueprintHelper-owned block。
```

规则：

```text
1. dry_run_mode="quick" 可用。
2. dry_run_mode="full" 可用。
3. dry_run_mode="none" 正式写入前普通 preflight。
4. 如果 block_id 不存在或 ownership 不匹配，失败。
```

错误码：

```text
full_dry_run_required
external_dependents_blocked
dependency_check_unavailable
replace_scope_mismatch
unsupported_replace_scope
function_definition_replace_unsupported
```

---

## 6. body

```ts
body: {
  schema: "BlueprintLogicSpec.v1"
  statements: Statement[]
}
```

规则：

```text
1. body 必填。
2. body 使用 BlueprintLogicSpec.v1。
3. body 只描述目标入口后方实现。
4. body 不声明 entry / function / custom_event / event。
5. body 不创建 helper entries。
6. body 不创建变量。
7. body 不设置 defaults。
8. body 不管理 Local Variables。
```

### 6.1 空实现

允许：

```ts
body: {
  schema: "BlueprintLogicSpec.v1",
  statements: []
}
```

语义：

```text
替换为“空实现”。
```

对 body scope：

```text
function_body
custom_event_body
event_body
block_implementation
```

表示：

```text
保留入口节点 / 签名 / 外部身份。
删除 body 内部旧节点和旧连接。
```

如果目标 body 内存在 user-owned / mixed / unknown nodes：

```text
1. dry_run_mode="quick" 不允许。
2. dry_run_mode="full" 可预演。
3. dry_run_mode="none" 正式写入前必须 full preflight。
4. Review / Journal 必须记录删除 diff。
5. 工具不得静默删除用户节点。
```

---

## 7. definition scope

### 7.1 Function definition 不属于 Replace

禁止：

```text
replace_scope="function_definition"
```

原因：

```text
Blueprint Function 的 definition / 函数头 / 参数变化归属 Function Signature Management。
ReplaceBlueprintGraph 对 Function 只处理 function_body。
```

### 7.2 Custom Event / Event definition

`custom_event_definition` / `event_definition` 支持删除 / 重建入口节点，但第一版必须保持同名同签名。

规则：

```text
1. 可删除 / 重建入口节点。
2. 必须保持原名称 / 原事件身份。
3. 必须保持原参数签名。
4. 不允许新增参数。
5. 不允许删除参数。
6. 不允许修改参数类型。
7. 不允许修改参数顺序。
8. 不允许修改参数默认值。
9. 必须 full dry_run / full preflight。
10. 必须检查 external_dependents。
11. 存在外部调用方则 blocked。
12. 依赖检查不可用则 blocked。
```

错误码：

```text
signature_change_unsupported
external_dependents_blocked
dependency_check_unavailable
full_dry_run_required
```

---

## 8. 参数读取规则

### 8.1 function_body

函数输入参数在 `function_body` 的 `BlueprintLogicSpec.v1` 中作为初始 temp 暴露。

示例函数：

```text
Function OpenDoor(TargetAngle: float, bForce: bool)
```

读取参数：

```ts
{
  kind: "use_temp",
  ref: "TargetAngle"
}
```

规则：

```text
1. function_body 初始 temp scope 包含函数输入参数。
2. temp_ref 名称等于函数参数名。
3. 函数参数 temp 不由 outputs 声明。
4. 函数参数 temp 不允许被 outputs 重名覆盖。
5. 函数输入参数不存在时，use_temp 返回 temp_ref_not_found。
```

### 8.2 custom_event_body / event_body

Custom Event / Event 参数使用：

```ts
{
  kind: "get_entry_param",
  name: "TargetAngle"
}
```

规则：

```text
1. get_entry_param 只读取当前 Custom Event / Event 入口参数。
2. 不跨 entry。
3. 不读取 Function 参数。
4. 不读取 member variable。
5. 不读取 local variable。
```

---

## 9. return statement

`return` 只允许用于 `function_body`。

规则：

```text
1. 有返回值函数必须有 return。
2. return 必须是顶层 statements 的最后一条。
3. return 不允许出现在 branch.then。
4. return 不允许出现在 branch.else。
5. return 不允许出现在 sequence.steps。
6. 无返回值函数不允许 return。
7. return.values 必须提供函数的全部返回值。
8. 不允许依赖 UE 默认返回 Pin 值。
```

示例：

```ts
{
  kind: "return",
  values: {
    bSuccess: { kind: "literal", value_type: "bool", value: true },
    OutLocation: { kind: "use_temp", ref: "door_location" }
  }
}
```

错误码：

```text
return_not_allowed
return_required
return_must_be_last_statement
nested_return_unsupported
empty_return_unsupported
return_value_missing
return_value_not_found
return_value_type_mismatch
```

---

## 10. Replace body 依赖边界

### 10.1 callee 必须已存在

Replace body 中：

```text
call_event / call_function / call_method 的目标必须在本次 Replace 执行前已存在并可解析。
```

规则：

```text
1. Replace body 不创建新 callee。
2. Replace body 不声明 helper Function。
3. Replace body 不声明 helper Custom Event。
4. Replace 不存在 Append 的“同一次 entries 内部互调”例外。
```

错误码：

```text
callee_not_found
function_not_found
event_not_found
method_not_found
```

### 10.2 允许自调用

Replace body 允许调用当前被替换的同名入口自身。

规则：

```text
1. 自调用必须显式写在 body 中。
2. 工具按普通 call_event / call_function 校验。
3. 不做特殊禁止。
4. 不自动检测并阻断递归。
5. verbose/debug 可报告 potential_recursion。
6. 普通成功结果不默认返回 warning。
```

---

## 11. Replace 不管理变量 / 默认值 / Local Variables

Replace 不创建、不删除、不修改 Local Variable。

```text
1. function_body 中间数据使用 temp_ref。
2. Replace body 不声明 locals。
3. 真实 Local Variable 操作必须使用 Local Variable 工具。
```

Replace 不修改成员变量默认值：

```text
1. 不设置 member default。
2. 不修改 Class Defaults。
3. 成员变量默认值必须使用 set_blueprint_member_default(s)。
```

Replace 不创建成员变量：

```text
1. 只能引用已存在成员变量。
2. 缺失变量返回 variable_not_found。
3. Agent 应先调用 add_blueprint_member_variable。
```

Replace 不创建新 Function / Custom Event / Event：

```text
1. 不允许 body.entries。
2. 不允许 body.helper_functions。
3. 不允许 helper_entries。
```

---

## 12. 保存 / 编译边界

Replace 不支持：

```text
save_after_write
auto_save
compile_after_write
auto_compile
```

规则：

```text
1. Replace 只修改 UE 编辑器内的资产状态。
2. Replace 不保存资产。
3. Replace 不编译资产。
4. Replace 成功后通过 validation.should_compile=true / validation.should_save=true 提示后续 workflow。
5. 编译 / 保存由独立工具处理。
```

---

## 13. 调试与禁止字段

Replace 支持：

```ts
verbose?: boolean
```

`verbose=true` 只影响返回细节，不改变行为。

可返回更多调试信息：

```text
statement_path -> generated node refs
unused_outputs
potential_recursion
ownership analysis detail
full preflight detail
replace target resolution detail
```

Replace 输入明确不支持：

```text
function_definition
save_after_write
compile_after_write
expected_target_state
allow_user_nodes
reason / user_intent
layout_hint
preserve_comments
body.entries
body.locals
body.defaults
body.variable_declarations
body.helper_entries
```

当前不考虑：

```text
并发
乐观锁
expected_target_state
signature_hash
body_hash
dry_run_proof
preflight_proof
```

---

## 14. 命令示例

### 14.1 替换函数体

```ts
replace_blueprint_graph({
  asset_path: "/Game/BP/BP_Door",
  dry_run_mode: "none",
  target: {
    target_type: "function",
    target_name: "CanOpenDoor"
  },
  replace_scope: "function_body",
  body: {
    schema: "BlueprintLogicSpec.v1",
    statements: [
      {
        kind: "call_function",
        call_kind: "pure",
        name: "CheckDoorState",
        outputs: {
          ReturnValue: "can_open"
        }
      },
      {
        kind: "return",
        values: {
          ReturnValue: {
            kind: "use_temp",
            ref: "can_open"
          }
        }
      }
    ]
  }
})
```

### 14.2 Full dry_run 替换 Custom Event definition

```ts
replace_blueprint_graph({
  asset_path: "/Game/BP/BP_Door",
  dry_run_mode: "full",
  target: {
    target_type: "custom_event",
    target_name: "OpenDoor"
  },
  replace_scope: "custom_event_definition",
  body: {
    schema: "BlueprintLogicSpec.v1",
    statements: []
  },
  verbose: true
})
```
