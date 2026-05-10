# BlueprintHelper BlueprintLogicSpec.v1 命令协议

日期：2026-05-04  
范围：TaskSpec / TaskPlan / GraphWrite capability / BlueprintLogicSpec.v1  
状态：架构变更后同步稿  
说明：本文件为 Graph Write 工具簇共享逻辑表达。Task Compiler 可在 TaskSpec 语义和 TaskPlan step args 中使用本 spec，Append 与 Replace capability 均可复用。

架构同步说明：`BlueprintLogicSpec.v1` 不再作为普通 Agent 直接调用底层 MCP 工具的独立入口。Agent 默认提交 `BlueprintHelper.TaskSpec.v1`；Python / MCP Task Compiler 负责把任务语义编译为 TaskPlan，并在需要写图表逻辑时生成本 spec。

---

## 1. 总体定位

`BlueprintLogicSpec.v1` 是 TaskSpec / TaskPlan 可复用的受控蓝图逻辑表达。

```ts
type BlueprintLogicSpec = {
  schema: "BlueprintLogicSpec.v1"
  statements: Statement[]
}
```

规则：

```text
1. 使用语句级 DSL。
2. 不暴露 RawJson。
3. 不暴露 K2 nodes + links。
4. Agent 在 TaskSpec 中表达逻辑意图和控制结构。
5. Python / MCP Task Compiler 和 UE Task Runtime 协作生成 K2 节点、Pin、Link。
6. 第一版不让普通 Agent 管理 node id / pin id / link id。
7. 工具内置 max_depth / max_statement_count / max_branch_count 保护。
```

不使用：

```text
自然语言 body
AgentImportGraph nodes + links 作为主协议
RawJson
custom_node
raw_k2_node
```

---

## 2. Statement

```ts
type Statement =
  | {
      kind: "call_event"
      name: string
      args?: Record<string, ValueExpr>
    }
  | {
      kind: "call_function"
      call_kind?: "auto" | "pure" | "impure"
      name: string
      args?: Record<string, ValueExpr>
      outputs?: Record<string, TempRef>
    }
  | {
      kind: "call_method"
      call_kind?: "auto" | "pure" | "impure"
      target: { kind: "get_member_variable"; name: string }
      method: string
      args?: Record<string, ValueExpr>
      outputs?: Record<string, TempRef>
    }
  | {
      kind: "set_member_variable"
      name: string
      value: ValueExpr
    }
  | {
      kind: "branch"
      condition: BoolExpr
      then: Statement[]
      else?: Statement[]
    }
  | {
      kind: "sequence"
      steps: Statement[]
    }
  | {
      kind: "return"
      values: Record<string, ValueExpr>
    }
```

第一版不支持：

```text
delay
spawn_actor
destroy_actor
print_string
timeline
for_each
cast
interface_call
event_dispatcher_call
custom_node
raw_k2_node
```

---

## 3. ValueExpr

```ts
type ValueExpr =
  | {
      kind: "literal"
      value_type: LiteralValueType
      value: unknown
    }
  | {
      kind: "enum_value"
      enum_path: string
      value: string
    }
  | {
      kind: "asset_ref"
      asset_path: string
      expected_class?: string
    }
  | {
      kind: "class_ref"
      class_path?: string
      asset_path?: string
      expected_parent_class?: string
    }
  | {
      kind: "none_ref"
      expected_type?: "object" | "class" | "asset"
      expected_class?: string
    }
  | {
      kind: "self_ref"
    }
  | {
      kind: "get_member_variable"
      name: string
    }
  | {
      kind: "get_entry_param"
      name: string
    }
  | {
      kind: "call_function"
      call_kind?: "auto" | "pure" | "impure"
      name: string
      args?: Record<string, ValueExpr>
    }
  | {
      kind: "array_literal"
      item_type?: ValueTypeHint
      items: ValueExpr[]
    }
  | {
      kind: "use_temp"
      ref: TempRef
    }

type BoolExpr = ValueExpr
type TempRef = string
```

不支持：

```text
ValueExpr.call_method
map_literal
set_literal
struct_literal
raw expression string
```

---

## 4. LiteralValueType

```ts
type LiteralValueType =
  | "bool"
  | "int"
  | "float"
  | "string"
  | "name"
  | "text"
  | "vector"
  | "rotator"
  | "transform"
  | "linear_color"
```

规则：

```text
1. 第一版只暴露 float，不单独暴露 double。
2. text 表示简单 invariant FText，不支持 namespace / localization key。
3. vector 必须完整 x/y/z。
4. rotator 必须完整 pitch/yaw/roll。
5. transform 必须完整 location / rotation / scale。
6. linear_color 必须完整 r/g/b/a，不强制 0..1。
7. 不支持 FColor / hex color / #RRGGBB。
```

示例：

