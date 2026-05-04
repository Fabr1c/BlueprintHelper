# BlueprintHelper TaskPlan 能力侧协议：Graph Write / PatchBlueprintGraph

日期：2026-05-04  
范围：TaskSpec / TaskPlan / Graph Write / PatchBlueprintGraph  
状态：架构变更后同步稿  
Schema 命名规则：第二层 schema 使用短命名，例如 `PatchBlueprintGraph.v1`、`BlueprintLogicSpec.v1`。  
依赖公共协议：所有 TaskPlan 写入 step / expert 写工具输入必须包含 `asset_path` 与 `dry_run_mode`，不得包含 Safety Profile 覆盖、权限覆盖、保存/编译联动等字段。

架构同步说明：`PatchBlueprintGraph` 不再作为普通 Agent 默认直调入口。Agent 通过 TaskSpec 描述精确修改意图；Python / MCP Task Compiler 负责补足上下文、定位目标并生成 TaskPlan step；UE Task Runtime 调用本 capability。本文字段同时保留给 debug / expert / 测试入口。

---

## 0. 工具职责边界

`PatchBlueprintGraph` 的最小职责是：**精确修改已有目标点**。

允许修改的目标：

```text
1. existing node
2. existing pin
3. existing link
4. existing node property
5. existing pin default
6. existing connection
7. existing node metadata / comment 等明确字段
```

第一版不允许：

```text
1. 创建新 Function。
2. 创建新 Custom Event。
3. 创建新 graph。
4. 创建新 BlueprintHelper-owned block。
5. 创建新的逻辑节点结构。
6. 根据自然语言模糊查找目标。
7. 大范围替换完整 body。
8. 接入已有执行流中的一段新逻辑。
```

Graph Write 工具职责分工：

```text
Append  → 新图表 / 空图表追加独立入口逻辑
Replace → 替换明确目标完整实现
Merge   → 把新逻辑接入已有执行流
Patch   → 精确修改已有点
```

---

## 1. TaskPlan step 输入结构总览

`PatchBlueprintGraph` 使用 `target` 对象表达定位范围，使用 `patch` 对象表达具体修改操作。

```ts
patch_blueprint_graph({
  asset_path: string,

  target:
    | {
        target_type: "node"
        target_graph: string
        node_path: string
      }
    | {
        target_type: "pin"
        target_graph: string
        pin_path: string
      }
    | {
        target_type: "link"
        target_graph: string
        link_path: string
      },

  patch:
    | {
        operation: "set_pin_default"
        expected_old_value?: StaticValueExpr
        value: StaticValueExpr
      }
    | {
        operation: "set_node_comment"
        expected_old_comment?: string
        comment: string
      }
    | {
        operation: "set_node_metadata"
        key: string
        expected_old_state?: {
          exists: boolean
          value?: string
        }
        value: string
      }
    | {
        operation: "connect_pins"
        from_pin_path: string
        to_pin_path: string
      }
    | {
        operation: "disconnect_link"
      },

  dry_run_mode: "none" | "quick" | "full",

  verbose?: boolean
})
```

字段分工：

```text
asset_path：
目标 Blueprint 资产。

target：
定位修改目标或定位上下文。

patch：
描述具体要执行的精确修改。

dry_run_mode：
none / quick / full，所有写工具必填。

verbose：
可选调试输出，不改变执行语义。
```

禁止平铺混用：

```ts
patch_blueprint_graph({
  asset_path: "/Game/BP/BP_Door",
  target_graph: "EG_PhysicsDoor",
  node_path: "...",
  operation: "set_pin_default",
  value: ...
})
```

---

## 2. PatchTarget

### 2.1 第一版只支持 node / pin / link

```ts
type PatchTarget =
  | {
      target_type: "node"
      target_graph: string
      node_path: string
    }
  | {
      target_type: "pin"
      target_graph: string
      pin_path: string
    }
  | {
      target_type: "link"
      target_graph: string
      link_path: string
    }
```

规则：

