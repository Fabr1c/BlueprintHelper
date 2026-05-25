# GraphWrite Field UE Editor Capability Engine Source Read Task

日期：2026-05-25

## 目标

为 GraphWrite `get` / `set` / `get_property` / `set_property` / `component_ref` / `field_access` 的完全通用化实现提供 UE 5.6 源码证据，明确普通 Blueprint `EventGraph` / `FunctionGraph` 内编辑器可创建、可拖拽、可连接的 Field/Variable/Property 节点能力边界，并产出一份可用于后续实现的 80% 编辑器操作覆盖矩阵。

本任务只做只读源码探索和证据表导出，不修改 BlueprintHelper、AgentFaceService 或 UE 源码。

## 根目录约定

本任务所有检索均以 UE 5.6 引擎目录作为根目录执行：

```powershell
$EngineRoot = "E:\UE_5.6\Engine"
Set-Location $EngineRoot
```

文档、结果表和引用中的 UE 源码路径必须使用 `Source/...` 相对路径，例如：

```text
Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp:123
```

不要在结果表中写入 `D:\UEProjects\Template\Plugins\BlueprintHelper` 作为检索根；BlueprintHelper 只用于存放最终结果文档。

## 输出文件

新线程完成探索后输出到：

```text
BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_Field_UEEditorCapability_EngineSourceReadResult_20260525_CN.md
```

输出必须包含：

1. 证据表：每个编辑器可见操作一行。
2. 80% 覆盖建议：哪些操作进入 first-class GraphWrite Field 能力，哪些保留现有工具职责，哪些作为诊断不生成。
3. 实现边界建议：需要 TaskSpec、ActionContext projection、ActionResolution、FragmentBuilder、readback 哪些证据。
4. 风险清单：会影响缓存复用、typed pin 推断、linked pin、编译诊断、Review/DebugBundle 的真实缺口。

## 探索范围

### 纳入范围

| 能力 | 说明 |
| --- | --- |
| `get` | 蓝图成员变量、本地变量、函数参数、组件变量的读取节点。 |
| `set` | 蓝图成员变量、本地变量、可写属性、组件变量的写入节点。 |
| `get_property` | 对对象或结构体的属性路径读取，包括 split pin、break struct、对象成员读取。 |
| `set_property` | 对对象或结构体的属性路径写入，包括 set members in struct、变量 set、必要的 make/break 组合。 |
| `component_ref` | Actor Blueprint 中 SCS/native/inherited component 的图内引用节点。 |
| `field_access` | 从 typed object pin、self、component pin 或返回值继续访问成员字段的图内节点链。 |

### 排除范围

| 项 | 原因 |
| --- | --- |
| Class Defaults / Details 面板属性编辑 | 不是 GraphWrite graph body 节点生成职责，保留现有非 GraphWrite 工具职责。 |
| DataAsset / DataTable / asset property 写入 | 属于资产内容编辑，不属于普通 Blueprint graph body Field 节点生成。 |
| UMG Widget Designer 层级属性编辑 | 不是普通 Blueprint `EventGraph` / `FunctionGraph` 图内 Field 生成。 |
| Animation Blueprint 专属图和 anim node 属性 | 不属于当前 GraphWrite 普通 Blueprint 范围。 |
| 函数调用、容器操作、make/break first-class 语义 | 本任务只判断它们是否作为 property path 实现依赖，不重新归属 FunctionAction / ContainerAction。 |

## 任务 1：Action Menu 与右键/拖拽入口枚举

目标：确认普通 Blueprint 图内 Field/Variable/Property 节点是如何进入 Action Menu、拖拽菜单和 pin-drag 菜单的。

读取路径：

```text
Source/Editor/BlueprintGraph/Public/BlueprintActionDatabase.h
Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp
Source/Editor/BlueprintGraph/Public/BlueprintActionFilter.h
Source/Editor/BlueprintGraph/Private/BlueprintActionFilter.cpp
Source/Editor/BlueprintGraph/Private/BlueprintActionFilterResultCache.cpp
Source/Editor/BlueprintGraph/Classes/EdGraphSchema_K2.h
Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp
Source/Editor/BlueprintGraph/Classes/EdGraphSchema_K2_Actions.h
Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2_Actions.cpp
Source/Editor/Kismet/Public/BPVariableDragDropAction.h
Source/Editor/Kismet/Private/BPVariableDragDropAction.cpp
```

建议检索：

