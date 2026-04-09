# BlueprintHelper Converter 全蓝图操作升级计划

## 一、结论

当前 Converter 架构 **不能** 直接支持全蓝图操作升级，原因不是代码质量问题，而是结构性瓶颈。
但好消息是：核心管线（T3D 解析 → JSON → 节点生成 → 连线）是通的，升级路径是"渐进重构 + 分版本扩展"而非推翻重写。

---

## 二、架构瓶颈分析

### 2.1 导入侧：`TextToBlueprintGenerator` 的结构性问题

#### 问题 A：节点类型识别是 if-else 硬编码链

当前 `ResolveNodeType()` 用四段 if-else 判断 `K2Node_CallFunction / VariableGet / VariableSet / MacroInstance`。
每新增一种节点类型就要加一段 if-else + 对应的 Spawn 函数 + 对应的 DTO 字段。

**影响**：蓝图有 50+ 种 K2Node 子类，if-else 链扩展到 10 种以上就不可维护。

#### 问题 B：`FParsedNode` 是扁平混合结构

一个 `FParsedNode` 混装了函数节点、变量节点、宏节点的所有字段：
- `FunctionName`（只有 CallFunction 用）
- `VariableReference`（只有 Variable 用）
- `MacroReference`（只有 Macro 用）

新增 CustomEvent、Tunnel、Timeline 等节点时，会继续在同一个结构体里堆字段，互相无关的字段越来越多。

**影响**：违反单一职责，新增节点类型时容易误触已有字段逻辑。

#### 问题 C：Spawn 逻辑单体化

`SpawnFunctionNode` / `SpawnVariableGetNode` / `SpawnMacroNode` 等都是独立的静态方法，散落在 Generator 类中。节点类型越多，类越膨胀。

**影响**：无法用策略模式按类型分派，不利于独立测试和扩展。

#### 问题 D：Pin Alias 硬编码在 `FindPinByAlias`

`execute`、`then`、`loopbody`、`firstindex` 等别名全部写死在一组 if-else 中。
不同节点类型有不同的 pin 别名需求（例如 Branch 的 Condition、Select 的 Index），无法统一管理。

**影响**：新增节点类型时必须同时修改 alias 代码，遗漏会导致连线静默失败。

#### 问题 E：不支持"图级操作"

当前 Generator 只能在已有图表中 **添加节点**，无法：
- 创建新的函数图（Custom Function）
- 创建新的事件图
- 创建新的宏图
- 添加成员变量到蓝图
- 添加事件分发器（Event Dispatcher）
- 修改蓝图类设置（Parent Class 等）

这些操作需要调用 `FBlueprintEditorUtils` 的 API，而非 `UEdGraph` 的节点操作 API，是完全不同的层级。

### 2.2 导出侧：`FBlueprintToTextConverter` 的结构性问题

#### 问题 F：依赖 T3D 文本解析而非 UEdGraph 对象模型

`ParseT3DToNodes` 用字符串匹配和正则解析 T3D 导出文本。这是一种"从序列化格式反推"的做法：
- T3D 格式不是稳定 API，UE 版本升级可能变化
- 不同节点类型的 T3D 导出格式不统一
- 复杂节点（Timeline、AnimGraph 等）的 T3D 包含深层嵌套对象，正则无法可靠解析

**影响**：每新增一种节点导出支持，就需要新写一组正则和特殊解析逻辑。

#### 问题 G：`FormatNodesToJson` 中节点类型判断同样硬编码

导出侧用 `IsNodeTypeMatch` 区分 VariableGet/Set 和 MacroInstance，其余统一当 CallFunction 处理。
新增节点类型需要在此处加分支。

#### 问题 H：剪贴板依赖

`ConvertClipboardToJson` 是唯一入口。程序化导出（Bridge/AI 驱动）需要绕过剪贴板。

### 2.3 JSON Schema 层面的局限

#### 问题 I：Schema 只描述"图内节点操作"

当前 JSON 顶层结构是 `{ declarations, nodes, links }`，只能描述一个图表内的节点添加。
缺少以下层级的表达能力：
- 蓝图级操作（创建函数、变量、事件分发器）
- 多图操作（同时编辑 EventGraph 和自定义函数）
- 资产级操作（创建蓝图、重命名、设置父类）

