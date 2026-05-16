# BlueprintHelper CallFunction UE Action Resolver 设计方案

日期�?026-05-16
状态：阶段 A-D 已实现并编译通过；candidate_functions 已通过 dry_run issue 结构化返回，后续仅剩更大规模多目标场景扩展验�?
## 1. 背景

当前 `callfunction` 解析主要依赖插件侧扫描所�?`UFunction`，再�?native name / display name / token 进行简单评分。该实现距离 UE 编辑器右键菜单搜索仍有明显差距，典型问题�?`Break Vector` 可能命中泛型或非预期函数，而不是用户在 UE 搜索中期望的 Kismet Math Library 节点�?
目标不是�?AgentFace 暴露大量低层字段，而是保持 TaskSpec 精简，将复杂匹配逻辑放到插件侧，尽量复用 UE 编辑器自己的搜索、过滤、排序和节点创建规则�?
## 2. 目标

1. AgentFace 保持精简，不要求 Agent 默认传完�?owner/native function path�?2. 插件侧尽量复�?UE 右键菜单搜索规则，包�?ActionDatabase、BlueprintActionFilter、GraphSchema 权重�?NodeSpawner�?3. 从现�?TaskSpec、SemanticIR、AssetType、Scope、Graph、BlueprintClass、args、expected type 中提取约束�?4. 不唯一时不盲选，返回精简候选列表给 Agent 再选�?5. �?`Break Vector`、`Make Vector`、`Set Timer` 等易歧义函数降低误匹配率�?6. preview 阶段尽量暴露解析歧义和类型不兼容，避�?execute 后才编译失败�?
## 3. 非目�?
1. 不把 AgentFace 扩展成完�?UE 节点搜索 API�?2. 不要求普�?call 都写 owner/native_name/input_types�?3. 不依赖硬编码 alias 作为长期主路径�?4. 不在第一阶段覆盖所有特殊节点生命周期，特殊复杂节点仍可�?C++ pattern 单独处理�?
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

新增一个可选轻量字�?`match`�?
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

| �?| 含义 |
| --- | --- |
| `ue_search` | 默认模式。尽量模�?UE 右键菜单搜索�?|
| `exact` | 只接受稳�?ID、native name、display name 的唯一精确命中�?|
| `fuzzy` | 允许 display/category/keyword/token 模糊匹配，但仍使用插件侧上下文过滤�?|

默认值：`ue_search`�?
### 4.2 `match.category_priority`

类型：字符串数组�?
作用：排序偏好，不是硬过滤。适合 `Math|Vector`、`Utilities|String`、`KismetMathLibrary` 等类别或 owner 名称�?
示例�?
```json
"category_priority": ["Math|Vector"]
```

### 4.3 `match.ambiguity`

| �?| 含义 |
| --- | --- |
| `return_candidates` | 默认值。不唯一时返回候选列表�?|
| `fail` | 不唯一直接失败�?|
| `pick_best` | 插件选择最高分候选，风险最高但 token 最省�?|

默认值：`return_candidates`�?
## 5. 插件侧架�?
新增独立 resolver，不继续扩大现有 `FBlueprintHelperCallFunctionResolver` 的职责�?
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

### 5.2 与旧 resolver 的关�?
1. �?`FBlueprintHelperCallFunctionResolver` 暂时保留�?fallback�?2. 新路径成功时，`callfunction` 使用 GraphActionResolver�?3. 新路径返回歧义时，preview 直接返回候选列表，不进入旧路径盲选�?4. 后续稳定后，�?resolver 降级�?legacy/internal-only�?
## 6. Resolver 输入约束来源

即使 AgentFace 不额外传字段，插件侧也应从现有信息提取约束�?
| 来源 | 可提取信�?|
| --- | --- |
| AssetType | Blueprint / WidgetBlueprint / Actor Blueprint 等�?|
| Scope | function / event / macro / graph�?|
| Graph | graph schema、是否支�?impure function、线程安全限制�?|
| BlueprintClass | ParentClass、GeneratedClass、SkeletonGeneratedClass�?|
| SemanticIR args | 参数名、literal 值、表达式类型、对�?target�?|
| SemanticIR expected type | 返回值期望类型、select/compare/branch 上下文�?|
| get/get_property/ref | 变量、组件、属性路径类型�?|
| match 字段 | mode、category priority、ambiguity policy�?|

