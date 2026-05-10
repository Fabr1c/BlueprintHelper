# BlueprintHelper 第 3 簇：Logic Read 分组结构 UE 字段映射与实现计划

日期：2026-05-02  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：LogicMD / LogicJson 分组结构确认稿  
本文边界：确认 LogicMD 与 LogicJson 在 `target_graph / blueprint / multi_target` 等多入口 scope 下的 UE 侧包装、字段映射、group 分组、node_ref / link_ref 反推规则，以及 stats scope 收敛规则。Agent 侧使用规则见独立文档。

---

## 1. 设计定位

Logic Read 簇包括：

```text
1. LogicMD：低 Token 文本逻辑摘要，用于 Agent 快速理解蓝图逻辑。
2. LogicJson：结构化逻辑视图，用于 Patch / Merge / Replace / Cleanup 前的精确分析和定位。
3. RawJson / RawJsonRef：完整保真格式，用于导入导出、兼容性调试、Pin 级底层排查。
```

本文只确认：

```text
LogicMD
LogicJson
```

不确认：

```text
RawJson / RawJsonRef 完整字段
Graph Write data 字段
Transaction Journal
Review Store
Diff / Rollback 结构
```

---

## 2. 已确认的关键决策

| 项 | 决策 |
|---|---|
| 返回外壳 | LogicMD / LogicJson 均使用第 0 簇 `ToolResultBase`。 |
| 工具类型 | 只读工具。 |
| transaction | 不生成。 |
| review | 不进入。 |
| safety | 不返回。 |
| validation | 不返回。 |
| diagnostics | 不返回顶层 diagnostics。 |
| data.schema | 使用短名，例如 `LogicMd.v1`、`LogicJson.v1`。 |
| detail | 删除。LogicMD / LogicJson / RawJson 已经代表不同细节层级。 |
| stats | 根据 scope 收敛，不适用字段省略，不用 0 填充。 |
| target_graph | 不是单入口 scope，必须由 UE 侧预先分组。 |
| LogicJson links | 顶层 `logic.links` 删除，links 移入 source node 内部。 |
| LogicJson node path | entry 保留完整 `node_path`，普通 node 默认只返回 `node_ref`。 |
| LogicJson link path | link 默认只返回 `link_ref`，完整 path 由 group entry 反推。 |
| LogicMD grouped | 多入口 scope 下增加 `grouped: true`，Markdown 按 group 分段。 |
| UE 侧包装 | 新增 `LogicGroupBuilder` 或等价模块，供 LogicMD / LogicJson 共用。 |

---

## 3. data.schema 全局短名规则

顶层 schema 表示 MCP Tool Result 基础协议：

```json
{
  "schema": "BlueprintHelper.McpToolResult.v1"
}
```

`data.schema` 只表示当前 payload 的局部格式版本，因此统一使用短名：

| Payload | data.schema |
|---|---|
| Runtime Profile | `RuntimeProfile.v1` |
| Diagnostics | `Diagnostics.v1` |
| LogicMD | `LogicMd.v1` |
| LogicJson | `LogicJson.v1` |
| RawJsonRef | `RawJsonRef.v1` |
| RawJson | `RawJson.v1` |
| AppendBlueprintGraph | `AppendBlueprintGraph.v1` |
| PatchBlueprintGraph | `PatchBlueprintGraph.v1` |
| MergeBlueprintGraph | `MergeBlueprintGraph.v1` |
| CleanupBlueprintHelperBlock | `CleanupBlueprintHelperBlock.v1` |

约束：

```text
1. 顶层 schema 保留完整命名空间。
2. data.schema 统一使用短名。
3. data.schema 不重复 BlueprintHelper 前缀。
```

---

# 4. Scope 语义修正

## 4.1 单入口 scope

以下 scope 可以使用单入口简写结构：

```text
target_block
target_function
target_event
target_custom_event
target_node
target_pin
```

LogicJson 可使用：

