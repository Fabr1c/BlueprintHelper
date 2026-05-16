# BlueprintHelper CallFunction UE Action Resolver 设计方案

日期：2026-05-16
状态：阶段 A-D 已实现并编译通过；candidate_functions 已通过 dry_run issue 结构化返回，后续仅剩更大规模多目标场景扩展验证

## 1. 背景

当前 `callfunction` 解析主要依赖插件侧扫描所有 `UFunction`，再用 native name / display name / token 进行简单评分。该实现距离 UE 编辑器右键菜单搜索仍有明显差距，典型问题是 `Break Vector` 可能命中泛型或非预期函数，而不是用户在 UE 搜索中期望的 Kismet Math Library 节点。

目标不是让 AgentFace 暴露大量低层字段，而是保持 TaskSpec 精简，将复杂匹配逻辑放到插件侧，尽量复用 UE 编辑器自己的搜索、过滤、排序和节点创建规则。

## 2. 目标

1. AgentFace 保持精简，不要求 Agent 默认传完整 owner/native function path。
2. 插件侧尽量复用 UE 右键菜单搜索规则，包括 ActionDatabase、BlueprintActionFilter、GraphSchema 权重和 NodeSpawner。
3. 从现有 TaskSpec、SemanticIR、AssetType、Scope、Graph、BlueprintClass、args、expected type 中提取约束。
4. 不唯一时不盲选，返回精简候选列表给 Agent 再选。
5. 对 `Break Vector`、`Make Vector`、`Set Timer` 等易歧义函数降低误匹配率。
6. preview 阶段尽量暴露解析歧义和类型不兼容，避免 execute 后才编译失败。

## 3. 非目标

1. 不把 AgentFace 扩展成完整 UE 节点搜索 API。
2. 不要求普通 call 都写 owner/native_name/input_types。
3. 不依赖硬编码 alias 作为长期主路径。
4. 不在第一阶段覆盖所有特殊节点生命周期，特殊复杂节点仍可由 C++ pattern 单独处理。

## 4. AgentFace 字段设计

保留现有短格式：

```json
{
  "kind": "call",
  "target": "Print String",
  "args": {
    "In String": "Hello"
  }
}
```

新增一个可选轻量字段 `match`：

```json
{
  "kind": "call",
  "target": "Break Vector",
  "args": {
    "InVec": {
      "kind": "get",
      "target": "ActorLocation"
    }
  },
  "match": {
    "mode": "ue_search",
    "category_priority": ["Math|Vector", "KismetMathLibrary"],
    "ambiguity": "return_candidates"
  }
}
```

### 4.1 `match.mode`

| 值 | 含义 |
| --- | --- |
| `ue_search` | 默认模式。尽量模拟 UE 右键菜单搜索。 |
| `exact` | 只接受稳定 ID、native name、display name 的唯一精确命中。 |
| `fuzzy` | 允许 display/category/keyword/token 模糊匹配，但仍使用插件侧上下文过滤。 |

默认值：`ue_search`。

### 4.2 `match.category_priority`

类型：字符串数组。

作用：排序偏好，不是硬过滤。适合 `Math|Vector`、`Utilities|String`、`KismetMathLibrary` 等类别或 owner 名称。

示例：

```json
"category_priority": ["Math|Vector"]
```

### 4.3 `match.ambiguity`

| 值 | 含义 |
| --- | --- |
| `return_candidates` | 默认值。不唯一时返回候选列表。 |
| `fail` | 不唯一直接失败。 |
| `pick_best` | 插件选择最高分候选，风险最高但 token 最省。 |

默认值：`return_candidates`。

## 5. 插件侧架构

新增独立 resolver，不继续扩大现有 `FBlueprintHelperCallFunctionResolver` 的职责。

建议类：

```text
FBlueprintHelperGraphActionResolver
FBlueprintHelperGraphActionResolveRequest
FBlueprintHelperGraphActionCandidate
FBlueprintHelperGraphActionResolveResult
```

### 5.1 职责分层

```text
SemanticIR / TaskSpec
-> GraphActionResolveRequest
-> UE ActionDatabase candidate universe
-> BlueprintActionFilter context pruning
-> UE-like text/category/keyword weighting
-> SemanticIR type constraint scoring
-> deterministic tie-break
-> NodeSpawner-based creation
```

### 5.2 与旧 resolver 的关系

