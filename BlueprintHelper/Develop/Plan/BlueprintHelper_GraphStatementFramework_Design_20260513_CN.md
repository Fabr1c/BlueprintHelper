# BlueprintHelper Graph Statement Framework Design 2026-05-13

## 1. 背景

当前 GraphWrite 能力已经覆盖 `append_new_owned_graph`、`replace_owned_graph`、`patch_owned_graph`、`merge_owned_graph` 等策略，但节点生成能力分散在不同路径中。

已观察到的问题：

1. `append_owned_graph` 有较完整的 body 生成路径，可承载一个语句生成多个节点。
2. `merge_owned_graph` 当前仍偏向直接生成单个 callable 节点，再围绕 anchor 接 exec 线。
3. `Object.Function` 这类显式组件/成员调用需要生成对象 getter、函数调用节点、target pin 连接，不能只生成一个 call 节点。
4. 如果后续逐个补 `branch`、`compare`、`select`、`make_struct`，会导致 append/replace/merge/patch 多套逻辑重复膨胀。
5. AgentFace 不应持续增加 UE 节点级字段，否则普通 Agent 会被迫理解 pin、node class、layout、anchor 等底层细节。

因此需要抽象一个公共图表语句框架，使 AgentFace 保持低字段、语义化输入，同时让 UE 端以数据驱动和低代码方式扩展图节点生成能力。

## 2. 目标

1. AgentFace 使用语句树表达逻辑，不暴露 UE 节点图和 pin 图。
2. 后端统一将语句树编译为 Graph Semantic IR，再由 Pattern Registry 生成 NodeFragment。
3. append / replace / merge / patch 共享节点片段生成能力，不再分别实现节点生成。
4. Pattern 以 C++ 注册复杂生命周期，以 JSON 配置别名、pin binding、默认值和简单类型转换。
5. 扩展新能力时优先新增 pattern / binding，而不是扩充 AgentFace 输入字段。
6. Review 粒度对 graph body 收缩到 function / event / macro 级别。
7. 组件、变量、继承、默认值等资产结构变更仍保持当前细粒度 Review。

## 3. 非目标

1. 不让 AgentFace 直接提交 UE 节点类名、pin 名、node guid、layout 坐标。
2. 不允许纯 JSON 定义复杂节点生命周期，例如 timeline、delegate bind、latent async node。
3. 不把 append / merge / replace 的落图策略混成一个低层桥接命令。
4. 不在普通 Agent workflow 中重新暴露 legacy low-level graph tools。
5. 不要求第一阶段支持所有 K2 节点类型。

## 4. 总体架构

```text
AgentFace TaskSpec
-> BlueprintLogicSpec
-> Graph Semantic IR
-> Pattern Registry
-> NodeFragment
-> Graph Composer
-> Append / Replace / Merge / Patch Adapter
-> UE Graph Mutator
```

## 5. 分层职责

| 层 | 职责 | 不负责 |
|---|---|---|
| AgentFace TaskSpec | 描述目标资产、写入策略、目标 function/event/macro、validation、逻辑 body | 不表达 UE 节点、pin、layout、anchor 细节 |
| BlueprintLogicSpec | 使用短名语句树表达逻辑，例如 `call`、`set`、`get`、`branch`、`let` | 不生成节点，不写 Review，不接 UE graph |
| Graph Semantic IR | 规范化语义，解析变量、组件、函数、临时值、类型和控制流 | 不关心具体 UE 节点如何生成 |
| Pattern Registry | 根据 IR 选择 pattern，例如 call function、set variable、branch、compare、select、make struct | 不决定 append/replace/merge 如何落图 |
| NodeFragment | 表达一段可组合节点片段，包括节点、内部连线、exec entry/exit、data input/output、metadata | 不直接插入目标图，不处理 anchor |
| Graph Composer | 串联多个 fragment，连接数据依赖，处理 branch 自动接回，生成 composed graph fragment | 不直接调用 UE mutation |
| Strategy Adapter | append/replace/merge/patch 各自负责落图策略、anchor、旧 body 删除、原 successor 保留 | 不重新实现节点生成 |
| UE Graph Mutator | 执行 Spawn node、connect pin、set default、metadata、transaction、dirty、compile/save | 不理解 AgentFace 高层语义 |

