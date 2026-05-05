# BlueprintHelper Graph Write / Setup / Cleanup 综合设计汇总

日期：2026-05-03  
适用范围：BlueprintHelper v0.4/v0.5 规划前置设计  
状态：已同步字段协议 Diff 的修订稿

---

## 0. 本文目的

本文整合并修正当前已确认的 BlueprintHelper 相关设计记忆，重点覆盖：

- Setup Profile 与安全档位
- Graph Write 工具簇：Append / Replace / Patch / Merge
- transaction_id / block_id / operation_id
- BlueprintHelper-owned ownership 标记
- Transaction Journal / Review / Diff / Rollback 数据记录
- Cleanup / Ownership 工具簇
- dry_run 与 Review 的关系
- runtime_profile / diagnostics / ToolResultBase 字段边界
- LogicMD / LogicJson 分组读取与精确读工具规划

本文用于后续实现、文档拆分、测试用例更新和 Agent Skill 生成。

---

## 0.1 本次同步 Diff 摘要（2026-05-03）

本版本同步已确认的字段协议和 Agent 规则差异：

```text
1. 普通能力工具不默认向 Agent 返回 transaction / review / safety；Graph Write、Cleanup、Ownership、Rollback 等高风险或后续引用流程可按工具需要暴露必要摘要。
2. safety_profile 只从 runtime_profile.active_profile 读取；单次工具结果不携带 safety_profile。
3. dry_run 数据只在 status=dry_run 时放在 data.dry_run。
4. runtime_profile.tool_capabilities 使用 unavailable_only 负向稀疏模式，不是完整工具索引，也不是 MCP schema。
5. diagnostics 是只读诊断，实际报告在 data.markdown；Markdown 中的 Blocking 不等于工具调用失败。
6. LogicMD 的 target_graph / blueprint / multi_target 是多入口分组读取，返回 grouped=true。
7. LogicJson 的 target_graph / blueprint / multi_target 使用 logic.groups[]；node_ref / link_ref 是 group 内局部引用。
8. 本文未发现父类修改写工具旧字段，因此无需做对应删除；Parent Class 修改不属于本文件的 Graph Write / Setup / Cleanup 范围。
9. Graph Write 成功返回采用极简 Agent-facing 口径：Append 只返回 graph / block_refs / write_ref / validation；Replace 只返回 target / write_ref / validation；Patch 只返回 target / patch / write_ref / validation。created_nodes / created_links / summary 等进入 Journal / Review，不默认返回。
```

---

# 1. 已修正的关键口径

## 1.1 transaction_id 口径修正

最终规则：

```text
一个 transaction_id 对应一次写工具调用。
```

transaction_id 不再表示：

```text
从打开 Editor 到关闭 Editor 的整个会话
```

也不记录：

```text
读操作、打开资产、编译、保存、PIE、preflight、搜索资产
```

transaction_id 只用于记录一次会修改项目或蓝图状态的写操作，例如：

```text
CreateBlueprint
AddVariable
AddFunction
AddComponent
SetClassDefaultProperty
AppendBlueprintGraph
ReplaceBlueprintGraph
PatchBlueprintGraph
MergeBlueprintGraph
CleanupBlueprintHelperBlock
CleanupBlueprintHelperFeature
ConvertBlueprintHelperBlockToUserOwned
```

生成方：

```text
UE 插件侧生成和管理。
```

用途：

```text
蓝图审阅、Diff、Rollback、Recovery、操作审计。
```

如果一次写工具调用修改多个蓝图或多个图表，仍属于同一个 transaction_id；Review UI 需要按蓝图、图表、函数、block_id 分组展示。

Agent-facing 暴露规则：

```text
transaction_id 可由 UE 插件侧为写操作内部生成，并写入 Transaction Journal / Review Store。
但并非所有写工具都必须把 transaction / review 字段默认返回给 Agent。
普通能力工具（Asset Factory、Blueprint Component、Blueprint Class Settings 等）成功结果应聚焦 status、modified、data.*_result、validation。
Graph Write、Cleanup、Ownership、Rollback 等需要后续引用 block_id / rollback / review 的工具，可按工具需要向 Agent 暴露必要的 transaction / block / rollback 摘要。
```


---

## 1.2 block_id 口径修正

最终规则：

```text
block_id = {图表名或函数名}_{调用名称}{递增id}
```

示例：

```text
EG_PhysicsDoor_TogglePhysicsDoor0
EG_PhysicsDoor_TogglePhysicsDoor1
OpenDoor_SetDoorOpen0
```

递增 id 作用域：

```text
同一蓝图 + 同一图表/函数 + 同一调用名称 内从 0 递增。
```

规则：