```json
{
  "logic": {
    "entry": {},
    "nodes": []
  }
}
```

LogicMD 可直接输出目标逻辑文本。

---

## 4.2 多入口 scope

以下 scope 必须分组：

```text
target_graph
blueprint
multi_target
```

原因：

```text
1. target_graph 本质上是遍历当前 graph 下的多个 block / entry / user region。
2. blueprint 本质上会遍历多个 graph / function / event。
3. multi_target 本质上会遍历多个目标。
4. 这些 scope 不能用单个 entry.node_path 表达。
```

多入口 scope 下：

```text
LogicJson 使用 logic.groups[]。
LogicMD 按 group 分段，并返回 grouped=true。
```

---

# 5. UE 侧分组规则

建议新增：

```cpp
class FBlueprintHelperLogicGroupBuilder
```

职责：

```text
1. 扫描目标 graph / blueprint / multi_target。
2. 根据 BlueprintHelper ownership metadata 识别 BlueprintHelper-owned block。
3. 根据入口节点识别 global event flow。
4. 根据连通性识别 user region。
5. 根据未接入执行流的节点识别 orphan_group。
6. 为每个 group 生成独立 entry。
7. 为每个 group 内节点生成局部 node_ref。
8. 为每个 group 内连接生成局部 link_ref。
9. 输出给 LogicMD / LogicJson 共用。
```

---

## 5.1 group_type 枚举

### UE 枚举建议

```cpp
enum class EBlueprintHelperLogicGroupType
{
    BlueprintHelperBlock,
    UserRegion,
    GlobalEventFlow,
    FunctionLikeRegion,
    OrphanGroup,
    Unknown
};
```

### MCP 返回值

| UE 枚举 | JSON 值 | 含义 |
|---|---|---|
| `BlueprintHelperBlock` | `blueprinthelper_block` | BlueprintHelper-owned block，通常带 block_id。 |
| `UserRegion` | `user_region` | 用户已有节点区域，非 BlueprintHelper-owned。 |
| `GlobalEventFlow` | `global_event_flow` | BeginPlay / Tick / Overlap / InputAction 等全局事件流。 |
| `FunctionLikeRegion` | `function_like_region` | 图表中可识别为独立调用链的区域。 |
| `OrphanGroup` | `orphan_group` | 无入口或未接入执行流的孤立节点组。 |
| `Unknown` | `unknown` | 无法稳定分类。 |

---

# 6. LogicJson 分组结构

