#if WITH_DEV_AUTOMATION_TESTS

#include "Bridge/BlueprintHelperBridgeRouter.h"
#include "Bridge/BlueprintHelperRequestValidator.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformMisc.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Services/BlueprintHelperAgentImportService.h"
#include "Services/BlueprintHelperAssetBrowseService.h"
#include "Services/BlueprintHelperBlueprintStructureService.h"
#include "Services/BlueprintHelperCompileService.h"
#include "Services/BlueprintHelperContextService.h"
#include "Services/BlueprintHelperDataTableService.h"
#include "Services/BlueprintHelperEditorCommandService.h"
#include "Services/BlueprintHelperRuntimeProfileService.h"
#include "Services/BlueprintHelperDiagnosticsService.h"
#include "Logic/BlueprintHelperLogicMdReadService.h"
#include "Logic/BlueprintHelperLogicJsonReadService.h"
#include "Services/BlueprintHelperAssetFactoryService.h"
#include "Services/BlueprintHelperComponentService.h"
#include "Services/BlueprintHelperClassSettingsService.h"
#include "Services/BlueprintHelperAppendBlueprintGraphService.h"
#include "Services/BlueprintHelperReplaceBlueprintGraphService.h"
#include "Services/BlueprintHelperPatchBlueprintGraphService.h"
#include "Services/BlueprintHelperMergeBlueprintGraphService.h"
#include "Services/BlueprintHelperCleanupBlueprintHelperBlockService.h"
#include "Services/BlueprintHelperRollbackCleanupTransactionService.h"
#include "Services/BlueprintHelperConvertBlockToUserOwnedService.h"
#include "Services/BlueprintHelperCompileAssetService.h"
#include "Transactions/BlueprintHelperTransactionQueryService.h"
#include "Services/BlueprintHelperBlueprintVariableService.h"
#include "GraphSupport/BlueprintHelperBlockIdService.h"
#include "GraphSupport/BlueprintHelperOwnershipService.h"
#include "Transactions/BlueprintHelperTransactionJournalService.h"
#include "GraphSupport/BlueprintHelperGraphSnapshotService.h"
#include "Logic/BlueprintHelperLogicJsonPathService.h"
#include "Services/BlueprintHelperExportService.h"
#include "GraphSupport/BlueprintHelperGraphResolver.h"
#include "Services/BlueprintHelperImportService.h"
#include "Services/BlueprintHelperPropertyReflectionService.h"
#include "Services/BlueprintHelperValidationService.h"
#include "Services/BlueprintHelperWidgetService.h"
#include "TextToBlueprintGenerator.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

namespace
{
class FBlueprintHelperScopedEnvVar
{
public:
	FBlueprintHelperScopedEnvVar(const TCHAR* InName, const TCHAR* InValue)
		: Name(InName)
		, PreviousValue(FPlatformMisc::GetEnvironmentVariable(InName))
	{
		FPlatformMisc::SetEnvironmentVar(*Name, InValue);
	}

	~FBlueprintHelperScopedEnvVar()
	{
		FPlatformMisc::SetEnvironmentVar(*Name, *PreviousValue);
	}

private:
	FString Name;
	FString PreviousValue;
};

FString MakeSafetyObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

UPackage* MakeSafetyPackage(const FString& Prefix)
{
	UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Game/BlueprintHelperSafety/%s"), *MakeSafetyObjectName(Prefix)));
	Package->SetDirtyFlag(false);
	return Package;
}

bool BuildOrderedHalfWriteFields(TMap<FString, FString>& OutFields)
{
	const TArray<FString> MissingFieldCandidates = {
		TEXT("MissingField"),
		TEXT("__MissingField"),
		TEXT("ZZZ_MissingField"),
		TEXT("A_MissingField"),
		TEXT("BlueprintHelperMissingField"),
		TEXT("NoSuchVectorProperty")
	};

	for (const FString& MissingField : MissingFieldCandidates)
	{
		TMap<FString, FString> Candidate;
		Candidate.Add(TEXT("X"), TEXT("42.0"));
		Candidate.Add(TEXT("Y"), TEXT("43.0"));
		Candidate.Add(TEXT("Z"), TEXT("44.0"));
		Candidate.Add(MissingField, TEXT("bad"));

		bool bSawValidFieldBeforeMissing = false;
		for (const TPair<FString, FString>& Pair : Candidate)
		{
			if (Pair.Key == MissingField)
			{
				if (bSawValidFieldBeforeMissing)
				{
					OutFields = MoveTemp(Candidate);
					return true;
				}
				break;
			}

			if (Pair.Key == TEXT("X") || Pair.Key == TEXT("Y") || Pair.Key == TEXT("Z"))
			{
				bSawValidFieldBeforeMissing = true;
			}
		}
	}

	return false;
}

UDataTable* MakeVectorDataTable(UPackage* Package, const FName TableName, const FName RowName, const FVector& InitialValue)
{
	UDataTable* DataTable = NewObject<UDataTable>(Package, TableName, RF_Public | RF_Standalone | RF_Transactional);
	TMap<FName, const uint8*> RawRows;
	RawRows.Add(RowName, reinterpret_cast<const uint8*>(&InitialValue));
	DataTable->CreateTableFromRawData(RawRows, TBaseStructure<FVector>::Get());
	Package->SetDirtyFlag(false);
	return DataTable;
}

struct FWidgetMoveFixture
{
	UPackage* Package = nullptr;
	UWidgetBlueprint* Blueprint = nullptr;
	UCanvasPanel* Root = nullptr;
	UTextBlock* Target = nullptr;
	UButton* FullButton = nullptr;
	FVector2D OldPosition = FVector2D::ZeroVector;
	FVector2D OldSize = FVector2D::ZeroVector;
	FAnchors OldAnchors;
	FVector2D OldAlignment = FVector2D::ZeroVector;
	int32 OldZOrder = 0;
	bool bOldAutoSize = false;
	int32 OldIndex = INDEX_NONE;
};

FWidgetMoveFixture MakeWidgetMoveFixture()
{
	FWidgetMoveFixture Fixture;
	Fixture.Package = MakeSafetyPackage(TEXT("WidgetMove"));
	Fixture.Blueprint = NewObject<UWidgetBlueprint>(
		Fixture.Package,
		*MakeSafetyObjectName(TEXT("WBP_WidgetMove")),
		RF_Public | RF_Standalone | RF_Transactional);
	Fixture.Blueprint->WidgetTree = NewObject<UWidgetTree>(Fixture.Blueprint, TEXT("WidgetTree"), RF_Transactional);

	Fixture.Root = Fixture.Blueprint->WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	Fixture.Blueprint->WidgetTree->RootWidget = Fixture.Root;

	UTextBlock* LeadingSibling = Fixture.Blueprint->WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("LeadingSibling"));
	Fixture.Root->AddChildToCanvas(LeadingSibling);

