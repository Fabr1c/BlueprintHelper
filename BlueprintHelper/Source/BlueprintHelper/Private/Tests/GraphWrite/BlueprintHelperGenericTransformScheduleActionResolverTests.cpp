#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintTypePromotion.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_PromotableOperator.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionDatabaseProjectionService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"
#include "UObject/Package.h"
#include "Tests/GraphWrite/BlueprintHelperScheduleTestUtils.h"

namespace
{
static FString MakeGenericTransformScheduleTestObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeGenericTransformScheduleTestBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperGenericTransformSchedule/%s"),
		*MakeGenericTransformScheduleTestObjectName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeGenericTransformScheduleTestObjectName(TEXT("BP_GenericTransformSchedule")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperGenericTransformScheduleTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* GetGenericTransformScheduleTestGraph(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}

static FBlueprintHelperActionResolutionRequest MakeGenericTransformScheduleRequest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	EBlueprintHelperActionSemanticKind SemanticKind)
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeGenericTransformScheduleTestObjectName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("generic_transform_schedule_projected_context");
	Request.SemanticConstraintsHash = TEXT("generic_transform_schedule_semantic_constraints");
	Request.Semantic.Kind = SemanticKind;
	Request.Semantic.SemanticFamily = SemanticKind == EBlueprintHelperActionSemanticKind::Convert
		? EBlueprintHelperActionSemanticFamily::Convert
		: EBlueprintHelperActionSemanticFamily::Schedule;
	Request.Semantic.SearchMode = TEXT("contextual");
	Request.Semantic.AmbiguityPolicy = TEXT("fail_on_ambiguity");
	Request.MaxCandidates = 8;
	return Request;
}

static FBlueprintHelperActionResolutionRequest MakeFunctionOwnerRequest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	EBlueprintHelperActionSemanticKind SemanticKind)
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeGenericTransformScheduleTestObjectName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("ambiguous_owner_projected_context");
	Request.SemanticConstraintsHash = TEXT("ambiguous_owner_semantic_constraints");
	Request.Semantic.Kind = SemanticKind;
	Request.Semantic.SemanticFamily = SemanticKind == EBlueprintHelperActionSemanticKind::Convert
		? EBlueprintHelperActionSemanticFamily::Convert
		: EBlueprintHelperActionSemanticFamily::Schedule;
	return Request;
}

static UBlueprintFunctionNodeSpawner* FindTypePromotionSpawnerForTest(FName OperatorName)
{
	if (OperatorName.IsNone())
	{
		return nullptr;
	}

	FTypePromotion::Get();
	if (UBlueprintFunctionNodeSpawner* Spawner = FTypePromotion::GetOperatorSpawner(OperatorName))
	{
		return Spawner;
	}

	FBlueprintActionDatabase::Get().RefreshAll();
	return FTypePromotion::GetOperatorSpawner(OperatorName);
}