- block_id 由工具生成，不由 Agent 生成。
- block_id 不包含蓝图文件名，因为 asset_path / target_blueprint 是调用前提。
- block_id 统一使用下划线 `_`，不使用连字符 `-`。
- NodeComment 可以显示人类可读内容，但稳定标识仍使用 `_`。
- block_id 必须写入 Metadata 与 NodeComment。
- ReplaceBlueprintGraph 替换同一个 BlueprintHelper-owned block 时保留原 block_id。

---

## 1.3 dry_run 口径修正

即使未来实现 Review / Diff / Rollback，dry_run 仍然必要。

最终定位：

```text
dry_run = 写入前安全预检
Review = 写入后用户审阅
```

dry_run 不应成为频繁打断用户的确认流程，而应由工具和 Agent 在写入前拦截明显错误。

dry_run 负责检查：

```text
权限
目标资产/图表
风险等级
命名冲突
全局事件禁用
目标函数/事件是否存在
Pin / Schema / K2 可连接性
ownership / dependency
Cleanup 删除范围
```

Review 负责：

```text
展示最终 diff
按蓝图/图表/函数/block_id 审阅
接受 / 拒绝 / 回滚
压缩或归档 Transaction Journal
```

Conservative 下：

```text
dry_run 无 error / conflict 可自动正式写入。
warning 不阻断，但写完后阶段报告说明。
error / conflict 阻断。
```

---

# 2. Setup Profile 与安全档位

## 2.1 Agent 默认场景

默认 Agent 客户端：

```text
Claude Code
```

如果没有加载 BlueprintHelper Skill / 项目引导文件，需要通过项目根目录：

```text
CLAUDE.md
AGENTS.md
Setup Profile
```

暴露插件文档、工具边界和安全策略。

---

## 2.2 缺失能力默认策略

Agent 遇到工具能力缺失时：

```text
立即停止并报告缺失工具。
```

不应假装继续完成。

用户手动补齐只能作为额外测试，不计入 Agent 独立完成能力。

---

## 2.3 四个安全档位

Setup 应提供：

```text
ReadOnly / 只读
Conservative / 保守
Standard / 标准
AutoRepair / 自动修复
```

Claude Code 默认：

```text
Conservative / 保守
```

---

## 2.4 Conservative 默认策略

Conservative 下：

```text
允许写操作。
高风险图表必须 dry_run。
dry_run 无 error / conflict 可自动正式写入。
info / warning 不阻断，写完后阶段报告说明。
error / conflict 默认停止并报告。
不自动 cleanup 旧 BlueprintHelper-owned 导入块。
不自动修改用户手写节点。
用户明确指定目标函数/图表时，允许修改该目标内的用户节点。
```

---

## 2.5 Standard 默认策略

Standard 下：

```text
旧 BlueprintHelper-owned block 冲突时，允许自动 cleanup 后重试。
但只清理同 block_id 的节点。
```

---

## 2.6 高风险图表定义

默认高风险：

```text
图表内已经写好的事件图
图表内已经写好的函数图
已有用户节点的图表
已有 BlueprintHelper-owned block 的图表
需要修改已有执行链的图表
```

除非用户明确指定要修改哪个函数或哪个图，否则 Agent 不应直接修改已有图表。

Agent 写事件逻辑时，应优先创建新的事件图表：

```text
EG_{FeatureName}
```

---

## 2.7 全局事件默认策略

Agent 默认不应创建或重复创建：

```text
BeginPlay
Tick
ConstructionScript
InputAction 入口
ActorBeginOverlap
ActorEndOverlap
ActorHit
生命周期事件
引擎回调事件
```

默认应创建完整命名的 Custom Event，例如：

```text
ShotBullet 功能 → Custom Event: ShotBullet
PhysicsDoor 功能 → Custom Event: TogglePhysicsDoor
```

如果需要接入已有 BeginPlay / Tick / InputAction / Overlap，应使用 MergeBlueprintGraph，并明确接入点。

---

## 2.8 命名策略

函数图命名规则由 Setup 配置。

用户未指定命名偏好时，默认使用描述型命名：

```text
TogglePhysicsDoor
InitializePhysicsDoor
ApplyDoorImpulse
ResetPhysicsDoor
```

不使用：

```text
NewFunction
DoThing
过短泛名
```

## 2.9 runtime_profile 使用规则

每个写入任务进入写入阶段前，Agent 必须读取一次 runtime_profile。

runtime_profile 的职责是提供当前运行时事实：

```text
Bridge / UE 插件状态
config_status
write_permission / Token 状态
risk_command 状态
active_profile.safety_profile
active_profile.missing_capability_policy
tool_capabilities unavailable_only 列表
```

runtime_profile 不负责提供：

```text
完整工具说明
完整 MCP tool schema
完整命名偏好全文
完整蓝图 / C++ 边界全文
Transaction / Review 历史
```

tool_capabilities 采用负向稀疏模式：

```json
{
  "tool_capabilities": {
    "mode": "unavailable_only",
    "unavailable": [
      {
        "cluster": "graph_write",
        "capability": "merge",
        "status": "unavailable",
        "reason": "not_implemented"
      }
    ]
  }
}
```

