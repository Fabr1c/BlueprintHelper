# UE NodeSpawner 右键搜索与精确 Spawn 链路分析

日期：2026-05-21  
项目：BlueprintHelper / GraphStatement Framework  
输入：`BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`、`引擎源码.7z`  
输出目标：把 UE 编辑器右键搜索到确切节点类型的路径，压缩为 `类::函数 [函数内调用] -> 下一个函数` 的嵌套结构，并映射到文档内四大 Spawner-Oriented Clusters 与 SemanticConstraints。

> 注意：本文只给出源码调用链、路径和行号引用，不复制 Unreal Engine 源码正文。

---

## 0. 核心结论

UE 蓝图编辑器内的“右键搜索”不是从自然语言 semantic kind 直接生成节点。真实流程是：

1. 先由 `FBlueprintActionDatabase` 收集所有可用 `UBlueprintNodeSpawner` 或派生 spawner。
2. 右键菜单创建时，依据当前 `Blueprint / Graph / Pin / SelectedObject / TargetClass / AssetSelection` 组装 `FBlueprintActionContext`。
3. `FBlueprintActionFilter` 按图类型、schema、pin 类型、成员可见性、latent / impure 限制、权限、binding 兼容性等条件过滤候选。
4. 搜索框只对已经通过上下文过滤的 `FEdGraphSchemaAction` 做文本匹配与权重排序。
5. 用户选中条目后，`FBlueprintActionMenuItem` 调用其内部保存的 `UBlueprintNodeSpawner::Invoke()`。
6. `NodeSpawner` 中保存的 `NodeClass`、`UFunction`、`FProperty`、`UScriptStruct`、`FAssetData`、delegate property 或 binding 对象，决定最终 spawn 的精确 `UK2Node_*` 类型和节点内部字段。

因此，BlueprintHelper 的 `SemanticConstraints` 应该作为 **簇内候选约束**，而不是替代 UE 的 spawner/action resolution。执行阶段应尽量复现 UE 右键菜单路径：

```text
GraphContext + TypedPins + TargetContext + SemanticConstraints
-> FBlueprintActionContext 等价上下文
-> ActionDatabase candidate set
-> BlueprintActionFilter / search text / semantic ranking
-> selected UBlueprintNodeSpawner
-> Invoke / SpawnEdGraphNode
```

设计文档要求的四大簇，本质上应映射到 UE 的 NodeSpawner 家族边界：

```text
FunctionActionCluster
  -> UBlueprintFunctionNodeSpawner
  -> UBlueprintTypePromotion / function registrar delegates

FieldVariableActionCluster
  -> UBlueprintFieldNodeSpawner
  -> UBlueprintVariableNodeSpawner
  -> UBlueprintComponentNodeSpawner

EventDelegateActionCluster
  -> UBlueprintEventNodeSpawner
  -> UBlueprintBoundEventNodeSpawner
  -> UAnimNotifyEventNodeSpawner
  -> UBlueprintDelegateNodeSpawner
  -> UBlueprintBoundNodeSpawner

GenericAssetStructControlActionCluster
  -> UBlueprintNodeSpawner
  -> UBlueprintAssetNodeSpawner
  -> struct / enum / generic registrar delegates
```

---

## 1. 设计文档边界对齐

输入设计文档把一级分发固定为 `EBlueprintHelperSpawnerClusterKind`，并明确 `call / get / set / op / construct / control` 等只能进入 `FBlueprintHelperActionSemanticConstraints`，作为所选 UE NodeSpawner family cluster 内部解析约束。也就是说：

```text
AgentFace semantic statement
-> Semantic Resolver
-> { ClusterKind, SemanticConstraints, GraphContext, TypedPins }
-> BlueprintActionResolutionCore
-> SpawnerClusterResolver.SelectCluster(ClusterKind)
-> selected UBlueprintNodeSpawner or derived spawner
```

本次源码追踪验证了这个方向：UE 侧确实不是按“call/get/control”这样的自然语义一级分发，而是把“可生成节点的动作”注册成 `UBlueprintNodeSpawner`，再通过上下文过滤和搜索文本让用户选择。

---

## 2. UE 编辑器右键：从鼠标事件到 `SBlueprintActionMenu`

### 2.1 右键释放进入图面上下文菜单

源码依据：

- `Source/Editor/GraphEditor/Private/SNodePanel.cpp:L986-L996`
- `Source/Editor/GraphEditor/Private/SGraphPanel.cpp:L1145-L1153`
- `Source/Editor/GraphEditor/Private/SGraphPanel.cpp:L1721-L1762`
- `Source/Editor/GraphEditor/Private/SGraphEditorImpl.cpp:L1243-L1335`
- `Source/Editor/Kismet/Private/BlueprintEditor.cpp:L1775-L1792`
- `Source/Editor/Kismet/Private/BlueprintEditor.cpp:L4001-L4023`
- `Source/Editor/Kismet/Private/SBlueprintActionMenu.cpp:L258-L424`
- `Source/Editor/GraphEditor/Private/SGraphActionMenu.cpp:L318-L423`

压缩链路：

```text
SNodePanel::OnMouseButtonUp
  [右键释放且鼠标位移仍在 dead zone 内 -> OnSummonContextMenu]
-> SGraphPanel::OnSummonContextMenu
  [读取 NodeUnderMouse / PinUnderCursor / NodeAddPosition]
-> SGraphPanel::SummonContextMenu
  [OnGetContextMenuFor.Execute(SpawnInfo) -> Slate PushMenu]
-> SGraphEditorImpl::GraphEd_OnGetContextMenuFor
  [若图可编辑且 schema 存在 -> OnCreateActionMenuAtLocation.Execute]
-> FBlueprintEditor::SetupGraphEditorEvents
  [K2 schema 绑定 OnCreateActionMenuAtLocation = OnCreateGraphActionMenu]
-> FBlueprintEditor::OnCreateGraphActionMenu
  [SNew(SBlueprintActionMenu).GraphObj(...).DraggedFromPins(...)]
-> SBlueprintActionMenu::Construct
  [ConstructActionContext; SNew(SGraphActionMenu).OnGetActionList(...)]
-> SGraphActionMenu::Construct
  [构建搜索框和 TreeView -> RefreshAllActions(false)]
```

关键点：

- 图面右键菜单是 `SGraphPanel` 创建的 Slate 菜单。
- Blueprint K2 图会被 `FBlueprintEditor` 替换成 `SBlueprintActionMenu`。
- `SBlueprintActionMenu` 不是直接 spawn node；它先创建一个 `SGraphActionMenu`，后者负责显示候选 action、搜索过滤、选择事件。

---

## 3. ActionDatabase：Spawner 候选从哪里来

### 3.1 数据库初始化和刷新

源码依据：

- `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:L1140-L1172`
- `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:L1409-L1455`
- `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:L1476-L1548`
- `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:L909-L918`
- `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabaseRegistrar.cpp:L127-L230`
- `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabaseRegistrar.cpp:L441-L463`

压缩链路：

```text
FBlueprintActionDatabase::Get
  [lazy singleton -> Init]
-> FBlueprintActionDatabase::Init
  [RefreshAll; 注册 asset registry / asset loaded 回调]
-> FBlueprintActionDatabase::RefreshAll
  [清空 ActionRegistry; 遍历 UObjectIterator<UClass>; RefreshClassActions]
-> FBlueprintActionDatabase::RefreshClassActions
  [若是 UEdGraphNode 子类 -> 创建 Registrar -> GetNodeSpecificActions]
-> BlueprintActionDatabaseImpl::GetNodeSpecificActions
  [NodeClass CDO -> UK2Node::GetMenuActions(Registrar)]
-> UK2Node_*::GetMenuActions
  [创建 UBlueprintNodeSpawner 或派生 spawner]
-> FBlueprintActionDatabaseRegistrar::AddBlueprintAction
  [按 Class / Field / Struct / Enum / Asset key 注册]
-> FBlueprintActionDatabaseRegistrar::AddActionToDatabase
  [ActionRegistry.FindOrAdd(ActionKey).Add(NodeSpawner)]
```