---

## 三、升级风险评估

| 风险 | 等级 | 描述 |
|------|------|------|
| 回归破坏 | 高 | 重构 FParsedNode 和 ResolveNodeType 可能影响已有功能 |
| Schema 不兼容 | 中 | 升级 JSON 格式后旧 JSON 无法导入 |
| T3D 解析不稳定 | 高 | 新节点的 T3D 格式可能不遵循现有解析假设 |
| 工作量膨胀 | 中 | 蓝图节点类型多，容器操作函数多，需控制每版范围 |
| 编辑器 API 兼容 | 低 | `FBlueprintEditorUtils` API 稳定，但需验证每个调用 |

---

## 四、架构调整方案

### 4.1 导入侧：引入节点处理器策略模式

**核心改动**：用注册式策略替代 if-else 链。

```cpp
// 节点处理器接口
class IBlueprintNodeHandler
{
public:
    virtual ~IBlueprintNodeHandler() = default;

    /** 是否接管此 JSON 节点。 */
    virtual bool CanHandle(const TSharedPtr<FJsonObject>& NodeObject) const = 0;

    /** 从 JSON 解析为中间数据。 */
    virtual TSharedPtr<FParsedNodeBase> Parse(const TSharedPtr<FJsonObject>& NodeObject) const = 0;

    /** 在图表中生成节点。 */
    virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNodeBase& NodeData, FString& OutError) const = 0;

    /** 返回此处理器支持的 Pin Alias 表。 */
    virtual TMap<FString, FString> GetPinAliases() const { return {}; }
};
```

**注册表**：

```cpp
class FBlueprintNodeHandlerRegistry
{
public:
    static FBlueprintNodeHandlerRegistry& Get();
    void Register(TSharedRef<IBlueprintNodeHandler> Handler);
    IBlueprintNodeHandler* FindHandler(const TSharedPtr<FJsonObject>& NodeObject) const;
    UEdGraphPin* FindPinByAlias(UK2Node* Node, const FString& Alias) const;

private:
    TArray<TSharedRef<IBlueprintNodeHandler>> Handlers;
};
```

**好处**：
- 每种节点类型一个 Handler 文件，互不干扰
- 新增节点只需实现 Handler + 注册，不改 Generator 主逻辑
- Pin Alias 跟着 Handler 走，不再全局硬编码
- 可以独立测试每个 Handler

### 4.2 数据模型：FParsedNode 改为继承体系

```cpp
// 基类：所有节点共享的字段
struct FParsedNodeBase
{
    FString Id;
    FString SourceType;
    float X = 0.f;
    float Y = 0.f;
    TMap<FString, FString> DefaultValues;
    virtual ~FParsedNodeBase() = default;
};

// 函数调用节点
struct FParsedCallFunctionNode : FParsedNodeBase
{
    FString FunctionName;
};

// 变量节点
struct FParsedVariableNode : FParsedNodeBase
{
    FParsedVariableReference VariableReference;
    bool bIsSet = false;
};

// 宏节点
struct FParsedMacroNode : FParsedNodeBase
{
    FParsedMacroReference MacroReference;
};

// Branch 节点
struct FParsedBranchNode : FParsedNodeBase {};

// Custom Event 节点
struct FParsedCustomEventNode : FParsedNodeBase
{
    FString EventName;
    TArray<FParsedPinType> AdditionalOutputs;
};

// ... 后续每种节点一个子类
```

### 4.3 导出侧：双轨导出

保留 T3D 解析路径（用于剪贴板兼容），新增 UEdGraph 对象直接导出路径：

```cpp
class FBlueprintToTextConverter
{
public:
    // 旧路径：T3D 文本解析（保留兼容）
    static FString ConvertTextToJson(const FString& T3DText);

    // 新路径：从图表对象直接导出
    static FString ConvertGraphToJson(UEdGraph* Graph, EExportScope Scope);

private:
    // 导出也走节点处理器策略
    static TSharedPtr<FJsonObject> ExportNode(UEdGraphNode* Node);
};
```

新路径基于 `UEdGraphNode` 运行时对象，不依赖 T3D 文本格式，可以直接读取：
- 节点类型（`GetClass()->GetName()`）
- 所有引脚及类型
- 连线关系（`Pin->LinkedTo`）
- 节点专属属性（通过 Cast 到具体 K2Node 子类）

