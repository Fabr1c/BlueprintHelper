#if WITH_DEV_AUTOMATION_TESTS

#include "Entry/Bridge/BlueprintHelperBridgeRouter.h"
#include "Entry/Bridge/BlueprintHelperRequestValidator.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformMisc.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Shared/Services/BlueprintHelperAgentImportService.h"
#include "Systems/Debug/BlueprintHelperAssetBrowseService.h"
#include "Shared/Services/BlueprintHelperBlueprintStructureService.h"
#include "Systems/Debug/BlueprintHelperCompileService.h"
#include "Systems/Debug/BlueprintHelperContextService.h"
#include "Systems/ToolClusters/DataTable/BlueprintHelperDataTableService.h"
#include "Systems/Debug/BlueprintHelperEditorCommandService.h"
#include "Systems/Debug/BlueprintHelperRuntimeProfileService.h"
#include "Systems/Debug/BlueprintHelperDiagnosticsService.h"
#include "Systems/Authorization/BlueprintHelperWriteAuthorizationService.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicMdReadService.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonReadService.h"
#include "Systems/ToolClusters/AssetFactory/BlueprintHelperAssetFactoryService.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperCleanupBlueprintHelperBlockService.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperRollbackCleanupTransactionService.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperConvertBlockToUserOwnedService.h"
#include "Systems/Debug/BlueprintHelperCompileAssetService.h"
#include "Systems/Transactions/BlueprintHelperTransactionQueryService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/ToolClusters/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/Transactions/BlueprintHelperTransactionJournalService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphSnapshotService.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"
#include "Shared/Services/BlueprintHelperExportService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Shared/Services/BlueprintHelperImportService.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "Systems/Debug/BlueprintHelperValidationService.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

class FBlueprintHelperSafetyTestsLocalUtils
{
public:
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

static FString MakeSafetyObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UPackage* MakeSafetyPackage(const FString& Prefix)
{
	UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Game/BlueprintHelperSafety/%s"), *MakeSafetyObjectName(Prefix)));
	Package->SetDirtyFlag(false);
	return Package;
}

static bool BuildOrderedHalfWriteFields(TMap<FString, FString>& OutFields)
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

static UDataTable* MakeVectorDataTable(UPackage* Package, const FName TableName, const FName RowName, const FVector& InitialValue)
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

static FWidgetMoveFixture MakeWidgetMoveFixture()
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

static bool AnchorsEqual(const FAnchors& A, const FAnchors& B)
{
	return FMath::IsNearlyEqual(A.Minimum.X, B.Minimum.X)
		&& FMath::IsNearlyEqual(A.Minimum.Y, B.Minimum.Y)
		&& FMath::IsNearlyEqual(A.Maximum.X, B.Maximum.X)
		&& FMath::IsNearlyEqual(A.Maximum.Y, B.Maximum.Y);
}

static UBlueprint* MakeSafetyActorBlueprint(const FString& Prefix)
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

static FBPVariableDescription* FindSafetyMemberVariable(UBlueprint* Blueprint, const FString& VariableName)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	const FName VariableFName(*VariableName);
	for (FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		if (Variable.VarName == VariableFName)
		{
			return &Variable;
		}
	}
	return nullptr;
}

static UEdGraph* GetSafetyEventGraph(UBlueprint* Blueprint)
{
	return Blueprint ? FBlueprintGraphWriteFacade::FindGraphByName(Blueprint, TEXT("EventGraph")) : nullptr;
}

static int32 GetSafetyEventGraphNodeCount(UBlueprint* Blueprint)
{
	UEdGraph* EventGraph = GetSafetyEventGraph(Blueprint);
	return EventGraph ? EventGraph->Nodes.Num() : INDEX_NONE;
}

static FBlueprintHelperImportResult RunStrictImport(UBlueprint* Blueprint, const FString& JsonText)
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