```text
1. target_graph 必填。
2. node_path / pin_path / link_path 必须是完整 LogicJson path。
3. 不接受 node_ref / pin_ref / link_ref 作为 Patch 输入主定位字段。
4. 不接受 block_id 直接定位 Patch 目标。
5. 不接受 function / event / custom_event 作为 PatchTarget。
6. 如果 Agent 只有 block_id 或局部 ref，必须先读取 LogicJson 并反推出完整 path。
```

### 2.2 不使用 UE 原生 GUID 作为 TaskSpec / TaskPlan 定位字段

第一版不允许使用：

```text
node_guid
pin_guid
link_guid
```

Patch 输入统一使用 LogicJson full path：

```ts
target: {
  target_type: "pin",
  target_graph: "EG_PhysicsDoor",
  pin_path: "groups[0].nodes[2].pins.Execute"
}
```

禁止：

```ts
target: {
  target_type: "pin",
  target_graph: "EG_PhysicsDoor",
  pin_guid: "..."
}
```

规则：

```text
1. Patch 第一版只接受 node_path / pin_path / link_path。
2. 不接受 node_guid / pin_guid / link_guid。
3. 不接受 node_ref / pin_ref / link_ref。
4. UE GUID 可作为工具内部实现定位依据。
5. verbose/debug 可返回 path -> guid 映射。
6. Journal 内部可记录 GUID，但不作为普通 Agent 输入协议。
```

---

## 3. PatchOperation 第一版

第一版最小安全操作集：

```ts
type PatchOperation =
  | {
      operation: "set_pin_default"
      expected_old_value?: StaticValueExpr
      value: StaticValueExpr
    }
  | {
      operation: "set_node_comment"
      expected_old_comment?: string
      comment: string
    }
  | {
      operation: "set_node_metadata"
      key: string
      expected_old_state?: {
        exists: boolean
        value?: string
      }
      value: string
    }
  | {
      operation: "connect_pins"
      from_pin_path: string
      to_pin_path: string
    }
  | {
      operation: "disconnect_link"
    }
```

operation 与 target_type 匹配规则：

```text
target_type="pin"
→ set_pin_default

target_type="node"
→ set_node_comment
→ set_node_metadata

target_type="link"
→ disconnect_link

connect_pins
→ 可以使用 target 作为图表上下文，但必须显式提供 from_pin_path / to_pin_path
```

第一版暂不支持：

```text
set_node_property
disconnect_pin
disconnect_all_pin_links
delete_node
delete_link
rename_node
set_pin_type
create_node
create_link_without_existing_pins
```

错误码建议：

```text
patch_operation_target_mismatch
unsupported_patch_operation
pin_default_type_mismatch
node_metadata_key_invalid
pin_connection_type_mismatch
link_not_found
```

---

## 4. set_pin_default

### 4.1 value 使用 StaticValueExpr

`set_pin_default.value` 复用 `BlueprintLogicSpec.v1` 的 `ValueExpr` 体系，但只允许静态子集 `StaticValueExpr`。

```ts
type StaticValueExpr =
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
      kind: "array_literal"
      item_type?: ValueTypeHint
      items: StaticValueExpr[]
    }
```

允许：

```ts
patch: {
  operation: "set_pin_default",
  value: {
    kind: "literal",
    value_type: "float",
    value: 90
  }
}
```

```ts
patch: {
  operation: "set_pin_default",
  value: {
    kind: "enum_value",
    enum_path: "/Script/Engine.ECollisionEnabled",
    value: "QueryAndPhysics"
  }
}
```

禁止动态表达式：

```text
use_temp
call_function
get_member_variable
get_entry_param
self_ref
```

规则：

```text
1. set_pin_default 只设置静态默认值。
2. 不创建运行时数据连接。
3. 不调用函数。
4. 不读取成员变量。
5. 不读取 entry 参数。
6. 不引用 temp。
7. array_literal 内部也只能包含 StaticValueExpr。
8. 目标 Pin 类型必须兼容该静态值。
```

错误码：

