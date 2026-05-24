#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_MakeArray.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace
{
static FString MakeGenericCreateTestObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeGenericCreateTestBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperGenericCreate/%s"),
		*MakeGenericCreateTestObjectName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeGenericCreateTestObjectName(TEXT("BP_GenericCreate")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperGenericCreateActionResolverTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* GetGenericCreateTestGraph(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}

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

static bool AssertAssetActionWeakSelectorResult(
	FAutomationTestBase& Test,
	const FBlueprintHelperActionResolutionResult& Result)
{
	bool bPassed = true;
	bPassed &= Test.TestTrue(
		TEXT("weak asset action selector is not classified as missing semantic context"),
		!(Result.Status == EBlueprintHelperActionResolutionStatus::InvalidRequest
			&& Result.ErrorCode == TEXT("needs_more_semantic_context")));

	if (Result.Status == EBlueprintHelperActionResolutionStatus::Resolved)
	{
		bPassed &= Test.TestNotNull(TEXT("weak selector selected spawner"), Result.SelectedSpawner.Get());
		bPassed &= Test.TestTrue(TEXT("weak selector spawner comes from ActionDatabase"), IsSpawnerPointerFromActionDatabaseForTest(Result.SelectedSpawner.Get()));
		bPassed &= Test.TestTrue(TEXT("weak selector candidate is ActionDatabase-backed"), Result.CandidateActions.Num() == 1 && Result.CandidateActions[0].bFromActionDatabase);
		return bPassed;
	}

	if (Result.Status == EBlueprintHelperActionResolutionStatus::Ambiguous)
	{
		bPassed &= Test.TestEqual(TEXT("weak selector ambiguous error"), Result.ErrorCode, FString(TEXT("asset_action_spawner_ambiguous")));
		bPassed &= Test.TestFalse(TEXT("weak selector ambiguous has no selected spawner"), Result.SelectedSpawner.IsValid());
		return bPassed;
	}

	Test.AddError(FString::Printf(
		TEXT("Weak asset action selector must resolve or report ambiguity, but status=%d error=%s message=%s"),
		static_cast<int32>(Result.Status),
		*Result.ErrorCode,
		*Result.Message));
	return false;
}

static FBlueprintHelperActionResolutionRequest MakeGenericCreateResolverRequest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FString& CreateOperation,
	const FString& ClassPath = FString(),
	const FString& PinType = FString(),
	const FString& KeyPinType = FString(),
	const FString& ValuePinType = FString(),
	const FString& AssetPath = FString())
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeGenericCreateTestObjectName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("generic_create_projected_context");
	Request.SemanticConstraintsHash = TEXT("generic_create_semantic_constraints");
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Create;
	Request.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Create;
	Request.Semantic.CreateOperation = CreateOperation;
	Request.Semantic.Query = CreateOperation;
	Request.Semantic.ClassPath = ClassPath;
	Request.Semantic.TargetPath = ClassPath;
	Request.Semantic.TypeName = ClassPath;
	Request.Semantic.AssetPath = AssetPath;
	if (!PinType.IsEmpty())
	{
		Request.Semantic.ArgumentTypes.Add(TEXT("element"), PinType);
	}
	if (!KeyPinType.IsEmpty())
	{
		Request.Semantic.ArgumentTypes.Add(TEXT("key"), KeyPinType);
	}
	if (!ValuePinType.IsEmpty())
	{
		Request.Semantic.ArgumentTypes.Add(TEXT("value"), ValuePinType);
	}
	Request.MaxCandidates = 8;
	return Request;
}

static bool HasGenericCreateCandidateNodeClassPath(
	const FBlueprintHelperActionResolutionResult& Result,
	const FString& ExpectedNodeClassPathPart)
{
	for (const FBlueprintHelperCallFunctionCandidateInfo& Candidate : Result.CandidateActions)
	{
		if (Candidate.NodeClassPath.Contains(ExpectedNodeClassPathPart))
		{
			return true;
		}
	}
	return false;
}