## 6. AgentFace 输入原则

AgentFace 主路径切换到短名语句。

基础字段：

```text
kind
target
args
value
condition
then
else
options
```

高级字段：

```text
id
type
outputs
name
```

字段约束：

1. `kind` 是语义动作，不是 UE 节点类名。
2. `target` 是语义目标，例如 `DoorPanel.AddAngularImpulseInDegrees`、`bDoorOpen`、`OpenDoor`。
3. `args` 使用语义参数名，不要求 Agent 知道真实 pin 名。
4. `value` 可以是 literal、get、call、compare、select、make_struct 等 expression。
5. `condition` 是 expression。
6. `then` / `else` 是嵌套 statement list。
7. `options` 用于少量语义策略，不允许承载 UE node payload。

## 7. BlueprintLogicSpec v2 示例

### 7.1 OpenDoor

```json
{
  "schema": "BlueprintLogicSpec.v2",
  "statements": [
    {
      "kind": "set",
      "target": "bDoorOpen",
      "value": true
    },
    {
      "kind": "call",
      "target": "DoorPanel.AddAngularImpulseInDegrees",
      "args": {
        "Impulse": {
          "kind": "literal",
          "type": "vector",
          "value": {
            "X": 0,
            "Y": 0,
            "Z": 50000
          }
        },
        "bVelChange": true
      }
    }
  ]
}
```

### 7.2 ToggleDoor

```json
{
  "schema": "BlueprintLogicSpec.v2",
  "statements": [
    {
      "kind": "branch",
      "condition": {
        "kind": "get",
        "target": "bDoorOpen"
      },
      "then": [
        {
          "kind": "call",
          "target": "CloseDoor"
        }
      ],
      "else": [
        {
          "kind": "call",
          "target": "OpenDoor"
        }
      ]
    }
  ]
}
```

### 7.3 显式命名临时值

```json
{
  "schema": "BlueprintLogicSpec.v2",
  "statements": [
    {
      "kind": "let",
      "name": "DoorYaw",
      "value": {
        "kind": "get_property",
        "target": "DoorPanel.RelativeRotation.Yaw"
      }
    },
    {
      "kind": "branch",
      "condition": {
        "kind": "compare",
        "op": "<",
        "left": {
          "kind": "ref",
          "name": "DoorYaw"
        },
        "right": {
          "kind": "get",
          "target": "MaxOpenAngle"
        }
      },
      "then": [
        {
          "kind": "call",
          "target": "OpenDoor"
        }
      ],
      "else": []
    }
  ]
}
```

## 8. 语句树与数据流图

AgentFace 使用语句树：

```text
sequence
  set bDoorOpen = true
  call DoorPanel.AddAngularImpulseInDegrees
    Impulse = vector(0, 0, 50000)
    bVelChange = true
```

内部 Graph Semantic IR 可以转换为数据流/控制流图：

```text
n1: literal vector(0,0,50000) -> n4.Impulse
n2: literal bool(true)        -> n4.bVelChange
n3: get DoorPanel            -> n4.Target
n4: call AddAngularImpulseInDegrees
```

Branch 的语句树：

```text
branch
  condition: get bDoorOpen
  then: call CloseDoor
  else: call OpenDoor
```

Branch 的内部图：

```text
n1: get bDoorOpen -> n2.Condition
n2: branch
n2.Then -> n3: call CloseDoor
n2.Else -> n4: call OpenDoor
n3.Exit -> join
n4.Exit -> join
join -> next statement
```

确认决策：