Agent 必须理解：

```text
未出现在 unavailable 中，不代表 runtime_profile 已完整确认其 schema。
runtime_profile 不是工具索引，也不是 MCP schema 文档。
具体工具边界来自 AgentGuide / tools 文档；具体参数来自当前 MCP tool schema。
stop_and_report 由 Agent 根据当前任务、missing_capability_policy、不可用能力和安全替代路径判断。
```

## 2.10 diagnostics 边界

Diagnostics 是只读诊断工具，用于安装、配置、Bridge、runtime 链路问题定位。

Diagnostics 返回 ToolResultBase 外壳，实际报告在：

```text
data.markdown
```

Diagnostics 不返回：

```text
blocking / warning / info JSON 数组
```

Markdown 固定包含：

```md
## Blocking
...

## Warning
...
```

`## Info` 可选。

如果 diagnostics 命令自身执行成功，即使 Markdown 中存在 Blocking，也应返回：

```text
ok=true
status=completed
```

Markdown Blocking 表示诊断报告发现阻断环境条件，不表示 MCP 工具调用失败。只有：

```text
ok=false
status=failed
```

才表示 diagnostics 工具自身失败。

---

# 3. Graph Write 工具簇总览

废弃含糊的 Import 命名。

Graph Write 工具簇使用：

```text
AppendBlueprintGraph
ReplaceBlueprintGraph
PatchBlueprintGraph
MergeBlueprintGraph
```

旧工具：

```text
blueprint_import_agent_graph
```

应标记为 Deprecated / Legacy，并提示使用明确写入工具。

---

# 4. AppendBlueprintGraph

## 4.1 最小职责

AppendBlueprintGraph 只负责：

```text
追加新的独立逻辑块。
```

它可以：

```text
创建新的 EG_{FeatureName} 事件图表
向已有图表追加独立逻辑块
创建唯一命名的 Custom Event
创建普通节点
创建新节点之间的连线
写入 BlueprintHelper-owned Metadata + NodeComment
```

它不可以：

```text
自动连接已有节点
自动接入已有执行流
覆盖旧节点
删除旧节点
清理旧 block
修改用户节点
创建全局事件节点
创建函数图
追加到函数图（Claude Code Conservative 下禁止）
```

---

## 4.2 新事件图表规则

Claude Code Conservative 下：

```text
AppendBlueprintGraph 允许创建新的 EG_{FeatureName}。
这是默认推荐路径。
```

图表名冲突处理：

```text
如果同名图表不存在：创建新图表并写入。
如果同名图表已存在且为空：允许继续写入。
如果同名图表已存在且非空：返回 error。
不自动改名。
```

---

## 4.3 Custom Event 规则

Append 创建 Custom Event 时：

```text
事件名必须唯一。
重名直接 error。
不自动改名。
```

Append 不允许创建全局事件节点。

一次 Append 可写入多个独立 Custom Event，例如：

```text
EG_PhysicsDoor
- InitializePhysicsDoor
- TogglePhysicsDoor
- OpenPhysicsDoor
- ClosePhysicsDoor
```

一个 transaction_id 对应该次 Append 写工具调用。

每个独立 Custom Event 逻辑入口生成一个 block_id：

```text
EG_PhysicsDoor_InitializePhysicsDoor0
EG_PhysicsDoor_TogglePhysicsDoor0
EG_PhysicsDoor_OpenPhysicsDoor0
EG_PhysicsDoor_ClosePhysicsDoor0
```

---

## 4.4 同一 transaction 内部调用

同一次 AppendBlueprintGraph write transaction 内：

```text
允许多个新建 Custom Event 互相调用。
```

例如：

```text
TogglePhysicsDoor
→ Branch bDoorOpen
  → false: OpenPhysicsDoor
  → true: ClosePhysicsDoor
```

这只属于新 EG 图表内部逻辑组织，不等于自动接入已有 BeginPlay / Tick / InputAction。

---

## 4.5 调用已有函数或 Custom Event

Append 创建的逻辑允许调用图表外已有函数或已有 Custom Event，但必须验证：

```text
目标存在
签名/参数匹配
Pin 可连接
```

如果目标不存在：

```text
直接 error
modified=false
不自动创建缺失函数
不自动创建缺失事件
不继续写入未连接节点
```

Append 只允许“调用”已有函数/事件，不允许修改它们的实现。

---

## 4.6 事务式写入

AppendBlueprintGraph 必须事务式写入。

如果写入过程中出现：

```text
部分节点创建成功但后续连线失败
UE/K2/Schema 检查失败
目标函数不存在
Pin 类型不匹配
Custom Event 重名
```

应整体回滚，不留下半成品节点或 broken block。

失败返回必须包含：