	Fixture.Target = Fixture.Blueprint->WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("TargetText"));
	UCanvasPanelSlot* TargetSlot = Fixture.Root->AddChildToCanvas(Fixture.Target);
	TargetSlot->SetPosition(FVector2D(123.0, 456.0));
	TargetSlot->SetSize(FVector2D(210.0, 45.0));
	TargetSlot->SetAnchors(FAnchors(0.25f, 0.5f, 0.25f, 0.5f));
	TargetSlot->SetAlignment(FVector2D(0.75, 0.25));
	TargetSlot->SetAutoSize(false);
	TargetSlot->SetZOrder(17);

	Fixture.FullButton = Fixture.Blueprint->WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("FullButton"));
	Fixture.Root->AddChildToCanvas(Fixture.FullButton);

	UTextBlock* ExistingButtonChild = Fixture.Blueprint->WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ExistingButtonChild"));
	Fixture.FullButton->AddChild(ExistingButtonChild);

	Fixture.OldPosition = TargetSlot->GetPosition();
	Fixture.OldSize = TargetSlot->GetSize();
	Fixture.OldAnchors = TargetSlot->GetAnchors();
	Fixture.OldAlignment = TargetSlot->GetAlignment();
	Fixture.bOldAutoSize = TargetSlot->GetAutoSize();
	Fixture.OldZOrder = TargetSlot->GetZOrder();
	Fixture.OldIndex = Fixture.Root->GetChildIndex(Fixture.Target);
	Fixture.Package->SetDirtyFlag(false);
	return Fixture;
}

bool AnchorsEqual(const FAnchors& A, const FAnchors& B)
{
	return FMath::IsNearlyEqual(A.Minimum.X, B.Minimum.X)
		&& FMath::IsNearlyEqual(A.Minimum.Y, B.Minimum.Y)
		&& FMath::IsNearlyEqual(A.Maximum.X, B.Maximum.X)
		&& FMath::IsNearlyEqual(A.Maximum.Y, B.Maximum.Y);
}