1. `then` / `else` 默认自动接回后续语句。
2. 只有显式 `return`、`break_flow`、`stop_flow` 或 latent 终止语义才阻止自动接回。

## 9. Graph Semantic IR

Graph Semantic IR 是 BlueprintLogicSpec 的规范化中间表示。

职责：

1. 将短名语句转换为稳定 typed IR。
2. 解析 `target` 字符串。
3. 区分函数、组件、变量、临时值、属性路径。
4. 执行基础类型推断。
5. 建立控制流和数据依赖。
6. 生成 statement-level identity，用于 Review / Debug / diagnostics。

示例 IR：

```json
{
  "kind": "call",
  "statement_id": "stmt_OpenDoor_002",
  "call": {
    "target_kind": "component_member_function",
    "object": {
      "kind": "component_ref",
      "name": "DoorPanel"
    },
    "function": {
      "query": "AddAngularImpulseInDegrees"
    },
    "args": {
      "Impulse": {
        "kind": "literal",
        "type": "vector",
        "value": {
          "X": 0,
          "Y": 0,
          "Z": 50000
        }
      },
      "bVelChange": {
        "kind": "literal",
        "type": "bool",
        "value": true
      }
    }
  }
}
```

## 10. Pattern Registry

Pattern Registry 是从 Semantic IR 到 NodeFragment 的分发层。

核心 pattern：

```text
call
set
get
branch
let
compare
select
make_struct
get_property
set_property
cast
return
```

Pattern 选择规则：

1. `kind=call` + `target=A.B` + `A` 是 component/member -> component member call pattern。
2. `kind=call` + `target=/Script/X:Y` -> owner-qualified function call pattern。
3. `kind=call` + `target=PrintString` -> graph-aware function call pattern。
4. `kind=set` + target 是 member variable -> set member variable pattern。
5. `kind=branch` -> branch pattern。
6. `kind=compare` -> comparison/operator pattern。

Pattern 输出 NodeFragment，不直接写图。

## 11. C++ Pattern 与 JSON Binding 分工

### 11.1 C++ Pattern 负责

1. Spawn UE 节点。
2. Allocate dynamic pins。
3. Resolve graph-compatible function。
4. 生成对象 getter。
5. 连接 target pin。
6. 处理 exec entry / exit。
7. 验证 UE graph 兼容性。
8. 生成 NodeFragment。
9. 生成 diagnostics。

复杂生命周期必须写 C++ pattern：

```text
timeline
delegate bind/unbind
latent async node
macro expansion
dynamic pin node
complex struct split/recombine
enhanced input binding node
```

### 11.2 JSON Binding 负责

低代码扩展只允许覆盖已有 pattern 的轻量绑定：

1. function alias。
2. pin alias。
3. 默认参数。
4. 简单类型转换。
5. display name / native name 映射。
6. pattern enable / disable。
7. 项目级命名约定。

示例：

```json
{
  "schema": "BlueprintHelper.GraphPatternBindings.v1",
  "pattern": "call",
  "aliases": {
    "add_angular_impulse": "AddAngularImpulseInDegrees"
  },
  "pin_aliases": {
    "vel_change": "bVelChange",
    "velocity_change": "bVelChange"
  },
  "defaults": {
    "bVelChange": true
  }
}
```

配置位置：

```text
插件内置：Resources/GraphPatterns/*.json
项目覆盖：Config/BlueprintHelper/GraphPatterns/*.json
```

加载规则：

1. 先加载插件内置配置。
2. 再加载项目覆盖配置。
3. 项目配置可以覆盖 alias / default / enable 状态。
4. 项目配置不能覆盖 C++ pattern 的安全边界。

## 12. NodeFragment Contract

NodeFragment 是一个可组合图片段。

必须包含：

```text
fragment_id
source_statement_id
nodes[]
internal_links[]
exec_entry
exec_exit
data_inputs[]
data_outputs[]
pin_bindings[]
layout_hints
ownership_tags
review_targets
diagnostics
```