```powershell
rg -n "GetGraphContextActions|GetContextMenuActions|GetVariable|get variable|set variable|PinType|FromPin|DragDrop|FBlueprintActionFilter|FBlueprintActionDatabase" Source/Editor/BlueprintGraph Source/Editor/Kismet -g "*.h" -g "*.cpp"
rg -n "BlueprintVariableNodeSpawner|BlueprintComponentNodeSpawner|FEdGraphSchemaAction_K2|FBlueprintActionFilter|FBlueprintActionDatabase" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
rg -n "CanPromotePin|PromotePin|SplitPin|RecombinePin|CreateSplitPinNode|GetLocalVariables" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
```

交付内容：

| 字段 | 说明 |
| --- | --- |
| `editor_entry` | 右键空白图、从变量面板拖拽、从 pin 拖拽、从 My Blueprint 菜单等。 |
| `filter_context` | 需要 graph type、pin type、owner blueprint、member scope、self context 中哪些上下文。 |
| `spawner_class` | 例如 `UBlueprintVariableNodeSpawner`、`UBlueprintComponentNodeSpawner`。 |
| `node_class` | 例如 `UK2Node_VariableGet`、`UK2Node_VariableSet`、`UK2Node_Self`。 |
| `required_graphwrite_evidence` | GraphWrite 要复现该行为所需最小证据。 |
| `source_path` | `Source/...:line`。 |

## 任务 2：变量、本地变量、参数、返回值覆盖

目标：分清 UE 对成员变量、本地变量、函数参数、返回值的节点表达方式和 `FMemberReference` / scope 证据要求。

读取路径：

```text
Source/Editor/BlueprintGraph/Classes/BlueprintVariableNodeSpawner.h
Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_Variable.h
Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_VariableGet.h
Source/Editor/BlueprintGraph/Private/K2Node_VariableGet.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_VariableSet.h
Source/Editor/BlueprintGraph/Private/K2Node_VariableSet.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_VariableSetRef.h
Source/Editor/BlueprintGraph/Private/K2Node_VariableSetRef.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_Self.h
Source/Editor/BlueprintGraph/Private/K2Node_Self.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_FunctionEntry.h
Source/Editor/BlueprintGraph/Private/K2Node_FunctionEntry.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_FunctionResult.h
Source/Editor/BlueprintGraph/Private/K2Node_FunctionResult.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_LocalVariable.h
Source/Editor/BlueprintGraph/Private/K2Node_LocalVariable.cpp
Source/Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h
Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp
```

建议检索：

```powershell
rg -n "CreateFromMemberOrParam|CreateFromLocalVariable|CreateFromField|FMemberReference|VariableReference|MemberParent|MemberName|LocalVariables|FindLocalVariable|FindLocalVariableGuidByName" Source/Editor/BlueprintGraph Source/Editor/UnrealEd -g "*.h" -g "*.cpp"
rg -n "GetBlueprintVarDescription|IsLocalVariable|VariableScope|GetLocalVariables|UK2Node_FunctionEntry|UK2Node_FunctionResult" Source/Editor/BlueprintGraph Source/Editor/Kismet -g "*.h" -g "*.cpp"
rg -n "FindPropertyByName|FindFProperty|SkeletonGeneratedClass|GeneratedClass|ParentClass|NewVariables" Source/Editor/BlueprintGraph Source/Editor/UnrealEd -g "*.h" -g "*.cpp"
```

交付内容：

| 操作 | 必须判断的问题 |
| --- | --- |
| member variable get/set | 是否只需要 variable name，还是必须保存 owner class、member guid、scope。 |
| inherited/native member get/set | 是否通过 parent class `FProperty` 进入，和 Blueprint `NewVariables` 的差异。 |
| local variable get/set | scope 是 function graph、`UK2Node_FunctionEntry` 还是 `UFunction`，GraphWrite 如何提供。 |
| function input parameter get | 参数是否作为 variable node、entry pin、还是特殊 member reference。 |
| function output/return set | 是否应归 `return` control 节点，而不是 Field set。 |
| by-ref variable set | `UK2Node_VariableSetRef` 是否属于普通 Field set 覆盖范围。 |

## 任务 3：component_ref 真实编辑器范围

目标：确认组件引用节点覆盖 SCS 组件、inherited SCS 组件、native inherited component、component template，以及它们在普通图内的 spawner 和 evidence。

读取路径：