UBlueprint* MakeSafetyActorBlueprint(const FString& Prefix)
{
	UPackage* Package = MakeSafetyPackage(Prefix);
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeSafetyObjectName(TEXT("BP_ImportStrict")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperSafety"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

UEdGraph* GetSafetyEventGraph(UBlueprint* Blueprint)
{
	return Blueprint ? TextToBlueprintGenerator::FindGraphByName(Blueprint, TEXT("EventGraph")) : nullptr;
}

int32 GetSafetyEventGraphNodeCount(UBlueprint* Blueprint)
{
	UEdGraph* EventGraph = GetSafetyEventGraph(Blueprint);
	return EventGraph ? EventGraph->Nodes.Num() : INDEX_NONE;
}

FBlueprintHelperImportResult RunStrictImport(UBlueprint* Blueprint, const FString& JsonText)
{
	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperValidationService Validator;
	FBlueprintHelperImportService ImportService(Resolver, Validator);

	FBlueprintHelperImportRequest Request;
	Request.Target.BlueprintPath = Blueprint ? Blueprint->GetPathName() : TEXT("");
	Request.Target.GraphName = TEXT("EventGraph");
	Request.JsonText = JsonText;
	Request.bStrict = true;
	Request.bAllowPartial = false;
	return ImportService.Import(Request);
}

bool HasDiagnosticCode(const FBlueprintHelperDiagnosticSet& Diagnostics, const FString& Code)
{
	return Diagnostics.Items.ContainsByPredicate([&Code](const FBlueprintHelperDiagnosticItem& Item)
	{
		return Item.Code == Code;
	});
}

FBlueprintHelperAgentImportResult RunAgentImport(const FString& JsonText)
{
	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperCompileService CompileService(Resolver);
	FBlueprintHelperAssetBrowseService AssetBrowseService;
	FBlueprintHelperAgentImportService AgentImportService(Resolver, CompileService, AssetBrowseService);

	FBlueprintHelperAgentImportRequest Request;
	Request.JsonText = JsonText;
	return AgentImportService.Import(Request);
}

bool HasAgentDiagnosticCode(const FBlueprintHelperAgentImportResult& Result, const FString& Code)
{
	return Result.Diagnostics.ContainsByPredicate([&Code](const FBlueprintHelperAgentImportDiagnostic& Diagnostic)
	{
		return Diagnostic.Code == Code;
	});
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperRequestValidatorScopeTest,
	"BlueprintHelper.Safety.RequestValidator.NormalizesExportScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperRequestValidatorScopeTest::RunTest(const FString& Parameters)
{
	EBlueprintHelperExportScope Scope = EBlueprintHelperExportScope::SingleGraph;
	FString EffectiveScope;
	FString Error;

	TestTrue(TEXT("legacy full_graph is accepted"),
		FBlueprintHelperRequestValidator::NormalizeExportScope(TEXT("full_graph"), Scope, EffectiveScope, Error));
	TestEqual(TEXT("full_graph maps to graph"), EffectiveScope, FString(TEXT("graph")));
	TestEqual(TEXT("full_graph maps to SingleGraph"), static_cast<uint8>(Scope), static_cast<uint8>(EBlueprintHelperExportScope::SingleGraph));

	TestTrue(TEXT("legacy full_blueprint is accepted"),
		FBlueprintHelperRequestValidator::NormalizeExportScope(TEXT("full_blueprint"), Scope, EffectiveScope, Error));
	TestEqual(TEXT("full_blueprint maps to blueprint"), EffectiveScope, FString(TEXT("blueprint")));
	TestEqual(TEXT("full_blueprint maps to FullBlueprint"), static_cast<uint8>(Scope), static_cast<uint8>(EBlueprintHelperExportScope::FullBlueprint));

	TestFalse(TEXT("unknown scope is rejected"),
		FBlueprintHelperRequestValidator::NormalizeExportScope(TEXT("everything"), Scope, EffectiveScope, Error));
	TestFalse(TEXT("unknown scope returns an error"), Error.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBridgeExportEffectiveScopeTest,
	"BlueprintHelper.Safety.BridgeExport.ReturnsEffectiveScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperBridgeExportEffectiveScopeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeSafetyActorBlueprint(TEXT("BridgeExportEffectiveScope"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	TestNotNull(TEXT("test EventGraph exists"), GetSafetyEventGraph(Blueprint));

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperValidationService Validator;
	FBlueprintHelperImportService ImportService(Resolver, Validator);
	FBlueprintHelperCompileService CompileService(Resolver);
	FBlueprintHelperAssetBrowseService AssetBrowseService;
	FBlueprintHelperAgentImportService AgentImportService(Resolver, CompileService, AssetBrowseService);
	FBlueprintHelperExportService ExportService(Resolver);
	FBlueprintHelperContextService ContextService(Resolver);
	FBlueprintHelperBlueprintStructureService StructureService(Resolver);
	FBlueprintHelperWidgetService WidgetService;
	FBlueprintHelperPropertyReflectionService PropertyReflectionService;
	FBlueprintHelperDataTableService DataTableService;
	FBlueprintHelperEditorCommandService EditorCommandService;
	FBlueprintHelperRuntimeProfileService RuntimeProfileService;
	FBlueprintHelperDiagnosticsService DiagnosticsService;
	FBlueprintHelperLogicMdReadService LogicMdReadService;
	FBlueprintHelperLogicJsonReadService LogicJsonReadService;
	FBlueprintHelperAssetFactoryService AssetFactoryService;
	FBlueprintHelperGraphResolver GraphResolver;
	FBlueprintHelperComponentService ComponentService(GraphResolver);
	FBlueprintHelperClassSettingsService ClassSettingsService(GraphResolver);
	FBlueprintHelperBlockIdService BlockIdService;
	FBlueprintHelperOwnershipService OwnershipService;
	FBlueprintHelperTransactionJournalService JournalService;
	FBlueprintHelperAppendBlueprintGraphService AppendGraphService(
		GraphResolver, AgentImportService, BlockIdService, OwnershipService, JournalService);
	FBlueprintHelperGraphSnapshotService SnapshotService;
	FBlueprintHelperReplaceBlueprintGraphService ReplaceGraphService(
		GraphResolver, AgentImportService, BlockIdService, OwnershipService, JournalService, SnapshotService);
	FBlueprintHelperLogicJsonPathService LogicJsonPathService;
	FBlueprintHelperPatchBlueprintGraphService PatchGraphService(
		GraphResolver, LogicJsonPathService, JournalService);
	FBlueprintHelperMergeBlueprintGraphService MergeGraphService(
		GraphResolver, LogicJsonPathService, JournalService);
	FBlueprintHelperCleanupBlueprintHelperBlockService CleanupBlockService(
		GraphResolver, JournalService);
	FBlueprintHelperRollbackCleanupTransactionService RollbackCleanupService(
		GraphResolver, JournalService);
	FBlueprintHelperConvertBlockToUserOwnedService ConvertBlockService(
		GraphResolver, OwnershipService, JournalService);
	FBlueprintHelperCompileAssetService CompileAssetService(CompileService);
	FBlueprintHelperTransactionQueryService TransactionQueryService;
	FBlueprintHelperBlueprintVariableService VariableService(GraphResolver, StructureService);
	FBlueprintHelperBridgeRouter Router(
		ImportService,
		AgentImportService,
		ExportService,
		CompileService,
		Validator,
		ContextService,
		AssetBrowseService,
		StructureService,
		WidgetService,
		PropertyReflectionService,
		DataTableService,
		EditorCommandService,
		RuntimeProfileService,
		DiagnosticsService,
		LogicMdReadService,
		LogicJsonReadService,
		AssetFactoryService,
		ComponentService,
		ClassSettingsService,
		AppendGraphService,
		ReplaceGraphService,
		PatchGraphService,
		MergeGraphService,
		CleanupBlockService,
		RollbackCleanupService,
		ConvertBlockService,
		CompileAssetService,
		TransactionQueryService,
		VariableService);

	FBlueprintHelperBridgeRequest Request;
	Request.RequestId = TEXT("effective-scope-test");
	Request.Command = TEXT("export_to_json");
	Request.Payload = MakeShared<FJsonObject>();
	Request.Payload->SetStringField(TEXT("target_blueprint"), Blueprint->GetPathName());
	Request.Payload->SetStringField(TEXT("target_graph"), TEXT("EventGraph"));
	Request.Payload->SetStringField(TEXT("scope"), TEXT("full_graph"));

	const FBlueprintHelperBridgeResponse Response = Router.HandleRequest(Request);
	TestTrue(TEXT("export_to_json succeeds"), Response.bSuccess);
	TestNotNull(TEXT("export_to_json returns a result object"), Response.Result.Get());

	if (Response.Result.IsValid())
	{
		FString EffectiveScope;
		TestTrue(TEXT("response includes effective_scope"),
			Response.Result->TryGetStringField(TEXT("effective_scope"), EffectiveScope));
		TestEqual(TEXT("legacy full_graph returns effective graph scope"),
			EffectiveScope, FString(TEXT("graph")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperRequestValidatorPayloadTest,
	"BlueprintHelper.Safety.RequestValidator.RejectsNullTargetGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperRequestValidatorPayloadTest::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetField(TEXT("target_graph"), MakeShared<FJsonValueNull>());

	FBlueprintHelperBridgeValidationError Error;
	TestFalse(TEXT("null target_graph is rejected before command execution"),
		FBlueprintHelperRequestValidator::ValidatePayloadForCommand(TEXT("import_json"), Payload, Error));
	TestEqual(TEXT("error identifies the invalid field"), Error.Field, FString(TEXT("payload.target_graph")));
	TestEqual(TEXT("error identifies expected type"), Error.ExpectedType, FString(TEXT("string")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperRequestValidatorRequiresTokenForWriteTest,
	"BlueprintHelper.Safety.RequestValidator.RequiresTokenForWrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperRequestValidatorRequiresTokenForWriteTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperScopedEnvVar TokenEnv(TEXT("BLUEPRINTHELPER_BRIDGE_TOKEN"), TEXT("bridge-token"));

	FBlueprintHelperBridgeRequest WriteRequest;
	WriteRequest.Command = TEXT("import_json");

	FBlueprintHelperBridgeValidationError Error;
	TestFalse(TEXT("write command without auth_token is rejected"),
		FBlueprintHelperRequestValidator::ValidateAuthorization(WriteRequest, Error));
	TestEqual(TEXT("write rejection uses unauthorized code"), Error.Code, FString(TEXT("unauthorized")));
	TestEqual(TEXT("write rejection identifies auth_token"), Error.Field, FString(TEXT("auth_token")));

	FBlueprintHelperBridgeRequest ReadRequest;
	ReadRequest.Command = TEXT("validate_json");

	Error = FBlueprintHelperBridgeValidationError();
	TestTrue(TEXT("validate_json remains readable without auth_token"),
		FBlueprintHelperRequestValidator::ValidateAuthorization(ReadRequest, Error));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperRequestValidatorHighRiskDefaultTest,
	"BlueprintHelper.Safety.RequestValidator.DisablesHighRiskByDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperRequestValidatorHighRiskDefaultTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperScopedEnvVar TokenEnv(TEXT("BLUEPRINTHELPER_BRIDGE_TOKEN"), TEXT("bridge-token"));
	FBlueprintHelperScopedEnvVar HighRiskEnv(TEXT("BLUEPRINTHELPER_ENABLE_HIGH_RISK_COMMANDS"), TEXT(""));

	FBlueprintHelperBridgeRequest ExecRequest;
	ExecRequest.Command = TEXT("exec_console_command");
	ExecRequest.AuthToken = TEXT("bridge-token");

	FBlueprintHelperBridgeValidationError Error;
	TestFalse(TEXT("exec_console_command is disabled by default"),
		FBlueprintHelperRequestValidator::ValidateAuthorization(ExecRequest, Error));
	TestEqual(TEXT("exec_console_command rejection uses command_disabled"),
		Error.Code, FString(TEXT("command_disabled")));

	FBlueprintHelperBridgeRequest CloseRequest;
	CloseRequest.Command = TEXT("close_editor");
	CloseRequest.AuthToken = TEXT("bridge-token");

	Error = FBlueprintHelperBridgeValidationError();
	TestFalse(TEXT("close_editor is disabled by default"),
		FBlueprintHelperRequestValidator::ValidateAuthorization(CloseRequest, Error));
	TestEqual(TEXT("close_editor rejection uses command_disabled"),
		Error.Code, FString(TEXT("command_disabled")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperValidationGraphScopeTest,
	"BlueprintHelper.Safety.Validation.GraphScopedNodeIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperValidationGraphScopeTest::RunTest(const FString& Parameters)
{
	const FString DuplicateIdsInDifferentGraphs = TEXT(R"JSON(
	{
		"version": "1.0",
		"graphs": [
			{"name": "GraphA", "nodes": [{"id": "Node_1", "type": "event"}], "links": []},
			{"name": "GraphB", "nodes": [{"id": "Node_1", "type": "event"}], "links": []}
		]
	}
	)JSON");

	FBlueprintHelperValidationService Validator;
	FBlueprintHelperValidationResult DuplicateResult = Validator.Validate(DuplicateIdsInDifferentGraphs);
	TestTrue(TEXT("duplicate local node ids are allowed across different graphs"), DuplicateResult.bValid);

	const FString CrossGraphLink = TEXT(R"JSON(
	{
		"version": "1.0",
		"graphs": [
			{
				"name": "GraphA",
				"nodes": [{"id": "Node_1", "type": "event"}],
				"links": [{"from_id": "Node_1", "to_id": "Node_2", "to_graph": "GraphB"}]
			},
			{"name": "GraphB", "nodes": [{"id": "Node_2", "type": "event"}], "links": []}
		]
	}
	)JSON");

	FBlueprintHelperValidationResult CrossGraphResult = Validator.Validate(CrossGraphLink);
	TestFalse(TEXT("cross graph links are rejected"), CrossGraphResult.bValid);
	TestTrue(TEXT("cross graph diagnostic is returned"),
		CrossGraphResult.Diagnostics.Items.ContainsByPredicate([](const FBlueprintHelperDiagnosticItem& Item)
		{
			return Item.Code == TEXT("cross_graph_link_not_supported");
		}));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDataTableUpdateNoHalfWriteTest,
	"BlueprintHelper.Safety.DataTableUpdate.NoHalfWrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDataTableUpdateNoHalfWriteTest::RunTest(const FString& Parameters)
{
	const FName RowName(TEXT("RowA"));
	const FVector InitialValue(1.0, 2.0, 3.0);

	UPackage* Package = MakeSafetyPackage(TEXT("DataTableNoHalfWrite"));
	UDataTable* DataTable = MakeVectorDataTable(Package, *MakeSafetyObjectName(TEXT("DT_VectorRows")), RowName, InitialValue);
	TestNotNull(TEXT("test DataTable is created"), DataTable);

	TMap<FString, FString> Fields;
	TestTrue(TEXT("test fields exercise a valid write before the invalid field"), BuildOrderedHalfWriteFields(Fields));

	FBlueprintHelperDataTableService Service;
	FBlueprintHelperDataTableMutationResult Result = Service.UpdateDataTableRow(DataTable->GetPathName(), RowName.ToString(), Fields);
	TestFalse(TEXT("multi-field update with an invalid field is rejected"), Result.bSuccess);

	const FVector* RowAfter = reinterpret_cast<const FVector*>(DataTable->FindRowUnchecked(RowName));
	TestNotNull(TEXT("row still exists"), RowAfter);
	if (RowAfter)
	{
		TestEqual(TEXT("X remains unchanged"), RowAfter->X, InitialValue.X);
		TestEqual(TEXT("Y remains unchanged"), RowAfter->Y, InitialValue.Y);
		TestEqual(TEXT("Z remains unchanged"), RowAfter->Z, InitialValue.Z);
	}
	TestFalse(TEXT("failed update does not dirty the package"), Package->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetMoveRestoresOldSlotTest,
	"BlueprintHelper.Safety.WidgetMove.RestoresOldSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetMoveRestoresOldSlotTest::RunTest(const FString& Parameters)
{
	FWidgetMoveFixture Fixture = MakeWidgetMoveFixture();
	TestNotNull(TEXT("WidgetBlueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("target widget is created"), Fixture.Target);
	TestNotNull(TEXT("full button is created"), Fixture.FullButton);

	FBlueprintHelperWidgetService Service;
	FBlueprintHelperWidgetMutationResult Result = Service.MoveWidget(
		Fixture.Blueprint->GetPathName(),
		TEXT("TargetText"),
		TEXT("FullButton"),
		0);

	TestFalse(TEXT("move into a full content widget is rejected"), Result.bSuccess);
	TestEqual(TEXT("target is restored to the old child index"), Fixture.Root->GetChildAt(Fixture.OldIndex), Cast<UWidget>(Fixture.Target));

	UCanvasPanelSlot* RestoredSlot = Cast<UCanvasPanelSlot>(Fixture.Target->Slot);
	TestNotNull(TEXT("target slot class is restored"), RestoredSlot);
	if (RestoredSlot)
	{
		TestEqual(TEXT("position X is restored"), RestoredSlot->GetPosition().X, Fixture.OldPosition.X);
		TestEqual(TEXT("position Y is restored"), RestoredSlot->GetPosition().Y, Fixture.OldPosition.Y);
		TestEqual(TEXT("size X is restored"), RestoredSlot->GetSize().X, Fixture.OldSize.X);
		TestEqual(TEXT("size Y is restored"), RestoredSlot->GetSize().Y, Fixture.OldSize.Y);
		TestTrue(TEXT("anchors are restored"), AnchorsEqual(RestoredSlot->GetAnchors(), Fixture.OldAnchors));
		TestEqual(TEXT("alignment X is restored"), RestoredSlot->GetAlignment().X, Fixture.OldAlignment.X);
		TestEqual(TEXT("alignment Y is restored"), RestoredSlot->GetAlignment().Y, Fixture.OldAlignment.Y);
		TestEqual(TEXT("auto size is restored"), RestoredSlot->GetAutoSize(), Fixture.bOldAutoSize);
		TestEqual(TEXT("ZOrder is restored"), RestoredSlot->GetZOrder(), Fixture.OldZOrder);
	}
	TestFalse(TEXT("failed move does not dirty the package"), Fixture.Package->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperObjectPropertyRejectsUnsafeFlagsTest,
	"BlueprintHelper.Safety.ObjectProperty.RejectsUnsafeFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperObjectPropertyRejectsUnsafeFlagsTest::RunTest(const FString& Parameters)
{
	UPackage* ObjectPackage = MakeSafetyPackage(TEXT("ObjectPropertyUnsafeFlags"));
	UTextBlock* TextBlock = NewObject<UTextBlock>(
		ObjectPackage,
		*MakeSafetyObjectName(TEXT("ObjectTextBlock")),
		RF_Public | RF_Standalone | RF_Transactional);

	FProperty* SlotProperty = TextBlock->GetClass()->FindPropertyByName(TEXT("Slot"));
	TestNotNull(TEXT("UWidget Slot property exists"), SlotProperty);
	if (SlotProperty)
	{
		TestTrue(TEXT("Slot is editor-visible"), SlotProperty->HasAnyPropertyFlags(CPF_Edit));
		TestTrue(TEXT("Slot is BlueprintReadOnly"), SlotProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly));
	}
	ObjectPackage->SetDirtyFlag(false);

	FBlueprintHelperPropertyReflectionService ObjectService;
	FBlueprintHelperSetPropertyResult ObjectResult = ObjectService.SetObjectProperty(
		TextBlock->GetPathName(),
		TEXT("Slot"),
		TEXT("None"));
	TestFalse(TEXT("UObject unsafe property is rejected"), ObjectResult.bSuccess);
	TestFalse(TEXT("rejected UObject property write does not dirty the package"), ObjectPackage->IsDirty());

	FWidgetMoveFixture WidgetFixture = MakeWidgetMoveFixture();
	FBlueprintHelperWidgetService WidgetService;
	FBlueprintHelperWidgetMutationResult WidgetResult = WidgetService.SetWidgetProperty(
		WidgetFixture.Blueprint->GetPathName(),
		TEXT("TargetText"),
		TEXT("Slot"),
		TEXT("None"));
	TestFalse(TEXT("Widget unsafe property uses the same rejection policy"), WidgetResult.bSuccess);
	TestFalse(TEXT("rejected Widget property write does not dirty the package"), WidgetFixture.Package->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDeleteNodesRejectsNodeIndexTest,
	"BlueprintHelper.Safety.DeleteNodes.RejectsNodeIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDeleteNodesRejectsNodeIndexTest::RunTest(const FString& Parameters)
{
	UPackage* Package = MakeSafetyPackage(TEXT("DeleteNodesRejectsNodeIndex"));
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeSafetyObjectName(TEXT("BP_DeleteNodes")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperSafety"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	Package->SetDirtyFlag(false);

	FBlueprintHelperGraphTarget Target;
	Target.BlueprintPath = Blueprint->GetPathName();
	Target.GraphName = TEXT("EventGraph");

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperBlueprintStructureService Service(Resolver);

	int32 DeletedCount = -1;
	FString Error;
	const bool bDeleted = Service.DeleteNodes(Target, { TEXT("Node_0") }, DeletedCount, Error);

	TestFalse(TEXT("Node_i ids are rejected"), bDeleted);
	TestEqual(TEXT("no nodes are deleted"), DeletedCount, 0);
	TestTrue(TEXT("error asks for node_guid"), Error.Contains(TEXT("node_guid")));
	TestFalse(TEXT("rejected delete does not dirty the package"), Package->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperImportStrictRollsBackOnMissingLinkPinTest,
	"BlueprintHelper.Safety.ImportStrict.RollsBackOnMissingLinkPin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperImportStrictRollsBackOnMissingLinkPinTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeSafetyActorBlueprint(TEXT("ImportStrictMissingLinkPin"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	TestNotNull(TEXT("test EventGraph exists"), GetSafetyEventGraph(Blueprint));

	const int32 NodeCountBefore = GetSafetyEventGraphNodeCount(Blueprint);
	const FString Json = TEXT(R"JSON(
	{
		"version": "1.0",
		"nodes": [
			{"id": "BeginPlay", "type": "K2Node_Event", "event": {"event_name": "BeginPlay"}, "x": 0, "y": 0},
			{"id": "Branch", "type": "K2Node_IfThenElse", "x": 300, "y": 0}
		],
		"links": [
			{"from_id": "BeginPlay", "from_pin": "then", "to_id": "Branch", "to_pin": "missing_exec_pin"}
		]
	}
	)JSON");

	const FBlueprintHelperImportResult Result = RunStrictImport(Blueprint, Json);
	TestFalse(TEXT("strict import fails when a link pin is missing"), Result.bSuccess);
	TestTrue(TEXT("strict import reports rollback"), Result.bRolledBack);
	TestEqual(TEXT("strict import status is failed"), Result.Status, FString(TEXT("failed")));
	TestTrue(TEXT("missing link pin has structured diagnostic"),
		HasDiagnosticCode(Result.Diagnostics, TEXT("link_pin_not_found")));
	TestTrue(TEXT("strict rollback diagnostic is returned"),
		HasDiagnosticCode(Result.Diagnostics, TEXT("strict_import_rolled_back")));
	TestEqual(TEXT("failed strict import leaves graph node count unchanged"),
		GetSafetyEventGraphNodeCount(Blueprint), NodeCountBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperImportStrictRollsBackOnMissingDefaultPinTest,
	"BlueprintHelper.Safety.ImportStrict.RollsBackOnMissingDefaultPin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperImportStrictRollsBackOnMissingDefaultPinTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeSafetyActorBlueprint(TEXT("ImportStrictMissingDefaultPin"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	TestNotNull(TEXT("test EventGraph exists"), GetSafetyEventGraph(Blueprint));

	const int32 NodeCountBefore = GetSafetyEventGraphNodeCount(Blueprint);
	const FString Json = TEXT(R"JSON(
	{
		"version": "1.0",
		"nodes": [
			{
				"id": "Branch",
				"type": "K2Node_IfThenElse",
				"x": 0,
				"y": 0,
				"default_values": {"missing_condition_pin": "true"}
			}
		],
		"links": []
	}
	)JSON");

	const FBlueprintHelperImportResult Result = RunStrictImport(Blueprint, Json);
	TestFalse(TEXT("strict import fails when a default value pin is missing"), Result.bSuccess);
	TestTrue(TEXT("strict import reports rollback"), Result.bRolledBack);
	TestEqual(TEXT("strict import status is failed"), Result.Status, FString(TEXT("failed")));
	TestTrue(TEXT("missing default pin has structured diagnostic"),
		HasDiagnosticCode(Result.Diagnostics, TEXT("default_pin_not_found")));
	TestTrue(TEXT("strict rollback diagnostic is returned"),
		HasDiagnosticCode(Result.Diagnostics, TEXT("strict_import_rolled_back")));
	TestEqual(TEXT("failed strict import leaves graph node count unchanged"),
		GetSafetyEventGraphNodeCount(Blueprint), NodeCountBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperImportStrictRollsBackOnInvalidPinTypeTest,
	"BlueprintHelper.Safety.ImportStrict.RollsBackOnInvalidPinType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperImportStrictRollsBackOnInvalidPinTypeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeSafetyActorBlueprint(TEXT("ImportStrictInvalidPinType"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	TestNotNull(TEXT("test EventGraph exists"), GetSafetyEventGraph(Blueprint));

	const int32 NodeCountBefore = GetSafetyEventGraphNodeCount(Blueprint);
	const FString Json = TEXT(R"JSON(
	{
		"version": "1.0",
		"nodes": [
			{
				"id": "BadEvent",
				"type": "K2Node_CustomEvent",
				"event": {
					"event_name": "BadPinTypeEvent",
					"params": [
						{"name": "BadParam", "pin_type": {"category": "unsupported_pin_type"}}
					]
				},
				"x": 0,
				"y": 0
			}
		],
		"links": []
	}
	)JSON");

	const FBlueprintHelperImportResult Result = RunStrictImport(Blueprint, Json);
	TestFalse(TEXT("strict import fails when pin_type cannot be converted"), Result.bSuccess);
	TestTrue(TEXT("strict import reports rollback"), Result.bRolledBack);
	TestEqual(TEXT("strict import status is failed"), Result.Status, FString(TEXT("failed")));
	TestTrue(TEXT("invalid pin type has structured diagnostic"),
		HasDiagnosticCode(Result.Diagnostics, TEXT("invalid_pin_type")));
	TestTrue(TEXT("strict rollback diagnostic is returned"),
		HasDiagnosticCode(Result.Diagnostics, TEXT("strict_import_rolled_back")));
	TestEqual(TEXT("failed strict import leaves graph node count unchanged"),
		GetSafetyEventGraphNodeCount(Blueprint), NodeCountBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAgentImportGraphSimpleBeginPlayPrintStringStrictTest,
	"BlueprintHelper.Safety.AgentImportGraph.SimpleBeginPlayPrintStringStrict",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAgentImportGraphSimpleBeginPlayPrintStringStrictTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeSafetyActorBlueprint(TEXT("AgentImportSimple"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	TestNotNull(TEXT("test EventGraph exists"), GetSafetyEventGraph(Blueprint));

	const int32 NodeCountBefore = GetSafetyEventGraphNodeCount(Blueprint);
	const FString Json = FString::Printf(TEXT(R"JSON(
	{
		"schema": "BlueprintHelper.AgentImportGraph",
		"version": "1.0",
		"target_blueprint": "%s",
		"target_graph": "EventGraph",
		"mode": "append",
		"nodes": [
			{"id": "begin_play", "kind": "event", "event": "BeginPlay"},
			{"id": "print", "kind": "call", "function": "PrintString", "inputs": {"InString": "Hello from AgentImportGraph"}}
		],
		"links": [
			{"kind": "exec", "from": "begin_play.then", "to": "print.execute"}
		],
		"options": {"compile": false}
	}
	)JSON"), *Blueprint->GetPathName());

	const FBlueprintHelperAgentImportResult Result = RunAgentImport(Json);
	TestTrue(TEXT("simple AgentImportGraph succeeds without explicit strict option"), Result.bSuccess);
	TestEqual(TEXT("simple AgentImportGraph status is full_success"), Result.Status, FString(TEXT("full_success")));
	TestEqual(TEXT("simple AgentImportGraph creates two nodes"), Result.CreatedNodeCount, 2);
	TestEqual(TEXT("simple AgentImportGraph creates one link"), Result.CreatedLinkCount, 1);
	TestEqual(TEXT("simple AgentImportGraph has no warnings"), Result.WarningCount, 0);
	TestEqual(TEXT("simple AgentImportGraph has no errors"), Result.ErrorCount, 0);
	TestFalse(TEXT("simple AgentImportGraph does not roll back"), Result.bRolledBack);
	TestEqual(TEXT("simple AgentImportGraph returns zero rollbacks"), Result.RollbackCount, 0);
	TestEqual(TEXT("simple AgentImportGraph writes expected nodes"),
		GetSafetyEventGraphNodeCount(Blueprint), NodeCountBefore + 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAgentImportGraphRejectsGraphTypoTest,
	"BlueprintHelper.Safety.AgentImportGraph.RejectsGraphTypoWithoutWritingEventGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAgentImportGraphRejectsGraphTypoTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeSafetyActorBlueprint(TEXT("AgentImportGraphTypo"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	TestNotNull(TEXT("test EventGraph exists"), GetSafetyEventGraph(Blueprint));

	const int32 NodeCountBefore = GetSafetyEventGraphNodeCount(Blueprint);
	const FString Json = FString::Printf(TEXT(R"JSON(
	{
		"schema": "BlueprintHelper.AgentImportGraph",
		"version": "1.0",
		"target_blueprint": "%s",
		"target_graph": "EventGrph",
		"mode": "append",
		"nodes": [
			{"id": "begin_play", "kind": "event", "event": "BeginPlay"}
		],
		"links": [],
		"options": {"compile": false}
	}
	)JSON"), *Blueprint->GetPathName());

	const FBlueprintHelperAgentImportResult Result = RunAgentImport(Json);
	TestFalse(TEXT("graph typo import fails"), Result.bSuccess);
	TestEqual(TEXT("graph typo status is failed"), Result.Status, FString(TEXT("failed")));
	TestTrue(TEXT("graph typo returns graph_not_found"),
		HasAgentDiagnosticCode(Result, TEXT("graph_not_found")));
	TestTrue(TEXT("graph typo diagnostic includes available EventGraph"),
		Result.Message.Contains(TEXT("EventGraph")));
	TestEqual(TEXT("graph typo leaves EventGraph node count unchanged"),
		GetSafetyEventGraphNodeCount(Blueprint), NodeCountBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAgentImportGraphDefaultStrictRollsBackOnMissingLinkPinTest,
	"BlueprintHelper.Safety.AgentImportGraph.DefaultStrictRollsBackOnMissingLinkPin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAgentImportGraphDefaultStrictRollsBackOnMissingLinkPinTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeSafetyActorBlueprint(TEXT("AgentImportMissingLinkPin"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	TestNotNull(TEXT("test EventGraph exists"), GetSafetyEventGraph(Blueprint));

	const int32 NodeCountBefore = GetSafetyEventGraphNodeCount(Blueprint);
	const FString Json = FString::Printf(TEXT(R"JSON(
	{
		"schema": "BlueprintHelper.AgentImportGraph",
		"version": "1.0",
		"target_blueprint": "%s",
		"target_graph": "EventGraph",
		"mode": "append",
		"nodes": [
			{"id": "begin_play", "kind": "event", "event": "BeginPlay"},
			{"id": "print", "kind": "call", "function": "PrintString"}
		],
		"links": [
			{"kind": "exec", "from": "begin_play.then", "to": "print.missing_exec_pin"}
		],
		"options": {"compile": false}
	}
	)JSON"), *Blueprint->GetPathName());

	const FBlueprintHelperAgentImportResult Result = RunAgentImport(Json);
	TestFalse(TEXT("missing link pin fails under default strict"), Result.bSuccess);
	TestEqual(TEXT("missing link pin status is failed"), Result.Status, FString(TEXT("failed")));
	TestTrue(TEXT("missing link pin reports rollback"), Result.bRolledBack);
	TestEqual(TEXT("missing link pin returns one rollback"), Result.RollbackCount, 1);
	TestTrue(TEXT("missing link pin diagnostic is returned"),
		HasAgentDiagnosticCode(Result, TEXT("link_pin_not_found")));
	TestTrue(TEXT("strict rollback diagnostic is returned"),
		HasAgentDiagnosticCode(Result, TEXT("strict_import_rolled_back")));
	TestEqual(TEXT("missing link pin leaves graph node count unchanged"),
		GetSafetyEventGraphNodeCount(Blueprint), NodeCountBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAgentImportGraphDefaultStrictRollsBackOnMissingDefaultPinTest,
	"BlueprintHelper.Safety.AgentImportGraph.DefaultStrictRollsBackOnMissingDefaultPin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAgentImportGraphDefaultStrictRollsBackOnMissingDefaultPinTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeSafetyActorBlueprint(TEXT("AgentImportMissingDefaultPin"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	TestNotNull(TEXT("test EventGraph exists"), GetSafetyEventGraph(Blueprint));

	const int32 NodeCountBefore = GetSafetyEventGraphNodeCount(Blueprint);
	const FString Json = FString::Printf(TEXT(R"JSON(
	{
		"schema": "BlueprintHelper.AgentImportGraph",
		"version": "1.0",
		"target_blueprint": "%s",
		"target_graph": "EventGraph",
		"mode": "append",
		"nodes": [
			{"id": "print", "kind": "call", "function": "PrintString", "inputs": {"missing_default_pin": "bad"}}
		],
		"links": [],
		"options": {"compile": false}
	}
	)JSON"), *Blueprint->GetPathName());

	const FBlueprintHelperAgentImportResult Result = RunAgentImport(Json);
	TestFalse(TEXT("missing default pin fails under default strict"), Result.bSuccess);
	TestEqual(TEXT("missing default pin status is failed"), Result.Status, FString(TEXT("failed")));
	TestTrue(TEXT("missing default pin reports rollback"), Result.bRolledBack);
	TestEqual(TEXT("missing default pin returns one rollback"), Result.RollbackCount, 1);
	TestTrue(TEXT("missing default pin diagnostic is returned"),
		HasAgentDiagnosticCode(Result, TEXT("default_pin_not_found")));
	TestTrue(TEXT("strict rollback diagnostic is returned"),
		HasAgentDiagnosticCode(Result, TEXT("strict_import_rolled_back")));
	TestEqual(TEXT("missing default pin leaves graph node count unchanged"),
		GetSafetyEventGraphNodeCount(Blueprint), NodeCountBefore);
	return true;
}

#endif