### 4.4 JSON Schema：引入操作层级

```json
{
  "version": "2.0",
  "schema": "BlueprintHelper.JsonToBlueprint",
  "target": {
    "blueprint": "/Game/BP/BP_Test.BP_Test",
    "graph": "EventGraph"
  },
  "blueprint_operations": [
    {
      "op": "add_member_variable",
      "name": "Health",
      "pin_type": { "category": "float" },
      "default_value": "100.0"
    },
    {
      "op": "add_function_graph",
      "name": "CalculateDamage",
      "inputs": [...],
      "outputs": [...]
    },
    {
      "op": "add_event_dispatcher",
      "name": "OnHealthChanged",
      "params": [...]
    }
  ],
  "declarations": { "local_variables": [] },
  "nodes": [],
  "links": []
}
```

**版本兼容策略**：
- version `1.x` JSON 维持现有行为不变
- version `2.0` 要求 target 字段，支持 blueprint_operations
- Generator 入口根据 version 分发

### 4.5 Pin Alias 配置化

从硬编码改为 JSON 配置表：

```json
{
  "global_aliases": {
    "execute": "execute",
    "then": "then",
    "completed": "Completed"
  },
  "node_aliases": {
    "K2Node_IfThenElse": {
      "condition": "Condition",
      "true": "then",
      "false": "else"
    },
    "K2Node_MacroInstance.ForLoop": {
      "loop_body": "LoopBody",
      "first_index": "FirstIndex",
      "last_index": "LastIndex",
      "index": "Index"
    },
    "K2Node_Select": {
      "index": "Index"
    }
  }
}
```

加载路径：`Resources/PinAliases.json`，模块启动时读入，Handler 可覆盖。

---

## 五、分版本升级计划

### 版本路线概览

| 版本 | 代号 | 核心目标 | JSON 版本 | 状态 |
|------|------|---------|-----------|------|
| v1.2 | 架构重构 | Handler 策略 + 数据模型拆分 + Pin Alias 配置化 | 1.2 | ✅ 已完成 |
| v1.3 | 控制流 | Branch / Sequence / Select / Switch / FlipFlop / DoOnce | 1.3 | ✅ 已完成 |
| v1.4 | 事件与委托 | CustomEvent / EventDispatcher / Bind / Unbind / Call | 1.4 | ✅ 已完成 |
| v1.5 | 完整宏与容器 | 所有标准宏 + ForEachLoop / Array / Map / Set 操作函数 | 1.5 | ✅ 已完成 |
| v2.0 | 蓝图级操作 | 创建变量 / 函数 / 事件分发器 / Schema 2.0 | 2.0 | ✅ 已完成 |
| v2.1 | 图表管理 | 创建/删除函数图 / 宏图 / 导出整图 | 2.0 |
| v2.2 | 高级节点 | Timeline / SpawnActor / Cast / Interface / Struct 操作 | 2.0 |
| v2.3 | 全覆盖收尾 | Tunnel / Composite / 动画蓝图节点 / 数学表达式节点 | 2.0 |

---

### v1.2 — 架构重构（基础设施）

**目标**：不新增功能，只重构结构，现有四种节点全部迁移到新架构，行为不变。

#### 任务列表

1. 定义 `IBlueprintNodeHandler` 接口
2. 定义 `FParsedNodeBase` 基类和现有四种子类
3. 建立 `FBlueprintNodeHandlerRegistry` 注册表
4. 实现四个 Handler：
   - `FCallFunctionNodeHandler`
   - `FVariableGetNodeHandler`
   - `FVariableSetNodeHandler`
   - `FMacroInstanceNodeHandler`
5. 提取 Pin Alias 配置文件 `Resources/PinAliases.json`
6. `GenerateBlueprintFromJson` 改为走 Registry 分发
7. 导出侧新增 `ConvertGraphToJson(UEdGraph*)` 基于对象模型的导出路径
8. 建立回归测试 JSON fixture（覆盖现有四种节点 + 连线）
9. **验收**：所有现有功能行为不变，测试全通过

#### 文件落点