```text
Source/Editor/BlueprintGraph/Classes/BlueprintComponentNodeSpawner.h
Source/Editor/BlueprintGraph/Private/BlueprintComponentNodeSpawner.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_VariableGet.h
Source/Editor/BlueprintGraph/Private/K2Node_VariableGet.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_VariableSet.h
Source/Editor/BlueprintGraph/Private/K2Node_VariableSet.cpp
Source/Editor/Kismet/Public/SSCSEditor.h
Source/Editor/Kismet/Private/SSCSEditor.cpp
Source/Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h
Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp
Source/Runtime/Engine/Classes/Engine/SimpleConstructionScript.h
Source/Runtime/Engine/Classes/Engine/SCS_Node.h
```

建议检索：

```powershell
rg -n "BlueprintComponentNodeSpawner|CreateFromComponent|ComponentProperty|ComponentTemplate|SCS_Node|SimpleConstructionScript|FindSCS_Node|GetSCSVariableNameList|IsSCSComponentProperty|FindUCSComponentTemplate" Source/Editor/BlueprintGraph Source/Editor/Kismet Source/Editor/UnrealEd Source/Runtime/Engine -g "*.h" -g "*.cpp"
rg -n "NativeComponent|InheritedSCS|InheritableComponentHandler|GetOverridenComponentTemplate|ComponentClassOverrides|ComponentTemplates" Source/Editor/Kismet Source/Editor/UnrealEd Source/Runtime/Engine -g "*.h" -g "*.cpp"
rg -n "GetVariableName\\(|GetActualComponentTemplate|GetOrCreateEditableComponentTemplate|FindChild\\(" Source/Editor/Kismet Source/Runtime/Engine -g "*.h" -g "*.cpp"
```

交付内容：

| 字段 | 说明 |
| --- | --- |
| `component_kind` | SCS、inherited SCS、native inherited、component template、instance-only。 |
| `ordinary_blueprint_graph_visible` | 是否可在普通 `EventGraph` / `FunctionGraph` 右键或拖拽创建。 |
| `spawner_class` | 组件 spawner 或 variable spawner。 |
| `member_reference_shape` | name、guid、owner class、component key、template path 中哪些是稳定证据。 |
| `graphwrite_scope` | `component_ref` 支持、诊断、或排除。 |

## 任务 4：field_access 与 object-pin 成员访问边界

目标：确认从 typed object pin、`self`、component output、函数返回对象继续访问成员字段时，UE 是创建变量节点、call function on member、还是通过 Action Menu 过滤选择 callable/member node。

读取路径：

```text
Source/Editor/BlueprintGraph/Classes/K2Node_CallFunctionOnMember.h
Source/Editor/BlueprintGraph/Private/K2Node_CallFunctionOnMember.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_CallFunction.h
Source/Editor/BlueprintGraph/Private/K2Node_CallFunction.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_VariableGet.h
Source/Editor/BlueprintGraph/Private/K2Node_VariableGet.cpp
Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp
Source/Editor/BlueprintGraph/Private/BlueprintFunctionNodeSpawner.cpp
Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp
```

建议检索：

```powershell
rg -n "CallFunctionOnMember|MemberVariableToCallOn|TargetPin|SelfContext|bSelfContext|FromPin|TargetObject|MemberReference|GetField|Property" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
rg -n "TargetGraph|TargetPin|PinType|ActionFilter|ContextTarget|OwnerClass|ClassFilter|MemberParent" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
rg -n "CreateFromMemberOrParam|CustomizeNodeDelegate|NodeSpawner|Invoke|SetFromField" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
```

交付内容：

| 问题 | 输出要求 |
| --- | --- |
| typed object pin 访问成员变量 | 列出可行 spawner、node class、所需 pin type evidence。 |
| component pin 访问成员属性 | 判断是 `field_access` 还是 `component_ref + property_path`。 |
| 函数返回对象访问成员 | 判断是否需要中间 temp/reference 节点。 |
| 成员函数和成员字段重叠 | 明确 Field 与 FunctionAction 的分界。 |
| ambiguous member name | 需要 owner class、member guid、pin type 还是 stable field path 才能消歧。 |

## 任务 5：复杂 property path 与 struct member 实现方式

目标：确认 `Object.Property.SubProperty`、结构体成员 get/set、split pin、make/break struct、set members in struct 的真实编辑器行为，并判断 GraphWrite 是应生成节点链、使用 split pins，还是专门的 property path fragment。

读取路径：