## 7. UE 搜索对齐策略

### 7.1 Candidate Universe

�?UE `FBlueprintActionDatabase` 获取 action，而不是自己扫描全�?`UFunction`�?
候选应保留�?
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

使用 UE `FBlueprintActionFilter` 或等效包装，尽量保持�?`FBlueprintActionMenuUtils::MakeContextMenu` 一致�?
必须纳入�?
1. 当前 `UBlueprint`�?2. 当前 `UEdGraph`�?3. Graph schema�?4. Context sensitive 规则�?5. Target class�?6. Thread safety�?7. Imported / global field 规则�?8. Exposed category 规则�?
### 7.3 Text Matching

�?UE `SGraphActionMenu` 的规则处理搜索文本：

1. trim�?2. 按空格分词�?3. lower�?4. 生成 sanitized term�?5. 所�?term 必须命中 action full search text�?
### 7.4 Weighting

优先复用 `UEdGraphSchema_K2::GetActionFilteredWeight`�?
如果不能直接复用，应实现等效简化权重：

1. title/menu name 权重最高�?2. native name 精确匹配高于 display fuzzy�?3. category/keywords/description 参与排序�?4. `category_priority` 追加 bonus�?5. SemanticIR 类型匹配追加 bonus�?6. 类型不兼容直接降权或剔除�?
## 8. SemanticIR 类型约束

Resolver 应消�?SemanticIR 已推断出的类型，而不是让 AgentFace 额外提供大量字段�?
### 8.1 输入类型

�?args 推断�?
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

�?`DoorPanel.WorldLocation` �?resolver 推断�?`Vector`，则 `Break Vector` 候选应优先匹配输入 pin 可接�?`Vector` �?action�?
### 8.2 输出类型

从外层表达式推断�?
1. `branch.condition` 期望 `bool`�?2. `select.options` 期望同类型�?3. `set target` 期望变量类型�?4. `return` 期望函数返回类型�?5. `compare.left/right` 期望可比较且 operator 支持�?
### 8.3 无类型时的行�?
如果没有足够类型上下文：

1. 使用 UE 搜索权重�?category priority�?2. 如果唯一则通过�?3. 如果不唯一，返回候选列表�?4. 不再盲选低置信候选�?
## 9. 歧义返回格式

用户期望精简格式�?
```json
{
  "候选函�?: []
}
```

建议内容为字符串数组，保留稳�?ID、显示名和分类：

```json
{
  "候选函�?: [
    "/Script/Engine.KismetMathLibrary:BreakVector | Break Vector | Math|Vector",
    "/Script/Engine.KismetMathLibrary:BreakVector2D | Break Vector2D | Math|Vector2D"
  ]
}
```

原因�?
1. 稳定 ID 可直接回填精确选择�?2. 显示名便�?Agent 判断�?3. 分类能提供低 token 的语义上下文�?
## 10. Preview 行为

Preview 必须做到�?
1. 解析唯一候选时返回 resolved function evidence�?2. 不唯一时返�?`ambiguous_function_call`，并携带 `候选函数`�?3. 无候选时返回 `function_call_not_found`�?4. 类型不兼容时返回 `function_call_type_mismatch`�?5. 不应�?execute 编译失败才暴露解析问题�?
## 11. Execute 行为

Execute 使用 preview 阶段同一 resolver�?
1. 如果 preview 已解�?stable id，execute 应使用同一 stable id�?2. 如果�?preview �?preview 结果过期，execute 重新解析并校验一致性�?3. 使用 `UBlueprintNodeSpawner` 创建节点�?4. 创建后仍需�?`AllocateDefaultPins` / `Autowire` / `ReconstructNode` 顺序安全�?5. DebugBundle 记录 resolver 输入、候选摘要、最终选择、失败原因�?
## 12. �?`Break Vector` 的期望行�?
输入�?
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

期望�?
1. 优先命中 `/Script/Engine.KismetMathLibrary:BreakVector`�?2. 如果缺少 Vector 类型上下文且存在多个 Break Vector 变体，返回候选列表�?3. 不命中泛型模板或不可执行的非菜单候选�?4. 最终节点通过 NodeSpawner 或等�?UE 菜单路径创建�?
## 13. 实现阶段

