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

static FEdGraphPinType MakeFieldVariableActionTestStructPinType(UScriptStruct* Struct)
{
	return FEdGraphPinType(UEdGraphSchema_K2::PC_Struct, NAME_None, Struct, EPinContainerType::None, false, FEdGraphTerminalType());
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
	const FString& FieldOperation,
	const FString& FieldScope,
	const FString& Query)
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeFieldVariableActionTestObjectName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("field_variable_projected_context");
	Request.SemanticConstraintsHash = TEXT("field_variable_semantic_constraints");
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Field;
	Request.Semantic.FieldOperation = FieldOperation;
	Request.Semantic.FieldScope = FieldScope;
	Request.Semantic.Query = Query;
	Request.Semantic.TargetPath = Query;
	Request.MaxCandidates = 8;
	return Request;
}

static void AddProjectedFieldEvidence(FBlueprintHelperActionResolutionRequest& Request, const FString& FieldName)
{
	Request.ContextEvidence.Add(TEXT("field_name"), FieldName);
	Request.ContextEvidence.Add(TEXT("field_owner_class"), Request.Blueprint && Request.Blueprint->GeneratedClass
		? Request.Blueprint->GeneratedClass->GetPathName()
		: FString(TEXT("/Script/Engine.Actor")));
}

static void AddProjectedPropertyEvidence(
	FBlueprintHelperActionResolutionRequest& Request,
	const FString& OwnerPath,
	const FString& FieldName,
	const FString& PropertyPath)
{
	Request.Semantic.TargetPath = OwnerPath;
	Request.Semantic.PropertyPath = PropertyPath;
	Request.ContextEvidence.Add(TEXT("field_name"), FieldName);
	Request.ContextEvidence.Add(TEXT("field_owner_class"), OwnerPath);
	Request.ContextEvidence.Add(TEXT("property_path"), PropertyPath);
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
		TEXT("get"),
		TEXT("variable"),
		TEXT("SmokeFloat"));
	AddProjectedFieldEvidence(Request, TEXT("SmokeFloat"));
	Request.Semantic.ExpectedReturnType = TEXT("float");

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("one candidate"), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() > 0)
	{
		TestEqual(TEXT("display name"), Result.CandidateActions[0].DisplayName, FString(TEXT("SmokeFloat")));
		TestTrue(TEXT("stable id contains field get"), Result.CandidateActions[0].StableId.Contains(TEXT(":field:get:variable")));
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

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("set"),
		TEXT("variable"),
		TEXT("bSmokeFlag"));
	AddProjectedFieldEvidence(Request, TEXT("bSmokeFlag"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("one candidate"), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() > 0)
	{
		TestTrue(TEXT("stable id contains field set"), Result.CandidateActions[0].StableId.Contains(TEXT(":field:set:variable")));
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

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("get"),
		TEXT("variable"),
		TEXT("MissingVariable"));
	AddProjectedFieldEvidence(Request, TEXT("MissingVariable"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);

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
		TEXT("get"),
		TEXT("variable"),
		TEXT("Smoke"));
	Request.ContextEvidence.Add(TEXT("field_owner_class"), Request.Blueprint && Request.Blueprint->GeneratedClass
		? Request.Blueprint->GeneratedClass->GetPathName()
		: FString(TEXT("/Script/Engine.Actor")));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterResolvesDoorOpenAngleWithEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.P3.GetDoorOpenAngleRequiresProjectedEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterResolvesDoorOpenAngleWithEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestTrue(TEXT("add variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("DoorOpenAngle"),
		MakeFieldVariableActionTestPinType(UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float)));

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("get"),
		TEXT("variable"),
		TEXT("DoorOpenAngle"));
	AddProjectedFieldEvidence(Request, TEXT("DoorOpenAngle"));
	Request.Semantic.ExpectedReturnType = TEXT("float");

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("selected spawner"), Result.SelectedSpawner.IsValid());
	TestTrue(TEXT("stable id contains field"), Result.SelectedStableId.Contains(TEXT("DoorOpenAngle")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterResolvesBIsClosedWithEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.P3.SetBIsClosedRequiresProjectedEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterResolvesBIsClosedWithEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestTrue(TEXT("add variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("bIsClosed"),
		MakeFieldVariableActionTestPinType(UEdGraphSchema_K2::PC_Boolean)));

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("set"),
		TEXT("variable"),
		TEXT("bIsClosed"));
	AddProjectedFieldEvidence(Request, TEXT("bIsClosed"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("selected spawner"), Result.SelectedSpawner.IsValid());
	TestTrue(TEXT("stable id contains field set"), Result.SelectedStableId.Contains(TEXT(":field:set:variable")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterResolvesDoorMeshRelativeRotationPropertyWithEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.P3.GetPropertyDoorMeshRelativeRotationRequiresOwnerAndPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterResolvesDoorMeshRelativeRotationPropertyWithEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestTrue(TEXT("add variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("RelativeRotation"),
		MakeFieldVariableActionTestStructPinType(TBaseStructure<FRotator>::Get())));

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("get"),
		TEXT("property_path"),
		TEXT("DoorMesh.RelativeRotation"));
	AddProjectedPropertyEvidence(
		Request,
		TEXT("DoorMesh"),
		TEXT("RelativeRotation"),
		TEXT("DoorMesh.RelativeRotation"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("selected spawner"), Result.SelectedSpawner.IsValid());
	TestTrue(TEXT("stable id contains field get property_path"), Result.SelectedStableId.Contains(TEXT(":field:get:property_path")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterResolvesDoorMeshSimulatePhysicsPropertyWithEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.P3.SetPropertyDoorMeshSimulatePhysicsRequiresOwnerAndPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterResolvesDoorMeshSimulatePhysicsPropertyWithEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestTrue(TEXT("add variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("SimulatePhysics"),
		MakeFieldVariableActionTestPinType(UEdGraphSchema_K2::PC_Boolean)));

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("set"),
		TEXT("property_path"),
		TEXT("DoorMesh.SimulatePhysics"));
	AddProjectedPropertyEvidence(
		Request,
		TEXT("DoorMesh"),
		TEXT("SimulatePhysics"),
		TEXT("DoorMesh.SimulatePhysics"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("selected spawner"), Result.SelectedSpawner.IsValid());
	TestTrue(TEXT("stable id contains field set property_path"), Result.SelectedStableId.Contains(TEXT(":field:set:property_path")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterRejectsGetPropertyWithoutOwnerEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.P3.GetPropertyWithoutOwnerReturnsMissingRequiredEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterRejectsGetPropertyWithoutOwnerEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestTrue(TEXT("add variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("RelativeRotation"),
		MakeFieldVariableActionTestStructPinType(TBaseStructure<FRotator>::Get())));

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("get"),
		TEXT("property_path"),
		TEXT("DoorMesh.RelativeRotation"));
	Request.Semantic.PropertyPath = TEXT("DoorMesh.RelativeRotation");
	Request.ContextEvidence.Add(TEXT("field_name"), TEXT("RelativeRotation"));
	Request.ContextEvidence.Add(TEXT("property_path"), TEXT("DoorMesh.RelativeRotation"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("error code"), Result.ErrorCode, FString(TEXT("missing_required_evidence")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterRejectsSetPropertyWithoutPropertyPathTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.P3.SetPropertyWithoutPropertyPathReturnsMissingRequiredEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterRejectsSetPropertyWithoutPropertyPathTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestTrue(TEXT("add variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("SimulatePhysics"),
		MakeFieldVariableActionTestPinType(UEdGraphSchema_K2::PC_Boolean)));

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("set"),
		TEXT("property_path"),
		TEXT("DoorMesh.SimulatePhysics"));
	Request.ContextEvidence.Add(TEXT("field_name"), TEXT("SimulatePhysics"));
	Request.ContextEvidence.Add(TEXT("field_owner_class"), TEXT("DoorMesh"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("error code"), Result.ErrorCode, FString(TEXT("missing_required_evidence")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterRejectsBroadQueryWithoutProjectedFieldEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.P3.BroadQueryWithoutProjectedFieldEvidenceReturnsMissingRequiredEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterRejectsBroadQueryWithoutProjectedFieldEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestTrue(TEXT("add closed variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("bIsClosed"),
		MakeFieldVariableActionTestPinType(UEdGraphSchema_K2::PC_Boolean)));
	TestTrue(TEXT("add locked variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("bIsLocked"),
		MakeFieldVariableActionTestPinType(UEdGraphSchema_K2::PC_Boolean)));

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("get"),
		TEXT("variable"),
		TEXT("Is"));
	Request.bAllowFuzzyUnique = true;

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("error code"), Result.ErrorCode, FString(TEXT("missing_required_evidence")));
	TestEqual(TEXT("no selected id"), Result.SelectedStableId, FString());
	return true;
}

#endif