```text
Source/Editor/BlueprintGraph/Classes/K2Node_MakeStruct.h
Source/Editor/BlueprintGraph/Private/K2Node_MakeStruct.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_BreakStruct.h
Source/Editor/BlueprintGraph/Private/K2Node_BreakStruct.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_SetFieldsInStruct.h
Source/Editor/BlueprintGraph/Private/K2Node_SetFieldsInStruct.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_StructOperation.h
Source/Editor/BlueprintGraph/Private/K2Node_StructOperation.cpp
Source/Editor/BlueprintGraph/Private/MakeStructHandler.h
Source/Editor/BlueprintGraph/Private/MakeStructHandler.cpp
Source/Editor/BlueprintGraph/Classes/EdGraphSchema_K2.h
Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp
Source/Editor/BlueprintGraph/Classes/K2Node.h
Source/Editor/BlueprintGraph/Private/K2Node.cpp
```

建议检索：

```powershell
rg -n "SplitPin|RecombinePin|CreateSplitPinNode|RestoreSplitPins|ExpandSplitPin|CanSplitPin|PinHasSplittableStructType" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
rg -n "UK2Node_MakeStruct|UK2Node_BreakStruct|UK2Node_SetFieldsInStruct|K2Node_StructOperation|Set members in|SetFieldsInStruct" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
rg -n "CanBeMade|CanBeBroken|CanBeSplit|ShowCustomPinActions|RestoreAllPins|RemoveFieldPins|OptionalPin" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
rg -n "MD_NativeDisableSplitPin|MakeStructureDefaultValue|HasMetaData|BlueprintType" Source/Editor/BlueprintGraph Source/Runtime -g "*.h" -g "*.cpp"
```

交付内容：

| property path 类型 | 预期 UE 表达 | GraphWrite 建议 |
| --- | --- | --- |
| simple member variable | `UK2Node_VariableGet/Set` | `get/set`。 |
| struct read member | split pin 或 `UK2Node_BreakStruct` | `get_property` 生成 read fragment。 |
| struct write member | `UK2Node_SetFieldsInStruct` 或 make/set chain | `set_property` 生成 write fragment。 |
| nested struct path | 多级 split/make/break/set chain | 需要 dedicated property path fragment。 |
| object property path | object target + member variable/call chain | `field_access` 或 `property_path`，由证据决定。 |
| unsupported/private/readonly path | 编译诊断或 ActionFilter 不可见 | GraphWrite 失败并返回 reason。 |

## 任务 6：readback、typed pin、linked pin 与成功判定

目标：定义运行后如何证明 Field 节点成功生成，而不是只看 resolver 找到候选。

读取路径：

```text
Source/Editor/BlueprintGraph/Classes/EdGraphSchema_K2.h
Source/Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp
Source/Editor/BlueprintGraph/Classes/K2Node.h
Source/Editor/BlueprintGraph/Private/K2Node.cpp
Source/Editor/BlueprintGraph/Classes/K2Node_Variable.h
Source/Editor/BlueprintGraph/Private/K2Node_Variable.cpp
Source/Editor/BlueprintGraph/Private/VariableSetHandler.cpp
Source/Editor/KismetCompiler/Public/KismetCompiler.h
Source/Editor/KismetCompiler/Private/KismetCompiler.cpp
```

建议检索：

```powershell
rg -n "CanCreateConnection|TryCreateConnection|ArePinsCompatible|PinType|LinkedTo|DefaultValue|AutogeneratedDefaultValue|SubPins|ParentPin" Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
rg -n "ValidateNodeDuringCompilation|MessageLog\\.Error|MessageLog\\.Warning|VariableSetHandler|LocalVariableNotFound|UnknownStructure" Source/Editor/BlueprintGraph Source/Editor/KismetCompiler -g "*.h" -g "*.cpp"
rg -n "FMemberReference|GetMemberName|GetMemberParentClass|ResolveMember|ResolveSimpleMemberReference" Source/Editor/BlueprintGraph Source/Runtime -g "*.h" -g "*.cpp"
```

交付内容：

| evidence | 通过标准 |
| --- | --- |
| node class | 节点类必须匹配预期族，例如 `UK2Node_VariableGet`、`UK2Node_SetFieldsInStruct`。 |
| member reference | `FMemberReference` 能解析到目标 property、scope 或 component。 |
| pin type | 数据 pin 类型与 TaskSpec/linked typed pin 推断一致。 |
| linked pin | 需要连接的输入/输出 pin 已连接到预期节点。 |
| default value | 未连接的 literal/default value 与 TaskSpec 一致。 |
| split state | split/sub pin 结构与 property path 一致。 |
| compile result | Blueprint 编译无与该 statement 相关的 error；warning 需要进入 DebugBundle。 |

## 任务 7：80% 覆盖矩阵与实现优先级