static void AddScheduleEvidence(
	const FBlueprintHelperProjectedScheduleActionEvidence& Evidence,
	FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperProjectedSpawnerEvidence::WriteScheduleActionEvidence(Evidence, Request.ContextEvidence);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericTransformConvertRequiresOperationTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Convert.RequiresGenericOperation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericTransformConvertRequiresOperationTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericTransformScheduleTestBlueprint();
	UEdGraph* Graph = GetGenericTransformScheduleTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericTransformScheduleRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Convert));

	TestEqual(TEXT("missing convert operation status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("missing convert operation code"), Result.ErrorCode, FString(TEXT("needs_more_semantic_context")));
	TestEqual(TEXT("missing convert cluster"), Result.ClusterKind, EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
	TestFalse(TEXT("missing convert has no fake spawner"), Result.SelectedSpawner.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericTransformDynamicCastResolvesTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Convert.DynamicCast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericTransformDynamicCastResolvesTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericTransformScheduleTestBlueprint();
	UEdGraph* Graph = GetGenericTransformScheduleTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperActionResolutionRequest Request =
		MakeGenericTransformScheduleRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Convert);
	Request.Semantic.TransformOperation = TEXT("dynamic_cast");
	Request.Semantic.ClassPath = AActor::StaticClass()->GetPathName();
	Request.Semantic.TargetPath = Request.Semantic.ClassPath;

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	TestEqual(TEXT("dynamic cast status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("dynamic cast cluster"), Result.ClusterKind, EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
	TestTrue(TEXT("dynamic cast stable id names generic transform"), Result.SelectedStableId.Contains(TEXT("generic_transform:dynamic_cast")));
	TestTrue(TEXT("dynamic cast selected spawner"), Result.SelectedSpawner.IsValid());
	TestTrue(TEXT("dynamic cast node class"), Result.NodeClass.Contains(TEXT("K2Node_DynamicCast")));
	TestTrue(TEXT("dynamic cast match reason"), Result.MatchReason.Contains(TEXT("generic_transform operation=dynamic_cast")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericTransformTypePromotionRequiresEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Convert.TypePromotionRequiresEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericTransformTypePromotionRequiresEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericTransformScheduleTestBlueprint();
	UEdGraph* Graph = GetGenericTransformScheduleTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	struct FTypePromotionEvidenceCase
	{
		const TCHAR* Label = TEXT("");
		const TCHAR* OperatorName = nullptr;
		const TCHAR* SourcePinType = nullptr;
		const TCHAR* TargetPinType = nullptr;
	};

	const TArray<FTypePromotionEvidenceCase> Cases = {
		{ TEXT("no projected evidence"), nullptr, nullptr, nullptr },
		{ TEXT("missing operator"), nullptr, TEXT("int"), TEXT("real") },
		{ TEXT("missing source pin type"), TEXT("Add"), nullptr, TEXT("real") },
		{ TEXT("missing target pin type"), TEXT("Add"), TEXT("int"), nullptr }
	};

	for (const FTypePromotionEvidenceCase& TestCase : Cases)
	{
		FBlueprintHelperActionResolutionRequest Request =
			MakeGenericTransformScheduleRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Convert);
		Request.Semantic.TransformOperation = TEXT("type_promotion");
		if (TestCase.OperatorName)
		{
			Request.ContextEvidence.Add(TEXT("type_promotion_operator"), TestCase.OperatorName);
		}
		if (TestCase.SourcePinType)
		{
			Request.ContextEvidence.Add(TEXT("type_promotion_source_pin_type"), TestCase.SourcePinType);
		}
		if (TestCase.TargetPinType)
		{
			Request.ContextEvidence.Add(TEXT("type_promotion_target_pin_type"), TestCase.TargetPinType);
		}

		const FBlueprintHelperActionResolutionResult Result =
			FBlueprintHelperActionResolutionCore::Resolve(Request);

		TestEqual(FString::Printf(TEXT("%s status"), TestCase.Label), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
		TestEqual(FString::Printf(TEXT("%s evidence code"), TestCase.Label), Result.ErrorCode, FString(TEXT("needs_more_semantic_context")));
		TestTrue(FString::Printf(TEXT("%s message"), TestCase.Label), Result.Message.Contains(TEXT("type_promotion")));
		TestFalse(FString::Printf(TEXT("%s has no fake spawner"), TestCase.Label), Result.SelectedSpawner.IsValid());
	}
	return true;
}

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

	UBlueprintFunctionNodeSpawner* ExpectedSpawner = FindTypePromotionSpawnerForTest(TEXT("Add"));
	if (!TestNotNull(TEXT("registered Add type promotion spawner"), ExpectedSpawner))
	{
		return false;
	}

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
	bPassed &= TestNotNull(TEXT("type promotion selected spawner"), Result.SelectedSpawner.Get());
	bPassed &= TestTrue(TEXT("type promotion selected spawner uses FTypePromotion registration"), Result.SelectedSpawner.Get() == ExpectedSpawner);
	bPassed &= TestTrue(TEXT("type promotion candidate count"), Result.CandidateActions.Num() == 1);
	if (Result.CandidateActions.Num() == 1)
	{
		const FBlueprintHelperCallFunctionCandidateInfo& Candidate = Result.CandidateActions[0];
		bPassed &= TestTrue(TEXT("type promotion candidate is database backed"), Candidate.bFromActionDatabase);
		bPassed &= TestTrue(TEXT("type promotion candidate node class"), Candidate.NodeClassPath.Contains(UK2Node_PromotableOperator::StaticClass()->GetName()));
		bPassed &= TestTrue(TEXT("type promotion candidate match reason"), Candidate.MatchReason.Contains(TEXT("type_promotion")) && Candidate.MatchReason.Contains(TEXT("FTypePromotion")));
	}
	bPassed &= TestTrue(TEXT("type promotion node class"), Result.NodeClass.Contains(UK2Node_PromotableOperator::StaticClass()->GetName()));
	bPassed &= TestTrue(TEXT("type promotion match reason"), Result.MatchReason.Contains(TEXT("type_promotion")) && Result.MatchReason.Contains(TEXT("FTypePromotion")));
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericScheduleRequiresOperationTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Schedule.RequiresGenericOperation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericScheduleRequiresOperationTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericTransformScheduleTestBlueprint();
	UEdGraph* Graph = GetGenericTransformScheduleTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericTransformScheduleRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Schedule));

	TestEqual(TEXT("missing schedule operation status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("missing schedule operation code"), Result.ErrorCode, FString(TEXT("needs_more_semantic_context")));
	TestEqual(TEXT("missing schedule cluster"), Result.ClusterKind, EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
	TestFalse(TEXT("missing schedule has no fake spawner"), Result.SelectedSpawner.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericScheduleRequiresSpawnerEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Schedule.RequiresSpawnerEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericScheduleRequiresSpawnerEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericTransformScheduleTestBlueprint();
	UEdGraph* Graph = GetGenericTransformScheduleTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const FString Operations[] = { TEXT("timer_delegate_node"), TEXT("latent_or_async_node") };
	for (const FString& Operation : Operations)
	{
		FBlueprintHelperActionResolutionRequest Request =
			MakeGenericTransformScheduleRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Schedule);
		Request.Semantic.ScheduleOperation = Operation;

		const FBlueprintHelperActionResolutionResult Result =
			FBlueprintHelperActionResolutionCore::Resolve(Request);

		TestEqual(FString::Printf(TEXT("%s status"), *Operation), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
		TestEqual(FString::Printf(TEXT("%s code"), *Operation), Result.ErrorCode, FString(TEXT("schedule_spawner_evidence_missing")));
		TestFalse(FString::Printf(TEXT("%s has no fake spawner"), *Operation), Result.SelectedSpawner.IsValid());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericScheduleTimerRequiresHandlerEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Schedule.TimerRequiresHandlerEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericScheduleTimerRequiresHandlerEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericTransformScheduleTestBlueprint();
	UEdGraph* Graph = GetGenericTransformScheduleTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperProjectedScheduleActionEvidence Evidence;
	Evidence.StableId = TEXT("missing");
	Evidence.NodeClassPath = TEXT("/Script/BlueprintGraph.K2Node_CallFunction");
	Evidence.SpawnerSignature = TEXT("missing");
	Evidence.OwnerPath = TEXT("/Script/Engine.KismetSystemLibrary");

	FBlueprintHelperActionResolutionRequest Request =
		MakeGenericTransformScheduleRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Schedule);
	Request.Semantic.ScheduleOperation = TEXT("timer_delegate_node");
	AddScheduleEvidence(Evidence, Request);

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	TestEqual(TEXT("timer handler evidence status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("timer handler evidence code"), Result.ErrorCode, FString(TEXT("handler_evidence_missing")));
	TestFalse(TEXT("timer missing handler has no selected spawner"), Result.SelectedSpawner.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericScheduleLatentRequiresGraphPermissionTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Schedule.LatentRequiresGraphPermission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericScheduleLatentRequiresGraphPermissionTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericTransformScheduleTestBlueprint();
	UEdGraph* Graph = GetGenericTransformScheduleTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperProjectedScheduleActionEvidence Evidence;
	Evidence.StableId = TEXT("missing");
	Evidence.NodeClassPath = TEXT("/Script/BlueprintGraph.K2Node_AsyncAction");
	Evidence.SpawnerSignature = TEXT("missing");
	Evidence.OwnerPath = TEXT("/Script/Engine");
	Evidence.GraphLatentAllowed = TEXT("false");

	FBlueprintHelperActionResolutionRequest Request =
		MakeGenericTransformScheduleRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Schedule);
	Request.Semantic.ScheduleOperation = TEXT("latent_or_async_node");
	AddScheduleEvidence(Evidence, Request);

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	TestEqual(TEXT("latent permission status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("latent permission code"), Result.ErrorCode, FString(TEXT("latent_function_not_allowed_in_graph")));
	TestFalse(TEXT("latent not allowed has no selected spawner"), Result.SelectedSpawner.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericScheduleProjectedEvidenceNotFoundTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Schedule.ProjectedEvidenceNotFound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericScheduleProjectedEvidenceNotFoundTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericTransformScheduleTestBlueprint();
	UEdGraph* Graph = GetGenericTransformScheduleTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperProjectedScheduleActionEvidence Evidence;
	Evidence.StableId = TEXT("action_database:/missing:/missing:/missing");
	Evidence.NodeClassPath = TEXT("/Script/BlueprintGraph.K2Node_CallFunction");
	Evidence.SpawnerSignature = TEXT("missing");
	Evidence.OwnerPath = TEXT("/Script/Engine.KismetSystemLibrary");
	Evidence.HandlerName = TEXT("HandleTimerElapsed");
	Evidence.HandlerFunctionPath = TEXT("/Game/BP/BP_Timer.HandleTimerElapsed");
	Evidence.HandlerSourceCluster = TEXT("BlueprintSignature");
	Evidence.SignatureEvidenceId = TEXT("signature:function:HandleTimerElapsed");

	FBlueprintHelperActionResolutionRequest Request =
		MakeGenericTransformScheduleRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Schedule);
	Request.Semantic.ScheduleOperation = TEXT("timer_delegate_node");
	AddScheduleEvidence(Evidence, Request);

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	TestEqual(TEXT("projected evidence not found status"), Result.Status, EBlueprintHelperActionResolutionStatus::NotFound);
	TestEqual(TEXT("projected evidence not found code"), Result.ErrorCode, FString(TEXT("schedule_spawner_not_found")));
	TestFalse(TEXT("not found has no selected spawner"), Result.SelectedSpawner.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericScheduleLatentUsesProjectedSpawnerEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Schedule.LatentUsesProjectedSpawnerEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericScheduleLatentUsesProjectedSpawnerEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericTransformScheduleTestBlueprint();
	UEdGraph* Graph = GetGenericTransformScheduleTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperProjectedScheduleActionEvidence Evidence;
	if (!TryProjectScheduleEvidence(Blueprint, Graph, TEXT("Async Load Primary Asset"), Evidence))
	{
		AddError(TEXT("Unable to project a unique Async Load Primary Asset ActionDatabase schedule candidate."));
		return false;
	}
	Evidence.GraphLatentAllowed = TEXT("true");

	FBlueprintHelperActionResolutionRequest Request =
		MakeGenericTransformScheduleRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Schedule);
	Request.Semantic.ScheduleOperation = TEXT("latent_or_async_node");
	AddScheduleEvidence(Evidence, Request);

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	bool bPassed = true;
	bPassed &= TestEqual(TEXT("latent schedule status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	bPassed &= TestTrue(TEXT("latent schedule spawner is current ActionDatabase spawner"), Result.SelectedSpawner.IsValid());
	bPassed &= TestTrue(TEXT("latent schedule candidate count"), Result.CandidateActions.Num() == 1);
	if (Result.CandidateActions.Num() == 1)
	{
		bPassed &= TestTrue(TEXT("latent schedule candidate is database backed"), Result.CandidateActions[0].bFromActionDatabase);
	}
	bPassed &= TestTrue(TEXT("latent schedule match reason"), Result.MatchReason.Contains(TEXT("latent_or_async_node")));
	bPassed &= TestEqual(TEXT("latent schedule stable id"), Result.SelectedStableId, Evidence.StableId);
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericScheduleTimerUsesProjectedSpawnerEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Schedule.TimerUsesProjectedSpawnerEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericScheduleTimerUsesProjectedSpawnerEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericTransformScheduleTestBlueprint();
	UEdGraph* Graph = GetGenericTransformScheduleTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperProjectedScheduleActionEvidence Evidence;
	if (!TryProjectScheduleEvidenceFromQueries(
		Blueprint,
		Graph,
		{ TEXT("Set Timer by Event"), TEXT("Set Timer by Delegate"), TEXT("Set Timer") },
		Evidence))
	{
		AddError(TEXT("Unable to project a timer ActionDatabase schedule candidate."));
		return false;
	}
	Evidence.HandlerName = TEXT("HandleTimerElapsed");
	Evidence.HandlerFunctionPath = TEXT("/Game/BP/BP_Timer.HandleTimerElapsed");
	Evidence.HandlerSourceCluster = TEXT("BlueprintSignature");
	Evidence.SignatureEvidenceId = TEXT("signature:function:HandleTimerElapsed");

	FBlueprintHelperActionResolutionRequest Request =
		MakeGenericTransformScheduleRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Schedule);
	Request.Semantic.ScheduleOperation = TEXT("timer_delegate_node");
	AddScheduleEvidence(Evidence, Request);

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	bool bPassed = true;
	bPassed &= TestEqual(TEXT("timer schedule status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	bPassed &= TestTrue(TEXT("timer schedule spawner is current ActionDatabase spawner"), Result.SelectedSpawner.IsValid());
	bPassed &= TestTrue(TEXT("timer schedule candidate count"), Result.CandidateActions.Num() == 1);
	if (Result.CandidateActions.Num() == 1)
	{
		bPassed &= TestTrue(TEXT("timer schedule candidate is database backed"), Result.CandidateActions[0].bFromActionDatabase);
	}
	bPassed &= TestTrue(TEXT("timer schedule match reason"), Result.MatchReason.Contains(TEXT("timer_delegate_node")));
	bPassed &= TestEqual(TEXT("timer schedule stable id"), Result.SelectedStableId, Evidence.StableId);
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperConvertScheduleAmbiguousOwnerRejectedTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.ConvertSchedule.AmbiguousOwnerRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperConvertScheduleAmbiguousOwnerRejectedTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericTransformScheduleTestBlueprint();
	UEdGraph* Graph = GetGenericTransformScheduleTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperActionResolutionRequest ConvertRequest =
		MakeFunctionOwnerRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Convert);
	ConvertRequest.Semantic.FunctionOperation = TEXT("convert_function");
	ConvertRequest.Semantic.TransformOperation = TEXT("dynamic_cast");

	const FBlueprintHelperActionResolutionResult ConvertResult =
		FBlueprintHelperActionResolutionCore::Resolve(ConvertRequest);
	TestEqual(TEXT("ambiguous convert status"), ConvertResult.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("ambiguous convert code"), ConvertResult.ErrorCode, FString(TEXT("ambiguous_generic_function_owner")));

	FBlueprintHelperActionResolutionRequest ScheduleRequest =
		MakeFunctionOwnerRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Schedule);
	ScheduleRequest.Semantic.FunctionOperation = TEXT("schedule_function");
	ScheduleRequest.Semantic.ScheduleOperation = TEXT("timer_delegate_node");

	const FBlueprintHelperActionResolutionResult ScheduleResult =
		FBlueprintHelperActionResolutionCore::Resolve(ScheduleRequest);
	TestEqual(TEXT("ambiguous schedule status"), ScheduleResult.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("ambiguous schedule code"), ScheduleResult.ErrorCode, FString(TEXT("ambiguous_generic_function_owner")));
	return true;
}

#endif