### 阶段 A：低风险修复

1. 组件模板 `reuse_existing` 改为 `reuse_if_exists`�?2. TS/Python compiler 增加 `reuse_existing -> reuse_if_exists` alias�?3. C++ adapter 可选增加同 alias 防御�?
完成标准：旧模板输入和新模板输入都能 preview/execute �?UE 侧合法枚举�?
### 阶段 B：SemanticIR preview 类型推断补强

1. literal 自动推断 `bool/int/double/string`�?2. compare 解析短操作符�?typed UE 函数候选�?3. select 校验 condition/index �?options 类型�?4. preview 阶段输出明确 diagnostics�?
完成标准：`select(compare(int == int))` preview 能稳定推断并 execute 编译通过；不兼容类型 preview 阶段失败�?
### 阶段 C：GraphActionResolver 第一�?
1. 新增 resolver request/result/candidate 类型�?2. 使用 ActionDatabase 构造候选�?3. 使用 BlueprintActionFilter 或等效上下文过滤�?4. 支持 `match.mode`、`category_priority`、`ambiguity`�?5. 不唯一时返�?`候选函数`�?
完成标准：`Break Vector` 在常见上下文下不再误命中泛型候选；不唯一时返回精简候选列表�?
### 阶段 D：NodeSpawner 创建路径

1. resolver candidate 绑定 `UBlueprintNodeSpawner`�?2. callfunction 创建节点改走 NodeSpawner�?3. 保留�?`UK2Node_CallFunction + SetFromFunction` fallback�?4. DebugBundle 记录 spawn strategy�?
完成标准：节点创建行为接�?UE 右键菜单；特殊函数初始化更稳定�?
### 阶段 E：收敛旧 resolver

1. �?resolver 只保�?legacy/internal fallback�?2. TaskSpec call 主路径走 GraphActionResolver�?3. 文档�?AgentGuide 更新默认推荐�?
完成标准：普�?call、display-name call、owner-qualified call、category-priority call 全部进入统一 resolver 主路径�?
## 14. 风险与缓�?
| 风险 | 影响 | 缓解 |
| --- | --- | --- |
| ActionDatabase 依赖编辑器初始化状�?| preview 可能不稳�?| resolver 检测数据库可用性，不可用时返回明确 blocker�?|
| NodeSpawner 创建路径复杂 | 可能引入节点创建回归 | 第一阶段保留旧创�?fallback�?|
| UE 搜索权重不可完全复用 | 排序仍可能和 UI 不一�?| 先复�?GraphSchema weight，不能复用时记录差距�?|
| 类型推断不完�?| 仍会出现歧义 | 返回候选列表，不盲选�?|
| AgentFace 字段扩展失控 | token 增长 | 只允许一个可�?`match` 对象�?|

## 15. 预期收益

当前实现约等�?UE 搜索效果�?4 成�?
完成本方案后�?
1. 不填�?`match`：预计达�?7-8 成�?2. 填写 `match.category_priority`：预计达�?8.5-9 成�?3. 仍不唯一：返回精简候选列表，避免错误写图�?
## 16. 待确认点

1. `match.mode` 是否需要保�?`fuzzy`，还是只保留 `exact` �?`ue_search`�?2. `候选函数` 是否固定使用中文字段名，还是内部 JSON 使用 `candidate_functions`，CLI summary 显示中文�?3. NodeSpawner 主路径是否允许在第一版只覆盖 `UK2Node_CallFunction`，特殊节点后续逐步接入�?4. 是否允许 DebugBundle 记录完整候选详情，�?CLI stdout 只返回精简列表�?## 17. 2026-05-16 修订：search_mode �?candidate_functions 分组

### 17.1 AgentFace 字段命名

最终采�?`search_mode` 作为 AgentFace 侧轻量字段，用于指定函数搜索策略。`search_mode` 可以放在 call statement �?call expression 内�?
示例�?
```json
{
  "kind": "call",
  "target": "Break Vector",
  "search_mode": "ue_search",
  "category_priority": ["Math|Vector"],
  "ambiguity": "return_candidates"
}
```