### 3.2 class 成员、变量、函数、delegate 不是只靠 `UK2Node::GetMenuActions`

源码依据：

- `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:L638-L651`
- `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:L654-L717`
- `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:L721-L779`
- `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:L837-L885`
- `Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp:L1749-L1768`

压缩链路：

```text
FBlueprintActionDatabase::RefreshClassActions
  [普通 class / BlueprintGeneratedClass 分支]
-> BlueprintActionDatabaseImpl::GetClassMemberActions
  [AddClassFunctionActions; AddClassPropertyActions; AddClassDataObjectActions]
-> BlueprintActionDatabaseImpl::AddClassFunctionActions
  [FunctionCanBePlacedAsEvent -> UBlueprintEventNodeSpawner::Create]
  [CanUserKismetCallFunction -> UBlueprintFunctionNodeSpawner::Create]
  [Blueprint interface -> message spawner]
-> BlueprintActionDatabaseImpl::AddClassPropertyActions
  [multicast delegate -> UBlueprintDelegateNodeSpawner / UBlueprintBoundEventNodeSpawner]
  [普通 property -> UBlueprintVariableNodeSpawner::CreateFromMemberOrParam]
-> BlueprintActionDatabaseImpl::AddBlueprintGraphActions
  [function input param / local variable -> UBlueprintVariableNodeSpawner]
-> FBlueprintActionDatabase::RefreshComponentActions
  [component type registry -> UBlueprintComponentNodeSpawner::Create]
```

这意味着同一个右键菜单候选集合同时来自：

- node class 自己的 `GetMenuActions()`；
- class member 函数；
- class property / delegate property；
- Blueprint graph 的局部变量、函数参数、macro；
- component type registry；
- asset registry / selected assets；
- struct / enum / class factory registrar delegates。

---

## 4. 菜单候选生成：Context、Filter、Search

### 4.1 `FBlueprintActionContext` 组装

源码依据：

- `Source/Editor/Kismet/Private/SBlueprintActionMenu.cpp:L531-L570`
- `Source/Editor/Kismet/Private/SBlueprintActionMenu.cpp:L572-L616`
- `Source/Editor/BlueprintGraph/Public/BlueprintActionFilter.h:L47-L83`

压缩链路：

```text
SGraphActionMenu::RefreshAllActions
  [OnGetActionList.Execute]
-> SBlueprintActionMenu::OnGetActionList
  [ConstructActionContext; MakeContextMenu; TryInsertPromoteToVariable]
-> SBlueprintActionMenu::ConstructActionContext
  [Context.Graphs.Add(GraphObj)]
  [Context.EditorPtr = BlueprintEditor]
  [Context.Blueprints.Add(Blueprint)]
  [若 context sensitive -> Context.Pins = DraggedFromPins]
  [若 MyBlueprint / SCS 选中变量或组件 -> Context.SelectedObjects]
-> FBlueprintActionContext
  [EditorPtr, Blueprints, Graphs, Pins, SelectedObjects]
```

`FBlueprintActionContext` 是 BlueprintHelper 应模拟的最小 UE 右键上下文核心。它不包含自然语言语义，只包含编辑器实际需要的图、蓝图、pin 和绑定对象上下文。

### 4.2 `FBlueprintActionMenuUtils::MakeContextMenu` 生成过滤器与菜单 section

源码依据：

- `Source/Editor/Kismet/Private/BlueprintActionMenuUtils.cpp:L491-L518`
- `Source/Editor/Kismet/Private/BlueprintActionMenuUtils.cpp:L521-L544`
- `Source/Editor/Kismet/Private/BlueprintActionMenuUtils.cpp:L551-L563`
- `Source/Editor/Kismet/Private/BlueprintActionMenuUtils.cpp:L636-L679`
- `Source/Editor/Kismet/Private/BlueprintActionMenuUtils.cpp:L681-L757`

压缩链路：

```text
FBlueprintActionMenuUtils::MakeContextMenu
  [按 context sensitive / target mask 设置 FilterFlags]
-> FBlueprintActionFilter::FBlueprintActionFilter
  [建立 rejection test 列表]
-> MakeContextMenu
  [MainMenuFilter.Context = Context]
  [依据 Blueprint skeleton class 添加 TargetClasses]
  [依据 Context.Pins 添加 pin object / node target / sibling pin target class]
  [依据 ContentBrowser 选中资产建立 AddComponent bound context]
  [建立 MainMenu / CallOnMember / AddComponent / Favorites 等 section]
-> FBlueprintActionMenuBuilder::RebuildActionList
```

`ClassTargetMask` 和 `Context.Pins` 是右键菜单能否“从一个 pin 拖出后直接搜到兼容函数 / 构造 / 转换节点”的关键。BlueprintHelper 的 typed pin 不应只用于连接节点，还应进入 action resolution。

### 4.3 `FBlueprintActionFilter` 执行 rejection tests

源码依据：

- `Source/Editor/BlueprintGraph/Private/BlueprintActionFilter.cpp:L2168-L2248`
- `Source/Editor/BlueprintGraph/Private/BlueprintActionFilter.cpp:L2297-L2323`
- `Source/Editor/BlueprintGraph/Private/BlueprintActionFilter.cpp:L2340-L2445`

压缩链路：

```text
FBlueprintActionFilter::FBlueprintActionFilter
  [AddRejectionTest(IsNodeTemplateSelfFiltered)]
  [AddRejectionTest(IsMissingMatchingPinParam)]
  [AddRejectionTest(IsFunctionMissingPinParam)]
  [AddRejectionTest(IsIncompatibleLatentNode)]
  [AddRejectionTest(IsIncompatibleImpureNode)]
  [AddRejectionTest(IsFieldInaccessible)]
  [AddRejectionTest(IsIncompatibleWithGraphType)]
  [AddRejectionTest(IsSchemaIncompatible)]
  [AddRejectionTest(IsDeprecated / IsNonTargetMember / IsStaleFieldAction)]
-> FBlueprintActionFilter::IsFiltered
  [IsFilteredByThis; OrFilters; AndFilters]
-> FBlueprintActionFilter::IsFilteredByThis
  [倒序执行 FilterTests; 执行 BlueprintGraph module 扩展过滤器]
```

### 4.4 从 ActionRegistry 到菜单项

源码依据：

- `Source/Editor/Kismet/Private/BlueprintActionMenuBuilder.cpp:L566-L671`
- `Source/Editor/Kismet/Private/BlueprintActionMenuBuilder.cpp:L460-L518`
- `Source/Editor/Kismet/Private/BlueprintActionMenuBuilder.cpp:L395-L457`
- `Source/Editor/Kismet/Private/BlueprintActionMenuBuilder.cpp:L105-L114`
- `Source/Editor/Kismet/Private/BlueprintActionMenuBuilder.cpp:L186-L193`
- `Source/Editor/Kismet/Private/BlueprintActionMenuBuilder.cpp:L233-L243`
- `Source/Editor/Kismet/Private/BlueprintActionMenuItem.cpp:L217-L228`

压缩链路：

```text
FBlueprintActionMenuBuilder::RebuildActionList
  [ActionDatabase.GetAllActions]
-> FBlueprintActionDatabase::GetAllActions
  [若 ActionRegistry 空 -> RefreshAll]
-> FBlueprintActionMenuBuilder::MakeMenuItems
  [每个 section 调用 FMenuSectionDefinition::MakeMenuItems]
-> FMenuSectionDefinition::MakeMenuItems
  [Filter.IsFiltered(DatabaseAction)]
  [通过后 -> ItemFactory.MakeActionMenuItem]
  [再对 SelectedObjects 尝试 AddBoundMenuItems]
-> FMenuSectionDefinition::AddBoundMenuItems
  [NodeSpawner->IsBindingCompatible(BindingObj)]
  [通过后 -> MakeBoundMenuItem]
-> FBlueprintActionMenuItemFactory::MakeActionMenuItem / MakeBoundMenuItem
  [Action->PrimeDefaultUiSpec(TargetGraph)]
  [Action->GetUiSpec(Context, Bindings)]
  [new FBlueprintActionMenuItem(Action, UiSignature, Bindings, ...)]
-> FBlueprintActionMenuItem::FBlueprintActionMenuItem
  [保存 UBlueprintNodeSpawner const* Action]
```

