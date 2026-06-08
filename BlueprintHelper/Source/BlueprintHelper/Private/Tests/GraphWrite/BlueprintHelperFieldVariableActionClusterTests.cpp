#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "Components/SceneComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_FunctionEntry.h"
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

static UEdGraph* AddFieldVariableActionFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName)
{
	if (!Blueprint || FunctionName.IsEmpty())
	{
		return nullptr;
	}

	UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*FunctionName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass());
	if (!FunctionGraph)
	{
		return nullptr;
	}

	FBlueprintEditorUtils::AddFunctionGraph<UFunction>(
		Blueprint,
		FunctionGraph,
		/*bIsUserCreated=*/ true,
		nullptr);
	return FunctionGraph;
}

static UK2Node_FunctionEntry* FindFieldVariableActionFunctionEntry(UEdGraph* FunctionGraph)
{
	if (!FunctionGraph)
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
		{
			return Entry;
		}
	}
	return nullptr;
}

static bool AddFieldVariableActionFunctionInputPin(
	UBlueprint* Blueprint,
	UEdGraph* FunctionGraph,
	const FString& PinName,
	const FEdGraphPinType& PinType)
{
	UK2Node_FunctionEntry* Entry = FindFieldVariableActionFunctionEntry(FunctionGraph);
	if (!Blueprint || !Entry || PinName.IsEmpty())
	{
		return false;
	}

	TSharedPtr<FUserPinInfo> NewPin = MakeShared<FUserPinInfo>();
	NewPin->PinName = FName(*PinName);
	NewPin->PinType = PinType;
	NewPin->DesiredPinDirection = EGPD_Output;
	Entry->UserDefinedPins.Add(NewPin);
	Entry->ReconstructNode();
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

static FBlueprintHelperCallFunctionPinType MakeFieldVariableActionCallPinType(const FString& Category, const FString& ObjectPath = FString())
{
	FBlueprintHelperCallFunctionPinType PinType;
	PinType.Category = Category;
	PinType.ObjectPath = ObjectPath;
	return PinType;
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
	FBlueprintHelperFieldVariableActionClusterResolvesFunctionLocalGetAndSetTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.FunctionScope.LocalGetSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterResolvesFunctionLocalGetAndSetTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);
	UEdGraph* FunctionGraph = AddFieldVariableActionFunctionGraph(Blueprint, TEXT("ComputeLocalSpeed"));
	TestNotNull(TEXT("function graph"), FunctionGraph);
	TestNotNull(TEXT("function entry"), FindFieldVariableActionFunctionEntry(FunctionGraph));
	if (!Blueprint || !FunctionGraph)
	{
		return false;
	}

	TestTrue(TEXT("add local variable"), FBlueprintEditorUtils::AddLocalVariable(
		Blueprint,
		FunctionGraph,
		TEXT("LocalSpeed"),
		MakeFieldVariableActionTestPinType(UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float)));
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	FBlueprintHelperActionResolutionRequest GetRequest = MakeFieldVariableActionRequest(
		Blueprint,
		FunctionGraph,
		TEXT("get"),
		TEXT("variable"),
		TEXT("LocalSpeed"));
	GetRequest.Semantic.CapabilityId = TEXT("field.local_get");
	GetRequest.Semantic.CapabilityFacts.Add(TEXT("field.member_name"), TEXT("LocalSpeed"));
	GetRequest.Semantic.CapabilityFacts.Add(TEXT("field.local_scope"), TEXT("ComputeLocalSpeed"));
	GetRequest.Semantic.CapabilityFacts.Add(TEXT("field.function_name"), TEXT("ComputeLocalSpeed"));

	const FBlueprintHelperActionResolutionResult GetResult = FBlueprintHelperActionResolutionCore::Resolve(GetRequest);
	TestEqual(TEXT("local get status"), GetResult.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("local get selected stable id"), GetResult.SelectedStableId.Contains(TEXT("field.local_get")));
	TestTrue(TEXT("local get spawner"), GetResult.SelectedSpawner.IsValid());
	TestEqual(TEXT("local get one candidate"), GetResult.CandidateActions.Num(), 1);
	if (GetResult.CandidateActions.Num() > 0)
	{
		const FBlueprintHelperActionCandidate& Candidate = GetResult.CandidateActions[0];
		TestEqual(TEXT("local get capability"), Candidate.CapabilityId, FString(TEXT("field.local_get")));
		TestTrue(TEXT("local get node class"), Candidate.NodeClassPath.Contains(TEXT("K2Node_VariableGet")));
		TestEqual(TEXT("local get scope fact"), Candidate.CapabilityFacts.FindRef(TEXT("field.local_scope")), FString(TEXT("ComputeLocalSpeed")));
		TestEqual(TEXT("local get kind fact"), Candidate.CapabilityFacts.FindRef(TEXT("field.kind")), FString(TEXT("local")));
	}

	FBlueprintHelperActionResolutionRequest SetRequest = MakeFieldVariableActionRequest(
		Blueprint,
		FunctionGraph,
		TEXT("set"),
		TEXT("variable"),
		TEXT("LocalSpeed"));
	SetRequest.Semantic.CapabilityId = TEXT("field.local_set");
	SetRequest.Semantic.CapabilityFacts = GetRequest.Semantic.CapabilityFacts;

	const FBlueprintHelperActionResolutionResult SetResult = FBlueprintHelperActionResolutionCore::Resolve(SetRequest);
	TestEqual(TEXT("local set status"), SetResult.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("local set spawner"), SetResult.SelectedSpawner.IsValid());
	TestEqual(TEXT("local set one candidate"), SetResult.CandidateActions.Num(), 1);
	if (SetResult.CandidateActions.Num() > 0)
	{
		const FBlueprintHelperActionCandidate& Candidate = SetResult.CandidateActions[0];
		TestEqual(TEXT("local set capability"), Candidate.CapabilityId, FString(TEXT("field.local_set")));
		TestTrue(TEXT("local set node class"), Candidate.NodeClassPath.Contains(TEXT("K2Node_VariableSet")));
		TestEqual(TEXT("local set scope fact"), Candidate.CapabilityFacts.FindRef(TEXT("field.local_scope")), FString(TEXT("ComputeLocalSpeed")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterResolvesFunctionParamGetTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.FunctionScope.ParamGet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterResolvesFunctionParamGetTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);
	UEdGraph* FunctionGraph = AddFieldVariableActionFunctionGraph(Blueprint, TEXT("ComputeInputSpeed"));
	TestNotNull(TEXT("function graph"), FunctionGraph);
	TestNotNull(TEXT("function entry"), FindFieldVariableActionFunctionEntry(FunctionGraph));
	if (!Blueprint || !FunctionGraph)
	{
		return false;
	}

	TestTrue(TEXT("add function input pin"), AddFieldVariableActionFunctionInputPin(
		Blueprint,
		FunctionGraph,
		TEXT("InputSpeed"),
		MakeFieldVariableActionTestPinType(UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float)));

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		FunctionGraph,
		TEXT("get"),
		TEXT("variable"),
		TEXT("InputSpeed"));
	Request.Semantic.CapabilityId = TEXT("field.function_param_get");
	Request.Semantic.CapabilityFacts.Add(TEXT("field.member_name"), TEXT("InputSpeed"));
	Request.Semantic.CapabilityFacts.Add(TEXT("field.function_name"), TEXT("ComputeInputSpeed"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("function param status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("function param selected stable id"), Result.SelectedStableId.Contains(TEXT("field.function_param_get")));
	TestTrue(TEXT("function param spawner"), Result.SelectedSpawner.IsValid());
	TestEqual(TEXT("function param one candidate"), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() > 0)
	{
		const FBlueprintHelperActionCandidate& Candidate = Result.CandidateActions[0];
		TestEqual(TEXT("function param capability"), Candidate.CapabilityId, FString(TEXT("field.function_param_get")));
		TestTrue(TEXT("function param node class"), Candidate.NodeClassPath.Contains(TEXT("K2Node_VariableGet")));
		TestEqual(TEXT("function param kind fact"), Candidate.CapabilityFacts.FindRef(TEXT("field.kind")), FString(TEXT("function_param")));
		TestEqual(TEXT("function param function fact"), Candidate.CapabilityFacts.FindRef(TEXT("field.function_name")), FString(TEXT("ComputeInputSpeed")));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterComponentRefResolvesTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.ComponentRefResolves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterComponentRefResolvesTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestTrue(TEXT("add component ref variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("DoorMesh"),
		FEdGraphPinType(UEdGraphSchema_K2::PC_Object, NAME_None, USceneComponent::StaticClass(), EPinContainerType::None, false, FEdGraphTerminalType())));

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("get"),
		TEXT("component_ref"),
		TEXT("DoorMesh"));
	Request.ContextEvidence.Add(TEXT("component_property_name"), TEXT("DoorMesh"));
	Request.ContextEvidence.Add(TEXT("component_binding_owner_class_path"), TEXT("/Game/Test/BP_Door.BP_Door_C"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("component ref stable id"), Result.SelectedStableId.StartsWith(TEXT("field_component_ref:")));
	TestEqual(TEXT("one candidate"), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() > 0)
	{
		TestEqual(TEXT("candidate category"), Result.CandidateActions[0].Category, FString(TEXT("field_component_ref")));
		TestTrue(TEXT("match reason contains scope"), Result.CandidateActions[0].MatchReason.Contains(TEXT("field_scope=component_ref")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterFieldAccessRequiresOwnerTypeTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.FieldAccessRequiresOwnerType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterFieldAccessRequiresOwnerTypeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestTrue(TEXT("add field access variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("RelativeRotation"),
		MakeFieldVariableActionTestStructPinType(TBaseStructure<FRotator>::Get())));

	FBlueprintHelperActionResolutionRequest MissingOwnerRequest = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("get"),
		TEXT("field_access"),
		TEXT("RelativeRotation"));
	MissingOwnerRequest.Semantic.PropertyPath = TEXT("RelativeRotation");

	const FBlueprintHelperActionResolutionResult MissingOwnerResult = FBlueprintHelperActionResolutionCore::Resolve(MissingOwnerRequest);
	TestEqual(TEXT("missing owner status"), MissingOwnerResult.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("missing owner error"), MissingOwnerResult.ErrorCode, FString(TEXT("needs_more_semantic_context")));

	FBlueprintHelperActionResolutionRequest Request = MissingOwnerRequest;
	Request.Semantic.TargetObjectPinType = MakeFieldVariableActionCallPinType(TEXT("object"), TEXT("/Script/Engine.SceneComponent"));
	Request.ContextEvidence.Add(TEXT("property_path"), TEXT("RelativeRotation"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("field access stable id"), Result.SelectedStableId.StartsWith(TEXT("field_access:")));
	if (Result.CandidateActions.Num() > 0)
	{
		TestEqual(TEXT("candidate category"), Result.CandidateActions[0].Category, FString(TEXT("field_access")));
		TestTrue(TEXT("match reason contains scope"), Result.CandidateActions[0].MatchReason.Contains(TEXT("field_scope=field_access")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterFieldAccessUsesOwnerClassFieldTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.FieldAccessUsesOwnerClassField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterFieldAccessUsesOwnerClassFieldTest::RunTest(const FString& Parameters)
{
	UBlueprint* GraphBlueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("graph blueprint"), GraphBlueprint);

	UBlueprint* OwnerBlueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("owner blueprint"), OwnerBlueprint);
	TestTrue(TEXT("add owner field"), AddFieldVariableActionTestVariable(
		OwnerBlueprint,
		TEXT("SavedHealth"),
		MakeFieldVariableActionTestPinType(UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float)));

	const FString OwnerClassPath = OwnerBlueprint && OwnerBlueprint->GeneratedClass
		? OwnerBlueprint->GeneratedClass->GetPathName()
		: FString();
	TestFalse(TEXT("owner class path"), OwnerClassPath.IsEmpty());

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		GraphBlueprint,
		GetFieldVariableActionTestGraph(GraphBlueprint),
		TEXT("get"),
		TEXT("field_access"),
		TEXT("SavedHealth"));
	Request.Semantic.TargetPath = TEXT("LoadedVitalsSave");
	Request.Semantic.PropertyPath = TEXT("SavedHealth");
	Request.Semantic.TargetObjectType = OwnerClassPath;
	Request.Semantic.CapabilityFacts.Add(TEXT("field.owner_class"), OwnerClassPath);
	Request.Semantic.CapabilityFacts.Add(TEXT("field.member_name"), TEXT("SavedHealth"));
	Request.ContextEvidence.Add(TEXT("field_owner_class"), OwnerClassPath);
	Request.ContextEvidence.Add(TEXT("target_object_type"), OwnerClassPath);
	Request.ContextEvidence.Add(TEXT("field_name"), TEXT("SavedHealth"));
	Request.ContextEvidence.Add(TEXT("property_path"), TEXT("SavedHealth"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("field access stable id"), Result.SelectedStableId.StartsWith(TEXT("field_access:")));
	TestEqual(TEXT("one candidate"), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() > 0)
	{
		TestEqual(TEXT("candidate display name"), Result.CandidateActions[0].DisplayName, FString(TEXT("SavedHealth")));
		TestEqual(TEXT("candidate owner"), Result.CandidateActions[0].OwnerClassPath, OwnerClassPath);
		TestEqual(TEXT("candidate category"), Result.CandidateActions[0].Category, FString(TEXT("field_access")));
		TestTrue(TEXT("match reason contains member"), Result.CandidateActions[0].MatchReason.Contains(TEXT("field=SavedHealth")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterComplexPropertyPathKeepsFullPathEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.ComplexPropertyPathKeepsFullPathEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterComplexPropertyPathKeepsFullPathEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestTrue(TEXT("add complex leaf variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("Pitch"),
		MakeFieldVariableActionTestPinType(UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float)));

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("set"),
		TEXT("property_path"),
		TEXT("DoorMesh"));
	Request.Semantic.TargetPath = TEXT("DoorMesh");
	Request.Semantic.PropertyPath = TEXT("RelativeRotation.Pitch");
	Request.ContextEvidence.Add(TEXT("field_name"), TEXT("Pitch"));
	Request.ContextEvidence.Add(TEXT("field_owner_class"), TEXT("/Script/Engine.SceneComponent"));
	Request.ContextEvidence.Add(TEXT("property_path"), TEXT("RelativeRotation.Pitch"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("requires fragment builder"), Result.bRequiresDedicatedFragmentBuilder);
	TestEqual(TEXT("match reason"), Result.MatchReason, FString(TEXT("complex_property_path_requires_field_path_fragment_builder")));
	TestTrue(TEXT("stable id keeps property path scope"), Result.SelectedStableId.Contains(TEXT(":field:set:property_path")));
	if (Result.CandidateActions.Num() > 0)
	{
		TestTrue(TEXT("candidate keeps composed full path"), Result.CandidateActions[0].MatchReason.Contains(TEXT("path=DoorMesh.RelativeRotation.Pitch")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterLinkedPinInfersExpectedReturnTypeTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.LinkedPinInfersExpectedReturnType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterLinkedPinInfersExpectedReturnTypeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestTrue(TEXT("add float variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("SmokeFloat"),
		MakeFieldVariableActionTestPinType(UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float)));
	TestTrue(TEXT("add string variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("SmokeString"),
		MakeFieldVariableActionTestPinType(UEdGraphSchema_K2::PC_String)));

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("get"),
		TEXT("variable"),
		TEXT("Smoke"));
	Request.ContextEvidence.Add(TEXT("field_owner_class"), TEXT("/Game/Test/BP_Door.BP_Door_C"));
	Request.Semantic.ExpectedReturnPinType = MakeFieldVariableActionCallPinType(UEdGraphSchema_K2::PC_String.ToString());
	Request.bAllowFuzzyUnique = true;

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("selected id"), Result.SelectedStableId.Contains(TEXT("SmokeString")), true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterCapabilityStableIdTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.CapabilityStableId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterCapabilityStableIdTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestTrue(TEXT("add variable"), AddFieldVariableActionTestVariable(
		Blueprint,
		TEXT("Health"),
		MakeFieldVariableActionTestPinType(UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float)));

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("get"),
		TEXT("variable"),
		TEXT("Health"));
	Request.Semantic.CapabilityId = TEXT("field.member_get");
	Request.Semantic.CapabilityFacts.Add(TEXT("field.member_name"), TEXT("Health"));
	AddProjectedFieldEvidence(Request, TEXT("Health"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("one candidate"), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() > 0)
	{
		const FBlueprintHelperActionCandidate& Candidate = Result.CandidateActions[0];
		TestEqual(TEXT("capability id"), Candidate.CapabilityId, FString(TEXT("field.member_get")));
		TestEqual(TEXT("expected node family"), Candidate.ExpectedNodeFamily, FString(TEXT("variable_get")));
		TestTrue(TEXT("stable id contains capability"), Candidate.StableId.Contains(TEXT("field.member_get")));
		TestEqual(TEXT("resolved member"), Candidate.CapabilityFacts.FindRef(TEXT("field.member_name")), FString(TEXT("Health")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterComponentRefUsesVariableGetTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.ComponentRef.UsesVariableGet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterComponentRefUsesVariableGetTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);

	FEdGraphPinType ComponentPinType(UEdGraphSchema_K2::PC_Object, NAME_None, USceneComponent::StaticClass(), EPinContainerType::None, false, FEdGraphTerminalType());
	TestTrue(TEXT("add component-like object property"), AddFieldVariableActionTestVariable(Blueprint, TEXT("MeshComponent"), ComponentPinType));

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("get"),
		TEXT("component_ref"),
		TEXT("MeshComponent"));
	Request.Semantic.CapabilityId = TEXT("field.component_ref_get");
	Request.Semantic.CapabilityFacts.Add(TEXT("field.component_name"), TEXT("MeshComponent"));
	Request.Semantic.CapabilityFacts.Add(TEXT("field.component_owner_class"), Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetPathName() : FString());
	Request.ContextEvidence.Add(TEXT("component_name"), TEXT("MeshComponent"));
	Request.ContextEvidence.Add(TEXT("component_kind"), TEXT("scs_or_native_property"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("candidate count"), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() > 0)
	{
		const FBlueprintHelperActionCandidate& Candidate = Result.CandidateActions[0];
		TestEqual(TEXT("capability"), Candidate.CapabilityId, FString(TEXT("field.component_ref_get")));
		TestTrue(TEXT("variable get node"), Candidate.NodeClassPath.Contains(TEXT("K2Node_VariableGet")));
		TestFalse(TEXT("not add component"), Candidate.NodeClassPath.Contains(TEXT("K2Node_AddComponent")));
		TestEqual(TEXT("component name fact"), Candidate.CapabilityFacts.FindRef(TEXT("field.component_name")), FString(TEXT("MeshComponent")));
		TestEqual(TEXT("component spawner readback"), Candidate.ReadbackFacts.FindRef(TEXT("component_ref_spawner")), FString(TEXT("UBlueprintVariableNodeSpawner")));
		TestTrue(TEXT("component class readback"), Candidate.ReadbackFacts.FindRef(TEXT("component_property_class")).Contains(TEXT("SceneComponent")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterComponentRefRejectsPlainObjectPropertyTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.ComponentRef.RejectsPlainObjectProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterComponentRefRejectsPlainObjectPropertyTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);

	FEdGraphPinType ObjectPinType(UEdGraphSchema_K2::PC_Object, NAME_None, UObject::StaticClass(), EPinContainerType::None, false, FEdGraphTerminalType());
	TestTrue(TEXT("add plain object property"), AddFieldVariableActionTestVariable(Blueprint, TEXT("PayloadObject"), ObjectPinType));

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("get"),
		TEXT("component_ref"),
		TEXT("PayloadObject"));
	Request.Semantic.CapabilityId = TEXT("field.component_ref_get");
	Request.Semantic.CapabilityFacts.Add(TEXT("field.component_name"), TEXT("PayloadObject"));
	Request.ContextEvidence.Add(TEXT("component_name"), TEXT("PayloadObject"));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::NotFound);
	TestEqual(TEXT("error code"), Result.ErrorCode, FString(TEXT("not_class_component_property")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldVariableActionClusterObjectPinMemberGetRequiresTargetPinTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FieldVariable.ObjectPinMemberGet.RequiresTargetPin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldVariableActionClusterObjectPinMemberGetRequiresTargetPinTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldVariableActionTestBlueprint();
	TestNotNull(TEXT("blueprint"), Blueprint);

	FBlueprintHelperActionResolutionRequest Request = MakeFieldVariableActionRequest(
		Blueprint,
		GetFieldVariableActionTestGraph(Blueprint),
		TEXT("get"),
		TEXT("field_access"),
		TEXT("Tags"));
	Request.Semantic.CapabilityId = TEXT("field.object_pin_member_get");
	Request.Semantic.CapabilityFacts.Add(TEXT("field.target_pin_ref"), TEXT("node:OwnerActor pin:ReturnValue"));
	Request.Semantic.CapabilityFacts.Add(TEXT("field.owner_class"), TEXT("/Script/Engine.Actor"));
	Request.Semantic.CapabilityFacts.Add(TEXT("field.member_name"), TEXT("Tags"));
	Request.Semantic.CapabilityFacts.Add(TEXT("field.target_pin_type"), UEdGraphSchema_K2::PC_Object.ToString());
	Request.Semantic.CapabilityFacts.Add(TEXT("field.target_pin_object_path"), TEXT("/Script/Engine.Actor"));
	Request.ContextEvidence.Add(TEXT("target_pin_ref"), Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_ref")));
	Request.ContextEvidence.Add(TEXT("linked_pin_type_category"), Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_type")));
	Request.ContextEvidence.Add(TEXT("linked_pin_type_object_path"), Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_object_path")));

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestEqual(TEXT("candidate count"), Result.CandidateActions.Num(), 1);
	if (Result.CandidateActions.Num() > 0)
	{
		const FBlueprintHelperActionCandidate& Candidate = Result.CandidateActions[0];
		TestEqual(TEXT("capability id"), Candidate.CapabilityId, FString(TEXT("field.object_pin_member_get")));
		TestEqual(TEXT("target pin category"), Candidate.CapabilityFacts.FindRef(TEXT("field.target_pin_type")), FString(UEdGraphSchema_K2::PC_Object.ToString()));
		TestEqual(TEXT("target pin object"), Candidate.CapabilityFacts.FindRef(TEXT("field.target_pin_object_path")), FString(TEXT("/Script/Engine.Actor")));
		TestTrue(TEXT("stable id includes target pin class"), Candidate.StableId.Contains(TEXT("/Script/Engine.Actor")));
	}
	return true;
}

#endif
