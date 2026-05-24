# GraphStatement Remaining Gap Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 关闭已核实的 GraphStatement 剩余 gap：Generic `asset_action` 真实 ActionDatabase spawner 接入、Generic `type_promotion` 投影 typed pin evidence 接入、Function fragment resolve/spawn lifecycle 收敛到共享 coordinator。

**Architecture:** 保持 `ClusterKind -> cluster resolver -> projected evidence -> selected UE spawner -> shared adapter -> FragmentDAG` 为唯一主线。`asset_action` 只能从 UE `FBlueprintActionDatabase` 中选中已存在的 `UBlueprintNodeSpawner`，不能用 `UBlueprintNodeSpawner::Create` 伪造成功；`type_promotion` 只能使用 UE `FTypePromotion` 已注册的 operator spawner，并由 typed pin evidence 校验。Function 的 call/action-provider fragment 不再在 `GraphStatementBuilder` 本地各自完成 resolve、adapter invoke、fragment metadata 填充，而是交给共享 coordinator。

**Tech Stack:** UE 5.6 C++ editor automation, BlueprintGraph `FBlueprintActionDatabase` / `FBlueprintActionFilter` / `FTypePromotion`, BlueprintHelper GraphWrite ActionResolution, GraphStatement FragmentDAG, AgentFace TaskSpec/CLI smoke.

---

## 执行规则

- 不执行 `git add`、`git commit`、`git push`；任务完成后只输出建议提交命令。
- 不新增 legacy intent、legacy parsed-node fallback、`FindFunctionByName` fallback，不能把旧工具路径作为新架构捷径。
- `asset_action` 缺少可重建的 ActionDatabase spawner evidence 时继续返回 `needs_more_semantic_context`，不能返回 `Resolved`。
- `type_promotion` 缺少 operator 和 typed pin evidence 时继续返回 `needs_more_semantic_context`，不能返回 `Resolved`。
- Smoke 中由 editor/Bridge 未启动造成的问题只记录为 harness 细节，不写入 bug 文档。

## 已核实状态

- `asset_action` 当前是受控诊断：`BlueprintHelperGenericCreateActionResolver.cpp` 识别该 operation，但返回 `needs_more_semantic_context`。
- Generic `type_promotion` 当前是受控诊断：`BlueprintHelperGenericTransformScheduleActionResolver.cpp` 识别该 operation，但没有普通成功路径。
- Function 已有 `FunctionActionCluster` 和 `ActionNodeSpawnerAdapter`，但 `BlueprintHelperGraphStatementBuilder.cpp` 的 call/action-provider fragment 仍直接拥有 resolve、adapter invoke、fragment 填充生命周期。

## Subagent 执行分配

- Task 1：小型代码修改，使用 `5.4 high` worker。
- Task 2：小型代码修改，使用 `5.4 high` worker。
- Task 3：中大型架构收敛，使用 `5.5 xhigh` worker。
- Task 4：验证和文档更新，使用 `5.4 high` worker。
- 全部计划结束后派发一次大型只读审计，使用 `5.4 high` reviewer，检查 no-fake-success、no-legacy-shortcut、测试证据和文档状态。

## File Structure

### 新增文件

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h`
  - 定义 `asset_action` 和 `type_promotion` 的投影 evidence DTO 与 `ContextEvidence` reader。
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.cpp`
  - 负责 evidence key 读取、trim、完整性判断。
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.h`
  - Generic `asset_action` resolver 边界。
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.cpp`
  - 只从 `FBlueprintActionDatabase` 选择已注册 spawner。
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperTypePromotionSpawnerEvidenceResolver.h`
  - Generic `type_promotion` resolver 边界。
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperTypePromotionSpawnerEvidenceResolver.cpp`
  - 只从 `FTypePromotion::GetOperatorSpawner` 选择已注册 operator spawner。
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentSpawnCoordinator.h`
  - 共享 action fragment resolve/spawn/metadata coordinator。
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentSpawnCoordinator.cpp`
  - 执行 resolve、candidate group、adapter invoke、fragment pins/common metadata。
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentBuildUtils.h`
  - 从 `GraphStatementBuilder` 抽出的 fragment pin/common metadata utility。
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentBuildUtils.cpp`
  - 承载 call/action-provider pin 填充和通用 metadata 逻辑。

### 修改文件

- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
  - 保留 `context_evidence` 到 SemanticLogicSpec / import node 输出。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericCreateActionResolver.cpp`
  - `asset_action` 分支转交 `FBlueprintHelperGenericAssetActionResolver`。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.cpp`
  - `type_promotion` 分支转交 `FBlueprintHelperTypePromotionSpawnerEvidenceResolver`。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
  - 为 statement/expression IR 增加 `ContextEvidence` map。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
  - 从 JSON `context_evidence` 读取 string map。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuildRequest.h`
  - 为 fragment build request 增加 `ContextEvidence` map。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuildRequest.cpp`
  - 从 statement/expression IR 复制 `ContextEvidence`。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
  - `BuildCallFunctionFragment` 和 `BuildActionProviderFragment` 转交共享 coordinator，并把 request evidence 合并进 ActionResolution request。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericCreateActionResolverTests.cpp`
  - 增加 ActionDatabase spawner 真实性测试。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericTransformScheduleActionResolverTests.cpp`
  - 增加 type-promotion typed evidence 成功测试。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`
  - 增加 no fake ActionDatabase success 和 Function coordinator contract。
- Modify: `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
  - 实现完成后更新能力闭环状态。
- Modify: `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`
  - 实现完成后关闭或缩窄对应 gap。
- Modify: `BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/*`
  - 实现完成后补充 positive evidence smoke 与 SmokeRecord。

## Task 1: 接入 Generic `asset_action` 真实 ActionDatabase spawner

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericCreateActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericCreateActionResolverTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

- [ ] **Step 1: 在测试中加入 ActionDatabase 真实性 helper**

In `BlueprintHelperGenericCreateActionResolverTests.cpp`, add includes:

```cpp
#include "BlueprintActionDatabase.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSpawner.h"
#include "K2Node_MakeArray.h"
```

Add helper inside the anonymous namespace:

```cpp
static FString MakeAssetActionStableIdForTest(
	const UObject* ActionOwner,
	const UBlueprintNodeSpawner* Spawner,
	const UClass* NodeClass)
{
	return FString::Printf(
		TEXT("action_database:%s:%s:%s"),
		ActionOwner ? *ActionOwner->GetPathName() : TEXT("none"),
		NodeClass ? *NodeClass->GetPathName() : TEXT("none"),
		Spawner ? *Spawner->GetSpawnerSignature().ToString() : TEXT("none"));
}

static bool FindMakeArrayActionDatabaseEvidenceForTest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	FString& OutStableId,
	FString& OutNodeClassPath,
	FString& OutMenuName)
{
	FBlueprintActionContext ActionContext;
	if (Blueprint)
	{
		ActionContext.Blueprints.Add(Blueprint);
	}
	if (Graph)
	{
		ActionContext.Graphs.Add(Graph);
	}

	FBlueprintActionFilter Filter(FBlueprintActionFilter::BPFILTER_RejectIncompatibleThreadSafety);
	Filter.Context = ActionContext;
	Filter.PermittedNodeTypes.Add(UK2Node_MakeArray::StaticClass());

	FBlueprintActionDatabase::Get().RefreshAll();
	const FBlueprintActionDatabase::FActionRegistry& Registry =
		FBlueprintActionDatabase::Get().GetAllActions();
	for (const TPair<FObjectKey, FBlueprintActionDatabase::FActionList>& Pair : Registry)
	{
		const UObject* ActionOwner = Pair.Key.ResolveObjectPtr();
		for (const TObjectPtr<UBlueprintNodeSpawner>& SpawnerPtr : Pair.Value)
		{
			UBlueprintNodeSpawner* Spawner = SpawnerPtr.Get();
			if (!Spawner)
			{
				continue;
			}

			FBlueprintActionInfo ActionInfo(ActionOwner, Spawner);
			if (Filter.IsFiltered(ActionInfo))
			{
				continue;
			}

			UClass* NodeClass = const_cast<UClass*>(ActionInfo.GetNodeClass());
			if (NodeClass != UK2Node_MakeArray::StaticClass())
			{
				continue;
			}

			const FBlueprintActionUiSpec UiSpec =
				Spawner->GetUiSpec(ActionContext, ActionInfo.GetBindings());
			OutNodeClassPath = NodeClass->GetPathName();
			OutMenuName = UiSpec.MenuName.ToString();
			OutStableId = MakeAssetActionStableIdForTest(ActionOwner, Spawner, NodeClass);
			return true;
		}
	}
	return false;
}

static bool IsSpawnerPointerFromActionDatabaseForTest(const UBlueprintNodeSpawner* ExpectedSpawner)
{
	if (!ExpectedSpawner)
	{
		return false;
	}

	FBlueprintActionDatabase::Get().RefreshAll();
	const FBlueprintActionDatabase::FActionRegistry& Registry =
		FBlueprintActionDatabase::Get().GetAllActions();
	for (const TPair<FObjectKey, FBlueprintActionDatabase::FActionList>& Pair : Registry)
	{
		for (const TObjectPtr<UBlueprintNodeSpawner>& SpawnerPtr : Pair.Value)
		{
			if (SpawnerPtr.Get() == ExpectedSpawner)
			{
				return true;
			}
		}
	}
	return false;
}
```