其中�?
| 字段 | 含义 |
| --- | --- |
| `search_mode` | `ue_search`、`exact`、`fuzzy`。默�?`ue_search`�?|
| `category_priority` | 排序偏好，不是硬过滤�?|
| `ambiguity` | `return_candidates`、`fail`、`pick_best`。默�?`return_candidates`�?|

### 17.2 candidate_functions 返回结构

�?TaskSpec 内指�?`search_mode`，并且某�?call 无法唯一解析时，返回�?`candidate_functions` 必须按目标函数分组，而不是扁平数组�?
结构�?
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

规则�?
1. 一�?TaskSpec 内有多个目标函数时，每个目标函数一个分组�?2. 单个 call resolver 也返回同样结构，只是数组中只有一个目标函数分组�?3. `target` 使用 AgentFace 原始目标文本，不使用 resolver 改写后的别名�?4. `candidates` 内部使用精简字符串，包含 stable id、UE 显示名、分类�?5. 如果某个目标函数没有候选但 search_mode 要求返回候选，也保留空数组，便�?Agent 精确定位是哪一�?call 失败�?
当前实现状态：单个 resolver �?ambiguity message 已按该结构输出；完整 TaskSpec �?call 聚合仍待 GraphActionResolver/preview 汇总阶段完成�?## 18. 2026-05-16 首轮实现记录

已完成：

1. 组件模板已改�?`reuse_if_exists`�?2. TypeScript compiler 支持 `reuse_existing -> reuse_if_exists` alias�?3. Python P1 compiler 支持 `reuse_existing -> reuse_if_exists` alias�?4. C++ TaskPlan component adapter 支持 `reuse_existing -> reuse_if_exists` 防御�?alias�?5. SemanticIR preview 增加 literal 自动类型推断，覆�?`bool/int/double/string`�?6. SemanticIR preview 增加 branch condition bool 校验�?7. SemanticIR preview 增加 compare operand 类型兼容校验�?8. SemanticIR preview 增加 select condition/options 类型校验�?9. callfunction ambiguity message 中的 `candidate_functions` 已改为按目标函数分组的结构�?10. 已移�?Break/Make Vector 局部特判；函数匹配改为 ActionDatabase/BlueprintActionFilter/NodeSpawner + 通用 category_priority 加权�?
已验证：

1. `npm --prefix AgentFaceService/task-core run build` 通过�?2. `npm --prefix AgentFaceService/cli run build` 通过�?3. `Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload` 通过�?
距离期望差距�?
1. 完整 `GraphActionResolver` 尚未实现，当前仍未真正接�?UE `ActionDatabase + BlueprintActionFilter + NodeSpawner` 主路径�?2. `search_mode` 尚未进入 C++ SemanticIR 结构化字段和 preview 汇总层�?3. �?call TaskSpec �?`candidate_functions` 聚合尚未实现；当前只保证�?resolver ambiguity message 使用目标函数分组格式�?4. NodeSpawner 创建路径尚未替代 `UK2Node_CallFunction + SetFromFunction`�?## 19. 2026-05-16 通用 ActionDatabase 路径实现记录

