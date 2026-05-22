# BlueprintHelper ActionContext Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有 `ActionResolutionRequest` DTO 基础上补齐稳定、可复用、可异步推断的 ActionContext 构建流程，让四大 Spawner-Oriented Cluster 消费同一套上下文包，而不是在各自 resolver/builder 内临时拼字段。

**Architecture:** 主链路为 `TaskSpec / SemanticIR -> ContextDemand -> GameThread Snapshot DTO -> Worker pure-data inference -> ResolvedActionContextBundle -> ActionResolutionRequest projection -> ActionResolutionCore`。UE `UObject` 读取、ActionDatabase/ActionFilter/NodeSpawner 调用仍严格留在 GameThread；异步线程只处理不可变 DTO、statement tree、data edge 和 symbol table。现有 `FBlueprintHelperActionResolutionRequest` 不删除，而是降级为由 `ResolvedActionContextBundle` 投影出的执行请求。

**Tech Stack:** Unreal Engine 5.6 C++、BlueprintHelper GraphWrite、ActionResolutionCore、UE ActionDatabase / BlueprintActionFilter / UBlueprintNodeSpawner、Automation Tests、BlueprintHelper CLI preview/execute。

---

## Scope

本计划只做上下文构建流水线骨架和第一轮接入，不一次性迁移所有节点创建细节。

必须完成：

1. 定义共享上下文 DTO。
2. 定义 `ContextDemand` 收集边界。
3. 定义 GameThread snapshot 捕获边界。
4. 定义 Worker 纯数据推断边界。
5. 定义 bundle 去重、合并、版本校验和投影边界。
6. 将 GraphStatement 到 ActionResolution 的入口改为消费 bundle 投影，而不是直接临时拼 `ActionResolutionRequest`。
7. 增加契约测试，防止 cluster 私自重复构建上下文。

不在本计划内：

1. 不接入 Schema Menu Builder。
2. 不恢复旧 NodeHandler / parsed-node fallback。
3. 不允许 worker 线程访问 `UObject*`。
4. 不迁移 ReviewPanel / ReadContext。
5. 不做旧 AgentFace 字段兼容。

硬性约束：

1. 实现过程中发现的硬编码配置项必须拆分为 Setting 配置项，不能继续散落在 resolver、builder、cluster、UI 或测试辅助逻辑中。
2. Setting 配置项应通过统一 settings service / runtime consumption 边界读取；不得让各工具簇直接读取本地文件或复制默认值。
3. 只有与 UE API 常量、schema 固定规则、枚举语义强绑定的值可以保留为代码常量；其余阈值、数量、策略、开关、padding、候选数量、搜索策略默认值都应进入 Setting。

---

## File Structure

### New files

| Path | Responsibility |
|---|---|
| `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h` | 纯 DTO、枚举、revision token、demand、snapshot、bundle、projection result。只包含结构体和枚举，不需要 `.cpp`。 |
| `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h` | 从 statement tree / SemanticIR 收集 context demand 的类声明。 |
| `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp` | `FBlueprintHelperActionContextDemandCollector` 实现。 |
| `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextSnapshotBuilder.h` | GameThread snapshot 捕获类声明。 |
| `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextSnapshotBuilder.cpp` | `FBlueprintHelperActionContextSnapshotBuilder` 实现，只读 UE 对象并输出 DTO。 |
| `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.h` | Worker-safe 纯数据推断类声明。 |
| `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp` | `FBlueprintHelperActionContextInferenceService` 实现，不访问 `UObject*`。 |
| `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.h` | `ResolvedActionContextBundle -> ActionResolutionRequest` 投影类声明。 |
| `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp` | 投影实现。 |
| `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextRevisionGuard.h` | revision 校验类声明。 |
| `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextRevisionGuard.cpp` | revision 校验实现。 |
| `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp` | 新上下文流水线契约测试。 |

### Modified files

| Path | Responsibility |
|---|---|
| `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h` | 仅在必要时接收更完整的 request 字段；不能重新引入一级 semantic intent 分发。 |
| `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp` | 将直接拼 `FBlueprintHelperActionResolutionRequest` 的路径迁移到 context bundle projection。 |
| `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp` | 增加禁止 cluster 内重复构建上下文的契约扫描。 |
| `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md` | 补充 ActionContext Pipeline 为方案 C 的必要前置层。 |
| `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_UEActionContext_InputMatrix_20260522_CN.md` | 将共享上下文获取途径指向新的 Context Pipeline 组件。 |

---

## Task 1: Define ActionContext DTOs

**Files:**

- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
- Test: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`

- [ ] **Step 1: Add DTO header**

Create `BlueprintHelperActionContextTypes.h` with these types:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

enum class EBlueprintHelperActionContextDemandKind : uint8
{
	Graph,
	TypedPins,
	Target,
	SearchPolicy,
	Binding,
	SpawnerEvidence
};

enum class EBlueprintHelperActionContextSourceThread : uint8
{
	GameThreadSnapshot,
	WorkerInference
};

struct FBlueprintHelperActionContextRevisionToken
{
	FString AssetPath;
	FString GraphName;
	FString TaskRunId;
	FString PlanHash;
	int32 BlueprintRevision = 0;
	int32 GraphRevision = 0;

	bool IsCompatibleWith(const FBlueprintHelperActionContextRevisionToken& Other) const
	{
		return AssetPath == Other.AssetPath
			&& GraphName == Other.GraphName
			&& TaskRunId == Other.TaskRunId
			&& PlanHash == Other.PlanHash
			&& BlueprintRevision == Other.BlueprintRevision
			&& GraphRevision == Other.GraphRevision;
	}
};

struct FBlueprintHelperActionContextDemand
{
	FString StatementId;
	EBlueprintHelperSpawnerClusterKind ClusterKind = EBlueprintHelperSpawnerClusterKind::Unknown;
	EBlueprintHelperActionSemanticKind SemanticKind = EBlueprintHelperActionSemanticKind::Unknown;
	TSet<EBlueprintHelperActionContextDemandKind> RequiredKinds;
	FString Query;
	FString TargetPath;
	FString PropertyPath;
	FString TypeName;
	TArray<FString> ArgumentNames;
	TArray<FString> SourceSymbolIds;
	TArray<FString> ConsumerSymbolIds;
};

struct FBlueprintHelperActionContextGraphSnapshot
{
	FString AssetPath;
	FString BlueprintClassPath;
	FString GraphName;
	FString GraphType;
	FString SchemaClassPath;
	FString FunctionName;
	bool bImpureAllowed = false;
	bool bLatentAllowed = false;
};

struct FBlueprintHelperActionContextFieldSnapshot
{
	FString Name;
	FString OwnerClassPath;
	FString PinCategory;
	FString PinSubCategory;
	FString PinSubCategoryObjectPath;
	bool bReadable = true;
	bool bWritable = true;
	bool bComponent = false;
};

struct FBlueprintHelperActionContextSnapshot
{
	FBlueprintHelperActionContextRevisionToken Revision;
	FBlueprintHelperActionContextGraphSnapshot Graph;
	TArray<FBlueprintHelperActionContextFieldSnapshot> Fields;
	TMap<FString, FBlueprintHelperCallFunctionPinType> SymbolPinTypes;
};

struct FBlueprintHelperResolvedActionContext
{
	FString StatementId;
	EBlueprintHelperSpawnerClusterKind ClusterKind = EBlueprintHelperSpawnerClusterKind::Unknown;
	FBlueprintHelperActionSemanticConstraints Semantic;
	FString GraphName;
	TMap<FString, FString> Evidence;
};

struct FBlueprintHelperResolvedActionContextBundle
{
	FBlueprintHelperActionContextRevisionToken Revision;
	TArray<FBlueprintHelperResolvedActionContext> Contexts;

	const FBlueprintHelperResolvedActionContext* FindByStatementId(const FString& StatementId) const
	{
		return Contexts.FindByPredicate(
			[&StatementId](const FBlueprintHelperResolvedActionContext& Context)
			{
				return Context.StatementId == StatementId;
			});
	}
};
```

- [ ] **Step 2: Add DTO smoke test**

Create `BlueprintHelperActionContextPipelineTests.cpp` with a first smoke test:

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextDtoSmokeTest,
	"BlueprintHelper.GraphWrite.ActionContext.DTO.Smoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextDtoSmokeTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionContextDemand Demand;
	Demand.StatementId = TEXT("stmt_1");
	Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Demand.SemanticKind = EBlueprintHelperActionSemanticKind::Call;
	Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Graph);

	TestEqual(TEXT("statement id"), Demand.StatementId, FString(TEXT("stmt_1")));
	TestTrue(TEXT("requires graph"), Demand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::Graph));

	FBlueprintHelperActionContextRevisionToken A;
	A.AssetPath = TEXT("/Game/Test/BP_Test");
	A.GraphName = TEXT("EventGraph");
	A.TaskRunId = TEXT("task_1");
	A.PlanHash = TEXT("plan_1");

	FBlueprintHelperActionContextRevisionToken B = A;
	TestTrue(TEXT("revision compatible"), A.IsCompatibleWith(B));

	B.GraphRevision = 1;
	TestFalse(TEXT("revision mismatch"), A.IsCompatibleWith(B));

	return true;
}

#endif
```

- [ ] **Step 3: Run the DTO test**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\BlueprintHelper.uplugin" -Package="D:\UEProjects\Template\Saved\BlueprintHelperBuildTest" -TargetPlatforms=Win64
```

Expected:

```text
BUILD SUCCESSFUL
```

If the project normally uses a different compile command in the active branch, use that existing command, but keep the expected result as a clean UE 5.6 compile.

---

## Task 2: Collect ContextDemand from GraphStatement

**Files:**

- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`

- [ ] **Step 1: Add collector header**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"

struct FBlueprintHelperGraphStatementNode;

class BLUEPRINTHELPER_API FBlueprintHelperActionContextDemandCollector
{
public:
	static TArray<FBlueprintHelperActionContextDemand> CollectFromStatements(
		const TArray<FBlueprintHelperGraphStatementNode>& Statements);

private:
	static FBlueprintHelperActionContextDemand BuildDemandForStatement(
		const FBlueprintHelperGraphStatementNode& Statement);
};
```