```text
error_code
message
failed_stage
failed_node / failed_link
conflicts
modified=false
rollback_result
recommended_next_actions
```

失败详细信息不受 verbose 控制。

---

## 4.7 dry_run

Append 必须支持 dry_run。

dry_run 不得修改蓝图。

dry_run 支持：

```text
quick
full
```

quick dry_run：

```text
参数校验
权限检查
目标资产/图表检查
图表风险判断
命名冲突检查
全局事件禁用检查
目标函数/事件存在性检查
```

full dry_run：

```text
包含 quick 全部检查
尽可能模拟节点创建
模拟 Pin 连接
Schema / K2 合法性检查
孤立执行流检查
不落盘、不修改蓝图
```

dry_run 模式由：

```text
Setup 默认策略
Agent 显式参数
工具风险判断
```

共同决定。

工具可因高风险自动将 quick 升级为 full。

---

## 4.8 成功返回

Append 正式写入成功后，Agent-facing 返回采用极简口径，只保留后续操作必需的 handle：

```text
ok
status
modified
target.asset_path
target.graph
data.append_result.graph.graph_id
data.append_result.graph.graph_name
data.append_result.block_refs[]
data.write_ref.transaction_id
data.write_ref.journal_recorded
validation.should_compile
validation.should_save
validation.compiled
validation.saved
```

`block_refs` 是 string 数组，不返回 block 对象快照。

完整 block_id 反推规则：

```text
full_block_id = graph_id + "_" + block_ref
```

`transaction_id` 可返回给 Agent，因为 Graph Write 产生的 block / rollback / review 常需要后续引用；但这不是所有普通写工具的通用返回要求。

Append 成功不默认返回：

```text
summary
created_blocks / created_nodes / created_links / created_variables 计数
called_existing_functions / called_existing_events 计数
blocks[].entry_type
blocks[].entry_name
ownership
review
safety
diagnostics
next
```

上述计数与完整 diff 进入 Transaction Journal / Review / verbose/debug。

---

# 5. ReplaceBlueprintGraph

## 5.1 最小职责

ReplaceBlueprintGraph 的最小职责：

```text
替换一个明确目标的完整实现。
```

目标可以是：

```text
block_id
function
custom_event
event
graph
```

Replace 不允许：

```text
模糊匹配同名逻辑并自动替换
目标不明确时继续写入
自动猜用户想替换哪个实现
```

---

## 5.2 用户手写目标

Replace 替换用户手写函数/图表时，默认允许的前提是：

```text
用户明确指定目标函数或目标图表。
```

目标不明确、用户未授权修改已有用户节点、或会影响目标范围外用户节点时，应停止并报告。

Claude Code Conservative 默认策略：

```text
review_only
```

即：

```text
不生成 block_id
不接管 BlueprintHelper ownership
记录 transaction_id
记录 before / after diff
进入 Review
支持审阅、回滚、审计
```

如果用户明确要求“以后交给 BlueprintHelper 管理”，可以生成 block_id 并接管 ownership，但必须 dry_run 明确提示 ownership 将改变。

---

## 5.3 dry_run / replace plan

Replace 替换任何已有目标前都必须 dry_run。

replace plan 应明确：

```text
will_delete_nodes
will_delete_links
will_create_nodes
will_create_links
will_preserve_nodes
will_modify_nodes
will_reuse_nodes
affected_user_nodes
affected_blueprinthelper_blocks
external_dependents
external_dependencies
target_scope
can_execute
recommended_next_actions
```

---

## 5.4 内部实现可最小 diff

Replace 对外语义是替换目标完整实现。

插件内部不强制必须整体删除旧节点再重建，可以做：

```text
最小 diff
原地更新
节点复用
布局保持
```

但 dry_run / replace plan 必须明确哪些节点会：

```text
删除
保留
复用
修改
新建
```

---

## 5.5 block_id 处理

替换 BlueprintHelper-owned block 时：

```text
保留原 block_id。
```

因为 Replace 表示同一逻辑块的新版本，不是创建新逻辑块。

变化通过：

```text
新的 transaction_id
新的 operation_id
Transaction Journal 中的新版本记录
before / after diff
```

表达。

替换用户手写目标时，是否生成 block_id / 是否接管 ownership，由用户 / Setup 决定。

---

## 5.6 replace_scope

Replace 必须显式区分：

```text
block_implementation
function_body
event_body
function_definition
event_definition
graph
```

含义：

```text
block_implementation：
替换 BlueprintHelper-owned block 的实现，保留原 block_id。

function_body：
保留函数入口、签名、外部可调用身份，只替换内部节点/连线。

event_body：
保留事件入口、名称、参数、外部可调用身份，只替换内部逻辑。

function_definition：
替换或重建函数本体，可能改变 UUID、签名、入口或外部引用。

event_definition：
替换或重建事件本体，可能影响外部调用方。

graph：
替换明确指定图表范围内的完整实现，高风险。
```