```ts
{
  kind: "literal",
  value_type: "transform",
  value: {
    location: { x: 0, y: 0, z: 100 },
    rotation: { pitch: 0, yaw: 90, roll: 0 },
    scale: { x: 1, y: 1, z: 1 }
  }
}
```

---

## 5. ValueTypeHint

`ValueTypeHint` 是 LogicSpec 内部轻量类型提示，不等同变量工具完整 `variable_type`。

```ts
type ValueTypeHint =
  | { kind: "bool" }
  | { kind: "int" }
  | { kind: "float" }
  | { kind: "string" }
  | { kind: "name" }
  | { kind: "text" }
  | { kind: "vector" }
  | { kind: "rotator" }
  | { kind: "transform" }
  | { kind: "linear_color" }
  | { kind: "enum"; enum_path: string }
  | { kind: "object"; expected_class?: string }
  | { kind: "class"; expected_parent_class?: string }
  | { kind: "array"; item_type: ValueTypeHint }
```

规则：

```text
1. 只用于 LogicSpec 内部轻量校验。
2. 不用于创建变量。
3. 不用于修改变量类型。
4. array.item_type 不允许再次是 array。
5. 不支持 map / set。
```

---

## 6. 成员变量与组件引用

### 6.1 get_member_variable

允许引用：

```text
1. 普通 Blueprint 成员变量。
2. UE 暴露为 Blueprint 成员变量 / 可解析变量引用的组件变量。
```

```ts
{ kind: "get_member_variable", name: "DoorMesh" }
```

### 6.2 set_member_variable

默认只用于普通成员变量。

规则：

```text
1. 不用于设置组件属性。
2. 如果 name 指向组件变量，默认返回 component_assignment_not_supported 或 property_not_writable。
3. 组件属性修改必须走 Blueprint Component 工具簇。
4. Graph Write 只引用已存在变量，不创建变量。
```

---

## 7. call_function / call_method

### 7.1 args

所有 `args` 只允许按参数名传参：

```ts
args: {
  TargetAngle: { kind: "literal", value_type: "float", value: 90.0 },
  bOpen: { kind: "literal", value_type: "bool", value: true }
}
```

禁止位置参数数组。

### 7.2 call_kind

```ts
call_kind?: "auto" | "pure" | "impure"
```

规则：

```text
1. 默认 auto。
2. call_kind 是 Agent 预期声明，不覆盖 UE 实际函数形态。
3. 工具以 UE 元数据 / Exec Pin 事实为准。
4. 声明与实际不一致时报错。
```

### 7.3 Statement.call_function

规则：

```text
1. impure call_function 表示执行流调用。
2. pure call_function 作为 Statement 时必须声明 outputs。
3. pure Statement.call_function 未声明 outputs 返回 invalid_pure_statement / missing_outputs。
4. pure 调用没有 Exec 语义，不生成无人使用节点。
```

### 7.4 ValueExpr.call_function

用于简单内联 pure 函数取值。

限制：

```text
1. 只允许 pure。
2. 必须有返回值。
3. 只允许 exactly one return value / one output。
4. 多输出函数必须用 Statement.call_function + outputs + use_temp。
```

### 7.5 Statement.call_method

```ts
{
  kind: "call_method",
  call_kind: "pure",
  target: { kind: "get_member_variable", name: "DoorMesh" },
  method: "GetComponentLocation",
  outputs: { ReturnValue: "door_location" }
}
```

规则：

```text
1. call_method 只作为 Statement，不进入 ValueExpr。
2. target 第一版只允许 get_member_variable。
3. target 必须解析为对象 / 组件引用。
4. method 必须存在且 BlueprintCallable。
5. pure method 作为 Statement 时必须声明 outputs。
6. call_method 不用于设置组件默认属性。
```

---

## 8. outputs / temp_ref / use_temp

### 8.1 outputs 双层语义

```ts
outputs: {
  ReturnValue: "door_location"
}
```

```text
ReturnValue = 真实输出 Pin 名。
door_location = Agent 自定义 temp_ref。
```

规则：

```text
1. outputs key 必须匹配真实输出 Pin 名。
2. 不同 statement 可以重复使用 ReturnValue 等输出 Pin 名。
3. outputs value 是 temp_ref。
4. 同一 scope 内 temp_ref 必须唯一。
5. use_temp.ref 引用 temp_ref，不引用输出 Pin 名。
```

### 8.2 TempRef

```regex
^[A-Za-z_][A-Za-z0-9_]*$
```

`temp_ref` 不是成员变量、局部变量、节点 ID、Journal ID 或 Review 定位字段。

### 8.3 pure / impure 前向引用

```text
1. impure call_function / call_method 产生的 temp：只能被后续语句引用。
2. pure call_function / call_method 产生的 temp：允许前向引用，由工具按数据依赖拓扑排序。
3. 无法确认 pure/impure 时按 impure 处理。
4. 循环依赖时报 temp_dependency_cycle。
```