1. 旧 `FBlueprintHelperCallFunctionResolver` 暂时保留为 fallback。
2. 新路径成功时，`callfunction` 使用 GraphActionResolver。
3. 新路径返回歧义时，preview 直接返回候选列表，不进入旧路径盲选。
4. 后续稳定后，旧 resolver 降级为 legacy/internal-only。

## 6. Resolver 输入约束来源

即使 AgentFace 不额外传字段，插件侧也应从现有信息提取约束。

| 来源 | 可提取信息 |
| --- | --- |
| AssetType | Blueprint / WidgetBlueprint / Actor Blueprint 等。 |
| Scope | function / event / macro / graph。 |
| Graph | graph schema、是否支持 impure function、线程安全限制。 |
| BlueprintClass | ParentClass、GeneratedClass、SkeletonGeneratedClass。 |
| SemanticIR args | 参数名、literal 值、表达式类型、对象 target。 |
| SemanticIR expected type | 返回值期望类型、select/compare/branch 上下文。 |
| get/get_property/ref | 变量、组件、属性路径类型。 |
| match 字段 | mode、category priority、ambiguity policy。 |

## 7. UE 搜索对齐策略

### 7.1 Candidate Universe

从 UE `FBlueprintActionDatabase` 获取 action，而不是自己扫描全部 `UFunction`。

候选应保留：

```text
stable_id
menu_name
display_name
native_function_name
owner_class_path
category
keywords
tooltip
node_spawner
node_class
b_graph_compatible
```

### 7.2 Context Filter

使用 UE `FBlueprintActionFilter` 或等效包装，尽量保持和 `FBlueprintActionMenuUtils::MakeContextMenu` 一致。

必须纳入：

1. 当前 `UBlueprint`。
2. 当前 `UEdGraph`。
3. Graph schema。
4. Context sensitive 规则。
5. Target class。
6. Thread safety。
7. Imported / global field 规则。
8. Exposed category 规则。

### 7.3 Text Matching

按 UE `SGraphActionMenu` 的规则处理搜索文本：

1. trim。
2. 按空格分词。
3. lower。
4. 生成 sanitized term。
5. 所有 term 必须命中 action full search text。

### 7.4 Weighting

优先复用 `UEdGraphSchema_K2::GetActionFilteredWeight`。

如果不能直接复用，应实现等效简化权重：

1. title/menu name 权重最高。
2. native name 精确匹配高于 display fuzzy。
3. category/keywords/description 参与排序。
4. `category_priority` 追加 bonus。
5. SemanticIR 类型匹配追加 bonus。
6. 类型不兼容直接降权或剔除。

## 8. SemanticIR 类型约束

Resolver 应消费 SemanticIR 已推断出的类型，而不是让 AgentFace 额外提供大量字段。

### 8.1 输入类型

从 args 推断：

```json
{
  "args": {
    "InVec": {
      "kind": "get_property",
      "target": "DoorPanel.WorldLocation"
    }
  }
}
```

若 `DoorPanel.WorldLocation` 被 resolver 推断为 `Vector`，则 `Break Vector` 候选应优先匹配输入 pin 可接受 `Vector` 的 action。

### 8.2 输出类型

从外层表达式推断：

1. `branch.condition` 期望 `bool`。
2. `select.options` 期望同类型。
3. `set target` 期望变量类型。
4. `return` 期望函数返回类型。
5. `compare.left/right` 期望可比较且 operator 支持。

### 8.3 无类型时的行为

如果没有足够类型上下文：

1. 使用 UE 搜索权重和 category priority。
2. 如果唯一则通过。
3. 如果不唯一，返回候选列表。
4. 不再盲选低置信候选。

## 9. 歧义返回格式

用户期望精简格式：

```json
{
  "候选函数": []
}
```

建议内容为字符串数组，保留稳定 ID、显示名和分类：

```json
{
  "候选函数": [
    "/Script/Engine.KismetMathLibrary:BreakVector | Break Vector | Math|Vector",
    "/Script/Engine.KismetMathLibrary:BreakVector2D | Break Vector2D | Math|Vector2D"
  ]
}
```

原因：

1. 稳定 ID 可直接回填精确选择。
2. 显示名便于 Agent 判断。
3. 分类能提供低 token 的语义上下文。

## 10. Preview 行为

Preview 必须做到：