| 新增文件 | 说明 |
|---------|------|
| `Public/NodeHandlers/BlueprintNodeHandler.h` | 接口 + 注册表 + 基类 DTO |
| `Private/NodeHandlers/CallFunctionNodeHandler.cpp` | 函数调用 Handler |
| `Private/NodeHandlers/VariableGetNodeHandler.cpp` | 变量读取 Handler |
| `Private/NodeHandlers/VariableSetNodeHandler.cpp` | 变量写入 Handler |
| `Private/NodeHandlers/MacroInstanceNodeHandler.cpp` | 宏节点 Handler |
| `Resources/PinAliases.json` | Pin 别名配置 |
| `Resources/TestFixtures/` | 回归测试 JSON 文件目录 |

#### 对现有文件的改造

| 文件 | 改动 |
|------|------|
| `TextToBlueprintGenerator.h` | 保留公共 API 签名不变，内部调用改为走 Registry |
| `TextToBlueprintGenerator.cpp` | `ResolveNodeType` / Spawn 系列方法标记 deprecated，逻辑迁移到 Handler |
| `BlueprintTextConverter.cpp` | `FormatNodesToJson` 中增加对象模型导出分支 |
| `BlueprintHelper.cpp` | `StartupModule` 中注册内置 Handler |

---

### v1.3 — 控制流节点

**目标**：支持最常用的控制流蓝图节点。

#### 新增节点支持

| 节点类型 | K2Node 类 | 优先级 |
|---------|----------|--------|
| Branch | `K2Node_IfThenElse` | P0 |
| Sequence | `K2Node_ExecutionSequence` | P0 |
| Select | `K2Node_Select` | P1 |
| Switch on Int/String/Name/Enum | `K2Node_SwitchInteger` 等 | P1 |
| FlipFlop | `K2Node_FlipFlop`（标准宏） | P2 |
| DoOnce | `K2Node_DoOnce`（标准宏） | P2 |
| Gate | `K2Node_Gate`（标准宏） | P2 |
| DoN | 标准宏 | P2 |
| WhileLoop | 标准宏 | P2 |
| ForEachLoop | 标准宏 | P2 |

#### 任务列表

1. 实现 `FBranchNodeHandler`
2. 实现 `FSequenceNodeHandler`
3. 实现 `FSelectNodeHandler`
4. 实现 `FSwitchNodeHandler`（含 SwitchOnInt/String/Name/Enum 分支）
5. 扩展 `FMacroInstanceNodeHandler` 支持所有标准宏库中的宏名（不再仅限 ForLoop）
6. 为每种节点补充 PinAliases.json 条目
7. 导出侧：在对象模型路径中支持上述节点类型识别与属性提取
8. 补充回归测试 fixture

#### JSON 示例

```json
{
  "id": "Node_Branch_0",
  "type": "K2Node_IfThenElse",
  "name": "Branch",
  "x": 300,
  "y": 0,
  "inputs": {}
}
```

```json
{
  "id": "Node_Sequence_0",
  "type": "K2Node_ExecutionSequence",
  "name": "Sequence",
  "x": 0,
  "y": 0,
  "inputs": {
    "num_outputs": "3"
  }
}
```

#### 宏节点策略变更

v1.2 的 `FMacroInstanceNodeHandler` 已经通过宏名在标准宏库中查找宏图。
v1.3 只需确保识别更多标准宏名即可，不需要新 Handler。

关键判断：FlipFlop / DoOnce / Gate / WhileLoop / ForEachLoop 等本质上都是 `K2Node_MacroInstance`，只是 `macro.name` 不同。
因此只需在 `PinAliases.json` 中为每个宏补充引脚别名。

---

### v1.4 — 事件与委托

**目标**：支持自定义事件、事件分发器相关操作。

#### 新增节点支持

| 节点类型 | K2Node 类 | 说明 |
|---------|----------|------|
| Custom Event | `K2Node_CustomEvent` | 创建自定义事件节点 |
| Event（引擎事件） | `K2Node_Event` | BeginPlay / Tick 等 |
| Call Function (Delegate) | `K2Node_CallDelegate` | 调用事件分发器 |
| Bind Event | `K2Node_AddDelegate` | 绑定事件分发器 |
| Unbind Event | `K2Node_RemoveDelegate` | 解绑事件分发器 |
| Unbind All | `K2Node_ClearDelegate` | 解绑所有 |
| Assign | `K2Node_AssignDelegate` | 快捷绑定 |
| Create Event | `K2Node_CreateDelegate` | 创建委托对象 |

