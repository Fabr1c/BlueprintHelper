# BlueprintHelper Agent 导入 JSON 技术文档（2026-04-30）

## 一、技术目标

本文档定义 `BlueprintHelper.AgentImportGraph` 的技术结构、导入管线、字段规范、校验规则、自动布局策略和 Bridge / MCP 集成方式。

该协议用于让 Agent 以短 JSON 描述蓝图修改意图，再由 BlueprintHelper 插件转换为真实 UE 蓝图节点。

---

## 二、推荐文件落点

### 2.1 C++ 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperAgentImportTypes.h
Source/BlueprintHelper/Public/Services/BlueprintHelperAgentImportProcessor.h
Source/BlueprintHelper/Private/Services/BlueprintHelperAgentImportProcessor.cpp
Source/BlueprintHelper/Private/Services/BlueprintHelperAgentImportValidator.cpp
Source/BlueprintHelper/Private/Services/BlueprintHelperGraphAutoLayout.cpp
```

第一阶段也可以先合并为两个文件：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperAgentImportProcessor.h
Source/BlueprintHelper/Private/Services/BlueprintHelperAgentImportProcessor.cpp
```

### 2.2 资源与测试文件

```text
Resources/TestFixtures/AgentImport/
Resources/Plan/
```

建议 fixture 命名：

```text
simple_beginplay_print.agent_import.json
simple_beginplay_print.expected.logic.md
branch_set_variable.agent_import.json
branch_set_variable.expected.logic.md
```

---

## 三、Schema 定义

### 3.1 顶层结构

```json
{
  "schema": "BlueprintHelper.AgentImportGraph",
  "version": "1.0",
  "target_blueprint": "/Game/BP/BP_Player.BP_Player",
  "target_graph": "EventGraph",
  "mode": "append",
  "layout": "auto",
  "symbols": {},
  "declarations": {},
  "nodes": [],
  "links": [],
  "options": {}
}
```

### 3.2 顶层字段说明

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `schema` | string | 是 | 固定为 `BlueprintHelper.AgentImportGraph` |
| `version` | string | 是 | 当前为 `1.0` |
| `target_blueprint` | string | 是 | 目标蓝图资产路径 |
| `target_graph` | string | 是 | 目标图表名称 |
| `mode` | string | 是 | `append`、`replace_graph`、后续 `patch` |
| `layout` | string/object | 否 | 默认 `auto` |
| `symbols` | object | 否 | 函数、类、变量路径别名 |
| `declarations` | object | 否 | 变量、函数、事件分发器声明 |
| `nodes` | array | 是 | 语义节点列表 |
| `links` | array | 否 | 节点连线列表 |
| `options` | object | 否 | 编译、保存、校验策略 |

---

## 四、字段裁剪规则

### 4.1 禁止字段

Agent 导入 JSON 不应接受以下字段。校验器可以允许但给 warning，也可以在 strict 模式下报错。

```text
Pos
PosX
PosY
NodePosX
NodePosY
NodeWidth
NodeHeight
GraphGuid
NodeGuid
PinGuid
PersistentGuid
CompilerMessage
ErrorType
ErrorMsg
AdvancedPinDisplay
bCommentBubbleVisible
CommentBubblePinned
```

### 4.2 可选但不推荐字段

以下字段仅在 debug 或 patch 模式下有意义：

```text
node_guid
pin_guid
raw_type
raw_pin_type
```

普通 `append` 模式不应要求这些字段。

---

## 五、内部类型草案