1. 解析唯一候选时返回 resolved function evidence。
2. 不唯一时返回 `ambiguous_function_call`，并携带 `候选函数`。
3. 无候选时返回 `function_call_not_found`。
4. 类型不兼容时返回 `function_call_type_mismatch`。
5. 不应等 execute 编译失败才暴露解析问题。

## 11. Execute 行为

Execute 使用 preview 阶段同一 resolver。

1. 如果 preview 已解析 stable id，execute 应使用同一 stable id。
2. 如果未 preview 或 preview 结果过期，execute 重新解析并校验一致性。
3. 使用 `UBlueprintNodeSpawner` 创建节点。
4. 创建后仍需要 `AllocateDefaultPins` / `Autowire` / `ReconstructNode` 顺序安全。
5. DebugBundle 记录 resolver 输入、候选摘要、最终选择、失败原因。

## 12. 对 `Break Vector` 的期望行为

输入：

```json
{
  "kind": "call",
  "target": "Break Vector",
  "match": {
    "mode": "ue_search",
    "category_priority": ["Math|Vector"],
    "ambiguity": "return_candidates"
  }
}
```

期望：

1. 优先命中 `/Script/Engine.KismetMathLibrary:BreakVector`。
2. 如果缺少 Vector 类型上下文且存在多个 Break Vector 变体，返回候选列表。
3. 不命中泛型模板或不可执行的非菜单候选。
4. 最终节点通过 NodeSpawner 或等效 UE 菜单路径创建。

## 13. 实现阶段

### 阶段 A：低风险修复

1. 组件模板 `reuse_existing` 改为 `reuse_if_exists`。
2. TS/Python compiler 增加 `reuse_existing -> reuse_if_exists` alias。
3. C++ adapter 可选增加同 alias 防御。

完成标准：旧模板输入和新模板输入都能 preview/execute 到 UE 侧合法枚举。

### 阶段 B：SemanticIR preview 类型推断补强

1. literal 自动推断 `bool/int/double/string`。
2. compare 解析短操作符到 typed UE 函数候选。
3. select 校验 condition/index 与 options 类型。
4. preview 阶段输出明确 diagnostics。

完成标准：`select(compare(int == int))` preview 能稳定推断并 execute 编译通过；不兼容类型 preview 阶段失败。

### 阶段 C：GraphActionResolver 第一版

1. 新增 resolver request/result/candidate 类型。
2. 使用 ActionDatabase 构造候选。
3. 使用 BlueprintActionFilter 或等效上下文过滤。
4. 支持 `match.mode`、`category_priority`、`ambiguity`。
5. 不唯一时返回 `候选函数`。

完成标准：`Break Vector` 在常见上下文下不再误命中泛型候选；不唯一时返回精简候选列表。

### 阶段 D：NodeSpawner 创建路径

1. resolver candidate 绑定 `UBlueprintNodeSpawner`。
2. callfunction 创建节点改走 NodeSpawner。
3. 保留旧 `UK2Node_CallFunction + SetFromFunction` fallback。
4. DebugBundle 记录 spawn strategy。

完成标准：节点创建行为接近 UE 右键菜单；特殊函数初始化更稳定。

### 阶段 E：收敛旧 resolver

1. 旧 resolver 只保留 legacy/internal fallback。
2. TaskSpec call 主路径走 GraphActionResolver。
3. 文档和 AgentGuide 更新默认推荐。

完成标准：普通 call、display-name call、owner-qualified call、category-priority call 全部进入统一 resolver 主路径。

## 14. 风险与缓解

| 风险 | 影响 | 缓解 |
| --- | --- | --- |
| ActionDatabase 依赖编辑器初始化状态 | preview 可能不稳定 | resolver 检测数据库可用性，不可用时返回明确 blocker。 |
| NodeSpawner 创建路径复杂 | 可能引入节点创建回归 | 第一阶段保留旧创建 fallback。 |
| UE 搜索权重不可完全复用 | 排序仍可能和 UI 不一致 | 先复用 GraphSchema weight，不能复用时记录差距。 |
| 类型推断不完整 | 仍会出现歧义 | 返回候选列表，不盲选。 |
| AgentFace 字段扩展失控 | token 增长 | 只允许一个可选 `match` 对象。 |

## 15. 预期收益

当前实现约等于 UE 搜索效果的 4 成。

完成本方案后：

1. 不填写 `match`：预计达到 7-8 成。
2. 填写 `match.category_priority`：预计达到 8.5-9 成。
3. 仍不唯一：返回精简候选列表，避免错误写图。