- [ ] **Step 2: Add collector implementation**

The first implementation should map semantic kind to demand kinds, without reaching into UE objects:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"

#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementTypes.h"

TArray<FBlueprintHelperActionContextDemand> FBlueprintHelperActionContextDemandCollector::CollectFromStatements(
	const TArray<FBlueprintHelperGraphStatementNode>& Statements)
{
	TArray<FBlueprintHelperActionContextDemand> Demands;
	Demands.Reserve(Statements.Num());

	for (const FBlueprintHelperGraphStatementNode& Statement : Statements)
	{
		FBlueprintHelperActionContextDemand Demand = BuildDemandForStatement(Statement);
		if (Demand.SemanticKind != EBlueprintHelperActionSemanticKind::Unknown)
		{
			Demands.Add(MoveTemp(Demand));
		}
	}

	return Demands;
}

FBlueprintHelperActionContextDemand FBlueprintHelperActionContextDemandCollector::BuildDemandForStatement(
	const FBlueprintHelperGraphStatementNode& Statement)
{
	FBlueprintHelperActionContextDemand Demand;
	Demand.StatementId = Statement.StableId;
	Demand.Query = Statement.Query;
	Demand.TargetPath = Statement.TargetPath;
	Demand.PropertyPath = Statement.PropertyPath;
	Demand.TypeName = Statement.TypeName;

	Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Graph);
	Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::SearchPolicy);

	switch (Statement.SemanticKind)
	{
	case EBlueprintHelperActionSemanticKind::Call:
	case EBlueprintHelperActionSemanticKind::Op:
	case EBlueprintHelperActionSemanticKind::Convert:
	case EBlueprintHelperActionSemanticKind::Schedule:
		Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
		Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::TypedPins);
		Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Target);
		break;

	case EBlueprintHelperActionSemanticKind::Get:
	case EBlueprintHelperActionSemanticKind::Set:
	case EBlueprintHelperActionSemanticKind::GetProperty:
	case EBlueprintHelperActionSemanticKind::SetProperty:
		Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
		Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::TypedPins);
		Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Target);
		break;

	case EBlueprintHelperActionSemanticKind::Event:
	case EBlueprintHelperActionSemanticKind::ComponentBoundEvent:
	case EBlueprintHelperActionSemanticKind::Bind:
		Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
		Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Binding);
		Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Target);
		break;

	case EBlueprintHelperActionSemanticKind::Construct:
	case EBlueprintHelperActionSemanticKind::Deconstruct:
	case EBlueprintHelperActionSemanticKind::Select:
	case EBlueprintHelperActionSemanticKind::Control:
	case EBlueprintHelperActionSemanticKind::Create:
		Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
		Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::TypedPins);
		Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Target);
		break;

	default:
		break;
	}

	Demand.SemanticKind = Statement.SemanticKind;
	return Demand;
}
```

If current `FBlueprintHelperGraphStatementNode` field names differ, update only the collector mapping layer. Do not push those differences into `ActionResolutionCore`.

- [ ] **Step 3: Add collector test**

Append a test that verifies `op` produces `FunctionAction + TypedPins`, and `set_property` produces `FieldVariableAction + Target`.

Expected assertions:

```cpp
TestEqual(TEXT("op cluster"), OpDemand.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
TestTrue(TEXT("op requires typed pins"), OpDemand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::TypedPins));
TestEqual(TEXT("set property cluster"), SetPropertyDemand.ClusterKind, EBlueprintHelperSpawnerClusterKind::FieldVariableAction);
TestTrue(TEXT("set property requires target"), SetPropertyDemand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::Target));
```

- [ ] **Step 4: Compile**

Run the UE 5.6 compile command.

Expected:

```text
BUILD SUCCESSFUL
```

---

## Task 3: Build GameThread Snapshot DTO

**Files:**

- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextSnapshotBuilder.h`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextSnapshotBuilder.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`

- [ ] **Step 1: Add snapshot builder header**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"

class UBlueprint;
class UEdGraph;

class BLUEPRINTHELPER_API FBlueprintHelperActionContextSnapshotBuilder
{
public:
	static FBlueprintHelperActionContextSnapshot BuildSnapshot(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const TArray<FBlueprintHelperActionContextDemand>& Demands,
		const FBlueprintHelperActionContextRevisionToken& Revision);

private:
	static FBlueprintHelperActionContextGraphSnapshot CaptureGraph(UBlueprint* Blueprint, UEdGraph* Graph);
	static void CaptureFields(UBlueprint* Blueprint, FBlueprintHelperActionContextSnapshot& Snapshot);
};
```

- [ ] **Step 2: Add implementation with GameThread guard**

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextSnapshotBuilder.h"

#include "BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Engine/Blueprint.h"