## 6.1 target_graph 示例

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_blueprint_logic_json_by_target",
  "trace_id": "trace_20260502_0401",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "graph",
    "graph": "EG_PhysicsDoor"
  },

  "data": {
    "schema": "LogicJson.v1",
    "format": "logic_json",
    "importable": false,
    "scope": "target_graph",

    "logic": {
      "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
      "graph": "EG_PhysicsDoor",

      "groups": [
        {
          "group_type": "blueprinthelper_block",
          "block_id": "EG_PhysicsDoor_TogglePhysicsDoor0",
          "entry": {
            "kind": "custom_event",
            "name": "TogglePhysicsDoor",
            "node_path": "$.graphs[EG_PhysicsDoor].blocks[EG_PhysicsDoor_TogglePhysicsDoor0].nodes[TogglePhysicsDoor]",
            "node_ref": "nodes[TogglePhysicsDoor]"
          },
          "nodes": [
            {
              "node_ref": "nodes[TogglePhysicsDoor]",
              "kind": "custom_event",
              "name": "TogglePhysicsDoor",
              "links": [
                {
                  "link_ref": "links[0]",
                  "type": "exec",
                  "from_pin": "Then",
                  "to_node": "nodes[Branch0]",
                  "to_pin": "Execute"
                }
              ]
            },
            {
              "node_ref": "nodes[Branch0]",
              "kind": "branch",
              "name": "Branch",
              "inputs": {
                "Condition": "bDoorOpen"
              },
              "outputs": {},
              "links": []
            }
          ]
        }
      ]
    },

    "stats": {
      "groups": 1,
      "events": 1,
      "nodes": 2,
      "exec_links": 1,
      "data_links": 0,
      "orphan_nodes": 0
    }
  }
}
```

---

## 6.2 target_block 示例

单入口 scope 可使用简写：

```json
{
  "data": {
    "schema": "LogicJson.v1",
    "format": "logic_json",
    "importable": false,
    "scope": "target_block",

    "logic": {
      "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
      "graph": "EG_PhysicsDoor",
      "block_id": "EG_PhysicsDoor_OpenPhysicsDoor0",

      "entry": {
        "kind": "custom_event",
        "name": "OpenPhysicsDoor",
        "node_path": "$.graphs[EG_PhysicsDoor].blocks[EG_PhysicsDoor_OpenPhysicsDoor0].nodes[OpenPhysicsDoor]",
        "node_ref": "nodes[OpenPhysicsDoor]"
      },

      "nodes": [
        {
          "node_ref": "nodes[OpenPhysicsDoor]",
          "kind": "custom_event",
          "name": "OpenPhysicsDoor",
          "links": [
            {
              "link_ref": "links[0]",
              "type": "exec",
              "from_pin": "Then",
              "to_node": "nodes[SetSimulatePhysics0]",
              "to_pin": "Execute"
            }
          ]
        },
        {
          "node_ref": "nodes[SetSimulatePhysics0]",
          "kind": "call_function",
          "name": "SetSimulatePhysics",
          "owner": "DoorMesh",
          "inputs": {
            "NewSimulate": true
          },
          "outputs": {},
          "links": []
        }
      ]
    },

    "stats": {
      "nodes": 2,
      "exec_links": 1,
      "data_links": 0,
      "orphan_nodes": 0
    }
  }
}
```

---

# 7. LogicJson 字段映射

## 7.1 FBlueprintHelperLogicJsonData

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Schema` | `FString` | `data.schema` | `string` | 是 | 固定为 `LogicJson.v1`。 |
| `Format` | `EBlueprintHelperLogicFormat` | `data.format` | `string enum` | 是 | 固定为 `logic_json`。 |
| `bImportable` | `bool` | `data.importable` | `boolean` | 是 | 固定为 `false`。 |
| `Scope` | `EBlueprintHelperLogicScope` | `data.scope` | `string enum` | 是 | 当前 LogicJson 覆盖范围。 |
| `Logic` | `FBlueprintHelperLogicJsonPayload` | `data.logic` | `object` | 是 | 结构化逻辑内容。 |
| `Stats` | `FBlueprintHelperLogicStats` | `data.stats` | `object` | 是 | 随 scope 收敛。 |
| `Detail` | 不使用 | 不返回 | - | 否 | 已删除。 |

---

## 7.2 FBlueprintHelperLogicJsonPayload

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 出现条件 | 说明 |
|---|---|---|---|---|---|
| `AssetPath` | `FString` | `data.logic.asset_path` | `string` | 始终 | 局部上下文。 |
| `Graph` | `FString` | `data.logic.graph` | `string` | 图表相关 | 当前 graph。 |
| `Function` | `FString` | `data.logic.function` | `string` | 函数相关 | 当前 function。 |
| `Event` | `FString` | `data.logic.event` | `string` | 事件相关 | 当前 event。 |
| `BlockId` | `FString` | `data.logic.block_id` | `string` | block 相关 | 当前 block_id。 |
| `Entry` | `FBlueprintHelperLogicEntry` | `data.logic.entry` | `object` | 单入口 scope | 入口。 |
| `Nodes` | `TArray<FBlueprintHelperLogicNode>` | `data.logic.nodes` | `array<object>` | 单入口 scope | 当前 scope 内节点。 |
| `Groups` | `TArray<FBlueprintHelperLogicGroup>` | `data.logic.groups` | `array<object>` | 多入口 scope | graph / blueprint / multi_target 分组。 |

