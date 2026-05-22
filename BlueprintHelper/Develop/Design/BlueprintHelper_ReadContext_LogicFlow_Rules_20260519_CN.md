# BlueprintHelper ReadContext LogicFlow 压缩规则

日期：2026-05-19

状态：规则已收敛，等待实现落地。

## 目标

`logic_flow` 是 ReadContext 的最压缩逻辑阅读格式，用于让普通 Agent 快速理解已适配 Blueprint 逻辑结构的主执行关系和关键数据依赖。

它不是 `logic_json` 的替代品，也不承诺可逆写回。需要稳定 `node_ref` / `pin_ref` / `link_ref` / `block_id` 锚点时必须读取 `logic_json`。

`logic_flow` 内部分为两种模式：

1. `execflow`：以 ExecPin 执行链为骨架，数据依赖嵌入节点输入参数。
2. `dataflow`：以值表达式 / 数据依赖为骨架，用 `$pN` 分段表达纯数据图或 ExecPin 很少的计算图。

## 三档格式定位

| 格式 | 压缩等级 | 推荐场景 | 不推荐场景 |
| --- | --- | --- | --- |
| `logic_flow` | 最高压缩 | 简单 `function` / `event` / `custom_event`，目标是先读懂执行链 | 全图读取、写入锚点、复杂分支/循环/debug |
| `logic_md` | 中等压缩 | 较大或逻辑链较复杂的 `function` / `event` / `custom_event`，需要按 Entry / Execution / Data / Orphans 分区查看 | 需要精确 patch/merge anchor |
| `logic_json` | 最完整 | 全量读取、未知规模读取、结构化分析、diff、patch、merge、debug | 只想快速理解一个简单入口时 |

默认选择规则：

1. 读简单 `function` / `event` / `custom_event`：优先 `logic_flow`。
2. 读较大、分支较多、循环较多或数据依赖较复杂的入口：使用 `logic_md`。
3. 读全量 Blueprint / Graph、读未知规模、需要稳定写入锚点、需要 debug：使用 `logic_json`。

## 输出契约

建议 payload：

```json
{
  "schema": "LogicFlow.v1",
  "mode": "execflow",
  "flow": "EventBeginPlay -> Init[] -> SetReady[Value=true]",
  "stats": {
    "nodes": 3,
    "exec_links": 2,
    "data_links": 1,
    "orphans": 0
  },
  "warnings": []
}
```

字段规则：

1. `payload.mode` 表示主模式，只允许 `execflow` 或 `dataflow`。
2. `payload.flow` 是主要阅读内容。
3. `payload.stats` 只保留结构化计数，不在 `flow` 内重复输出统计行。
4. `payload.warnings` 只放降级、歧义、截断、未知节点/未知 link 等压缩风险。
5. `payload.schema` 使用短 schema 名 `LogicFlow.v1`，不带 `BlueprintHelper.` 前缀。

模式选择：

1. 存在稳定 ExecPin 主链时使用 `execflow`。
2. 没有 ExecPin、ExecPin 只用于返回/提交结果，或图主要由 pure 节点计算构成时使用 `dataflow`。
3. `execflow` 中仍可内联 data expression；只有当数据依赖成为主体时才切到 `dataflow`。
4. Timeline / Async 等拥有独立回调出口的结构可以在外层 `execflow` 内声明子 `execflow` / `dataflow` 块。
5. Macro / Collapsed Graph 在 `LogicFlow.v1` 中只保留调用边界，不默认展开内部；Agent 需要内部语义时使用 function chain 或专用读工具按需读取。

## 基础语法

节点表达式：

```txt
NodeName[输入参数](输出参数)
```

省略规则：

1. 没有输入参数时省略 `[]`。
2. 没有输出参数时省略 `()`。
3. 只有执行流、没有关键数据依赖时只输出节点名。

执行流：

```txt
A -> B -> C
```

数据引用：

```txt
&        前一个执行节点
&%-1     前两个执行节点
&%1      后一个执行节点
&.Pin    相对节点的输出 Pin
$p0      提前提升的共享 pure 节点结果
@n4      压缩输出中的短节点消歧 id
```

示例：

```txt
事件Secondary Thumbstick(Axis_X,Axis_Y) -> DoLook[Yaw=&.Axis_X, Pitch=&.Axis_Y]
事件Primary Thumbstick(Axis_X,Axis_Y) -> DoMove[Right=&.Axis_X, Forward=&.Axis_Y]
事件Touch Jump Start -> DoJumpStart
事件Touch Jump End -> DoJumpEnd
orphans: 注释 x2
```

`dataflow` 示例：

