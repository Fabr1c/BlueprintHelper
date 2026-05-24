#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "BlueprintActionDatabase.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace
{
static FString MakeUnifiedSmokeObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeUnifiedSmokeBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperFunctionFieldUnifiedSmoke/%s"),
		*MakeUnifiedSmokeObjectName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeUnifiedSmokeObjectName(TEXT("BP_FunctionFieldUnifiedSmoke")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperGraphWriteFunctionFieldUnifiedSmokeTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* GetUnifiedSmokeGraph(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}

static FEdGraphPinType MakeUnifiedSmokePinType(const FName Category, const FName SubCategory = NAME_None)
{
	return FEdGraphPinType(Category, SubCategory, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
}

static FEdGraphPinType MakeUnifiedSmokeStructPinType(UScriptStruct* Struct)
{
	return FEdGraphPinType(UEdGraphSchema_K2::PC_Struct, NAME_None, Struct, EPinContainerType::None, false, FEdGraphTerminalType());
}

static bool AddUnifiedSmokeVariable(UBlueprint* Blueprint, const FString& Name, const FEdGraphPinType& Type)
{
	if (!Blueprint || !FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*Name), Type))
	{
		return false;
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	return true;
}

static FBlueprintHelperCallFunctionPinType MakeUnifiedSmokeCallPinType(
	const FString& Category,
	const FString& ObjectPath = FString())
{
	FBlueprintHelperCallFunctionPinType PinType;
	PinType.Category = Category;
	PinType.ObjectPath = ObjectPath;
	return PinType;
}

static FBlueprintHelperActionResolutionRequest MakeUnifiedFunctionRequest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query)
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeUnifiedSmokeObjectName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("unified_function_projected_context");
	Request.SemanticConstraintsHash = TEXT("unified_function_semantic_constraints");
	Request.Semantic.Kind = SemanticKind;
	Request.Semantic.Query = Query;
	Request.Semantic.SearchMode = TEXT("contextual");
	Request.Semantic.AmbiguityPolicy = TEXT("fail_on_ambiguity");
	Request.MaxCandidates = 16;
	return Request;
}

static FBlueprintHelperActionResolutionRequest MakeUnifiedFieldRequest(
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
	Request.StatementId = MakeUnifiedSmokeObjectName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("unified_field_projected_context");
	Request.SemanticConstraintsHash = TEXT("unified_field_semantic_constraints");
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Field;
	Request.Semantic.Query = Query;
	Request.Semantic.TargetPath = Query;
	Request.Semantic.FieldOperation = FieldOperation;
	Request.Semantic.FieldScope = FieldScope;
	Request.MaxCandidates = 8;
	return Request;
}

static FString BuildPluginRoot()
{
	return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("BlueprintHelper"));
}