规则：

```text
单入口 scope 使用 entry + nodes。
多入口 scope 使用 groups[]。
不在同一个 payload 中同时返回 entry/nodes 与 groups，除非后续明确设计兼容模式。
```

---

## 7.3 FBlueprintHelperLogicGroup

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `GroupType` | `EBlueprintHelperLogicGroupType` | `group.group_type` | `string enum` | 是 | 分组类型。 |
| `BlockId` | `FString` | `group.block_id` | `string` | block group 时 | BlueprintHelper-owned block id。 |
| `Name` | `FString` | `group.name` | `string` | user region / unknown 时 | 分组名。 |
| `Entry` | `FBlueprintHelperLogicEntry` | `group.entry` | `object` | 是 | 分组入口。 |
| `Nodes` | `TArray<FBlueprintHelperLogicNode>` | `group.nodes` | `array<object>` | 是 | 分组内节点。 |

---

## 7.4 FBlueprintHelperLogicEntry

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Kind` | `EBlueprintHelperLogicNodeKind` | `entry.kind` | `string enum` | 是 | 入口节点语义类型。 |
| `Name` | `FString` | `entry.name` | `string` | 是 | 入口名。 |
| `NodePath` | `FString` | `entry.node_path` | `string` | 是 | 完整锚点路径。 |
| `NodeRef` | `FString` | `entry.node_ref` | `string` | 是 | 当前 group 内局部 node 引用。 |

规则：

```text
entry.node_path 必须是完整节点路径，不允许是 $.graphs[GraphName] 这种非节点路径。
entry.node_path 是当前 group 内 node_ref / link_ref 的反推锚点。
```

---

## 7.5 FBlueprintHelperLogicNode

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `NodeRef` | `FString` | `node.node_ref` | `string` | 是 | 当前 group 内唯一局部引用，例如 `nodes[SetSimulatePhysics0]`。 |
| `Kind` | `EBlueprintHelperLogicNodeKind` | `node.kind` | `string enum` | 是 | 语义节点类型。 |
| `Name` | `FString` | `node.name` | `string` | 是 | 人类可读名。 |
| `Owner` | `FString` | `node.owner` | `string` | 可选 | 组件或对象 owner，例如 `DoorMesh`。 |
| `Inputs` | `TSharedPtr<FJsonObject>` | `node.inputs` | `object` | 可选 | 语义输入值。 |
| `Outputs` | `TSharedPtr<FJsonObject>` | `node.outputs` | `object` | 可选 | 语义输出摘要。 |
| `Links` | `TArray<FBlueprintHelperLogicLink>` | `node.links` | `array<object>` | 是 | 从该 node 发出的 outgoing links。 |
| `NodePath` | 不默认返回 | 不返回 | - | 否 | 普通 node 默认不返回完整 node_path。 |

规则：

```text
1. node_ref 必须在当前 group 内唯一。
2. node.links 表达 outgoing links。
3. node 内不写完整 node_path，除非未来 explicit debug / pin_detail 模式。
```

---

## 7.6 FBlueprintHelperLogicLink

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `LinkRef` | `FString` | `link.link_ref` | `string` | 是 | 当前 group 内唯一局部 link 引用，例如 `links[0]`。 |
| `Type` | `EBlueprintHelperLogicLinkType` | `link.type` | `string enum` | 是 | `exec` 或 `data`。 |
| `FromPin` | `FString` | `link.from_pin` | `string` | 是 | 当前 source node 的输出 pin。 |
| `ToNode` | `FString` | `link.to_node` | `string` | 是 | 目标 node_ref。 |
| `ToPin` | `FString` | `link.to_pin` | `string` | 是 | 目标 pin。 |
| `FromNode` | 不使用 | 不返回 | - | 否 | source node 就是当前 node。 |
| `LinkPath` | 不默认返回 | 不返回 | - | 否 | 默认只返回 link_ref。 |

规则：

```text
1. link 放在 source node 内部。
2. 不返回 from_node。
3. 不返回顶层 logic.links。
4. 完整 link_path 由 group.entry.node_path 和 link_ref 反推。
```

---

# 8. Path 反推规则

## 8.1 node_ref 反推

完整 node_path：

```text
group.entry.node_path 去掉最后一段 nodes[EntryName]
+
node_ref
```

示例：

```text
group.entry.node_path:
$.graphs[EG_PhysicsDoor].blocks[EG_PhysicsDoor_OpenPhysicsDoor0].nodes[OpenPhysicsDoor]

