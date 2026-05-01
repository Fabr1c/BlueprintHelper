# BlueprintHelper LogicProcessor 模块设计（2026-04-28）

## 一、修改原因

### 1.1 Agent 不应该直接解析所有蓝图节点细节

当前 raw JSON 中的节点字段是为了可导入、可回放而设计。Agent 若直接消费 raw JSON，会被以下细节干扰：

- `K2Node_*` 具体类型差异
- `inputs` 默认值与实际连线输入混合
- links 只表达 Pin 到 Pin，不直接表达执行流
- 坐标、GUID、编辑器布局字段与逻辑理解无关
- 变量、函数、宏、委托节点需要额外语义归类

因此需要一个模块把 raw JSON 转为 Agent 友好的逻辑图。

### 1.2 逻辑视图应从 raw JSON 派生

不要在导出阶段直接生成逻辑格式。更好的边界是：

```text
UEdGraph -> raw JSON -> logic view
```

原因：

- raw JSON 继续作为唯一事实来源。
- LogicProcessor 可独立测试。
- Bridge / MCP 可以按需生成不同视图。
- 后续 raw JSON schema 增强后，LogicProcessor 可自然利用新增字段。

---

## 二、模块职责

新增 `FBlueprintHelperLogicProcessor`，职责包括：

1. 解析 `BlueprintHelper.JsonToBlueprint` raw JSON。
2. 兼容单图和完整蓝图导出结构。
3. 将节点归类为事件、调用、变量读写、分支、循环、委托、Timeline 等逻辑类型。
4. 根据 links 构建执行流和数据依赖。
5. 输出 `logic_json` 或 `logic_md`。
6. 记录无法可靠归类的节点和连线。

不负责：

- 不直接访问 UE 对象。
- 不生成蓝图节点。
- 不修改 raw JSON。
- 不承担编译诊断。

---

## 三、文件落点

推荐新增：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperLogicProcessor.h
Source/BlueprintHelper/Private/Services/BlueprintHelperLogicProcessor.cpp
```

可选拆分：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperLogicTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperLogicJsonWriter.cpp
Source/BlueprintHelper/Private/Services/BlueprintHelperLogicMarkdownWriter.cpp
```

第一阶段可先保持单 cpp，待逻辑稳定后拆分 Writer。

---

## 四、公开接口草案

```cpp
enum class EBlueprintHelperLogicOutputFormat : uint8
{
    LogicJson,
    Markdown
};

enum class EBlueprintHelperLogicDetailLevel : uint8
{
    Brief,
    Normal,
    Debug
};

struct FBlueprintHelperLogicOptions
{
    EBlueprintHelperLogicOutputFormat Format = EBlueprintHelperLogicOutputFormat::LogicJson;
    EBlueprintHelperLogicDetailLevel DetailLevel = EBlueprintHelperLogicDetailLevel::Normal;

    bool bIncludeDataDependencies = true;
    bool bIncludeOrphanNodes = true;
    bool bIncludeNodeIds = false;
    bool bIncludePositions = false;
    bool bIncludeRawNodeTypes = false;
};

struct FBlueprintHelperLogicResult
{
    bool bSuccess = false;
    FString OutputText;
    FString ErrorMessage;

    int32 NodeCount = 0;
    int32 ExecLinkCount = 0;
    int32 DataLinkCount = 0;
    int32 EntryPointCount = 0;
    int32 OrphanNodeCount = 0;
};

class BLUEPRINTHELPER_API FBlueprintHelperLogicProcessor
{
public:
    static FBlueprintHelperLogicResult ProcessRawJson(
        const FString& RawJsonText,
        const FBlueprintHelperLogicOptions& Options
    );
};
```

---

## 五、内部中间结构

建议内部使用轻量结构，不直接复用导入侧 `FParsedNode`，避免耦合导入语义。

```cpp
struct FBlueprintHelperLogicNode
{
    FString Id;
    FString NodeGuid;
    FString RawType;
    FString Kind;
    FString Label;

    int32 X = 0;
    int32 Y = 0;

    TMap<FString, FString> Inputs;
    TMap<FString, FString> Meta;
};

struct FBlueprintHelperLogicLink
{
    FString FromId;
    FString FromPin;
    FString ToId;
    FString ToPin;
    FString Kind;
};

struct FBlueprintHelperLogicGraph
{
    FString Name;
    TArray<FBlueprintHelperLogicNode> Nodes;
    TArray<FBlueprintHelperLogicLink> Links;
};
```

---

## 六、节点语义分类规则