- [ ] **Step 2: 写 `asset_action` 成功路径红测**

Add after `FBlueprintHelperGenericCreateAssetActionRequiresSpawnerEvidenceTest`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericCreateAssetActionUsesActionDatabaseSpawnerEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Create.AssetActionUsesActionDatabaseSpawnerEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericCreateAssetActionUsesActionDatabaseSpawnerEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericCreateTestBlueprint();
	UEdGraph* Graph = GetGenericCreateTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FString StableId;
	FString NodeClassPath;
	FString MenuName;
	if (!FindMakeArrayActionDatabaseEvidenceForTest(Blueprint, Graph, StableId, NodeClassPath, MenuName))
	{
		AddError(TEXT("MakeArray action database evidence was not available for the test graph."));
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request =
		MakeGenericCreateResolverRequest(Blueprint, Graph, TEXT("asset_action"));
	Request.ContextEvidence.Add(TEXT("asset_action_stable_id"), StableId);
	Request.ContextEvidence.Add(TEXT("asset_action_node_class"), NodeClassPath);
	Request.ContextEvidence.Add(TEXT("asset_action_menu_name"), MenuName);
	Request.ContextEvidence.Add(TEXT("asset_action_query"), MenuName);

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	bool bPassed = true;
	bPassed &= TestEqual(TEXT("asset action status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	bPassed &= TestEqual(TEXT("asset action cluster"), Result.ClusterKind, EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
	bPassed &= TestEqual(TEXT("asset action stable id"), Result.SelectedStableId, StableId);
	bPassed &= TestNotNull(TEXT("asset action selected spawner"), Result.SelectedSpawner.Get());
	bPassed &= TestTrue(TEXT("selected spawner comes from ActionDatabase"), IsSpawnerPointerFromActionDatabaseForTest(Result.SelectedSpawner.Get()));
	bPassed &= TestTrue(TEXT("asset action candidate is database backed"), Result.CandidateActions.Num() == 1 && Result.CandidateActions[0].bFromActionDatabase);
	bPassed &= TestTrue(TEXT("match reason records database evidence"), Result.MatchReason.Contains(TEXT("action_database")));
	return bPassed;
}
```

- [ ] **Step 3: 运行红测**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.Create.AssetActionUsesActionDatabaseSpawnerEvidence;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_AssetAction_RED_001'
```

Expected: fails because `asset_action` still returns `needs_more_semantic_context`.

- [ ] **Step 4: 增加投影 evidence DTO**

Create `BlueprintHelperProjectedSpawnerEvidence.h`:

```cpp
#pragma once

#include "CoreMinimal.h"

struct FBlueprintHelperProjectedAssetActionEvidence
{
	FString StableId;
	FString NodeClassPath;
	FString SpawnerSignature;
	FString OwnerPath;
	FString Query;
	FString MenuName;
	FString Category;

	bool HasSelector() const
	{
		return !StableId.IsEmpty()
			|| !SpawnerSignature.IsEmpty()
			|| (!NodeClassPath.IsEmpty() && (!Query.IsEmpty() || !MenuName.IsEmpty()));
	}
};

struct FBlueprintHelperProjectedTypePromotionEvidence
{
	FString StableId;
	FString OperatorName;
	FString SourcePinType;
	FString TargetPinType;
	FString ResultPinType;

	bool IsComplete() const
	{
		return !OperatorName.IsEmpty()
			&& !SourcePinType.IsEmpty()
			&& !TargetPinType.IsEmpty();
	}
};

class BLUEPRINTHELPER_API FBlueprintHelperProjectedSpawnerEvidenceReader
{
public:
	static FBlueprintHelperProjectedAssetActionEvidence ReadAssetAction(
		const TMap<FString, FString>& ContextEvidence);
	static FBlueprintHelperProjectedTypePromotionEvidence ReadTypePromotion(
		const TMap<FString, FString>& ContextEvidence);
};
```

Create `BlueprintHelperProjectedSpawnerEvidence.cpp`:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"

namespace
{
static FString ReadTrimmed(const TMap<FString, FString>& Evidence, const TCHAR* Key)
{
	const FString* Value = Evidence.Find(Key);
	return Value ? Value->TrimStartAndEnd() : FString();
}
}

FBlueprintHelperProjectedAssetActionEvidence FBlueprintHelperProjectedSpawnerEvidenceReader::ReadAssetAction(
	const TMap<FString, FString>& ContextEvidence)
{
	FBlueprintHelperProjectedAssetActionEvidence Evidence;
	Evidence.StableId = ReadTrimmed(ContextEvidence, TEXT("asset_action_stable_id"));
	Evidence.NodeClassPath = ReadTrimmed(ContextEvidence, TEXT("asset_action_node_class"));
	Evidence.SpawnerSignature = ReadTrimmed(ContextEvidence, TEXT("asset_action_spawner_signature"));
	Evidence.OwnerPath = ReadTrimmed(ContextEvidence, TEXT("asset_action_owner_path"));
	Evidence.Query = ReadTrimmed(ContextEvidence, TEXT("asset_action_query"));
	Evidence.MenuName = ReadTrimmed(ContextEvidence, TEXT("asset_action_menu_name"));
	Evidence.Category = ReadTrimmed(ContextEvidence, TEXT("asset_action_category"));
	return Evidence;
}

FBlueprintHelperProjectedTypePromotionEvidence FBlueprintHelperProjectedSpawnerEvidenceReader::ReadTypePromotion(
	const TMap<FString, FString>& ContextEvidence)
{
	FBlueprintHelperProjectedTypePromotionEvidence Evidence;
	Evidence.StableId = ReadTrimmed(ContextEvidence, TEXT("type_promotion_stable_id"));
	Evidence.OperatorName = ReadTrimmed(ContextEvidence, TEXT("type_promotion_operator"));
	Evidence.SourcePinType = ReadTrimmed(ContextEvidence, TEXT("type_promotion_source_pin_type"));
	Evidence.TargetPinType = ReadTrimmed(ContextEvidence, TEXT("type_promotion_target_pin_type"));
	Evidence.ResultPinType = ReadTrimmed(ContextEvidence, TEXT("type_promotion_result_pin_type"));
	return Evidence;
}
```

- [ ] **Step 5: 创建 `asset_action` resolver**

Create `BlueprintHelperGenericAssetActionResolver.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class FBlueprintHelperActionClusterContextView;

class BLUEPRINTHELPER_API FBlueprintHelperGenericAssetActionResolver
{
public:
	static FBlueprintHelperActionResolutionResult Resolve(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FBlueprintHelperActionClusterContextView& Context);
};
```

Create `BlueprintHelperGenericAssetActionResolver.cpp`:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"

namespace
{
struct FBlueprintHelperAssetActionDbCandidate
{
	FString StableId;
	FString MenuName;
	FString Category;
	FString NodeClassPath;
	FString OwnerPath;
	FString SpawnerSignature;
	UBlueprintNodeSpawner* Spawner = nullptr;
};

static FString NormalizeAssetActionText(const FString& Value)
{
	FString Normalized = Value.TrimStartAndEnd().ToLower();
	Normalized.ReplaceInline(TEXT(" "), TEXT(""));
	Normalized.ReplaceInline(TEXT("_"), TEXT(""));
	Normalized.ReplaceInline(TEXT("-"), TEXT(""));
	return Normalized;
}

static FBlueprintHelperActionResolutionResult MakeAssetActionNeedsEvidence()
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("needs_more_semantic_context");
	Result.Message = TEXT("asset_action requires projected ActionDatabase spawner evidence.");
	return Result;
}

static FBlueprintHelperActionResolutionResult MakeAssetActionNotFound(
	const FBlueprintHelperProjectedAssetActionEvidence& Evidence)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("asset_action_spawner_not_found");
	Result.Message = FString::Printf(
		TEXT("No ActionDatabase spawner matched asset_action evidence stable_id='%s' node_class='%s' query='%s'."),
		*Evidence.StableId,
		*Evidence.NodeClassPath,
		*Evidence.Query);
	return Result;
}

static FString MakeAssetActionStableId(
	const UObject* ActionOwner,
	const UBlueprintNodeSpawner* Spawner,
	const UClass* NodeClass)
{
	return FString::Printf(
		TEXT("action_database:%s:%s:%s"),
		ActionOwner ? *ActionOwner->GetPathName() : TEXT("none"),
		NodeClass ? *NodeClass->GetPathName() : TEXT("none"),
		Spawner ? *Spawner->GetSpawnerSignature().ToString() : TEXT("none"));
}

static FBlueprintActionContext MakeActionContext(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintActionContext ActionContext;
	if (Request.Blueprint)
	{
		ActionContext.Blueprints.Add(Request.Blueprint);
	}
	if (Request.TargetGraph)
	{
		ActionContext.Graphs.Add(Request.TargetGraph);
	}
	return ActionContext;
}

static bool CandidateMatchesEvidence(
	const FBlueprintHelperAssetActionDbCandidate& Candidate,
	const FBlueprintHelperProjectedAssetActionEvidence& Evidence)
{
	if (!Evidence.StableId.IsEmpty() && Candidate.StableId != Evidence.StableId)
	{
		return false;
	}
	if (!Evidence.SpawnerSignature.IsEmpty() && Candidate.SpawnerSignature != Evidence.SpawnerSignature)
	{
		return false;
	}
	if (!Evidence.OwnerPath.IsEmpty() && Candidate.OwnerPath != Evidence.OwnerPath)
	{
		return false;
	}
	if (!Evidence.NodeClassPath.IsEmpty() && Candidate.NodeClassPath != Evidence.NodeClassPath)
	{
		return false;
	}

	const FString Query = NormalizeAssetActionText(Evidence.Query);
	const FString MenuName = NormalizeAssetActionText(Evidence.MenuName);
	const FString CandidateMenuName = NormalizeAssetActionText(Candidate.MenuName);
	const bool bQueryMatches = Query.IsEmpty() || CandidateMenuName.Contains(Query);
	const bool bMenuMatches = MenuName.IsEmpty() || CandidateMenuName == MenuName;
	return bQueryMatches && bMenuMatches;
}

static void CollectActionDatabaseCandidates(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperProjectedAssetActionEvidence& Evidence,
	TArray<FBlueprintHelperAssetActionDbCandidate>& OutCandidates)
{
	const FBlueprintActionContext ActionContext = MakeActionContext(Request);
	FBlueprintActionFilter Filter(FBlueprintActionFilter::BPFILTER_RejectIncompatibleThreadSafety);
	Filter.Context = ActionContext;

	FBlueprintActionDatabase::Get().RefreshAll();
	const FBlueprintActionDatabase::FActionRegistry& Registry =
		FBlueprintActionDatabase::Get().GetAllActions();
	for (const TPair<FObjectKey, FBlueprintActionDatabase::FActionList>& Pair : Registry)
	{
		const UObject* ActionOwner = Pair.Key.ResolveObjectPtr();
		for (const TObjectPtr<UBlueprintNodeSpawner>& SpawnerPtr : Pair.Value)
		{
			UBlueprintNodeSpawner* Spawner = SpawnerPtr.Get();
			if (!Spawner)
			{
				continue;
			}

			FBlueprintActionInfo ActionInfo(ActionOwner, Spawner);
			if (Filter.IsFiltered(ActionInfo))
			{
				continue;
			}

			UClass* NodeClass = const_cast<UClass*>(ActionInfo.GetNodeClass());
			if (!NodeClass || !NodeClass->IsChildOf(UEdGraphNode::StaticClass()))
			{
				continue;
			}

			const FBlueprintActionUiSpec UiSpec =
				Spawner->GetUiSpec(ActionContext, ActionInfo.GetBindings());

			FBlueprintHelperAssetActionDbCandidate Candidate;
			Candidate.StableId = MakeAssetActionStableId(ActionOwner, Spawner, NodeClass);
			Candidate.MenuName = UiSpec.MenuName.ToString();
			Candidate.Category = UiSpec.Category.ToString();
			Candidate.NodeClassPath = NodeClass->GetPathName();
			Candidate.OwnerPath = ActionOwner ? ActionOwner->GetPathName() : FString();
			Candidate.SpawnerSignature = Spawner->GetSpawnerSignature().ToString();
			Candidate.Spawner = Spawner;
			if (CandidateMatchesEvidence(Candidate, Evidence))
			{
				OutCandidates.Add(MoveTemp(Candidate));
			}
		}
	}
}

static FBlueprintHelperCallFunctionCandidateInfo MakeCandidateInfo(
	const FBlueprintHelperAssetActionDbCandidate& Candidate)
{
	FBlueprintHelperCallFunctionCandidateInfo Info;
	Info.StableId = Candidate.StableId;
	Info.DisplayName = Candidate.MenuName;
	Info.Category = Candidate.Category;
	Info.NodeClassPath = Candidate.NodeClassPath;
	Info.MatchReason = TEXT("action_database_projected_spawner_evidence");
	Info.Score = 100;
	Info.bGraphCompatible = true;
	Info.bFromActionDatabase = true;
	return Info;
}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	const FBlueprintHelperProjectedAssetActionEvidence Evidence =
		FBlueprintHelperProjectedSpawnerEvidenceReader::ReadAssetAction(Request.ContextEvidence);
	if (!Evidence.HasSelector())
	{
		return MakeAssetActionNeedsEvidence();
	}

	TArray<FBlueprintHelperAssetActionDbCandidate> Candidates;
	CollectActionDatabaseCandidates(Request, Evidence, Candidates);
	if (Candidates.Num() == 0)
	{
		return MakeAssetActionNotFound(Evidence);
	}
	if (Candidates.Num() > 1)
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::Ambiguous;
		Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
		Result.ErrorCode = TEXT("asset_action_ambiguous");
		Result.Message = TEXT("asset_action evidence matched multiple ActionDatabase spawners.");
		for (const FBlueprintHelperAssetActionDbCandidate& Candidate : Candidates)
		{
			Result.CandidateActions.Add(MakeCandidateInfo(Candidate));
		}
		return Result;
	}

	const FBlueprintHelperAssetActionDbCandidate& Candidate = Candidates[0];
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.SelectedStableId = Candidate.StableId;
	Result.SelectedSpawner = Candidate.Spawner;
	Result.CandidateActions.Add(MakeCandidateInfo(Candidate));
	Result.SpawnerClass = Candidate.Spawner ? Candidate.Spawner->GetClass()->GetPathName() : FString();
	Result.NodeClass = Candidate.NodeClassPath;
	Result.MatchReason = TEXT("action_database_projected_spawner_evidence");
	return Result;
}
```

- [ ] **Step 6: 路由 `asset_action`**

In `BlueprintHelperGenericCreateActionResolver.cpp`, add:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.h"
```

Replace the current `asset_action` block with:

```cpp
if (Operation == TEXT("asset_action"))
{
	return FBlueprintHelperGenericAssetActionResolver::Resolve(Request, Context);
}
```

- [ ] **Step 7: 增加 no-fake-success contract**

In `BlueprintHelperActionResolutionContractTests.cpp`, add:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionResolutionAssetActionNoSyntheticSpawnerContractTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.AssetActionNoSyntheticSpawner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionAssetActionNoSyntheticSpawnerContractTest::RunTest(const FString& Parameters)
{
	const FString SourcePath = BuildGraphWritePrivateSourcePath(
		TEXT("ActionResolution"),
		TEXT("BlueprintHelperGenericAssetActionResolver.cpp"));

	FString Source;
	if (!FFileHelper::LoadFileToString(Source, *SourcePath))
	{
		AddError(FString::Printf(TEXT("GenericAssetActionResolver source could not be read: %s"), *SourcePath));
		return false;
	}

	TestFalse(
		TEXT("asset_action resolver must not synthesize a node spawner"),
		Source.Contains(TEXT("UBlueprintNodeSpawner::Create")));
	TestTrue(
		TEXT("asset_action resolver must read FBlueprintActionDatabase"),
		Source.Contains(TEXT("FBlueprintActionDatabase::Get().GetAllActions()")));
	return true;
}
```

- [ ] **Step 8: 运行 Task 1 验证**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.Create.AssetAction;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_AssetAction_GREEN_001'
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract.AssetActionNoSyntheticSpawner;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_AssetActionContract_GREEN_001'
```

Expected: both reports have `failed=0`; the existing `AssetActionRequiresSpawnerEvidence` test still passes.

## Task 2: 接入 Generic `type_promotion` typed spawner evidence

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperTypePromotionSpawnerEvidenceResolver.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperTypePromotionSpawnerEvidenceResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericTransformScheduleActionResolverTests.cpp`

- [ ] **Step 1: 写 `type_promotion` 成功路径红测**

In `BlueprintHelperGenericTransformScheduleActionResolverTests.cpp`, add includes:

```cpp
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintTypePromotion.h"
```

Add after `FBlueprintHelperGenericTransformTypePromotionRequiresEvidenceTest`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericTransformTypePromotionUsesProjectedSpawnerEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Convert.TypePromotionUsesProjectedSpawnerEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericTransformTypePromotionUsesProjectedSpawnerEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericTransformScheduleTestBlueprint();
	UEdGraph* Graph = GetGenericTransformScheduleTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FTypePromotion::Get();
	UBlueprintFunctionNodeSpawner* ExpectedSpawner = FTypePromotion::GetOperatorSpawner(TEXT("Add"));
	TestNotNull(TEXT("Add operator spawner"), ExpectedSpawner);

	FBlueprintHelperActionResolutionRequest Request =
		MakeGenericTransformScheduleRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Convert);
	Request.Semantic.TransformOperation = TEXT("type_promotion");
	Request.ContextEvidence.Add(TEXT("type_promotion_stable_id"), TEXT("type_promotion:Add:int:real"));
	Request.ContextEvidence.Add(TEXT("type_promotion_operator"), TEXT("Add"));
	Request.ContextEvidence.Add(TEXT("type_promotion_source_pin_type"), TEXT("int"));
	Request.ContextEvidence.Add(TEXT("type_promotion_target_pin_type"), TEXT("real"));
	Request.ContextEvidence.Add(TEXT("type_promotion_result_pin_type"), TEXT("real"));

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	bool bPassed = true;
	bPassed &= TestEqual(TEXT("type promotion status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	bPassed &= TestEqual(TEXT("type promotion cluster"), Result.ClusterKind, EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
	bPassed &= TestEqual(TEXT("type promotion stable id"), Result.SelectedStableId, FString(TEXT("type_promotion:Add:int:real")));
	bPassed &= TestEqual(TEXT("type promotion selected spawner"), Result.SelectedSpawner.Get(), ExpectedSpawner);
	bPassed &= TestTrue(TEXT("type promotion node class"), Result.NodeClass.Contains(TEXT("K2Node_PromotableOperator")));
	bPassed &= TestTrue(TEXT("type promotion match reason"), Result.MatchReason.Contains(TEXT("projected_type_promotion_spawner_evidence")));
	return bPassed;
}
```

- [ ] **Step 2: 运行红测**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.Convert.TypePromotionUsesProjectedSpawnerEvidence;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_TypePromotion_RED_001'
```

Expected: fails because `type_promotion` still returns `needs_more_semantic_context`.

- [ ] **Step 3: 创建 resolver**

Create `BlueprintHelperTypePromotionSpawnerEvidenceResolver.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class FBlueprintHelperActionClusterContextView;

class BLUEPRINTHELPER_API FBlueprintHelperTypePromotionSpawnerEvidenceResolver
{
public:
	static FBlueprintHelperActionResolutionResult ResolveGenericTypePromotion(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FBlueprintHelperActionClusterContextView& Context);
};
```

Create `BlueprintHelperTypePromotionSpawnerEvidenceResolver.cpp`:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperTypePromotionSpawnerEvidenceResolver.h"

#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintTypePromotion.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_PromotableOperator.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"

namespace
{
static FBlueprintHelperActionResolutionResult MakeTypePromotionNeedsEvidence()
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("needs_more_semantic_context");
	Result.Message = TEXT("type_promotion requires projected type-promotion spawner evidence.");
	return Result;
}

static bool TryMakePrimitivePinType(const FString& Token, FEdGraphPinType& OutType)
{
	const FString Normalized = Token.TrimStartAndEnd().ToLower();
	if (Normalized == TEXT("int") || Normalized == TEXT("integer"))
	{
		OutType.PinCategory = UEdGraphSchema_K2::PC_Int;
		return true;
	}
	if (Normalized == TEXT("real") || Normalized == TEXT("float") || Normalized == TEXT("double"))
	{
		OutType.PinCategory = UEdGraphSchema_K2::PC_Real;
		return true;
	}
	if (Normalized == TEXT("byte"))
	{
		OutType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		return true;
	}
	return false;
}

static FBlueprintHelperActionResolutionResult MakeTypePromotionNotFound(const FString& OperatorName)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("type_promotion_spawner_not_found");
	Result.Message = FString::Printf(TEXT("No UE type-promotion spawner was registered for operator '%s'."), *OperatorName);
	return Result;
}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperTypePromotionSpawnerEvidenceResolver::ResolveGenericTypePromotion(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	const FBlueprintHelperProjectedTypePromotionEvidence Evidence =
		FBlueprintHelperProjectedSpawnerEvidenceReader::ReadTypePromotion(Request.ContextEvidence);
	if (!Evidence.IsComplete())
	{
		return MakeTypePromotionNeedsEvidence();
	}

	FEdGraphPinType SourceType;
	FEdGraphPinType TargetType;
	if (!TryMakePrimitivePinType(Evidence.SourcePinType, SourceType)
		|| !TryMakePrimitivePinType(Evidence.TargetPinType, TargetType)
		|| !FTypePromotion::IsValidPromotion(SourceType, TargetType))
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
		Result.ErrorCode = TEXT("type_promotion_invalid_pin_evidence");
		Result.Message = FString::Printf(
			TEXT("type_promotion pin evidence is not promotable: %s -> %s."),
			*Evidence.SourcePinType,
			*Evidence.TargetPinType);
		return Result;
	}

	FTypePromotion::Get();
	UBlueprintFunctionNodeSpawner* Spawner =
		FTypePromotion::GetOperatorSpawner(FName(*Evidence.OperatorName));
	if (!Spawner)
	{
		return MakeTypePromotionNotFound(Evidence.OperatorName);
	}

	FBlueprintHelperCallFunctionCandidateInfo Candidate;
	Candidate.StableId = Evidence.StableId.IsEmpty()
		? FString::Printf(TEXT("type_promotion:%s:%s:%s"), *Evidence.OperatorName, *Evidence.SourcePinType, *Evidence.TargetPinType)
		: Evidence.StableId;
	Candidate.DisplayName = Evidence.OperatorName;
	Candidate.Category = TEXT("GenericTypePromotion");
	Candidate.NodeClassPath = UK2Node_PromotableOperator::StaticClass()->GetPathName();
	Candidate.MatchReason = TEXT("projected_type_promotion_spawner_evidence");
	Candidate.Score = 100;
	Candidate.bGraphCompatible = Request.TargetGraph != nullptr;
	Candidate.bFromActionDatabase = true;
	Candidate.bBlueprintPure = true;

	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.SelectedStableId = Candidate.StableId;
	Result.SelectedSpawner = Spawner;
	Result.CandidateActions.Add(Candidate);
	Result.SpawnerClass = Spawner->GetClass()->GetPathName();
	Result.NodeClass = UK2Node_PromotableOperator::StaticClass()->GetPathName();
	Result.MatchReason = TEXT("projected_type_promotion_spawner_evidence");
	return Result;
}
```

- [ ] **Step 4: 路由 `type_promotion`**

In `BlueprintHelperGenericTransformScheduleActionResolver.cpp`, add:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperTypePromotionSpawnerEvidenceResolver.h"
```