static bool AssertGenericCreateResolved(
	FAutomationTestBase& Test,
	const FBlueprintHelperActionResolutionResult& Result,
	const FString& ExpectedOperation,
	const FString& ExpectedNodeClassPathPart)
{
	bool bPassed = true;
	bPassed &= Test.TestEqual(TEXT("create result status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	bPassed &= Test.TestEqual(TEXT("create result cluster"), Result.ClusterKind, EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
	bPassed &= Test.TestNotNull(TEXT("create selected spawner"), Result.SelectedSpawner.Get());
	bPassed &= Test.TestTrue(TEXT("create stable id names operation"), Result.SelectedStableId.Contains(FString(TEXT("generic_create:")) + ExpectedOperation));
	bPassed &= Test.TestTrue(TEXT("create candidate records node class"), HasGenericCreateCandidateNodeClassPath(Result, ExpectedNodeClassPathPart));
	bPassed &= Test.TestTrue(TEXT("create match reason records operation"), Result.MatchReason.Contains(FString(TEXT("create_operation=")) + ExpectedOperation));
	return bPassed;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericCreateRequiresOperationTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Create.RequiresOperation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericCreateRequiresOperationTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericCreateTestBlueprint();
	UEdGraph* Graph = GetGenericCreateTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericCreateResolverRequest(Blueprint, Graph, FString(), AActor::StaticClass()->GetPathName()));

	TestEqual(TEXT("missing create operation status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("missing create operation error"), Result.ErrorCode, FString(TEXT("needs_more_semantic_context")));
	TestFalse(TEXT("missing create operation has no spawner"), Result.SelectedSpawner.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericCreateConcreteOperationsTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Create.ConcreteOperations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericCreateConcreteOperationsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericCreateTestBlueprint();
	UEdGraph* Graph = GetGenericCreateTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	bool bPassed = true;
	bPassed &= AssertGenericCreateResolved(
		*this,
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericCreateResolverRequest(Blueprint, Graph, TEXT("spawn_actor"), AActor::StaticClass()->GetPathName())),
		TEXT("spawn_actor"),
		TEXT("K2Node_SpawnActorFromClass"));
	bPassed &= AssertGenericCreateResolved(
		*this,
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericCreateResolverRequest(Blueprint, Graph, TEXT("create_widget"), TEXT("/Script/UMG.UserWidget"))),
		TEXT("create_widget"),
		TEXT("K2Node_CreateWidget"));
	bPassed &= AssertGenericCreateResolved(
		*this,
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericCreateResolverRequest(Blueprint, Graph, TEXT("construct_object"), UObject::StaticClass()->GetPathName())),
		TEXT("construct_object"),
		TEXT("K2Node_GenericCreateObject"));
	bPassed &= AssertGenericCreateResolved(
		*this,
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericCreateResolverRequest(Blueprint, Graph, TEXT("make_array"), FString(), TEXT("int"))),
		TEXT("make_array"),
		TEXT("K2Node_MakeArray"));
	bPassed &= AssertGenericCreateResolved(
		*this,
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericCreateResolverRequest(Blueprint, Graph, TEXT("make_map"), FString(), FString(), TEXT("name"), TEXT("int"))),
		TEXT("make_map"),
		TEXT("K2Node_MakeMap"));
	bPassed &= AssertGenericCreateResolved(
		*this,
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericCreateResolverRequest(Blueprint, Graph, TEXT("make_set"), FString(), TEXT("name"))),
		TEXT("make_set"),
		TEXT("K2Node_MakeSet"));
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericCreateAssetActionRequiresSpawnerEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Create.AssetActionRequiresSpawnerEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericCreateAssetActionRequiresSpawnerEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericCreateTestBlueprint();
	UEdGraph* Graph = GetGenericCreateTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericCreateResolverRequest(Blueprint, Graph, TEXT("asset_action"), FString(), FString(), FString(), FString(), TEXT("/Game/MissingAsset")));

	TestEqual(TEXT("asset action create status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("asset action create error"), Result.ErrorCode, FString(TEXT("needs_more_semantic_context")));
	TestFalse(TEXT("asset action create has no selected spawner"), Result.SelectedSpawner.IsValid());
	return true;
}

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericCreateAssetActionAllowsNodeClassOnlySelectorTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Create.AssetActionAllowsNodeClassOnlySelector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericCreateAssetActionAllowsNodeClassOnlySelectorTest::RunTest(const FString& Parameters)
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
	Request.ContextEvidence.Add(TEXT("asset_action_node_class"), NodeClassPath);

	return AssertAssetActionWeakSelectorResult(*this, FBlueprintHelperActionResolutionCore::Resolve(Request));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericCreateAssetActionAllowsQueryOnlySelectorTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Create.AssetActionAllowsQueryOnlySelector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericCreateAssetActionAllowsQueryOnlySelectorTest::RunTest(const FString& Parameters)
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
	Request.ContextEvidence.Add(TEXT("asset_action_query"), MenuName);

	return AssertAssetActionWeakSelectorResult(*this, FBlueprintHelperActionResolutionCore::Resolve(Request));
}

#endif