```text
pin_default_dynamic_value_unsupported
pin_default_type_mismatch
invalid_static_value_expr
```

### 4.2 expected_old_value

```ts
{
  operation: "set_pin_default",
  expected_old_value?: StaticValueExpr,
  value: StaticValueExpr
}
```

规则：

```text
1. expected_old_value 表示旧 Pin 默认值。
2. 类型为 StaticValueExpr。
3. 如果当前 Pin 默认值与 expected_old_value 不匹配，则失败。
4. expected_old_value 可选。
5. expected_old_value 不用于权限判断。
6. expected_old_value 不替代 dry_run_mode。
7. expected_old_value 不自动修复冲突。
```

错误码：

```text
expected_old_value_mismatch
expected_old_value_type_mismatch
```

### 4.3 不允许作用于已有连接 Pin

如果目标 Pin 已有输入连接 / 数据连接，则 `set_pin_default` 失败。

规则：

```text
1. 工具不自动断开连接。
2. 工具不设置“可能被连接覆盖”的默认值。
3. Agent 若要替换为默认值，必须先 disconnect_link，再 set_pin_default。
4. set_pin_default 只负责设置未连接 Pin 的静态默认值。
```

错误码：

```text
pin_has_connection
pin_default_ignored_due_to_connection
```

正确替换流程：

```text
1. 读取 LogicJson，定位 link_path。
2. patch_blueprint_graph(... operation="disconnect_link")
3. patch_blueprint_graph(... operation="set_pin_default")
```

### 4.4 不允许作用于 Exec Pin

```text
Exec Pin 没有默认值。
set_pin_default 只允许作用于 Data Pin。
```

错误码：

```text
pin_default_not_supported
exec_pin_default_unsupported
```

---

## 5. set_node_comment

### 5.1 基础结构

```ts
{
  operation: "set_node_comment"
  expected_old_comment?: string
  comment: string
}
```

规则：

```text
1. set_node_comment 只作用于 target_type="node"。
2. comment 类型为 string。
3. 修改用户节点注释仍按 ownership + dry_run_mode 风险处理。
```

### 5.2 允许空字符串

`comment=""` 合法，表示清空目标节点的 `NodeComment`。

```ts
patch: {
  operation: "set_node_comment",
  comment: ""
}
```

规则：

```text
1. 空字符串表示清空注释。
2. 工具不得把空字符串解释为“不修改”。
```

错误码：

```text
patch_operation_target_mismatch
node_comment_value_invalid
```

### 5.3 expected_old_comment

```ts
{
  operation: "set_node_comment",
  expected_old_comment?: string,
  comment: string
}
```

规则：

```text
1. expected_old_comment 表示旧 NodeComment。
2. 类型为 string。
3. 空字符串合法，表示旧注释为空。
4. 如果当前 NodeComment 与 expected_old_comment 不匹配，则失败。
```

错误码：

```text
expected_old_comment_mismatch
```

---

## 6. set_node_metadata

### 6.1 只允许 BlueprintHelper namespace

第一版只允许写 `BlueprintHelper.*` namespace 下的 metadata。

允许：

```ts
patch: {
  operation: "set_node_metadata",
  key: "BlueprintHelper.ReviewNote",
  value: "checked"
}
```

禁止：

```ts
patch: {
  operation: "set_node_metadata",
  key: "SomeOtherPlugin.State",
  value: "..."
}
```

规则：

```text
1. key 必须以 BlueprintHelper. 开头。
2. 不允许修改 UE 内部 metadata。
3. 不允许修改用户自定义 metadata。
4. 不允许修改其他插件 metadata。
5. 第一版只支持 set，不支持 delete / clear。
6. value 第一版为 string。
```

错误码：

```text
node_metadata_key_not_allowed
unsupported_metadata_namespace
metadata_delete_unsupported
```

### 6.2 expected_old_state

`set_node_metadata` 使用 `expected_old_state` 表达旧 metadata 状态，不使用 `expected_old_value`。

```ts
{
  operation: "set_node_metadata"
  key: string
  expected_old_state?: {
    exists: boolean
    value?: string
  }
  value: string
}
```