## 16. 待确认点

1. `match.mode` 是否需要保留 `fuzzy`，还是只保留 `exact` 与 `ue_search`。
2. `候选函数` 是否固定使用中文字段名，还是内部 JSON 使用 `candidate_functions`，CLI summary 显示中文。
3. NodeSpawner 主路径是否允许在第一版只覆盖 `UK2Node_CallFunction`，特殊节点后续逐步接入。
4. 是否允许 DebugBundle 记录完整候选详情，而 CLI stdout 只返回精简列表。
## 17. 2026-05-16 修订：search_mode 与 candidate_functions 分组

### 17.1 AgentFace 字段命名

最终采用 `search_mode` 作为 AgentFace 侧轻量字段，用于指定函数搜索策略。`search_mode` 可以放在 call statement 或 call expression 内。

示例：

```json
{
  "kind": "call",
  "target": "Break Vector",
  "search_mode": "ue_search",
  "category_priority": ["Math|Vector"],
  "ambiguity": "return_candidates"
}
```

其中：

| 字段 | 含义 |
| --- | --- |
| `search_mode` | `ue_search`、`exact`、`fuzzy`。默认 `ue_search`。 |
| `category_priority` | 排序偏好，不是硬过滤。 |
| `ambiguity` | `return_candidates`、`fail`、`pick_best`。默认 `return_candidates`。 |

### 17.2 candidate_functions 返回结构

当 TaskSpec 内指定 `search_mode`，并且某个 call 无法唯一解析时，返回的 `candidate_functions` 必须按目标函数分组，而不是扁平数组。

结构：

```json
{
  "candidate_functions": [
    {
      "target": "Break Vector",
      "candidates": [
        "/Script/Engine.KismetMathLibrary:BreakVector | Break Vector | Math|Vector",
        "/Script/Engine.KismetMathLibrary:BreakVector2D | Break Vector2D | Math|Vector2D"
      ]
    },
    {
      "target": "Print String",
      "candidates": []
    }
  ]
}
```

规则：

1. 一个 TaskSpec 内有多个目标函数时，每个目标函数一个分组。
2. 单个 call resolver 也返回同样结构，只是数组中只有一个目标函数分组。
3. `target` 使用 AgentFace 原始目标文本，不使用 resolver 改写后的别名。
4. `candidates` 内部使用精简字符串，包含 stable id、UE 显示名、分类。
5. 如果某个目标函数没有候选但 search_mode 要求返回候选，也保留空数组，便于 Agent 精确定位是哪一个 call 失败。

当前实现状态：单个 resolver 的 ambiguity message 已按该结构输出；完整 TaskSpec 多 call 聚合仍待 GraphActionResolver/preview 汇总阶段完成。
## 18. 2026-05-16 首轮实现记录

已完成：

1. 组件模板已改为 `reuse_if_exists`。
2. TypeScript compiler 支持 `reuse_existing -> reuse_if_exists` alias。
3. Python P1 compiler 支持 `reuse_existing -> reuse_if_exists` alias。
4. C++ TaskPlan component adapter 支持 `reuse_existing -> reuse_if_exists` 防御性 alias。
5. SemanticIR preview 增加 literal 自动类型推断，覆盖 `bool/int/double/string`。
6. SemanticIR preview 增加 branch condition bool 校验。
7. SemanticIR preview 增加 compare operand 类型兼容校验。
8. SemanticIR preview 增加 select condition/options 类型校验。
9. callfunction ambiguity message 中的 `candidate_functions` 已改为按目标函数分组的结构。
10. 已移除 Break/Make Vector 局部特判；函数匹配改为 ActionDatabase/BlueprintActionFilter/NodeSpawner + 通用 category_priority 加权。

已验证：

1. `npm --prefix AgentFaceService/task-core run build` 通过。
2. `npm --prefix AgentFaceService/cli run build` 通过。
3. `Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload` 通过。

距离期望差距：

1. 完整 `GraphActionResolver` 尚未实现，当前仍未真正接入 UE `ActionDatabase + BlueprintActionFilter + NodeSpawner` 主路径。
2. `search_mode` 尚未进入 C++ SemanticIR 结构化字段和 preview 汇总层。
3. 多 call TaskSpec 的 `candidate_functions` 聚合尚未实现；当前只保证单 resolver ambiguity message 使用目标函数分组格式。
4. NodeSpawner 创建路径尚未替代 `UK2Node_CallFunction + SetFromFunction`。
## 19. 2026-05-16 通用 ActionDatabase 路径实现记录