```txt
dataflow:
  $p0 = GetActorLocation[]
  $p1 = GetVelocity[]
  $p2 = *[$p1, DeltaSeconds]
  $p3 = +[$p0, $p2]
  ReturnValue = $p3
```

## 参数输出规则

输入参数 `[]`：

1. 只输出已连接参数和非默认字面量参数。
2. 默认值、空值、`Self`、`WorldContextObject` 默认省略。
3. 参数顺序优先使用节点 pin 顺序，不能稳定取得时按导出顺序。
4. 参数名使用显示 pin 名；同名 pin 需要加序号或短 pin 名消歧。

输出参数 `()`：

1. 事件入口输出事件参数，例如 `EventTick(DeltaSeconds)`。
2. 普通 impure 节点只有在输出被后续节点使用时才输出。
3. 单输出 pure 节点内联时可以省略输出 pin 名。
4. 多输出 pure 节点必须保留 pin 名，例如 `BreakHitResult[Hit].bBlockingHit`。

## Pure 节点压缩

没有 ExecPin 的节点默认嵌套到使用它的输入参数中：

```txt
SetActorLocation[NewLocation=+[GetActorLocation[], *(GetVelocity[], DeltaSeconds)]]
```

共享 pure 结果被多处使用时，提升为 `$pN`：

```txt
$p0 = GetPlayerCharacter[PlayerIndex=0]
BeginPlay -> SetOwner[NewOwner=$p0] -> AttachToActor[ParentActor=$p0]
```

提升规则：

1. 同一 pure 节点被两个以上参数使用时提升。
2. 嵌套深度超过 3 层时提升。
3. 内联后单行过长时提升。
4. 存在副作用不明或 latent/async 风险时不要当作 pure 内联，保留为节点。

## 分支和多执行输出

Branch：

```txt
EventInteract -> Branch[Condition=IsValid[Object=Target]]
  True -> UseActor[Target=Target]
  False -> PrintString[InString="No target"]
```

Sequence：

```txt
BeginPlay -> Sequence
  Then0 -> InitHUD
  Then1 -> BindInput
  Then2 -> LoadSave
```

Switch：

```txt
InputAction -> SwitchOnEnum[Selection=Mode]
  Idle -> StartIdle
  Combat -> StartCombat
  Default -> StartFallback
```

规则：

1. 多执行输出必须按 exec pin 名分支，不允许压成单条链。
2. 分支块使用两空格缩进。
3. 分支内部继续使用同一套 `Node[inputs](outputs)` 规则。
4. 分支过深或分支数量过多时，`warnings` 提示建议升档到 `logic_md`。

## 循环、Latent 和 Async

ForEachLoop：

```txt
ForEachLoop[Array=Enemies]
  LoopBody(ArrayElement,ArrayIndex) -> ApplyDamage[DamagedActor=&.ArrayElement]
  Completed -> PrintString[InString="Done"]
```

ForLoop：

```txt
ForLoop[FirstIndex=0, LastIndex=Count]
  LoopBody(Index) -> ProcessIndex[Index=&.Index]
  Completed -> Finish
```

Delay / Timeline / Async：

```txt
BeginPlay -> Delay[Duration=1.0]
  Completed -> SpawnActor[Class=EnemyClass](ReturnValue)
```

规则：

1. Loop 必须保留 `LoopBody` 和 `Completed`。
2. Latent 节点必须保留完成 pin，例如 `Completed` / `Finished`。
3. Async 节点必须保留 delegate-like 输出执行 pin。
4. Timeline 建议只在 `logic_flow` 中保留执行出口和关键 float/vector 输出；复杂轨道升档到 `logic_md` 或 `logic_json`。

## 变量、属性和结构体

变量读取作为值：

```txt
Branch[Condition=IsDead]
```

变量写入作为执行节点：

```txt
EventDamage(Damage) -> Set Health[Value=Clamp[-(Health, &.Damage), 0, MaxHealth]]
```

结构体 Break / Make：

```txt
SetActorLocation[NewLocation=MakeVector[X=0, Y=0, Z=BreakHitResult[Hit].Location.Z]]
```

规则：

1. `Get Variable` 默认压成变量名。
2. `Set Variable` 保留为 `Set VariableName[Value=...]`。
3. `Break Struct` 多输出按 `.Field` 引用。
4. `Make Struct` 作为函数式 value 表达式内联。

## Cast、Spawn 和组件调用

Cast：

```txt
ActorBeginOverlap(OtherActor) -> CastToBP_Enemy[Object=&.OtherActor](AsBP_Enemy)
  Success -> ApplyDamage[DamagedActor=&.AsBP_Enemy]
  CastFailed -> PrintString[InString="Not enemy"]
```