关键点：菜单项最终保存的不是 `UK2Node`，而是 `UBlueprintNodeSpawner` 指针及可选 `Bindings`。这就是“右键搜索后能 spawn 精确节点”的核心对象。

### 4.5 搜索框只是过滤和排序，不负责生成 action

源码依据：

- `Source/Editor/GraphEditor/Private/SGraphActionMenu.cpp:L829-L839`
- `Source/Editor/GraphEditor/Private/SGraphActionMenu.cpp:L756-L808`
- `Source/Editor/GraphEditor/Private/SGraphActionMenu.cpp:L1279-L1374`

压缩链路：

```text
SGraphActionMenu::OnFilterTextChanged
  [GenerateFilteredItems(false)]
-> SGraphActionMenu::GenerateFilteredItems
  [清空 FilteredRootAction -> ScoreAndAddActions -> Sort -> MarkActiveSuggestion]
-> SGraphActionMenu::ScoreAndAddActions
  [读取搜索框文本]
  [拆成 FilterTerms / SanitizedFilterTerms]
  [遍历 AllActions]
  [CurrentAction->GetFullSearchText() 包含所有 term 才显示]
  [GraphSchema->GetActionFilteredWeight(...) 计算权重]
  [FilteredRootAction->AddChild(CurrentAction)]
```

这说明 Semantic Resolver 不应把“搜索文本匹配”当作唯一选择依据。UE 自己先进行了上下文过滤，搜索只是对候选菜单项进行文本检索和排序。

---

## 5. 选中菜单项后：从 `PerformAction` 到 `SpawnEdGraphNode`

源码依据：

- `Source/Editor/GraphEditor/Private/SGraphActionMenu.cpp:L842-L858`
- `Source/Editor/GraphEditor/Private/SGraphActionMenu.cpp:L1086-L1091`
- `Source/Editor/GraphEditor/Private/SGraphActionMenu.cpp:L1142-L1150`
- `Source/Editor/GraphEditor/Private/SGraphActionMenu.cpp:L1247-L1264`
- `Source/Editor/Kismet/Private/SBlueprintActionMenu.cpp:L630-L646`
- `Source/Editor/Kismet/Private/BlueprintActionMenuItem.cpp:L109-L132`
- `Source/Editor/Kismet/Private/BlueprintActionMenuItem.cpp:L235-L338`
- `Source/Editor/BlueprintGraph/Private/BlueprintNodeSpawner.cpp:L275-L278`
- `Source/Editor/BlueprintGraph/Private/BlueprintNodeSpawner.cpp:L325-L357`
- `Source/Editor/BlueprintGraph/Classes/BlueprintNodeBinder.h:L250-L265`

压缩链路：

```text
SGraphActionMenu::OnMouseButtonDownEvent
  [OnActionSelected.Execute({Action}, OnMouseClick)]

SGraphActionMenu::OnKeyDown
  [Enter -> TryToSpawnActiveSuggestion]
-> SGraphActionMenu::TryToSpawnActiveSuggestion
  [OnItemSelected(..., OnKeyPress)]
-> SGraphActionMenu::OnItemSelected
  [HandleSelection]
-> SGraphActionMenu::HandleSelection
  [OnActionSelected.Execute({InSelectedItem->Action}, InSelectionType)]
-> SBlueprintActionMenu::OnActionSelected
  [DismissAllMenus; SelectedAction->PerformAction(GraphObj, DraggedFromPins, NewNodePosition)]
-> FBlueprintActionMenuItem::PerformAction
  [ScopedTransaction]
  [按 Bindings 拆分一次或多次 InvokeAction]
  [若 FromPin 存在 -> AutowireSpawnedNodes]
  [DirtyBlueprintFromNewNode]
  [SelectNodeSet]
-> FBlueprintMenuActionItemImpl::InvokeAction
  [Action->Invoke(ParentGraph, Bindings, Location)]
-> UBlueprintNodeSpawner::Invoke 或派生 spawner::Invoke
  [SpawnNode / PostSpawnDelegate / field setup / binding setup]
-> UBlueprintNodeSpawner::SpawnEdGraphNode
  [NewObject<UEdGraphNode>(ParentGraph, InNodeClass)]
  [CreateNewGuid]
  [NodePosX / NodePosY]
  [PostSpawnDelegate.ExecuteIfBound]
  [AllocateDefaultPins]
  [PostPlacedNewNode]
  [ParentGraph->Modify]
  [ParentGraph->AddNode(NewNode, bFromUI=true, bSelectNewNode=false)]
  [ApplyBindings]
-> IBlueprintNodeBinder::ApplyBindings
  [for Binding in Bindings -> BindToNode(Node, Binding)]
```

最终节点类型由 `Spawner->NodeClass` 和派生类 `Invoke()` 的附加逻辑共同决定。例如：

- generic spawner：直接 spawn `NodeClass`。
- function spawner：先根据函数 metadata 决定 `UK2Node_CallFunction`、`UK2Node_CallArrayFunction`、`UK2Node_PromotableOperator` 等；binding 场景下可能改成 `UK2Node_CallFunctionOnMember`。
- variable spawner：`NodeClass` 是 `UK2Node_VariableGet` 或 `UK2Node_VariableSet`，spawn 后写入 `FProperty` / local variable reference。
- event spawner：可能返回已存在 event node，而不是新建节点。
- component spawner：spawn `UK2Node_AddComponent`，再写入 component class/template。

---

## 6. 每一种 NodeSpawner 的上下文需求

### 6.1 `UBlueprintNodeSpawner`：generic node spawner

源码依据：

- `BlueprintNodeSpawner.cpp:L51-L65`
- `BlueprintNodeSpawner.cpp:L82-L105`
- `BlueprintNodeSpawner.cpp:L249-L278`
- `BlueprintNodeSpawner.cpp:L325-L357`

压缩链路：

```text
UK2Node_*::GetMenuActions
  [UBlueprintNodeSpawner::Create(GetClass())]
-> UBlueprintNodeSpawner::Create
  [保存 NodeClass; 保存 CustomizeNodeDelegate]
-> FBlueprintActionMenuItem::PerformAction
  [Action->Invoke]
-> UBlueprintNodeSpawner::Invoke
  [SpawnNode<UEdGraphNode>(NodeClass, ParentGraph, Bindings, Location, Delegate)]
-> UBlueprintNodeSpawner::SpawnEdGraphNode
  [NewObject; pins; PostPlacedNewNode; AddNode; ApplyBindings]
```

所需上下文：

- 注册期：`NodeClass`，可选 `CustomizeNodeDelegate`。
- 菜单期：`TargetGraph` 用于 template node / UI / filter；`FBlueprintActionContext` 用于 `GetUiSpec` 和 filter。
- 执行期：`ParentGraph`、spawn location、optional bindings。
- 典型 semantic：`control`、`select`、`construct`、`deconstruct`、`create`、`container_action`、generic `asset_action`。

### 6.2 `UBlueprintFieldNodeSpawner`：字段节点公共基类

源码依据：

- `BlueprintFieldNodeSpawner.cpp:L17-L29`
- `BlueprintFieldNodeSpawner.cpp:L40-L67`
- `K2Node_StructOperation.cpp:L109-L148`

压缩链路：

