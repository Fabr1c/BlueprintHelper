#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace
{
static FString MakeArrayIdenticalObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeArrayIdenticalBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperArrayIdentical/%s"),
		*MakeArrayIdenticalObjectName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeArrayIdenticalObjectName(TEXT("BP_ArrayIdentical")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperArrayIdenticalOpTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static FBlueprintHelperActionResolutionRequest MakeArrayIdenticalRequest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FString& LhsPinType = FString(),
	const FString& RhsPinType = FString())
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeArrayIdenticalObjectName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("array_identical_projected_context");
	Request.SemanticConstraintsHash = TEXT("array_identical_semantic_constraints");
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Op;
	Request.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Operator;
	Request.Semantic.FunctionOperation = TEXT("op.array_identical");
	Request.ContextEvidence.Add(TEXT("op.operation_id"), TEXT("array_identical"));
	if (!LhsPinType.IsEmpty())
	{
		Request.ContextEvidence.Add(TEXT("op.array_lhs_pin_type"), LhsPinType);
	}
	if (!RhsPinType.IsEmpty())
	{
		Request.ContextEvidence.Add(TEXT("op.array_rhs_pin_type"), RhsPinType);
	}
	return Request;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperArrayIdenticalOpMissingTypedPinsTest,
	"BlueprintHelper.GraphWrite.OpCoverage.ArrayIdentical.MissingTypedPinsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperArrayIdenticalOpMissingTypedPinsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeArrayIdenticalBlueprint();
	UEdGraph* Graph = Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(MakeArrayIdenticalRequest(Blueprint, Graph));
	TestEqual(TEXT("missing typed pins status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("missing typed pins code"), Result.ErrorCode, FString(TEXT("array_typed_pin_missing")));
	TestNotEqual(TEXT("array_identical does not fall back to equal"), Result.SelectedStableId, FString(TEXT("promotable_operator:EqualEqual")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperArrayIdenticalOpMismatchedTypedPinsTest,
	"BlueprintHelper.GraphWrite.OpCoverage.ArrayIdentical.MismatchedTypedPinsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperArrayIdenticalOpMismatchedTypedPinsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeArrayIdenticalBlueprint();
	UEdGraph* Graph = Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(
		MakeArrayIdenticalRequest(Blueprint, Graph, TEXT("array|int"), TEXT("array|bool")));
	TestEqual(TEXT("mismatched typed pins status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("mismatched typed pins code"), Result.ErrorCode, FString(TEXT("array_typed_pin_mismatch")));
	TestNotEqual(TEXT("array_identical does not fall back to equal"), Result.SelectedStableId, FString(TEXT("promotable_operator:EqualEqual")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperArrayIdenticalOpStructElementMismatchTest,
	"BlueprintHelper.GraphWrite.OpCoverage.ArrayIdentical.StructElementMismatchRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperArrayIdenticalOpStructElementMismatchTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeArrayIdenticalBlueprint();
	UEdGraph* Graph = Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(
		MakeArrayIdenticalRequest(
			Blueprint,
			Graph,
			TEXT("array|struct|/Script/CoreUObject.Vector"),
			TEXT("array|struct|/Script/CoreUObject.Rotator")));
	TestEqual(TEXT("struct element mismatch status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("struct element mismatch code"), Result.ErrorCode, FString(TEXT("array_typed_pin_mismatch")));

	const FBlueprintHelperActionResolutionResult NamedResult = FBlueprintHelperActionResolutionCore::Resolve(
		MakeArrayIdenticalRequest(
			Blueprint,
			Graph,
			TEXT("category=struct|object=/Script/CoreUObject.Vector|container=array"),
			TEXT("category=struct|object=/Script/CoreUObject.Rotator|container=array")));
	TestEqual(TEXT("named struct element mismatch status"), NamedResult.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("named struct element mismatch code"), NamedResult.ErrorCode, FString(TEXT("array_typed_pin_mismatch")));

	const FBlueprintHelperActionResolutionResult ObjectResult = FBlueprintHelperActionResolutionCore::Resolve(
		MakeArrayIdenticalRequest(
			Blueprint,
			Graph,
			TEXT("category=object|object=/Script/Engine.Actor|container=array"),
			TEXT("category=object|object=/Script/Engine.Pawn|container=array")));
	TestEqual(TEXT("object element mismatch status"), ObjectResult.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("object element mismatch code"), ObjectResult.ErrorCode, FString(TEXT("array_typed_pin_mismatch")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperArrayIdenticalOpValidTypedPinsTest,
	"BlueprintHelper.GraphWrite.OpCoverage.ArrayIdentical.ValidTypedPinsPermitCallArrayFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperArrayIdenticalOpValidTypedPinsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeArrayIdenticalBlueprint();
	UEdGraph* Graph = Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(
		MakeArrayIdenticalRequest(Blueprint, Graph, TEXT("array|int"), TEXT("array|int")));
	TestEqual(TEXT("valid typed pins status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("valid typed pins stable id"), Result.SelectedStableId, FString(TEXT("/Script/Engine.KismetArrayLibrary:Array_Identical")));
	TestEqual(TEXT("valid typed pins node class"), Result.FunctionCandidate.NodeClassPath, FString(TEXT("/Script/BlueprintGraph.K2Node_CallArrayFunction")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperArrayIdenticalOpNamedStructTypedPinsTest,
	"BlueprintHelper.GraphWrite.OpCoverage.ArrayIdentical.NamedStructTypedPinsPermitCallArrayFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperArrayIdenticalOpNamedStructTypedPinsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeArrayIdenticalBlueprint();
	UEdGraph* Graph = Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(
		MakeArrayIdenticalRequest(
			Blueprint,
			Graph,
			TEXT("category=struct|object=/Script/CoreUObject.Vector|container=array"),
			TEXT("category=struct|object=/Script/CoreUObject.Vector|container=array")));
	TestEqual(TEXT("named struct typed pins status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("named struct typed pins stable id"), Result.SelectedStableId, FString(TEXT("/Script/Engine.KismetArrayLibrary:Array_Identical")));
	TestEqual(TEXT("named struct typed pins node class"), Result.FunctionCandidate.NodeClassPath, FString(TEXT("/Script/BlueprintGraph.K2Node_CallArrayFunction")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperArrayIdenticalOpNamedObjectTypedPinsTest,
	"BlueprintHelper.GraphWrite.OpCoverage.ArrayIdentical.NamedObjectTypedPinsPermitCallArrayFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperArrayIdenticalOpNamedObjectTypedPinsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeArrayIdenticalBlueprint();
	UEdGraph* Graph = Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(
		MakeArrayIdenticalRequest(
			Blueprint,
			Graph,
			TEXT("category=object|object=/Script/Engine.Actor|container=array"),
			TEXT("category=object|object=/Script/Engine.Actor|container=array")));
	TestEqual(TEXT("named object typed pins status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("named object typed pins stable id"), Result.SelectedStableId, FString(TEXT("/Script/Engine.KismetArrayLibrary:Array_Identical")));
	TestEqual(TEXT("named object typed pins node class"), Result.FunctionCandidate.NodeClassPath, FString(TEXT("/Script/BlueprintGraph.K2Node_CallArrayFunction")));
	return true;
}

#endif