状态：已实现并通过构建验证�?
已完成：
1. `FBlueprintHelperCallFunctionResolver` 的候选来源改为优先读�?UE `FBlueprintActionDatabase`，并使用 `FBlueprintActionFilter` 按当�?`UBlueprint` �?`UEdGraph` 上下文过�?`UK2Node_CallFunction` 候选�?2. resolver candidate 绑定 `UBlueprintNodeSpawner`；节点创建主路径改为 `NodeSpawner->Invoke(...)`，仅在缺�?spawner 时保�?`UK2Node_CallFunction + SetFromFunction` fallback�?3. 移除 `Break Vector / Make Vector` �?KismetMathLibrary/Vector 局部特殊加权，不再用单点函数名补丁影响排序�?4. 新增通用 `category_priority` 排序加权：按 category、owner class path、native function name、display name 进行优先级匹配，作为 resolver 排序 bonus�?5. 新增 `search_mode`：`exact/precise` 只接�?stable id、owner-qualified native、native exact、display exact；默�?`ue_search/fuzzy` 继续允许 compact/token 匹配�?6. 新增 `ambiguity`：默�?`return_candidates`；`pick_best/best` 可显式选择最高分候选�?7. `candidate_functions` ambiguity message 已保持按目标函数分组的结构：`[{ target, candidates }]`�?8. `search_mode`、`ambiguity/ambiguity_policy`、`category_priority` 已接�?SemanticIR statement/expression 解析、fragment metadata、statement call �?expression call �?`FParsedNode`�?9. `make_struct` 移除 Vector 专用 `MakeVector` 路径，回到通用 `K2Node_MakeStruct` struct operation fragment builder�?10. 组件 `reuse_existing` 兼容别名已同步到 TypeScript compiler、Python P1 compiler �?C++ TaskPlan adapter，规范输出仍�?`reuse_if_exists`�?
验证�?1. `npm.cmd --prefix .\AgentFaceService\task-core run build` 通过�?2. `npm.cmd --prefix .\AgentFaceService\cli run build` 通过�?3. `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload` 通过�?
距离期望差距�?1. �?call TaskSpec �?`candidate_functions` 跨步骤汇总还没有�?preview 汇总层形成一个统一数组；当前单�?resolver 返回已是目标函数分组结构�?2. 类型约束评分目前仍主要依�?SemanticIR 已有类型校验和参数名，尚未完整消费每个候�?pin �?typed compatibility weight�?3. 尚未做真实编辑器端到端覆盖测试来确认 `Break Vector`、`Make Vector`、`Set Timer` 等复杂搜索场景与 UE 菜单排序完全一致�?
阻塞内容�?1. 无编译阻塞；剩余�?preview 汇总层和更完整 typed pin 评分的后续实现项�?## 20. 2026-05-16 结构�?candidate_functions 落地记录

新增内容�?1. candidate_functions 不再只依�?message 内嵌 JSON 字符串，UE dry_run issue 已增加结构化 candidate_functions: [{ target, candidates }] 字段�?2. AgentFace TaskIssueSchema 改为 passthrough，preview/execute issue 收集保留 UE 返回的扩展字段，便于 Agent 直接读取候选函数数组�?3. 候选函数来源保持通用 ActionDatabase + BlueprintActionFilter + NodeSpawner 路径，没有对 Break Vector / Make Vector 做局部特判�?
修复内容�?1. 修复 ambiguity preview 只能从自然语言 message 提取候选函数的问题，候选信息现在在 	oolResult.data.issues[].candidate_functions �?extra.issues[].candidate_functions 同步出现�?2. 保持正常成功路径不受影响：Print String 通过 ue_search preview/execute 成功，并可编译保存�?
变更需求：
1. candidate_functions 采用目标函数分组结构：[{ target: string, candidates: string[] }]，符�?TaskSpec 指定多个目标函数时继续扩展为多组的设计方向�?
快速修复：
1. 测试 TaskSpec �?Graph 名和 Custom Event 名不能相同，否则 UE 编译会报“图表已存在/无法生成另一个同名图表”�?
阻塞内容�?1. 无当前阻塞。更大规模多 call 汇总场景尚未作为独立压力测试覆盖，但主链路和单目标分组结构已验证通过�?## 21. 2026-05-16 typed pin model 与结构化候选修�?
时间�?026-05-16 14:54:29