#### JSON 示例

```json
{
  "id": "Node_CustomEvent_0",
  "type": "K2Node_CustomEvent",
  "name": "OnDamageReceived",
  "x": 0,
  "y": 0,
  "event": {
    "event_name": "OnDamageReceived",
    "params": [
      { "name": "Damage", "pin_type": { "category": "float" } },
      { "name": "Instigator", "pin_type": { "category": "object", "object_path": "/Script/Engine.Actor" } }
    ]
  }
}
```

#### 特殊处理

- `K2Node_CustomEvent` 需要通过 `FBlueprintEditorUtils::AddCustomEvent` 创建（非直接 NewObject）
- `K2Node_Event` 是引擎预定义事件，只需查找现有 override 入口
- 委托操作需要已有事件分发器变量，Handler 需报告"分发器不存在"错误

---

### v1.5 — 完整宏与容器操作

**目标**：确保所有标准宏可用，并支持 Array/Map/Set 的操作函数。

#### 标准宏全覆盖
确保以下标准宏在导入/导出两端都正确处理：
- ForLoop / ForLoopWithBreak
- ForEachLoop / ForEachLoopWithBreak
- WhileLoop
- FlipFlop / DoOnce / DoN / Gate
- IsValid（宏版本）
- Delay / RetriggerableDelay（需 Latent 处理）

#### 容器操作函数
以下操作本质上是 `K2Node_CallFunction`，当前架构已可处理。
问题在于 **AI 可能不知道正确函数名**，需在规则文档中列出常用容器操作：

| 操作 | 实际函数名 | 备注 |
|------|----------|------|
| Array Add | `Array_Add` | UKismetArrayLibrary |
| Array Remove | `Array_Remove` | |
| Array Find | `Array_Find` | |
| Array Length | `Array_Length` | |
| Array Get | `Array_Get` | |
| Array Contains | `Array_Contains` | |
| Array Clear | `Array_Clear` | |
| Map Add | `Map_Add` | UBlueprintMapLibrary |
| Map Remove | `Map_Remove` | |
| Map Find | `Map_Find` | |
| Map Contains | `Map_Contains` | |
| Map Keys | `Map_Keys` | |
| Map Values | `Map_Values` | |
| Map Length | `Map_Length` | |
| Set Add | `Set_Add` | UBlueprintSetLibrary |
| Set Remove | `Set_Remove` | |
| Set Contains | `Set_Contains` | |
| Set Length | `Set_Length` | |
| Set Clear | `Set_Clear` | |

#### 特殊节点

| 节点类型 | K2Node 类 | 说明 |
|---------|----------|------|
| MakeArray | `K2Node_MakeArray` | 构造数组字面量 |
| MakeMap | `K2Node_MakeMap` | 构造 Map 字面量 |
| MakeSet | `K2Node_MakeSet` | 构造 Set 字面量 |
| MakeStruct | `K2Node_MakeStruct` | 构造结构体 |
| BreakStruct | `K2Node_BreakStruct` | 拆解结构体 |

需新增 Handler：`FMakeContainerNodeHandler`、`FStructOperationNodeHandler`。

#### Latent 函数处理

Delay / RetriggerableDelay 是 Latent 函数，虽然是宏或 CallFunction 节点，但需要特殊语义：
- 蓝图需要在 EventGraph 中使用（不能在函数图中）
- Latent 节点有隐藏的 WorldContextObject 引脚

Handler 需检测 `HasLatentInfo` 元数据并做限制。

#### 更新规则文档

在 `JsonToBlueprintRules.md` 中新增：
- 容器操作函数快查表
- MakeArray / MakeMap 等字面量节点示例
- Latent 函数使用约束

---

### v2.0 — 蓝图级操作（Schema 大版本）

**目标**：突破"图内节点操作"的边界，支持蓝图资产级操作。

#### 新增能力