```cpp
enum class EBlueprintHelperAgentImportMode : uint8
{
    Append,
    ReplaceGraph,
    Patch
};

enum class EBlueprintHelperAgentLayoutStrategy : uint8
{
    Auto,
    AppendRight,
    AppendBelow,
    Compact,
    DebugSpread,
    PreserveExisting
};

struct FBlueprintHelperAgentImportOptions
{
    bool bCompile = true;
    bool bSave = false;
    bool bStrict = false;
    bool bDryRun = false;
    bool bCreateMissingVariables = true;
    bool bCreateMissingFunctions = false;
    bool bAutoLayout = true;
};

struct FBlueprintHelperAgentImportNode
{
    FString Id;
    FString Kind;
    FString Label;
    TSharedPtr<FJsonObject> Payload;
};

struct FBlueprintHelperAgentImportLink
{
    FString Kind;
    FString FromNode;
    FString FromPin;
    FString ToNode;
    FString ToPin;
};

struct FBlueprintHelperAgentImportResult
{
    bool bSuccess = false;
    FString ErrorCode;
    FString Message;
    TArray<FString> Warnings;
    int32 CreatedNodeCount = 0;
    int32 CreatedLinkCount = 0;
    int32 CreatedVariableCount = 0;
};
```

---

## 六、公开接口草案

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperAgentImportProcessor
{
public:
    static FBlueprintHelperAgentImportResult ImportFromJsonText(
        const FString& JsonText
    );

private:
    static bool ParseRequest(
        const FString& JsonText,
        FBlueprintHelperAgentImportRequest& OutRequest,
        FString& OutError
    );

    static bool ValidateRequest(
        const FBlueprintHelperAgentImportRequest& Request,
        TArray<FString>& OutWarnings,
        FString& OutError
    );

    static bool ResolveTargetGraph(
        const FString& TargetBlueprint,
        const FString& TargetGraph,
        UBlueprint*& OutBlueprint,
        UEdGraph*& OutGraph,
        FString& OutError
    );
};
```

---

## 七、导入管线

推荐处理流程：

```text
JSON Text
  ↓
Parse AgentImportGraph
  ↓
Validate Schema / Required Fields
  ↓
Resolve target_blueprint / target_graph
  ↓
Resolve symbols
  ↓
Apply declarations
  ↓
Create semantic nodes
  ↓
Resolve pins and defaults
  ↓
Create links
  ↓
Auto layout
  ↓
Compile blueprint
  ↓
Optional save
  ↓
