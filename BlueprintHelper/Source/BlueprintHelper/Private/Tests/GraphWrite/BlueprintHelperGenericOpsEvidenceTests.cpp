#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericOpsEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace BlueprintHelperGenericOpsEvidenceTests
{
static FString SourcePath(const FString& RelativePath)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlueprintHelper"));
	return Plugin.IsValid()
		? FPaths::Combine(Plugin->GetBaseDir(), RelativePath)
		: FString();
}

static bool LoadSourceFile(FAutomationTestBase& Test, const FString& RelativePath, FString& OutSource)
{
	const FString Path = SourcePath(RelativePath);
	if (Path.IsEmpty() || !FFileHelper::LoadFileToString(OutSource, *Path))
	{
		Test.AddError(FString::Printf(TEXT("Unable to load source file for GenericOps guard: %s"), *RelativePath));
		return false;
	}
	return true;
}

static FBlueprintHelperActionResolutionRequest MakeGenericRequest()
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Control;
	Request.Semantic.Query = TEXT("switch_enum");
	return Request;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsCoreDtoGuardTest,
	"BlueprintHelper.GraphWrite.GenericOps.Evidence.CoreDtoGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsCoreDtoGuardTest::RunTest(const FString& Parameters)
{
	FString ActionContextSource;
	FString SemanticSource;
	if (!BlueprintHelperGenericOpsEvidenceTests::LoadSourceFile(
			*this,
			TEXT("Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"),
			ActionContextSource)
		|| !BlueprintHelperGenericOpsEvidenceTests::LoadSourceFile(
			*this,
			TEXT("Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"),
			SemanticSource))
	{
		return false;
	}

	const TArray<FString> ForbiddenTokens = {
		TEXT("SwitchCasePins"),
		TEXT("MacroGraphPath"),
		TEXT("MacroPinShapeSnapshot"),
		TEXT("ExposeOnSpawnProperties"),
		TEXT("AsyncProxyDelegateHandlers"),
		TEXT("SelectOptionProofs"),
		TEXT("SetFieldsInStructFields")
	};

	bool bPassed = true;
	for (const FString& Token : ForbiddenTokens)
	{
		if (ActionContextSource.Contains(Token) || SemanticSource.Contains(Token))
		{
			AddError(FString::Printf(
				TEXT("GenericOps evidence must stay in ContextEvidence/focused readers, not core DTO field '%s'."),
				*Token));
			bPassed = false;
		}
	}
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsEvidenceReaderTest,
	"BlueprintHelper.GraphWrite.GenericOps.Evidence.Reader",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsEvidenceReaderTest::RunTest(const FString& Parameters)
{
	bool bPassed = true;

	FBlueprintHelperActionResolutionRequest ControlRequest = BlueprintHelperGenericOpsEvidenceTests::MakeGenericRequest();
	ControlRequest.ContextEvidence.Add(TEXT("generic.control.operation"), TEXT("switch_enum"));
	ControlRequest.ContextEvidence.Add(TEXT("generic.control.case_values"), TEXT("Idle,Running"));
	ControlRequest.ContextEvidence.Add(TEXT("generic.control.default_policy"), TEXT("has_default"));

	FBlueprintHelperGenericOpsControlOperationEvidence ControlEvidence;
	FString ErrorCode;
	FString Message;
	bPassed &= TestTrue(
		TEXT("control evidence reads operation and case values"),
		FBlueprintHelperControlOperationEvidenceReader::Read(ControlRequest, ControlEvidence, ErrorCode, Message));
	bPassed &= TestEqual(TEXT("control operation"), ControlEvidence.Operation, FString(TEXT("switch_enum")));
	bPassed &= TestEqual(TEXT("control case count"), ControlEvidence.CaseValues.Num(), 2);

	FBlueprintHelperActionResolutionRequest MacroRequest = BlueprintHelperGenericOpsEvidenceTests::MakeGenericRequest();
	MacroRequest.ContextEvidence.Add(TEXT("generic.control.operation"), TEXT("for_loop"));
	MacroRequest.ContextEvidence.Add(TEXT("generic.macro.graph_path"), TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros:ForLoop"));
	FBlueprintHelperGenericOpsMacroInstanceEvidence MacroEvidence;
	bPassed &= TestFalse(
		TEXT("macro missing pin shape snapshot rejected"),
		FBlueprintHelperMacroInstanceEvidenceReader::Read(
			MacroRequest,
			MacroEvidence,
			ErrorCode,
			Message));
	bPassed &= TestEqual(TEXT("macro missing snapshot code"), ErrorCode, FString(TEXT("missing_evidence.generic.macro.pin_shape_snapshot")));

	FBlueprintHelperActionResolutionRequest CreateRequest = BlueprintHelperGenericOpsEvidenceTests::MakeGenericRequest();
	CreateRequest.Semantic.Kind = EBlueprintHelperActionSemanticKind::Create;
	CreateRequest.Semantic.CreateOperation = TEXT("spawn_actor");
	CreateRequest.ContextEvidence.Add(TEXT("generic.create.class_path"), TEXT("/Script/Engine.Actor"));
	FBlueprintHelperGenericOpsCreateEvidence CreateEvidence;
	bPassed &= TestTrue(
		TEXT("create evidence reads semantic operation and class path"),
		FBlueprintHelperGenericCreateEvidenceReader::Read(CreateRequest, CreateEvidence, ErrorCode, Message));
	bPassed &= TestEqual(TEXT("create operation"), CreateEvidence.Operation, FString(TEXT("spawn_actor")));
	bPassed &= TestEqual(TEXT("create class path"), CreateEvidence.ClassPath, FString(TEXT("/Script/Engine.Actor")));

	FBlueprintHelperActionResolutionRequest TransformRequest = BlueprintHelperGenericOpsEvidenceTests::MakeGenericRequest();
	TransformRequest.Semantic.Kind = EBlueprintHelperActionSemanticKind::Convert;
	TransformRequest.ContextEvidence.Add(TEXT("generic.transform.operation"), TEXT("dynamic_cast"));
	TransformRequest.ContextEvidence.Add(TEXT("generic.transform.source_pin_type"), TEXT("object|/Script/Engine.Actor"));
	TransformRequest.ContextEvidence.Add(TEXT("generic.transform.target_pin_type"), TEXT("object|/Script/Engine.Pawn"));
	FBlueprintHelperGenericOpsTransformEvidence TransformEvidence;
	bPassed &= TestTrue(
		TEXT("transform evidence reads pin type proof"),
		FBlueprintHelperGenericTransformEvidenceReader::Read(TransformRequest, TransformEvidence, ErrorCode, Message));
	bPassed &= TestEqual(TEXT("transform operation"), TransformEvidence.Operation, FString(TEXT("dynamic_cast")));

	FBlueprintHelperActionResolutionRequest ScheduleRequest = BlueprintHelperGenericOpsEvidenceTests::MakeGenericRequest();
	ScheduleRequest.Semantic.Kind = EBlueprintHelperActionSemanticKind::Schedule;
	ScheduleRequest.ContextEvidence.Add(TEXT("generic.schedule.operation"), TEXT("latent_or_async_node"));
	ScheduleRequest.ContextEvidence.Add(TEXT("generic.schedule.graph_latent_allowed"), TEXT("false"));
	FBlueprintHelperGenericOpsScheduleEvidence ScheduleEvidence;
	bPassed &= TestFalse(
		TEXT("latent schedule rejects graph without latent permission"),
		FBlueprintHelperGenericScheduleEvidenceReader::Read(ScheduleRequest, ScheduleEvidence, ErrorCode, Message));
	bPassed &= TestEqual(TEXT("latent not allowed code"), ErrorCode, FString(TEXT("latent_not_allowed")));

	FBlueprintHelperActionResolutionRequest StructRequest = BlueprintHelperGenericOpsEvidenceTests::MakeGenericRequest();
	StructRequest.Semantic.Kind = EBlueprintHelperActionSemanticKind::Select;
	StructRequest.ContextEvidence.Add(TEXT("generic.struct.operation"), TEXT("set_fields_in_struct"));
	StructRequest.ContextEvidence.Add(TEXT("generic.struct.struct_path"), TEXT("/Script/CoreUObject.Vector"));
	StructRequest.ContextEvidence.Add(TEXT("generic.struct.selected_field_paths"), TEXT("X,Y"));
	FBlueprintHelperGenericOpsStructFieldPolicyEvidence StructEvidence;
	bPassed &= TestTrue(
		TEXT("struct field policy reads selected fields"),
		FBlueprintHelperStructFieldPolicyEvidenceReader::Read(StructRequest, StructEvidence, ErrorCode, Message));
	bPassed &= TestEqual(TEXT("selected field count"), StructEvidence.SelectedFieldPaths.Num(), 2);

	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericOpsActionContextProjectionTest,
	"BlueprintHelper.GraphWrite.GenericOps.Evidence.ActionContextProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericOpsActionContextProjectionTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBlueprintHelperGraphStatementIR> Statement = MakeShared<FBlueprintHelperGraphStatementIR>();
	Statement->StatementId = TEXT("stmt_switch");
	Statement->Path = TEXT("$.statements[0]");
	Statement->Kind = EBlueprintHelperGraphStatementKind::Branch;
	Statement->ContextEvidence.Add(TEXT("generic.control.operation"), TEXT("switch_enum"));
	Statement->ContextEvidence.Add(TEXT("generic.control.case_values"), TEXT("Idle,Running"));

	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>> Statements;
	Statements.Add(Statement);
	const TArray<FBlueprintHelperActionContextDemand> Demands =
		FBlueprintHelperActionContextDemandCollector::CollectFromStatements(Statements);
	TestEqual(TEXT("one control demand"), Demands.Num(), 1);
	if (Demands.Num() != 1)
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestEqual(
		TEXT("generic operation default projected"),
		Demands[0].DefaultValues.FindRef(TEXT("generic.control.operation")),
		FString(TEXT("switch_enum")));
	bPassed &= TestEqual(
		TEXT("generic case values default projected"),
		Demands[0].DefaultValues.FindRef(TEXT("generic.control.case_values")),
		FString(TEXT("Idle,Running")));

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Graph.GraphName = TEXT("EventGraph");
	const FBlueprintHelperResolvedActionContext Context =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, Demands[0]);
	bPassed &= TestEqual(
		TEXT("context generic operation evidence"),
		Context.Evidence.FindRef(TEXT("generic.control.operation")),
		FString(TEXT("switch_enum")));
	bPassed &= TestEqual(
		TEXT("context generic case values evidence"),
		Context.Evidence.FindRef(TEXT("generic.control.case_values")),
		FString(TEXT("Idle,Running")));
	return bPassed;
}

#endif
