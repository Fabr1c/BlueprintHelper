# BlueprintHelper AppendBlueprintGraph TaskPlan 能力协议

日期：2026-05-04  
范围：Graph Write / AppendBlueprintGraph / TaskPlan step args  
状态：架构变更后同步稿  
依赖：公共协议、BlueprintLogicSpec.v1

架构同步说明：`AppendBlueprintGraph` 不是普通 Agent 默认直调入口。Agent 默认通过任务级 MCP 工具提交 TaskSpec；Python / MCP Task Compiler 判断需要追加新图表或空图表逻辑时，生成 TaskPlan step，再由 UE Task Runtime 调用本 capability。本文字段同时保留给 debug / expert / 测试入口。

---

## 1. 工具职责

`AppendBlueprintGraph` 第一版只负责：

```text
向新图表或空图表中创建一个或多个完整命名 Custom Event entry，
并为每个 entry 写入独立 BlueprintHelper-owned 逻辑块。
```

不负责：

```text
1. 写入非空图表。
2. 接入已有执行流。
3. 替换已有实现。
4. Patch 精确修改。
5. 创建 Function。
6. 创建 engine event / input action event / overlap / tick / begin play。
7. 创建成员变量 / Local Variable / 默认值。
8. 自动创建缺失 callee。
9. 自动改名 entry。
10. 自动 cleanup 旧 block。
```

---

## 2. TaskPlan step 输入结构

```ts
type AppendBlueprintGraphRequest = {
  asset_path: string
  dry_run_mode: "none" | "quick" | "full"

  target_graph: string
  create_graph_if_missing?: boolean

  entries: AppendGraphEntry[]

  verbose?: boolean
}
```

`verbose?: boolean` 仅影响返回调试细节，不改变执行、安全、权限、dry_run_mode 或 ownership 行为。

---

## 3. target_graph

图表目标字段使用：

```ts
target_graph: string
```

规则：

```text
1. target_graph 不存在且 create_graph_if_missing=true：允许创建图表。
2. target_graph 不存在且 create_graph_if_missing=false：失败。
3. target_graph 已存在但为空：允许写入。
4. target_graph 已存在且非空：失败。
5. 不自动改名图表。
6. 不自动切换到 Merge / Replace / Patch。
```

---

## 4. create_graph_if_missing

```ts
create_graph_if_missing?: boolean
```

语义：

```text
true：target_graph 不存在时允许创建。
false：target_graph 不存在时报错。
未传：按 AppendBlueprintGraph 工具默认策略处理。
```

该字段只属于 Append。Replace / Patch / Merge 不允许自动创建目标图表。

---

## 5. entries

```ts
type AppendGraphEntry = {
  entry_type: "custom_event"
  entry_name: string
  params?: EntryParam[]
  body: BlueprintLogicSpec
}
```

规则：

```text
1. entries = TaskPlan 请求创建的逻辑入口列表。
2. block_id / block_ref 由 UE capability 生成，不由 Agent 指定。
3. 输入侧不使用 blocks / block_ids / block_refs。
4. 一次 Append 可以创建多个 Custom Event entry。
5. 同一次 entries 内定义的 custom_event 可以互相 call_event 调用。
```

---

## 6. entry_type

第一版只允许：

```ts
entry_type: "custom_event"
```

禁止：

```text
function
engine_event
input_action_event
overlap_event
hit_event
tick_event
begin_play
construction_script
macro
interface_event
override_event
```

函数创建、函数签名、Event Signature、Interface、Override 等进入后续专用工具簇。

---

## 7. entry_name

规则：

```text
1. entry_name 必须由 Agent 显式提供。
2. 同名 Custom Event 已存在时失败。
3. 不自动改名。
4. 不支持 name_collision_policy。
5. 不支持 auto_rename / replace_existing。
```

错误码：

```text
entry_name_conflict
```

---

## 8. params

Custom Event entry 可以定义输入参数：

```ts
type EntryParam = {
  name: string
  type: ValueTypeHint
}
```

规则：