```text
Registrar / UK2Node_StructOperation::GetMenuActions
  [UBlueprintFieldNodeSpawner::Create(NodeClass, Field, OwnerClass)]
-> UBlueprintFieldNodeSpawner::Create
  [保存 NodeClass; 保存 FFieldVariant; 保存 OwnerClass]
-> UBlueprintFieldNodeSpawner::Invoke
  [PostSpawn: SetNodeFieldDelegate(NewNode, Field)]
-> UBlueprintNodeSpawner::SpawnEdGraphNode
  [NewObject exact NodeClass; AddNode]
```

所需上下文：

- 注册期：`NodeClass`、`FFieldVariant`、可选 `OwnerClass`。
- 菜单期：Graph / Pins / Blueprint 用于过滤字段可见性、结构体 pin 匹配、category override。
- 执行期：ParentGraph、location、bindings。
- 典型 semantic：`construct`、`deconstruct`、`field_access`，以及 variable/delegate 的公共基类行为。

### 6.3 `UBlueprintFunctionNodeSpawner`：函数、operator、latent/async/factory

源码依据：

- `BlueprintActionDatabase.cpp:L654-L717`
- `BlueprintFunctionNodeSpawner.cpp:L208-L248`
- `BlueprintFunctionNodeSpawner.cpp:L251-L354`
- `BlueprintFunctionNodeSpawner.cpp:L460-L520`
- `K2Node_AsyncAction.cpp:L31-L67`

压缩链路：

```text
BlueprintActionDatabaseImpl::AddClassFunctionActions
  [CanUserKismetCallFunction -> UBlueprintFunctionNodeSpawner::Create(Function)]
-> UBlueprintFunctionNodeSpawner::Create(UFunction)
  [根据 Function metadata 选择 NodeClass]
  [Promotable -> UK2Node_PromotableOperator]
  [CommutativeAssoc pure -> UK2Node_CommutativeAssociativeBinaryOperator]
  [MPC func -> UK2Node_CallMaterialParameterCollectionFunction]
  [DataTable func -> UK2Node_CallDataTableFunction]
  [ArrayParam -> UK2Node_CallArrayFunction]
  [default -> UK2Node_CallFunction]
-> UBlueprintFunctionNodeSpawner::Create(NodeClass, Function)
  [保存 UFunction field]
  [设置 UI name/category/keywords/icon]
  [SetNodeFieldDelegate = UK2Node_CallFunction::SetFromFunction]
-> FBlueprintActionMenuItem::PerformAction
  [Action->Invoke]
-> UBlueprintFunctionNodeSpawner::Invoke
  [PostSpawn: SetNodeFieldDelegate(NewNode, Function)]
  [若单个 object property binding 且 compact/template -> UK2Node_CallFunctionOnMember]
  [SpawnNode(SpawnClass)]
-> UBlueprintNodeSpawner::SpawnEdGraphNode
```

所需上下文：

- 注册期：`UFunction`，owner class，function metadata，optional explicit `NodeClass`。
- 菜单期：Blueprint class、graph、dragged pins、target classes、binding selected objects、function visibility、thread-safety、latent/impure compatibility。
- 执行期：ParentGraph、location、bindings；bindings 可把普通调用变成 call-on-member。
- 典型 semantic：`call`、`op`、`convert_function`、`schedule_function`、`latent_or_async_function`。

### 6.4 `UBlueprintVariableNodeSpawner`：成员变量、本地变量、参数 get/set

源码依据：

- `BlueprintActionDatabase.cpp:L721-L779`
- `BlueprintActionDatabase.cpp:L837-L885`
- `BlueprintVariableNodeSpawner.cpp:L43-L112`
- `BlueprintVariableNodeSpawner.cpp:L115-L167`
- `BlueprintVariableNodeSpawner.cpp:L233-L260`

压缩链路：

```text
BlueprintActionDatabaseImpl::AddClassPropertyActions
  [普通 blueprint-visible property]
-> UBlueprintVariableNodeSpawner::CreateFromMemberOrParam
  [NodeClass = UK2Node_VariableGet 或 UK2Node_VariableSet]
  [保存 FProperty field; VarContext; OwnerClass]
  [PostSpawn: VarNode->SetFromProperty(Property, self-context, OwnerClass)]
-> UBlueprintFieldNodeSpawner::Invoke
  [SetNodeFieldDelegate]
-> UBlueprintNodeSpawner::SpawnEdGraphNode
```

本地变量 / 函数参数链：

```text
BlueprintActionDatabaseImpl::AddBlueprintGraphActions
  [function input param -> CreateFromMemberOrParam(Get, Param, FunctionGraph)]
  [local variable -> CreateFromLocal(Get/Set, FunctionGraph, VarDesc, Property)]
-> UBlueprintVariableNodeSpawner::CreateFromLocal
  [保存 LocalVarOuter; LocalVarDesc; FProperty]
-> UBlueprintVariableNodeSpawner::Invoke
  [若 local -> VarNode->VariableReference.SetLocalMember]
  [否则 -> UBlueprintFieldNodeSpawner::Invoke]
```

所需上下文：

- 注册期：`FProperty`、getter/setter `NodeClass`、owner class 或 local `UEdGraph` + `FBPVariableDescription` + GUID。
- 菜单期：Blueprint / graph scope，用于判断 local variable 是否在作用域内；pins 用于读写方向和类型匹配；target class 用于成员可见性。
- 执行期：ParentGraph、location、optional bindings。
- 典型 semantic：`get`、`set`、`get_property`、`set_property`、`field_access`。

### 6.5 `UBlueprintEventNodeSpawner`：普通事件、自定义事件、函数事件入口

源码依据：

- `BlueprintActionDatabase.cpp:L654-L686`
- `BlueprintEventNodeSpawner.cpp:L75-L132`
- `BlueprintEventNodeSpawner.cpp:L142-L234`
- `K2Node_CustomEvent.cpp:L395` 附近：custom event 使用 `UBlueprintEventNodeSpawner::Create`。

压缩链路：

```text
BlueprintActionDatabaseImpl::AddClassFunctionActions
  [FunctionCanBePlacedAsEvent -> UBlueprintEventNodeSpawner::Create(EventFunc)]
-> UBlueprintEventNodeSpawner::Create(UFunction)
  [NodeClass = UK2Node_Event; EventFunc = Function]
-> UBlueprintEventNodeSpawner::Invoke
  [FindBlueprintForGraph]
  [FindPreExistingEvent]
  [若同名 FunctionGraph 已存在 -> 返回 FunctionEntry]
  [若 ghost event 存在 -> remove ghost]
  [PostSpawn: EventReference.SetFromField 或 CustomFunctionName]
-> UBlueprintNodeSpawner::SpawnEdGraphNode
```

所需上下文：

- 注册期：event `UFunction` 或 custom event name + event `NodeClass`。
- 菜单期：Blueprint / graph type，event implementability，是否已有同名事件或函数图。
- 执行期：ParentGraph，location，bindings；可能返回已有节点而非新建。
- 典型 semantic：`event`。

### 6.6 `UBlueprintBoundEventNodeSpawner`：component/actor bound event

源码依据：

- `BlueprintActionDatabase.cpp:L721-L770`
- `BlueprintActionDatabase.cpp:L283-L292`
- `BlueprintBoundEventNodeSpawner.cpp:L70-L89`
- `BlueprintBoundEventNodeSpawner.cpp:L108-L116`
- `BlueprintBoundEventNodeSpawner.cpp:L119-L170`
- `BlueprintBoundEventNodeSpawner.cpp:L173-L200`

压缩链路：