---

## 5.7 外部依赖规则

如果 Replace 的目标是函数或事件本体：

```text
遇到 external_dependents 默认阻止并报告。
```

原因是替换后蓝图内部 UUID、节点引用或其他数据可能变化，外部调用方可能失效，编译或运行时报错。

如果 Replace 的目标是函数或事件内部逻辑实现：

```text
不因 external_dependents 直接阻止。
dry_run 必须报告 external_dependents。
若签名、入口身份、调用 Pin 或外部引用会变化，则阻止。
```

---

## 5.8 Conservative 自动执行策略

Claude Code Conservative 下可自动执行：

```text
block_implementation
function_body
event_body
```

前提：

```text
用户明确指定目标
dry_run 无 error / conflict
不改变入口身份
不改变签名
不破坏外部调用方
```

不可自动执行：

```text
function_definition
event_definition
graph
```

---

## 5.9 function_body / event_body

替换 function_body / event_body 时，允许删除并重建内部普通节点和连线，但必须保留：

```text
函数入口 / 事件入口本体
函数签名
参数 Pin
返回值 Pin
外部可调用身份
外部调用引用稳定性
```

所有被删除、替换、修改、断开连接的用户手写内部节点和连线，必须进入 Review diff。

如果目标内部混有 BlueprintHelper-owned 节点和用户手写节点，允许替换整个 body，但 Review diff 必须区分 owned 节点和用户节点。

内部旧 block_id 按复用关系处理：

```text
一对一复用 / 原地更新：保留原 block_id
删除 / 重建 / 无法稳定对应：废弃旧 block_id，新逻辑块生成新 block_id
```

---

## 5.10 成功返回

Replace 正式写入成功后，Agent-facing 返回采用极简口径：

```text
ok
status
modified
target.asset_path
target.graph
target.replace_scope
data.replace_result.target.graph_id
data.replace_result.target.target_ref
data.replace_result.target.target_kind
data.write_ref.transaction_id
data.write_ref.journal_recorded
validation.should_compile
validation.should_save
```

Replace 成功不默认返回：

```text
summary
deleted_nodes / created_nodes / modified_nodes / preserved_nodes 计数
before / after
full_diff
ownership
review
safety
diagnostics
next
```

替换 BlueprintHelper-owned block 时保留原 block_id / block_ref。替换用户手写目标时默认不接管 ownership，不生成 block_ref。

---

# 6. PatchBlueprintGraph

## 6.1 最小职责

PatchBlueprintGraph 的最小职责：

```text
精确修改一个明确目标点。
```

Patch 必须定位到：

```text
具体节点
具体 Pin
具体默认值
具体属性
具体连接
```

不允许根据自然语言描述模糊查找并直接修改。

目标无法唯一定位时，返回 error，并建议先读取 LogicJson / LogicMD 或使用 Replace / Merge。

---

## 6.2 expected_old_value

Patch 不强制所有场景都携带 expected_old_value / expected_old_state。

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

即使省略 expected_old_value，工具仍应：

```text
执行前读取当前状态
Journal / Review 中记录 before / after
默认 Agent-facing 成功结果不返回 before_summary / after_summary
verbose/debug 可返回完整 before/after diff
```

---

## 6.3 目标定位

Patch 目标定位优先使用：

```text
BlueprintHelper block_id + LogicJson 局部路径
```

不足时再使用：

```text
UE 原生 node GUID / pin GUID
```

节点显示名 / Pin 名不能单独作为稳定定位依据。

Patch 的 node_path / pin_path / link_path 应与 LogicJson schema 保持一致。

当前 LogicJson 路径规则：

```text
target_graph / blueprint / multi_target 使用 logic.groups[]，不是单入口 entry + nodes。
target_block / target_function / target_event / target_custom_event / target_node / target_pin 可使用 entry + nodes 简写。
普通节点默认返回 node_ref，不默认返回完整 node_path。
连接默认返回 link_ref，不默认返回完整 link_path。
node_ref / link_ref 只在当前 group 内有效。
如果工具需要完整 node_path / link_path，Agent 应从 group.entry.node_path 反推。
links 存在 source node 的 node.links 内，表示 outgoing links；LogicJson 不返回顶层 logic.links。
```


---

## 6.4 成功返回

Patch 正式写入成功后，Agent-facing 返回采用极简口径：

```text
ok
status
modified
target.asset_path
target.graph
target.patch_scope
data.patch_result.target
data.patch_result.patch.patch_type
data.patch_result.patch.expected_old_state_provided
data.patch_result.patch.changed
data.write_ref.transaction_id
data.write_ref.journal_recorded
validation.should_compile
validation.should_save
```

Patch 成功不默认返回：

```text
summary
modified_nodes / modified_pins / created_links / deleted_links 计数
before / after
old_value / new_value
patch_plan
full_diff
ownership
review
safety
diagnostics
next
```