| 操作 | UE API | 说明 |
|------|--------|------|
| 创建成员变量 | `FBlueprintEditorUtils::AddMemberVariable` | |
| 删除成员变量 | `FBlueprintEditorUtils::RemoveMemberVariable` | |
| 创建函数图 | `FBlueprintEditorUtils::AddFunctionGraph` | |
| 删除函数图 | `FBlueprintEditorUtils::RemoveGraph` | |
| 创建宏图 | `FBlueprintEditorUtils::AddMacroGraph` | |
| 创建事件分发器 | `FBlueprintEditorUtils::AddMulticastDelegate` | |
| 设置函数/变量标记 | Flags / Category / Tooltip 等 | |
| 创建函数输入输出 | 在函数入口节点上添加引脚 | |

#### 架构变更

新增 `IBlueprintOperationHandler` 接口（与节点 Handler 平行）：

```cpp
class IBlueprintOperationHandler
{
public:
    virtual ~IBlueprintOperationHandler() = default;
    virtual bool CanHandle(const FString& OpName) const = 0;
    virtual bool Execute(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& OpPayload, FString& OutError) = 0;
};
```

#### JSON Schema 2.0

```json
{
  "version": "2.0",
  "schema": "BlueprintHelper.JsonToBlueprint",
  "target": {
    "blueprint": "/Game/BP/BP_Test.BP_Test",
    "graph": "EventGraph"
  },
  "blueprint_operations": [
    {
      "op": "add_member_variable",
      "name": "Health",
      "pin_type": { "category": "float" },
      "default_value": "100.0",
      "category": "Stats",
      "flags": { "blueprint_read_only": false, "expose_on_spawn": false }
    },
    {
      "op": "add_function_graph",
      "name": "CalculateDamage",
      "inputs": [
        { "name": "BaseDamage", "pin_type": { "category": "float" } }
      ],
      "outputs": [
        { "name": "FinalDamage", "pin_type": { "category": "float" } }
      ],
      "is_pure": false
    },
    {
      "op": "add_event_dispatcher",
      "name": "OnHealthChanged",
      "params": [
        { "name": "NewHealth", "pin_type": { "category": "float" } }
      ]
    }
  ],
  "declarations": { "local_variables": [] },
  "nodes": [],
  "links": []
}
```

#### 执行顺序

`blueprint_operations` 在 `nodes` 之前执行：
1. 先执行蓝图级操作（创建变量、函数等）
2. 再执行节点操作（在指定图表中添加节点）
3. 最后执行连线

这保证了引用完整性：节点可以引用刚创建的变量或函数。

#### 版本兼容

- version 1.x 的 JSON 走旧路径（无 blueprint_operations，无 target）
- version 2.0 要求 target 字段

---

### v2.1 — 图表管理

**目标**：支持多图操作、整图导出、图表创建删除。

#### 新增能力

- 导出整个蓝图为 JSON（所有图表 + 所有变量 + 所有函数签名）
- 指定目标图表（EventGraph / 自定义函数名）
- 在 JSON 中描述多个图表的节点

#### JSON 格式扩展

```json
{
  "version": "2.0",
  "graphs": [
    {
      "graph": "EventGraph",
      "nodes": [...],
      "links": [...]
    },
    {
      "graph": "CalculateDamage",
      "nodes": [...],
      "links": [...]
    }
  ]
}
```

---

### v2.2 — 高级节点

**目标**：覆盖日常蓝图开发中频繁使用的高级节点。

#### 新增节点支持

| 节点类型 | K2Node 类 | 难度 |
|---------|----------|------|
| SpawnActor | `K2Node_SpawnActor` | 中 |
| Cast | `K2Node_DynamicCast` | 低 |
| Interface Call | `K2Node_CallParentFunction` | 中 |
| Self | `K2Node_Self` | 低 |
| Get/Set by Index (Array) | `K2Node_GetArrayItem` | 低 |
| Timeline | `K2Node_Timeline` | 高 |
| Format Text | `K2Node_FormatText` | 中 |
| Promote to Variable | N/A（编辑器操作） | 中 |
| Construct Object | `K2Node_ConstructObjectFromClass` | 中 |
| Validate Get | `K2Node_CallFunction` IsValid | 低 |
| Math Expression | `K2Node_MathExpression` | 高 |

#### Timeline 特殊处理