```text
BlueprintActionDatabaseImpl::AddClassPropertyActions
  [multicast delegate on component/actor]
-> FBlueprintNodeSpawnerFactory::MakeComponentBoundEventSpawner
  [UBlueprintBoundEventNodeSpawner::Create(UK2Node_ComponentBoundEvent, Delegate)]
-> FBlueprintNodeSpawnerFactory::MakeActorBoundEventSpawner
  [UBlueprintBoundEventNodeSpawner::Create(UK2Node_ActorBoundEvent, Delegate)]
-> FMenuSectionDefinition::AddBoundMenuItems
  [Spawner->IsBindingCompatible(SelectedObject)]
-> UBlueprintBoundEventNodeSpawner::Invoke
  [Bindings.Num > 0 -> Super::Invoke]
-> UBlueprintNodeSpawner::SpawnEdGraphNode
  [ApplyBindings]
-> UBlueprintBoundEventNodeSpawner::BindToNode
  [ComponentBoundEvent.InitializeComponentBoundEventParams]
  [ActorBoundEvent.InitializeActorBoundEventParams]
```

所需上下文：

- 注册期：`FMulticastDelegateProperty`，node class 为 component-bound 或 actor-bound event。
- 菜单期：`Context.SelectedObjects` 必须有可绑定的 component property 或 level actor；binding class 必须是 delegate owner 子类；变量 category 不被隐藏。
- 执行期：ParentGraph、location、binding object。
- 典型 semantic：`component_bound_event`。

### 6.7 `UAnimNotifyEventNodeSpawner`：动画通知事件

源码依据：

- `AnimNotifyEventNodeSpawner.cpp:L10-L36`
- `BlueprintActionFilter.cpp:L2201`：filter 中存在 anim notify 兼容性 rejection test。

压缩链路：

```text
Anim notify action registration
  [UAnimNotifyEventNodeSpawner::Create(SkeletonObjectPath, NotifyName)]
-> UAnimNotifyEventNodeSpawner::Create
  [NodeClass = UK2Node_Event]
  [CustomEventName = AnimNotify_<NotifyName>]
  [PostSpawn: EventReference.SetExternalMember(..., UAnimInstance)]
-> UBlueprintEventNodeSpawner / UBlueprintNodeSpawner spawn path
```

所需上下文：

- 注册期：`FSoftObjectPath SkeletonObjectPath`、notify name。
- 菜单期：Anim Blueprint / skeleton 兼容性。
- 执行期：ParentGraph、location。
- 典型 semantic：`anim_notify_event`。

### 6.8 `UBlueprintDelegateNodeSpawner`：delegate bind/assign/call/remove/clear

源码依据：

- `BlueprintActionDatabase.cpp:L721-L770`
- `BlueprintActionDatabase.cpp:L275-L280`
- `BlueprintDelegateNodeSpawner.cpp:L74-L121`
- `BlueprintDelegateNodeSpawner.cpp:L130-L132`

压缩链路：

```text
BlueprintActionDatabaseImpl::AddClassPropertyActions
  [FMulticastDelegateProperty]
  [BlueprintAssignable -> AddDelegate / AssignDelegate]
  [BlueprintCallable -> CallDelegate]
  [always -> RemoveDelegate / ClearDelegate]
-> UBlueprintDelegateNodeSpawner::Create(NodeClass, DelegateProperty)
  [SetField(DelegateProperty); NodeClass = UK2Node_*Delegate]
  [SetNodeFieldDelegate = DelegateNode->SetFromProperty]
-> UBlueprintFieldNodeSpawner::Invoke
  [PostSpawn: Set delegate property]
-> UBlueprintNodeSpawner::SpawnEdGraphNode
```

所需上下文：

- 注册期：`FMulticastDelegateProperty`、delegate operation node class。
- 菜单期：Blueprint / graph / pins / member visibility；assign/bind/call/remove/clear 可由 semantic kind 约束。
- 执行期：ParentGraph、location、optional bindings。
- 典型 semantic：`bind`、`unbind`、`assign`、`delegate_call`、`delegate_clear`。

### 6.9 `UBlueprintComponentNodeSpawner`：Add Component / component action

源码依据：

- `BlueprintActionDatabase.cpp:L1749-L1768`
- `BlueprintComponentNodeSpawner.cpp:L78-L146`
- `BlueprintComponentNodeSpawner.cpp:L155-L260`
- `BlueprintComponentNodeSpawner.cpp:L263-L313`
- `BlueprintComponentNodeSpawner.cpp:L322-L343`

压缩链路：

```text
FBlueprintActionDatabase::RefreshComponentActions
  [遍历 FComponentTypeRegistry]
-> UBlueprintComponentNodeSpawner::Create(FComponentTypeEntry)
  [NodeClass = UK2Node_AddComponent]
  [保存 ComponentClass 或 ComponentAssetName]
  [设置 menu name/category/tooltip/icon]
-> FMenuSectionDefinition::AddBoundMenuItems
  [若 selected asset 可绑定 -> IsBindingCompatible]
-> UBlueprintComponentNodeSpawner::Invoke
  [PostSpawn: AddComponent function reference; TemplateType = ComponentClass]
  [SpawnNode<UK2Node_AddComponent>]
  [若 class 需加载 -> LoadObject]
  [创建 ComponentTemplate]
  [设置 TemplateName pin]
  [ReconstructNode]
  [ApplyBindings]
```

所需上下文：

- 注册期：`FComponentTypeEntry`，component class 或 component asset path。
- 菜单期：Blueprint 是否支持 components；不能在 static function graph；selected asset 可通过 component asset brokerage 绑定。
- 执行期：ParentGraph、location、bindings；可能创建 component template。
- 典型 semantic：`component_ref` 的一部分，以及 `create`/`asset_action` 中的 Add Component 场景。注意：读取已有组件变量通常更接近 `UBlueprintVariableNodeSpawner`。

### 6.10 `UBlueprintAssetNodeSpawner`：资产驱动节点

源码依据：

- `BlueprintAssetNodeSpawner.cpp:L13-L34`
- `BlueprintActionMenuUtils.cpp:L689-L715`：selected content-browser assets 可进入 binding context。
- `BlueprintActionDatabaseRegistrar.cpp:L191-L230`：asset owner 或 unloaded asset data 可作为 action key。

压缩链路：

```text
asset-specific action registration
  [UBlueprintAssetNodeSpawner::Create(NodeClass, FAssetData, Outer, Delegate)]
-> UBlueprintAssetNodeSpawner::Create
  [保存 NodeClass; 保存 AssetData; 保存 CustomizeNodeDelegate]
-> UBlueprintNodeSpawner::Invoke
  [SpawnEdGraphNode]
  [PostSpawnDelegate 使用 AssetData 配置节点]
```

所需上下文：

- 注册期：`FAssetData`、node class、optional post-spawn delegate。
- 菜单期：Content Browser selected assets、asset reference permission、asset class support。
- 执行期：ParentGraph、location。
- 典型 semantic：`asset_action`、部分 `create`、`component_ref` 绑定资产场景。

### 6.11 `UBlueprintBoundNodeSpawner`：通用绑定对象上下文 spawner

源码依据：

- `BlueprintBoundNodeSpawner.cpp:L18-L29`
- `BlueprintBoundNodeSpawner.cpp:L38-L59`
- `BlueprintBoundNodeSpawner.cpp:L63-L80`
- `BlueprintActionMenuBuilder.cpp:L395-L457`

压缩链路：

```text
custom bound action registration
  [UBlueprintBoundNodeSpawner::Create(NodeClass)]
  [配置 CanBindObjectDelegate / OnBindObjectDelegate / FindPreExistingNodeDelegate]
-> FMenuSectionDefinition::AddBoundMenuItems
  [Spawner->IsBindingCompatible(SelectedObject)]
-> UBlueprintBoundNodeSpawner::Invoke
  [FindPreExistingNodeDelegate 可返回已有节点]
  [否则 -> UBlueprintNodeSpawner::Invoke]
-> UBlueprintBoundNodeSpawner::BindToNode
  [OnBindObjectDelegate(Node, BindingObject)]
```

所需上下文：

- 注册期：node class + 三类自定义 delegate。
- 菜单期：`Context.SelectedObjects` 中的可绑定对象。
- 执行期：ParentGraph、location、binding object。
- 典型 semantic：`component_bound_event`、`bind` 或其他 bound generic action 的扩展点。