### 8.4 pure outputs 使用

```text
1. pure 调用的 outputs 至少一个 temp 被 use_temp 使用，该 pure 节点才不算孤立。
2. pure outputs 全部无人使用时报 unused_pure_output。
3. pure outputs 中部分未使用的 temp 允许存在。
4. 未使用 pure temp 默认不在普通结果中报告，只在 verbose/debug 中报告。
5. impure outputs 可以不被使用。
```

---

## 9. array_literal

```ts
{
  kind: "array_literal",
  item_type?: ValueTypeHint,
  items: ValueExpr[]
}
```

规则：

```text
1. items 可以包含任意非数组 ValueExpr。
2. 每个 item 递归校验。
3. 每个 item 类型必须兼容目标数组元素类型。
4. item_type 可选，只做预期校验。
5. 不支持嵌套 array_literal。
6. 不支持 map_literal / set_literal。
7. 空数组可不传 item_type，但必须能从目标 Pin 推断元素类型。
```

---

## 10. enum_value

```ts
{
  kind: "enum_value",
  enum_path: string,
  value: string
}
```

规则：

```text
1. Blueprint Enum 使用 asset path：/Game/Enums/E_DoorState。
2. Native Enum 使用 /Script/... path：/Script/Engine.ECollisionEnabled。
3. value 使用枚举项短名 / internal name。
4. 不使用 DisplayName。
5. enum 不放入 literal。
```

---

## 11. UE 引用表达式

### 11.1 asset_ref

```ts
{
  kind: "asset_ref",
  asset_path: "/Game/Audio/SFX_DoorOpen",
  expected_class: "/Script/Engine.SoundBase"
}
```

规则：

```text
1. asset_path 使用 /Game/.../AssetName，不带对象名后缀。
2. expected_class 可选，只做校验。
3. asset_ref 不表示组件实例。
```

### 11.2 class_ref

二选一：

```ts
{ kind: "class_ref", class_path: "/Script/Engine.Actor" }
```

或：

```ts
{ kind: "class_ref", asset_path: "/Game/Blueprints/BP_DoorActor" }
```

规则：

```text
1. class_path 直接解析为 UClass。
2. asset_path 指向 Blueprint Class 资产，由工具读取 GeneratedClass。
3. class_path / asset_path 必须二选一。
4. expected_parent_class 可选，只做继承校验。
```

### 11.3 none_ref

```ts
{
  kind: "none_ref",
  expected_type?: "object" | "class" | "asset",
  expected_class?: string
}
```

表示 UE None / nullptr，只允许用于 object / class / asset 引用类型位置。

### 11.4 self_ref

```ts
{ kind: "self_ref" }
```

规则：

```text
1. self_ref 表示当前 asset_path 对应 Blueprint 的 self。
2. self_ref 可用于任意类型兼容当前 Blueprint self 的参数。
3. self_ref 不表示组件。
4. self_ref 不表示其他 Actor 实例。
```

---

## 12. call_event

规则：

```text
1. call_event 不支持 outputs。
2. call_event.args 允许，且只按参数名传参。
3. call_event.args 必须显式包含 Target。
4. 当前 Blueprint 内事件调用时 Target 使用 self_ref。
5. 所有用户输入参数必须显式传入。
6. 不依赖 UE Pin 默认值。
7. call_event.args.Target 允许任意对象 ValueExpr。
8. Target 的最终类型必须是对象引用。
9. 工具必须校验 Target 对象类型上存在目标 Custom Event / 可调用事件。
```

示例：

```ts
{
  kind: "call_event",
  name: "OpenPhysicsDoor",
  args: {
    Target: { kind: "self_ref" },
    bForce: { kind: "literal", value_type: "bool", value: true },
    TargetAngle: { kind: "literal", value_type: "float", value: 90 }
  }
}
```

---

## 13. get_entry_param

```ts
{
  kind: "get_entry_param",
  name: "TargetAngle"
}
```

规则：

```text
1. Custom Event / Event body 使用 get_entry_param。
2. name 必须匹配当前 entry / target 参数。
3. 不跨 entry。
4. 不读取 member variable。
5. 不读取 local variable。
6. Function body 不用 get_entry_param；Function 输入参数作为初始 temp，用 use_temp 读取。
```

---

## 14. return statement

`return` 只用于 `function_body`。

```ts
{
  kind: "return",
  values: {
    ReturnValue: { kind: "literal", value_type: "bool", value: true }
  }
}
```

规则：

```text
1. return 只允许用于 function_body。
2. return 必须是顶层 statements 的最后一条。
3. return 不允许出现在 branch.then / branch.else / sequence.steps。
4. 有返回值函数必须有顶层最后 return。
5. 无返回值函数不允许 return。
6. return.values 必须提供函数的全部返回值。
7. 不允许依赖 UE 默认返回 Pin 值。
```
