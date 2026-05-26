# BlueprintHelper UMGWidget 能力缺口源码探索任务

日期：2026-05-26

状态：SOURCE EXPLORATION TASK

用途：交给新线程做只读源码探索，回答“UMG 能力簇还缺少哪些能力，以及要补这些能力需要读 UE 5.6 哪些源码”。

## 范围

本任务只覆盖 BlueprintHelper 的 UMG Widget Blueprint 能力簇：

- TaskSpec / TaskPlan 的 `edit_umg_widget` 与 `umg_widget`。
- UE 侧 `UMGWidget` bridge route、service、TaskPlan adapter、runtime cluster。
- `read_context` 中与 `widget_context` 相关的读路径。
- Review / DebugBundle / overlay 中 UMG widget tree 与 widget property 的目标语义。

不纳入本任务：

- Graph body 里的 `create_widget` / `set_property` 节点能力。那属于 GraphWrite 普通节点能力，不等价于设计时 WidgetTree 修改。
- 冻结或 legacy 直连 MCP 工具本身的可用性。它们只能作为底层 bridge/service 证据，不能算 AgentFace TaskSpec 公共能力。
- 非 UE 5.6 的兼容策略。新线程应以 UE 5.6 为生产基线，只在必要时记录版本差异风险。

## 当前结论

当前公开文档和测试能稳定证明的 UMG 写能力很窄：

1. TaskSpec 能表达 `edit_umg_widget`，TaskPlan 能表达 `umg_widget`。
2. UE 侧 TaskPlan adapter / runtime 已覆盖 `add_widget`、`set_widget_property`、`remove_widget`。
3. UE 侧 bridge / service 还暴露 `get_widget_tree`、`get_widget_properties`、`move_widget`。
4. `move_widget` 仍是 bridge/service-only 能力，当前 TaskSpec contract 明确拒绝它。
5. `widget_context` 在 task-core read bridge 中能路由到 `get_widget_tree` / `get_widget_properties`，但 MCP `blueprinthelper_read_context` 的公开入口仍有“currently supports blueprint_logic only”的拦截痕迹；CLI 可达性不能从 MCP 证据外推，需要单独确认。
6. 历史设计文档提到 slot property read/write、批量 property set 等能力，但当前 service / adapter / schema / tests 没有形成完整公共链路。
7. `blueprint_get_widget_tree` 等直连 widget MCP 工具属于 frozen / legacy / expert-only 入口；它们只能证明底层 bridge command 存在，不能提升为 public TaskSpec 或 public read_context 能力。

因此，新线程应把 public TaskSpec / public read_context、bridge/service-only、frozen legacy MCP direct tool 三层严格分开。

## 当前源码证据