### 12.1 Exec

```json
{
  "exec_entry": {
    "node": "call_AddAngularImpulse",
    "pin": "execute"
  },
  "exec_exit": {
    "node": "call_AddAngularImpulse",
    "pin": "then"
  }
}
```

Branch fragment 可有多个 exit：

```json
{
  "exec_entry": {
    "node": "branch_001",
    "pin": "execute"
  },
  "exec_exits": {
    "then": {
      "node": "branch_001",
      "pin": "then"
    },
    "else": {
      "node": "branch_001",
      "pin": "else"
    }
  }
}
```

### 12.2 Data

```json
{
  "data_outputs": {
    "DoorPanel": {
      "node": "get_DoorPanel",
      "pin": "DoorPanel",
      "type": "StaticMeshComponent"
    }
  }
}
```

### 12.3 Ownership

Ownership metadata 由 adapter / mutator 最终写入，但 fragment 必须带来源：

```json
{
  "ownership_tags": {
    "body_scope": "custom_event_body",
    "body_name": "OpenDoor",
    "statement_id": "stmt_OpenDoor_002"
  }
}
```

## 13. Graph Composer

Graph Composer 输入多个 NodeFragment，输出 ComposedGraphFragment。

职责：

1. 顺序串联普通 statement。
2. 编排 branch then/else。
3. 默认将 branch then/else exit 接回后续 statement。
4. 解析 `let` 临时值生命周期。
5. 连接 expression data dependency。
6. 安排局部 layout。
7. 合并 diagnostics。
8. 汇总 review targets。

Composer 不直接修改 UE 图。

### 13.1 Sequence composition

```text
fragment A exit -> fragment B entry
fragment B exit -> fragment C entry
```

### 13.2 Branch auto join

```text
branch.Then -> then body entry
then body exit -> join / next

branch.Else -> else body entry
else body exit -> join / next
```

如果某分支为空：

```text
branch.Then -> join / next
```

如果分支显式终止：

```text
branch.Then -> return
return has no exit
```

## 14. Strategy Adapter

### 14.1 Append Adapter

职责：

1. 创建新 owned body。
2. 使用 Composer 生成完整 body fragment。
3. 将 body fragment 放入目标 graph。
4. 写入 body-level ownership metadata。
5. 生成 function/event/macro body 级 Review evidence。

适用：

```text
新 custom event body
新 function body
新 owned graph body
```

### 14.2 Replace Adapter

职责：

1. 定位已有 owned body。
2. 删除旧 body 内旧 owned nodes。
3. 使用 Composer 生成新 body fragment。
4. 保持 entry signature。
5. 写入新的 ownership metadata。
6. 生成 body-level Review evidence。

适用：

```text
替换 OpenDoor body
替换 InitializeDoor body
替换 function body
```

### 14.3 Merge Adapter

职责：

1. 使用 block-scoped anchor 定位插入点。
2. 使用 Composer 生成插入 fragment。
3. 将 fragment 插入 anchor 与 successor 之间，或执行 branch fork。
4. 保留原 successor 可达性。
5. 防止 orphaned nodes。
6. 只处理落图和 anchor，不生成节点细节。

第一阶段限制：

1. merge 需要从“单 CallNode 插入”升级为“ComposedFragment 插入”。
2. 在升级前，不开放依赖 target pin wiring 的 `Object.Function` merge。

### 14.4 Patch Adapter

职责：

1. 修改已有 owned fragment 内节点属性。
2. 修改 pin default。
3. 修改 node comment / position。
4. 不负责生成全新复杂逻辑。

## 15. UE Graph Mutator

UE Graph Mutator 是唯一直接改图层。

职责：

1. Spawn node。
2. Allocate / reconstruct pins。
3. Connect pins。
4. Set pin defaults。
5. Set node positions。
6. Write metadata。
7. Mark blueprint dirty。
8. Handle transaction scope。
9. Compile/save by validation policy。