FBlueprintHelperActionContextSnapshot FBlueprintHelperActionContextSnapshotBuilder::BuildSnapshot(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const TArray<FBlueprintHelperActionContextDemand>& Demands,
	const FBlueprintHelperActionContextRevisionToken& Revision)
{
	check(IsInGameThread());

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Revision = Revision;
	Snapshot.Graph = CaptureGraph(Blueprint, Graph);
	CaptureFields(Blueprint, Snapshot);
	return Snapshot;
}

FBlueprintHelperActionContextGraphSnapshot FBlueprintHelperActionContextSnapshotBuilder::CaptureGraph(
	UBlueprint* Blueprint,
	UEdGraph* Graph)
{
	FBlueprintHelperActionContextGraphSnapshot GraphSnapshot;
	if (!Blueprint || !Graph)
	{
		return GraphSnapshot;
	}

	GraphSnapshot.AssetPath = Blueprint->GetPathName();
	GraphSnapshot.GraphName = Graph->GetName();
	GraphSnapshot.BlueprintClassPath = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetPathName() : FString();
	GraphSnapshot.SchemaClassPath = Graph->GetSchema() ? Graph->GetSchema()->GetClass()->GetPathName() : FString();
	GraphSnapshot.GraphType = FBlueprintEditorUtils::IsEventGraph(Graph) ? TEXT("event_graph") : TEXT("graph");
	GraphSnapshot.bImpureAllowed = !FBlueprintEditorUtils::IsGraphReadOnly(Graph);
	GraphSnapshot.bLatentAllowed = FBlueprintEditorUtils::IsEventGraph(Graph);
	return GraphSnapshot;
}

void FBlueprintHelperActionContextSnapshotBuilder::CaptureFields(
	UBlueprint* Blueprint,
	FBlueprintHelperActionContextSnapshot& Snapshot)
{
	if (!Blueprint)
	{
		return;
	}

	for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		FBlueprintHelperActionContextFieldSnapshot Field;
		Field.Name = Variable.VarName.ToString();
		Field.OwnerClassPath = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetPathName() : FString();
		Field.PinCategory = Variable.VarType.PinCategory.ToString();
		Field.PinSubCategory = Variable.VarType.PinSubCategory.ToString();
		Field.PinSubCategoryObjectPath = Variable.VarType.PinSubCategoryObject.Get() ? Variable.VarType.PinSubCategoryObject->GetPathName() : FString();
		Field.bReadable = true;
		Field.bWritable = true;
		Snapshot.Fields.Add(MoveTemp(Field));
	}
}
```

If UE 5.6 API names differ in this branch, keep the same responsibility split and adjust only the local helper implementation.

- [ ] **Step 3: Add source hygiene test**

Add a test that scans `BlueprintHelperActionContextInferenceService.cpp` and fails if it contains `UObject*`, `UBlueprint*`, `UEdGraph*`, `FindObject`, `LoadObject`, or `GetSchema`.

Expected error if violated:

```text
Worker inference must not access UObject or UE graph APIs
```

- [ ] **Step 4: Compile**

Run the UE 5.6 compile command.

Expected:

```text
BUILD SUCCESSFUL
```

---

## Task 4: Implement Worker-Safe Context Inference

**Files:**

- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.h`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`

- [ ] **Step 1: Add inference service header**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperActionContextInferenceService
{
public:
	static FBlueprintHelperResolvedActionContextBundle Infer(
		const FBlueprintHelperActionContextSnapshot& Snapshot,
		const TArray<FBlueprintHelperActionContextDemand>& Demands);

private:
	static FBlueprintHelperResolvedActionContext BuildContext(
		const FBlueprintHelperActionContextSnapshot& Snapshot,
		const FBlueprintHelperActionContextDemand& Demand);
};
```

- [ ] **Step 2: Add pure DTO implementation**

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.h"

FBlueprintHelperResolvedActionContextBundle FBlueprintHelperActionContextInferenceService::Infer(
	const FBlueprintHelperActionContextSnapshot& Snapshot,
	const TArray<FBlueprintHelperActionContextDemand>& Demands)
{
	FBlueprintHelperResolvedActionContextBundle Bundle;
	Bundle.Revision = Snapshot.Revision;
	Bundle.Contexts.Reserve(Demands.Num());

	TSet<FString> SeenStatementIds;
	for (const FBlueprintHelperActionContextDemand& Demand : Demands)
	{
		if (SeenStatementIds.Contains(Demand.StatementId))
		{
			continue;
		}

		SeenStatementIds.Add(Demand.StatementId);
		Bundle.Contexts.Add(BuildContext(Snapshot, Demand));
	}

	return Bundle;
}