---

## 7. 四大簇的压缩链路

### 7.1 FunctionActionCluster

覆盖 semantic：

```text
call
op
convert_function
schedule_function
latent_or_async_function
```

主链路：

```text
FBlueprintActionDatabase::RefreshClassActions
-> BlueprintActionDatabaseImpl::AddClassFunctionActions
  [遍历 UFunction]
  [FunctionCanBePlacedAsEvent -> Event spawner]
  [CanUserKismetCallFunction -> UBlueprintFunctionNodeSpawner::Create]
-> UBlueprintFunctionNodeSpawner::Create(UFunction)
  [按 metadata / type promotion 选择 exact NodeClass]
-> FBlueprintActionDatabaseRegistrar::AddBlueprintAction / ActionListOut.Add
-> SBlueprintActionMenu::OnGetActionList
-> FBlueprintActionMenuUtils::MakeContextMenu
-> FBlueprintActionFilter::IsFiltered
  [target class / pin / latent / impure / graph type]
-> SGraphActionMenu::ScoreAndAddActions
  [search text 匹配 action UI]
-> FBlueprintActionMenuItem::PerformAction
-> UBlueprintFunctionNodeSpawner::Invoke
  [SetFromFunction; optional CallFunctionOnMember]
-> UBlueprintNodeSpawner::SpawnEdGraphNode
```

`op` 的关键：

```text
UBlueprintFunctionNodeSpawner::Create(UFunction)
  [FTypePromotion::IsFunctionPromotionReady]
-> NodeClass = UK2Node_PromotableOperator
-> operator spawner 注册到 FTypePromotion map
-> 搜索 / pin 类型过滤
-> Spawn UK2Node_PromotableOperator 或关联 function-call node
```

Async/factory 的关键：

```text
UK2Node_AsyncAction::GetMenuActions
-> FBlueprintActionDatabaseRegistrar::RegisterClassFactoryActions<UBlueprintAsyncActionBase>
-> UBlueprintFunctionNodeSpawner::Create(FactoryFunc)
-> NodeSpawner->NodeClass = UK2Node_AsyncAction
-> PostSpawn: ProxyFactoryFunctionName / ProxyFactoryClass / ProxyClass
-> Spawn UK2Node_AsyncAction
```

FunctionActionCluster 必须提供的上下文交集：

```text
Blueprint / Graph / Schema
TypedPins: source pin type, direction, desired return type
Target: self class, explicit target object/class, bound object property
Function constraints: function name/friendly name, owner class, static/member, pure/impure,
  latent/async/timer/factory, params, return type, metadata
Search policy: context sensitive, target mask, ambiguity policy
```

### 7.2 FieldVariableActionCluster

覆盖 semantic：

```text
get
set
get_property
set_property
component_ref
field_access
```

变量 / property 链路：

```text
FBlueprintActionDatabase::RefreshClassActions
-> BlueprintActionDatabaseImpl::AddClassPropertyActions
  [普通 visible property]
-> UBlueprintVariableNodeSpawner::CreateFromMemberOrParam
  [NodeClass = UK2Node_VariableGet 或 UK2Node_VariableSet]
  [SetNodeFieldDelegate = VarNode->SetFromProperty]
-> FBlueprintActionFilter::IsFiltered
  [field visibility; non-target member; pin type; graph scope]
-> FBlueprintActionMenuItem::PerformAction
-> UBlueprintFieldNodeSpawner::Invoke
-> UBlueprintNodeSpawner::SpawnEdGraphNode
```

局部变量 / 参数链路：

```text
BlueprintActionDatabaseImpl::AddBlueprintGraphActions
  [function input param -> CreateFromMemberOrParam(Get, Param, FunctionGraph)]
  [local var -> CreateFromLocal(Get/Set, FunctionGraph, VarDesc, Property)]
-> UBlueprintVariableNodeSpawner::Invoke
  [local -> VariableReference.SetLocalMember]
-> UBlueprintNodeSpawner::SpawnEdGraphNode
```

Component / asset-bound AddComponent 链路：

```text
FBlueprintActionDatabase::RefreshComponentActions
-> UBlueprintComponentNodeSpawner::Create(FComponentTypeEntry)
-> FBlueprintActionMenuUtils::MakeContextMenu
  [AddComponentFilter; selected content-browser asset 可进入 SelectedObjects]
-> FMenuSectionDefinition::AddBoundMenuItems
  [IsBindingCompatible(asset)]
-> UBlueprintComponentNodeSpawner::Invoke
  [UK2Node_AddComponent; component template; ApplyBindings]
```

FieldVariableActionCluster 必须提供的上下文交集：

```text
Blueprint / Graph / Schema
Variable scope: member, parent member, local, param, sparse class data
Field identity: property name, owner class, category, path segment
Access kind: get / set / read property path / write property path
TypedPins: required pin type, source/target direction, by-ref rules
Target: self class, explicit target object/class, selected SCS component
Component: component variable name/type or component asset/class
```

### 7.3 EventDelegateActionCluster

覆盖 semantic：

```text
event
component_bound_event
anim_notify_event
bind
unbind
assign
delegate_call
delegate_clear
```

普通事件链路：

```text
BlueprintActionDatabaseImpl::AddClassFunctionActions
  [FunctionCanBePlacedAsEvent]
-> UBlueprintEventNodeSpawner::Create(EventFunc)
-> FBlueprintActionFilter::IsFiltered
  [event implementable; graph type; existing event]
-> FBlueprintActionMenuItem::PerformAction
-> UBlueprintEventNodeSpawner::Invoke
  [FindPreExistingEvent; Set EventReference; spawn or return existing]
```

Component / Actor bound event 链路：

```text
BlueprintActionDatabaseImpl::AddClassPropertyActions
  [multicast delegate on component or actor class]
-> UBlueprintBoundEventNodeSpawner::Create(UK2Node_ComponentBoundEvent / ActorBoundEvent, Delegate)
-> FMenuSectionDefinition::AddBoundMenuItems
  [SelectedObjects -> IsBindingCompatible]
-> UBlueprintBoundEventNodeSpawner::Invoke
-> UBlueprintBoundEventNodeSpawner::BindToNode
  [InitializeComponentBoundEventParams / InitializeActorBoundEventParams]
```

Delegate operation 链路：

```text
BlueprintActionDatabaseImpl::AddClassPropertyActions
  [FMulticastDelegateProperty]
-> UBlueprintDelegateNodeSpawner::Create(UK2Node_AddDelegate / Assign / Call / Remove / Clear, Delegate)
-> UBlueprintFieldNodeSpawner::Invoke
  [DelegateNode->SetFromProperty]
-> UBlueprintNodeSpawner::SpawnEdGraphNode
```

Anim notify event 链路：

```text
Anim notify action registration
-> UAnimNotifyEventNodeSpawner::Create(SkeletonObjectPath, NotifyName)
-> NodeClass = UK2Node_Event; CustomEventName = AnimNotify_<Notify>
-> PostSpawn: SetExternalMember(..., UAnimInstance)
-> Spawn UK2Node_Event
```

EventDelegateActionCluster 必须提供的上下文交集：

```text
Blueprint / Graph / Schema
Event identity: UFunction event, custom event name, override/implement flag
Uniqueness policy: existing event allowed? focus existing? custom event new name?
Delegate identity: FMulticastDelegateProperty, owner class, flags BlueprintAssignable/Callable
Binding object: component property, level actor, selected object, binding class
Anim notify: skeleton path, notify name, animation blueprint compatibility
TypedPins: delegate signature pin compatibility, exec/data pin requirements
```

### 7.4 GenericAssetStructControlActionCluster

覆盖 semantic：

```text
construct
deconstruct
select
control
create
asset_action
container_action
```

Generic control/select/container 链路：