状态：已实现并通过构建验证。

已完成：
1. `FBlueprintHelperCallFunctionResolver` 的候选来源改为优先读取 UE `FBlueprintActionDatabase`，并使用 `FBlueprintActionFilter` 按当前 `UBlueprint` 与 `UEdGraph` 上下文过滤 `UK2Node_CallFunction` 候选。
2. resolver candidate 绑定 `UBlueprintNodeSpawner`；节点创建主路径改为 `NodeSpawner->Invoke(...)`，仅在缺少 spawner 时保留 `UK2Node_CallFunction + SetFromFunction` fallback。
3. 移除 `Break Vector / Make Vector` 的 KismetMathLibrary/Vector 局部特殊加权，不再用单点函数名补丁影响排序。
4. 新增通用 `category_priority` 排序加权：按 category、owner class path、native function name、display name 进行优先级匹配，作为 resolver 排序 bonus。
5. 新增 `search_mode`：`exact/precise` 只接受 stable id、owner-qualified native、native exact、display exact；默认 `ue_search/fuzzy` 继续允许 compact/token 匹配。
6. 新增 `ambiguity`：默认 `return_candidates`；`pick_best/best` 可显式选择最高分候选。
7. `candidate_functions` ambiguity message 已保持按目标函数分组的结构：`[{ target, candidates }]`。
8. `search_mode`、`ambiguity/ambiguity_policy`、`category_priority` 已接入 SemanticIR statement/expression 解析、fragment metadata、statement call 和 expression call 的 `FParsedNode`。
9. `make_struct` 移除 Vector 专用 `MakeVector` 路径，回到通用 `K2Node_MakeStruct` struct operation fragment builder。
10. 组件 `reuse_existing` 兼容别名已同步到 TypeScript compiler、Python P1 compiler 与 C++ TaskPlan adapter，规范输出仍为 `reuse_if_exists`。

验证：
1. `npm.cmd --prefix .\AgentFaceService\task-core run build` 通过。
2. `npm.cmd --prefix .\AgentFaceService\cli run build` 通过。
3. `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload` 通过。

距离期望差距：
1. 多 call TaskSpec 的 `candidate_functions` 跨步骤汇总还没有在 preview 汇总层形成一个统一数组；当前单个 resolver 返回已是目标函数分组结构。
2. 类型约束评分目前仍主要依赖 SemanticIR 已有类型校验和参数名，尚未完整消费每个候选 pin 的 typed compatibility weight。
3. 尚未做真实编辑器端到端覆盖测试来确认 `Break Vector`、`Make Vector`、`Set Timer` 等复杂搜索场景与 UE 菜单排序完全一致。

阻塞内容：
1. 无编译阻塞；剩余是 preview 汇总层和更完整 typed pin 评分的后续实现项。
## 20. 2026-05-16 结构化 candidate_functions 落地记录

新增内容：
1. candidate_functions 不再只依赖 message 内嵌 JSON 字符串，UE dry_run issue 已增加结构化 candidate_functions: [{ target, candidates }] 字段。
2. AgentFace TaskIssueSchema 改为 passthrough，preview/execute issue 收集保留 UE 返回的扩展字段，便于 Agent 直接读取候选函数数组。
3. 候选函数来源保持通用 ActionDatabase + BlueprintActionFilter + NodeSpawner 路径，没有对 Break Vector / Make Vector 做局部特判。

修复内容：
1. 修复 ambiguity preview 只能从自然语言 message 提取候选函数的问题，候选信息现在在 	oolResult.data.issues[].candidate_functions 和 extra.issues[].candidate_functions 同步出现。
2. 保持正常成功路径不受影响：Print String 通过 ue_search preview/execute 成功，并可编译保存。

变更需求：
1. candidate_functions 采用目标函数分组结构：[{ target: string, candidates: string[] }]，符合 TaskSpec 指定多个目标函数时继续扩展为多组的设计方向。

快速修复：
1. 测试 TaskSpec 中 Graph 名和 Custom Event 名不能相同，否则 UE 编译会报“图表已存在/无法生成另一个同名图表”。

阻塞内容：
1. 无当前阻塞。更大规模多 call 汇总场景尚未作为独立压力测试覆盖，但主链路和单目标分组结构已验证通过。