Replace the current `type_promotion` block with:

```cpp
if (Operation == TEXT("type_promotion"))
{
	return FBlueprintHelperTypePromotionSpawnerEvidenceResolver::ResolveGenericTypePromotion(Request, Context);
}
```

- [ ] **Step 5: 运行 Task 2 验证**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.Convert.TypePromotion;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_TypePromotion_GREEN_001'
```

Expected: `TypePromotionRequiresEvidence` and `TypePromotionUsesProjectedSpawnerEvidence` both pass with `failed=0`.

## Task 3: Function fragment lifecycle 收敛到共享 coordinator

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentSpawnCoordinator.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentSpawnCoordinator.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentBuildUtils.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentBuildUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

- [ ] **Step 1: 写 Function coordinator contract 红测**

In `BlueprintHelperActionResolutionContractTests.cpp`, add:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteFunctionFragmentLifecycleCoordinatorContractTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.FunctionFragmentLifecycleCoordinator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteFunctionFragmentLifecycleCoordinatorContractTest::RunTest(const FString& Parameters)
{
	const FString BuilderPath = BuildGraphWritePrivateSourcePath(
		TEXT("GraphStatement"),
		TEXT("BlueprintHelperGraphStatementBuilder.cpp"));
	const FString CoordinatorPath = BuildGraphWritePrivateSourcePath(
		TEXT("GraphStatement"),
		TEXT("BlueprintHelperActionFragmentSpawnCoordinator.cpp"));

	FString BuilderSource;
	FString CoordinatorSource;
	if (!FFileHelper::LoadFileToString(BuilderSource, *BuilderPath))
	{
		AddError(FString::Printf(TEXT("GraphStatementBuilder source could not be read: %s"), *BuilderPath));
		return false;
	}
	if (!FFileHelper::LoadFileToString(CoordinatorSource, *CoordinatorPath))
	{
		AddError(FString::Printf(TEXT("ActionFragmentSpawnCoordinator source could not be read: %s"), *CoordinatorPath));
		return false;
	}

	const int32 CallFunctionStart = BuilderSource.Find(TEXT("BuildCallFunctionFragment"));
	const int32 VariableSetStart = BuilderSource.Find(TEXT("BuildVariableSetFragment"));
	const int32 ActionProviderStart = BuilderSource.Find(TEXT("BuildActionProviderFragment"));
	const int32 SequenceStart = BuilderSource.Find(TEXT("BuildSequenceFragment"));
	if (CallFunctionStart == INDEX_NONE || VariableSetStart == INDEX_NONE || ActionProviderStart == INDEX_NONE || SequenceStart == INDEX_NONE)
	{
		AddError(TEXT("Could not locate expected GraphStatementBuilder fragment function boundaries."));
		return false;
	}
	const FString CallFunctionRegion = BuilderSource.Mid(
		CallFunctionStart,
		VariableSetStart > CallFunctionStart ? VariableSetStart - CallFunctionStart : 0);
	const FString ActionProviderRegion = BuilderSource.Mid(
		ActionProviderStart,
		SequenceStart > ActionProviderStart ? SequenceStart - ActionProviderStart : 0);
	const FString FunctionRegions = CallFunctionRegion + TEXT("\n") + ActionProviderRegion;

	bool bClean = true;
	bClean &= TestFalse(
		TEXT("Function fragment region should not directly resolve actions"),
		FunctionRegions.Contains(TEXT("FBlueprintGraphWriteFacade::ResolveActionForGraph")));
	bClean &= TestFalse(
		TEXT("Function fragment region should not directly invoke selected spawner"),
		FunctionRegions.Contains(TEXT("FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner")));
	bClean &= TestTrue(
		TEXT("Coordinator owns action resolution"),
		CoordinatorSource.Contains(TEXT("FBlueprintGraphWriteFacade::ResolveActionForGraph")));
	bClean &= TestTrue(
		TEXT("Coordinator owns shared adapter invoke"),
		CoordinatorSource.Contains(TEXT("FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner")));
	return bClean;
}
```