FBlueprintHelperResolvedActionContext FBlueprintHelperActionContextInferenceService::BuildContext(
	const FBlueprintHelperActionContextSnapshot& Snapshot,
	const FBlueprintHelperActionContextDemand& Demand)
{
	FBlueprintHelperResolvedActionContext Context;
	Context.StatementId = Demand.StatementId;
	Context.ClusterKind = Demand.ClusterKind;
	Context.GraphName = Snapshot.Graph.GraphName;
	Context.Semantic.Kind = Demand.SemanticKind;
	Context.Semantic.Query = Demand.Query;
	Context.Semantic.StableId = Demand.StatementId;
	Context.Semantic.TargetPath = Demand.TargetPath;
	Context.Semantic.PropertyPath = Demand.PropertyPath;
	Context.Semantic.TypeName = Demand.TypeName;
	Context.Semantic.SearchMode = TEXT("settings_default");
	Context.Semantic.AmbiguityPolicy = TEXT("settings_default");

	if (!Demand.TargetPath.IsEmpty())
	{
		const FBlueprintHelperActionContextFieldSnapshot* Field = Snapshot.Fields.FindByPredicate(
			[&Demand](const FBlueprintHelperActionContextFieldSnapshot& Candidate)
			{
				return Candidate.Name == Demand.TargetPath;
			});

		if (Field)
		{
			Context.Semantic.TargetObjectType = Field->OwnerClassPath;
			Context.Evidence.Add(TEXT("field_name"), Field->Name);
			Context.Evidence.Add(TEXT("field_pin_category"), Field->PinCategory);
		}
	}

	return Context;
}
```

- [ ] **Step 3: Add dedupe test**

Create two demands with the same `StatementId` and verify the bundle only has one context.

Expected assertion:

```cpp
TestEqual(TEXT("deduped contexts"), Bundle.Contexts.Num(), 1);
```

- [ ] **Step 4: Run tests / compile**

Run the UE 5.6 compile command.

Expected:

```text
BUILD SUCCESSFUL
```

---

## Task 5: Project Bundle into ActionResolutionRequest

**Files:**

- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.h`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`

- [ ] **Step 1: Add projector header**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"

class UBlueprint;
class UEdGraph;

class BLUEPRINTHELPER_API FBlueprintHelperActionContextBundleProjector
{
public:
	static bool TryBuildRequest(
		const FBlueprintHelperResolvedActionContextBundle& Bundle,
		const FString& StatementId,
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		FBlueprintHelperActionResolutionRequest& OutRequest,
		FString& OutError);
};
```

- [ ] **Step 2: Add projector implementation**

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.h"