static bool HasDiagnosticCode(const FBlueprintHelperDiagnosticSet& Diagnostics, const FString& Code)
{
	return Diagnostics.Items.ContainsByPredicate([&Code](const FBlueprintHelperDiagnosticItem& Item)
	{
		return Item.Code == Code;
	});
}


};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperRuntimeProfileGraphWriteMergeAvailableTest,
	"BlueprintHelper.RuntimeDiagnostics.RuntimeProfile.GraphWriteMergeAvailable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperRuntimeProfileGraphWriteMergeAvailableTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperRuntimeProfileService RuntimeProfileService;
	const FBlueprintHelperRuntimeProfileData Profile = RuntimeProfileService.GetRuntimeProfile();

	bool bReportsMergeNotImplemented = false;
	for (const FBlueprintHelperUnavailableCapability& Item : Profile.ToolCapabilities.Unavailable)
	{
		if (Item.Cluster == TEXT("graph_write") &&
			Item.Capability == TEXT("merge") &&
			Item.Status == EBlueprintHelperCapabilityStatus::Unavailable &&
			Item.Reason == EBlueprintHelperCapabilityUnavailableReason::NotImplemented)
		{
			bReportsMergeNotImplemented = true;
			break;
		}
	}

	TestFalse(TEXT("runtime profile no longer reports graph_write.merge as not_implemented"), bReportsMergeNotImplemented);
	return true;
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
	UBlueprint* Blueprint = FBlueprintHelperSafetyTestsLocalUtils::MakeSafetyActorBlueprint(TEXT("BridgeExportEffectiveScope"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	TestNotNull(TEXT("test EventGraph exists"), FBlueprintHelperSafetyTestsLocalUtils::GetSafetyEventGraph(Blueprint));

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
	FBlueprintHelperDebugCaseStoreService DebugCaseStoreService;
	FBlueprintHelperDebugEntryService DebugEntryService(DebugCaseStoreService);
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
		GraphResolver, BlockIdService, OwnershipService, JournalService);
	FBlueprintHelperGraphSnapshotService SnapshotService;
	FBlueprintHelperReplaceBlueprintGraphService ReplaceGraphService(
		GraphResolver, BlockIdService, OwnershipService, JournalService, SnapshotService);
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
	FBlueprintHelperReviewStoreService ReviewStoreService;
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
		DebugEntryService,
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
		VariableService,
		ReviewStoreService);

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
	FBlueprintHelperBlueprintVariableSetMemberDefaultTest,
	"BlueprintHelper.Safety.BlueprintVariable.SetMemberDefaultWritesBlueprintDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperBlueprintVariableSetMemberDefaultTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSafetyTestsLocalUtils::MakeSafetyActorBlueprint(TEXT("VariableDefault"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);

	FBlueprintHelperGraphResolver GraphResolver;
	FBlueprintHelperBlueprintStructureService StructureService(GraphResolver);
	FBlueprintHelperBlueprintVariableService VariableService(GraphResolver, StructureService);

	TSharedRef<FJsonObject> PinType = MakeShared<FJsonObject>();
	PinType->SetStringField(TEXT("category"), TEXT("int"));

	TSharedRef<FJsonObject> AddPayload = MakeShared<FJsonObject>();
	AddPayload->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
	AddPayload->SetStringField(TEXT("name"), TEXT("Health"));
	AddPayload->SetObjectField(TEXT("pin_type"), PinType);

	const FBlueprintHelperToolResultBase AddResult = VariableService.AddMemberVariable(AddPayload);
	TestTrue(TEXT("member variable is added"), AddResult.bOk);
	TestNotNull(TEXT("member variable exists after add"), FBlueprintHelperSafetyTestsLocalUtils::FindSafetyMemberVariable(Blueprint, TEXT("Health")));

	TSharedRef<FJsonObject> SetPayload = MakeShared<FJsonObject>();
	SetPayload->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
	SetPayload->SetStringField(TEXT("name"), TEXT("Health"));
	SetPayload->SetField(TEXT("value"), MakeShared<FJsonValueNumber>(100.0));

	const FBlueprintHelperToolResultBase SetResult = VariableService.SetMemberDefault(SetPayload);
	TestTrue(TEXT("set member default succeeds"), SetResult.bOk);
	TestEqual(TEXT("set member default applies change"), SetResult.Status, EBlueprintHelperToolStatus::Applied);
	TestTrue(TEXT("set member default marks modified"), SetResult.bModified);
	TestTrue(TEXT("set member default has validation summary"), SetResult.Validation.IsSet());
	TestTrue(TEXT("set member default requests compile"), SetResult.Validation.IsSet() && SetResult.Validation->bShouldCompile);
	TestTrue(TEXT("set member default requests save"), SetResult.Validation.IsSet() && SetResult.Validation->bShouldSave);

	const FBPVariableDescription* HealthVariable = FBlueprintHelperSafetyTestsLocalUtils::FindSafetyMemberVariable(Blueprint, TEXT("Health"));
	TestNotNull(TEXT("Health variable still exists"), HealthVariable);
	if (HealthVariable)
	{
		TestEqual(TEXT("Health default value is written"), HealthVariable->DefaultValue, FString(TEXT("100")));
	}

	FString DataSchema;
	TestTrue(TEXT("set member default returns data schema"),
		SetResult.Data.IsValid() && SetResult.Data->TryGetStringField(TEXT("schema"), DataSchema));
	TestEqual(TEXT("set member default data schema"), DataSchema, FString(TEXT("SetMemberDefault.v1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBlueprintVariableSetMemberVariablePropertiesTest,
	"BlueprintHelper.Safety.BlueprintVariable.SetMemberVariablePropertiesWritesMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperBlueprintVariableSetMemberVariablePropertiesTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSafetyTestsLocalUtils::MakeSafetyActorBlueprint(TEXT("VariableProperties"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);

	FBlueprintHelperGraphResolver GraphResolver;
	FBlueprintHelperBlueprintStructureService StructureService(GraphResolver);
	FBlueprintHelperBlueprintVariableService VariableService(GraphResolver, StructureService);

	TSharedRef<FJsonObject> PinType = MakeShared<FJsonObject>();
	PinType->SetStringField(TEXT("category"), TEXT("int"));

	TSharedRef<FJsonObject> AddPayload = MakeShared<FJsonObject>();
	AddPayload->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
	AddPayload->SetStringField(TEXT("name"), TEXT("Health"));
	AddPayload->SetObjectField(TEXT("pin_type"), PinType);
	TestTrue(TEXT("member variable is added"), VariableService.AddMemberVariable(AddPayload).bOk);

	auto MakeSetting = [](const FString& PropertyPath, const TSharedPtr<FJsonValue>& Value)
	{
		TSharedRef<FJsonObject> Setting = MakeShared<FJsonObject>();
		Setting->SetStringField(TEXT("property_path"), PropertyPath);
		Setting->SetField(TEXT("value"), Value);
		return MakeShared<FJsonValueObject>(Setting);
	};

	TArray<TSharedPtr<FJsonValue>> Settings;
	Settings.Add(MakeSetting(TEXT("category"), MakeShared<FJsonValueString>(TEXT("BHStats"))));
	Settings.Add(MakeSetting(TEXT("tooltip"), MakeShared<FJsonValueString>(TEXT("Current health."))));
	Settings.Add(MakeSetting(TEXT("instance_editable"), MakeShared<FJsonValueBoolean>(true)));
	Settings.Add(MakeSetting(TEXT("expose_on_spawn"), MakeShared<FJsonValueBoolean>(true)));

	TSharedRef<FJsonObject> SetPayload = MakeShared<FJsonObject>();
	SetPayload->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
	SetPayload->SetStringField(TEXT("name"), TEXT("Health"));
	SetPayload->SetArrayField(TEXT("settings"), Settings);

	const FBlueprintHelperToolResultBase SetResult = VariableService.SetMemberVariableProperties(SetPayload);
	TestTrue(TEXT("set member variable properties succeeds"), SetResult.bOk);
	TestEqual(TEXT("set member variable properties applies change"), SetResult.Status, EBlueprintHelperToolStatus::Applied);
	TestTrue(TEXT("set member variable properties requests compile"), SetResult.Validation.IsSet() && SetResult.Validation->bShouldCompile);
	TestTrue(TEXT("set member variable properties requests save"), SetResult.Validation.IsSet() && SetResult.Validation->bShouldSave);

	const FName HealthName(TEXT("Health"));
	TestEqual(TEXT("Health category is written"),
		FBlueprintEditorUtils::GetBlueprintVariableCategory(Blueprint, HealthName, nullptr).ToString(),
		FString(TEXT("BHStats")));

	FString Tooltip;
	TestTrue(TEXT("Health tooltip metadata exists"),
		FBlueprintEditorUtils::GetBlueprintVariableMetaData(Blueprint, HealthName, nullptr, FBlueprintMetadata::MD_Tooltip, Tooltip));
	TestEqual(TEXT("Health tooltip is written"), Tooltip, FString(TEXT("Current health.")));

	FString ExposeOnSpawn;
	TestTrue(TEXT("Health expose_on_spawn metadata exists"),
		FBlueprintEditorUtils::GetBlueprintVariableMetaData(Blueprint, HealthName, nullptr, FBlueprintMetadata::MD_ExposeOnSpawn, ExposeOnSpawn));
	TestEqual(TEXT("Health expose_on_spawn metadata is true"), ExposeOnSpawn, FString(TEXT("true")));

	const FBPVariableDescription* HealthVariable = FBlueprintHelperSafetyTestsLocalUtils::FindSafetyMemberVariable(Blueprint, TEXT("Health"));
	TestNotNull(TEXT("Health variable still exists"), HealthVariable);
	if (HealthVariable)
	{
		TestFalse(TEXT("Health instance editable flag is enabled"),
			(HealthVariable->PropertyFlags & CPF_DisableEditOnInstance) != 0);
	}

	FString DataSchema;
	TestTrue(TEXT("set member variable properties returns data schema"),
		SetResult.Data.IsValid() && SetResult.Data->TryGetStringField(TEXT("schema"), DataSchema));
	TestEqual(TEXT("set member variable properties data schema"), DataSchema, FString(TEXT("SetMemberVariableProperties.v1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBlueprintVariableSetMemberDefaultsBatchTest,
	"BlueprintHelper.Safety.BlueprintVariable.SetMemberDefaultsWritesBlueprintDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperBlueprintVariableSetMemberDefaultsBatchTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSafetyTestsLocalUtils::MakeSafetyActorBlueprint(TEXT("VariableDefaultsBatch"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);

	FBlueprintHelperGraphResolver GraphResolver;
	FBlueprintHelperBlueprintStructureService StructureService(GraphResolver);
	FBlueprintHelperBlueprintVariableService VariableService(GraphResolver, StructureService);

	auto AddMember = [&VariableService, Blueprint](const FString& Name, const FString& Category)
	{
		TSharedRef<FJsonObject> PinType = MakeShared<FJsonObject>();
		PinType->SetStringField(TEXT("category"), Category);

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
		Payload->SetStringField(TEXT("name"), Name);
		Payload->SetObjectField(TEXT("pin_type"), PinType);
		return VariableService.AddMemberVariable(Payload);
	};

	TestTrue(TEXT("Mana variable is added"), AddMember(TEXT("Mana"), TEXT("int")).bOk);
	TestTrue(TEXT("Enabled variable is added"), AddMember(TEXT("bEnabled"), TEXT("bool")).bOk);

	TSharedRef<FJsonObject> ManaDefault = MakeShared<FJsonObject>();
	ManaDefault->SetStringField(TEXT("name"), TEXT("Mana"));
	ManaDefault->SetField(TEXT("value"), MakeShared<FJsonValueNumber>(50.0));

	TSharedRef<FJsonObject> EnabledDefault = MakeShared<FJsonObject>();
	EnabledDefault->SetStringField(TEXT("variable_name"), TEXT("bEnabled"));
	EnabledDefault->SetField(TEXT("default_value"), MakeShared<FJsonValueBoolean>(true));

	TArray<TSharedPtr<FJsonValue>> Defaults;
	Defaults.Add(MakeShared<FJsonValueObject>(ManaDefault));
	Defaults.Add(MakeShared<FJsonValueObject>(EnabledDefault));

	TSharedRef<FJsonObject> BatchPayload = MakeShared<FJsonObject>();
	BatchPayload->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
	BatchPayload->SetArrayField(TEXT("defaults"), Defaults);

	const FBlueprintHelperToolResultBase BatchResult = VariableService.SetMemberDefaults(BatchPayload);
	TestTrue(TEXT("set member defaults succeeds"), BatchResult.bOk);
	TestEqual(TEXT("set member defaults applies change"), BatchResult.Status, EBlueprintHelperToolStatus::Applied);
	TestTrue(TEXT("set member defaults has validation summary"), BatchResult.Validation.IsSet());
	TestTrue(TEXT("set member defaults requests compile"), BatchResult.Validation.IsSet() && BatchResult.Validation->bShouldCompile);
	TestTrue(TEXT("set member defaults requests save"), BatchResult.Validation.IsSet() && BatchResult.Validation->bShouldSave);

	const FBPVariableDescription* ManaVariable = FBlueprintHelperSafetyTestsLocalUtils::FindSafetyMemberVariable(Blueprint, TEXT("Mana"));
	const FBPVariableDescription* EnabledVariable = FBlueprintHelperSafetyTestsLocalUtils::FindSafetyMemberVariable(Blueprint, TEXT("bEnabled"));
	TestNotNull(TEXT("Mana variable still exists"), ManaVariable);
	TestNotNull(TEXT("Enabled variable still exists"), EnabledVariable);
	if (ManaVariable)
	{
		TestEqual(TEXT("Mana default value is written"), ManaVariable->DefaultValue, FString(TEXT("50")));
	}
	if (EnabledVariable)
	{
		TestEqual(TEXT("Enabled default value is written"), EnabledVariable->DefaultValue, FString(TEXT("true")));
	}

	FString DataSchema;
	TestTrue(TEXT("set member defaults returns data schema"),
		BatchResult.Data.IsValid() && BatchResult.Data->TryGetStringField(TEXT("schema"), DataSchema));
	TestEqual(TEXT("set member defaults data schema"), DataSchema, FString(TEXT("SetMemberDefaults.v1")));
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
	FBlueprintHelperRequestValidatorRequiresWriteSessionTest,
	"BlueprintHelper.Safety.RequestValidator.RequiresWriteSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperRequestValidatorRequiresWriteSessionTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperWriteAuthorizationService& AuthService = FBlueprintHelperWriteAuthorizationService::Get();
	AuthService.ResetForTesting();

	FBlueprintHelperBridgeRequest WriteRequest;
	WriteRequest.Command = TEXT("import_json");
	WriteRequest.Payload = MakeShared<FJsonObject>();
	WriteRequest.Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/Tests/BP_Door.BP_Door"));

	FBlueprintHelperBridgeValidationError Error;
	TestFalse(TEXT("write command without an active write session is rejected"),
		FBlueprintHelperRequestValidator::ValidateAuthorization(WriteRequest, Error));
	TestEqual(TEXT("write rejection uses unauthorized code"), Error.Code, FString(TEXT("unauthorized")));
	TestEqual(TEXT("write rejection identifies auth_session"), Error.Field, FString(TEXT("auth_session")));

	FBlueprintHelperWriteSessionRequest SessionRequest;
	SessionRequest.Reason = TEXT("automation test");
	SessionRequest.Scope = TEXT("project");
	SessionRequest.TtlSeconds = 60;
	const FBlueprintHelperWriteSessionGrant Grant = AuthService.CreateApprovedSessionForTesting(SessionRequest);

	Error = FBlueprintHelperBridgeValidationError();
	TestTrue(TEXT("write command without auth_session is accepted under active Editor write session"),
		FBlueprintHelperRequestValidator::ValidateAuthorization(WriteRequest, Error));

	WriteRequest.AuthSession = Grant.SessionId;
	Error = FBlueprintHelperBridgeValidationError();
	TestTrue(TEXT("write command with approved auth_session is accepted"),
		FBlueprintHelperRequestValidator::ValidateAuthorization(WriteRequest, Error));

	AuthService.ResetForTesting();

	FBlueprintHelperWriteSessionRequest AssetListSessionRequest;
	AssetListSessionRequest.Reason = TEXT("automation scoped session test");
	AssetListSessionRequest.Scope = TEXT("asset_list");
	AssetListSessionRequest.TtlSeconds = 60;
	AssetListSessionRequest.AssetPaths.Add(TEXT("/Game/Tests/BP_Door.BP_Door"));
	AuthService.CreateApprovedSessionForTesting(AssetListSessionRequest);

	FBlueprintHelperBridgeRequest ScopedTaskRequest;
	ScopedTaskRequest.Command = TEXT("execute_task_plan");
	ScopedTaskRequest.Payload = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> TargetAssets;
	TargetAssets.Add(MakeShared<FJsonValueString>(TEXT("/Game/Tests/BP_Door.BP_Door")));
	TaskPlan->SetArrayField(TEXT("target_assets"), TargetAssets);
	ScopedTaskRequest.Payload->SetObjectField(TEXT("task_plan"), TaskPlan);

	Error = FBlueprintHelperBridgeValidationError();
	TestTrue(TEXT("asset_list Editor write session covers TaskPlan target_assets without auth_session"),
		FBlueprintHelperRequestValidator::ValidateAuthorization(ScopedTaskRequest, Error));

	ScopedTaskRequest.AuthSession = TEXT("stale-session-from-another-client");
	Error = FBlueprintHelperBridgeValidationError();
	TestTrue(TEXT("active Editor write session is not blocked by stale cached auth_session"),
		FBlueprintHelperRequestValidator::ValidateAuthorization(ScopedTaskRequest, Error));

	ScopedTaskRequest.AuthSession = FString();
	TargetAssets.Reset();
	TargetAssets.Add(MakeShared<FJsonValueString>(TEXT("/Game/Tests/BP_Other.BP_Other")));
	TaskPlan->SetArrayField(TEXT("target_assets"), TargetAssets);

	Error = FBlueprintHelperBridgeValidationError();
	TestFalse(TEXT("asset_list Editor write session rejects uncovered TaskPlan target_assets"),
		FBlueprintHelperRequestValidator::ValidateAuthorization(ScopedTaskRequest, Error));
	TestEqual(TEXT("uncovered asset rejection uses unauthorized code"), Error.Code, FString(TEXT("unauthorized")));
	TestEqual(TEXT("uncovered asset rejection identifies auth_session"), Error.Field, FString(TEXT("auth_session")));

	FBlueprintHelperBridgeRequest ReadRequest;
	ReadRequest.Command = TEXT("validate_json");

	Error = FBlueprintHelperBridgeValidationError();
	TestTrue(TEXT("validate_json remains readable without auth_session"),
		FBlueprintHelperRequestValidator::ValidateAuthorization(ReadRequest, Error));

	AuthService.ResetForTesting();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperRequestValidatorHighRiskDefaultTest,
	"BlueprintHelper.Safety.RequestValidator.DisablesHighRiskByDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperRequestValidatorHighRiskDefaultTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperSafetyTestsLocalUtils::FBlueprintHelperScopedEnvVar HighRiskEnv(TEXT("BLUEPRINTHELPER_ENABLE_HIGH_RISK_COMMANDS"), TEXT(""));
	FBlueprintHelperWriteAuthorizationService& AuthService = FBlueprintHelperWriteAuthorizationService::Get();
	AuthService.ResetForTesting();
	FBlueprintHelperWriteSessionRequest SessionRequest;
	SessionRequest.Reason = TEXT("automation high risk test");
	const FBlueprintHelperWriteSessionGrant Grant = AuthService.CreateApprovedSessionForTesting(SessionRequest);

	FBlueprintHelperBridgeRequest ExecRequest;
	ExecRequest.Command = TEXT("exec_console_command");
	ExecRequest.AuthSession = Grant.SessionId;

	FBlueprintHelperBridgeValidationError Error;
	TestFalse(TEXT("exec_console_command is disabled by default"),
		FBlueprintHelperRequestValidator::ValidateAuthorization(ExecRequest, Error));
	TestEqual(TEXT("exec_console_command rejection uses command_disabled"),
		Error.Code, FString(TEXT("command_disabled")));

	FBlueprintHelperBridgeRequest CloseRequest;
	CloseRequest.Command = TEXT("close_editor");
	CloseRequest.AuthSession = Grant.SessionId;

	Error = FBlueprintHelperBridgeValidationError();
	TestFalse(TEXT("close_editor is disabled by default"),
		FBlueprintHelperRequestValidator::ValidateAuthorization(CloseRequest, Error));
	TestEqual(TEXT("close_editor rejection uses command_disabled"),
		Error.Code, FString(TEXT("command_disabled")));

	AuthService.ResetForTesting();
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

	UPackage* Package = FBlueprintHelperSafetyTestsLocalUtils::MakeSafetyPackage(TEXT("DataTableNoHalfWrite"));
	UDataTable* DataTable = FBlueprintHelperSafetyTestsLocalUtils::MakeVectorDataTable(Package, *FBlueprintHelperSafetyTestsLocalUtils::MakeSafetyObjectName(TEXT("DT_VectorRows")), RowName, InitialValue);
	TestNotNull(TEXT("test DataTable is created"), DataTable);

	TMap<FString, FString> Fields;
	TestTrue(TEXT("test fields exercise a valid write before the invalid field"), FBlueprintHelperSafetyTestsLocalUtils::BuildOrderedHalfWriteFields(Fields));

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
	FBlueprintHelperSafetyTestsLocalUtils::FWidgetMoveFixture Fixture = FBlueprintHelperSafetyTestsLocalUtils::MakeWidgetMoveFixture();
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
		TestTrue(TEXT("anchors are restored"), FBlueprintHelperSafetyTestsLocalUtils::AnchorsEqual(RestoredSlot->GetAnchors(), Fixture.OldAnchors));
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
	UPackage* ObjectPackage = FBlueprintHelperSafetyTestsLocalUtils::MakeSafetyPackage(TEXT("ObjectPropertyUnsafeFlags"));
	UTextBlock* TextBlock = NewObject<UTextBlock>(
		ObjectPackage,
		*FBlueprintHelperSafetyTestsLocalUtils::MakeSafetyObjectName(TEXT("ObjectTextBlock")),
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

	FBlueprintHelperSafetyTestsLocalUtils::FWidgetMoveFixture WidgetFixture = FBlueprintHelperSafetyTestsLocalUtils::MakeWidgetMoveFixture();
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
	UPackage* Package = FBlueprintHelperSafetyTestsLocalUtils::MakeSafetyPackage(TEXT("DeleteNodesRejectsNodeIndex"));
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*FBlueprintHelperSafetyTestsLocalUtils::MakeSafetyObjectName(TEXT("BP_DeleteNodes")),
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
	UBlueprint* Blueprint = FBlueprintHelperSafetyTestsLocalUtils::MakeSafetyActorBlueprint(TEXT("ImportStrictMissingLinkPin"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	TestNotNull(TEXT("test EventGraph exists"), FBlueprintHelperSafetyTestsLocalUtils::GetSafetyEventGraph(Blueprint));

	const int32 NodeCountBefore = FBlueprintHelperSafetyTestsLocalUtils::GetSafetyEventGraphNodeCount(Blueprint);
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

	const FBlueprintHelperImportResult Result = FBlueprintHelperSafetyTestsLocalUtils::RunStrictImport(Blueprint, Json);
	TestFalse(TEXT("strict import fails when a link pin is missing"), Result.bSuccess);
	TestTrue(TEXT("strict import reports rollback"), Result.bRolledBack);
	TestEqual(TEXT("strict import status is failed"), Result.Status, FString(TEXT("failed")));
	TestTrue(TEXT("missing link pin has structured diagnostic"),
		FBlueprintHelperSafetyTestsLocalUtils::HasDiagnosticCode(Result.Diagnostics, TEXT("link_pin_not_found")));
	TestTrue(TEXT("strict rollback diagnostic is returned"),
		FBlueprintHelperSafetyTestsLocalUtils::HasDiagnosticCode(Result.Diagnostics, TEXT("strict_import_rolled_back")));
	TestEqual(TEXT("failed strict import leaves graph node count unchanged"),
		FBlueprintHelperSafetyTestsLocalUtils::GetSafetyEventGraphNodeCount(Blueprint), NodeCountBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperImportStrictRollsBackOnMissingDefaultPinTest,
	"BlueprintHelper.Safety.ImportStrict.RollsBackOnMissingDefaultPin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperImportStrictRollsBackOnMissingDefaultPinTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSafetyTestsLocalUtils::MakeSafetyActorBlueprint(TEXT("ImportStrictMissingDefaultPin"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	TestNotNull(TEXT("test EventGraph exists"), FBlueprintHelperSafetyTestsLocalUtils::GetSafetyEventGraph(Blueprint));

	const int32 NodeCountBefore = FBlueprintHelperSafetyTestsLocalUtils::GetSafetyEventGraphNodeCount(Blueprint);
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

	const FBlueprintHelperImportResult Result = FBlueprintHelperSafetyTestsLocalUtils::RunStrictImport(Blueprint, Json);
	TestFalse(TEXT("strict import fails when a default value pin is missing"), Result.bSuccess);
	TestTrue(TEXT("strict import reports rollback"), Result.bRolledBack);
	TestEqual(TEXT("strict import status is failed"), Result.Status, FString(TEXT("failed")));
	TestTrue(TEXT("missing default pin has structured diagnostic"),
		FBlueprintHelperSafetyTestsLocalUtils::HasDiagnosticCode(Result.Diagnostics, TEXT("default_pin_not_found")));
	TestTrue(TEXT("strict rollback diagnostic is returned"),
		FBlueprintHelperSafetyTestsLocalUtils::HasDiagnosticCode(Result.Diagnostics, TEXT("strict_import_rolled_back")));
	TestEqual(TEXT("failed strict import leaves graph node count unchanged"),
		FBlueprintHelperSafetyTestsLocalUtils::GetSafetyEventGraphNodeCount(Blueprint), NodeCountBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperImportStrictRollsBackOnInvalidPinTypeTest,
	"BlueprintHelper.Safety.ImportStrict.RollsBackOnInvalidPinType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperImportStrictRollsBackOnInvalidPinTypeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperSafetyTestsLocalUtils::MakeSafetyActorBlueprint(TEXT("ImportStrictInvalidPinType"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	TestNotNull(TEXT("test EventGraph exists"), FBlueprintHelperSafetyTestsLocalUtils::GetSafetyEventGraph(Blueprint));

	const int32 NodeCountBefore = FBlueprintHelperSafetyTestsLocalUtils::GetSafetyEventGraphNodeCount(Blueprint);
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

	const FBlueprintHelperImportResult Result = FBlueprintHelperSafetyTestsLocalUtils::RunStrictImport(Blueprint, Json);
	TestFalse(TEXT("strict import fails when pin_type cannot be converted"), Result.bSuccess);
	TestTrue(TEXT("strict import reports rollback"), Result.bRolledBack);
	TestEqual(TEXT("strict import status is failed"), Result.Status, FString(TEXT("failed")));
	TestTrue(TEXT("invalid pin type has structured diagnostic"),
		FBlueprintHelperSafetyTestsLocalUtils::HasDiagnosticCode(Result.Diagnostics, TEXT("invalid_pin_type")));
	TestTrue(TEXT("strict rollback diagnostic is returned"),
		FBlueprintHelperSafetyTestsLocalUtils::HasDiagnosticCode(Result.Diagnostics, TEXT("strict_import_rolled_back")));
	TestEqual(TEXT("failed strict import leaves graph node count unchanged"),
		FBlueprintHelperSafetyTestsLocalUtils::GetSafetyEventGraphNodeCount(Blueprint), NodeCountBefore);
	return true;
}

#endif