Mutator 输入应是 mutation plan，不是 AgentFace TaskSpec。

## 16. Review 粒度

确认决策：

1. Graph body 默认按 function/event/macro 生成 Review。
2. 一个 function/event body 内部生成多少节点，都默认显示为一条 body Review。
3. Reject `OpenDoor` 表示拒绝 `OpenDoor` body 内全部节点变化。
4. DebugBundle 和 diff 高亮可以保留 fragment/node 级 evidence。
5. 组件、变量、继承、默认值更改保持当前细粒度。

Graph Review 示例：

```text
修改了 [OpenDoor] 事件
修改了 [CloseDoor] 事件
修改了 [ToggleDoor] 事件
```

结构 Review 示例：

```text
新增 [DoorPanel] 组件
修改 [bDoorOpen] 变量
修改 [OpenKickImpulse] 变量默认值
修改 ParentClass
```

## 17. Debug 与 Diagnostics

Preview diagnostics：

1. statement path。
2. normalized target。
3. selected pattern。
4. resolver result。
5. binding result。
6. type conversion result。
7. fragment shape summary。

Execute diagnostics：

1. mutation plan id。
2. spawned nodes count。
3. created links count。
4. failed node/pin path。
5. UE graph mutation error。

DebugBundle 可包含：

1. TaskSpec input。
2. normalized BlueprintLogicSpec。
3. Graph Semantic IR。
4. Pattern resolution log。
5. Fragment summary。
6. Composer plan summary。
7. Adapter mutation plan summary。
8. UE result summary。

默认 CLI 响应不得直接返回大体积 DebugBundle 内容，只返回 summary / DebugCase id / artifact reference policy 内允许的字段。

## 18. Validation / Dry-run

Dry-run 应覆盖：

1. AgentFace schema validation。
2. BlueprintLogicSpec semantic validation。
3. target resolution。
4. pattern resolution。
5. function / variable / component resolver。
6. pin binding。
7. type compatibility。
8. fragment composability。
9. adapter anchor validation。
10. mutation plan safety validation。

Dry-run 不应：

1. 生成持久节点。
2. 写 ReviewRecord。
3. 修改 Blueprint。
4. 依赖当前编辑器选中状态。

## 19. 迁移策略

### 19.1 AgentFace 短名切换

新主路径：

```text
call
set
get
branch
let
compare
select
make_struct
```

旧字段逐步移除：

```text
call_function
set_member_variable
```

迁移规则：

1. 第一阶段：短名和旧名都可编译到同一 Semantic IR。
2. 第二阶段：文档主推短名，旧名标记 deprecated。
3. 第三阶段：普通 AgentGuide 移除旧名。
4. 第四阶段：测试和 fixtures 去旧名。
5. 第五阶段：Compiler 拒绝旧名或仅 expert compatibility path 支持。

### 19.2 Append / Replace / Merge 收敛

第一阶段：

1. 抽出 `GraphStatementBuilder`。
2. append / replace 使用公共 builder。
3. 保持 merge 旧路径，但不新增复杂能力。

第二阶段：

1. 抽出 `NodeFragment`。
2. append / replace 使用 `GraphComposer`。
3. merge 引入 `ComposedFragment` 插入。

第三阶段：

1. merge 支持 `Object.Function` target wiring。
2. merge 支持 branch fragment。
3. merge read-back 校验 fragment reachability。

第四阶段：

1. patch 以 fragment metadata 识别 owned nodes。
2. Review / Debug 使用 statement/body identity 聚合。

## 20. 初始 Pattern 清单

P0：

```text
literal
get
set
call
branch
let
ref
compare
```

P1：

```text
select
make_struct
break_struct
get_property
set_property
cast
return
```

P2：

```text
delegate_bind
delegate_unbind
timeline
latent_call
async_task
enhanced_input_bind
```

## 21. 物理门目标映射

### 21.1 InitializeDoor