目标：按普通 Blueprint 用户常见操作频率和 GraphWrite 架构价值，确定 first-class Field 能力优先级，避免为了低频特殊入口硬编码。

建议检索：

```powershell
rg -n "Variable|Local Variable|Get|Set|Component|Split Struct Pin|Break Struct|Make Struct|Set members" Source/Editor/Kismet Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
rg -n "MenuBuilder\\.AddMenuEntry|AddMenuEntry|FBlueprintEditorCommands|FMyBlueprintCommands|GetGraphContextActions" Source/Editor/Kismet Source/Editor/BlueprintGraph -g "*.h" -g "*.cpp"
```

输出矩阵字段：

| 字段 | 说明 |
| --- | --- |
| `capability_id` | 例如 `field.member_get`、`field.local_set`、`field.component_ref_get`、`field.struct_member_set`。 |
| `editor_operation` | 编辑器中用户看到或执行的操作名称。 |
| `graph_scope` | `EventGraph`、`FunctionGraph`、或二者均可。 |
| `node_family` | variable、component、struct make/break/set members、split pin、call on member。 |
| `ue_node_class` | 具体 UE 节点类。 |
| `spawner_or_api` | NodeSpawner、schema split pin API、或 graph mutation API。 |
| `minimum_task_spec_context` | TaskSpec 需要提供的最小上下文。 |
| `action_context_projection` | 可以从 Blueprint/graph/linked pin 自动投影的证据。 |
| `readback_evidence` | 运行后可验证证据。 |
| `coverage_priority` | `P0` / `P1` / `P2` / `excluded`。 |
| `owner_cluster` | FieldVariableAction、FunctionAction、ContainerAction、existing non-GraphWrite tool、excluded。 |
| `reason` | 纳入或排除理由。 |
| `source_path` | `Source/...:line`。 |

优先级规则：

| 优先级 | 纳入条件 |
| --- | --- |
| `P0` | 普通蓝图最常见且当前 GraphWrite Field 主线必须支持：member get/set、local get/set、component ref get、simple property get/set。 |
| `P1` | 为 80% 编辑器能力必要：inherited/native member、function parameter get、struct member get/set、linked typed pin 推断。 |
| `P2` | 通用化重要但实现风险较高：nested property path、object-pin member access、split pin roundtrip、by-ref set。 |
| `excluded` | 非图内节点生成、动画/UMG 专属、asset property 编辑、或应由现有非 GraphWrite 工具保留职责。 |

## 最终结论模板

新线程结果文档末尾必须给出以下结论：

| 结论项 | 写法要求 |
| --- | --- |
| UE 5.6 普通 Blueprint Field/Variable/Property 编辑器操作总数 | 写实际数字，并说明计数规则。 |
| 建议 GraphWrite first-class 覆盖数量 | 写实际数字，并列出纳入的 capability id。 |
| 可达到的编辑器操作覆盖比例 | 写实际百分比，并说明分母是否排除了非 GraphWrite 职责。 |
| 仍由现有非 GraphWrite 工具保留职责的数量 | 写实际数字，并列出 owner tool 或 owner cluster。 |
| 当前 BlueprintHelper Field 实现与目标覆盖之间的真实缺陷 | 写源码证据支持的缺陷列表，每项包含 source path。 |
| 后续实现前必须讨论的架构决策 | 写需要用户决策的问题列表，每项包含影响范围。 |

填表时必须写源码证据支持的实际结论，不使用空泛或未落证据的描述。

## 后续实现边界

源码探索结果用于后续 GraphWrite Field 通用化实现，但本任务不实现。后续实现必须遵守：

1. `get/set/get_property/set_property/component_ref/field_access` 继续属于 FieldVariableActionCluster，不通过 FunctionAction 或 legacy parsed API 伪装成功。
2. ActionContext projection 可以补齐 Blueprint、graph、self class、target pin type、linked pin、component metadata，但不能硬编码单个变量名或单个组件名。
3. property path 需要 dedicated fragment/readback 时，应扩展 builder/composer 边界，不把多级路径拆成散落的特殊分支。
4. component_ref 只处理普通 Actor Blueprint 图内可引用组件；组件资产模板或 Details 面板属性编辑保留给现有工具。
5. 成功判定必须基于 node class、member reference、pin type、link/default、split state 和 compile diagnostics，不能只依赖 resolver 命中。
6. Review evidence 保持 graph_block 级别；Field 操作细节进入 DebugBundle/readback facts，不新增 per-field Review target 粒度。