Spawn：

```txt
SpawnActor[Class=EnemyClass, Transform=SpawnTransform](ReturnValue) -> SetOwner[Target=&.ReturnValue, NewOwner=Self]
```

组件调用：

```txt
BeginPlay -> Mesh.SetVisibility[NewVisibility=false]
```

规则：

1. Cast 成功路径和失败路径必须保留。
2. Spawn 的 `ReturnValue` 如果立即被使用，保留输出。
3. `Target` 是组件或变量时，可压成 `Target.FunctionName[...]`。
4. `Target=Self` 默认省略。

## Delegate、事件绑定和自定义事件

绑定 delegate：

```txt
BeginPlay -> BindEvent[Event=OnHealthChanged, CustomEvent=HandleHealthChanged]
```

调用 delegate：

```txt
SetHealth -> Broadcast OnHealthChanged[NewHealth=Health]
```

规则：

1. Delegate 绑定必须保留被绑定事件名。
2. Custom Event 作为入口时按普通事件入口输出。
3. 事件分发器调用保留 `Broadcast` 语义。
4. 多个绑定目标时逐行输出，避免把事件关系压成普通函数调用。

## Macro、Collapsed Graph 和 Function Call

规则：

1. 普通函数调用按 `FunctionName[inputs](outputs)` 输出。
2. Macro / Collapsed Graph 在 `logic_flow` 中当作边界节点输出，不展开内部。
3. Macro / Collapsed Graph 内部语义不属于 `LogicFlow.v1` 默认职责；Agent 需要时通过 function chain 或专用读工具继续读取。
4. Macro 节点需要保留输入、输出和可见执行出口，避免主链丢失上下文。
5. 如果 Macro / Collapsed Graph 的控制流出口无法可靠识别，在 `warnings` 中提示 `macro_boundary_ambiguous`。

## Reroute、Knot、注释和孤岛节点

规则：

1. Reroute / Knot 默认折叠，不作为节点输出。
2. 注释节点默认不进入执行流。
3. 注释作为 orphan 时合并计数：`orphans: 注释 x2`。
4. 非注释 orphan 节点需要输出名称；超过 5 个时压缩为计数并提示升档。

示例：

```txt
orphans: 注释 x2, PrintString x1
```

## Unknown 和降级

遇到无法可靠分类的节点或 link：

1. 保留节点名。
2. 保留已知输入输出 pin。
3. 不猜测执行语义。
4. 在 `warnings` 中记录 `unknown_node` / `unknown_link` / `ambiguous_flow`。

示例：

```txt
@n7 UnknownNode[Input=A](Output)
```

## 升档条件

满足任一条件时，Agent 应从 `logic_flow` 升档到 `logic_md` 或 `logic_json`：

1. 需要写入、patch、merge、定位 anchor。
2. 目标不是 `function` / `event` / `custom_event`。
3. 读取全量 Blueprint / Graph。
4. `warnings` 包含未知控制流或歧义。
5. 分支深度超过 3 层。
6. 分支数量超过 8 个。
7. pure 嵌套压缩后仍难以阅读。
8. 发现大量 delegate、Timeline、async proxy，或 Macro / Collapsed Graph 边界出口存在歧义。
9. 需要检查节点坐标、布局、GUID、pin/link 原始属性。

升档优先级：

```txt
读懂复杂入口 -> logic_md
需要精确结构/锚点/全量/debug -> logic_json
```

## 实现约束

1. `logic_flow` 应从结构化逻辑模型生成，不能从 `logic_md` 文本正则反解析。
2. 生成器应复用 `logic_json` 已有的节点、link、entry、stats 语义来源。
3. `logic_flow` 是独立 payload schema：`LogicFlow.v1`。
4. ReadContext capabilities 中的 format 顺序按压缩程度表达：`logic_flow`, `logic_md`, `logic_json`。
5. `graph_context` 和需要 anchor 的场景继续只推荐 `logic_json`。
6. 普通输出不暴露 UE GUID；如需 debug/专家视图，走 expert/debug artifact 边界。

## 验收标准

1. 简单输入事件链能压成 1 行一个入口。
2. Branch / Sequence / Loop / Delay 能保留控制流 pin 名。
3. Pure 数据依赖默认内联，共享 pure 能提升为 `$pN`。
4. `NodeName[输入参数](输出参数)` 的空输入/空输出能正确省略。
5. 写入相关工作流不会把 `logic_flow` 当作 anchor 来源。
6. 文档、capabilities、ReadSpec schema、测试用例同步描述三档格式。