```json
[
  {
    "kind": "set",
    "target": "bDoorOpen",
    "value": false
  }
]
```

### 21.2 OpenDoor

```json
[
  {
    "kind": "set",
    "target": "bDoorOpen",
    "value": true
  },
  {
    "kind": "call",
    "target": "DoorPanel.AddAngularImpulseInDegrees",
    "args": {
      "Impulse": {
        "kind": "literal",
        "type": "vector",
        "value": {
          "X": 0,
          "Y": 0,
          "Z": 50000
        }
      },
      "bVelChange": true
    }
  }
]
```

### 21.3 CloseDoor

```json
[
  {
    "kind": "set",
    "target": "bDoorOpen",
    "value": false
  },
  {
    "kind": "call",
    "target": "DoorPanel.AddAngularImpulseInDegrees",
    "args": {
      "Impulse": {
        "kind": "literal",
        "type": "vector",
        "value": {
          "X": 0,
          "Y": 0,
          "Z": -50000
        }
      },
      "bVelChange": true
    }
  }
]
```

### 21.4 ToggleDoor

```json
[
  {
    "kind": "branch",
    "condition": {
      "kind": "get",
      "target": "bDoorOpen"
    },
    "then": [
      {
        "kind": "call",
        "target": "CloseDoor"
      }
    ],
    "else": [
      {
        "kind": "call",
        "target": "OpenDoor"
      }
    ]
  }
]
```

## 22. 风险

1. Pattern 过度数据驱动会导致运行时错误难以诊断。
2. Fragment contract 如果过早设计过大，会增加实现成本。
3. Merge adapter 如果未完成 ComposedFragment 插入就开放复杂 pattern，会产生假阳性 preview。
4. Review 粒度收缩后，需要保留足够内部 evidence 支撑 diff 高亮和 DebugBundle。
5. 旧 `call_function` / `set_member_variable` 迁移期间需要兼容已有测试和文档。

## 23. 实施建议

推荐顺序：

1. 定义 `BlueprintLogicSpec.v2` 短名 schema。
2. 在 task-core 中将短名和旧名编译到统一 Semantic IR。
3. UE 端新增 `FBlueprintHelperGraphStatementBuilder`。
4. 定义 `FBlueprintHelperNodeFragment`。
5. 将 append call/set 迁移到 builder。
6. 将 replace body 迁移到 builder。
7. 引入 `FBlueprintHelperGraphComposer`。
8. 支持 branch auto join。
9. merge 从单 CallNode 插入升级为 ComposedFragment 插入。
10. 更新 Review evidence 为 graph body 级聚合。
11. 接入 JSON binding loader。
12. 添加 DebugBundle 输出 IR / Fragment / ComposerPlan 摘要。

## 24. 验收标准

1. AgentFace 可以用短名 `call` / `set` 实现 OpenDoor / CloseDoor。
2. AgentFace 可以用嵌套 `branch.then/else` 实现 ToggleDoor。
3. `DoorPanel.AddAngularImpulseInDegrees` 不需要 Agent 暴露 target pin。
4. append / replace 使用同一个 statement builder。
5. merge 支持插入 composed fragment 后，原 successor 仍可达。
6. ReviewPanel 对 `OpenDoor` 只显示一条 body 级 Review。
7. 组件/变量/default/继承仍按当前结构级粒度显示 Review。
8. JSON binding 可新增 pin alias，不需要改 C++。
9. 新复杂节点 pattern 需要 C++ 注册，不允许纯 JSON 绕过安全边界。

## 25. 已确认决策

1. AgentFace 使用语句树。
2. 支持显式命名临时值。
3. `branch.then` / `branch.else` 默认自动接回后续语句。
4. Graph body Review 按 function/event/macro 聚合。
5. Pattern JSON 配置采用插件内置和项目覆盖两级目录。
6. 直接切换到短名语句，旧字段随短名实现进度逐步移除。