Return structured result
```

### 7.1 Parse

解析阶段只处理 JSON 合法性和基础字段读取，不访问 UE 对象。

错误示例：

```json
{
  "success": false,
  "error_code": "JsonParseFailed",
  "message": "Agent import JSON is not valid JSON."
}
```

### 7.2 Validate

校验阶段检查：

- `schema` 是否为 `BlueprintHelper.AgentImportGraph`。
- `version` 是否支持。
- `target_blueprint` 是否存在。
- `target_graph` 是否存在或是否允许创建。
- 所有 `nodes[].id` 是否唯一。
- 所有 links 是否引用存在的节点。
- `kind` 是否支持。
- 是否包含禁用字段。

### 7.3 Resolve Symbols

Agent 可以通过 `symbols` 减少重复路径：

```json
{
  "symbols": {
    "PrintString": "/Script/Engine.KismetSystemLibrary:PrintString"
  },
  "nodes": [
    {
      "id": "print",
      "kind": "call",
      "function": "$PrintString"
    }
  ]
}
```

解析器应在创建节点前把 `$PrintString` 展开为完整路径。

### 7.4 Apply Declarations

支持声明变量：

```json
{
  "declarations": {
    "variables": [
      {
        "name": "Health",
        "type": "float",
        "default": 100.0,
        "editable": true,
        "category": "Stats"
      }
    ]
  }
}
```

第一阶段建议只支持变量声明。函数图、宏图、事件分发器可作为后续扩展。

### 7.5 Create Nodes

每种 `kind` 应有独立处理函数：

```cpp
CreateEventNode(...)
CreateCustomEventNode(...)
CreateCallFunctionNode(...)
CreateVariableGetNode(...)
CreateVariableSetNode(...)
CreateBranchNode(...)
CreateSequenceNode(...)
CreateSwitchNode(...)
CreateLoopNode(...)
CreateCommentNode(...)
```

所有创建出的节点都注册到本地 id 映射：

```text
Agent Node Id -> UEdGraphNode*
```

### 7.6 Resolve Pins

Pin 解析规则：

1. 优先精确匹配 Pin 名。
2. 支持常见别名，例如 `execute` / `exec`、`then` / `Then`。
3. 对 `call` 节点，允许只写非默认输入。
4. 对 `set` 节点，允许 `value` 映射到变量输入 Pin。
5. 无法匹配时返回结构化错误，不静默忽略。

### 7.7 Create Links

连线支持两种格式。

简写格式：

```json
{
  "kind": "exec",
  "from": "begin_play.then",
  "to": "print.execute"
}
```

结构化格式：

```json
{
  "kind": "exec",
  "from_node": "begin_play",
  "from_pin": "then",
  "to_node": "print",
  "to_pin": "execute"
}
```

内部统一转换为结构化格式。

---

## 八、节点 kind 规范

### 8.1 `event`

```json
{
  "id": "begin_play",
  "kind": "event",
  "event": "ReceiveBeginPlay"
}
```

必须字段：

| 字段 | 说明 |
|------|------|
| `event` | 原生事件名 |

### 8.2 `custom_event`

```json
{
  "id": "on_damage",
  "kind": "custom_event",
  "name": "OnDamage",
  "params": [
    { "name": "Damage", "type": "float" }
  ]
}
```

### 8.3 `call`

```json
{
  "id": "print",
  "kind": "call",
  "function": "/Script/Engine.KismetSystemLibrary:PrintString",
  "inputs": {
    "InString": "Hello"
  }
}
```

### 8.4 `get`

```json
{
  "id": "get_health",
  "kind": "get",
  "var": "Health"
}
```

### 8.5 `set`

```json
{
  "id": "set_health",
  "kind": "set",
  "var": "Health",
  "value": 100.0
}
```

### 8.6 `branch`

```json
{
  "id": "branch_alive",
  "kind": "branch",
  "condition": "is_alive.value"
}
```

`condition` 可以是节点输出引用，也可以由 data link 指定。

### 8.7 `comment`

```json
{
  "id": "comment_init",
  "kind": "comment",
  "text": "初始化玩家状态",
  "contains": ["begin_play", "set_health"]
}
```

Comment 节点不需要坐标。布局器根据 `contains` 自动计算注释框范围。

---

## 九、自动布局策略

### 9.1 基础布局算法

第一阶段推荐简单稳定算法：

1. 找到入口节点：event、custom_event、function entry。
2. 沿 exec link 做 BFS 或拓扑遍历。
3. 每一层 X 递增固定间距。
4. 同层节点 Y 按入口流分组。
5. Branch / Switch 的 True、False、Case 分支拉开 Y 间距。
6. 数据节点放在消费节点左上方。
7. 孤立节点放在图表底部单独区域。
8. Comment 节点最后根据包含节点包围盒生成。

### 9.2 默认间距建议

```cpp
constexpr int32 ExecLayerSpacingX = 420;
constexpr int32 ExecNodeSpacingY = 220;
constexpr int32 DataNodeOffsetX = -260;
constexpr int32 DataNodeOffsetY = -120;
constexpr int32 BranchSpacingY = 260;
constexpr int32 OrphanAreaOffsetY = 900;
```

这些值应集中定义，方便后续调参。

---

## 十、Bridge / MCP 集成

### 10.1 Bridge 命令

新增命令：

```text
import_agent_graph
```

请求：

```json
{
  "request_id": "req_import_001",
  "command": "import_agent_graph",
  "payload": {
    "schema": "BlueprintHelper.AgentImportGraph",
    "version": "1.0",
    "target_blueprint": "/Game/BP/BP_Player.BP_Player",
    "target_graph": "EventGraph",
    "mode": "append",
    "layout": "auto",
    "nodes": [],
    "links": []
  }
}
```

响应：

```json
{
  "request_id": "req_import_001",
  "success": true,
  "result": {
    "schema": "BlueprintHelper.AgentImportResult",
    "created_nodes": 2,
    "created_links": 1,
    "created_variables": 0,
    "compiled": true,
    "saved": false,
    "warnings": []
  }
}
```

### 10.2 MCP 工具建议

新增 MCP 工具：

```text
blueprint_import_agent_graph
```

用途：Agent 传入 `BlueprintHelper.AgentImportGraph`，插件将其转换为蓝图节点。

保留现有：

```text
blueprint_import_json
```

用途：导入完整 raw JSON。

---

## 十一、错误码

| 错误码 | 场景 |
|--------|------|
| `JsonParseFailed` | JSON 解析失败 |
| `UnsupportedSchema` | schema 不支持 |
| `UnsupportedVersion` | version 不支持 |
| `InvalidTargetBlueprint` | 蓝图路径无效 |
| `InvalidTargetGraph` | 图表不存在或类型不支持 |
| `DuplicateNodeId` | 节点 id 重复 |
| `UnknownNodeKind` | 节点 kind 不支持 |
| `UnknownSymbol` | `$Symbol` 无法解析 |
| `UnknownVariable` | 变量不存在且不允许创建 |
| `UnknownFunction` | 函数路径无法解析 |
| `InvalidLinkEndpoint` | 连线引用不存在的节点或 Pin |
| `PinTypeMismatch` | Pin 类型不兼容 |
| `CompileFailed` | 导入后编译失败 |

错误响应必须包含可供 Agent 修正的信息：

```json
{
  "success": false,
  "error_code": "InvalidLinkEndpoint",
  "message": "Link target pin 'print.exec' was not found.",
  "diagnostics": [
    {
      "path": "links[0].to",
      "suggestion": "Use 'print.execute' or inspect available pins."
    }
  ]
}
```

---

## 十二、测试计划

### 12.1 单元测试

| 用例 | 验证点 |
|------|--------|
| Parse minimal graph | 最小 JSON 可解析 |
| Reject missing target | 缺少目标蓝图时报错 |
| Reject duplicate id | 重复节点 id 报错 |
| Parse shorthand link | `node.pin` 格式可解析 |
| Create BeginPlay + PrintString | 可生成基本蓝图 |
| Remove Pos fields | JSON 不含坐标仍可布局 |
| Unknown function | 函数路径错误可诊断 |

### 12.2 集成测试

1. 导入 `BeginPlay -> PrintString`。
2. 编译蓝图成功。
3. 导出 logic_md，确认执行流正确。
4. 导出 raw_json，确认节点坐标由插件生成。
5. 保存关闭再打开，蓝图结构保持。

---

## 十三、实现优先级

| 优先级 | 内容 |
|--------|------|
| P0 | schema 解析、append 模式、event/call/link、自动布局 |
| P0 | 移除 Pos 依赖，插件内部布局 |
| P1 | get/set/branch/sequence/comment |
| P1 | declarations.variables |
| P1 | 结构化错误与 dry_run |
| P2 | switch/loop/cast/spawn_actor |
| P2 | symbols 表 |
| P3 | patch 模式、review diff、preserve_existing layout |

---

## 十四、兼容性要求

1. 不修改现有 `blueprint_import_json` 的行为。
2. 不改变 `BlueprintHelper.JsonToBlueprint` raw JSON schema 的必要字段。
3. 新增命令失败不影响旧 MCP 工具。
4. `AgentImportGraph` 不能伪装成 raw JSON。
5. 所有写操作必须明确目标资产路径和目标图表。

---

## 十五、最终建议

第一阶段只实现稳定的最小闭环：

```text
AgentImportGraph JSON
  -> BeginPlay / CustomEvent / Call / Get / Set / Branch
  -> Exec/Data Links
  -> Auto Layout
  -> Compile
  -> Structured Result
```

这能覆盖大多数 Agent 初始构图场景，同时避免过早进入复杂 patch 和 diff 合并问题。