```text
FBlueprintActionDatabase::RefreshClassActions
-> BlueprintActionDatabaseImpl::GetNodeSpecificActions
-> UK2Node_IfThenElse::GetMenuActions
  [UBlueprintNodeSpawner::Create(GetClass)]
-> UK2Node_ExecutionSequence::GetMenuActions
  [UBlueprintNodeSpawner::Create(GetClass)]
-> UK2Node_Select::GetMenuActions
  [UBlueprintNodeSpawner::Create(GetClass)]
-> UK2Node_MakeContainer::GetMenuActions
  [UBlueprintNodeSpawner::Create(GetClass)]
-> FBlueprintActionDatabaseRegistrar::AddBlueprintAction
-> filter/search/perform/spawn generic path
```

Struct make/break 链路：

```text
UK2Node_StructOperation::GetMenuActions
-> FBlueprintActionDatabaseRegistrar::RegisterStructActions
-> UBlueprintFieldNodeSpawner::Create(NodeClass, UScriptStruct)
  [SetNodeFieldDelegate: StructNode->StructType = Struct]
  [DynamicUiSignatureGetter: 根据 Context.Pins 调整 category]
-> UBlueprintFieldNodeSpawner::Invoke
-> UBlueprintNodeSpawner::SpawnEdGraphNode
```

Create / construct object 链路：

```text
UK2Node_ConstructObjectFromClass::GetMenuActions
-> UBlueprintNodeSpawner::Create(GetClass)
-> generic spawn UK2Node_ConstructObjectFromClass
```

Class factory / async create 链路：

```text
UK2Node_AsyncAction::GetMenuActions
-> RegisterClassFactoryActions<UBlueprintAsyncActionBase>
-> UBlueprintFunctionNodeSpawner::Create(FactoryFunc)
-> override NodeClass = UK2Node_AsyncAction
-> spawn async action node with factory function metadata
```

Asset action 链路：

```text
asset action registration
-> UBlueprintAssetNodeSpawner::Create(NodeClass, FAssetData, Delegate)
-> UBlueprintNodeSpawner::Invoke
-> PostSpawnDelegate consumes AssetData
-> Spawn exact asset-backed NodeClass
```

GenericAssetStructControlActionCluster 必须提供的上下文交集：

```text
Blueprint / Graph / Schema
Control flow: exec source pin, graph type, desired control node category
Data type: wildcard type, struct type, enum type, container kind and element/key/value type
Create target: class to construct/spawn, object outer/world context, factory function, asset data
TypedPins: source pin compatibility, desired output type, wildcard promotion target
Asset: FAssetData, selected asset, asset reference permissions
```

---

## 8. SemanticConstraints 到 UE 右键上下文的交集

### 8.1 全部 semantic 的公共最小上下文

所有四大簇共享以下上下文，否则无法稳定复现 UE 右键菜单选择：

```text
GraphContext
  Blueprint: UBlueprint 或 asset path resolve 后的 Blueprint
  Graph: UEdGraph
  Schema: UEdGraphSchema_K2
  GraphType: event graph / function graph / macro / construction script / anim graph 等
  NewNodePosition: execute 时使用；preview 可选

TypedPins
  DraggedFromPins: UE 右键菜单的 pin context
  PinType: category / subcategory / subcategory object / container flags
  Direction: input / output / exec
  DesiredType: semantic resolver 推断出的目标 pin 类型

TargetContext
  SelfClass: Blueprint skeleton/generated/parent class
  TargetClass: 显式 target object/class 或 pin object class
  SelectedObjects: MyBlueprint 变量、SCS component、level actor、content browser asset
  BindingSet: bound node/delegate/component 的绑定对象集合

SearchAndPolicy
  ContextSensitive: 是否启用上下文敏感过滤
  ContextTargetMask: Blueprint / pin object / node target / libraries 等目标范围
  SearchText: 由 semantic 生成的候选菜单搜索词，不应是唯一判定
  AmbiguityPolicy: exact / ranked / require_user_resolution

SemanticConstraints
  Kind: call/get/set/event/control/... 仅为簇内约束
  NameHints: 用户语义名、别名、friendly name、path segments
  TypeConstraints: 输入、输出、target、delegate signature、struct/container 类型
  MetadataConstraints: latent、pure、async、factory、operator、category、asset type
```

### 8.2 每个 SemanticConstraints.Kind 的上下文交集

#### `call`

```text
默认簇: FunctionActionCluster
核心上下文交集:
  Function name / friendly name / owner class / static-member distinction
  Target object or self class
  Argument pin types and return type
  Graph compatibility: latent allowed? impure allowed? thread-safe required?
  Optional binding: component property / selected object for call-on-member
```

#### `op`

```text
默认簇: FunctionActionCluster
核心上下文交集:
  Operator semantic name: add, subtract, equal, less, append, etc.
  Operand pin types and desired result type
  Type promotion target
  Function metadata: promotable operator / commutative associative binary operator
  Search text only ranks candidates after typed filtering
```

#### `get` / `set`

```text
默认簇: FieldVariableActionCluster
核心上下文交集:
  Variable/property name
  Access direction: get -> UK2Node_VariableGet; set -> UK2Node_VariableSet
  Scope: member / inherited member / local / param
  Owner class or function graph
  Property type vs dragged pin type
  Visibility and BlueprintRead/Write permissions
```

#### `get_property` / `set_property` / `field_access`

```text
默认簇: FieldVariableActionCluster
核心上下文交集:
  Target object/class and property path
  First segment may be variable spawner; nested path may need struct make/break or accessors
  Type of final segment
  Read/write permission at each segment
  If UE ActionDatabase cannot express entire path, compose multiple NodeFragments
```

#### `component_ref`

```text
默认簇: FieldVariableActionCluster
核心上下文交集:
  Existing component variable -> variable get/set path
  Add component action -> UBlueprintComponentNodeSpawner path
  Component class/name or SCS variable property
  Blueprint supports components and graph is not static function graph
  Optional selected asset binding
```

#### `event`

```text
默认簇: EventDelegateActionCluster
核心上下文交集:
  Event UFunction or custom event name
  Blueprint can implement/override event
  Graph type allows event placement
  Existing event handling: focus existing or create custom
```

#### `component_bound_event`

```text
默认簇: EventDelegateActionCluster
核心上下文交集:
  Component property or actor binding object
  Delegate property name and owner class
  Binding class child-of delegate owner
  SelectedObjects must include binding target
```

#### `anim_notify_event`

```text
默认簇: EventDelegateActionCluster
核心上下文交集:
  Skeleton object path
  Notify name
  Animation blueprint / anim instance context
  Event name convention: AnimNotify_<NotifyName>
```

#### `bind` / `unbind` / `assign` / `delegate_call` / `delegate_clear`

```text
默认簇: EventDelegateActionCluster
核心上下文交集:
  FMulticastDelegateProperty
  Operation-specific NodeClass: Add/Remove/Assign/Call/Clear delegate
  Delegate owner class and flags: BlueprintAssignable / BlueprintCallable
  Optional target object binding
  Delegate signature compatibility for pins
```

#### `construct` / `deconstruct`

```text
默认簇: GenericAssetStructControlActionCluster
核心上下文交集:
  Struct type, enum type, container kind, wildcard promotion target
  For struct: RegisterStructActions -> UBlueprintFieldNodeSpawner
  For containers: UBlueprintNodeSpawner generic MakeContainer path
  Pin direction and desired value type
```

#### `select`

```text
默认簇: GenericAssetStructControlActionCluster
核心上下文交集:
  Desired result type and option pin types
  Wildcard pin promotion context
  Optional boolean/index/enum selector type
  Graph and pin compatibility
```

#### `control`

```text
默认簇: GenericAssetStructControlActionCluster
核心上下文交集:
  Control node intent: Branch / Sequence / Switch / Return / Loop / Gate
  Exec pin context: source exec pin, graph type, function return availability
  For Switch: enum/name/string/int type and case constraints
```

#### `create`