新增内容�?1. FBlueprintHelperCallFunctionCandidateInfo 增加结构化候选字段：stable_id、display_name、owner�?ative_name、category�?ode_class、match_reason�?eturn_type、score、input_pins、world_context_pin�?arget_object_pin�?lueprint_callable�?lueprint_pure、latent�?equires_world_context�?2. candidate_functions[].candidates 的设计从字符串数组升级为结构化对象数组；message 内嵌 JSON 也同步改为对象，不再继续输出旧字符串候选�?3. FBlueprintHelperCallFunctionResolveRequest 增加 typed pin/target 约束输入：ArgumentTypes、ArgumentPinTypes、TargetObjectType、TargetObjectPinType�?4. SemanticIR statement/expression call 会把 literal/表达式类型下沉到 FParsedNode.ArgumentTypes，组�?对象 target 类型下沉�?FParsedNode.TargetObjectType�?
修复内容�?1. ActionDatabase + BlueprintActionFilter 已通过的候选不再被二次 CanFunctionBeUsedInGraph 误判�?graph-incompatible，避免挡�?PrintString �?WorldContext 静态库函数�?2. 当存�?typed target object 时，候选宇宙收敛到目标类继承链，避免组�?member call 继续全量扫描 ActionDatabase 与所�?UClass 导致 Bridge 超时�?3. 参数名过滤、参数语义类型过滤、target object 类型过滤、world context、latent、pure/callable metadata 过滤已进�?resolver 主路径�?
变更需求：
1. candidate_functions 文档中旧�?candidates: string[] 形状已被本节修订�?candidates: object[]；旧形状只作为历史记录，不再作为当前期望�?2. typed pin model 目前�?SemanticIR 已能推断出的类型为输入，不要�?AgentFace 增加大量字段�?
快速修复：
1. 无�?
验证结果�?1. Build.bat TemplateEditor Win64 Development 已编译到 BlueprintHelperCallFunctionResolver.cpp，本�?CallFunction 改动本身无编译错误�?2. 运行 smoke 中，candidate_functions 已在 preview issue 中以结构化对象出现，路径�?D:\UEProjects\Template\Saved\BlueprintHelper\CodexRuns\CallFunctionTypedPin_EventGraph_20260516_144954\candidate_preview.out.json�?3. PrintString �?typed target object 的新逻辑尚未能用�?DLL 复测，因为全量构建被 Review 系统文件阻断�?
距离期望差距�?1. 还需要在 Review 编译错误修复后重新启动编辑器，复�?PrintString、Set ambiguity、SmokeMesh.SetVisibility 三个真实 CLI 用例�?2. typed pin model 目前覆盖常见 bool/string/name/text/numeric/struct/object/class 语义类型，容�?pin、引�?pin、const/ref 权重仍只是结构字段，未做完整 UE pin-level scoring�?3. �?call TaskSpec 的跨目标 candidate_functions 汇总仍未做压力测试�?
阻塞内容�?1. 全量构建被非本任务范围的 Review 文件阻断：BlueprintHelperReviewGraphBounds.cpp 首行包含字面 `
`，导致预处理器错误和后续 FBlueprintHelperReviewGraphBoundsUtils 未识别。本任务按要求不修改 Review 系统�
## 22. 2026-05-16 去旧兼容与 ActionDatabase 主路径收敛

时间：2026-05-16 15:10:25

新增内容：
1. CallFunction 节点创建现在只接受 ActionDatabase 提供的 UBlueprintNodeSpawner 路径。
2. resolver 候选仍可补充 typed target class / blueprint class 信息用于候选诊断，但写入节点不再走旧 SetFromFunction 创建路径。
3. Logic Group 的 out_links 与 graph-level links 统一使用 GraphWriteClassificationUtils::IdentifyGraphLinkType 分类，避免 data edge 和 exec edge 双轨规则不一致。

修复内容：
1. 修复 out_links 中缺少 	ype=data 时被默认当成 exec link 的问题。
2. 修复 typed target 只有 pin type 但无法解析 UClass 时会误跳过 ActionDatabase 候选的问题。
3. 修复 candidate_functions message 内嵌 JSON 对 \r/\n/\t 等控制字符转义不足的问题。

变更需求：
1. 移除 FBlueprintGraphNodeSpawner::SpawnFunctionNode 旧 CallFunction 创建入口。
2. 移除 SpawnResolvedNode 内部 NewObject<UK2Node_CallFunction> + SetFromFunction fallback；没有 NodeSpawner 的候选会明确失败并报告不再支持 legacy fallback。

快速修复：
1. 删除未调用的旧函数声明与定义，降低后续误用概率。

验证结果：
1. E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload 通过。
2. 本次构建实际编译了 BlueprintHelperCallFunctionResolver.cpp、BlueprintGraphNodeSpawner.cpp、BlueprintHelperLogicGroupBuilder.cpp 并完成 UnrealEditor-BlueprintHelper.dll 链接。

距离期望差距：
1. 尚未运行编辑器端 CLI 覆盖测试；本轮只完成源码收敛和编译验证。
2. 如果 ActionDatabase 对某些 Blueprint 自定义函数不给 NodeSpawner，后续会按新规则明确失败，而不是旧 fallback 静默创建。

阻塞内容：
1. 无当前编译阻塞。