- [ ] **Step 2: 运行红测**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract.FunctionFragmentLifecycleCoordinator;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_FunctionCoordinator_RED_001'
```

Expected: fails because the coordinator file does not exist and `GraphStatementBuilder` still owns the lifecycle.

- [ ] **Step 3: 抽出 fragment build utilities**

Create `BlueprintHelperActionFragmentBuildUtils.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

struct FBlueprintHelperCandidateFunctionGroup;
struct FBlueprintHelperActionResolutionResult;

enum class EBlueprintHelperActionFragmentPinProfile : uint8
{
	Call,
	ActionProvider
};

class FBlueprintHelperActionFragmentBuildUtils
{
public:
	static void PopulatePins(
		EBlueprintHelperActionFragmentPinProfile PinProfile,
		UK2Node* Node,
		FBlueprintHelperNodeFragment& OutFragment);
	static void PopulateCommonMetadata(
		const FBlueprintHelperGraphFragmentBuildRequest& Request,
		FBlueprintHelperNodeFragment& OutFragment);
	static void AppendCandidateActionGroup(
		const FString& Target,
		const FBlueprintHelperActionResolutionResult& ResolveResult,
		TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions);
};
```

Create `BlueprintHelperActionFragmentBuildUtils.cpp` with this implementation:

```cpp
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentBuildUtils.h"