static bool LoadRequiredSmokeSource(FAutomationTestBase& Test, const FString& FilePath, FString& OutText)
{
	if (!IFileManager::Get().FileExists(*FilePath))
	{
		Test.AddError(FString::Printf(TEXT("Required smoke source file is missing: %s"), *FilePath));
		return false;
	}
	if (!FFileHelper::LoadFileToString(OutText, *FilePath))
	{
		Test.AddError(FString::Printf(TEXT("Required smoke source file could not be read: %s"), *FilePath));
		return false;
	}
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteUnifiedSmokeFunctionConvertAndScheduleTest,
	"BlueprintHelper.GraphWrite.FunctionFieldUnifiedSmoke.FunctionConvertAndSchedule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteUnifiedSmokeFunctionConvertAndScheduleTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeUnifiedSmokeBlueprint();
	UEdGraph* Graph = GetUnifiedSmokeGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	FBlueprintActionDatabase::Get().RefreshAll();

	FBlueprintHelperActionResolutionRequest ConvertRequest =
		MakeUnifiedFunctionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Convert, TEXT("PrintString"));
	ConvertRequest.Semantic.FunctionOperation = TEXT("convert_function");
	ConvertRequest.Semantic.TransformOperation = TEXT("convert");
	ConvertRequest.Semantic.ArgumentNames.Add(TEXT("InString"));
	ConvertRequest.Semantic.ArgumentPinTypes.Add(TEXT("InString"), MakeUnifiedSmokeCallPinType(TEXT("string")));

	const FBlueprintHelperActionResolutionResult ConvertResult =
		FBlueprintHelperActionResolutionCore::Resolve(ConvertRequest);
	TestEqual(TEXT("convert status"), ConvertResult.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestNotEqual(TEXT("convert is not unsupported"), ConvertResult.ErrorCode, FString(TEXT("unsupported_function_cluster_semantic")));
	TestEqual(TEXT("convert cluster"), ConvertResult.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
	TestEqual(TEXT("convert stable id"), ConvertResult.SelectedStableId, FString(TEXT("/Script/Engine.KismetSystemLibrary:PrintString")));

	FBlueprintHelperActionResolutionRequest ScheduleRequest =
		MakeUnifiedFunctionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Schedule, TEXT("Delay"));
	ScheduleRequest.Semantic.FunctionOperation = TEXT("latent_or_async_function");
	ScheduleRequest.Semantic.ScheduleOperation = TEXT("latent_or_async");
	ScheduleRequest.ContextEvidence.Add(TEXT("graph_latent_allowed"), TEXT("false"));

	const FBlueprintHelperActionResolutionResult ScheduleResult =
		FBlueprintHelperActionResolutionCore::Resolve(ScheduleRequest);
	TestEqual(TEXT("latent schedule status"), ScheduleResult.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("latent schedule error"), ScheduleResult.ErrorCode, FString(TEXT("latent_function_not_allowed_in_graph")));
	TestNotEqual(TEXT("schedule is not unsupported"), ScheduleResult.ErrorCode, FString(TEXT("unsupported_function_cluster_semantic")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteUnifiedSmokeFieldComponentRefAndFieldAccessTest,
	"BlueprintHelper.GraphWrite.FunctionFieldUnifiedSmoke.FieldComponentRefAndFieldAccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteUnifiedSmokeFieldComponentRefAndFieldAccessTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeUnifiedSmokeBlueprint();
	UEdGraph* Graph = GetUnifiedSmokeGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	TestTrue(TEXT("add DoorMesh variable"), AddUnifiedSmokeVariable(
		Blueprint,
		TEXT("DoorMesh"),
		MakeUnifiedSmokePinType(UEdGraphSchema_K2::PC_Object)));
	TestTrue(TEXT("add RelativeRotation variable"), AddUnifiedSmokeVariable(
		Blueprint,
		TEXT("RelativeRotation"),
		MakeUnifiedSmokeStructPinType(TBaseStructure<FRotator>::Get())));
	TestTrue(TEXT("add Pitch variable"), AddUnifiedSmokeVariable(
		Blueprint,
		TEXT("Pitch"),
		MakeUnifiedSmokePinType(UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float)));

	FBlueprintHelperActionResolutionRequest ComponentRequest =
		MakeUnifiedFieldRequest(Blueprint, Graph, TEXT("get"), TEXT("component_ref"), TEXT("DoorMesh"));
	ComponentRequest.ContextEvidence.Add(TEXT("component_property_name"), TEXT("DoorMesh"));
	ComponentRequest.ContextEvidence.Add(TEXT("component_binding_owner_class_path"), TEXT("/Game/Test/BP_Door.BP_Door_C"));

	const FBlueprintHelperActionResolutionResult ComponentResult =
		FBlueprintHelperActionResolutionCore::Resolve(ComponentRequest);
	TestEqual(TEXT("component_ref status"), ComponentResult.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("component_ref stable id"), ComponentResult.SelectedStableId.StartsWith(TEXT("field_component_ref:")));
	TestTrue(TEXT("component_ref category"), ComponentResult.CandidateActions.Num() > 0
		&& ComponentResult.CandidateActions[0].Category == TEXT("field_component_ref"));

	FBlueprintHelperActionResolutionRequest FieldAccessRequest =
		MakeUnifiedFieldRequest(Blueprint, Graph, TEXT("get"), TEXT("field_access"), TEXT("RelativeRotation"));
	FieldAccessRequest.Semantic.PropertyPath = TEXT("RelativeRotation");
	FieldAccessRequest.Semantic.TargetObjectPinType =
		MakeUnifiedSmokeCallPinType(TEXT("object"), TEXT("/Script/Engine.SceneComponent"));

	const FBlueprintHelperActionResolutionResult FieldAccessResult =
		FBlueprintHelperActionResolutionCore::Resolve(FieldAccessRequest);
	TestEqual(TEXT("field_access status"), FieldAccessResult.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("field_access stable id"), FieldAccessResult.SelectedStableId.StartsWith(TEXT("field_access:")));
	TestTrue(TEXT("field_access category"), FieldAccessResult.CandidateActions.Num() > 0
		&& FieldAccessResult.CandidateActions[0].Category == TEXT("field_access"));

	FBlueprintHelperActionResolutionRequest ComplexPathRequest =
		MakeUnifiedFieldRequest(Blueprint, Graph, TEXT("set"), TEXT("property_path"), TEXT("DoorMesh"));
	ComplexPathRequest.Semantic.TargetPath = TEXT("DoorMesh");
	ComplexPathRequest.Semantic.PropertyPath = TEXT("RelativeRotation.Pitch");
	ComplexPathRequest.ContextEvidence.Add(TEXT("field_name"), TEXT("Pitch"));
	ComplexPathRequest.ContextEvidence.Add(TEXT("field_owner_class"), TEXT("/Script/Engine.SceneComponent"));
	ComplexPathRequest.ContextEvidence.Add(TEXT("property_path"), TEXT("RelativeRotation.Pitch"));

	const FBlueprintHelperActionResolutionResult ComplexPathResult =
		FBlueprintHelperActionResolutionCore::Resolve(ComplexPathRequest);
	TestEqual(TEXT("complex property path status"), ComplexPathResult.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestTrue(TEXT("complex property path requires fragment builder"), ComplexPathResult.bRequiresDedicatedFragmentBuilder);
	TestEqual(TEXT("complex property path reason"), ComplexPathResult.MatchReason, FString(TEXT("complex_property_path_requires_field_path_fragment_builder")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteUnifiedSmokeEventSignatureBoundaryTest,
	"BlueprintHelper.GraphWrite.FunctionFieldUnifiedSmoke.EventSignatureBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteUnifiedSmokeEventSignatureBoundaryTest::RunTest(const FString& Parameters)
{
	const FString EventDelegateClusterPath = FPaths::Combine(
		BuildPluginRoot(),
		TEXT("BlueprintHelper"),
		TEXT("Source"),
		TEXT("BlueprintHelper"),
		TEXT("Private"),
		TEXT("Systems"),
		TEXT("ToolClusters"),
		TEXT("GraphWrite"),
		TEXT("ActionResolution"),
		TEXT("BlueprintHelperEventDelegateActionCluster.cpp"));
	const FString TaskCompilerPath = FPaths::Combine(
		BuildPluginRoot(),
		TEXT("AgentFaceService"),
		TEXT("task-core"),
		TEXT("src"),
		TEXT("task"),
		TEXT("compiler"),
		TEXT("task-compiler.ts"));

	FString EventDelegateSource;
	FString TaskCompilerSource;
	if (!LoadRequiredSmokeSource(*this, EventDelegateClusterPath, EventDelegateSource)
		|| !LoadRequiredSmokeSource(*this, TaskCompilerPath, TaskCompilerSource))
	{
		return false;
	}

	bool bClean = true;
	bClean &= TestTrue(TEXT("EventDelegate owns component-bound event use site"), EventDelegateSource.Contains(TEXT("ComponentBoundEvent")));
	bClean &= TestTrue(TEXT("EventDelegate owns delegate use site"), EventDelegateSource.Contains(TEXT("Delegate")));
	bClean &= TestFalse(TEXT("EventDelegate does not declare custom events"), EventDelegateSource.Contains(TEXT("ensure_custom_event")));
	bClean &= TestFalse(TEXT("EventDelegate does not declare override events"), EventDelegateSource.Contains(TEXT("ensure_override_event")));
	bClean &= TestFalse(TEXT("EventDelegate does not own native_event lifecycle"), EventDelegateSource.Contains(TEXT("native_event")));

	bClean &= TestTrue(TEXT("Task compiler keeps custom event signature step"), TaskCompilerSource.Contains(TEXT("blueprint_signature"))
		&& TaskCompilerSource.Contains(TEXT("ensure_custom_event")));
	bClean &= TestTrue(TEXT("Task compiler keeps dependent graph_write body step"), TaskCompilerSource.Contains(TEXT("depends_on: ['step_001']")));
	return bClean;
}

#endif