Timeline 是最复杂的单个节点：
- 需要创建 `UTimelineTemplate` 子对象
- 包含 Float/Vector/Color/Event 曲线轨道
- 每条轨道有关键帧数据

建议通过 JSON 定义轨道结构，在 Handler 中调用 `FBlueprintEditorUtils::AddTimeline` + 填充曲线数据。

---

### v2.3 — 全覆盖收尾

**目标**：覆盖剩余长尾节点类型，达成"可表达任何蓝图操作"。

#### 覆盖列表

| 类别 | 包含节点 |
|------|---------|
| Tunnel / Composite | `K2Node_Tunnel`、`K2Node_Composite` |
| Component Getter | `K2Node_ComponentBoundEvent`、`K2Node_VariableGet` 组件变量 |
| Literal | `K2Node_Literal`（对象引用常量） |
| Enum | `K2Node_GetEnumeratorName`、`K2Node_GetEnumeratorNameAsString` |
| 注释 | `UEdGraphNode_Comment` |
| 重新路由 | `K2Node_Knot` |
| AnimGraph 节点 | 视需求扩展 |

---

## 六、导出侧升级路线

导出侧每个版本跟随导入侧升级：

| 版本 | 导出侧任务 |
|------|----------|
| v1.2 | 新增 `ConvertGraphToJson` 基于对象模型路径 |
| v1.3 | 识别控制流节点，导出 type + 专属字段 |
| v1.4 | 识别事件/委托节点，导出 event / delegate 字段 |
| v1.5 | 识别 MakeArray / MakeMap 等容器字面量节点 |
| v2.0 | 导出蓝图级信息：变量列表、函数列表、事件分发器列表 |
| v2.1 | 导出多图结构 |
| v2.2 | 识别 Timeline、SpawnActor 等高级节点属性 |
| v2.3 | 导出 Comment、Knot、Tunnel 等 |

---

## 七、每版改造范围控制

### 原则
- 每个版本只新增一类节点族，不跨越边界
- 每个版本都必须同时更新：Handler + PinAliases + JSON 规则文档 + 回归 fixture
- 导入导出必须同版本配套，不能一端超前
- 版本号体现在 JSON 的 `version` 字段中，Generator 做向后兼容检查

### 回归测试策略

每版新增的 fixture 文件命名规范：
每个版本生成一段测试Json，包含该版本新增的节点类型和功能点，命名为 `v{version}_{feature}.json`，放在 `Resources/TestFixtures/` 目录下。
```
Resources/TestFixtures/
  v1.2_CallFunction.json
  v1.2_VariableGetSet.json
  v1.2_MacroForLoop.json
  v1.3_Branch.json
  v1.3_Sequence.json
  v1.3_AllStandardMacros.json
  v1.4_CustomEvent.json
  v1.4_EventDispatcher.json
  v1.5_MakeArray.json
  v1.5_ContainerFunctions.json
  v2.0_BlueprintOperations.json
  ...
```

---

## 八、与 AI Bridge 计划的关系

本升级计划与实施计划文档（`Module_BlueprintHelper_AI_ImplementationPlan_20260408.md`）的关系：

| AI 实施计划 Phase | 本计划版本 | 关系 |
|------------------|----------|------|
| Phase 1: Core 头less 化 | v1.2 | **合并执行**：v1.2 重构可与 Service 抽取同步进行 |
| Phase 2: Bridge 落地 | v1.2 完成后 | Bridge 基于 v1.2 的 Registry 架构暴露能力 |
| Phase 3: MCP 接入 | v1.3+ | MCP tools 暴露的能力随版本扩展 |
| Phase 4: 成功率增强 | v1.3 ~ v2.3 | v1.3 以后每个版本都在增强 AI 可操作范围 |

建议：**v1.2 与 Phase 1 合并为同一批任务**，一次性完成架构升级 + Service 抽取。

---

## 九、结论

当前架构核心管线是可复用的，但扩展机制不足。
关键改造是 v1.2 的策略模式引入 —— 一旦完成，后续每个版本的工作量可预测、可隔离。
不需要推翻现有代码，但需要将 if-else 分派 → 注册表分派、扁平 DTO → 继承体系、硬编码 alias → 配置文件 三项基础设施落地后再开始扩展节点类型。