bool FBlueprintHelperActionContextBundleProjector::TryBuildRequest(
	const FBlueprintHelperResolvedActionContextBundle& Bundle,
	const FString& StatementId,
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	FBlueprintHelperActionResolutionRequest& OutRequest,
	FString& OutError)
{
	const FBlueprintHelperResolvedActionContext* Context = Bundle.FindByStatementId(StatementId);
	if (!Context)
	{
		OutError = FString::Printf(TEXT("action_context_not_found:%s"), *StatementId);
		return false;
	}

	if (!Blueprint || !Graph)
	{
		OutError = TEXT("action_context_missing_blueprint_or_graph");
		return false;
	}

	OutRequest = FBlueprintHelperActionResolutionRequest();
	OutRequest.ClusterKind = Context->ClusterKind;
	OutRequest.Blueprint = Blueprint;
	OutRequest.TargetGraph = Graph;
	OutRequest.Semantic = Context->Semantic;
	return true;
}
```

- [ ] **Step 3: Add projection test**

Use a bundle with `StatementId = "stmt_project"` and verify the request receives the same cluster and semantic kind.

Expected assertions:

```cpp
TestTrue(TEXT("projection success"), bProjected);
TestEqual(TEXT("cluster projected"), Request.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
TestEqual(TEXT("semantic projected"), Request.Semantic.Kind, EBlueprintHelperActionSemanticKind::Call);
```

- [ ] **Step 4: Compile**

Run the UE 5.6 compile command.

Expected:

```text
BUILD SUCCESSFUL
```

---

## Task 6: Add Revision Guard

**Files:**

- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextRevisionGuard.h`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextRevisionGuard.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`

- [ ] **Step 1: Add revision guard header**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperActionContextRevisionGuard
{
public:
	static bool Validate(
		const FBlueprintHelperActionContextRevisionToken& Expected,
		const FBlueprintHelperActionContextRevisionToken& Current,
		FString& OutError);
};
```

- [ ] **Step 2: Add revision guard implementation**

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextRevisionGuard.h"

bool FBlueprintHelperActionContextRevisionGuard::Validate(
	const FBlueprintHelperActionContextRevisionToken& Expected,
	const FBlueprintHelperActionContextRevisionToken& Current,
	FString& OutError)
{
	if (Expected.IsCompatibleWith(Current))
	{
		return true;
	}

	OutError = FString::Printf(
		TEXT("action_context_stale:asset=%s graph=%s"),
		*Expected.AssetPath,
		*Expected.GraphName);
	return false;
}
```

- [ ] **Step 3: Add stale context test**

Expected assertion:

```cpp
TestFalse(TEXT("stale rejected"), FBlueprintHelperActionContextRevisionGuard::Validate(Expected, Current, Error));
TestTrue(TEXT("stale error"), Error.StartsWith(TEXT("action_context_stale")));
```

- [ ] **Step 4: Compile**

Run the UE 5.6 compile command.

Expected:

```text
BUILD SUCCESSFUL
```

---

## Task 7: Migrate GraphStatementBuilder Entry to Context Bundle

**Files:**

- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

- [ ] **Step 1: Locate direct request construction**

Search:

```powershell
rg -n "FBlueprintHelperActionResolutionRequest|ActionRequest\\.|Resolve\\(ActionRequest|ActionResolutionCore::Resolve" "D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp"
```

Expected:

```text
All ActionResolution requests inside GraphStatementBuilder are visible for migration.
```

- [ ] **Step 2: Add includes**

Add these includes near the existing ActionResolution include block:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextSnapshotBuilder.h"
```

- [ ] **Step 3: Build bundle once per graph generation scope**

At the graph generation scope boundary, before resolving individual statements:

```cpp
const TArray<FBlueprintHelperActionContextDemand> ContextDemands =
	FBlueprintHelperActionContextDemandCollector::CollectFromStatements(Statements);

FBlueprintHelperActionContextRevisionToken Revision;
Revision.AssetPath = Blueprint ? Blueprint->GetPathName() : FString();
Revision.GraphName = TargetGraph ? TargetGraph->GetName() : FString();
Revision.TaskRunId = TaskRunId;
Revision.PlanHash = PlanHash;

const FBlueprintHelperActionContextSnapshot ContextSnapshot =
	FBlueprintHelperActionContextSnapshotBuilder::BuildSnapshot(Blueprint, TargetGraph, ContextDemands, Revision);

const FBlueprintHelperResolvedActionContextBundle ContextBundle =
	FBlueprintHelperActionContextInferenceService::Infer(ContextSnapshot, ContextDemands);
```

If `TaskRunId` or `PlanHash` are not currently available at this layer, derive a deterministic local value from the graph generation request and document it in code:

```cpp
Revision.TaskRunId = RequestStableId;
Revision.PlanHash = GraphStableId;
```

- [ ] **Step 4: Replace direct request construction**

Replace local request setup:

```cpp
FBlueprintHelperActionResolutionRequest ActionRequest;
ActionRequest.ClusterKind = ClusterKind;
ActionRequest.Blueprint = Blueprint;
ActionRequest.TargetGraph = TargetGraph;
ActionRequest.Semantic = SemanticConstraints;
```

with:

```cpp
FBlueprintHelperActionResolutionRequest ActionRequest;
FString ProjectionError;
if (!FBlueprintHelperActionContextBundleProjector::TryBuildRequest(
	ContextBundle,
	Statement.StableId,
	Blueprint,
	TargetGraph,
	ActionRequest,
	ProjectionError))
{
	OutFragment.Diagnostics.Add(FBlueprintGeneratorDiagnostic::Error(ProjectionError));
	return false;
}
```

Use the existing local diagnostic type if the exact constructor differs.

- [ ] **Step 5: Add contract scan**

In `BlueprintHelperActionResolutionContractTests.cpp`, add a source scan that fails when `GraphStatementBuilder.cpp` assigns these fields directly outside `BlueprintHelperActionContextBundleProjector`:

```text
ActionRequest.ClusterKind =
ActionRequest.Semantic =
```

Expected:

```text
GraphStatementBuilder must project ActionResolutionRequest from ActionContextBundle
```

- [ ] **Step 6: Compile**

Run the UE 5.6 compile command.

Expected:

```text
BUILD SUCCESSFUL
```

---

## Task 8: Keep Cluster Resolvers Context-Only

**Files:**

- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

- [ ] **Step 1: Add source hygiene rules**

Extend the contract test to scan files under:

```text
D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution
```

Allow these files to build or project context:

```text
Context/BlueprintHelperActionContextDemandCollector.cpp
Context/BlueprintHelperActionContextSnapshotBuilder.cpp
Context/BlueprintHelperActionContextInferenceService.cpp
Context/BlueprintHelperActionContextBundleProjector.cpp
Context/BlueprintHelperActionContextRevisionGuard.cpp
```

For other files, fail on:

```text
CollectFromStatements(
BuildSnapshot(
Infer(
TryBuildRequest(
ActionRequest.Semantic =
ActionRequest.ClusterKind =
```

Expected:

```text
ActionResolution clusters must consume request context, not rebuild the context pipeline
```

- [ ] **Step 2: Compile**

Run the UE 5.6 compile command.

Expected:

```text
BUILD SUCCESSFUL
```

---

## Task 9: Document the New Pipeline

**Files:**

- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_UEActionContext_InputMatrix_20260522_CN.md`

- [ ] **Step 1: Update framework design**

Add this section to the framework design document:

```markdown
## ActionContext Pipeline

`ActionResolutionRequest` 不再作为上下文收集入口，而是作为 `ResolvedActionContextBundle` 投影出的执行请求。

主链路：

```text
TaskSpec / SemanticIR
-> ContextDemandCollector
-> GameThread SnapshotBuilder
-> Worker-safe InferenceService
-> ResolvedActionContextBundle
-> BundleProjector
-> ActionResolutionCore
```

硬性规则：

1. Worker 线程不得访问 `UObject*`、`UBlueprint*`、`UEdGraph*`、`UEdGraphPin*`。
2. UE 对象读取集中在 `SnapshotBuilder`。
3. statement 前后文推断集中在 `InferenceService`。
4. cluster resolver 只能消费 `ActionResolutionRequest`，不能反向扫描 TaskSpec 或重新构建上下文。
5. preview 和 execute 使用同一套 context pipeline。
```

- [ ] **Step 2: Update input matrix**

In the input matrix, update the shared context section to say:

```markdown
已确认获取途径应由 ActionContext Pipeline 消费：

1. `SnapshotBuilder` 负责 UE runtime 可读字段。
2. `InferenceService` 负责 statement/dataflow/symbol 推断字段。
3. `BundleProjector` 负责把共享上下文投影为 `ActionResolutionRequest`。
4. `ActionResolutionCore` 不再直接负责上下文收集。
```

- [ ] **Step 3: No compile needed for docs-only step**

This task is documentation-only. Compile is covered by the implementation tasks.

---

## Task 10: Runtime Smoke

**Files:**

- No source edits expected.
- Runtime artifacts may appear under `D:/UEProjects/Template/Saved/BlueprintHelper`.

- [ ] **Step 1: Start editor through global MCP**

Use the available global BlueprintHelper MCP editor lifecycle command.

Expected:

```text
Editor starts and Bridge is available.
```

- [ ] **Step 2: Run preview for one function call statement**

Use a compact graphwrite TaskSpec with one function action statement.

Expected:

```json
{
  "ok": true,
  "status": "previewed"
}
```

The preview result should contain no `action_context_not_found` error.

- [ ] **Step 3: Run preview for one get/set statement**

Use a compact graphwrite TaskSpec with one `get` and one `set`.

Expected:

```json
{
  "ok": true,
  "status": "previewed"
}
```

The preview result should prove FieldVariableAction receives projected context.

- [ ] **Step 4: Run execute for the same TaskSpec**

Expected:

```json
{
  "ok": true,
  "status": "executed",
  "summary": {
    "modified": true
  }
}
```

- [ ] **Step 5: Close editor through global MCP**

Expected:

```text
Editor closes without crash.
```

- [ ] **Step 6: Compile after runtime smoke**

Run the UE 5.6 compile command.

Expected:

```text
BUILD SUCCESSFUL
```

---

## Self-Review Checklist

- [ ] The plan keeps UE object access on GameThread.
- [ ] The plan keeps worker inference pure DTO only.
- [ ] The plan does not reintroduce old NodeHandler, parsed-node fallback, or legacy AgentFace compatibility.
- [ ] The plan preserves `SpawnerClusterKind` as the only ActionResolution first-level dispatch type.
- [ ] The plan makes `FBlueprintHelperActionResolutionRequest` a projected request, not the context source of truth.
- [ ] The plan includes tests for DTO shape, demand collection, dedupe, projection, stale revision, and source hygiene.
- [ ] The plan updates both architecture and input matrix documents.
- [ ] Any newly discovered hardcoded policy, threshold, default count, strategy, or UI/graph tuning value is moved into Setting and consumed through the unified settings runtime boundary.
- [ ] The plan ends with compile and runtime smoke.

---

## Completion Output Format

When execution finishes, report:

```text
新增内容：
1. xxx

修复内容：
1. xxx

变更需求：
1. xxx

快速修复：
1. xxx

阻塞内容：
无
```

Omit empty sections except `阻塞内容`.

---

## 2026-05-22 Tests/Docs-only implementation note

Status: DONE_WITH_CONCERNS

Completed within current write scope:

1. Added compile-safe ActionContext source-contract automation tests without including absent production ActionContext headers.
2. Added DTO shape, revision guard, bundle projection, worker-pure source hygiene, and settings-hardcoded source hygiene contracts.
3. Extended ActionResolution contract tests to forbid GraphStatementBuilder direct `ActionRequest.ClusterKind` / `ActionRequest.Semantic` assignment and to keep cluster files from rebuilding the context pipeline.
4. Updated the GraphStatement framework design document with the ActionContext Pipeline boundary and settings-hardcoded constraint.
5. Updated the UE ActionContext input matrix with pipeline ownership for demand, snapshot, inference, projection, and settings consumption.

Not marked complete:

1. Production ActionContext headers and implementations were not present during this tests/docs-only pass, and production C++ outside tests was intentionally not edited.
2. Compile/runtime smoke was not run because this task explicitly scoped implementation to tests/docs and did not request validation commands.

---

## 2026-05-22 Full Implementation Sync

Status: DONE

Completed:

1. Added production ActionContext Pipeline files under `ActionResolution/Context`:
   - `BlueprintHelperActionContextTypes.h`
   - `BlueprintHelperActionContextDemandCollector.h/.cpp`
   - `BlueprintHelperActionContextSnapshotBuilder.h/.cpp`
   - `BlueprintHelperActionContextInferenceService.h/.cpp`
   - `BlueprintHelperActionContextBundleProjector.h/.cpp`
   - `BlueprintHelperActionContextRevisionGuard.h/.cpp`
2. `SnapshotBuilder` is the GameThread UE-object boundary and captures Blueprint/Graph/Schema/field snapshot DTOs.
3. `InferenceService` consumes only DTO data and does not access `UObject*`, `UBlueprint*`, `UEdGraph*`, or `UEdGraphPin*`.
4. `BundleProjector` is the single projection point from `ResolvedActionContextBundle` to `FBlueprintHelperActionResolutionRequest`.
5. `GraphStatementBuilder` now routes scalar action resolution through `ContextDemand -> Snapshot -> Inference -> Bundle -> Projector` before calling `ResolveActionForGraph`.
6. Added ActionContext source-contract tests and ActionResolution contract tests that guard against direct request rebuilding and worker-side UE object access.
7. Updated framework/input-matrix docs to make ActionContext Pipeline the ownership boundary for shared UE action context.

Validation:

1. UE compile passed:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
```

Result: `Succeeded`.

2. Editor lifecycle smoke passed with global MCP `blueprint_open_editor`.
3. CLI `task preview` passed for a graphwrite smoke covering `set`, `get`, `op`, `branch`, and `call`.
4. CLI `task execute` passed for the same smoke TaskSpec:

```json
{
  "status": "executed",
  "summary": {
    "warnings": 0,
    "errors": 0,
    "modified": true
  }
}
```

5. Editor lifecycle close passed with global MCP `blueprint_close_editor` using `save_all=false` to avoid persisting temporary smoke graph changes.
6. Post-smoke UE compile passed; target was up to date.

Notes:

1. Runtime smoke initially hit a non-plugin PowerShell issue: `Set-Content -Encoding UTF8` wrote a BOM and CLI returned `Unexpected token '﻿' ... is not valid JSON`. This workaround is already documented in `BlueprintHelper_CLI_Tips_20260514_CN.md`; the smoke was rerun with `.NET` UTF-8 no BOM writing and passed.
2. The current GraphStatementBuilder integration builds a local single-demand context per scalar action path. This satisfies the plan's end-to-end consumption path, while future optimization can move bundle creation to the graph generation scope and reuse one bundle across all statements.

### Implementation Update 2026-05-22 Final Architecture Attempt

- [x] Added `FBlueprintHelperActionContextScope` as the reusable resolved bundle boundary. GraphStatementBuilder can now consume a prebuilt graph-scope bundle, while direct public Builder calls build a single-demand scope through the same ActionContext pipeline.
- [x] Added `FBlueprintHelperActionContextBuildService` with `BuildSync` and `BuildAsyncFromSnapshot`; worker-side inference remains pure DTO and schedules completion back to GameThread without allowing worker `UObject*` access.
- [x] Added `FBlueprintHelperActionResolutionSettingsResolver` and moved ActionResolution default candidate count/search policy consumption to `tool_clusters.graph_write.action_resolution.*` settings keys.
- [x] `BlueprintGraphGenerationPipeline` now collects all `ContextDemand` from `SemanticIR` once, builds one `ActionContextScope` per graph generation, and passes that scope through statement/expression fragment emission.
- [x] Expanded `ContextDemand` / `ResolvedActionContext` coverage for defaults, argument type hints, pin type hints, expected return hints, target object hints, and binding object evidence.
- [x] Fixed graph-scope demand normalization: field-variable semantics now use field target/property as query; op uses operator token; construct/deconstruct use TypeName.
- [x] Full-line preview smoke for `call/get/set/op/construct/deconstruct/select` passed after graph-scope demand normalization.
- [x] Execute smoke reached real Blueprint compile and exposed an existing `deconstruct Vector` native break selection gap. A generic native `UFunction -> UBlueprintFunctionNodeSpawner` path was added for `HasNativeMake/HasNativeBreak` metadata, avoiding a Vector-only hardcoded branch.
- [x] After recompilation, the saved full-line graphwrite TaskSpec passed `preview` and `execute` against the running editor.

Validation evidence:

```json
{
  "preview": {
    "status": "preview_passed",
    "warnings": 0,
    "errors": 0,
    "modified": false
  },
  "execute": {
    "status": "executed",
    "warnings": 0,
    "errors": 0,
    "modified": true
  },
  "task_spec": "D:\\UEProjects\\Template\\Saved\\BlueprintHelper\\CodexSmoke\\ActionContextPipeline_20260522_025331\\full_graph_20260522_025331.json",
  "preview_result": "D:\\UEProjects\\Template\\Saved\\BlueprintHelper\\Cli\\preview_1779390661344_0001\\result.json",
  "execute_result": "D:\\UEProjects\\Template\\Saved\\BlueprintHelper\\Cli\\task_B2EAD85344B2BAE367EF60BD961DD1F2\\result.json"
}
```

距离期望差距：
- [x] 当前已无 ActionContextPipeline 计划内阻塞项。后续工作应转入下一簇能力或更细粒度覆盖测试，不再作为本计划未完成项记录。

阻塞内容：
- 无