node_ref:
nodes[SetSimulatePhysics0]

反推:
$.graphs[EG_PhysicsDoor].blocks[EG_PhysicsDoor_OpenPhysicsDoor0].nodes[SetSimulatePhysics0]
```

## 8.2 link_ref 反推

完整 link_path：

```text
group.entry.node_path 去掉最后一段 nodes[EntryName]
+
link_ref
```

示例：

```text
group.entry.node_path:
$.graphs[EG_PhysicsDoor].blocks[EG_PhysicsDoor_OpenPhysicsDoor0].nodes[OpenPhysicsDoor]

link_ref:
links[1]

反推:
$.graphs[EG_PhysicsDoor].blocks[EG_PhysicsDoor_OpenPhysicsDoor0].links[1]
```

---

# 9. LogicMD 分组结构

## 9.1 target_graph 示例

```json
{
  "data": {
    "schema": "LogicMd.v1",
    "format": "logic_md",
    "importable": false,
    "scope": "target_graph",
    "grouped": true,
    "markdown": "EG_PhysicsDoor\n\n[BlueprintHelper Block] EG_PhysicsDoor_TogglePhysicsDoor0\nTogglePhysicsDoor() -> Branch(bDoorOpen)\n  false -> OpenPhysicsDoor()\n  true -> ClosePhysicsDoor()\n\n[BlueprintHelper Block] EG_PhysicsDoor_OpenPhysicsDoor0\nOpenPhysicsDoor() -> DoorMesh.SetSimulatePhysics(true) -> DoorMesh.AddImpulse(...)\n\n[User Region] UserExistingBeginPlayFlow\nBeginPlay() -> ExistingInit()",
    "stats": {
      "groups": 3,
      "events": 3,
      "nodes": 21,
      "exec_links": 18,
      "data_links": 6,
      "orphan_nodes": 0
    }
  }
}
```

## 9.2 grouped 字段规则

| scope | grouped |
|---|---|
| `target_graph` | `true` |
| `blueprint` | `true` |
| `multi_target` | `true` |
| `target_block` | 不返回 |
| `target_function` | 不返回 |
| `target_event` | 不返回 |
| `target_custom_event` | 不返回 |

说明：

```text
grouped 只在多入口 scope 出现。
单入口 scope 不返回 grouped，而不是返回 false。
```

---

# 10. Stats scope 收敛规则

| scope | 允许 stats 字段 |
|---|---|
| `blueprint` | `groups`, `graphs`, `functions`, `events`, `nodes`, `exec_links`, `data_links`, `orphan_nodes` |
| `target_graph` | `groups`, `events`, `nodes`, `exec_links`, `data_links`, `orphan_nodes` |
| `target_function` | `nodes`, `exec_links`, `data_links`, `orphan_nodes` |
| `target_event` | `nodes`, `exec_links`, `data_links`, `orphan_nodes` |
| `target_custom_event` | `nodes`, `exec_links`, `data_links`, `orphan_nodes` |
| `target_block` | `nodes`, `exec_links`, `data_links`, `orphan_nodes` |
| `target_node` | `pins`, `input_pins`, `output_pins` |
| `target_pin` | 不返回 stats 或返回空对象 |
| `multi_target` | `groups`, `targets`, `nodes`, `exec_links`, `data_links`, `orphan_nodes` |

规则：

```text
1. 不适用字段直接省略。
2. 不用 0 填充不适用字段。
3. target_graph 不返回 graphs / functions。
4. target_function 不返回 graphs / functions / events。
5. 多入口 scope 返回 groups。
```

---

# 11. Node kind 枚举建议

```cpp
enum class EBlueprintHelperLogicNodeKind
{
    Event,
    CustomEvent,
    CallFunction,
    Branch,
    Sequence,
    VariableGet,
    VariableSet,
    ComponentGet,
    Literal,
    Return,
    Macro,
    DelegateBind,
    DelegateCall,
    Timeline,
    Unknown
};
```

MCP 返回值：

```text
event
custom_event
call_function
branch
sequence
variable_get
variable_set
component_get
literal
return
macro
delegate_bind
delegate_call
timeline
unknown
```

默认不返回 UE 原始 K2Node 类名。UE 原始类名属于 RawJson 或未来 debug 模式。

---

# 12. Link type 枚举建议

```cpp
enum class EBlueprintHelperLogicLinkType
{
    Exec,
    Data
};
```

MCP 返回值：

```text
exec
data
```

---

# 13. UE 侧服务建议

建议新增或调整：

```cpp
FBlueprintHelperLogicGroupBuilder
FBlueprintHelperLogicMdReadService
FBlueprintHelperLogicJsonReadService
```

## FBlueprintHelperLogicGroupBuilder

职责：

```text
1. 构建 group。
2. 生成 group.entry。
3. 生成 group 内 node_ref。
4. 生成 group 内 link_ref。
5. 将 link 移入 source node。
6. 输出 LogicMD / LogicJson 共用的中间分组模型。
```

## FBlueprintHelperLogicMdReadService

职责：

```text
1. 使用 LogicGroupBuilder 的分组结果。
2. 多入口 scope 下按 group 分段输出 Markdown。
3. 多入口 scope 下返回 grouped=true。
4. 生成 scope 收敛后的 stats。
```

## FBlueprintHelperLogicJsonReadService

职责：

```text
1. 使用 LogicGroupBuilder 的分组结果。
2. 单入口 scope 输出 entry + nodes。
3. 多入口 scope 输出 groups[]。
4. nodes 内部包含 outgoing links。
5. 普通 node 默认只返回 node_ref。
6. link 默认只返回 link_ref。
```

---

# 14. 验收标准

```text
1. target_graph 不允许使用单 entry。
2. target_graph 必须由 UE 侧预分组为 groups。
3. LogicJson 在 target_graph / blueprint / multi_target 下使用 groups[]。
4. LogicMD 在 target_graph / blueprint / multi_target 下按 group 分段。
5. LogicMD 多入口 scope 返回 grouped=true。
6. grouped 只在多入口 scope 出现，单入口 scope 不返回 grouped=false。
7. group.entry.node_path 必须是完整节点路径。
8. group.entry.node_path 不允许是 $.graphs[GraphName]。
9. group.entry.node_path 是 node_ref / link_ref 的反推锚点。
10. node_ref / link_ref 只在当前 group 内有效。
11. LogicJson 删除顶层 logic.links。
12. links 移入 source node 内。
13. node.links 只表达 outgoing links。
14. link 不返回 from_node。
15. 普通 node 默认不返回完整 node_path。
16. link 默认不返回完整 link_path。
17. stats 在多入口 scope 下增加 groups。
18. stats 不适用字段省略，不用 0 填充。
```

---

# 15. 待后续讨论项

```text
1. target_node / target_pin 的 LogicJson 精确结构。
2. pin_detail 模式是否存在。
3. LogicMD Markdown 分段标题最终格式。
4. user_region 的自动命名规则。
5. orphan_group 是否需要暴露 warning。
6. RawJsonRef 字段设计。
7. LogicJson 与 PatchBlueprintGraph 参数路径的精确衔接。
```