| 层级 | 当前证据 | 状态 | 缺口判断 |
| --- | --- | --- | --- |
| TaskSpec schema | `AgentFaceService/task-core/src/task/schema/task-schemas.ts:609` 定义 `UMGWidgetTaskSpecSchema`，`task_type` 为 `edit_umg_widget`；`changes[].kind` 是自由字符串。 | 部分公开 | 缺少强枚举、缺少 slot / move / batch 语义。 |
| TaskPlan schema | `AgentFaceService/task-core/src/task/schema/task-schemas.ts:1012` 定义 `umg_widget` step。 | 已公开 | 仅是容器 schema，不代表所有 UMG 操作可用。 |
| 能力契约 | `AgentFaceService/task-core/src/task/schema/task-contract.ts:401` 记录 `create_widget`、`update_widget_property`、`delete_widget`，并在 `task-contract.ts:416` 绑定 `umg_widget`。 | 已公开 | contract 层写明 `move_widget` rejected；slot / batch / readback 没有公共语义。 |
| 能力矩阵 | `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md:40` 只声明 add / set property / remove；`...:258` 说明 `move_widget` 底层存在但 TaskSpec reject。 | 已公开 | 文档已承认当前公共 UMG 写面较窄。 |
| Canonical TS compiler | `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:80` 附近当前分支只处理 asset、variables、object properties、signature、class settings、graph write 等类型。 | 缺口 | 没有 `edit_umg_widget` canonical TS compile branch 时，TaskSpec 到 TaskPlan 可能不可执行。 |
| Compiler registry | `AgentFaceService/task-core/src/task/compiler/task-compiler-registry.ts:10` 附近注册的 canonical task types 未见 UMG。 | 缺口 | public schema 有 UMG，但 canonical compiler 注册可能缺失。 |
| UMG templates | `AgentFaceService/agent-guide/Templates/write/taskspec_edit_umg_widget_template.json:3` 有写模板；`AgentFaceService/agent-guide/Workflows/06_UMG_Data_Workflows.md:17` 有 root / child 创建说明。 | 部分公开 | 模板示例缺少 delete / move / slot / batch，并且 `property_name` / `property_path` 语义需要统一。 |
| ReadContext bridge | `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-schemas.ts:11` 包含 `widget_context`；`read-context-route-builder.ts:113` 路由 widget read，`...:116` 到 property，`...:118` 到 tree。 | 底层存在 | 只能证明 task-core route 存在；不能直接证明 MCP / CLI public exposure。 |
| ReadContext templates | `AgentFaceService/agent-guide/Templates/read/read_context_widget_tree_template.json:3` 与 `read_context_widget_property_template.json:3` 使用 `widget_context`；`SEMANTIC_INDEX.md:49` 指向 `blueprinthelper_read_context`。 | 部分公开 | 模板存在，但仍需验证 CLI / MCP 入口是否都放行。 |
| MCP read_context | `AgentFaceService/mcp/src/mcp/tools/register-tools.ts:796` 注册 `blueprinthelper_read_context`；`...:809` 有 `read_context currently supports blueprint_logic only` 报错文本。 | 可疑 | MCP public exposure 可能挡住 `widget_context`；不要外推到 CLI。 |
| Legacy frozen MCP direct tools | `AgentFaceService/mcp/src/mcp/tools/register-tools.ts:55` 定义 `FROZEN_TOOL_PREFIX`；`:2202`、`:2221`、`:2249`、`:2269`、`:2294`、`:2314` 分别注册 `blueprint_get_widget_tree`、`blueprint_add_widget`、`blueprint_remove_widget`、`blueprint_move_widget`、`blueprint_get_widget_properties`、`blueprint_set_widget_property`。 | legacy/frozen | 只能作为 direct bridge command 证据，不能算 public TaskSpec / read_context 能力。 |
| Bridge route | `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Routes/BlueprintHelperUMGWidgetBridgeRoutes.cpp:43` 起识别 `add_widget`、`remove_widget`、`move_widget`、`set_widget_property`。 | 底层存在 | route 覆盖面宽于 TaskSpec 公共写面。 |
| Service API | `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h:89` 起声明 `GetWidgetTree`、`AddWidget`、`RemoveWidget`、`MoveWidget`、`GetWidgetProperties`、`SetWidgetProperty`。 | 底层存在 | 没有 slot property read/write、batch set、root replacement public API。 |
| Service class resolution | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.cpp:255` 附近 `FindWidgetClass` 主要按已加载 `UClass` 名称匹配。 | 可疑 | 可能缺少 Widget Blueprint generated class / asset path 加载。 |
| Service property write | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.cpp:626` 起读取 direct property；`...:679` 起设置 property。 | 部分实现 | `property_path` 目前可能只是 flat property name，不支持嵌套 struct / slot path。 |
| Root lifecycle | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.cpp:494` 阻止通过 `remove_widget` 删除 RootWidget。 | 已保护 | 缺少显式 root replace / root delete / root migration 策略。 |
| Slot DTO | `BlueprintHelper/Source/BlueprintHelper/Public/Shared/UMGWidget/BlueprintHelperWidgetTypes.h:211` 有 `FBlueprintHelperWidgetSlotProperties`；`:230` 有 `ReadWidgetSlotProperties.v1`。 | 历史/半成品 | DTO 存在，但 public service / bridge / TaskSpec / Review target / tests 不完整。 |
| Adapter ops | `BlueprintHelper/Source/BlueprintHelper/Public/Runtime/TaskRuntime/TaskPlanAdapters/UMGWidget/BlueprintHelperWidgetTaskPlanAdapter.h:25` 只列 `add_widget`、`set_widget_property`、`remove_widget`。 | 已限制 | move / slot / batch 不在 adapter 公共 op 集合。 |
| Adapter reject | `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/TaskPlanAdapters/UMGWidget/BlueprintHelperWidgetTaskPlanAdapter.cpp:574` 返回 `unsupported_umg_widget_op`。 | 已限制 | 当前明确只支持三种 op。 |
| Adapter tests | `BlueprintHelper/Source/BlueprintHelper/Private/Tests/UMGWidget/BlueprintHelperTaskPlanWidgetAdapterTests.cpp:146`、`:214`、`:267` 覆盖 add / set / remove lowering；`:292`、`:322`、`:351` 覆盖 dry-run；`:406` 覆盖旧 `operation` 字段拒绝。 | 已验证 | 缺少 move / slot / batch / read_context 端到端测试。 |
| Runtime tests | `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp:3540` 与 `:3599` 覆盖 UMG runtime execute / dry-run planned widget state。 | 已验证 | runtime 覆盖仍只围绕现有写面。 |
| Review tests | `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp:2255` 起有 `umg_widget` target；`:2311` 起有 `umg_widget_property` target。 | 部分验证 | 未见 slot target kind；DebugBundle / overlay / AcceptReject 是否同一模型消费需要继续核对。 |

## 缺失能力矩阵

| 优先级 | 能力缺口 | 当前状态 | 新线程要确认的问题 |
| --- | --- | --- | --- |
| P0 | `edit_umg_widget` canonical TS compile path | schema / contract 有，compiler registry 可能缺。 | TaskSpec 是否能从 CLI / preview / execute 正常 compile 到 `umg_widget` TaskPlan？如果不能，缺哪些 compiler 文件和 tests？ |
| P0 | 严格公共 schema | `changes[].kind` 是自由字符串，target 仍偏 generic。 | 是否应把 create / update / delete 枚举化，并区分 `widget_blueprint` target？是否需要拒绝未知字段或只在 adapter reject？ |
| P0 | `widget_context` MCP public exposure | task-core read bridge 和 read templates 支持，MCP 入口有疑似 blueprint_logic-only 限制。 | MCP `blueprinthelper_read_context` 是否能读 WidgetTree 和单 widget properties？如果不能，拦截点和 schema/help/capabilities 的不一致在哪里？ |
| P0 | `widget_context` CLI public exposure | task-core route 存在，CLI 可达性未由 MCP 证据证明。 | CLI `bh blueprinthelper_read_context` 是否能通过 widget tree/property 模板？CLI help、template index、capabilities 输出是否一致？ |
| P1 | `move_widget` 公共能力 | bridge / service 有，TaskSpec contract reject，adapter 无 op。 | 是否应升级为 TaskSpec 能力？需要哪些 parent / index / named slot 语义、dry-run、review evidence、rollback 信息？ |
| P1 | slot property read/write | DTO 有，历史文档有，service / adapter / route 证据不足。 | 需要补 `read_widget_slot_properties`、`set_widget_slot_property`、`set_widget_slot_properties` 吗？UE slot 类型如何泛化？ |
| P1 | 批量 widget/property 操作 | 当前 adapter 假设 step 中单个 op。 | 是否要支持 `set_widget_properties` 和多 widget batch？事务、dry-run、partial failure、Review grouping 如何表达？ |
| P1 | 嵌套 `property_path` | adapter 接受 `property_name` 或 `property_path`，service 似乎按 flat property 查找。 | 是否需要像 UObject property path 一样支持 struct / nested / array / map？和 slot path 是否同一套 resolver？ |
| P1 | root lifecycle 策略 | remove root 被禁止。 | 是否应显式支持 set root / replace root / delete root？对应 UE source 中 root replacement 有哪些约束？ |
| P1 | widget class path resolution | service 主要按已加载 class 名查找。 | 是否需要支持 `/Game/...WBP_X.WBP_X_C`、Blueprint asset path、native class path、soft class loading？ |
| P1 | name collision / rename / replace | 当前 add / remove / move 之外没有清晰 rename/replace。 | UE 对 duplicate widget name、generated variable name、BindWidget 字段如何约束？ |
| P2 | WidgetTree readback verifier | 当前 UMG runtime tests 可执行，但缺专门 readback verifier。 | 是否需要类似 GraphWrite readback 的 widget existence / parent / slot / property verifier？ |
| P2 | Review target kind 扩展 | 已见 `umg_widget`、`umg_widget_property`。 | 是否需要 `umg_widget_slot`、`umg_widget_binding`、`umg_widget_animation`、`umg_named_slot`？ |
| P2 | property binding | `UWidgetBlueprint::Bindings` 是 UE 层能力。 | 是否应纳入 UMG 能力簇，还是另开绑定能力簇？需要读哪些 compiler validation？ |
| P2 | animation binding / WidgetAnimation | `UWidgetBlueprint::Animations` 与 `UWidgetAnimation` 独立存在。 | 是否只读展示，还是支持创建/绑定/删除？TaskSpec target 如何建模？ |
| P2 | named slot content | UE 支持 named slot host / content 查找与替换。 | 是否要把 named slot 当 parent 语义、slot 语义，还是专门 op？ |
| P2 | navigation / visibility / brush / style 等复杂属性 | 当前 flat property write 可能可覆盖一部分。 | 哪些属性可用文本导入安全写，哪些需要专门 resolver / builder？ |
| P2 | UMG validation / compile verdict | 当前结果主要是 `WidgetMutation.v1` 和 operation-level error。 | 是否需要 UMG 专属 validation / compile verdict schema，还是复用现有 TaskRuntime diagnostics？ |

## UE 5.6 源码阅读清单

以下路径以 UE 5.6 Engine 根目录为基准。

### WidgetTree 与基础树修改

- `Engine/Source/Runtime/UMG/Public/Blueprint/WidgetTree.h`
  - 重点：`RemoveWidget`、`FindWidgetParent`、`ConstructWidget`、`RootWidget`、`NamedSlotBindings`。
- `Engine/Source/Runtime/UMG/Private/WidgetTree.cpp`
  - 重点：树遍历、remove 行为、named slot binding 行为、root / child 关系更新。
- `Engine/Source/Editor/UMGEditor/Private/WidgetBlueprintEditorUtils.cpp`
  - 重点：editor 中 remove、move、replace、paste、root 替换、named slot 处理。
- `Engine/Source/Editor/UMGEditor/Public/WidgetBlueprintEditorUtils.h`
  - 重点：`FindNamedSlotHostForContent`、`RemoveNamedSlotHostContent`、`ReplaceNamedSlotHostContent`、`IsBindWidgetProperty`、`IsBindWidgetAnimProperty`。

### WidgetBlueprint 数据模型与编译校验

- `Engine/Source/Editor/UMGEditor/Public/WidgetBlueprint.h`
  - 重点：`FDelegateEditorBinding`、`Bindings`、`Animations`、named slot helper、tickability stats。
- `Engine/Source/Editor/UMGEditor/Private/WidgetBlueprintCompiler.cpp`
  - 重点：root hierarchy、BindWidget、BindWidgetAnim、animation、property binding 的 compiler validation。
- `Engine/Source/Editor/UMGEditor/Private/WidgetBlueprintFactory.cpp`
  - 重点：新 Widget Blueprint 默认 root 创建策略。
- `Engine/Source/Runtime/UMG/Private/WidgetBlueprintGeneratedClass.cpp`
  - 重点：`InitializeWidgetStatic`、`InitializeBindingsStatic`、`BindAnimationsStatic`、`GetNamedSlotArchetypeContent`。

### Slot / panel / layout 属性

- `Engine/Source/Runtime/UMG/Public/Components/PanelWidget.h`
- `Engine/Source/Runtime/UMG/Public/Components/PanelSlot.h`
- `Engine/Source/Runtime/UMG/Public/Components/CanvasPanelSlot.h`
- `Engine/Source/Runtime/UMG/Public/Components/NamedSlot.h`
- `Engine/Source/Runtime/UMG/Public/Components/NamedSlotInterface.h`

重点问题：

- 不同 `UPanelSlot` 子类的属性能否用统一 reflection path 写入？
- `CanvasPanelSlot` 之类常用 slot 是否需要专门 schema，还是通用 `slot.property_path` 足够？
- named slot content 与 panel slot child 在 UE 语义上是否应统一建模？

### Binding / animation / generated variables

- `Engine/Source/Runtime/UMG/Public/Animation/WidgetAnimation.h`
- `Engine/Source/Runtime/UMG/Public/Animation/WidgetAnimationBinding.h`
- `Engine/Source/Runtime/UMG/Public/Animation/WidgetAnimationDelegateBinding.h`
- `Engine/Source/Runtime/UMG/Public/Components/Widget.h`
- `Engine/Source/Editor/UMGEditor/Private/Widgets/SBindWidgetView.cpp`

重点问题：

- `BindWidget` / `BindWidgetOptional` 对 widget name 和 generated variable 有哪些硬约束？
- `BindWidgetAnim` 与 `UWidgetBlueprint::Animations` 如何同步？
- 创建 / 删除 widget 时，Bindings / Animations / generated variable GUID 是否需要联动清理？

## BlueprintHelper 源码阅读清单

### AgentFace task-core / CLI / MCP

```powershell
rg -n "edit_umg_widget|UMGWidgetTaskSpecSchema|UMGWidgetTaskPlanStepSchema|umg_widget" AgentFaceService/task-core/src
rg -n "widget_context|get_widget_tree|get_widget_properties|blueprint_logic only" AgentFaceService/task-core/src AgentFaceService/mcp/src AgentFaceService/cli/src
rg -n "taskspec_edit_umg_widget|edit_umg_widget|move_widget|delete_widget|property_path|property_name" AgentFaceService/agent-guide AgentFaceService/docs
rg -n "FROZEN|blueprint_get_widget_tree|blueprint_add_widget|blueprint_remove_widget|blueprint_move_widget|blueprint_get_widget_properties|blueprint_set_widget_property" AgentFaceService/mcp/src/mcp/tools/register-tools.ts
```

必须回答：

- `edit_umg_widget` 是否在 canonical TS compiler registry 中注册？
- compiler 是否能把每种 `changes[].kind` 降到 `umg_widget` TaskPlan？
- preview / execute / CLI / MCP 是否都经过同一条 compile policy？
- MCP `blueprinthelper_read_context` 是否放行 `widget_context`，还是仍被 blueprint_logic-only 逻辑拦截？
- CLI `bh blueprinthelper_read_context` 是否放行 `widget_context`，不要用 MCP 结果替代 CLI 结果。
- `widget_context` read_context 的 public template、capabilities、help、MCP schema、CLI behavior 是否一致？
- frozen / legacy direct widget MCP tools 是否只作为 bridge command 证据记录，没有混入 public capability 结论？

### UE plugin UMG service / bridge / runtime

```powershell
rg -n "GetWidgetTree|AddWidget|RemoveWidget|MoveWidget|GetWidgetProperties|SetWidgetProperty|SlotProperties|ReadWidgetSlotProperties" BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/UMGWidget BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/UMGWidget BlueprintHelper/Source/BlueprintHelper/Public/Shared/UMGWidget
rg -n "add_widget|remove_widget|move_widget|get_widget_tree|get_widget_properties|set_widget_property|unsupported_umg_widget_op" BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Routes BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/TaskPlanAdapters/UMGWidget BlueprintHelper/Source/BlueprintHelper/Public/Runtime/TaskRuntime/TaskPlanAdapters/UMGWidget
rg -n "umg_widget|umg_widget_property|UMGWidgetTree|slot" BlueprintHelper/Source/BlueprintHelper/Private/Tests BlueprintHelper/Source/BlueprintHelper/Public BlueprintHelper/Source/BlueprintHelper/Private
```

必须回答：

- service、bridge route、TaskPlan adapter、runtime cluster 的 op 集合是否一致？
- 每个 op 是否有 dry-run、execute、review evidence、rollback / reject evidence？
- slot DTO 是否还有调用者，还是孤立遗留结构？
- `property_path` 是否真正支持路径，还是只被转成 property name？
- `MoveWidget` 的 rollback / planned state / review target 是否足以公共化？

### UE 5.6 engine source

```powershell
rg -n "RemoveWidget|FindWidgetParent|ConstructWidget|RootWidget|NamedSlotBindings" Engine/Source/Runtime/UMG/Public/Blueprint Engine/Source/Runtime/UMG/Private
rg -n "Move|Replace|Remove|NamedSlot|RootWidget|FindNamedSlotHost|IsBindWidgetProperty|IsBindWidgetAnimProperty" Engine/Source/Editor/UMGEditor/Public Engine/Source/Editor/UMGEditor/Private
rg -n "FDelegateEditorBinding|Bindings|Animations|Generated|NamedSlots" Engine/Source/Editor/UMGEditor/Public/WidgetBlueprint.h Engine/Source/Editor/UMGEditor/Private
rg -n "class UPanelSlot|class UCanvasPanelSlot|SynchronizeFromTemplate|NamedSlot" Engine/Source/Runtime/UMG/Public/Components
```

必须回答：

- UE editor 对 move / replace / root replacement 的真实实现顺序是什么？
- 哪些操作必须走 `FWidgetBlueprintEditorUtils` 才能保持 editor / compiler / generated class 状态一致？
- Slot property 写入是否应通过 `UPanelSlot` reflection，还是要 per-slot adapter？
- 删除 / 移动 widget 时，named slot、binding、animation、generated variable GUID 是否需要同步清理？

## 建议输出格式

新线程最终输出一个 Markdown 表格，至少包含以下列：

| capability | requested/public name | evidence_layer | status | current evidence | missing files | UE source to read | proposed owner boundary | required tests | notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |

`evidence_layer` 只能使用这些值：

- `public_taskspec`
- `public_read_context`
- `bridge_service_only`
- `legacy_frozen_mcp_direct`
- `historical_only`
- `engine_source_only`
- `review_model`

`legacy_frozen_mcp_direct` 不能提升 public 能力结论；它只能解释 direct tool 和 bridge command 是否存在。

`status` 只能使用这些值：

- `supported`
- `public_missing`
- `bridge_only`
- `historical_only`
- `implemented_not_tested`
- `explicitly_excluded`
- `needs_engine_source_decision`

`proposed owner boundary` 应优先落在这些边界之一：

- task-core schema / compiler / templates
- UMGWidget service
- UMGWidget TaskPlan adapter
- UMGWidget bridge route
- read_context route / MCP schema
- Review model / presenter / target registry
- shared property path resolver

不要建议把 workflow、async、lifecycle、Review 状态、readback verifier 逻辑塞进 UI widget。

## 最终判定标准

新线程完成探索前，不能只给概念判断。每个结论必须至少有一条当前 BlueprintHelper 源码证据；如果建议补能力，还必须给对应 UE 5.6 engine source 证据或明确说明仍需 engine source 决策。

完成条件：

1. 明确列出 UMG 当前公共可用能力、底层已实现但未公开能力、历史提到但当前缺链路能力。
2. 单独判定 `edit_umg_widget` canonical TS compile 缺口是否真实存在。
3. 单独判定 `widget_context` read_context 的 MCP public exposure 是否真实可用。
4. 单独判定 `widget_context` read_context 的 CLI public exposure 是否真实可用。
5. 单独判定 `move_widget` 是否可以升级到 TaskSpec，还是应继续显式排除。
6. 单独判定 slot property read/write 应进入 UMGWidget service，还是进入通用 property path resolver。
7. 给出最小补齐路线，但不要写实现代码。
8. 给出需要新增或扩展的测试列表，覆盖 TS schema/compiler、C++ adapter/service、runtime dry-run/execute、read_context、Review target。
9. 明确不把 legacy direct MCP widget tools 当成 TaskSpec 或 read_context 公共能力证据。