```text
1. params 定义新建 Custom Event 的输入 Pin。
2. 只支持输入参数。
3. 不支持输出参数。
4. 不支持 ref/out 参数。
5. 不支持默认参数值。
6. params[].name 必须在当前 entry 内唯一。
7. params[].type 使用 ValueTypeHint。
8. ValueTypeHint 支持一维 array；不支持嵌套 array / map / set。
```

`params[].name` 字符集：

```regex
^[A-Za-z_][A-Za-z0-9_]*$
```

不强制命名风格，允许：

```text
bForce
b_force
TargetAngle
target_angle
```

---

## 9. body

```ts
body: {
  schema: "BlueprintLogicSpec.v1"
  statements: Statement[]
}
```

规则：

```text
1. body 使用 BlueprintLogicSpec.v1。
2. 第二层 schema 使用短命名。
3. body 不使用 RawJson / nodes+links / 自然语言。
4. body 不声明 entries。
5. body 不创建变量。
6. body 不创建 Function。
7. body 不创建 Event。
```

---

## 10. entry params 与 get_entry_param

当前 Custom Event body 中读取入口参数使用：

```ts
{ kind: "get_entry_param", name: "TargetAngle" }
```

规则：

```text
1. get_entry_param 只读取当前 entry.params 中定义的参数。
2. 不跨 entry。
3. 不读取 member variable。
4. 不读取 local variable。
5. 不等于 temp_ref。
```

同一 `entry.body` 内：

```text
entry param name 与 temp_ref 共用数据命名空间，不允许同名。
```

错误码：

```text
entry_data_name_conflict
entry_param_name_conflict
temp_ref_conflict
```

---

## 11. callee 规则

Append 中 `call_event / call_function` 允许调用：

```text
1. 目标 Blueprint 中已存在的 Custom Event。
2. 目标 Blueprint 中已存在的 Function。
3. 同一次 AppendBlueprintGraph entries 中定义的其他 Custom Event。
```

禁止：

```text
1. call_event 目标不存在时自动创建空 Custom Event。
2. call_function 目标不存在时自动创建 Function。
3. create_missing_callees。
4. 偷偷补接口函数、Override、函数图或事件入口。
```

错误码：

```text
callee_not_found
function_not_found
event_not_found
```

---

## 12. call_event Target

`call_event.args` 必须显式包含 `Target`。

规则：

```text
1. 当前 Blueprint 内事件调用时 Target 使用 self_ref。
2. 所有用户输入参数也必须显式传入。
3. 不依赖 UE Pin 默认值。
4. call_event.args.Target 允许任意对象 ValueExpr。
5. Target 的最终类型必须是对象引用。
6. 工具必须校验 Target 对象类型上存在目标 Custom Event / 可调用事件。
```

示例：

```ts
{
  kind: "call_event",
  name: "OpenPhysicsDoor",
  args: {
    Target: { kind: "self_ref" },
    bForce: { kind: "literal", value_type: "bool", value: true }
  }
}
```

---

## 13. dry_run_mode

Append 作为写工具必须显式传：

```ts
dry_run_mode: "none" | "quick" | "full"
```

规则：

```text
dry_run_mode="quick" 或 "full"：
不写资产，返回 status=dry_run / modified=false / data.dry_run。

dry_run_mode="none"：
正式写入请求，写入前仍执行内部 preflight。
```

Append 第一版只写新图表或空图表；非空图表在 dry_run 或正式写入 preflight 中均失败。

---

## 14. 命令示例

```ts
append_blueprint_graph({
  asset_path: "/Game/BP/BP_Door",
  dry_run_mode: "full",
  target_graph: "EG_PhysicsDoor",
  create_graph_if_missing: true,
  entries: [
    {
      entry_type: "custom_event",
      entry_name: "OpenPhysicsDoor",
      params: [
        { name: "bForce", type: { kind: "bool" } },
        { name: "TargetAngle", type: { kind: "float" } }
      ],
      body: {
        schema: "BlueprintLogicSpec.v1",
        statements: [
          {
            kind: "call_method",
            target: { kind: "get_member_variable", name: "DoorMesh" },
            method: "SetSimulatePhysics",
            args: {
              bSimulate: { kind: "literal", value_type: "bool", value: true }
            }
          }
        ]
      }
    }
  ]
})
```