规则：

```text
1. expected_old_state 可选。
2. 不传 expected_old_state：不检查旧 metadata 状态，直接 set。
3. expected_old_state.exists=false：要求当前 key 不存在。
4. expected_old_state.exists=true：要求当前 key 存在。
5. exists=true 且提供 value：要求当前 key 存在且值匹配。
6. exists=true 但不提供 value：只要求 key 存在，不校验值。
7. exists=false 时不允许提供 value。
8. value 是新 metadata 值。
```

只在 key 不存在时写入：

```ts
{
  operation: "set_node_metadata",
  key: "BlueprintHelper.ReviewNote",
  expected_old_state: {
    exists: false
  },
  value: "checked"
}
```

只在 key 存在且旧值为 `"pending"` 时写入：

```ts
{
  operation: "set_node_metadata",
  key: "BlueprintHelper.ReviewNote",
  expected_old_state: {
    exists: true,
    value: "pending"
  },
  value: "checked"
}
```

错误码：

```text
expected_old_metadata_state_mismatch
invalid_expected_old_metadata_state
```

---

## 7. connect_pins

### 7.1 必须显式 from_pin_path / to_pin_path

```ts
{
  operation: "connect_pins",
  from_pin_path: string,
  to_pin_path: string
}
```

规则：

```text
1. from_pin_path 必须是完整 LogicJson pin_path。
2. to_pin_path 必须是完整 LogicJson pin_path。
3. from / to 方向必须由 Agent 显式声明。
4. target 只作为图表上下文或校验上下文。
5. 不允许 target pin + other_pin_path 的隐式方向写法。
6. 不允许工具根据 Pin 类型自动推断 from/to 字段方向。
```

示例：

```ts
patch_blueprint_graph({
  asset_path: "/Game/BP/BP_Door",
  target: {
    target_type: "pin",
    target_graph: "EG_PhysicsDoor",
    pin_path: "groups[0].nodes[1].pins.Then"
  },
  patch: {
    operation: "connect_pins",
    from_pin_path: "groups[0].nodes[1].pins.Then",
    to_pin_path: "groups[0].nodes[2].pins.Execute"
  },
  dry_run_mode: "none"
})
```

校验：

```text
1. 两个 Pin 必须存在。
2. Pin 类型必须兼容。
3. Exec Pin 方向必须正确。
4. Data Pin 方向必须正确。
5. 如果目标 Pin 已有连接且不允许多连，返回 pin_already_connected。
```

### 7.2 不自动断开已有连接

`connect_pins` 只创建新连接，不自动断开任何已有连接。

规则：

```text
1. 如果 from_pin_path 或 to_pin_path 对应 Pin 不支持多连接且已有连接，则失败。
2. 工具不得自动断开旧连接。
3. 工具不得自动替换旧连接。
4. 不提供 replace_existing。
5. 不提供 auto_disconnect。
```

如果 Agent 要替换连接，应拆成两个 Patch 调用：

```text
1. patch_blueprint_graph(... operation="disconnect_link")
2. patch_blueprint_graph(... operation="connect_pins")
```

错误码：

```text
pin_already_connected
pin_connection_type_mismatch
```

---

## 8. disconnect_link

`disconnect_link` 只接受 `target_type="link"`。

允许：

```ts
patch_blueprint_graph({
  asset_path: "/Game/BP/BP_Door",
  target: {
    target_type: "link",
    target_graph: "EG_PhysicsDoor",
    link_path: "groups[0].nodes[1].links[0]"
  },
  patch: {
    operation: "disconnect_link"
  },
  dry_run_mode: "none"
})
```

禁止：

```ts
target: {
  target_type: "pin",
  target_graph: "EG_PhysicsDoor",
  pin_path: "..."
},
patch: {
  operation: "disconnect_link"
}
```

规则：