Patch 目标必须使用明确 `node_path / pin_path / link_path`，或使用可从 LogicJson group 反推完整路径的局部引用。仅靠显示名 / Pin 名不允许直接修改。

---

# 7. MergeBlueprintGraph

## 7.1 最小职责

MergeBlueprintGraph 的最小职责：

```text
把新逻辑接入已有执行流。
```

Merge 专门负责将：

```text
BlueprintHelper 逻辑块
函数调用
Custom Event 调用
```

接入明确指定的已有执行链，例如：

```text
BeginPlay
DoInteract Override
InputAction
Overlap
已有函数入口
已有 Branch 分支
```

Merge 不负责 Append / Replace / Patch。

---

## 7.2 接入点要求

Merge 必须要求明确接入点，否则 error。

调用方必须提供：

```text
目标图表
目标节点或稳定定位路径
目标 Pin
插入策略
```

接入点定位优先：

```text
LogicJson node_path / pin_path
```

如果读取的是 target_graph / blueprint / multi_target 范围，Agent 必须先选择明确 group，再使用该 group 内的 node_ref / link_ref 反推完整路径；不得跨 group 使用局部引用。

不足时使用：

```text
UE node GUID / pin GUID
```

节点显示名 + Pin 名只能作为辅助显示信息。

---

## 7.3 insert_strategy

插入已有 Exec 链时，必须由 Agent 显式指定 insert_strategy。

至少包括：

```text
append_after
insert_between
branch_fork
```

不同策略会改变执行顺序和副作用，不能默认猜。

---

## 7.4 append_after

如果 target Exec Pin 已有后继连接：

```text
直接 error。
```

不自动改成 insert_between 或 branch_fork。

---

## 7.5 insert_between

insert_between 允许：

```text
断开目标 Exec Pin 的既有连接
插入新逻辑
重接原后继
```

前提：

```text
明确 target_graph
明确 target_node / node_path
明确 target_pin / pin_path
明确 insert_strategy=insert_between
必须 dry_run
```

dry_run / merge plan 必须展示：

```text
将断开的旧连接
将新增的节点或调用
将新建的连接
原后继如何重接
执行顺序变化
是否涉及用户节点
```

---

## 7.6 branch_fork

branch_fork 使用时，工具应自动插入：

```text
Sequence 节点或等价分发节点
```

不能假设 Exec Pin 支持多后继。

Sequence 的顺序必须由 Agent 显式指定：

```text
原后继接 Then0 还是 Then1
新逻辑接 Then0 还是 Then1
```

dry_run / merge plan 必须展示：

```text
将插入的 Sequence 节点
原连接迁移位置
新逻辑连接位置
sequence_order / branch_order
执行顺序变化
是否影响用户节点
```

---

# 8. Ownership / Cleanup 工具簇

## 8.1 工具清单

Cleanup / Ownership 工具簇初版：

```text
CleanupBlueprintHelperBlock
CleanupBlueprintHelperFeature
RollbackCleanupTransaction
ConvertBlueprintHelperBlockToUserOwned
```

dry_run 作为参数，不单独拆工具。

---

## 8.2 CleanupBlueprintHelperBlock

只接受明确 block_id。

不接受：

```text
模糊名称
entry_name
graph / event_name 多字段定位
```

如果 block_id 不存在，由参数控制：

```text
missing_policy = error | ignore
```

默认：

```text
missing_policy=error
```

恢复、批处理、重复清理场景可用：

```text
missing_policy=ignore
```

但 ignore 只处理 block 不存在，不掩盖 ownership 冲突、依赖冲突、用户节点冲突或 rollback 状态不匹配。

---

## 8.3 CleanupBlueprintHelperFeature

支持按功能组清理，但必须 dry_run。

多字段联合匹配：

```text
feature_name
feature_group
graph 名
block_id 前缀
Transaction Journal 记录
target_asset
入口事件名 / 函数名
```

匹配置信度：

```text
high
medium
low
```

默认正式 cleanup 只自动删除 high 置信度匹配。

medium / low 需要用户明确确认。

dry_run 必须返回默认执行计划：

```text
will_delete
will_keep
requires_confirmation
blocked_by
external_dependencies
external_dependents
confidence 分组
would_delete_nodes
would_delete_links
can_execute
recommended_next_actions
```

---

## 8.4 Cleanup 依赖方向

必须区分：

```text
external_dependents：
外部谁正在依赖目标 block / 功能组。
删除目标可能破坏外部调用方。

external_dependencies：
目标 block / 功能组正在依赖哪些外部 block。
删除目标不会破坏这些外部 block。
```

策略：

```text
外部依赖目标：需要用户确认或停止报告。
目标依赖外部：Standard / AutoRepair 可删除目标并保留外部依赖。
```

任何 cleanup 只能删除 BlueprintHelper-owned block，不能删除用户手写节点或来源不明节点。