```text
默认簇: GenericAssetStructControlActionCluster 或 FunctionActionCluster
核心上下文交集:
  Object/Actor/Widget/Component/Async type
  Constructor or factory function
  World context / outer / owner requirements
  Asset data if asset-backed
  Class factory actions may use UBlueprintFunctionNodeSpawner with NodeClass override
```

#### `asset_action`

```text
默认簇: GenericAssetStructControlActionCluster
核心上下文交集:
  FAssetData
  Asset class and reference permission
  NodeClass and post-spawn delegate that consumes the asset
  Content Browser selected asset binding if emulating right-click fully
```

#### `container_action`

```text
默认簇: GenericAssetStructControlActionCluster
核心上下文交集:
  Container kind: array / map / set
  Element type, key type, value type
  Operation intent: make, get, add, remove, foreach
  Pin context for wildcard resolution
```

---

## 9. 对 BlueprintHelper ActionResolutionCore 的实现建议

### 9.1 不要把 semantic kind 当作一级 action resolver

错误模型：

```text
kind = call -> FindFunctionByName -> NewObject<UK2Node_CallFunction>
kind = branch -> NewObject<UK2Node_IfThenElse>
kind = set -> NewObject<UK2Node_VariableSet>
```

推荐模型：

```text
Request.ClusterKind
-> Select Spawner-Oriented Cluster
-> Build UE-equivalent action query context
-> Query ActionDatabase candidate spawners
-> Apply BlueprintActionFilter-like checks
-> Apply SemanticConstraints-specific ranking
-> Return selected UBlueprintNodeSpawner signature / candidate report
-> Execute by re-resolving spawner and invoking it
```

### 9.2 Preview 结果应暴露 action evidence，而不是 node class shortcut

Preview 推荐返回：

```text
candidate_id
spawner_class
node_class
menu_name
category
keywords
signature
associated_function / associated_property / associated_struct / associated_asset
owner_class
graph_compatibility_result
pin_compatibility_result
semantic_score
ambiguity_reason
```

执行时不要持久保存裸指针。`UBlueprintNodeSpawner` 来自 ActionDatabase，可能因 blueprint 编译、asset registry、class reload 失效。应持久保存可重建的 signature/evidence，然后在 execute 重新 query。

### 9.3 SemanticConstraints 应成为簇内谓词

示例：`call Print String`：

```text
ClusterKind = FunctionActionCluster
SemanticConstraints:
  kind = call
  name_hint = Print String
  target = self or library
  arg_types = [string]
  latent = false
Resolution:
  Query UBlueprintFunctionNodeSpawner candidates
  Filter by UFunction name/friendly name/category/owner
  Filter by pin compatibility
  Rank by exact/friendly name and type fit
```

示例：`set Health`：

```text
ClusterKind = FieldVariableActionCluster
SemanticConstraints:
  kind = set
  field_name = Health
  value_type = float
Resolution:
  Query UBlueprintVariableNodeSpawner candidates
  Require NodeClass child-of UK2Node_VariableSet
  Match FProperty name / owner class / scope
  Match value pin type
```

示例：`Branch`：

```text
ClusterKind = GenericAssetStructControlActionCluster
SemanticConstraints:
  kind = control
  control_kind = branch
  condition_type = bool
Resolution:
  Query generic UBlueprintNodeSpawner candidates
  Match menu name / NodeClass / category via action UI and node template
  Require graph and exec pin compatibility
```

### 9.4 何时允许专用 FragmentBuilder

只有两类：

```text
1. UE ActionDatabase / NodeSpawner 无法表达该 semantic operation。
2. 一个 semantic operation 本质上需要多个 UE node 编排。
```

即使使用专用 builder，也应保留：

```text
typed target
typed pin
candidate report
review/debug evidence
why_not_node_spawner_reason
```

---

## 10. 最小可落地的数据结构草案

```cpp
struct FBlueprintHelperRightClickEmulationContext
{
    UBlueprint* Blueprint;
    UEdGraph* Graph;
    TArray<UEdGraphPin*> DraggedFromPins;
    TArray<FFieldVariant> SelectedObjects;
    uint32 ContextTargetMask;
    bool bContextSensitive;
};

struct FBlueprintHelperResolvedSpawnerEvidence
{
    FString CandidateId;
    FString SpawnerClass;
    FString NodeClass;
    FString MenuName;
    FString Category;
    FString SpawnerSignature;
    FString AssociatedFunctionPath;
    FString AssociatedPropertyPath;
    FString AssociatedStructPath;
    FString AssociatedAssetPath;
    float SemanticScore;
};
```

推荐执行链：

```text
FBlueprintHelperActionResolutionRequest
-> FBlueprintHelperRightClickEmulationContextBuilder::Build
-> FBlueprintHelperSpawnerClusterResolver::SelectCluster
-> ClusterResolver::CollectCandidates(ActionDatabase, Context)
-> ClusterResolver::ApplyUEFilterEquivalent
-> ClusterResolver::ApplySemanticConstraints
-> ClusterResolver::RankAndResolve
-> FBlueprintHelperNodeFragmentFactory::FromResolvedSpawner
-> Execute: Spawner->Invoke(Graph, Bindings, Location)
```

---

## 11. 源码引用索引

主要引用文件：

```text
Source/Editor/GraphEditor/Private/SNodePanel.cpp
Source/Editor/GraphEditor/Private/SGraphPanel.cpp
Source/Editor/GraphEditor/Private/SGraphEditorImpl.cpp
Source/Editor/GraphEditor/Private/SGraphActionMenu.cpp
Source/Editor/Kismet/Private/BlueprintEditor.cpp
Source/Editor/Kismet/Private/SBlueprintActionMenu.cpp
Source/Editor/Kismet/Private/BlueprintActionMenuUtils.cpp
Source/Editor/Kismet/Private/BlueprintActionMenuBuilder.cpp
Source/Editor/Kismet/Private/BlueprintActionMenuItem.cpp
Source/Editor/BlueprintGraph/Public/BlueprintActionFilter.h
Source/Editor/BlueprintGraph/Private/BlueprintActionFilter.cpp
Source/Editor/BlueprintGraph/Private/BlueprintActionDatabase.cpp
Source/Editor/BlueprintGraph/Private/BlueprintActionDatabaseRegistrar.cpp
Source/Editor/BlueprintGraph/Private/BlueprintNodeSpawner.cpp
Source/Editor/BlueprintGraph/Private/BlueprintFieldNodeSpawner.cpp
Source/Editor/BlueprintGraph/Private/BlueprintFunctionNodeSpawner.cpp
Source/Editor/BlueprintGraph/Private/BlueprintVariableNodeSpawner.cpp
Source/Editor/BlueprintGraph/Private/BlueprintEventNodeSpawner.cpp
Source/Editor/BlueprintGraph/Private/BlueprintBoundEventNodeSpawner.cpp
Source/Editor/BlueprintGraph/Private/AnimNotifyEventNodeSpawner.cpp
Source/Editor/BlueprintGraph/Private/BlueprintDelegateNodeSpawner.cpp
Source/Editor/BlueprintGraph/Private/BlueprintComponentNodeSpawner.cpp
Source/Editor/BlueprintGraph/Private/BlueprintAssetNodeSpawner.cpp
Source/Editor/BlueprintGraph/Private/BlueprintBoundNodeSpawner.cpp
```

---

## 12. 一句话结论

UE 右键菜单能从搜索文本 spawn 出确切节点，是因为候选 action 早已由 `FBlueprintActionDatabase` 注册为带有 `NodeClass + field/function/asset/binding payload` 的 `UBlueprintNodeSpawner`；搜索只是在 `FBlueprintActionContext` 和 `FBlueprintActionFilter` 已经裁剪后的候选集上做文本匹配和排序。BlueprintHelper 应把 `SemanticConstraints` 设计成对这些 spawner 候选的约束和排序依据，而不是绕过 ActionDatabase 直接硬编码 `UK2Node_*`。