#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

namespace
{
static void PopulateCallFragmentPins(UK2Node* CallNode, FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(CallNode, TEXT("execute"));
	OutFragment.ExecExitPin = FBlueprintGraphWriteFacade::FindPinByAlias(CallNode, TEXT("then"));
	OutFragment.PinBindings.Add(TEXT("execute"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("execute"), TEXT("exec"), OutFragment.ExecEntryPin });
	OutFragment.PinBindings.Add(TEXT("then"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("then"), TEXT("exec"), OutFragment.ExecExitPin });
	if (!CallNode)
	{
		return;
	}

	for (UEdGraphPin* Pin : CallNode->Pins)
	{
		if (!Pin || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			continue;
		}

		const FString PinName = Pin->PinName.ToString();
		FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), PinName, Pin->PinType.PinCategory.ToString(), Pin };
		OutFragment.PinBindings.Add(PinName, PinRef);
		if (Pin->Direction == EGPD_Input)
		{
			OutFragment.DataInputs.Add(PinName, PinRef);
		}
		else if (Pin->Direction == EGPD_Output)
		{
			OutFragment.DataOutputs.Add(PinName, PinRef);
			if (!OutFragment.DataOutputs.Contains(TEXT("return")))
			{
				OutFragment.DataOutputs.Add(TEXT("return"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("return"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("return"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("return"), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!OutFragment.DataOutputs.Contains(TEXT("result")))
			{
				OutFragment.DataOutputs.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("result"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("result"), Pin->PinType.PinCategory.ToString(), Pin });
			}
		}
	}
}

static void PopulateActionProviderFragmentPins(UK2Node* Node, FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(Node, TEXT("execute"));
	OutFragment.ExecExitPin = FBlueprintGraphWriteFacade::FindPinByAlias(Node, TEXT("then"));
	if (OutFragment.ExecEntryPin)
	{
		OutFragment.PinBindings.Add(TEXT("execute"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("execute"), TEXT("exec"), OutFragment.ExecEntryPin });
	}
	if (OutFragment.ExecExitPin)
	{
		OutFragment.PinBindings.Add(TEXT("then"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("then"), TEXT("exec"), OutFragment.ExecExitPin });
	}
	if (!Node)
	{
		return;
	}

	int32 DataInputIndex = 0;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			continue;
		}

		const FString PinName = Pin->PinName.ToString();
		FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), PinName, Pin->PinType.PinCategory.ToString(), Pin };
		OutFragment.PinBindings.Add(PinName, PinRef);
		if (!OutFragment.PinBindings.Contains(PinName.ToLower()))
		{
			OutFragment.PinBindings.Add(PinName.ToLower(), FBlueprintHelperFragmentPinRef{ TEXT("primary"), PinName.ToLower(), Pin->PinType.PinCategory.ToString(), Pin });
		}
		if (Pin->Direction == EGPD_Input)
		{
			OutFragment.DataInputs.Add(PinName, PinRef);
			if (!OutFragment.DataInputs.Contains(PinName.ToLower()))
			{
				OutFragment.DataInputs.Add(PinName.ToLower(), FBlueprintHelperFragmentPinRef{ TEXT("primary"), PinName.ToLower(), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!PinName.Equals(TEXT("self"), ESearchCase::IgnoreCase) && !OutFragment.DataInputs.Contains(TEXT("value")))
			{
				OutFragment.DataInputs.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("value"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("value"), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!PinName.Equals(TEXT("self"), ESearchCase::IgnoreCase))
			{
				if (DataInputIndex == 0 && !OutFragment.DataInputs.Contains(TEXT("left")))
				{
					OutFragment.DataInputs.Add(TEXT("left"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("left"), Pin->PinType.PinCategory.ToString(), Pin });
					OutFragment.PinBindings.Add(TEXT("left"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("left"), Pin->PinType.PinCategory.ToString(), Pin });
					if (!OutFragment.DataInputs.Contains(TEXT("condition")))
					{
						OutFragment.DataInputs.Add(TEXT("condition"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("condition"), Pin->PinType.PinCategory.ToString(), Pin });
						OutFragment.PinBindings.Add(TEXT("condition"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("condition"), Pin->PinType.PinCategory.ToString(), Pin });
					}
				}
				else if (DataInputIndex == 1 && !OutFragment.DataInputs.Contains(TEXT("right")))
				{
					OutFragment.DataInputs.Add(TEXT("right"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("right"), Pin->PinType.PinCategory.ToString(), Pin });
					OutFragment.PinBindings.Add(TEXT("right"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("right"), Pin->PinType.PinCategory.ToString(), Pin });
				}
				++DataInputIndex;
			}
		}
		else if (Pin->Direction == EGPD_Output)
		{
			OutFragment.DataOutputs.Add(PinName, PinRef);
			if (!OutFragment.DataOutputs.Contains(PinName.ToLower()))
			{
				OutFragment.DataOutputs.Add(PinName.ToLower(), FBlueprintHelperFragmentPinRef{ TEXT("primary"), PinName.ToLower(), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!OutFragment.DataOutputs.Contains(TEXT("value")))
			{
				OutFragment.DataOutputs.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("value"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("value"), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!OutFragment.DataOutputs.Contains(TEXT("result")))
			{
				OutFragment.DataOutputs.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("result"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("result"), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!OutFragment.DataOutputs.Contains(TEXT("return")))
			{
				OutFragment.DataOutputs.Add(TEXT("return"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("return"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("return"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("return"), Pin->PinType.PinCategory.ToString(), Pin });
			}
		}
	}
}
}

void FBlueprintHelperActionFragmentBuildUtils::PopulatePins(
	EBlueprintHelperActionFragmentPinProfile PinProfile,
	UK2Node* Node,
	FBlueprintHelperNodeFragment& OutFragment)
{
	if (PinProfile == EBlueprintHelperActionFragmentPinProfile::Call)
	{
		PopulateCallFragmentPins(Node, OutFragment);
		return;
	}
	PopulateActionProviderFragmentPins(Node, OutFragment);
}

void FBlueprintHelperActionFragmentBuildUtils::PopulateCommonMetadata(
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.OwnershipTags.Add(TEXT("statement_id"), Request.FragmentId);
	OutFragment.ReviewTargets.Add(Request.FragmentId);
	OutFragment.LayoutHints.Add(TEXT("x"), LexToString(Request.Location.X));
	OutFragment.LayoutHints.Add(TEXT("y"), LexToString(Request.Location.Y));
}

void FBlueprintHelperActionFragmentBuildUtils::AppendCandidateActionGroup(
	const FString& Target,
	const FBlueprintHelperActionResolutionResult& ResolveResult,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions)
{
	if (!OutCandidateFunctions || ResolveResult.CandidateActions.Num() == 0)
	{
		return;
	}

	FBlueprintHelperCandidateFunctionGroup Group;
	Group.Target = Target;
	Group.Candidates = ResolveResult.CandidateActions;
	OutCandidateFunctions->Add(MoveTemp(Group));
}
```

- [ ] **Step 4: 创建 coordinator header**

Create `BlueprintHelperActionFragmentSpawnCoordinator.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

enum class EBlueprintHelperActionFragmentPinProfile : uint8;

struct FBlueprintHelperActionFragmentSpawnCoordinatorRequest
{
	UEdGraph* TargetGraph = nullptr;
	FBlueprintHelperGraphFragmentBuildRequest BuildRequest;
	FBlueprintHelperActionResolutionRequest ActionRequest;
	EBlueprintHelperActionSemanticKind SemanticKind = EBlueprintHelperActionSemanticKind::Unknown;
	EBlueprintHelperActionFragmentPinProfile PinProfile;
	FString CandidateGroupTarget;
	FString FailurePrefix;
	bool bAppendSemanticKindOwnershipTag = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperActionFragmentSpawnCoordinator
{
public:
	static bool BuildResolvedActionFragment(
		const FBlueprintHelperActionFragmentSpawnCoordinatorRequest& Request,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError,
		TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions = nullptr);
};
```

- [ ] **Step 5: 创建 coordinator implementation**

Create `BlueprintHelperActionFragmentSpawnCoordinator.cpp`:

```cpp
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentSpawnCoordinator.h"

#include "K2Node.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentBuildUtils.h"

bool FBlueprintHelperActionFragmentSpawnCoordinator::BuildResolvedActionFragment(
	const FBlueprintHelperActionFragmentSpawnCoordinatorRequest& Request,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions)
{
	OutFragment = FBlueprintHelperNodeFragment();

	const FBlueprintHelperActionResolutionResult ActionResult =
		FBlueprintGraphWriteFacade::ResolveActionForGraph(Request.ActionRequest);
	if (!ActionResult.IsResolved())
	{
		FBlueprintHelperActionFragmentBuildUtils::AppendCandidateActionGroup(
			Request.CandidateGroupTarget,
			ActionResult,
			OutCandidateFunctions);
		OutError = ActionResult.Message.IsEmpty()
			? FString::Printf(TEXT("%s: %s"), *Request.FailurePrefix, *Request.CandidateGroupTarget)
			: ActionResult.Message;
		return false;
	}

	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = Request.BuildRequest.FragmentId;
	SpawnOptions.DefaultValues = Request.BuildRequest.DefaultValues;
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		Request.TargetGraph,
		ActionResult,
		FVector2D(Request.BuildRequest.Location.X, Request.BuildRequest.Location.Y),
		SpawnOptions,
		OutError);
	if (!SpawnedNode)
	{
		return false;
	}

	OutFragment.FragmentId = Request.BuildRequest.FragmentId;
	OutFragment.SourceStatementId = Request.BuildRequest.SourceStatementId.IsEmpty()
		? Request.BuildRequest.FragmentId
		: Request.BuildRequest.SourceStatementId;
	OutFragment.PrimaryNode = SpawnedNode;
	OutFragment.Nodes.Add(SpawnedNode);
	FBlueprintHelperActionFragmentBuildUtils::PopulatePins(
		Request.PinProfile,
		SpawnedNode,
		OutFragment);
	FBlueprintHelperActionFragmentBuildUtils::PopulateCommonMetadata(
		Request.BuildRequest,
		OutFragment);
	if (Request.bAppendSemanticKindOwnershipTag)
	{
		OutFragment.OwnershipTags.Add(
			TEXT("semantic_kind"),
			FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.SemanticKind));
	}
	return true;
}
```

- [ ] **Step 6: 迁移 `BuildCallFunctionFragment`**

In `BlueprintHelperGraphStatementBuilder.cpp`, add includes:

```cpp
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentBuildUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentSpawnCoordinator.h"
```

Keep `ApplyCallPatternBindings`, `ApplyCallActionRequestOverrides`, and `ApplyCallPatternDefaults`. Replace the local resolve/spawn/populate block after `ApplyCallPatternDefaults(BoundRequest);` with:

```cpp
FBlueprintHelperActionFragmentSpawnCoordinatorRequest CoordinatorRequest;
CoordinatorRequest.TargetGraph = TargetGraph;
CoordinatorRequest.BuildRequest = BoundRequest;
CoordinatorRequest.ActionRequest = ActionRequest;
CoordinatorRequest.SemanticKind = EBlueprintHelperActionSemanticKind::Call;
CoordinatorRequest.PinProfile = EBlueprintHelperActionFragmentPinProfile::Call;
CoordinatorRequest.CandidateGroupTarget = BoundRequest.Query;
CoordinatorRequest.FailurePrefix = TEXT("call_function resolve failed");
CoordinatorRequest.bAppendSemanticKindOwnershipTag = false;

return FBlueprintHelperActionFragmentSpawnCoordinator::BuildResolvedActionFragment(
	CoordinatorRequest,
	OutFragment,
	OutError,
	OutCandidateFunctions);
```

- [ ] **Step 7: 迁移 `BuildActionProviderFragment`**

In `BlueprintHelperGraphStatementBuilder.cpp`, replace the local `ResolveActionForGraph` and `InvokeSelectedSpawner` block with:

```cpp
FBlueprintHelperActionFragmentSpawnCoordinatorRequest CoordinatorRequest;
CoordinatorRequest.TargetGraph = TargetGraph;
CoordinatorRequest.BuildRequest = Request;
CoordinatorRequest.ActionRequest = ActionRequest;
CoordinatorRequest.SemanticKind = SemanticKind;
CoordinatorRequest.PinProfile = EBlueprintHelperActionFragmentPinProfile::ActionProvider;
CoordinatorRequest.CandidateGroupTarget = Request.Query;
CoordinatorRequest.FailurePrefix = FString::Printf(
	TEXT("action provider unavailable: semantic=%s"),
	*FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind));
CoordinatorRequest.bAppendSemanticKindOwnershipTag = true;

return FBlueprintHelperActionFragmentSpawnCoordinator::BuildResolvedActionFragment(
	CoordinatorRequest,
	OutFragment,
	OutError,
	nullptr);
```

- [ ] **Step 8: 运行 Task 3 验证**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract.FunctionFragmentLifecycleCoordinator;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_FunctionCoordinator_GREEN_001'
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.FunctionAction;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_FunctionAction_AfterCoordinator_001'
```

Expected: both reports have `failed=0`.

## Task 4: Smoke、文档和最终验证

**Files:**
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuildRequest.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuildRequest.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
- Modify: `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
- Modify: `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`
- Modify: `BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/04_generic_graph.json`
- Modify: `BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/05_generic_expected_diagnostics.json`
- Modify: `BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/SmokeRecord_20260524_CN.md`

- [ ] **Step 1: 接入 `context_evidence` 从 TaskSpec 到 ActionResolution 的透传**

In `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`, add this helper near `copyConvertScheduleSemanticFields`:

```ts
function copyContextEvidence(source: Record<string, unknown>, target: Record<string, unknown>): void {
  const evidence = source['context_evidence'];
  if (isRecord(evidence)) {
    target['context_evidence'] = { ...evidence };
  }
}
```

Call it in `cloneLogicExpressionWithCompiledIds` immediately after `const out: Record<string, unknown> = { ...expression, id: nodeId };`:

```ts
copyContextEvidence(expression, out);
```

Call it in `cloneLogicStatementWithCompiledIds` immediately after `const out: Record<string, unknown> = { ...statementRecord, id: statementId };`:

```ts
copyContextEvidence(statementRecord, out);
```

Call it in `compileStatementNode` after `copyConvertScheduleSemanticFields(statementRecord, node);`:

```ts
copyContextEvidence(statementRecord, node);
```

In `BlueprintHelperGraphSemanticIR.h`, add this member to both `FBlueprintHelperGraphExpressionIR` and `FBlueprintHelperGraphStatementIR`:

```cpp
TMap<FString, FString> ContextEvidence;
```

In `BlueprintHelperGraphSemanticIR.cpp`, add this helper near `ReadOptionalJsonValueAsString`:

```cpp
static void ReadOptionalStringMapField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	TMap<FString, FString>& OutMap)
{
	OutMap.Reset();
	const TSharedPtr<FJsonObject>* MapObject = nullptr;
	if (!Object.IsValid() || !Object->TryGetObjectField(FieldName, MapObject) || !MapObject || !MapObject->IsValid())
	{
		return;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*MapObject)->Values)
	{
		OutMap.Add(Pair.Key, FBlueprintHelperGraphSemanticIRUtils::JsonValueToString(Pair.Value));
	}
}
```

Call it in `ParseStatement` after category priority is read:

```cpp
ReadOptionalStringMapField(StatementObject, TEXT("context_evidence"), Statement->ContextEvidence);
```

Call it in `ParseExpression` after category priority is read:

```cpp
ReadOptionalStringMapField(ExpressionObject, TEXT("context_evidence"), Expression->ContextEvidence);
```

In `BlueprintHelperGraphFragmentBuildRequest.h`, add:

```cpp
TMap<FString, FString> ContextEvidence;
```

In `BlueprintHelperGraphFragmentBuildRequest.cpp`, copy it in both constructors:

```cpp
Request.ContextEvidence = Statement.ContextEvidence;
```

```cpp
Request.ContextEvidence = Expression.ContextEvidence;
```

In `BlueprintHelperGraphStatementBuilder.cpp`, after every projected `ActionRequest` is built for call/create/action-provider fragments, append the build-request evidence:

```cpp
ActionRequest.ContextEvidence.Append(BoundRequest.ContextEvidence);
```

for `BuildCallFunctionFragment`, and:

```cpp
ActionRequest.ContextEvidence.Append(Request.ContextEvidence);
```

for `BuildCreateFragment` and `BuildActionProviderFragment`.

Expected: `context_evidence` keys in a TaskSpec graph statement are visible to `FBlueprintHelperActionResolutionRequest::ContextEvidence` without being inferred or synthesized by the resolver.

- [ ] **Step 2: 保留 missing-evidence expected diagnostics**

Keep `05_generic_expected_diagnostics.json` rows for no-evidence `type_promotion`, timer/delegate, latent/async. These remain `preview_blocked` with `needs_more_semantic_context`.

- [ ] **Step 3: 补充 positive evidence smoke**

Add a positive `type_promotion` row to `04_generic_graph.json` through the `context_evidence` field from Step 1:

```json
{
  "kind": "convert",
  "transform_operation": "type_promotion",
  "context_evidence": {
    "type_promotion_stable_id": "type_promotion:Add:int:real",
    "type_promotion_operator": "Add",
    "type_promotion_source_pin_type": "int",
    "type_promotion_target_pin_type": "real",
    "type_promotion_result_pin_type": "real"
  },
  "args": {
    "left": { "kind": "literal", "value_type": "int", "value": 1 },
    "right": { "kind": "literal", "value_type": "real", "value": 1.5 }
  }
}
```

For `asset_action`, add positive smoke only after the smoke fixture can generate an `asset_action_stable_id` from an ActionDatabase projection step. Until that fixture exists, record `asset_action` positive proof as C++ automation and keep CLI smoke focused on no-evidence diagnostics.

- [ ] **Step 4: 运行四簇 smoke**

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\PlanArtifacts\GraphWriteFourClusterE2ESmoke_20260524\run_four_cluster_smoke.ps1"
```

Expected:

- Existing positive specs remain `preview_passed` and `executed`.
- No-evidence diagnostics remain accepted `preview_blocked`.
- New `type_promotion` positive evidence spec passes through TaskSpec JSON, TS compiler lowering, UE SemanticIR parsing, `GraphFragmentBuildRequest`, and `ActionResolution`.

- [ ] **Step 5: 更新设计文档**

Append to `BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`:

```markdown
## 2026-05-24 Remaining Gap Closure Update

- Generic `asset_action` closes the no-fake-success gap by selecting only spawners already present in UE `FBlueprintActionDatabase`. Missing or non-matching projected evidence still returns `needs_more_semantic_context` / `asset_action_spawner_not_found`.
- Generic `type_promotion` closes the projected evidence path by resolving only UE `FTypePromotion` registered operator spawners after typed pin promotion validation. Missing evidence remains an expected diagnostic.
- Function call/action-provider fragment lifecycle now routes through `FBlueprintHelperActionFragmentSpawnCoordinator`; `GraphStatementBuilder` keeps request shaping and delegates resolve/spawn/common fragment metadata.
- TaskSpec `context_evidence` now survives TS compiler lowering, UE SemanticIR parsing, fragment build request creation, and ActionResolution request projection.
```

- [ ] **Step 6: 更新 gap 文档**

In `BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`, replace the three relevant open rows with:

```markdown
| Gap | Status | Evidence |
|---|---|---|
| Generic `asset_action` no-fake-success / ActionDatabase spawner evidence | Closed in C++ ActionResolution | `AssetActionUsesActionDatabaseSpawnerEvidence`, `AssetActionNoSyntheticSpawner`, report paths in SmokeRecord. |
| Generic `type_promotion` projected typed spawner evidence | Closed in C++ ActionResolution | `TypePromotionRequiresEvidence`, `TypePromotionUsesProjectedSpawnerEvidence`, report paths in SmokeRecord. |
| Function shared adapter / lifecycle convergence | Closed for call/action-provider fragments | `FunctionFragmentLifecycleCoordinator`, `FunctionAction` regression report paths in SmokeRecord. |
| TaskSpec `context_evidence` projection for positive Generic smoke | Closed for `type_promotion` | Positive `type_promotion` smoke row and compiler/UE report paths in SmokeRecord. |
```

- [ ] **Step 7: 更新 SmokeRecord**

Append this section to `SmokeRecord_20260524_CN.md`:

```markdown
## Remaining Gap Repair Verification

- Generic focused automation: `D:\UEProjects\Template\Saved\Automation\GraphWrite_Generic_RemainingGaps_Final_001`, `failed=0`.
- Function focused automation: `D:\UEProjects\Template\Saved\Automation\GraphWrite_Function_RemainingGaps_Final_001`, `failed=0`.
- Contract automation: `D:\UEProjects\Template\Saved\Automation\GraphWrite_Contract_RemainingGaps_Final_001`, `failed=0`.
- Full GraphWrite automation: `D:\UEProjects\Template\Saved\Automation\GraphWrite_RemainingGaps_Full_Final_001`, `failed=0`.
- Four-cluster smoke: `run_four_cluster_smoke.ps1` completed; no-evidence diagnostics remained accepted `preview_blocked`; positive `type_promotion` evidence path passed.
- UE 5.6 build: `Result: Succeeded`.
- Editor/Bridge lifecycle harness issues were not written into bug docs.
```

- [ ] **Step 8: 运行最终验证**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Generic_RemainingGaps_Final_001'
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.FunctionAction;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Function_RemainingGaps_Final_001'
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Contract_RemainingGaps_Final_001'
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_RemainingGaps_Full_Final_001'
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core run test
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli run build
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\mcp run test
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
```

Expected:

- all automation report paths show `failed=0`,
- Node/CLI gates pass,
- UBT output ends with `Result: Succeeded`.

- [ ] **Step 9: 运行 source hygiene scans**

Run:

```powershell
rg -n "asset_action.*Resolved|type_promotion.*Resolved|UBlueprintNodeSpawner::Create|FindFunctionByName|fake success|unsupported.*success" BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite BlueprintHelper\Source\BlueprintHelper\Private\Tests\GraphWrite -g "*.cpp" -g "*.h"
```

Expected:

- `asset_action` resolved path appears only in `BlueprintHelperGenericAssetActionResolver.cpp` and is guarded by ActionDatabase-selected spawner evidence.
- `type_promotion` resolved path appears only in `BlueprintHelperTypePromotionSpawnerEvidenceResolver.cpp` and is guarded by typed evidence plus `FTypePromotion::GetOperatorSpawner`.
- `UBlueprintNodeSpawner::Create` remains only in existing concrete node operations such as `make_array`, `dynamic_cast`, `spawn_actor`-style direct Generic operations, not in `asset_action`.
- No new `FindFunctionByName`, `fake success`, or unsupported-success path appears.

## Self-Review

- Spec coverage:
  - `asset_action 不应伪造成功`: Task 1 selects only pointers present in `FBlueprintActionDatabase` and adds a contract forbidding `UBlueprintNodeSpawner::Create` in the asset action resolver.
  - `type_promotion 仍依赖 projected spawner evidence`: Task 2 requires operator and typed pin evidence, validates primitive promotion, and uses only `FTypePromotion::GetOperatorSpawner`.
  - `Function 共享 adapter/lifecycle 收敛债`: Task 3 moves Function call/action-provider resolve/spawn/common fragment lifecycle into `FBlueprintHelperActionFragmentSpawnCoordinator`.
  - 统一 smoke: Task 4 keeps no-evidence diagnostics and adds positive rows only when TaskSpec can carry evidence honestly.
- Red-flag scan:
  - 禁用词扫描已通过；每个修改步骤都有明确文件、代码片段或命令与期望结果。
- Type consistency:
  - Evidence uses existing `FBlueprintHelperActionResolutionRequest::ContextEvidence`.
  - Success paths return `FBlueprintHelperActionResolutionResult` with `SelectedSpawner`.
  - Missing-evidence paths keep `needs_more_semantic_context`.