---

## 8.5 Cleanup 执行与 Review

Cleanup 正式执行后必须：

```text
生成 transaction_id
写入 Transaction Journal
记录 rollback_data
进入 Review 队列
```

即使只删除 BlueprintHelper-owned 节点，也属于破坏性写操作。

Cleanup Review 被拒绝时：

```text
不在 Agent 执行闭环中自动 rollback。
只标记 rejected / needs_action，或由 Review UI 的显式 Reject / rollback 流程处理。
Agent 不等待用户 Review Accept / Reject，也不把 Review 状态作为本轮任务完成条件。
```

---

## 8.6 ConvertBlueprintHelperBlockToUserOwned

支持：

```text
单 block 转换
feature 批量转换
```

feature 批量必须 dry_run。

执行后：

```text
移除或改写 BlueprintHelper ownership metadata
清理或转换 NodeComment
Journal 处理方式由 Setup 配置
进入 Review 队列
```

Review 通过前可以 rollback。

Review 通过并归档后，默认不再自动恢复 ownership。

暂不规划重新接管用户节点工具。

---

# 9. Transaction Journal / Review 数据设计

## 9.1 节点 Metadata

节点 Metadata 只保存最小 ownership 索引：

```json
{
  "BlueprintHelperOwned": true,
  "BlueprintHelperBlockId": "EG_PhysicsDoor_TogglePhysicsDoor0",
  "BlueprintHelperTransactionId": "tx_20260501_0007",
  "BlueprintHelperTool": "AppendBlueprintGraph",
  "BlueprintHelperFeatureName": "TogglePhysicsDoor"
}
```

完整依赖、diff、diagnostics 不应全部写入节点 metadata。

---

## 9.2 NodeComment

NodeComment 用于人类审查：

```text
[BlueprintHelper]
block_id=EG_PhysicsDoor_TogglePhysicsDoor0
tx=tx_20260501_0007
tool=AppendBlueprintGraph
```

工具判断 ownership 时以 Metadata 为准，NodeComment 只作辅助。

---

## 9.3 Transaction Journal

完整写入记录放在：

```text
<Project>/Saved/BlueprintHelper/Transactions/
```

Transaction Journal 记录：

```text
transaction_id
tool
status
target_assets
operations
blocks
created_nodes
created_links
references_block_ids
references_events
diagnostics
validation
rollback_data
review_status
```

审查通过后是否压缩由 Setup 决定：

```text
KeepFull
CompactToSummary
DeleteJournalKeepMetadata
```

默认保留节点 Metadata / NodeComment，除非用户明确 ConvertToUserOwned。

---

# 10. Read / Logic 工具规划同步

## 10.1 LogicMD 默认阅读规则

LogicMD 是 Agent 默认蓝图逻辑阅读格式，用于快速理解蓝图做了什么、有哪些入口、哪些逻辑属于 BlueprintHelper-owned block、哪些属于用户区域。

多入口 scope：

```text
target_graph
blueprint
multi_target
```

这些 scope 必须按 group 分段阅读，而不是视为一条连续执行流。

多入口 LogicMD 返回：

```json
{
  "grouped": true
}
```

`grouped=true` 表示 Markdown 已按以下区域分段：

```text
BlueprintHelper Block
User Region
Global Event Flow
Orphan Group
Unknown Group
```

Agent 不得把不同 group 自动连接成同一条执行链。

单入口 scope 通常不返回 grouped 字段。字段缺失不表示 `grouped=false`，只表示当前 scope 不需要分组标记。

## 10.2 LogicJson 精确分析规则

LogicJson 是 Patch / Merge / Replace / Cleanup 前的结构化分析格式。

多入口 scope：

```text
target_graph
blueprint
multi_target
```

使用：

```text
logic.groups[]
```

单入口 scope：

```text
target_block
target_function
target_event
target_custom_event
target_node
target_pin
```

可以使用：

```text
logic.entry + logic.nodes
```

每个 group 必须有 entry，且 `entry.node_path` 是当前 group 的完整节点路径和路径反推锚点。

普通 node 默认只返回：

```text
node_ref
```

普通 link 默认只返回：

```text
link_ref
```

规则：

```text
node_ref / link_ref 只在当前 group 内有效，不是全局路径。
links 存在 source node 的 node.links 内，表示 outgoing links。
link 内不写 from_node，因为 source node 就是当前 node。
to_node 是目标 node_ref，也只在当前 group 内有效。
第一版不提供 incoming_refs，需要反查其他 node 的 outgoing links。
LogicJson 默认返回语义 kind，不默认返回 UE K2Node 原始类名。
LogicJson 必须 importable=false，不可作为导入格式。
```

## 10.3 精确读工具规划

新增按目标读取逻辑工具：

```text
ReadBlueprintLogicJsonByTarget
ReadBlueprintLogicMdByTarget
```