| Raw Type | Logic Kind | 说明 |
|----------|------------|------|
| `K2Node_Event` | `event` | 原生事件入口 |
| `K2Node_CustomEvent` | `event` | 自定义事件入口 |
| `K2Node_ComponentBoundEvent` | `event` | 组件绑定事件 |
| `K2Node_EnhancedInputAction` | `event` | Enhanced Input 输入事件 |
| `K2Node_CallFunction` | `call` | 函数调用 |
| `K2Node_VariableGet` | `get` | 变量读取 |
| `K2Node_VariableSet` | `set` | 变量写入 |
| `K2Node_IfThenElse` | `branch` | Branch |
| `K2Node_SwitchInteger` / `SwitchString` / `SwitchName` / `SwitchEnum` | `switch` | Switch 分支 |
| `K2Node_ExecutionSequence` | `sequence` | Sequence |
| `K2Node_MacroInstance` + `ForLoop` | `loop` | 标准循环宏 |
| `K2Node_CallDelegate` | `broadcast` | 委托广播 |
| `K2Node_AddDelegate` | `bind_delegate` | 绑定委托 |
| `K2Node_RemoveDelegate` | `unbind_delegate` | 解绑委托 |
| `K2Node_Timeline` | `timeline` | Timeline |
| `K2Node_DynamicCast` | `cast` | 类型转换 |
| `K2Node_Knot` | `reroute` | 重路由节点 |
| `EdGraphNode_Comment` | `comment` | 注释 |
| 未识别类型 | `unknown` | 保留原始类型 |

---

## 七、执行线与数据线分类

### 7.1 优先读取 raw JSON 中的 `kind`

后续 raw JSON 增强后，LogicProcessor 应优先使用 link 的 `kind` 字段：

```json
{
  "from_id": "Node_A",
  "from_pin": "then",
  "to_id": "Node_B",
  "to_pin": "execute",
  "kind": "exec"
}
```

### 7.2 兼容旧 JSON 的启发式判断

旧 JSON 没有 `kind` 时，可以用 Pin 名称进行判断：

```cpp
static bool IsLikelyExecPinName(const FString& PinName)
{
    return PinName.Equals(TEXT("execute"), ESearchCase::IgnoreCase)
        || PinName.Equals(TEXT("then"), ESearchCase::IgnoreCase)
        || PinName.Equals(TEXT("completed"), ESearchCase::IgnoreCase)
        || PinName.Equals(TEXT("loop body"), ESearchCase::IgnoreCase)
        || PinName.Equals(TEXT("true"), ESearchCase::IgnoreCase)
        || PinName.Equals(TEXT("false"), ESearchCase::IgnoreCase)
        || PinName.Equals(TEXT("cast failed"), ESearchCase::IgnoreCase)
        || PinName.Equals(TEXT("update"), ESearchCase::IgnoreCase)
        || PinName.Equals(TEXT("finished"), ESearchCase::IgnoreCase);
}
```

启发式判断必须在输出中允许标记不确定性：

```json
{
  "kind": "exec",
  "confidence": "inferred"
}
```

---

## 八、logic_json 输出草案

```json
{
  "version": "1.0",
  "schema": "BlueprintHelper.LogicGraph",
  "source_schema": "BlueprintHelper.JsonToBlueprint",
  "target": {
    "blueprint": "/Game/BP/BP_Player.BP_Player",
    "graph": "EventGraph"
  },
  "stats": {
    "nodes": 18,
    "exec_links": 12,
    "data_links": 7,
    "entry_points": 2,
    "orphans": 1
  },
  "graphs": [
    {
      "name": "EventGraph",
      "entry_points": [
        {
          "event": "ReceiveBeginPlay",
          "flow": [
            {
              "op": "call",
              "function": "PrintString",
              "args": {
                "InString": "Health System Initialized"
              }
            },
            {
              "op": "set",
              "var": "bIsDead",
              "value": "false"
            }
          ]
        }
      ],
      "data_dependencies": [
        {
          "from": "Health",
          "to": "OnHealthChanged.NewHealth"
        }
      ],
      "orphans": []
    }
  ]
}
```

---

## 九、logic_md 输出草案

```md
# EventGraph

## ReceiveBeginPlay

1. Call `PrintString`
   - InString = "Health System Initialized"

2. Set `bIsDead` = false

## Data Dependencies

- `Health` -> `OnHealthChanged.NewHealth`

## Orphans

- None
```

---

## 十、修改计划

### Phase A：纯转换器实现

- 新增 LogicProcessor 类。
- 只依赖 raw JSON 字符串。
- 实现单图 `nodes` / `links` 解析。
- 输出 basic `logic_json`。

### Phase B：完整蓝图结构支持

- 支持 `graphs` 数组。
- 支持 `blueprint_operations` 摘要。
- 支持函数图、宏图、事件图分组。

### Phase C：Markdown Writer

- 输出人类可读 Markdown。
- 支持 `brief` / `normal` / `debug` 细节级别。

### Phase D：Bridge 集成

- 新增 `export_logic` 命令。
- 调用 ExportService 获取 raw JSON。
- 调用 LogicProcessor 生成结果。

### Phase E：增强 raw JSON 语义字段

- 为 links 补 `kind`。
- 为 pins 补 `direction` / `category`。
- LogicProcessor 优先使用显式语义字段。

