#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace
{
static FString MakeFieldVariableActionTestObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeFieldVariableActionTestBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperFieldVariableAction/%s"),
		*MakeFieldVariableActionTestObjectName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeFieldVariableActionTestObjectName(TEXT("BP_FieldVariableAction")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperFieldVariableActionTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* GetFieldVariableActionTestGraph(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}

static FEdGraphPinType MakeFieldVariableActionTestPinType(const FName Category, const FName SubCategory = NAME_None)
{
	return FEdGraphPinType(Category, SubCategory, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
}

static bool AddFieldVariableActionTestVariable(UBlueprint* Blueprint, const FString& Name, const FEdGraphPinType& Type)
{
	if (!Blueprint || !FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*Name), Type))
	{
		return false;
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	return true;
}

static FBlueprintHelperActionResolutionRequest MakeFieldVariableActionRequest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const EBlueprintHelperActionSemanticKind Semantic,
	const FString& Query)
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.Semantic.Kind = Semantic;
	Request.Semantic.Query = Query;
	Request.Semantic.TargetPath = Query;
	Request.MaxCandidates = 8;
	return Request;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterResolvesGetTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.ResolvesGet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterResolvesGetTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestTrue(TEXT("add variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("SmokeFloat"),
		MakeFieldVariableActionTestPinType(UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float)));

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		EBlueprintHelperActionSemanticKind::Get,
		TEXT("SmokeFloat"));
	Request.Semantic.ExpectedReturnType = TEXT("float");

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("one candidate"), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() > 0)
	{
		TestEqual(TEXT("display name"), Result.CandidateActions[0].DisplayName, FString(TEXT("SmokeFloat")));
		TestTrue(TEXT("stable id contains get"), Result.CandidateActions[0].StableId.Contains(TEXT(":get")));
		TestTrue(TEXT("node class is get"), Result.CandidateActions[0].NodeClassPath.Contains(TEXT("K2Node_VariableGet")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterResolvesSetTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.ResolvesSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterResolvesSetTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestTrue(TEXT("add variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("bSmokeFlag"),
		MakeFieldVariableActionTestPinType(UEdGraphSchema_K2::PC_Boolean)));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(
		MakeFieldVariableActionRequest(
			Blueprint,
			GetFieldVariableActionTestGraph(Blueprint),
			EBlueprintHelperActionSemanticKind::Set,
			TEXT("bSmokeFlag")));

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("one candidate"), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() > 0)
	{
		TestTrue(TEXT("stable id contains set"), Result.CandidateActions[0].StableId.Contains(TEXT(":set")));
		TestTrue(TEXT("node class is set"), Result.CandidateActions[0].NodeClassPath.Contains(TEXT("K2Node_VariableSet")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterNotFoundTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.NotFound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterNotFoundTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(
		MakeFieldVariableActionRequest(
			Blueprint,
			GetFieldVariableActionTestGraph(Blueprint),
			EBlueprintHelperActionSemanticKind::Get,
			TEXT("MissingVariable")));

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::NotFound);
	TestEqual(TEXT("no candidates"), Result.CandidateActions.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterAmbiguousTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.Ambiguous",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterAmbiguousTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestTrue(TEXT("add label variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("SmokeLabel"),
		MakeFieldVariableActionTestPinType(UEdGraphSchema_K2::PC_String)));
	TestTrue(TEXT("add float variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("SmokeFloat"),
		MakeFieldVariableActionTestPinType(UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float)));

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		EBlueprintHelperActionSemanticKind::Get,
		TEXT("Smoke"));
	Request.bAllowFuzzyUnique = true;

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Ambiguous);
	TestTrue(TEXT("multiple candidates"), Result.CandidateActions.Num() >= 2);
	if (Result.CandidateActions.Num() > 0)
	{
		TestEqual(TEXT("candidate cluster category"), Result.CandidateActions[0].Category, FString(TEXT("field_variable")));
		TestFalse(TEXT("candidate stable id"), Result.CandidateActions[0].StableId.IsEmpty());
		TestFalse(TEXT("candidate display name"), Result.CandidateActions[0].DisplayName.IsEmpty());
		TestFalse(TEXT("candidate field name"), Result.CandidateActions[0].NativeFunctionName.IsEmpty());
		TestFalse(TEXT("candidate pin type"), Result.CandidateActions[0].ReturnType.IsEmpty());
		TestTrue(TEXT("candidate score"), Result.CandidateActions[0].Score > 0);
		TestFalse(TEXT("candidate reason"), Result.CandidateActions[0].MatchReason.IsEmpty());
	}
	return true;
}

#endif