```text
1. disconnect_link 只断开一个明确 link_path。
2. 不支持通过 pin_path 断开所有连接。
3. 不支持 disconnect_pin_links。
4. 不支持 disconnect_all。
5. Agent 必须先读取 LogicJson，定位具体 link_path。
```

错误码：

```text
patch_operation_target_mismatch
link_not_found
```

---

## 9. 批量边界

`PatchBlueprintGraph` 第一版不支持批量操作。

规则：

```text
1. 不提供 patches[]。
2. 不提供 batch_patch。
3. 不提供 partial_success。
4. 不提供 allow_partial。
5. 一个 target + 一个 patch。
6. 多点修改由 Task Compiler 拆成多个 TaskPlan Patch step。
7. 如果需要替换完整 body，应使用 ReplaceBlueprintGraph。
```

---

## 10. ownership / dry_run_mode 风险规则

Patch 的 quick / full 要求由目标 ownership 决定。

### 10.1 BlueprintHelper-owned 目标

如果 Patch 目标属于 BlueprintHelper-owned：

```text
dry_run_mode="quick"：允许
dry_run_mode="full"：允许
dry_run_mode="none"：正式写入前普通 preflight
```

适用示例：

```text
1. 修改 BlueprintHelper-owned 节点的 Pin 默认值。
2. 修改 BlueprintHelper-owned 节点的 NodeComment。
3. 修改 BlueprintHelper-owned 节点的 BlueprintHelper.* metadata。
4. 断开 / 连接 BlueprintHelper-owned block 内部明确连线。
```

### 10.2 user-owned / mixed / unknown 目标

如果 Patch 目标属于：

```text
user-owned
mixed ownership
unknown ownership
```

则：

```text
dry_run_mode="quick"：不允许
dry_run_mode="full"：允许预演
dry_run_mode="none"：正式写入前必须 full preflight
```

原因：

```text
Patch 虽然是精确修改，但仍可能影响用户节点、用户 Pin、用户连线或执行流。
```

当目标不是明确 BlueprintHelper-owned，而 Agent 传：

```ts
dry_run_mode: "quick"
```

返回：

```text
full_dry_run_required
```

### 10.3 正式写入

当 Agent 传：

```ts
dry_run_mode: "none"
```

工具必须：

```text
1. 解析目标 path。
2. 判断目标 ownership。
3. 如果 owned-only，执行普通 preflight 后写入。
4. 如果 user-owned / mixed / unknown，执行 full preflight。
5. full preflight 不通过则阻断，不写入。
```

---

## 11. verbose

`PatchBlueprintGraph` 支持：

```ts
verbose?: boolean
```

规则：

```text
1. 默认 false。
2. verbose=true 只影响返回细节。
3. verbose=true 不改变执行行为。
4. verbose=true 不改变权限。
5. verbose=true 不改变 Safety Profile。
6. verbose=true 不允许绕过 dry_run_mode。
7. verbose=true 不允许绕过 ownership / preflight。
```

可返回的额外信息示例：

```text
path -> guid 映射
target ownership analysis
current value summary
connection compatibility detail
generated diff detail
expected_old_* mismatch detail
```

---

## 12. 明确不支持字段 / 能力

Patch 第一版明确不支持：

```text
batch patches / patches[]
node_ref / pin_ref / link_ref
node_guid / pin_guid / link_guid
block_id 直接定位
function / event / custom_event 高层定位
set_node_property
disconnect_pin_links
delete_node
delete_link
rename_node
create_node
replace_existing connection
auto_disconnect
save_after_write
compile_after_write
reason / user_intent
layout_hint
preserve_comments
```

---

## 13. ToolResult / Journal 说明

普通成功结果仍保持极简，重点返回：

```text
status
modified
data.patch_result
validation
```

Patch 不应默认返回完整 diff、GUID 映射、ownership 细节。  
这些信息可以进入：

```text
1. Transaction Journal
2. Review Store
3. verbose=true 返回
```

Patch 正式写入成功应由 UE 插件内部生成 transaction_id 并写入 Journal / Review；普通工具成功结果不必默认向 Agent 暴露 transaction / review / safety 字段。