输入：

```text
asset_path / target_blueprint
target_type: function / event / custom_event / graph / block
target_name 或 block_id
format: logic_json / logic_md
```

推荐读工具分层：

```text
Global LogicMD：快速搜索和全局理解
Target LogicMD：理解单个目标
Target LogicJson：精确修改前分析
```

---

# 11. 后续优先级

当前测试后续修复优先级：

```text
E. 图表写入
F. 验证链路
G. 引导链路
H. 通信恢复
D. Override
C. 接口
B. 输入链路
A. 组件添加
```

当前文档主要完成：

```text
E. 图表写入
Cleanup / Ownership
部分 Transaction Journal / Review 数据设计
```

后续建议继续补齐或引用已拆分文档：

```text
Validation / Diagnostics 工具簇：遵循 data.markdown diagnostics 规则。
Preflight / Setup 工具簇：以 runtime_profile + active_profile 为写入阶段事实来源。
Interface / Override 工具簇：Class Settings 只添加 Implemented Interface；接口函数实现体仍交给 Graph Write。
Enhanced Input 工具簇：当前阶段默认不编辑 IA / IMC，只引用用户已有 IA 并通过 Graph Write 创建事件入口。
Component 工具簇：add_component 只创建组件和 attachment；transform / collision / physics / mesh / material 均走 property 写入。
```
---

# 2026-05-04 混合 TaskSpec / TaskPlan 架构同步

## 同步结论

Graph Write 工具簇不推翻。Append / Replace / Patch / Merge 的边界继续作为底层能力边界，但普通 Agent 不再直接手动选择和拼装所有 Graph Write 工具。

新口径：

```text
Agent-facing：TaskSpec 描述目标、范围、行为和集成策略。
Python / MCP：TaskSpec → TaskPlan，决定使用 Append / Replace / Patch / Merge。
UE Task Runtime：执行 TaskPlan 中的 Graph Write step。
Graph Write 能力簇：保持原语义边界和字段协议。
```

## 新增 TaskSpec / TaskPlan 关系

TaskSpec 是 Agent-facing 语义规格，例如：

```json
{
  "task_type": "create_blueprint_feature",
  "feature_name": "PhysicsDoor",
  "target": { "asset_path": "/Game/BP/BP_Door" },
  "scope_policy": {
    "prefer_new_graph": true,
    "graph_name": "EG_PhysicsDoor",
    "allow_modify_user_nodes": false,
    "allow_merge_existing_execution_flow": false
  },
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": []
  }
}
```

TaskPlan 是 Task Compiler 输出给 UE Task Runtime 的执行计划，例如：

```json
{
  "schema": "BlueprintHelper.TaskPlan.v1",
  "steps": [
    {
      "step_id": "step_graph_001",
      "capability": "graph_write",
      "target": {
        "asset_path": "/Game/BP/BP_Door",
        "graph": "EG_PhysicsDoor"
      },
      "write": {
        "strategy": "owned_graph_edit",
        "ops": [
          {
            "op": "ensure_entry",
            "entry": {
              "kind": "custom_event",
              "name": "OnSmokeTest",
              "statements": []
            }
          }
        ]
      }
    }
  ]
}
```

`append_blueprint_graph` 是 UE Task Runtime lowering 到现有 Bridge capability cluster 时的 adapter operation，不是 Agent 或 Task Compiler 写入 TaskPlan step 的字段。

## Agent-facing 调整

Agent 不应再输出：

```text
read_logic_json → append_blueprint_graph → merge_blueprint_graph → compile → save
```

Agent 应输出：

```text
TaskSpec → preview_task → execute_task
```

Graph Write 文档仍必须保留，因为 Task Compiler / Task Runtime 必须遵守：

```text
1. Append 只能追加独立逻辑块。
2. Replace 只能替换明确目标的完整实现。
3. Patch 必须精确定位 node / pin / link / default value。
4. Merge 才能接入已有执行流。
5. 接入已有执行流必须 dry_run。
6. rollback blocked / failed 后不得继续 compile/save/patch。
```

## 新增 task_run_id 口径

本文件原有 transaction_id 口径保持不变：

```text
一个 transaction_id 对应一次真实写工具调用。
```

新增：

```text
一个 task_run_id 对应一次 TaskSpec / TaskPlan 执行。
一个 task_run_id 下可以包含多个 child transaction_id。
```

Graph Write step 仍生成自己的 transaction_id；整个任务由 TaskRunJournal 关联。

## 成功返回层级调整

底层 Graph Write 成功返回仍保持极简字段，例如 write_ref / validation。

但 Agent-facing execute_task 默认只返回：

```text
task_run_id
feature_name
target_assets
applied_steps
created_assets / modified_assets 摘要
validation.compiled / should_save / saved
```

不默认返回 child transaction_ids，除非失败、debug、rollback 或用户明确要求。
