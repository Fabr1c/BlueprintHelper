#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Curves/CurveFloat.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/PrimaryAssetLabel.h"
#include "GameFramework/Actor.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "HAL/FileManager.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"
#include "Systems/Transactions/BlueprintHelperTransactionJournalService.h"
#include "UI/Review/BlueprintHelperReviewDebugText.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewAssetPresenters.h"
#include "UI/Review/BlueprintHelperReviewGraphBounds.h"
#include "UI/Review/BlueprintHelperReviewGraphResolver.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenter.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "UI/Review/SBlueprintHelperReviewPanel.h"
#include "Engine/Blueprint.h"
#include "UObject/MetaData.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

class FBlueprintHelperReviewStoreServiceTestsLocalUtils
{
public:
	static FBlueprintHelperReviewAtomicTarget MakeReviewTestTarget(
		const FString& TargetKey,
		const FString& VisualGroupKey,
		const FString& TransactionId,
		const FString& RecordedAfterHash = TEXT("after_hash"))
	{
		FBlueprintHelperReviewAtomicTarget Target;
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
		Target.AssetPath = TEXT("/Game/BP_Door");
		Target.GraphName = TEXT("EventGraph");
		Target.TargetKey = TargetKey;
		Target.TargetKind = TEXT("graph_node");
		Target.VisualGroupKey = VisualGroupKey;
		Target.DisplayLabel = TEXT("Door flow");
		Target.LatestTransactionId = TransactionId;
		Target.SourceTransactionIds.Add(TransactionId);
		Target.RecordedAfterHash = RecordedAfterHash;
		Target.BaselineHash = TEXT("baseline_hash");
		Target.RollbackDataRef = TEXT("review://rollback/door_flow");
		Target.Ownership = TEXT("blueprinthelper_owned");
		return Target;
	}

	static FBlueprintHelperWriteReviewEvidence MakeReviewTestEvidence(
		const FString& ArchiveSessionId,
		const FString& TaskRunId,
		const FString& TransactionId,
		const FString& AssetPath,
		const FBlueprintHelperReviewAtomicTarget& Target)
	{
		FBlueprintHelperWriteReviewEvidence Evidence;
		Evidence.ArchiveSessionId = ArchiveSessionId;
		Evidence.TaskRunId = TaskRunId;
		Evidence.TransactionId = TransactionId;
		Evidence.AssetPath = AssetPath;
		Evidence.OperationKind = TEXT("append_blueprint_graph");
		Evidence.DisplayLabel = TEXT("Door flow");
		Evidence.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
		Evidence.AtomicTargets.Add(Target);
		return Evidence;
	}

	static FString MakeUniqueReviewArchiveId(const FString& Prefix)
	{
		return Prefix + TEXT("_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}

	static FString GetReviewRecordPath(const FString& ReviewRecordId)
	{
		return FPaths::ProjectSavedDir()
			/ TEXT("BlueprintHelper")
			/ TEXT("Review")
			/ TEXT("Records")
			/ FString::Printf(TEXT("%s.json"), *ReviewRecordId);
	}

	static void DeleteReviewRecordFile(const FString& ReviewRecordId)
	{
		if (!ReviewRecordId.IsEmpty())
		{
			IFileManager::Get().Delete(*GetReviewRecordPath(ReviewRecordId), false, true);
		}
	}

	static UBlueprint* MakeReviewConversionTestBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperReview/%s_%s"),
			*Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		if (!Package)
		{
			return nullptr;
		}

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*FString::Printf(TEXT("BP_%s"), *Prefix),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperReviewStoreServiceTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UBlueprint* MakeReviewObjectBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperReview/%s_%s"),
			*Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		if (!Package)
		{
			return nullptr;
		}

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			UObject::StaticClass(),
			Package,
			*FString::Printf(TEXT("BP_%s"), *Prefix),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperReviewStoreServiceTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UBlueprint* MakeReviewNamedBlueprint(const FString& Prefix)
	{
		const FString AssetName = FString::Printf(
			TEXT("BP_%s_%s"),
			*Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperReview/%s"),
			*AssetName));
		if (!Package)
		{
			return nullptr;
		}

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*AssetName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperReviewStoreServiceTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UDataTable* MakeReviewDataTable(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperReview/%s_%s"),
			*Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		if (!Package)
		{
			return nullptr;
		}

		UDataTable* DataTable = NewObject<UDataTable>(
			Package,
			*FString::Printf(TEXT("DT_%s"), *Prefix),
			RF_Public | RF_Standalone | RF_Transactional);
		if (DataTable)
		{
			const FVector InitialValue(1.0, 2.0, 3.0);
			TMap<FName, const uint8*> RawRows;
			RawRows.Add(FName(TEXT("DamageSmall")), reinterpret_cast<const uint8*>(&InitialValue));
			DataTable->CreateTableFromRawData(RawRows, TBaseStructure<FVector>::Get());
			Package->SetDirtyFlag(false);
		}
		return DataTable;
	}

	static UObject* MakeReviewGenericObject(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperReview/%s_%s"),
			*Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		if (!Package)
		{
			return nullptr;
		}

		UObject* Object = NewObject<UCurveFloat>(
			Package,
			*FString::Printf(TEXT("Obj_%s"), *Prefix),
			RF_Public | RF_Standalone | RF_Transactional);
		Package->SetDirtyFlag(false);
		return Object;
	}

	static UDataAsset* MakeReviewDataAsset(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperReview/%s_%s"),
			*Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		if (!Package)
		{
			return nullptr;
		}

		UDataAsset* DataAsset = NewObject<UPrimaryAssetLabel>(
			Package,
			*FString::Printf(TEXT("DA_%s"), *Prefix),
			RF_Public | RF_Standalone | RF_Transactional);
		Package->SetDirtyFlag(false);
		return DataAsset;
	}

	static UWidgetBlueprint* MakeReviewWidgetBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperReview/%s_%s"),
			*Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		if (!Package)
		{
			return nullptr;
		}

		UWidgetBlueprint* WidgetBlueprint = NewObject<UWidgetBlueprint>(
			Package,
			*FString::Printf(TEXT("WBP_%s"), *Prefix),
			RF_Public | RF_Standalone | RF_Transactional);
		if (!WidgetBlueprint)
		{
			return nullptr;
		}

		WidgetBlueprint->ParentClass = UUserWidget::StaticClass();
		WidgetBlueprint->WidgetTree = NewObject<UWidgetTree>(
			WidgetBlueprint,
			TEXT("WidgetTree"),
			RF_Transactional);
		UCanvasPanel* Root = WidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("RootCanvas"));
		WidgetBlueprint->WidgetTree->RootWidget = Root;
		UTextBlock* Text = WidgetBlueprint->WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("SmokeText"));
		Root->AddChild(Text);
		Package->SetDirtyFlag(false);
		return WidgetBlueprint;
	}

	static UK2Node_CustomEvent* AddReviewConversionEventNode(UEdGraph* Graph, const FString& EventName)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UK2Node_CustomEvent* EventNode = NewObject<UK2Node_CustomEvent>(Graph);
		Graph->AddNode(EventNode, true, false);
		EventNode->CreateNewGuid();
		EventNode->CustomFunctionName = FName(*EventName);
		EventNode->PostPlacedNewNode();
		EventNode->AllocateDefaultPins();
		return EventNode;
	}

	static void MarkReviewNodeAsBlueprintHelperOwned(UEdGraphNode* Node, const FString& BlockId)
	{
		if (!Node)
		{
			return;
		}
		if (UPackage* Package = Node->GetOutermost())
		{
			FMetaData& MetaData = Package->GetMetaData();
			MetaData.SetValue(Node, TEXT("BlueprintHelperOwned"), TEXT("true"));
			MetaData.SetValue(Node, TEXT("BlueprintHelperBlockId"), *BlockId);
		}
	}

	static bool IsReviewNodeBlueprintHelperOwned(UEdGraphNode* Node)
	{
		if (!Node)
		{
			return false;
		}
		if (UPackage* Package = Node->GetOutermost())
		{
			FMetaData& MetaData = Package->GetMetaData();
			return MetaData.GetValue(Node, TEXT("BlueprintHelperOwned")) == FString(TEXT("true"))
				&& !MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId")).IsEmpty();
		}
		return false;
	}

	static bool ReviewGraphContainsNode(UEdGraph* Graph, const UEdGraphNode* Node)
	{
		return Graph && Node && Graph->Nodes.Contains(const_cast<UEdGraphNode*>(Node));
	}

};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewColorMappingTest,
	"BlueprintHelper.Review.VisibleChange.ColorMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewColorMappingTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("added changes render green"),
		FString(BlueprintHelperReviewChangeKindToColorName(EBlueprintHelperReviewChangeKind::Added)),
		FString(TEXT("green")));
	TestEqual(TEXT("removed changes render red"),
		FString(BlueprintHelperReviewChangeKindToColorName(EBlueprintHelperReviewChangeKind::Removed)),
		FString(TEXT("red")));
	TestEqual(TEXT("variable modifications render yellow"),
		FString(BlueprintHelperReviewChangeKindToColorName(EBlueprintHelperReviewChangeKind::VariableModified)),
		FString(TEXT("yellow")));
	TestEqual(TEXT("signature modifications render yellow"),
		FString(BlueprintHelperReviewChangeKindToColorName(EBlueprintHelperReviewChangeKind::SignatureModified)),
		FString(TEXT("yellow")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewSurfaceClassificationTest,
	"BlueprintHelper.Review.VisibleChange.SurfaceClassification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewSurfaceClassificationTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange ComponentChange;
	ComponentChange.LocationKey = TEXT("component:FakeDiffComponent");
	TestTrue(TEXT("component changes render in Components"),
		BlueprintHelperReviewShouldShowInComponents(ComponentChange));
	TestFalse(TEXT("component changes do not render as My Blueprint entries"),
		BlueprintHelperReviewShouldShowInMyBlueprint(ComponentChange));

	FBlueprintHelperReviewVisibleChange GraphChange;
	GraphChange.GraphName = TEXT("FakeDiffGraph");
	GraphChange.LocationKey = TEXT("graph:FakeDiffGraph/node:PrintString");
	TestTrue(TEXT("graph changes render in the graph page"),
		BlueprintHelperReviewShouldShowInGraph(GraphChange));
	TestFalse(TEXT("graph changes do not cover My Blueprint"),
		BlueprintHelperReviewShouldShowInMyBlueprint(GraphChange));

	FBlueprintHelperReviewAtomicTarget GraphTarget;
	GraphTarget.Surface = EBlueprintHelperReviewSurface::Graph;
	GraphTarget.TargetKey = TEXT("node:PrintString");
	GraphChange.AtomicTargets.Add(GraphTarget);
	TestFalse(TEXT("explicit graph atoms stay out of My Blueprint"),
		BlueprintHelperReviewShouldShowInMyBlueprint(GraphChange));

	FBlueprintHelperReviewVisibleChange EventGraphChange;
	EventGraphChange.GraphName = TEXT("EventGraph");
	EventGraphChange.LocationKey = TEXT("graph:EventGraph/node:BeginPlay");
	TestFalse(TEXT("EventGraph node changes do not match top-level event rows"),
		BlueprintHelperReviewShouldShowInMyBlueprint(EventGraphChange));

	FBlueprintHelperReviewVisibleChange SignatureChange;
	SignatureChange.LocationKey = TEXT("function:FakeDiffFunction:signature");
	SignatureChange.ChangeKind = EBlueprintHelperReviewChangeKind::SignatureModified;
	TestTrue(TEXT("signature changes render in My Blueprint"),
		BlueprintHelperReviewShouldShowInMyBlueprint(SignatureChange));
	TestTrue(TEXT("signature changes render in Details"),
		BlueprintHelperReviewShouldShowInDetails(SignatureChange));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewGraphRequiresExplicitGraphTargetTest,
	"BlueprintHelper.Review.VisibleChange.ReviewShouldShowInGraphRequiresGraphTargetWhenTargetsAreExplicit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewGraphRequiresExplicitGraphTargetTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange SignatureOnlyChange;
	SignatureOnlyChange.ChangeId = TEXT("tx_signature_only");
	SignatureOnlyChange.GraphName = TEXT("EventGraph");
	SignatureOnlyChange.LocationKey = TEXT("function:ApplyDamage:signature");
	SignatureOnlyChange.ChangeKind = EBlueprintHelperReviewChangeKind::SignatureModified;

	FBlueprintHelperReviewAtomicTarget SignatureTarget;
	SignatureTarget.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
	SignatureTarget.TargetKind = TEXT("signature");
	SignatureTarget.TargetKey = TEXT("signature:ApplyDamage");
	SignatureOnlyChange.AtomicTargets.Add(SignatureTarget);

	TestFalse(TEXT("explicit MyBlueprint signature target does not route to Graph fallback"),
		BlueprintHelperReviewShouldShowInGraph(SignatureOnlyChange));
	TestTrue(TEXT("explicit MyBlueprint signature target routes to MyBlueprint"),
		BlueprintHelperReviewShouldShowInMyBlueprint(SignatureOnlyChange));
	TestTrue(TEXT("explicit MyBlueprint signature target also routes to Details for signature inspection"),
		BlueprintHelperReviewShouldShowInDetails(SignatureOnlyChange));

	const FBlueprintHelperReviewSurfaceRouteDecision HiddenGraphDecision =
		FBlueprintHelperReviewSurfacePresenterRouter::RouteChangeToSurface(
			SignatureOnlyChange,
			EBlueprintHelperReviewSurface::Graph);
	TestFalse(TEXT("presenter hides Graph without Graph target"), HiddenGraphDecision.bShouldShow);
	TestEqual(TEXT("presenter records no_surface_anchor reason"),
		HiddenGraphDecision.Reason,
		FString(TEXT("no_surface_anchor")));

	FBlueprintHelperReviewVisibleChange TrueGraphChange = SignatureOnlyChange;
	TrueGraphChange.ChangeId = TEXT("tx_1778317276165");
	TrueGraphChange.AtomicTargets.Reset();
	FBlueprintHelperReviewAtomicTarget GraphTarget;
	GraphTarget.Surface = EBlueprintHelperReviewSurface::Graph;
	GraphTarget.GraphName = TEXT("EventGraph");
	GraphTarget.TargetKind = TEXT("graph_node");
	GraphTarget.TargetKey = TEXT("graph:EventGraph/node:PrintString");
	TrueGraphChange.AtomicTargets.Add(GraphTarget);

	TestTrue(TEXT("explicit Graph target remains routable"),
		BlueprintHelperReviewShouldShowInGraph(TrueGraphChange));
	const FBlueprintHelperReviewSurfaceRouteDecision ShownGraphDecision =
		FBlueprintHelperReviewSurfacePresenterRouter::RouteChangeToSurface(
			TrueGraphChange,
			EBlueprintHelperReviewSurface::Graph);
	TestTrue(TEXT("presenter shows true Graph target"), ShownGraphDecision.bShouldShow);
	TestEqual(TEXT("presenter records target_match reason"),
		ShownGraphDecision.Reason,
		FString(TEXT("target_match")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPresenterRoutingContractsTest,
	"BlueprintHelper.Review.VisibleChange.PresenterRoutingContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPresenterRoutingContractsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange GraphChange;
	GraphChange.ChangeId = TEXT("tx_graph_presenter");
	FBlueprintHelperReviewAtomicTarget GraphTarget;
	GraphTarget.Surface = EBlueprintHelperReviewSurface::Graph;
	GraphTarget.TargetKind = TEXT("graph_node");
	GraphTarget.TargetKey = TEXT("graph:EventGraph/node:PrintString");
	GraphChange.AtomicTargets.Add(GraphTarget);
	TestTrue(TEXT("Graph presenter accepts graph anchor"),
		FBlueprintHelperReviewGraphPresenter::ShouldShowChange(GraphChange));
	TestFalse(TEXT("Components presenter rejects graph anchor"),
		FBlueprintHelperReviewBlueprintComponentsPresenter::ShouldShowChange(GraphChange));

	FBlueprintHelperReviewVisibleChange ComponentChange;
	ComponentChange.ChangeId = TEXT("tx_component_presenter");
	FBlueprintHelperReviewAtomicTarget ComponentTarget;
	ComponentTarget.Surface = EBlueprintHelperReviewSurface::Components;
	ComponentTarget.TargetKind = TEXT("component");
	ComponentTarget.TargetKey = TEXT("component:SmokeSceneComp");
	ComponentChange.AtomicTargets.Add(ComponentTarget);
	TestTrue(TEXT("Components presenter accepts component anchor"),
		FBlueprintHelperReviewBlueprintComponentsPresenter::ShouldShowChange(ComponentChange));
	TestFalse(TEXT("Graph presenter rejects component anchor"),
		FBlueprintHelperReviewGraphPresenter::ShouldShowChange(ComponentChange));

	FBlueprintHelperReviewVisibleChange SignatureChange;
	SignatureChange.ChangeId = TEXT("tx_signature_presenter");
	FBlueprintHelperReviewAtomicTarget SignatureTarget;
	SignatureTarget.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
	SignatureTarget.TargetKind = TEXT("signature");
	SignatureTarget.TargetKey = TEXT("signature:ApplyDamage");
	SignatureChange.AtomicTargets.Add(SignatureTarget);
	TestTrue(TEXT("MyBlueprint presenter accepts signature anchor"),
		FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange(SignatureChange));
	TestTrue(TEXT("Details presenter accepts MyBlueprint signature anchor for signature inspection"),
		FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange(SignatureChange));

	FBlueprintHelperReviewVisibleChange DetailsChange;
	DetailsChange.ChangeId = TEXT("tx_details_presenter");
	FBlueprintHelperReviewAtomicTarget DetailsTarget;
	DetailsTarget.Surface = EBlueprintHelperReviewSurface::Details;
	DetailsTarget.TargetKind = TEXT("class_default_property");
	DetailsTarget.TargetKey = TEXT("class_default_property:SmokeHealth");
	DetailsChange.AtomicTargets.Add(DetailsTarget);
	TestTrue(TEXT("Details presenter accepts class default property anchor"),
		FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange(DetailsChange));
	TestFalse(TEXT("Graph presenter rejects object property anchor"),
		FBlueprintHelperReviewGraphPresenter::ShouldShowChange(DetailsChange));

	FBlueprintHelperReviewVisibleChange DataTableChange;
	DataTableChange.ChangeId = TEXT("tx_datatable_independent_presenter");
	FBlueprintHelperReviewAtomicTarget DataTableTarget;
	DataTableTarget.Surface = EBlueprintHelperReviewSurface::DataTable;
	DataTableTarget.TargetKind = TEXT("datatable_row");
	DataTableTarget.TargetKey = TEXT("datatable_row:DamageSmall");
	DataTableChange.AtomicTargets.Add(DataTableTarget);
	TestTrue(TEXT("DataTable presenter accepts DataTable surface"),
		FBlueprintHelperReviewDataTablePresenter::ShouldShowChange(DataTableChange));
	TestFalse(TEXT("Details presenter rejects DataTable surface"),
		FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange(DataTableChange));

	FBlueprintHelperReviewVisibleChange LegacyDataTableChange = DataTableChange;
	LegacyDataTableChange.ChangeId = TEXT("tx_datatable_legacy_details_presenter");
	LegacyDataTableChange.AtomicTargets[0].Surface = EBlueprintHelperReviewSurface::Details;
	TestFalse(TEXT("DataTable presenter no longer routes via Details target kind compatibility"),
		FBlueprintHelperReviewDataTablePresenter::ShouldShowChange(LegacyDataTableChange));

	FBlueprintHelperReviewVisibleChange MixedSurfaceChange;
	MixedSurfaceChange.ChangeId = TEXT("tx_mixed_surface_presenter");
	FBlueprintHelperReviewAtomicTarget MixedDataTableTarget;
	MixedDataTableTarget.Surface = EBlueprintHelperReviewSurface::DataTable;
	MixedDataTableTarget.TargetKind = TEXT("unknown_anchor");
	MixedDataTableTarget.TargetKey = TEXT("datatable_row:DamageSmall");
	MixedSurfaceChange.AtomicTargets.Add(MixedDataTableTarget);
	FBlueprintHelperReviewAtomicTarget MixedDataAssetTarget;
	MixedDataAssetTarget.Surface = EBlueprintHelperReviewSurface::DataAsset;
	MixedDataAssetTarget.TargetKind = TEXT("object_property");
	MixedDataAssetTarget.TargetKey = TEXT("object_property:SmokeHealth");
	MixedSurfaceChange.AtomicTargets.Add(MixedDataAssetTarget);
	TestFalse(TEXT("DataTable presenter only accepts target kind on the DataTable surface"),
		FBlueprintHelperReviewDataTablePresenter::ShouldShowChange(MixedSurfaceChange));
	TestTrue(TEXT("DataAsset presenter accepts its own matching surface target"),
		FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange(MixedSurfaceChange));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPresenterOverlayBuildsDeterministicReviewListTest,
	"BlueprintHelper.Review.VisibleChange.PresenterOverlayBuildsDeterministicReviewList",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPresenterOverlayBuildsDeterministicReviewListTest::RunTest(const FString& Parameters)
{
	struct FOverlayCase
	{
		EBlueprintHelperReviewSurface Surface;
		EBlueprintHelperReviewAssetKind AssetKind;
		FString DebugSurfaceName;
		FString SurfaceToken;
		FString ChangePrefix;
		FString TargetKind;
		TFunction<TSharedRef<SWidget>(const FBlueprintHelperReviewPanelSurfacePresenterArgs&)> BuildOverlay;
	};

	const TArray<FOverlayCase> Cases = {
		{
			EBlueprintHelperReviewSurface::DataTable,
			EBlueprintHelperReviewAssetKind::DataTable,
			TEXT("DataTable"),
			TEXT("data_table"),
			TEXT("datatable"),
			TEXT("datatable_row"),
			[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
			{
				return FBlueprintHelperReviewDataTablePresenter::BuildOverlay(Args);
			}
		},
		{
			EBlueprintHelperReviewSurface::DataAsset,
			EBlueprintHelperReviewAssetKind::DataAsset,
			TEXT("DataAsset"),
			TEXT("data_asset"),
			TEXT("dataasset"),
			TEXT("data_asset_property"),
			[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
			{
				return FBlueprintHelperReviewDataAssetPresenter::BuildOverlay(Args);
			}
		}
	};

	for (const FOverlayCase& OverlayCase : Cases)
	{
		TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Items;
		for (int32 Index = 0; Index < 2; ++Index)
		{
			FBlueprintHelperReviewVisibleChange Change;
			Change.ChangeId = FString::Printf(TEXT("tx_%s_overlay_%d"), *OverlayCase.ChangePrefix, Index);
			Change.AssetPath = TEXT("/Game/BlueprintHelper/Smoke/ReviewAsset");
			Change.DisplayLabel = FString::Printf(TEXT("%s row %d"), *OverlayCase.ChangePrefix, Index);
			Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;

			FBlueprintHelperReviewAtomicTarget Target;
			Target.Surface = OverlayCase.Surface;
			Target.TargetKind = OverlayCase.TargetKind;
			Target.TargetKey = FString::Printf(TEXT("%s:Target%d"), *OverlayCase.TargetKind, Index);
			Target.DisplayLabel = FString::Printf(TEXT("Target%d"), Index);
			Change.AtomicTargets.Add(Target);
			Items.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(Change));
		}

		TArray<FString> DebugMessages;
		FBlueprintHelperReviewAssetContext Context;
		Context.AssetKind = OverlayCase.AssetKind;
		FBlueprintHelperReviewPanelSurfacePresenterArgs Args;
		Args.AssetContext = &Context;
		Args.ChangeItems = &Items;
		Args.SelectedChange = Items[0];
		Args.AddDebugMessage = [&DebugMessages](const FString& Message)
		{
			DebugMessages.Add(Message);
		};
		Args.OnAcceptChange = [](TSharedPtr<FBlueprintHelperReviewVisibleChange>)
		{
			return FReply::Handled();
		};
		Args.OnRejectChange = [](TSharedPtr<FBlueprintHelperReviewVisibleChange>)
		{
			return FReply::Handled();
		};
		Args.GetChangeColor = [](EBlueprintHelperReviewChangeKind)
		{
			return FSlateColor(FLinearColor::Green);
		};
		Args.GetSelectedDiffColor = []()
		{
			return FSlateColor(FLinearColor::Yellow);
		};

		TSharedRef<SWidget> Overlay = OverlayCase.BuildOverlay(Args);
		TestTrue(FString::Printf(TEXT("%s overlay waits for stable row geometry"), *OverlayCase.DebugSurfaceName),
			&Overlay.Get() == &SNullWidget::NullWidget.Get());

		bool bSawRoute = false;
		bool bSawFallbackGeometry = false;
		bool bSawRow0 = false;
		bool bSawRow1 = false;
		for (const FString& Message : DebugMessages)
		{
			bSawRoute |= Message.Contains(FString::Printf(
				TEXT("ReviewRoute change=tx_%s_overlay_0 surface=%s"),
				*OverlayCase.ChangePrefix,
				*OverlayCase.DebugSurfaceName));
			bSawFallbackGeometry |= Message.Contains(TEXT("mode=fallback_geometry"));
			bSawRow0 |= Message.Contains(FString::Printf(
				TEXT("ReviewFrameGeometry change=tx_%s_overlay_0 surface=%s mode=slate_row result=pending reason=geometry_not_ready"),
				*OverlayCase.ChangePrefix,
				*OverlayCase.SurfaceToken));
			bSawRow1 |= Message.Contains(FString::Printf(
				TEXT("ReviewFrameGeometry change=tx_%s_overlay_1 surface=%s mode=slate_row result=pending reason=geometry_not_ready"),
				*OverlayCase.ChangePrefix,
				*OverlayCase.SurfaceToken));
		}
		TestTrue(FString::Printf(TEXT("%s overlay logs route debug"), *OverlayCase.DebugSurfaceName), bSawRoute);
		TestFalse(FString::Printf(TEXT("%s overlay does not log fake geometry"), *OverlayCase.DebugSurfaceName), bSawFallbackGeometry);
		TestTrue(FString::Printf(TEXT("%s overlay logs pending row 0"), *OverlayCase.DebugSurfaceName), bSawRow0);
		TestTrue(FString::Printf(TEXT("%s overlay logs pending row 1"), *OverlayCase.DebugSurfaceName), bSawRow1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPresenterOverlayHidesBuiltInPanelFallbackWithoutSlateRowGeometryTest,
	"BlueprintHelper.Review.VisibleChange.PresenterOverlayHidesBuiltInPanelFallbackWithoutSlateRowGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPresenterOverlayHidesBuiltInPanelFallbackWithoutSlateRowGeometryTest::RunTest(const FString& Parameters)
{
	struct FOverlayCase
	{
		EBlueprintHelperReviewSurface Surface;
		EBlueprintHelperReviewAssetKind AssetKind;
		FString DebugSurfaceName;
		FString SurfaceToken;
		FString ChangePrefix;
		FString TargetKind;
		TFunction<TSharedRef<SWidget>(const FBlueprintHelperReviewPanelSurfacePresenterArgs&)> BuildOverlay;
	};

	const TArray<FOverlayCase> Cases = {
		{
			EBlueprintHelperReviewSurface::Components,
			EBlueprintHelperReviewAssetKind::Blueprint,
			TEXT("Components"),
			TEXT("components"),
			TEXT("component"),
			TEXT("component"),
			[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
			{
				return FBlueprintHelperReviewBlueprintComponentsPresenter::BuildOverlay(Args);
			}
		},
		{
			EBlueprintHelperReviewSurface::MyBlueprint,
			EBlueprintHelperReviewAssetKind::Blueprint,
			TEXT("MyBlueprint"),
			TEXT("my_blueprint"),
			TEXT("myblueprint"),
			TEXT("signature"),
			[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
			{
				return FBlueprintHelperReviewMyBlueprintPresenter::BuildOverlay(Args);
			}
		},
		{
			EBlueprintHelperReviewSurface::Details,
			EBlueprintHelperReviewAssetKind::Blueprint,
			TEXT("Details"),
			TEXT("details"),
			TEXT("details"),
			TEXT("class_default_property"),
			[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
			{
				return FBlueprintHelperReviewObjectDetailsPresenter::BuildOverlay(Args);
			}
		},
		{
			EBlueprintHelperReviewSurface::UMGWidgetTree,
			EBlueprintHelperReviewAssetKind::WidgetBlueprint,
			TEXT("UMGWidgetTree"),
			TEXT("umg_widget_tree"),
			TEXT("umg"),
			TEXT("umg_widget"),
			[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
			{
				return FBlueprintHelperReviewUMGWidgetTreePresenter::BuildOverlay(Args);
			}
		}
	};

	for (const FOverlayCase& OverlayCase : Cases)
	{
		TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Items;
		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = FString::Printf(TEXT("tx_%s_no_geometry"), *OverlayCase.ChangePrefix);
		Change.AssetPath = TEXT("/Game/BlueprintHelper/Smoke/ReviewAsset");
		Change.DisplayLabel = FString::Printf(TEXT("%s row"), *OverlayCase.ChangePrefix);
		Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;

		FBlueprintHelperReviewAtomicTarget Target;
		Target.Surface = OverlayCase.Surface;
		Target.TargetKind = OverlayCase.TargetKind;
		Target.TargetKey = FString::Printf(TEXT("%s:Target"), *OverlayCase.TargetKind);
		Target.DisplayLabel = TEXT("Target");
		Change.AtomicTargets.Add(Target);
		Items.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(Change));

		TArray<FString> DebugMessages;
		FBlueprintHelperReviewAssetContext Context;
		Context.AssetKind = OverlayCase.AssetKind;
		FBlueprintHelperReviewPanelSurfacePresenterArgs Args;
		Args.AssetContext = &Context;
		Args.ChangeItems = &Items;
		Args.SelectedChange = Items[0];
		Args.AddDebugMessage = [&DebugMessages](const FString& Message)
		{
			DebugMessages.Add(Message);
		};
		Args.OnAcceptChange = [](TSharedPtr<FBlueprintHelperReviewVisibleChange>)
		{
			return FReply::Handled();
		};
		Args.OnRejectChange = [](TSharedPtr<FBlueprintHelperReviewVisibleChange>)
		{
			return FReply::Handled();
		};
		Args.GetChangeColor = [](EBlueprintHelperReviewChangeKind)
		{
			return FSlateColor(FLinearColor::Green);
		};
		Args.GetSelectedDiffColor = []()
		{
			return FSlateColor(FLinearColor::Yellow);
		};

		TSharedRef<SWidget> Overlay = OverlayCase.BuildOverlay(Args);
		TestTrue(FString::Printf(TEXT("%s overlay hides no-geometry fallback"), *OverlayCase.DebugSurfaceName),
			&Overlay.Get() == &SNullWidget::NullWidget.Get());

		bool bSawRoute = false;
		bool bSawHiddenGeometry = false;
		bool bSawShownReviewList = false;
		for (const FString& Message : DebugMessages)
		{
			bSawRoute |= Message.Contains(FString::Printf(
				TEXT("ReviewRoute change=tx_%s_no_geometry surface=%s"),
				*OverlayCase.ChangePrefix,
				*OverlayCase.DebugSurfaceName));
			bSawHiddenGeometry |= Message.Contains(FString::Printf(
				TEXT("ReviewFrameGeometry change=tx_%s_no_geometry surface=%s mode=slate_row result=pending reason=geometry_not_ready"),
				*OverlayCase.ChangePrefix,
				*OverlayCase.SurfaceToken));
			bSawShownReviewList |= Message.Contains(TEXT("mode=review_list result=shown"));
		}
		TestTrue(FString::Printf(TEXT("%s overlay logs route debug"), *OverlayCase.DebugSurfaceName), bSawRoute);
		TestTrue(FString::Printf(TEXT("%s overlay logs pending no-geometry state"), *OverlayCase.DebugSurfaceName), bSawHiddenGeometry);
		TestFalse(FString::Printf(TEXT("%s overlay does not show text review-list fallback"), *OverlayCase.DebugSurfaceName), bSawShownReviewList);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPresenterOverlayUsesStableSlateRowGeometryTest,
	"BlueprintHelper.Review.VisibleChange.PresenterOverlayUsesStableSlateRowGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPresenterOverlayUsesStableSlateRowGeometryTest::RunTest(const FString& Parameters)
{
	struct FOverlayCase
	{
		EBlueprintHelperReviewSurface Surface;
		EBlueprintHelperReviewAssetKind AssetKind;
		FString DebugSurfaceName;
		FString SurfaceToken;
		FString ChangePrefix;
		FString TargetKind;
		TFunction<TSharedRef<SWidget>(const FBlueprintHelperReviewPanelSurfacePresenterArgs&)> BuildOverlay;
	};

	const TArray<FOverlayCase> Cases = {
		{
			EBlueprintHelperReviewSurface::Components,
			EBlueprintHelperReviewAssetKind::Blueprint,
			TEXT("Components"),
			TEXT("components"),
			TEXT("component"),
			TEXT("component"),
			[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
			{
				return FBlueprintHelperReviewBlueprintComponentsPresenter::BuildOverlay(Args);
			}
		},
		{
			EBlueprintHelperReviewSurface::MyBlueprint,
			EBlueprintHelperReviewAssetKind::Blueprint,
			TEXT("MyBlueprint"),
			TEXT("my_blueprint"),
			TEXT("myblueprint"),
			TEXT("signature"),
			[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
			{
				return FBlueprintHelperReviewMyBlueprintPresenter::BuildOverlay(Args);
			}
		},
		{
			EBlueprintHelperReviewSurface::Details,
			EBlueprintHelperReviewAssetKind::Blueprint,
			TEXT("Details"),
			TEXT("details"),
			TEXT("details"),
			TEXT("class_default_property"),
			[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
			{
				return FBlueprintHelperReviewObjectDetailsPresenter::BuildOverlay(Args);
			}
		},
		{
			EBlueprintHelperReviewSurface::UMGWidgetTree,
			EBlueprintHelperReviewAssetKind::WidgetBlueprint,
			TEXT("UMGWidgetTree"),
			TEXT("umg_widget_tree"),
			TEXT("umg"),
			TEXT("umg_widget"),
			[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
			{
				return FBlueprintHelperReviewUMGWidgetTreePresenter::BuildOverlay(Args);
			}
		},
		{
			EBlueprintHelperReviewSurface::DataTable,
			EBlueprintHelperReviewAssetKind::DataTable,
			TEXT("DataTable"),
			TEXT("data_table"),
			TEXT("datatable"),
			TEXT("datatable_row"),
			[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
			{
				return FBlueprintHelperReviewDataTablePresenter::BuildOverlay(Args);
			}
		},
		{
			EBlueprintHelperReviewSurface::DataAsset,
			EBlueprintHelperReviewAssetKind::DataAsset,
			TEXT("DataAsset"),
			TEXT("data_asset"),
			TEXT("dataasset"),
			TEXT("data_asset_property"),
			[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
			{
				return FBlueprintHelperReviewDataAssetPresenter::BuildOverlay(Args);
			}
		}
	};

	for (const FOverlayCase& OverlayCase : Cases)
	{
		TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Items;
		for (int32 Index = 0; Index < 2; ++Index)
		{
			FBlueprintHelperReviewVisibleChange Change;
			Change.ChangeId = FString::Printf(TEXT("tx_%s_slate_row_%d"), *OverlayCase.ChangePrefix, Index);
			Change.AssetPath = TEXT("/Game/BlueprintHelper/Smoke/ReviewAsset");
			Change.DisplayLabel = FString::Printf(TEXT("%s row %d"), *OverlayCase.ChangePrefix, Index);
			Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;

			FBlueprintHelperReviewAtomicTarget Target;
			Target.Surface = OverlayCase.Surface;
			Target.TargetKind = OverlayCase.TargetKind;
			Target.TargetKey = FString::Printf(TEXT("%s:Target%d"), *OverlayCase.TargetKind, Index);
			Target.DisplayLabel = FString::Printf(TEXT("Target%d"), Index);
			Change.AtomicTargets.Add(Target);
			Items.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(Change));
		}

		TArray<FString> DebugMessages;
		FBlueprintHelperReviewAssetContext Context;
		Context.AssetKind = OverlayCase.AssetKind;
		FBlueprintHelperReviewPanelSurfacePresenterArgs Args;
		Args.AssetContext = &Context;
		Args.ChangeItems = &Items;
		Args.SelectedChange = Items[0];
		Args.AddDebugMessage = [&DebugMessages](const FString& Message)
		{
			DebugMessages.Add(Message);
		};
		Args.OnAcceptChange = [](TSharedPtr<FBlueprintHelperReviewVisibleChange>)
		{
			return FReply::Handled();
		};
		Args.OnRejectChange = [](TSharedPtr<FBlueprintHelperReviewVisibleChange>)
		{
			return FReply::Handled();
		};
		Args.GetChangeColor = [](EBlueprintHelperReviewChangeKind)
		{
			return FSlateColor(FLinearColor::Green);
		};
		Args.GetSelectedDiffColor = []()
		{
			return FSlateColor(FLinearColor::Yellow);
		};
		Args.ResolveRowGeometry.BindLambda([](
			const FBlueprintHelperReviewVisibleChange& Change,
			EBlueprintHelperReviewSurface,
			FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
		{
			const int32 RowIndex = Change.ChangeId.EndsWith(TEXT("_1")) ? 1 : 0;
			OutAnchor.bIsValid = true;
			OutAnchor.Position = FVector2D(12.0f, 24.0f + RowIndex * 44.0f);
			OutAnchor.Size = FVector2D(260.0f, 32.0f);
			OutAnchor.TargetText = Change.DisplayLabel;
			OutAnchor.Reason = TEXT("test_stable_slate_row_geometry");
			return true;
		});

		TSharedRef<SWidget> Overlay = OverlayCase.BuildOverlay(Args);
		TestTrue(FString::Printf(TEXT("%s stable geometry overlay builds a widget"), *OverlayCase.DebugSurfaceName),
			&Overlay.Get() != &SNullWidget::NullWidget.Get());

		bool bSawRow0 = false;
		bool bSawRow1 = false;
		bool bSawReviewList = false;
		for (const FString& Message : DebugMessages)
		{
			bSawReviewList |= Message.Contains(TEXT("mode=review_list"));
			bSawRow0 |= Message.Contains(FString::Printf(
				TEXT("ReviewFrameGeometry change=tx_%s_slate_row_0 surface=%s mode=slate_row result=shown"),
				*OverlayCase.ChangePrefix,
				*OverlayCase.SurfaceToken))
				&& Message.Contains(TEXT("pos=(2.0,14.0)"))
				&& Message.Contains(TEXT("size=(280.0,52.0)"));
			bSawRow1 |= Message.Contains(FString::Printf(
				TEXT("ReviewFrameGeometry change=tx_%s_slate_row_1 surface=%s mode=slate_row result=shown"),
				*OverlayCase.ChangePrefix,
				*OverlayCase.SurfaceToken))
				&& Message.Contains(TEXT("pos=(2.0,58.0)"))
				&& Message.Contains(TEXT("size=(280.0,52.0)"));
		}

		TestFalse(FString::Printf(TEXT("%s stable geometry does not fall back to review-list"), *OverlayCase.DebugSurfaceName), bSawReviewList);
		TestTrue(FString::Printf(TEXT("%s logs stable slate row 0"), *OverlayCase.DebugSurfaceName), bSawRow0);
		TestTrue(FString::Printf(TEXT("%s logs stable slate row 1"), *OverlayCase.DebugSurfaceName), bSawRow1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewDiffFrameBackgroundUsesTranslucentBaseTest,
	"BlueprintHelper.Review.UI.DiffFrameBackgroundUsesTranslucentBase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewDiffFrameBackgroundUsesTranslucentBaseTest::RunTest(const FString& Parameters)
{
	const FLinearColor FilledBackground =
		FBlueprintHelperReviewSurfaceFrameBuilder::GetDiffFrameBackgroundColor(true);
	TestTrue(TEXT("filled diff frame background uses alpha 0.6"),
		FMath::IsNearlyEqual(FilledBackground.A, 0.60f));
	TestTrue(TEXT("filled diff frame background remains visible"),
		FilledBackground.R > 0.0f && FilledBackground.G > 0.0f && FilledBackground.B > 0.0f);

	const FLinearColor TransparentBackground =
		FBlueprintHelperReviewSurfaceFrameBuilder::GetDiffFrameBackgroundColor(false);
	TestTrue(TEXT("explicit no-fill diff frame background stays transparent"),
		TransparentBackground.Equals(FLinearColor::Transparent));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewDiffFrameFillUsesChangeColorTest,
	"BlueprintHelper.Review.UI.DiffFrameFillUsesChangeColor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewDiffFrameFillUsesChangeColorTest::RunTest(const FString& Parameters)
{
	const FLinearColor AddedFrameColor(0.05f, 0.75f, 0.22f, 0.85f);
	const FLinearColor AddedFill =
		FBlueprintHelperReviewSurfaceFrameBuilder::GetDiffFrameFillColor(AddedFrameColor, true, false);
	TestTrue(TEXT("added fill keeps green channel dominant"),
		AddedFill.G > AddedFill.R && AddedFill.G > AddedFill.B);
	TestTrue(TEXT("added fill uses default review opacity"),
		FMath::IsNearlyEqual(AddedFill.A, 0.60f));

	const FLinearColor SelectedFill =
		FBlueprintHelperReviewSurfaceFrameBuilder::GetDiffFrameFillColor(AddedFrameColor, true, true);
	TestTrue(TEXT("selected fill is stronger than unselected fill"),
		SelectedFill.A > AddedFill.A);

	const FLinearColor NoFill =
		FBlueprintHelperReviewSurfaceFrameBuilder::GetDiffFrameFillColor(AddedFrameColor, false, false);
	TestTrue(TEXT("no-fill diff frame remains transparent"),
		NoFill.Equals(FLinearColor::Transparent));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewReadableChangeTitleTest,
	"BlueprintHelper.Review.UI.ReadableChangeTitleUsesReviewTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewReadableChangeTitleTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange WidgetChange;
	WidgetChange.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	FBlueprintHelperReviewAtomicTarget WidgetTarget;
	WidgetTarget.Surface = EBlueprintHelperReviewSurface::UMGWidgetTree;
	WidgetTarget.TargetKind = TEXT("umg_widget");
	WidgetTarget.TargetKey = TEXT("umg_widget:SmokeText");
	WidgetTarget.DisplayLabel = TEXT("SmokeText Widget");
	WidgetChange.AtomicTargets.Add(WidgetTarget);
	TestEqual(
		TEXT("widget row title is user-readable"),
		FBlueprintHelperReviewSurfaceFrameBuilder::BuildReadableChangeTitle(WidgetChange),
		FString(TEXT("\u4fee\u6539\u4e86[SmokeText]")));

	FBlueprintHelperReviewVisibleChange WidgetPropertyChange;
	WidgetPropertyChange.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	FBlueprintHelperReviewAtomicTarget WidgetPropertyTarget;
	WidgetPropertyTarget.Surface = EBlueprintHelperReviewSurface::UMGWidgetTree;
	WidgetPropertyTarget.TargetKind = TEXT("umg_widget_property");
	WidgetPropertyTarget.TargetKey = TEXT("umg_widget:SmokeText");
	WidgetPropertyTarget.PropertyPath = TEXT("Text");
	WidgetPropertyTarget.DisplayLabel = TEXT("SmokeText.Text");
	WidgetPropertyChange.AtomicTargets.Add(WidgetPropertyTarget);
	TestEqual(
		TEXT("widget property row title keeps widget anchor"),
		FBlueprintHelperReviewSurfaceFrameBuilder::BuildReadableChangeTitle(WidgetPropertyChange),
		FString(TEXT("\u4fee\u6539\u4e86[SmokeText]")));

	FBlueprintHelperReviewVisibleChange DataTableChange;
	DataTableChange.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	FBlueprintHelperReviewAtomicTarget DataTableTarget;
	DataTableTarget.Surface = EBlueprintHelperReviewSurface::DataTable;
	DataTableTarget.TargetKind = TEXT("datatable_row");
	DataTableTarget.TargetKey = TEXT("datatable_row:DamageSmall");
	DataTableTarget.DisplayLabel = TEXT("DamageSmall Row");
	DataTableChange.AtomicTargets.Add(DataTableTarget);
	TestEqual(
		TEXT("datatable row title includes row suffix"),
		FBlueprintHelperReviewSurfaceFrameBuilder::BuildReadableChangeTitle(DataTableChange),
		FString(TEXT("\u4fee\u6539\u4e86[DamageSmall]\u884c")));

	FBlueprintHelperReviewVisibleChange DataAssetChange;
	DataAssetChange.ChangeKind = EBlueprintHelperReviewChangeKind::VariableModified;
	FBlueprintHelperReviewAtomicTarget DataAssetTarget;
	DataAssetTarget.Surface = EBlueprintHelperReviewSurface::DataAsset;
	DataAssetTarget.TargetKind = TEXT("data_asset_property");
	DataAssetTarget.TargetKey = TEXT("data_asset_property:SmokeHealth");
	DataAssetTarget.PropertyPath = TEXT("SmokeHealth");
	DataAssetTarget.DisplayLabel = TEXT("SmokeHealth");
	DataAssetChange.AtomicTargets.Add(DataAssetTarget);
	TestEqual(
		TEXT("data asset property title includes variable suffix"),
		FBlueprintHelperReviewSurfaceFrameBuilder::BuildReadableChangeTitle(DataAssetChange),
		FString(TEXT("\u4fee\u6539\u4e86[SmokeHealth]\u53d8\u91cf")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelSurfacePlacementContractTest,
	"BlueprintHelper.Review.UI.PanelSurfacePlacementContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelSurfacePlacementContractTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("Blueprint structure panel owns Components"),
		static_cast<int32>(FBlueprintHelperReviewSurfacePresenterRouter::GetStructurePanelSurfaceForAssetKind(
			EBlueprintHelperReviewAssetKind::Blueprint)),
		static_cast<int32>(EBlueprintHelperReviewSurface::Components));
	TestEqual(
		TEXT("WidgetBlueprint structure panel owns WidgetTree"),
		static_cast<int32>(FBlueprintHelperReviewSurfacePresenterRouter::GetStructurePanelSurfaceForAssetKind(
			EBlueprintHelperReviewAssetKind::WidgetBlueprint)),
		static_cast<int32>(EBlueprintHelperReviewSurface::UMGWidgetTree));
	TestEqual(
		TEXT("Blueprint main workspace owns Graph"),
		static_cast<int32>(FBlueprintHelperReviewSurfacePresenterRouter::GetMainWorkspaceSurfaceForAssetKind(
			EBlueprintHelperReviewAssetKind::Blueprint)),
		static_cast<int32>(EBlueprintHelperReviewSurface::Graph));
	TestEqual(
		TEXT("WidgetBlueprint main workspace owns Graph"),
		static_cast<int32>(FBlueprintHelperReviewSurfacePresenterRouter::GetMainWorkspaceSurfaceForAssetKind(
			EBlueprintHelperReviewAssetKind::WidgetBlueprint)),
		static_cast<int32>(EBlueprintHelperReviewSurface::Graph));
	TestEqual(
		TEXT("DataTable main workspace owns DataTable"),
		static_cast<int32>(FBlueprintHelperReviewSurfacePresenterRouter::GetMainWorkspaceSurfaceForAssetKind(
			EBlueprintHelperReviewAssetKind::DataTable)),
		static_cast<int32>(EBlueprintHelperReviewSurface::DataTable));
	TestEqual(
		TEXT("DataAsset main workspace owns DataAsset"),
		static_cast<int32>(FBlueprintHelperReviewSurfacePresenterRouter::GetMainWorkspaceSurfaceForAssetKind(
			EBlueprintHelperReviewAssetKind::DataAsset)),
		static_cast<int32>(EBlueprintHelperReviewSurface::DataAsset));
	TestEqual(
		TEXT("GenericObject main workspace owns DataAsset presenter"),
		static_cast<int32>(FBlueprintHelperReviewSurfacePresenterRouter::GetMainWorkspaceSurfaceForAssetKind(
			EBlueprintHelperReviewAssetKind::GenericObject)),
		static_cast<int32>(EBlueprintHelperReviewSurface::DataAsset));
	TestTrue(TEXT("Details overlay remains details-only"),
		FBlueprintHelperReviewSurfacePresenterRouter::ShouldDetailsPanelOwnOverlay(
			EBlueprintHelperReviewSurface::Details));
	TestFalse(TEXT("Details overlay does not own WidgetTree"),
		FBlueprintHelperReviewSurfacePresenterRouter::ShouldDetailsPanelOwnOverlay(
			EBlueprintHelperReviewSurface::UMGWidgetTree));
	TestFalse(TEXT("Details overlay does not own DataTable"),
		FBlueprintHelperReviewSurfacePresenterRouter::ShouldDetailsPanelOwnOverlay(
			EBlueprintHelperReviewSurface::DataTable));
	TestFalse(TEXT("Details overlay does not own DataAsset"),
		FBlueprintHelperReviewSurfacePresenterRouter::ShouldDetailsPanelOwnOverlay(
			EBlueprintHelperReviewSurface::DataAsset));
	TestTrue(TEXT("Main workspace overlay owns DataTable"),
		FBlueprintHelperReviewSurfacePresenterRouter::ShouldMainWorkspaceOwnOverlay(
			EBlueprintHelperReviewSurface::DataTable));
	TestTrue(TEXT("Main workspace overlay owns DataAsset"),
		FBlueprintHelperReviewSurfacePresenterRouter::ShouldMainWorkspaceOwnOverlay(
			EBlueprintHelperReviewSurface::DataAsset));
	TestFalse(TEXT("Main workspace overlay does not own WidgetTree"),
		FBlueprintHelperReviewSurfacePresenterRouter::ShouldMainWorkspaceOwnOverlay(
			EBlueprintHelperReviewSurface::UMGWidgetTree));
	TestFalse(TEXT("Main workspace overlay does not own Details"),
		FBlueprintHelperReviewSurfacePresenterRouter::ShouldMainWorkspaceOwnOverlay(
			EBlueprintHelperReviewSurface::Details));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPresenterOverlayShowsOnlyReadySlateRowGeometryTest,
	"BlueprintHelper.Review.VisibleChange.PresenterOverlayShowsOnlyReadySlateRowGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPresenterOverlayShowsOnlyReadySlateRowGeometryTest::RunTest(const FString& Parameters)
{
	TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Items;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = FString::Printf(TEXT("tx_component_partial_geometry_%d"), Index);
		Change.AssetPath = TEXT("/Game/BlueprintHelper/Smoke/BP_ComponentSmoke");
		Change.DisplayLabel = FString::Printf(TEXT("Component row %d"), Index);
		Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;

		FBlueprintHelperReviewAtomicTarget Target;
		Target.Surface = EBlueprintHelperReviewSurface::Components;
		Target.TargetKind = TEXT("component");
		Target.TargetKey = FString::Printf(TEXT("component:Target%d"), Index);
		Target.DisplayLabel = FString::Printf(TEXT("Target%d"), Index);
		Change.AtomicTargets.Add(Target);
		Items.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(Change));
	}

	TArray<FString> DebugMessages;
	FBlueprintHelperReviewAssetContext Context;
	Context.AssetKind = EBlueprintHelperReviewAssetKind::Blueprint;
	FBlueprintHelperReviewPanelSurfacePresenterArgs Args;
	Args.AssetContext = &Context;
	Args.ChangeItems = &Items;
	Args.SelectedChange = Items[0];
	Args.AddDebugMessage = [&DebugMessages](const FString& Message)
	{
		DebugMessages.Add(Message);
	};
	Args.OnAcceptChange = [](TSharedPtr<FBlueprintHelperReviewVisibleChange>)
	{
		return FReply::Handled();
	};
	Args.OnRejectChange = [](TSharedPtr<FBlueprintHelperReviewVisibleChange>)
	{
		return FReply::Handled();
	};
	Args.GetChangeColor = [](EBlueprintHelperReviewChangeKind)
	{
		return FSlateColor(FLinearColor::Green);
	};
	Args.GetSelectedDiffColor = []()
	{
		return FSlateColor(FLinearColor::Yellow);
	};
	Args.ResolveRowGeometry.BindLambda([](
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
	{
		if (Change.ChangeId.EndsWith(TEXT("_0")))
		{
			OutAnchor.bIsValid = true;
			OutAnchor.Position = FVector2D(12.0f, 24.0f);
			OutAnchor.Size = FVector2D(260.0f, 32.0f);
			OutAnchor.Reason = TEXT("test_stable_slate_row_geometry");
			return true;
		}

		OutAnchor.Reason = TEXT("test_missing_slate_row_geometry");
		return false;
	});

	TSharedRef<SWidget> Overlay = FBlueprintHelperReviewBlueprintComponentsPresenter::BuildOverlay(Args);
	TestTrue(TEXT("partial geometry overlay renders only stable frames"),
		&Overlay.Get() != &SNullWidget::NullWidget.Get());

	bool bSawShownSlateRow0 = false;
	bool bSawPendingRow1 = false;
	bool bSawShownReviewList = false;
	for (const FString& Message : DebugMessages)
	{
		bSawShownSlateRow0 |= Message.Contains(
			TEXT("ReviewFrameGeometry change=tx_component_partial_geometry_0 surface=components mode=slate_row result=shown"));
		bSawPendingRow1 |= Message.Contains(
			TEXT("ReviewFrameGeometry change=tx_component_partial_geometry_1 surface=components mode=slate_row result=pending reason=geometry_not_ready"));
		bSawShownReviewList |= Message.Contains(TEXT("mode=review_list result=shown"));
	}

	TestTrue(TEXT("partial geometry renders stable slate row 0"), bSawShownSlateRow0);
	TestTrue(TEXT("partial geometry leaves row 1 pending"), bSawPendingRow1);
	TestFalse(TEXT("partial geometry does not show text review-list fallback"), bSawShownReviewList);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewUMGWidgetAnchorRoutesToUMGPresenterTest,
	"BlueprintHelper.Review.VisibleChange.UMGWidgetAnchorRoutesToWidgetPresenter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewUMGWidgetAnchorRoutesToUMGPresenterTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_umg_widget_presenter");
	Change.LocationKey = TEXT("umg_widget:SmokeText");
	FBlueprintHelperReviewAtomicTarget Target;
	Target.Surface = EBlueprintHelperReviewSurface::UMGWidgetTree;
	Target.TargetKind = TEXT("umg_widget");
	Target.TargetKey = TEXT("umg_widget:SmokeText");
	Target.DisplayLabel = TEXT("SmokeText");
	Change.AtomicTargets.Add(Target);

	TestTrue(TEXT("UMG widget anchor routes to UMG presenter"),
		FBlueprintHelperReviewUMGWidgetTreePresenter::ShouldShowChange(Change));
	TestFalse(TEXT("UMG widget anchor does not route to DataTable presenter"),
		FBlueprintHelperReviewDataTablePresenter::ShouldShowChange(Change));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewDataTableRowAnchorRoutesToDataTablePresenterTest,
	"BlueprintHelper.Review.VisibleChange.DataTableRowAnchorRoutesToDataTablePresenter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewDataTableRowAnchorRoutesToDataTablePresenterTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_datatable_row_presenter");
	Change.LocationKey = TEXT("datatable_row:DamageSmall");
	FBlueprintHelperReviewAtomicTarget Target;
	Target.Surface = EBlueprintHelperReviewSurface::DataTable;
	Target.TargetKind = TEXT("datatable_row");
	Target.TargetKey = TEXT("datatable_row:DamageSmall");
	Target.DisplayLabel = TEXT("DamageSmall");
	Change.AtomicTargets.Add(Target);

	TestTrue(TEXT("DataTable row anchor routes to DataTable presenter"),
		FBlueprintHelperReviewDataTablePresenter::ShouldShowChange(Change));
	TestFalse(TEXT("DataTable row anchor does not route to UMG presenter"),
		FBlueprintHelperReviewUMGWidgetTreePresenter::ShouldShowChange(Change));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewDataAssetPropertyAnchorRoutesToDataAssetPresenterTest,
	"BlueprintHelper.Review.VisibleChange.DataAssetPropertyAnchorRoutesToDataAssetPresenter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewDataAssetPropertyAnchorRoutesToDataAssetPresenterTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_data_asset_property_presenter");
	Change.LocationKey = TEXT("data_asset_property:SmokeHealth");
	FBlueprintHelperReviewAtomicTarget Target;
	Target.Surface = EBlueprintHelperReviewSurface::DataAsset;
	Target.TargetKind = TEXT("data_asset_property");
	Target.TargetKey = TEXT("data_asset_property:SmokeHealth");
	Target.PropertyPath = TEXT("SmokeHealth");
	Target.DisplayLabel = TEXT("SmokeHealth");
	Change.AtomicTargets.Add(Target);

	TestTrue(TEXT("DataAsset property anchor routes to DataAsset presenter"),
		FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange(Change));
	TestFalse(TEXT("DataAsset property anchor does not route to DataTable presenter"),
		FBlueprintHelperReviewDataTablePresenter::ShouldShowChange(Change));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRenameExpansionTest,
	"BlueprintHelper.Review.VisibleChange.RenameRendersAsDeleteAndAdd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRenameExpansionTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;

	FBlueprintHelperReviewTransactionInput Rename;
	Rename.TransactionId = TEXT("tx_rename");
	Rename.AssetPath = TEXT("/Game/BP_Door");
	Rename.LocationKey = TEXT("my_blueprint:function:OpenDoor");
	Rename.ChangeKind = EBlueprintHelperReviewChangeKind::Renamed;
	Rename.DisplayLabel = TEXT("OpenDoor -> OpenDoorFast");
	Rename.BeforeSummary = TEXT("OpenDoor");
	Rename.AfterSummary = TEXT("OpenDoorFast");

	TArray<FBlueprintHelperReviewTransactionInput> RenameInputs;
	RenameInputs.Add(Rename);
	const TArray<FBlueprintHelperReviewVisibleChange> Changes = Store.BuildVisibleChanges(RenameInputs);

	TestEqual(TEXT("rename expands to delete plus add"), Changes.Num(), 2);
	if (Changes.Num() == 2)
	{
		TestEqual(TEXT("first rename half is removed"), Changes[0].ChangeKind, EBlueprintHelperReviewChangeKind::Removed);
		TestEqual(TEXT("second rename half is added"), Changes[1].ChangeKind, EBlueprintHelperReviewChangeKind::Added);
		TestEqual(TEXT("removed side keeps before name"), Changes[0].BeforeSummary, FString(TEXT("OpenDoor")));
		TestEqual(TEXT("added side keeps after name"), Changes[1].AfterSummary, FString(TEXT("OpenDoorFast")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewVisibleChangeCollapseTest,
	"BlueprintHelper.Review.VisibleChange.CollapsesSupersededTransactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewVisibleChangeCollapseTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;

	FBlueprintHelperReviewTransactionInput T1;
	T1.TransactionId = TEXT("tx_t1");
	T1.AssetPath = TEXT("/Game/BP_Door");
	T1.GraphName = TEXT("EventGraph");
	T1.LocationKey = TEXT("graph:EventGraph/node:PrintString/input:InString");
	T1.ChangeKind = EBlueprintHelperReviewChangeKind::VariableModified;
	T1.DisplayLabel = TEXT("PrintString InString");
	T1.BeforeSummary = TEXT("Open");
	T1.AfterSummary = TEXT("Opening");

	FBlueprintHelperReviewTransactionInput T2 = T1;
	T2.TransactionId = TEXT("tx_t2");
	T2.BeforeSummary = TEXT("Opening");
	T2.AfterSummary = TEXT("Door Opened");

	TArray<FBlueprintHelperReviewTransactionInput> CollapseInputs;
	CollapseInputs.Add(T1);
	CollapseInputs.Add(T2);
	const TArray<FBlueprintHelperReviewVisibleChange> Changes = Store.BuildVisibleChanges(CollapseInputs);

	TestEqual(TEXT("one final visible change remains"), Changes.Num(), 1);
	if (Changes.Num() == 1)
	{
		const FBlueprintHelperReviewVisibleChange& Change = Changes[0];
		TestEqual(TEXT("latest transaction wins"), Change.LatestTransactionId, FString(TEXT("tx_t2")));
		TestEqual(TEXT("baseline before summary is preserved"), Change.BeforeSummary, FString(TEXT("Open")));
		TestEqual(TEXT("final after summary is preserved"), Change.AfterSummary, FString(TEXT("Door Opened")));
		TestEqual(TEXT("source transaction chain kept"), Change.SourceTransactionIds.Num(), 2);
		if (Change.SourceTransactionIds.Num() == 2)
		{
			TestEqual(TEXT("first source is T1"), Change.SourceTransactionIds[0], FString(TEXT("tx_t1")));
			TestEqual(TEXT("second source is T2"), Change.SourceTransactionIds[1], FString(TEXT("tx_t2")));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAtomicIntersectionCollapseTest,
	"BlueprintHelper.Review.VisibleChange.AtomicIntersectionUsesLatestTransaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAtomicIntersectionCollapseTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;

	FBlueprintHelperReviewAtomicTarget T1Node1;
	T1Node1.Surface = EBlueprintHelperReviewSurface::Graph;
	T1Node1.GraphName = TEXT("EventGraph");
	T1Node1.TargetKey = TEXT("node:N1");
	T1Node1.VisualGroupKey = TEXT("graph:EventGraph/block:DoorFlow");
	T1Node1.DisplayLabel = TEXT("Door flow");

	FBlueprintHelperReviewAtomicTarget T1Node2 = T1Node1;
	T1Node2.TargetKey = TEXT("node:N2");

	FBlueprintHelperReviewTransactionInput T1;
	T1.TransactionId = TEXT("tx_t1");
	T1.AssetPath = TEXT("/Game/BP_Door");
	T1.GraphName = TEXT("EventGraph");
	T1.LocationKey = TEXT("graph:EventGraph/block:DoorFlow");
	T1.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	T1.DisplayLabel = TEXT("Door flow");
	T1.BeforeSummary = TEXT("A");
	T1.AfterSummary = TEXT("B");
	T1.AtomicTargets.Add(T1Node1);
	T1.AtomicTargets.Add(T1Node2);

	FBlueprintHelperReviewAtomicTarget T2Node2 = T1Node2;
	FBlueprintHelperReviewAtomicTarget T2Node3 = T1Node1;
	T2Node3.TargetKey = TEXT("node:N3");

	FBlueprintHelperReviewTransactionInput T2 = T1;
	T2.TransactionId = TEXT("tx_t2");
	T2.BeforeSummary = TEXT("B");
	T2.AfterSummary = TEXT("C");
	T2.AtomicTargets.Reset();
	T2.AtomicTargets.Add(T2Node2);
	T2.AtomicTargets.Add(T2Node3);

	TArray<FBlueprintHelperReviewTransactionInput> Inputs;
	Inputs.Add(T1);
	Inputs.Add(T2);
	const TArray<FBlueprintHelperReviewVisibleChange> Changes = Store.BuildVisibleChanges(Inputs);

	TestEqual(TEXT("one visual block remains"), Changes.Num(), 1);
	if (Changes.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewVisibleChange& Change = Changes[0];
	TestEqual(TEXT("merged block keeps three atomic targets"), Change.AtomicTargets.Num(), 3);
	TestTrue(TEXT("leaf records both latest transactions"),
		Change.LatestTransactionIds.Contains(TEXT("tx_t1"))
		&& Change.LatestTransactionIds.Contains(TEXT("tx_t2")));

	const FBlueprintHelperReviewAtomicTarget* Node1 = Change.AtomicTargets.FindByPredicate(
		[](const FBlueprintHelperReviewAtomicTarget& Target)
		{
			return Target.TargetKey == TEXT("node:N1");
		});
	const FBlueprintHelperReviewAtomicTarget* Node2 = Change.AtomicTargets.FindByPredicate(
		[](const FBlueprintHelperReviewAtomicTarget& Target)
		{
			return Target.TargetKey == TEXT("node:N2");
		});
	const FBlueprintHelperReviewAtomicTarget* Node3 = Change.AtomicTargets.FindByPredicate(
		[](const FBlueprintHelperReviewAtomicTarget& Target)
		{
			return Target.TargetKey == TEXT("node:N3");
		});

	TestNotNull(TEXT("N1 remains visible"), Node1);
	TestNotNull(TEXT("N2 remains visible"), Node2);
	TestNotNull(TEXT("N3 remains visible"), Node3);
	if (Node1 && Node2 && Node3)
	{
		TestEqual(TEXT("N1 belongs to T1"), Node1->LatestTransactionId, FString(TEXT("tx_t1")));
		TestEqual(TEXT("N2 intersection belongs to T2"), Node2->LatestTransactionId, FString(TEXT("tx_t2")));
		TestEqual(TEXT("N3 belongs to T2"), Node3->LatestTransactionId, FString(TEXT("tx_t2")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewMultiSurfaceTreeModelTest,
	"BlueprintHelper.Review.VisibleChange.MultiSurfaceLeafTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewMultiSurfaceTreeModelTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;

	FBlueprintHelperReviewTransactionInput ComponentChange;
	ComponentChange.TransactionId = TEXT("tx_component");
	ComponentChange.AssetPath = TEXT("/Game/BP_Door");
	ComponentChange.LocationKey = TEXT("component:DoorMesh");
	ComponentChange.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
	ComponentChange.DisplayLabel = TEXT("DoorMesh");
	FBlueprintHelperReviewAtomicTarget ComponentTarget;
	ComponentTarget.Surface = EBlueprintHelperReviewSurface::Components;
	ComponentTarget.TargetKey = TEXT("component:DoorMesh");
	ComponentTarget.ComponentPath = TEXT("DefaultSceneRoot/DoorMesh");
	ComponentTarget.VisualGroupKey = TEXT("component:DoorMesh");
	ComponentChange.AtomicTargets.Add(ComponentTarget);

	FBlueprintHelperReviewTransactionInput PropertyChange;
	PropertyChange.TransactionId = TEXT("tx_property");
	PropertyChange.AssetPath = TEXT("/Game/BP_Door");
	PropertyChange.LocationKey = TEXT("property:SmokeValue");
	PropertyChange.ChangeKind = EBlueprintHelperReviewChangeKind::VariableModified;
	PropertyChange.DisplayLabel = TEXT("SmokeValue");
	FBlueprintHelperReviewAtomicTarget PropertyTarget;
	PropertyTarget.Surface = EBlueprintHelperReviewSurface::Details;
	PropertyTarget.TargetKey = TEXT("property:SmokeValue");
	PropertyTarget.PropertyPath = TEXT("Smoke.SmokeValue");
	PropertyTarget.VisualGroupKey = TEXT("property:SmokeValue");
	PropertyChange.AtomicTargets.Add(PropertyTarget);

	TArray<FBlueprintHelperReviewTransactionInput> Inputs;
	Inputs.Add(ComponentChange);
	Inputs.Add(PropertyChange);
	const TArray<FBlueprintHelperReviewVisibleChange> Changes = Store.BuildVisibleChanges(Inputs);

	TestEqual(TEXT("two leaves for one asset"), Changes.Num(), 2);
	if (Changes.Num() == 2)
	{
		TestTrue(TEXT("component leaf routes to Components"),
			BlueprintHelperReviewShouldShowInComponents(Changes[0])
			|| BlueprintHelperReviewShouldShowInComponents(Changes[1]));
		TestTrue(TEXT("property leaf routes to Details"),
			BlueprintHelperReviewShouldShowInDetails(Changes[0])
			|| BlueprintHelperReviewShouldShowInDetails(Changes[1]));
		TestEqual(TEXT("both leaves have the same asset"), Changes[0].AssetPath, FString(TEXT("/Game/BP_Door")));
		TestEqual(TEXT("both leaves have the same asset"), Changes[1].AssetPath, FString(TEXT("/Game/BP_Door")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRecordIdentityAssetFirstGroupingTest,
	"BlueprintHelper.Review.Record.IdentityIsArchiveSessionPlusAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRecordIdentityAssetFirstGroupingTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;

	FBlueprintHelperReviewAtomicTarget DoorTarget = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		TEXT("graph_node:N1"),
		TEXT("graph:EventGraph:block:DoorFlow"),
		TEXT("tx_1"));
	DoorTarget.AssetPath = TEXT("/Game/BP_Door");
	FBlueprintHelperReviewAtomicTarget WindowTarget = DoorTarget;
	WindowTarget.AssetPath = TEXT("/Game/BP_Window");
	WindowTarget.TargetKey = TEXT("graph_node:W1");
	WindowTarget.VisualGroupKey = TEXT("graph:EventGraph:block:WindowFlow");

	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(TEXT("archive_1"), TEXT("task_1"), TEXT("tx_1"), TEXT("/Game/BP_Door"), DoorTarget));
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(TEXT("archive_1"), TEXT("task_1"), TEXT("tx_2"), TEXT("/Game/BP_Window"), WindowTarget));

	const TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	TestEqual(TEXT("one archive session is split into asset-first records"), Records.Num(), 2);

	const FBlueprintHelperReviewRecord* DoorRecord = Records.FindByPredicate(
		[](const FBlueprintHelperReviewRecord& Record)
		{
			return Record.AssetPath == TEXT("/Game/BP_Door");
		});
	const FBlueprintHelperReviewRecord* WindowRecord = Records.FindByPredicate(
		[](const FBlueprintHelperReviewRecord& Record)
		{
			return Record.AssetPath == TEXT("/Game/BP_Window");
		});

	TestNotNull(TEXT("door asset record exists"), DoorRecord);
	TestNotNull(TEXT("window asset record exists"), WindowRecord);
	if (DoorRecord)
	{
		TestEqual(TEXT("review record id is archive plus asset"),
			DoorRecord->ReviewRecordId,
			FBlueprintHelperReviewStoreService::MakeReviewRecordId(TEXT("archive_1"), TEXT("/Game/BP_Door")));
		TestEqual(TEXT("source task run is preserved"), DoorRecord->SourceTaskRunIds.Num(), 1);
		TestEqual(TEXT("source summary counts one transaction"), DoorRecord->SourceTransactionSummary.TransactionCount, 1);
		TestEqual(TEXT("record storage starts active"),
			DoorRecord->StorageStatus,
			EBlueprintHelperReviewStorageStatus::Active);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRecordVisibleChangesKeepTargetAssetPathTest,
	"BlueprintHelper.Review.Record.VisibleChangesKeepTargetAssetPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRecordVisibleChangesKeepTargetAssetPathTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;

	FBlueprintHelperReviewAtomicTarget DataTableTarget;
	DataTableTarget.Surface = EBlueprintHelperReviewSurface::DataTable;
	DataTableTarget.AssetPath = TEXT("/Game/BlueprintHelper/Smoke/DT_SmokeDamageTable");
	DataTableTarget.TargetKind = TEXT("datatable_row");
	DataTableTarget.TargetKey = TEXT("datatable_row:DamageSmall");
	DataTableTarget.VisualGroupKey = TEXT("shared_surface_change");
	DataTableTarget.DisplayLabel = TEXT("DamageSmall");
	DataTableTarget.RecordedAfterHash = TEXT("after_hash_dt");
	DataTableTarget.BaselineHash = TEXT("baseline_hash");
	DataTableTarget.RollbackDataRef = TEXT("review://rollback/datatable");
	DataTableTarget.Ownership = TEXT("blueprinthelper_owned");

	FBlueprintHelperReviewAtomicTarget DataAssetTarget = DataTableTarget;
	DataAssetTarget.Surface = EBlueprintHelperReviewSurface::DataAsset;
	DataAssetTarget.AssetPath = TEXT("/Game/BlueprintHelper/Smoke/DA_SmokeTuning");
	DataAssetTarget.TargetKind = TEXT("data_asset_property");
	DataAssetTarget.TargetKey = TEXT("data_asset_property:SmokeHealth");
	DataAssetTarget.DisplayLabel = TEXT("SmokeHealth");
	DataAssetTarget.PropertyPath = TEXT("SmokeHealth");
	DataAssetTarget.RecordedAfterHash = TEXT("after_hash_da");
	DataAssetTarget.RollbackDataRef = TEXT("review://rollback/dataasset");

	FBlueprintHelperWriteReviewEvidence Evidence;
	Evidence.ArchiveSessionId = TEXT("archive_cross_asset_stage5");
	Evidence.TaskRunId = TEXT("task_cross_asset_stage5");
	Evidence.TransactionId = TEXT("tx_widget_save_cross_asset");
	Evidence.AssetPath = TEXT("/Game/BlueprintHelper/Smoke/WBP_WidgetSmoke");
	Evidence.OperationKind = TEXT("save_widget_blueprint");
	Evidence.DisplayLabel = TEXT("Widget save with referenced data changes");
	Evidence.AtomicTargets.Add(DataTableTarget);
	Evidence.AtomicTargets.Add(DataAssetTarget);

	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(Evidence);

	const TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	TestEqual(TEXT("one widget record still owns the transaction envelope"), Records.Num(), 1);
	if (Records.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewRecord& Record = Records[0];
	TestEqual(TEXT("two independent target-asset visible changes"), Record.VisibleChanges.Num(), 2);
	if (Record.VisibleChanges.Num() != 2)
	{
		return false;
	}

	const FBlueprintHelperReviewVisibleChange* DataTableChange = Record.VisibleChanges.FindByPredicate(
		[](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.AssetPath == TEXT("/Game/BlueprintHelper/Smoke/DT_SmokeDamageTable");
		});
	const FBlueprintHelperReviewVisibleChange* DataAssetChange = Record.VisibleChanges.FindByPredicate(
		[](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.AssetPath == TEXT("/Game/BlueprintHelper/Smoke/DA_SmokeTuning");
		});

	TestNotNull(TEXT("DataTable visible change keeps target asset path"), DataTableChange);
	TestNotNull(TEXT("DataAsset visible change keeps target asset path"), DataAssetChange);
	return DataTableChange && DataAssetChange;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRecordExplicitEvidenceDoesNotInferMissingAnchorTest,
	"BlueprintHelper.Review.Record.ExplicitEvidenceDoesNotInferMissingAnchor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRecordExplicitEvidenceDoesNotInferMissingAnchorTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;

	FBlueprintHelperReviewAtomicTarget MissingAnchorTarget = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		TEXT(""),
		TEXT("graph:EventGraph:block:DoorFlow"),
		TEXT("tx_missing_anchor"));
	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
		TEXT("archive_1"),
		TEXT("task_1"),
		TEXT("tx_missing_anchor"),
		TEXT("/Game/BP_Door"),
		MissingAnchorTarget));

	const TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	TestEqual(TEXT("record is retained for needs-action evidence"), Records.Num(), 1);
	if (Records.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewRecord& Record = Records[0];
	TestEqual(TEXT("one visible change is retained"), Record.VisibleChanges.Num(), 1);
	if (Record.VisibleChanges.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewVisibleChange& Change = Record.VisibleChanges[0];
	TestEqual(TEXT("missing target anchor is marked needs_action"),
		Change.Status,
		EBlueprintHelperReviewChangeStatus::NeedsAction);
	TestTrue(TEXT("needs action reason names missing anchor"),
		Change.NeedsActionReason.Contains(TEXT("missing_anchor")));
	TestEqual(TEXT("ReviewStore does not fill target key from visual group"),
		Change.AtomicTargets[0].TargetKey,
		FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRecordLatestWinsWithProducerHashesTest,
	"BlueprintHelper.Review.Record.LatestWinsPreservesProducerHashes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRecordLatestWinsWithProducerHashesTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;

	FBlueprintHelperReviewAtomicTarget T1 = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		TEXT("graph_node:N1"),
		TEXT("graph:EventGraph:block:DoorFlow"),
		TEXT("tx_1"),
		TEXT("after_1"));
	FBlueprintHelperReviewAtomicTarget T2 = T1;
	T2.LatestTransactionId = TEXT("tx_2");
	T2.SourceTransactionIds.Reset();
	T2.SourceTransactionIds.Add(TEXT("tx_2"));
	T2.RecordedAfterHash = TEXT("after_2");

	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(TEXT("archive_1"), TEXT("task_1"), TEXT("tx_1"), TEXT("/Game/BP_Door"), T1));
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(TEXT("archive_1"), TEXT("task_1"), TEXT("tx_2"), TEXT("/Game/BP_Door"), T2));

	const TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	TestEqual(TEXT("one asset record is built"), Records.Num(), 1);
	if (Records.Num() != 1 || Records[0].VisibleChanges.Num() != 1 || Records[0].VisibleChanges[0].AtomicTargets.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& Target = Records[0].VisibleChanges[0].AtomicTargets[0];
	TestEqual(TEXT("latest transaction wins for an atomic target"),
		Target.LatestTransactionId,
		FString(TEXT("tx_2")));
	TestEqual(TEXT("latest recorded_after_hash is preserved"),
		Target.RecordedAfterHash,
		FString(TEXT("after_2")));
	TestEqual(TEXT("source transaction chain is retained"),
		Target.SourceTransactionIds.Num(),
		2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRecordNormalizesLegacyDetailsSurfacesFromEvidenceTest,
	"BlueprintHelper.Review.Record.NormalizesLegacyDetailsSurfacesFromEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRecordNormalizesLegacyDetailsSurfacesFromEvidenceTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;

	struct FLegacySurfaceCase
	{
		FString AssetPath;
		FString TransactionId;
		FString OperationKind;
		FString TargetKind;
		FString TargetKey;
		EBlueprintHelperReviewSurface ExpectedSurface;
	};

	const TArray<FLegacySurfaceCase> Cases = {
		{
			TEXT("/Game/BlueprintHelper/Smoke/WBP_WidgetSmoke"),
			TEXT("tx_legacy_umg_surface"),
			TEXT("modify_umg_widget"),
			TEXT("umg_widget_property"),
			TEXT("umg_widget:SmokeText:Text"),
			EBlueprintHelperReviewSurface::UMGWidgetTree
		},
		{
			TEXT("/Game/BlueprintHelper/Smoke/DT_SmokeDamageTable"),
			TEXT("tx_legacy_datatable_surface"),
			TEXT("set_datatable_row"),
			TEXT("datatable_row"),
			TEXT("datatable_row:DamageSmall"),
			EBlueprintHelperReviewSurface::DataTable
		},
		{
			TEXT("/Game/BlueprintHelper/Smoke/DA_SmokeTuning"),
			TEXT("tx_legacy_dataasset_surface"),
			TEXT("set_object_property"),
			TEXT("object_property"),
			TEXT("object_property:SmokeHealth"),
			EBlueprintHelperReviewSurface::DataAsset
		}
	};

	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	for (const FLegacySurfaceCase& SurfaceCase : Cases)
	{
		FBlueprintHelperReviewAtomicTarget Target;
		Target.Surface = EBlueprintHelperReviewSurface::Details;
		Target.AssetPath = SurfaceCase.AssetPath;
		Target.TargetKind = SurfaceCase.TargetKind;
		Target.TargetKey = SurfaceCase.TargetKey;
		Target.VisualGroupKey = SurfaceCase.TargetKey;
		Target.DisplayLabel = SurfaceCase.TargetKey;
		Target.RecordedAfterHash = TEXT("after_hash");
		Target.BaselineHash = TEXT("baseline_hash");
		Target.RollbackDataRef = TEXT("review://rollback/legacy_surface");
		Target.Ownership = TEXT("blueprinthelper_owned");

		FBlueprintHelperWriteReviewEvidence Evidence;
		Evidence.ArchiveSessionId = TEXT("archive_legacy_surface");
		Evidence.TaskRunId = TEXT("task_legacy_surface");
		Evidence.TransactionId = SurfaceCase.TransactionId;
		Evidence.AssetPath = SurfaceCase.AssetPath;
		Evidence.OperationKind = SurfaceCase.OperationKind;
		Evidence.DisplayLabel = SurfaceCase.TargetKey;
		Evidence.AtomicTargets.Add(Target);
		Evidences.Add(Evidence);
	}

	const TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	TestEqual(TEXT("one record is built per asset"), Records.Num(), Cases.Num());
	if (Records.Num() != Cases.Num())
	{
		return false;
	}

	for (const FLegacySurfaceCase& SurfaceCase : Cases)
	{
		const FBlueprintHelperReviewRecord* Record = Records.FindByPredicate(
			[&SurfaceCase](const FBlueprintHelperReviewRecord& Candidate)
			{
				return Candidate.AssetPath == SurfaceCase.AssetPath;
			});
		TestNotNull(FString::Printf(TEXT("record exists for %s"), *SurfaceCase.AssetPath), Record);
		if (!Record || Record->VisibleChanges.Num() != 1 || Record->VisibleChanges[0].AtomicTargets.Num() != 1)
		{
			return false;
		}

		TestEqual(
			FString::Printf(TEXT("legacy Details target normalizes for %s"), *SurfaceCase.TargetKind),
			static_cast<int32>(Record->VisibleChanges[0].AtomicTargets[0].Surface),
			static_cast<int32>(SurfaceCase.ExpectedSurface));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRecordPreservesIndependentSurfaceStringsTest,
	"BlueprintHelper.Review.Record.PreservesIndependentSurfaceStrings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRecordPreservesIndependentSurfaceStringsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_independent_surfaces"));

	FBlueprintHelperReviewRecord Record;
	Record.ReviewRecordId = FBlueprintHelperReviewStoreService::MakeReviewRecordId(
		ArchiveSessionId,
		TEXT("/Game/BlueprintHelper/Smoke/WBP_WidgetSmoke"));
	Record.ArchiveSessionId = ArchiveSessionId;
	Record.AssetPath = TEXT("/Game/BlueprintHelper/Smoke/WBP_WidgetSmoke");

	struct FIndependentSurfaceCase
	{
		FString AssetPath;
		FString ChangeId;
		FString TargetKind;
		FString TargetKey;
		EBlueprintHelperReviewSurface Surface;
	};

	const TArray<FIndependentSurfaceCase> Cases = {
		{ TEXT("/Game/BlueprintHelper/Smoke/WBP_WidgetSmoke"), TEXT("tx_save_umg_surface"), TEXT("umg_widget"), TEXT("umg_widget:SmokeText"), EBlueprintHelperReviewSurface::UMGWidgetTree },
		{ TEXT("/Game/BlueprintHelper/Smoke/DT_SmokeDamageTable"), TEXT("tx_save_datatable_surface"), TEXT("datatable_row"), TEXT("datatable_row:DamageSmall"), EBlueprintHelperReviewSurface::DataTable },
		{ TEXT("/Game/BlueprintHelper/Smoke/DA_SmokeTuning"), TEXT("tx_save_dataasset_surface"), TEXT("data_asset_property"), TEXT("data_asset_property:SmokeHealth"), EBlueprintHelperReviewSurface::DataAsset }
	};
	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(Record.ReviewRecordId);

	for (const FIndependentSurfaceCase& SurfaceCase : Cases)
	{
		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = SurfaceCase.ChangeId;
		Change.AssetPath = SurfaceCase.AssetPath;
		Change.LocationKey = SurfaceCase.TargetKey;
		Change.LatestTransactionId = SurfaceCase.ChangeId;
		Change.SourceTransactionIds.Add(SurfaceCase.ChangeId);
		Change.DisplayLabel = SurfaceCase.TargetKey;

		FBlueprintHelperReviewAtomicTarget Target;
		Target.Surface = SurfaceCase.Surface;
		Target.AssetPath = SurfaceCase.AssetPath;
		Target.TargetKind = SurfaceCase.TargetKind;
		Target.TargetKey = SurfaceCase.TargetKey;
		Target.VisualGroupKey = SurfaceCase.TargetKey;
		Target.DisplayLabel = SurfaceCase.TargetKey;
		Target.LatestTransactionId = SurfaceCase.ChangeId;
		Target.SourceTransactionIds.Add(SurfaceCase.ChangeId);
		Target.RecordedAfterHash = TEXT("after_hash");
		Target.BaselineHash = TEXT("baseline_hash");
		Target.RollbackDataRef = TEXT("review://rollback/independent_surface");
		Target.Ownership = TEXT("blueprinthelper_owned");
		Change.AtomicTargets.Add(Target);
		Record.VisibleChanges.Add(Change);
	}

	FString SaveError;
	TestTrue(TEXT("record with independent surfaces saves"), Store.SaveReviewRecord(Record, SaveError));

	FBlueprintHelperReviewRecord Loaded;
	FString LoadError;
	TestTrue(TEXT("record with independent surfaces reloads"),
		Store.LoadReviewRecordById(Record.ReviewRecordId, Loaded, LoadError));
	TestEqual(TEXT("loaded change count"), Loaded.VisibleChanges.Num(), Cases.Num());
	if (Loaded.VisibleChanges.Num() != Cases.Num())
	{
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(Record.ReviewRecordId);
		return false;
	}

	for (const FIndependentSurfaceCase& SurfaceCase : Cases)
	{
		const FBlueprintHelperReviewVisibleChange* Change = Loaded.VisibleChanges.FindByPredicate(
			[&SurfaceCase](const FBlueprintHelperReviewVisibleChange& Candidate)
			{
				return Candidate.ChangeId == SurfaceCase.ChangeId;
			});
		TestNotNull(FString::Printf(TEXT("loaded change exists for %s"), *SurfaceCase.ChangeId), Change);
		if (!Change || Change->AtomicTargets.Num() != 1)
		{
			FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(Record.ReviewRecordId);
			return false;
		}

		TestEqual(
			FString::Printf(TEXT("independent surface is preserved for %s"), *SurfaceCase.TargetKind),
			static_cast<int32>(Change->AtomicTargets[0].Surface),
			static_cast<int32>(SurfaceCase.Surface));
		TestEqual(
			FString::Printf(TEXT("visible change keeps target asset path for %s"), *SurfaceCase.TargetKind),
			Change->AssetPath,
			SurfaceCase.AssetPath);
		TestEqual(
			FString::Printf(TEXT("atomic target keeps target asset path for %s"), *SurfaceCase.TargetKind),
			Change->AtomicTargets[0].AssetPath,
			SurfaceCase.AssetPath);
	}

	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(Record.ReviewRecordId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRecordPersistsDebugCaseIdsTest,
	"BlueprintHelper.Review.Record.PersistsDebugCaseIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRecordPersistsDebugCaseIdsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_debug_case_ids"));

	FBlueprintHelperReviewRecord Record;
	Record.ReviewRecordId = FBlueprintHelperReviewStoreService::MakeReviewRecordId(
		ArchiveSessionId,
		TEXT("/Game/BP_Door"));
	Record.ArchiveSessionId = ArchiveSessionId;
	Record.AssetPath = TEXT("/Game/BP_Door");
	Record.DebugCaseIds.Add(TEXT("debug_case_preview_blocked"));

	FString SaveError;
	TestTrue(TEXT("record with debug case ids saves"), Store.SaveReviewRecord(Record, SaveError));

	FBlueprintHelperReviewRecord Loaded;
	FString LoadError;
	TestTrue(TEXT("record with debug case ids reloads"),
		Store.LoadReviewRecordById(Record.ReviewRecordId, Loaded, LoadError));
	TestTrue(TEXT("debug_case_ids persist"),
		Loaded.DebugCaseIds.Contains(TEXT("debug_case_preview_blocked")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRecordMergesDebugCaseIdsTest,
	"BlueprintHelper.Review.Record.MergesDebugCaseIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRecordMergesDebugCaseIdsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_debug_case_merge"));
	const FString RecordId = FBlueprintHelperReviewStoreService::MakeReviewRecordId(
		ArchiveSessionId,
		TEXT("/Game/BP_Door"));

	FBlueprintHelperReviewRecord Existing;
	Existing.ReviewRecordId = RecordId;
	Existing.ArchiveSessionId = ArchiveSessionId;
	Existing.AssetPath = TEXT("/Game/BP_Door");
	Existing.DebugCaseIds.Add(TEXT("debug_case_existing"));

	FString SaveError;
	TestTrue(TEXT("existing debug case record saves"), Store.SaveReviewRecord(Existing, SaveError));

	FBlueprintHelperReviewRecord Incoming = Existing;
	Incoming.DebugCaseIds.Reset();
	Incoming.DebugCaseIds.Add(TEXT("debug_case_incoming"));

	TArray<FBlueprintHelperReviewRecord> Records;
	Records.Add(Incoming);
	TestTrue(TEXT("incoming debug case record merges"), Store.SaveReviewRecords(Records, SaveError));

	FBlueprintHelperReviewRecord Loaded;
	FString LoadError;
	TestTrue(TEXT("merged debug case record reloads"),
		Store.LoadReviewRecordById(RecordId, Loaded, LoadError));
	TestTrue(TEXT("existing debug case id remains"),
		Loaded.DebugCaseIds.Contains(TEXT("debug_case_existing")));
	TestTrue(TEXT("incoming debug case id is added"),
		Loaded.DebugCaseIds.Contains(TEXT("debug_case_incoming")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRecordBuildsDebugCaseIdsFromEvidenceTest,
	"BlueprintHelper.Review.Record.BuildsDebugCaseIdsFromEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRecordBuildsDebugCaseIdsFromEvidenceTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;

	FBlueprintHelperReviewAtomicTarget Target = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		TEXT("graph_node:N1"),
		TEXT("graph:EventGraph:block:DoorFlow"),
		TEXT("tx_debug_case_evidence"));
	FBlueprintHelperWriteReviewEvidence Evidence = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
		TEXT("archive_debug_case_evidence"),
		TEXT("task_debug_case_evidence"),
		TEXT("tx_debug_case_evidence"),
		TEXT("/Game/BP_Door"),
		Target);
	Evidence.DebugCaseIds.Add(TEXT("debug_case_preview_blocked"));
	Evidence.DebugCaseIds.Add(TEXT("debug_case_partial_failure"));

	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(Evidence);
	const TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	TestEqual(TEXT("one debug-linked record is built"), Records.Num(), 1);
	if (Records.Num() != 1)
	{
		return false;
	}

	TestTrue(TEXT("first evidence debug case id is copied"),
		Records[0].DebugCaseIds.Contains(TEXT("debug_case_preview_blocked")));
	TestTrue(TEXT("second evidence debug case id is copied"),
		Records[0].DebugCaseIds.Contains(TEXT("debug_case_partial_failure")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewJournalBackedEvidenceIncludesHashesTest,
	"BlueprintHelper.Review.Producer.JournalBackedEvidenceIncludesHashes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewJournalBackedEvidenceIncludesHashesTest::RunTest(const FString& Parameters)
{
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_journal_evidence"));
	FBlueprintHelperAppendJournalRecord JournalRecord;
	JournalRecord.TransactionId = TEXT("tx_journal_evidence");
	JournalRecord.ArchiveSessionId = ArchiveSessionId;
	JournalRecord.TaskRunId = TEXT("task_journal_evidence");
	JournalRecord.Tool = TEXT("AppendBlueprintGraph");
	JournalRecord.Status = TEXT("applied");
	JournalRecord.TargetAssets.Add(TEXT("/Game/BP_Door"));
	JournalRecord.GraphId = TEXT("EventGraph");
	JournalRecord.GraphName = TEXT("EventGraph");
	JournalRecord.BlockIds.Add(TEXT("DoorFlow"));
	JournalRecord.RollbackData = TEXT("{\"node_guids\":[\"N1\"]}");

	FBlueprintHelperTransactionJournalService JournalService;
	FString JournalError;
	TestTrue(TEXT("journal write succeeds"), JournalService.WriteAppendJournal(JournalRecord, JournalError));

	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewRecordQuery Query;
	Query.ArchiveSessionIdFilter = ArchiveSessionId;
	Query.bPendingOnly = false;
	const TArray<FBlueprintHelperReviewRecord> Records = Store.QueryReviewRecords(Query);
	TestEqual(TEXT("journal write creates one review record"), Records.Num(), 1);
	if (Records.Num() != 1 || Records[0].VisibleChanges.Num() == 0 || Records[0].VisibleChanges[0].AtomicTargets.Num() == 0)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& Target = Records[0].VisibleChanges[0].AtomicTargets[0];
	TestFalse(TEXT("baseline hash is emitted"), Target.BaselineHash.IsEmpty());
	TestFalse(TEXT("recorded after hash is emitted"), Target.RecordedAfterHash.IsEmpty());
	TestFalse(TEXT("rollback data ref is emitted"), Target.RollbackDataRef.IsEmpty());
	TestEqual(TEXT("journal evidence is complete enough to stay pending"),
		Target.Status,
		EBlueprintHelperReviewChangeStatus::Pending);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewLoadPendingVisibleChangesUsesRecordQueryTest,
	"BlueprintHelper.Review.UI.LoadPendingVisibleChangesUsesRecordQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewLoadPendingVisibleChangesUsesRecordQueryTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_ui_query"));
	const FString AssetPath = FString::Printf(
		TEXT("/Game/BP_Door_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));

	FBlueprintHelperReviewAtomicTarget PendingTarget = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		TEXT("graph_node:UIPending"),
		TEXT("graph:EventGraph:block:UIQuery"),
		TEXT("tx_ui_pending"));
	PendingTarget.AssetPath = AssetPath;

	FBlueprintHelperReviewAtomicTarget AcceptedTarget = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		TEXT("graph_node:UIAccepted"),
		TEXT("graph:EventGraph:block:UIAccepted"),
		TEXT("tx_ui_accepted"));
	AcceptedTarget.AssetPath = AssetPath;

	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
		ArchiveSessionId,
		TEXT("task_ui_query"),
		TEXT("tx_ui_pending"),
		AssetPath,
		PendingTarget));
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
		ArchiveSessionId,
		TEXT("task_ui_query"),
		TEXT("tx_ui_accepted"),
		AssetPath,
		AcceptedTarget));

	TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	TestEqual(TEXT("one UI query record is built"), Records.Num(), 1);
	if (Records.Num() != 1 || Records[0].VisibleChanges.Num() != 2)
	{
		return false;
	}

	for (FBlueprintHelperReviewVisibleChange& Change : Records[0].VisibleChanges)
	{
		if (Change.AtomicTargets.Num() > 0
			&& Change.AtomicTargets[0].TargetKey == AcceptedTarget.TargetKey)
		{
			Change.AtomicTargets[0].Status = EBlueprintHelperReviewChangeStatus::Accepted;
			Change.Status = EBlueprintHelperReviewChangeStatus::Accepted;
		}
	}
	Records[0].Status = EBlueprintHelperReviewChangeStatus::Pending;

	FString SaveError;
	TestTrue(TEXT("UI query record saves"), Store.SaveReviewRecords(Records, SaveError));

	const TArray<FBlueprintHelperReviewVisibleChange> PendingChanges =
		Store.LoadPendingVisibleChanges(AssetPath);
	const bool bContainsPending = PendingChanges.ContainsByPredicate(
		[&PendingTarget](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.AtomicTargets.ContainsByPredicate(
				[&PendingTarget](const FBlueprintHelperReviewAtomicTarget& Target)
				{
					return Target.TargetKey == PendingTarget.TargetKey;
				});
		});
	const bool bContainsAccepted = PendingChanges.ContainsByPredicate(
		[&AcceptedTarget](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.AtomicTargets.ContainsByPredicate(
				[&AcceptedTarget](const FBlueprintHelperReviewAtomicTarget& Target)
				{
					return Target.TargetKey == AcceptedTarget.TargetKey;
				});
		});

	TestTrue(TEXT("pending change is loaded through record query"), bContainsPending);
	TestFalse(TEXT("accepted change is excluded by pending query"), bContainsAccepted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewLoadPendingVisibleChangesSkipsMissingAssetInGlobalQueryTest,
	"BlueprintHelper.Review.UI.LoadPendingVisibleChangesSkipsMissingAssetInGlobalQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewLoadPendingVisibleChangesSkipsMissingAssetInGlobalQueryTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_ui_stale"));
	const FString AssetPath = FString::Printf(
		TEXT("/Game/BP_MissingReview_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));

	FBlueprintHelperReviewAtomicTarget PendingTarget = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		TEXT("graph_node:MissingReview"),
		TEXT("graph:EventGraph:block:MissingReview"),
		TEXT("tx_ui_stale"));
	PendingTarget.AssetPath = AssetPath;

	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
		ArchiveSessionId,
		TEXT("task_ui_stale"),
		TEXT("tx_ui_stale"),
		AssetPath,
		PendingTarget));

	TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	TestEqual(TEXT("one stale UI query record is built"), Records.Num(), 1);
	if (Records.Num() != 1)
	{
		return false;
	}

	FString SaveError;
	TestTrue(TEXT("stale UI query record saves"), Store.SaveReviewRecords(Records, SaveError));

	const auto ContainsMissingAsset = [&AssetPath](const FBlueprintHelperReviewVisibleChange& Change)
	{
		return Change.AssetPath == AssetPath;
	};

	const TArray<FBlueprintHelperReviewVisibleChange> GlobalPendingChanges =
		Store.LoadPendingVisibleChanges();
	const TArray<FBlueprintHelperReviewVisibleChange> ExplicitPendingChanges =
		Store.LoadPendingVisibleChanges(AssetPath);

	TestFalse(TEXT("global pending changes skip missing asset records"), GlobalPendingChanges.ContainsByPredicate(ContainsMissingAsset));
	TestTrue(TEXT("explicit asset pending query still exposes missing asset diagnostics"), ExplicitPendingChanges.ContainsByPredicate(ContainsMissingAsset));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAcceptVisibleChangeTest,
	"BlueprintHelper.Review.Action.AcceptUsesFinalVisibleChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAcceptVisibleChangeTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewActionService ActionService;

	FBlueprintHelperReviewTransactionInput T1;
	T1.TransactionId = TEXT("tx_t1");
	T1.AssetPath = TEXT("/Game/BP_Door");
	T1.LocationKey = TEXT("function:OpenDoor:signature");
	T1.ChangeKind = EBlueprintHelperReviewChangeKind::SignatureModified;
	T1.DisplayLabel = TEXT("OpenDoor signature");
	T1.BeforeSummary = TEXT("OpenDoor()");
	T1.AfterSummary = TEXT("OpenDoor(Input)");

	FBlueprintHelperReviewTransactionInput T2 = T1;
	T2.TransactionId = TEXT("tx_t2");
	T2.AfterSummary = TEXT("OpenDoor(Input, Speed)");

	TArray<FBlueprintHelperReviewTransactionInput> CollapseInputs;
	CollapseInputs.Add(T1);
	CollapseInputs.Add(T2);
	const TArray<FBlueprintHelperReviewVisibleChange> Changes = Store.BuildVisibleChanges(CollapseInputs);
	TestEqual(TEXT("one final visible change"), Changes.Num(), 1);
	if (Changes.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewActionResult Result = ActionService.AcceptVisibleChange(Changes[0]);
	TestTrue(TEXT("accept succeeds for final visible change"), Result.bSucceeded);
	TestEqual(TEXT("accept targets latest transaction"), Result.TargetTransactionId, FString(TEXT("tx_t2")));
	TestEqual(TEXT("accept marks change accepted"), Result.NewStatus, EBlueprintHelperReviewChangeStatus::Accepted);
	TestTrue(TEXT("superseded transaction data is compaction eligible"), Result.bSupersededDataCompactionEligible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectVisibleChangeTest,
	"BlueprintHelper.Review.Action.RejectRequestsArchiveBaselineRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewMixedLatestAcceptTest,
	"BlueprintHelper.Review.Action.MixedLatestLeafAcceptsWholeLeaf",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewMixedLatestAcceptTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_t2");
	Change.AssetPath = TEXT("/Game/BP_Door");
	Change.LatestTransactionId = TEXT("tx_t2");
	Change.LatestTransactionIds.Add(TEXT("tx_t1"));
	Change.LatestTransactionIds.Add(TEXT("tx_t2"));
	Change.SourceTransactionIds.Add(TEXT("tx_t1"));
	Change.SourceTransactionIds.Add(TEXT("tx_t2"));
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.AcceptVisibleChange(Change);

	TestTrue(TEXT("mixed latest leaf accepts as one unit"), Result.bSucceeded);
	TestFalse(TEXT("mixed latest leaf does not compact still-owned atom chains"), Result.bSupersededDataCompactionEligible);
	return true;
}

bool FBlueprintHelperReviewRejectVisibleChangeTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_t2");
	Change.AssetPath = TEXT("/Game/BP_Door");
	Change.LocationKey = TEXT("graph:EventGraph/node:PrintString/input:InString");
	Change.LatestTransactionId = TEXT("tx_t2");
	Change.SourceTransactionIds.Add(TEXT("tx_t1"));
	Change.SourceTransactionIds.Add(TEXT("tx_t2"));
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::VariableModified;
	Change.BeforeSummary = TEXT("Open");
	Change.AfterSummary = TEXT("Door Opened");

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.RejectVisibleChange(Change);

	TestFalse(TEXT("first slice does not fake a completed rollback"), Result.bSucceeded);
	TestEqual(TEXT("reject enters needs action until archive rollback backend is wired"),
		Result.NewStatus,
		EBlueprintHelperReviewChangeStatus::NeedsAction);
	TestEqual(TEXT("reject uses archive baseline rollback mode"),
		Result.RollbackMode,
		FString(TEXT("archive_baseline")));
	TestTrue(TEXT("needs action reason explains missing rollback backend"), !Result.Message.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectVisibleChangeSuccessTest,
	"BlueprintHelper.Review.Action.RejectSucceedsWithMatchingHashAndRollbackData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectVisibleChangeSuccessTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("change_1");
	Change.AssetPath = TEXT("/Game/BP_Door");
	Change.LatestTransactionId = TEXT("tx_2");
	Change.Status = EBlueprintHelperReviewChangeStatus::Pending;
	Change.AtomicTargets.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		TEXT("graph_node:N1"),
		TEXT("graph:EventGraph:block:DoorFlow"),
		TEXT("tx_2"),
		TEXT("after_2")));

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(TEXT("graph_node:N1"), TEXT("after_2"));
	Options.bRollbackExecutorAvailable = true;
	Options.bRollbackSucceeded = true;

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.RejectVisibleChange(Change, Options);

	TestTrue(TEXT("reject succeeds after strict hash match and rollback"), Result.bSucceeded);
	TestEqual(TEXT("reject marks change rejected"),
		Result.NewStatus,
		EBlueprintHelperReviewChangeStatus::Rejected);
	TestEqual(TEXT("reject targets latest transaction"),
		Result.TargetTransactionId,
		FString(TEXT("tx_2")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectVisibleChangeToctouMismatchTest,
	"BlueprintHelper.Review.Action.RejectBlocksCurrentStateMismatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectVisibleChangeToctouMismatchTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("change_1");
	Change.AssetPath = TEXT("/Game/BP_Door");
	Change.LatestTransactionId = TEXT("tx_2");
	Change.AtomicTargets.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		TEXT("graph_node:N1"),
		TEXT("graph:EventGraph:block:DoorFlow"),
		TEXT("tx_2"),
		TEXT("after_2")));

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(TEXT("graph_node:N1"), TEXT("user_changed"));
	Options.bRollbackExecutorAvailable = true;
	Options.bRollbackSucceeded = true;

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.RejectVisibleChange(Change, Options);

	TestFalse(TEXT("reject does not overwrite user changes"), Result.bSucceeded);
	TestEqual(TEXT("TOCTOU mismatch needs user action"),
		Result.NewStatus,
		EBlueprintHelperReviewChangeStatus::NeedsAction);
	TestTrue(TEXT("message reports current state changed"),
		Result.Message.Contains(TEXT("current_state_changed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAcceptTargetsPersistsActionHistoryTest,
	"BlueprintHelper.Review.Action.AcceptTargetsPersistsActionHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAcceptTargetsPersistsActionHistoryTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_accept"));
	FBlueprintHelperReviewAtomicTarget Target = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		TEXT("graph_node:N1"),
		TEXT("graph:EventGraph:block:DoorFlow"),
		TEXT("tx_accept"));
	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
		ArchiveSessionId,
		TEXT("task_accept"),
		TEXT("tx_accept"),
		TEXT("/Game/BP_Door"),
		Target));
	TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	TestEqual(TEXT("one record for accept persistence"), Records.Num(), 1);
	if (Records.Num() != 1)
	{
		return false;
	}

	FString SaveError;
	TestTrue(TEXT("record saved before accept"), Store.SaveReviewRecords(Records, SaveError));

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.AcceptReviewTargets(
		Records[0].ReviewRecordId,
		{TEXT("graph_node:N1")});
	TestTrue(TEXT("persisted accept succeeds"), Result.bSucceeded);

	FBlueprintHelperReviewRecordQuery Query;
	Query.ArchiveSessionIdFilter = ArchiveSessionId;
	Query.bPendingOnly = false;
	const TArray<FBlueprintHelperReviewRecord> LoadedRecords = Store.QueryReviewRecords(Query);
	TestEqual(TEXT("accepted record can be queried"), LoadedRecords.Num(), 1);
	if (LoadedRecords.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewRecord& Loaded = LoadedRecords[0];
	TestEqual(TEXT("record status accepted"),
		Loaded.Status,
		EBlueprintHelperReviewChangeStatus::Accepted);
	TestEqual(TEXT("action history records accept"), Loaded.ReviewActions.Num(), 1);
	if (Loaded.ReviewActions.Num() == 1)
	{
		TestEqual(TEXT("action name is accept"), Loaded.ReviewActions[0].Action, FString(TEXT("accept")));
		TestEqual(TEXT("accept keeps managed ownership policy"),
			Loaded.ReviewActions[0].OwnershipPolicy,
			FString(TEXT("keep_managed")));
	}
	TestEqual(TEXT("target status accepted"),
		Loaded.VisibleChanges[0].AtomicTargets[0].Status,
		EBlueprintHelperReviewChangeStatus::Accepted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPartialAcceptPropagatesPendingStatusTest,
	"BlueprintHelper.Review.Action.PartialAcceptPropagatesPendingStatus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPartialAcceptPropagatesPendingStatusTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_partial_accept"));
	FBlueprintHelperReviewAtomicTarget FirstTarget = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		TEXT("graph_node:N1"),
		TEXT("graph:EventGraph:block:DoorFlow"),
		TEXT("tx_partial_accept"));
	FBlueprintHelperReviewAtomicTarget SecondTarget = FirstTarget;
	SecondTarget.TargetKey = TEXT("graph_node:N2");

	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
		ArchiveSessionId,
		TEXT("task_partial_accept"),
		TEXT("tx_partial_accept"),
		TEXT("/Game/BP_Door"),
		FirstTarget));
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
		ArchiveSessionId,
		TEXT("task_partial_accept"),
		TEXT("tx_partial_accept"),
		TEXT("/Game/BP_Door"),
		SecondTarget));
	TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	FString SaveError;
	TestTrue(TEXT("record saved before partial accept"), Store.SaveReviewRecords(Records, SaveError));

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.AcceptReviewTargets(
		Records[0].ReviewRecordId,
		{TEXT("graph_node:N1")});
	TestTrue(TEXT("partial accept succeeds"), Result.bSucceeded);
	TestEqual(TEXT("partial accept leaves record pending"),
		Result.NewStatus,
		EBlueprintHelperReviewChangeStatus::Pending);

	FBlueprintHelperReviewRecord Loaded;
	FString LoadError;
	TestTrue(TEXT("partially accepted record reloads"),
		Store.LoadReviewRecordById(Records[0].ReviewRecordId, Loaded, LoadError));
	TestEqual(TEXT("record remains pending while one target is pending"),
		Loaded.Status,
		EBlueprintHelperReviewChangeStatus::Pending);
	TestEqual(TEXT("one visible change remains after partial accept"), Loaded.VisibleChanges.Num(), 1);
	if (Loaded.VisibleChanges.Num() != 1)
	{
		return false;
	}
	TestEqual(TEXT("visible change remains pending while one target is pending"),
		Loaded.VisibleChanges[0].Status,
		EBlueprintHelperReviewChangeStatus::Pending);
	TestEqual(TEXT("both targets remain after partial accept"), Loaded.VisibleChanges[0].AtomicTargets.Num(), 2);
	if (Loaded.VisibleChanges[0].AtomicTargets.Num() != 2)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget* AcceptedTarget = Loaded.VisibleChanges[0].AtomicTargets.FindByPredicate(
		[](const FBlueprintHelperReviewAtomicTarget& Target)
		{
			return Target.TargetKey == TEXT("graph_node:N1");
		});
	const FBlueprintHelperReviewAtomicTarget* PendingTarget = Loaded.VisibleChanges[0].AtomicTargets.FindByPredicate(
		[](const FBlueprintHelperReviewAtomicTarget& Target)
		{
			return Target.TargetKey == TEXT("graph_node:N2");
		});
	TestNotNull(TEXT("accepted target remains addressable"), AcceptedTarget);
	TestNotNull(TEXT("pending target remains addressable"), PendingTarget);
	if (AcceptedTarget)
	{
		TestEqual(TEXT("selected target is accepted"),
			AcceptedTarget->Status,
			EBlueprintHelperReviewChangeStatus::Accepted);
	}
	if (PendingTarget)
	{
		TestEqual(TEXT("unselected target remains pending"),
			PendingTarget->Status,
			EBlueprintHelperReviewChangeStatus::Pending);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectTargetsPersistsActionHistoryTest,
	"BlueprintHelper.Review.Action.RejectTargetsPersistsActionHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectTargetsPersistsActionHistoryTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_reject_targets"));
	FBlueprintHelperReviewAtomicTarget Target = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		TEXT("graph_node:N1"),
		TEXT("graph:EventGraph:block:DoorFlow"),
		TEXT("tx_reject_targets"),
		TEXT("after_reject"));
	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
		ArchiveSessionId,
		TEXT("task_reject_targets"),
		TEXT("tx_reject_targets"),
		TEXT("/Game/BP_Door"),
		Target));
	TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	FString SaveError;
	TestTrue(TEXT("record saved before reject targets"), Store.SaveReviewRecords(Records, SaveError));

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(TEXT("graph_node:N1"), TEXT("after_reject"));
	Options.bRollbackExecutorAvailable = true;
	Options.bRollbackSucceeded = true;

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.RejectReviewTargets(
		Records[0].ReviewRecordId,
		{TEXT("graph_node:N1")},
		Options);
	TestTrue(TEXT("persisted reject succeeds"), Result.bSucceeded);

	FBlueprintHelperReviewRecord Loaded;
	FString LoadError;
	TestTrue(TEXT("rejected record reloads"),
		Store.LoadReviewRecordById(Records[0].ReviewRecordId, Loaded, LoadError));
	TestEqual(TEXT("record status rejected"),
		Loaded.Status,
		EBlueprintHelperReviewChangeStatus::Rejected);
	TestEqual(TEXT("one visible change remains after reject"), Loaded.VisibleChanges.Num(), 1);
	if (Loaded.VisibleChanges.Num() != 1 || Loaded.VisibleChanges[0].AtomicTargets.Num() != 1)
	{
		return false;
	}
	TestEqual(TEXT("action history records reject"), Loaded.ReviewActions.Num(), 1);
	if (Loaded.ReviewActions.Num() == 1)
	{
		TestEqual(TEXT("action name is reject"), Loaded.ReviewActions[0].Action, FString(TEXT("reject")));
		TestEqual(TEXT("reject records archive baseline policy"),
			Loaded.ReviewActions[0].OwnershipPolicy,
			FString(TEXT("archive_baseline")));
		TestTrue(TEXT("reject action targets selected key"),
			Loaded.ReviewActions[0].TargetKeys.Contains(TEXT("graph_node:N1")));
	}
	TestEqual(TEXT("target status rejected"),
		Loaded.VisibleChanges[0].AtomicTargets[0].Status,
		EBlueprintHelperReviewChangeStatus::Rejected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectNeedsActionCreatesDebugCaseTest,
	"BlueprintHelper.Review.Integration.RejectNeedsActionCreatesDebugCase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectNeedsActionCreatesDebugCaseTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_reject_debug_needs_action"));
	FBlueprintHelperReviewAtomicTarget Target = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		TEXT("graph_node:N1"),
		TEXT("graph:EventGraph:block:DoorFlow"),
		TEXT("tx_reject_debug_needs_action"),
		TEXT("after_original"));
	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
		ArchiveSessionId,
		TEXT("task_reject_debug_needs_action"),
		TEXT("tx_reject_debug_needs_action"),
		TEXT("/Game/BP_Door"),
		Target));
	TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	FString SaveError;
	TestTrue(TEXT("record saved before reject needs-action debug capture"), Store.SaveReviewRecords(Records, SaveError));

	FBlueprintHelperDebugCaseStoreService DebugStore;
	FBlueprintHelperDebugEntryService DebugEntry(DebugStore);
	FBlueprintHelperReviewActionService ActionService(&DebugEntry);

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(TEXT("graph_node:N1"), TEXT("user_changed"));
	Options.bRollbackExecutorAvailable = true;
	Options.bRollbackSucceeded = true;

	const FBlueprintHelperReviewActionResult Result = ActionService.RejectReviewTargets(
		Records[0].ReviewRecordId,
		{ TEXT("graph_node:N1") },
		Options);
	TestFalse(TEXT("reject needs action reports non-success"), Result.bSucceeded);
	TestEqual(TEXT("reject reports needs action"),
		Result.NewStatus,
		EBlueprintHelperReviewChangeStatus::NeedsAction);

	FBlueprintHelperReviewRecord Loaded;
	FString LoadError;
	TestTrue(TEXT("needs-action record reloads"),
		Store.LoadReviewRecordById(Records[0].ReviewRecordId, Loaded, LoadError));
	TestEqual(TEXT("one debug case id is linked to review record"), Loaded.DebugCaseIds.Num(), 1);
	if (Loaded.DebugCaseIds.Num() != 1)
	{
		return false;
	}

	FBlueprintHelperDebugCase DebugCase;
	FString DebugLoadError;
	TestTrue(TEXT("linked needs-action debug case loads"),
		DebugStore.LoadCase(Loaded.DebugCaseIds[0], DebugCase, &DebugLoadError));
	TestEqual(TEXT("debug source identifies review needs action"),
		DebugCase.Source,
		FString(TEXT("review_reject_needs_action")));
	TestTrue(TEXT("debug case carries review asset path"),
		DebugCase.AssetPaths.Contains(TEXT("/Game/BP_Door")));
	TestTrue(TEXT("debug case links originating review record id"),
		DebugCase.ReviewRecordIds.Contains(Loaded.ReviewRecordId));
	TestEqual(TEXT("debug case links source transaction summary"), DebugCase.TransactionLinks.Num(), 1);
	if (DebugCase.TransactionLinks.Num() == 1)
	{
		TestEqual(TEXT("debug case links source transaction id"),
			DebugCase.TransactionLinks[0].TransactionId,
			FString(TEXT("tx_reject_debug_needs_action")));
		TestEqual(TEXT("debug case marks transaction link role"),
			DebugCase.TransactionLinks[0].Role,
			FString(TEXT("review_reject_failed")));
	}
	TestTrue(TEXT("debug case message carries reject reason"),
		DebugCase.Error.Message.Contains(TEXT("current_state_changed")));
	IFileManager::Get().Delete(*FBlueprintHelperDebugCaseStoreService::GetCasePath(Loaded.DebugCaseIds[0]), false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectFailedCreatesDebugCaseTest,
	"BlueprintHelper.Review.Integration.RejectFailedCreatesDebugCase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectFailedCreatesDebugCaseTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_reject_debug_failed"));
	FBlueprintHelperReviewAtomicTarget Target = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		TEXT("graph_node:N1"),
		TEXT("graph:EventGraph:block:DoorFlow"),
		TEXT("tx_reject_debug_failed"),
		TEXT("after_failed"));
	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
		ArchiveSessionId,
		TEXT("task_reject_debug_failed"),
		TEXT("tx_reject_debug_failed"),
		TEXT("/Game/BP_Door"),
		Target));
	TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	FString SaveError;
	TestTrue(TEXT("record saved before reject-failed debug capture"), Store.SaveReviewRecords(Records, SaveError));

	FBlueprintHelperDebugCaseStoreService DebugStore;
	FBlueprintHelperDebugEntryService DebugEntry(DebugStore);
	FBlueprintHelperReviewActionService ActionService(&DebugEntry);

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(TEXT("graph_node:N1"), TEXT("after_failed"));
	Options.bRollbackExecutorAvailable = true;
	Options.bRollbackSucceeded = false;
	Options.RollbackFailureMessage = TEXT("rollback_backend_failed");

	const FBlueprintHelperReviewActionResult Result = ActionService.RejectReviewTargets(
		Records[0].ReviewRecordId,
		{ TEXT("graph_node:N1") },
		Options);
	TestFalse(TEXT("reject failed reports non-success"), Result.bSucceeded);
	TestEqual(TEXT("reject reports reject failed"),
		Result.NewStatus,
		EBlueprintHelperReviewChangeStatus::RejectFailed);

	FBlueprintHelperReviewRecord Loaded;
	FString LoadError;
	TestTrue(TEXT("reject-failed record reloads"),
		Store.LoadReviewRecordById(Records[0].ReviewRecordId, Loaded, LoadError));
	TestEqual(TEXT("one reject-failed debug case id is linked"), Loaded.DebugCaseIds.Num(), 1);
	if (Loaded.DebugCaseIds.Num() != 1)
	{
		return false;
	}

	FBlueprintHelperDebugCase DebugCase;
	FString DebugLoadError;
	TestTrue(TEXT("linked reject-failed debug case loads"),
		DebugStore.LoadCase(Loaded.DebugCaseIds[0], DebugCase, &DebugLoadError));
	TestEqual(TEXT("debug source identifies review reject failure"),
		DebugCase.Source,
		FString(TEXT("review_reject_failed")));
	TestEqual(TEXT("debug case records review reject operation"),
		DebugCase.Operation,
		FString(TEXT("reject_review_targets")));
	TestTrue(TEXT("reject-failed debug case links originating review record id"),
		DebugCase.ReviewRecordIds.Contains(Loaded.ReviewRecordId));
	TestEqual(TEXT("reject-failed debug case links source transaction summary"), DebugCase.TransactionLinks.Num(), 1);
	if (DebugCase.TransactionLinks.Num() == 1)
	{
		TestEqual(TEXT("reject-failed debug case links source transaction id"),
			DebugCase.TransactionLinks[0].TransactionId,
			FString(TEXT("tx_reject_debug_failed")));
		TestEqual(TEXT("reject-failed debug case marks transaction link role"),
			DebugCase.TransactionLinks[0].Role,
			FString(TEXT("review_reject_failed")));
	}
	TestTrue(TEXT("debug case message carries rollback failure"),
		DebugCase.Error.Message.Contains(TEXT("rollback_backend_failed")));
	IFileManager::Get().Delete(*FBlueprintHelperDebugCaseStoreService::GetCasePath(Loaded.DebugCaseIds[0]), false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectAllPersistsToctouNeedsActionTest,
	"BlueprintHelper.Review.Action.RejectAllPersistsToctouNeedsAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectAllPersistsToctouNeedsActionTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_reject_all"));
	FBlueprintHelperReviewAtomicTarget Target = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		TEXT("graph_node:N1"),
		TEXT("graph:EventGraph:block:DoorFlow"),
		TEXT("tx_reject_all"),
		TEXT("after_original"));
	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
		ArchiveSessionId,
		TEXT("task_reject_all"),
		TEXT("tx_reject_all"),
		TEXT("/Game/BP_Door"),
		Target));
	TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	FString SaveError;
	TestTrue(TEXT("record saved before reject all"), Store.SaveReviewRecords(Records, SaveError));

	FBlueprintHelperReviewRecordQuery Query;
	Query.ArchiveSessionIdFilter = ArchiveSessionId;
	Query.bPendingOnly = true;

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(TEXT("graph_node:N1"), TEXT("user_changed"));
	Options.bRollbackExecutorAvailable = true;
	Options.bRollbackSucceeded = true;

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.RejectAll(Query, Options);
	TestFalse(TEXT("reject all reports non-success when target needs action"), Result.bSucceeded);
	TestEqual(TEXT("reject all reports needs action"),
		Result.NewStatus,
		EBlueprintHelperReviewChangeStatus::NeedsAction);

	FBlueprintHelperReviewRecordQuery LoadQuery;
	LoadQuery.ArchiveSessionIdFilter = ArchiveSessionId;
	LoadQuery.bPendingOnly = false;
	const TArray<FBlueprintHelperReviewRecord> LoadedRecords = Store.QueryReviewRecords(LoadQuery);
	TestEqual(TEXT("reject all record can be queried"), LoadedRecords.Num(), 1);
	if (LoadedRecords.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewRecord& Loaded = LoadedRecords[0];
	TestEqual(TEXT("record status needs action after mismatch"),
		Loaded.Status,
		EBlueprintHelperReviewChangeStatus::NeedsAction);
	TestTrue(TEXT("reject action was recorded"), Loaded.ReviewActions.Num() >= 1);
	TestTrue(TEXT("needs action reason records current state change"),
		Loaded.VisibleChanges[0].NeedsActionReason.Contains(TEXT("current_state_changed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectAllIteratesPendingTargetsTest,
	"BlueprintHelper.Review.Action.RejectAllIteratesPendingTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectAllIteratesPendingTargetsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_reject_all_iterates"));
	FBlueprintHelperReviewAtomicTarget FirstTarget = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		TEXT("graph_node:N1"),
		TEXT("graph:EventGraph:block:DoorFlow"),
		TEXT("tx_reject_all_iterates"),
		TEXT("after_first"));
	FBlueprintHelperReviewAtomicTarget SecondTarget = FirstTarget;
	SecondTarget.TargetKey = TEXT("graph_node:N2");
	SecondTarget.RecordedAfterHash = TEXT("after_second");

	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
		ArchiveSessionId,
		TEXT("task_reject_all_iterates"),
		TEXT("tx_reject_all_iterates"),
		TEXT("/Game/BP_Door"),
		FirstTarget));
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
		ArchiveSessionId,
		TEXT("task_reject_all_iterates"),
		TEXT("tx_reject_all_iterates"),
		TEXT("/Game/BP_Door"),
		SecondTarget));
	TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	FString SaveError;
	TestTrue(TEXT("record saved before reject all iteration"), Store.SaveReviewRecords(Records, SaveError));

	FBlueprintHelperReviewRecordQuery Query;
	Query.ArchiveSessionIdFilter = ArchiveSessionId;
	Query.bPendingOnly = true;

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(TEXT("graph_node:N1"), TEXT("after_first"));
	Options.CurrentHashesByTargetKey.Add(TEXT("graph_node:N2"), TEXT("after_second"));
	Options.bRollbackExecutorAvailable = true;
	Options.bRollbackSucceeded = true;

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.RejectAll(Query, Options);
	TestTrue(TEXT("reject all succeeds when every pending target rolls back"), Result.bSucceeded);
	TestEqual(TEXT("reject all reports rejected"),
		Result.NewStatus,
		EBlueprintHelperReviewChangeStatus::Rejected);

	FBlueprintHelperReviewRecord Loaded;
	FString LoadError;
	TestTrue(TEXT("reject all iteration record reloads"),
		Store.LoadReviewRecordById(Records[0].ReviewRecordId, Loaded, LoadError));
	TestEqual(TEXT("record status rejected after all targets reject"),
		Loaded.Status,
		EBlueprintHelperReviewChangeStatus::Rejected);
	TestEqual(TEXT("one visible change remains after reject all"), Loaded.VisibleChanges.Num(), 1);
	if (Loaded.VisibleChanges.Num() != 1)
	{
		return false;
	}
	TestEqual(TEXT("visible change status rejected after all targets reject"),
		Loaded.VisibleChanges[0].Status,
		EBlueprintHelperReviewChangeStatus::Rejected);
	TestEqual(TEXT("both targets remain after reject all"), Loaded.VisibleChanges[0].AtomicTargets.Num(), 2);
	if (Loaded.VisibleChanges[0].AtomicTargets.Num() != 2)
	{
		return false;
	}
	for (const FBlueprintHelperReviewAtomicTarget& Target : Loaded.VisibleChanges[0].AtomicTargets)
	{
		TestEqual(TEXT("each pending target was rejected"),
			Target.Status,
			EBlueprintHelperReviewChangeStatus::Rejected);
	}
	TestEqual(TEXT("one reject action records the batch"), Loaded.ReviewActions.Num(), 1);
	if (Loaded.ReviewActions.Num() == 1)
	{
		TestTrue(TEXT("reject all action includes first target"),
			Loaded.ReviewActions[0].TargetKeys.Contains(TEXT("graph_node:N1")));
		TestTrue(TEXT("reject all action includes second target"),
			Loaded.ReviewActions[0].TargetKeys.Contains(TEXT("graph_node:N2")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewConvertOwnerBlockRequiresSettingProfileTest,
	"BlueprintHelper.Review.Action.ConvertOwnerBlockRequiresSettingProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewConvertOwnerBlockRequiresSettingProfileTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_convert_owner_policy"));
	FBlueprintHelperReviewAtomicTarget Target = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		TEXT("graph:EventGraph:block:DoorFlow"),
		TEXT("graph:EventGraph:block:DoorFlow"),
		TEXT("tx_convert_policy"));
	Target.TargetKind = TEXT("graph_block");
	Target.Ownership = TEXT("blueprinthelper_owned");
	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
		ArchiveSessionId,
		TEXT("task_convert_policy"),
		TEXT("tx_convert_policy"),
		TEXT("/Game/BP_Door"),
		Target));
	TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	FString SaveError;
	TestTrue(TEXT("record saved before convert owner policy gate"), Store.SaveReviewRecords(Records, SaveError));

	FBlueprintHelperReviewConvertOwnerBlockRequest Request;
	Request.ReviewRecordId = Records[0].ReviewRecordId;
	Request.Direction = TEXT("bh_to_user");
	Request.BlockTargetKey = TEXT("graph:EventGraph:block:DoorFlow");
	Request.EntryAnchor = TEXT("graph:EventGraph:entry:N1");
	Request.NodeAnchors.Add(TEXT("graph:EventGraph:node:N1"));
	Request.DesiredBlockRef = TEXT("DoorFlow");
	Request.ConversionTransactionId = TEXT("tx_convert_policy_denied");
	Request.bSettingProfileAllowsConversion = false;

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.ConvertOwnerBlock(Request);
	TestFalse(TEXT("convert owner action is blocked by settings"), Result.bSucceeded);
	TestEqual(TEXT("setting profile gate is reported"),
		Result.Message,
		FString(TEXT("convert_owner_block_not_allowed_by_setting_profile")));

	FBlueprintHelperReviewRecord Loaded;
	FString LoadError;
	TestTrue(TEXT("policy-gated record reloads"),
		Store.LoadReviewRecordById(Records[0].ReviewRecordId, Loaded, LoadError));
	TestEqual(TEXT("conversion policy failure action is recorded"), Loaded.ReviewActions.Num(), 1);
	if (Loaded.ReviewActions.Num() == 1)
	{
		TestEqual(TEXT("policy failure action keeps message"),
			Loaded.ReviewActions[0].Message,
			FString(TEXT("convert_owner_block_not_allowed_by_setting_profile")));
		TestTrue(TEXT("policy gate does not record conversion transaction"),
			Loaded.ReviewActions[0].SourceTransactionId.IsEmpty());
	}
	TestEqual(TEXT("one visible change remains after policy gate"), Loaded.VisibleChanges.Num(), 1);
	if (Loaded.VisibleChanges.Num() != 1 || Loaded.VisibleChanges[0].AtomicTargets.Num() != 1)
	{
		return false;
	}
	TestEqual(TEXT("ownership is unchanged after policy gate"),
		Loaded.VisibleChanges[0].AtomicTargets[0].Ownership,
		FString(TEXT("blueprinthelper_owned")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewConvertOwnerBlockRequiresNodeAnchorsTest,
	"BlueprintHelper.Review.Action.ConvertOwnerBlockRequiresNodeAnchors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewConvertOwnerBlockRequiresNodeAnchorsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_convert_owner"));
	FBlueprintHelperReviewAtomicTarget Target = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		TEXT("graph:EventGraph:block:DoorFlow"),
		TEXT("graph:EventGraph:block:DoorFlow"),
		TEXT("tx_convert"));
	Target.TargetKind = TEXT("graph_block");
	Target.Ownership = TEXT("blueprinthelper_owned");
	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
		ArchiveSessionId,
		TEXT("task_convert"),
		TEXT("tx_convert"),
		TEXT("/Game/BP_Door"),
		Target));
	TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	FString SaveError;
	TestTrue(TEXT("record saved before convert owner"), Store.SaveReviewRecords(Records, SaveError));

	FBlueprintHelperReviewConvertOwnerBlockRequest Request;
	Request.ReviewRecordId = Records[0].ReviewRecordId;
	Request.Direction = TEXT("bh_to_user");
	Request.BlockTargetKey = TEXT("graph:EventGraph:block:DoorFlow");
	Request.EntryAnchor = TEXT("graph:EventGraph:entry:N1");
	Request.DesiredBlockRef = TEXT("DoorFlow");
	Request.ConversionTransactionId = TEXT("tx_convert_owner");
	Request.bSettingProfileAllowsConversion = true;

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.ConvertOwnerBlock(Request);
	TestFalse(TEXT("convert owner action is blocked"), Result.bSucceeded);
	TestEqual(TEXT("missing node anchors are reported"),
		Result.Message,
		FString(TEXT("missing_convert_owner_block_node_anchors")));

	FBlueprintHelperReviewRecordQuery Query;
	Query.ArchiveSessionIdFilter = ArchiveSessionId;
	Query.bPendingOnly = false;
	const TArray<FBlueprintHelperReviewRecord> LoadedRecords = Store.QueryReviewRecords(Query);
	TestEqual(TEXT("converted record can be queried"), LoadedRecords.Num(), 1);
	if (LoadedRecords.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewRecord& Loaded = LoadedRecords[0];
	TestEqual(TEXT("conversion failure action is recorded"), Loaded.ReviewActions.Num(), 1);
	if (Loaded.ReviewActions.Num() == 1)
	{
		TestEqual(TEXT("failure action keeps message"),
			Loaded.ReviewActions[0].Message,
			FString(TEXT("missing_convert_owner_block_node_anchors")));
	}
	TestEqual(TEXT("ownership is unchanged in record"),
		Loaded.VisibleChanges[0].AtomicTargets[0].Ownership,
		FString(TEXT("blueprinthelper_owned")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewConvertOwnerBlockExecutesBhToUserTest,
	"BlueprintHelper.Review.Action.ConvertOwnerBlockExecutesBhToUser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewConvertOwnerBlockExecutesBhToUserTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("ConvertOwnerBhToUser"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0)
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString GraphName = Graph ? Graph->GetName() : FString();
	const FString BlockRef = TEXT("DoorFlow");
	const FString BlockId = FBlueprintHelperReviewStoreService::NormalizeGraphBlockTargetId(GraphName, BlockRef);
	UK2Node_CustomEvent* EventNode = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("ReviewConvertDoorFlow"));
	TestNotNull(TEXT("test event node created"), EventNode);
	if (!Graph || !EventNode)
	{
		return false;
	}

	FBlueprintHelperReviewStoreServiceTestsLocalUtils::MarkReviewNodeAsBlueprintHelperOwned(EventNode, BlockId);
	TestTrue(TEXT("node starts BlueprintHelper-owned"), FBlueprintHelperReviewStoreServiceTestsLocalUtils::IsReviewNodeBlueprintHelperOwned(EventNode));

	FBlueprintHelperReviewAtomicTarget Target = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		FString::Printf(TEXT("graph:%s:block:%s"), *GraphName, *BlockId),
		FString::Printf(TEXT("graph:%s:block:%s"), *GraphName, *BlockId),
		TEXT("tx_convert_real"));
	Target.AssetPath = Blueprint->GetPathName();
	Target.GraphName = GraphName;
	Target.TargetKind = TEXT("graph_block");
	Target.Ownership = TEXT("blueprinthelper_owned");

	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_convert_owner_real"));
	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
		ArchiveSessionId,
		TEXT("task_convert_real"),
		TEXT("tx_convert_real"),
		Blueprint->GetPathName(),
		Target));
	TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	FString SaveError;
	TestTrue(TEXT("record saved before real convert owner"), Store.SaveReviewRecords(Records, SaveError));

	FBlueprintHelperReviewConvertOwnerBlockRequest Request;
	Request.ReviewRecordId = Records[0].ReviewRecordId;
	Request.Direction = TEXT("bh_to_user");
	Request.BlockTargetKey = Target.TargetKey;
	Request.EntryAnchor = FString::Printf(TEXT("graph:%s:entry:%s"), *GraphName, *EventNode->GetName());
	Request.NodeAnchors.Add(FString::Printf(TEXT("graph:%s:node:%s"), *GraphName, *EventNode->GetName()));
	Request.DesiredBlockRef = BlockRef;
	Request.ConversionTransactionId = TEXT("tx_review_convert_owner_real");
	Request.bSettingProfileAllowsConversion = true;

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.ConvertOwnerBlock(Request);
	TestTrue(TEXT("convert owner action succeeds"), Result.bSucceeded);
	TestFalse(TEXT("node ownership metadata is removed"), FBlueprintHelperReviewStoreServiceTestsLocalUtils::IsReviewNodeBlueprintHelperOwned(EventNode));

	FBlueprintHelperReviewRecord LoadedRecord;
	FString LoadError;
	TestTrue(TEXT("converted review record reloads"), Store.LoadReviewRecordById(Records[0].ReviewRecordId, LoadedRecord, LoadError));
	if (LoadedRecord.VisibleChanges.Num() == 0 || LoadedRecord.VisibleChanges[0].AtomicTargets.Num() == 0)
	{
		AddError(TEXT("converted review record has no visible target"));
		return false;
	}

	TestEqual(TEXT("ownership updated in persisted record"),
		LoadedRecord.VisibleChanges[0].AtomicTargets[0].Ownership,
		FString(TEXT("user_owned")));
	TestEqual(TEXT("conversion action recorded"), LoadedRecord.ReviewActions.Num(), 1);
	if (LoadedRecord.ReviewActions.Num() == 1)
	{
		TestEqual(TEXT("conversion transaction linked"),
			LoadedRecord.ReviewActions[0].SourceTransactionId,
			FString(TEXT("tx_review_convert_owner_real")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectDefaultDispatcherRemovesSelectedGraphNodeTest,
	"BlueprintHelper.Review.Rollback.RejectRemovesOnlySelectedGraphNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectDefaultDispatcherRemovesSelectedGraphNodeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("RejectSelectedGraphNode"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* SelectedNode = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("ReviewRejectSelected"));
	UK2Node_CustomEvent* UnselectedNode = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("ReviewRejectUnselected"));
	TestNotNull(TEXT("selected rollback node created"), SelectedNode);
	TestNotNull(TEXT("unselected rollback node created"), UnselectedNode);
	if (!SelectedNode || !UnselectedNode)
	{
		return false;
	}

	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_reject_default"));
	const FString TransactionId = FString::Printf(
		TEXT("tx_reject_default_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));

	FBlueprintHelperAppendJournalRecord JournalRecord;
	JournalRecord.TransactionId = TransactionId;
	JournalRecord.ArchiveSessionId = ArchiveSessionId;
	JournalRecord.TaskRunId = TEXT("task_reject_default");
	JournalRecord.Tool = TEXT("AppendBlueprintGraph");
	JournalRecord.Status = TEXT("applied");
	JournalRecord.TargetAssets.Add(Blueprint->GetPathName());
	JournalRecord.GraphId = Graph->GetName();
	JournalRecord.GraphName = Graph->GetName();
	JournalRecord.CreatedNodePaths.Add(SelectedNode->GetName());
	JournalRecord.CreatedNodePaths.Add(UnselectedNode->GetName());
	JournalRecord.RollbackData = TEXT("{\"node_guids\":[\"selected\",\"unselected\"]}");

	FBlueprintHelperTransactionJournalService JournalService;
	FString JournalError;
	TestTrue(TEXT("journal-backed review evidence writes"), JournalService.WriteAppendJournal(JournalRecord, JournalError));

	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewRecordQuery Query;
	Query.ArchiveSessionIdFilter = ArchiveSessionId;
	Query.bPendingOnly = false;
	const TArray<FBlueprintHelperReviewRecord> Records = Store.QueryReviewRecords(Query);
	TestEqual(TEXT("one rollback review record is created"), Records.Num(), 1);
	if (Records.Num() != 1)
	{
		return false;
	}

	FString SelectedTargetKey;
	for (const FBlueprintHelperReviewVisibleChange& Change : Records[0].VisibleChanges)
	{
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (Target.TargetKey.Contains(SelectedNode->GetName()))
			{
				SelectedTargetKey = Target.TargetKey;
			}
		}
	}
	TestFalse(TEXT("selected target key resolved"), SelectedTargetKey.IsEmpty());
	if (SelectedTargetKey.IsEmpty())
	{
		return false;
	}

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.RejectReviewTargets(
		Records[0].ReviewRecordId,
		{ SelectedTargetKey },
		FBlueprintHelperReviewRejectOptions());

	TestTrue(TEXT("selected target rollback succeeds"), Result.bSucceeded);
	TestFalse(TEXT("selected node is removed"), FBlueprintHelperReviewStoreServiceTestsLocalUtils::ReviewGraphContainsNode(Graph, SelectedNode));
	TestTrue(TEXT("unselected node remains"), FBlueprintHelperReviewStoreServiceTestsLocalUtils::ReviewGraphContainsNode(Graph, UnselectedNode));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewConvertOwnerBlockExecutesUserToBhTest,
	"BlueprintHelper.Review.Action.ConvertOwnerBlockExecutesUserToBh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewConvertOwnerBlockExecutesUserToBhTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("ConvertOwnerUserToBh"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0)
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString GraphName = Graph ? Graph->GetName() : FString();
	const FString BlockRef = TEXT("DoorFlowUserOwned");
	const FString BlockId = FBlueprintHelperReviewStoreService::NormalizeGraphBlockTargetId(GraphName, BlockRef);
	UK2Node_CustomEvent* EventNode = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("ReviewConvertUserToBh"));
	TestNotNull(TEXT("test event node created"), EventNode);
	if (!Graph || !EventNode)
	{
		return false;
	}
	TestFalse(TEXT("node starts user-owned"), FBlueprintHelperReviewStoreServiceTestsLocalUtils::IsReviewNodeBlueprintHelperOwned(EventNode));

	FBlueprintHelperReviewAtomicTarget Target = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
		FString::Printf(TEXT("graph:%s:block:%s"), *GraphName, *BlockId),
		FString::Printf(TEXT("graph:%s:block:%s"), *GraphName, *BlockId),
		TEXT("tx_convert_user_to_bh"));
	Target.AssetPath = Blueprint->GetPathName();
	Target.GraphName = GraphName;
	Target.TargetKind = TEXT("graph_block");
	Target.Ownership = TEXT("user_owned");

	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_convert_user_to_bh"));
	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
		ArchiveSessionId,
		TEXT("task_convert_user_to_bh"),
		TEXT("tx_convert_user_to_bh"),
		Blueprint->GetPathName(),
		Target));
	TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	FString SaveError;
	TestTrue(TEXT("record saved before user-to-bh convert owner"), Store.SaveReviewRecords(Records, SaveError));

	FBlueprintHelperReviewConvertOwnerBlockRequest Request;
	Request.ReviewRecordId = Records[0].ReviewRecordId;
	Request.Direction = TEXT("user_to_bh");
	Request.BlockTargetKey = Target.TargetKey;
	Request.EntryAnchor = FString::Printf(TEXT("graph:%s:entry:%s"), *GraphName, *EventNode->GetName());
	Request.NodeAnchors.Add(FString::Printf(TEXT("graph:%s:node:%s"), *GraphName, *EventNode->GetName()));
	Request.DesiredBlockRef = BlockRef;
	Request.ConversionTransactionId = TEXT("tx_review_convert_user_to_bh");
	Request.bSettingProfileAllowsConversion = true;

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.ConvertOwnerBlock(Request);
	TestTrue(TEXT("user-to-bh convert owner action succeeds"), Result.bSucceeded);
	TestTrue(TEXT("node ownership metadata is written"), FBlueprintHelperReviewStoreServiceTestsLocalUtils::IsReviewNodeBlueprintHelperOwned(EventNode));

	FBlueprintHelperReviewRecord LoadedRecord;
	FString LoadError;
	TestTrue(TEXT("converted review record reloads"), Store.LoadReviewRecordById(Records[0].ReviewRecordId, LoadedRecord, LoadError));
	if (LoadedRecord.VisibleChanges.Num() == 0 || LoadedRecord.VisibleChanges[0].AtomicTargets.Num() == 0)
	{
		AddError(TEXT("converted review record has no visible target"));
		return false;
	}

	TestEqual(TEXT("ownership updated in persisted record"),
		LoadedRecord.VisibleChanges[0].AtomicTargets[0].Ownership,
		FString(TEXT("blueprinthelper_owned")));
	TestEqual(TEXT("conversion action recorded"), LoadedRecord.ReviewActions.Num(), 1);
	if (LoadedRecord.ReviewActions.Num() == 1)
	{
		TestEqual(TEXT("conversion transaction linked"),
			LoadedRecord.ReviewActions[0].SourceTransactionId,
			FString(TEXT("tx_review_convert_user_to_bh")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelConstructsTest,
	"BlueprintHelper.Review.UI.PanelConstructsWithSyntheticVisibleChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelConstructsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_visible");
	Change.AssetPath = TEXT("/Game/BP_Door");
	Change.GraphName = TEXT("EventGraph");
	Change.LocationKey = TEXT("function:OpenDoor:input:Input");
	Change.LatestTransactionId = TEXT("tx_visible");
	Change.SourceTransactionIds.Add(TEXT("tx_visible"));
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::SignatureModified;
	Change.DisplayLabel = TEXT("OpenDoor Input");
	Change.BeforeSummary = TEXT("OpenDoor()");
	Change.AfterSummary = TEXT("OpenDoor(Input)");

	TArray<FBlueprintHelperReviewVisibleChange> InitialChanges;
	InitialChanges.Add(Change);

	TSharedRef<SWidget> Widget = SNew(SBlueprintHelperReviewPanel)
		.InitialChanges(InitialChanges);

	TestTrue(TEXT("review panel widget is constructed"), Widget != SNullWidget::NullWidget);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAssetContextLoadsBlueprintFromPackagePathTest,
	"BlueprintHelper.Review.UI.AssetContextLoadsBlueprintFromPackagePath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAssetContextLoadsBlueprintFromPackagePathTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewNamedBlueprint(
		TEXT("ReviewAssetContextBlueprintPackagePath"));
	TestNotNull(TEXT("Blueprint fixture exists"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	const FString PackagePath = Blueprint->GetOutermost()->GetName();
	const FBlueprintHelperReviewAssetContext Context =
		FBlueprintHelperReviewAssetContext::LoadForAssetPath(PackagePath);
	TestTrue(TEXT("Blueprint package-path context is valid"), Context.IsValid());
	TestEqual(TEXT("Blueprint package-path context kind"),
		static_cast<int32>(Context.AssetKind),
		static_cast<int32>(EBlueprintHelperReviewAssetKind::Blueprint));
	TestTrue(TEXT("Blueprint package-path context resolves fixture"),
		Context.Blueprint.Get() == Blueprint);
	TestEqual(TEXT("Blueprint package-path object path resolves asset object"),
		Context.ObjectPath,
		Blueprint->GetPathName());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAssetContextLoadsDataTableTest,
	"BlueprintHelper.Review.UI.AssetContextLoadsDataTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAssetContextLoadsDataTableTest::RunTest(const FString& Parameters)
{
	UDataTable* DataTable = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewDataTable(TEXT("ReviewAssetContextDataTable"));
	TestNotNull(TEXT("DataTable fixture exists"), DataTable);
	if (!DataTable)
	{
		return false;
	}

	const FBlueprintHelperReviewAssetContext Context =
		FBlueprintHelperReviewAssetContext::LoadForAssetPath(DataTable->GetPathName());
	TestTrue(TEXT("DataTable context is valid"), Context.IsValid());
	TestEqual(TEXT("DataTable context kind"),
		static_cast<int32>(Context.AssetKind),
		static_cast<int32>(EBlueprintHelperReviewAssetKind::DataTable));
	TestTrue(TEXT("DataTable context object matches fixture"),
		Context.DataTable.Get() == DataTable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAssetContextLoadsGenericObjectTest,
	"BlueprintHelper.Review.UI.AssetContextLoadsGenericObject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAssetContextLoadsGenericObjectTest::RunTest(const FString& Parameters)
{
	UObject* Object = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewGenericObject(TEXT("ReviewAssetContextGenericObject"));
	TestNotNull(TEXT("generic UObject fixture exists"), Object);
	if (!Object)
	{
		return false;
	}

	const FBlueprintHelperReviewAssetContext Context =
		FBlueprintHelperReviewAssetContext::LoadForAssetPath(Object->GetPathName());
	TestTrue(TEXT("generic object context is valid"), Context.IsValid());
	TestEqual(TEXT("generic object context kind"),
		static_cast<int32>(Context.AssetKind),
		static_cast<int32>(EBlueprintHelperReviewAssetKind::GenericObject));
	TestTrue(TEXT("generic object context object matches fixture"),
		Context.AssetObject.Get() == Object);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAssetContextLoadsDataAssetTest,
	"BlueprintHelper.Review.UI.AssetContextLoadsDataAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAssetContextLoadsDataAssetTest::RunTest(const FString& Parameters)
{
	UDataAsset* DataAsset = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewDataAsset(TEXT("ReviewAssetContextDataAsset"));
	TestNotNull(TEXT("DataAsset fixture exists"), DataAsset);
	if (!DataAsset)
	{
		return false;
	}

	const FBlueprintHelperReviewAssetContext Context =
		FBlueprintHelperReviewAssetContext::LoadForAssetPath(DataAsset->GetPathName());
	TestTrue(TEXT("DataAsset context is valid"), Context.IsValid());
	TestEqual(TEXT("DataAsset context kind"),
		static_cast<int32>(Context.AssetKind),
		static_cast<int32>(EBlueprintHelperReviewAssetKind::DataAsset));
	TestTrue(TEXT("DataAsset context object matches fixture"),
		Context.AssetObject.Get() == DataAsset);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAssetContextLoadsWidgetBlueprintTest,
	"BlueprintHelper.Review.UI.AssetContextLoadsWidgetBlueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAssetContextLoadsWidgetBlueprintTest::RunTest(const FString& Parameters)
{
	UWidgetBlueprint* WidgetBlueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewWidgetBlueprint(TEXT("ReviewAssetContextWidgetBlueprint"));
	TestNotNull(TEXT("WidgetBlueprint fixture exists"), WidgetBlueprint);
	if (!WidgetBlueprint)
	{
		return false;
	}

	const FBlueprintHelperReviewAssetContext Context =
		FBlueprintHelperReviewAssetContext::LoadForAssetPath(WidgetBlueprint->GetPathName());
	TestTrue(TEXT("WidgetBlueprint context is valid"), Context.IsValid());
	TestEqual(TEXT("WidgetBlueprint context kind"),
		static_cast<int32>(Context.AssetKind),
		static_cast<int32>(EBlueprintHelperReviewAssetKind::WidgetBlueprint));
	TestTrue(TEXT("WidgetBlueprint context object matches fixture"),
		Context.Blueprint.Get() == WidgetBlueprint);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewUMGWidgetPresenterBuildsReadonlyTreeTest,
	"BlueprintHelper.Review.UI.UMGWidgetPresenterBuildsReadonlyTree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewUMGWidgetPresenterBuildsReadonlyTreeTest::RunTest(const FString& Parameters)
{
	UWidgetBlueprint* WidgetBlueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewWidgetBlueprint(TEXT("ReviewUMGPresenterContent"));
	TestNotNull(TEXT("WidgetBlueprint fixture exists"), WidgetBlueprint);
	if (!WidgetBlueprint)
	{
		return false;
	}

	const FBlueprintHelperReviewAssetContext Context =
		FBlueprintHelperReviewAssetContext::LoadForAssetPath(WidgetBlueprint->GetPathName());
	FBlueprintHelperReviewWidgetTreePresenterState WidgetTreeState;
	TSharedRef<SWidget> Widget = FBlueprintHelperReviewUMGWidgetTreePresenter::BuildContent(Context, WidgetTreeState);
	TestTrue(TEXT("UMG presenter content is constructed"), Widget != SNullWidget::NullWidget);
	TestTrue(TEXT("UMG presenter owns readonly row data"), WidgetTreeState.RootItems.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewDataTablePresenterBuildsReadonlyRowsTest,
	"BlueprintHelper.Review.UI.DataTablePresenterBuildsReadonlyRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewDataTablePresenterBuildsReadonlyRowsTest::RunTest(const FString& Parameters)
{
	UDataTable* DataTable = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewDataTable(TEXT("ReviewDataTablePresenterContent"));
	TestNotNull(TEXT("DataTable fixture exists"), DataTable);
	if (!DataTable)
	{
		return false;
	}

	const FBlueprintHelperReviewAssetContext Context =
		FBlueprintHelperReviewAssetContext::LoadForAssetPath(DataTable->GetPathName());
	TSharedRef<SWidget> Widget = FBlueprintHelperReviewDataTablePresenter::BuildContent(Context);
	TestTrue(TEXT("DataTable presenter content is constructed"), Widget != SNullWidget::NullWidget);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewDataAssetPresenterBuildsReadonlyDetailsTest,
	"BlueprintHelper.Review.UI.DataAssetPresenterBuildsReadonlyDetails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewDataAssetPresenterBuildsReadonlyDetailsTest::RunTest(const FString& Parameters)
{
	UDataAsset* DataAsset = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewDataAsset(TEXT("ReviewDataAssetPresenterContent"));
	TestNotNull(TEXT("DataAsset fixture exists"), DataAsset);
	if (!DataAsset)
	{
		return false;
	}

	const FBlueprintHelperReviewAssetContext Context =
		FBlueprintHelperReviewAssetContext::LoadForAssetPath(DataAsset->GetPathName());
	TSharedRef<SWidget> Widget = FBlueprintHelperReviewDataAssetPresenter::BuildContent(Context);
	TestTrue(TEXT("DataAsset presenter content is constructed"), Widget != SNullWidget::NullWidget);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelEmptyConstructsTest,
	"BlueprintHelper.Review.UI.PanelConstructsWithEmptyVisibleChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelEmptyConstructsTest::RunTest(const FString& Parameters)
{
	TArray<FBlueprintHelperReviewVisibleChange> InitialChanges;
	TSharedRef<SWidget> Widget = SNew(SBlueprintHelperReviewPanel)
		.InitialChanges(InitialChanges);

	TestTrue(TEXT("empty review panel widget is constructed"), Widget != SNullWidget::NullWidget);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelObjectBlueprintConstructsTest,
	"BlueprintHelper.Review.UI.PanelConstructsWithObjectBlueprintVisibleChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelObjectBlueprintConstructsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewObjectBlueprint(TEXT("ReviewPanelObjectBlueprint"));
	TestNotNull(TEXT("object Blueprint exists"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_object_visible");
	Change.AssetPath = Blueprint->GetPathName();
	Change.GraphName = TEXT("EventGraph");
	Change.LocationKey = TEXT("object:class_settings");
	Change.LatestTransactionId = TEXT("tx_object_visible");
	Change.SourceTransactionIds.Add(TEXT("tx_object_visible"));
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	Change.DisplayLabel = TEXT("Object Blueprint Review");
	Change.BeforeSummary = TEXT("Before");
	Change.AfterSummary = TEXT("After");

	TArray<FBlueprintHelperReviewVisibleChange> InitialChanges;
	InitialChanges.Add(Change);

	TSharedRef<SWidget> Widget = SNew(SBlueprintHelperReviewPanel)
		.InitialChanges(InitialChanges);

	TestTrue(TEXT("review panel widget is constructed for non-Actor Blueprint"), Widget != SNullWidget::NullWidget);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelDataTableConstructsTest,
	"BlueprintHelper.Review.UI.ReviewPanelConstructsWithDataTableVisibleChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelDataTableConstructsTest::RunTest(const FString& Parameters)
{
	UDataTable* DataTable = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewDataTable(TEXT("ReviewPanelDataTable"));
	TestNotNull(TEXT("DataTable fixture exists"), DataTable);
	if (!DataTable)
	{
		return false;
	}

	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_datatable_visible");
	Change.AssetPath = DataTable->GetPathName();
	Change.LocationKey = TEXT("asset_factory:data_table");
	Change.LatestTransactionId = TEXT("tx_datatable_visible");
	Change.SourceTransactionIds.Add(TEXT("tx_datatable_visible"));
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
	Change.DisplayLabel = TEXT("DataTable Review");
	Change.BeforeSummary = TEXT("Missing");
	Change.AfterSummary = TEXT("Created DataTable");

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Change.AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::DataTable;
	Target.TargetKind = TEXT("asset_factory");
	Target.TargetKey = TEXT("asset_factory:data_table");
	Target.DisplayLabel = TEXT("DataTable Review");
	Change.AtomicTargets.Add(Target);

	TArray<FBlueprintHelperReviewVisibleChange> InitialChanges;
	InitialChanges.Add(Change);

	TSharedRef<SWidget> Widget = SNew(SBlueprintHelperReviewPanel)
		.InitialChanges(InitialChanges);

	TestTrue(TEXT("review panel widget is constructed for DataTable"), Widget != SNullWidget::NullWidget);
	TestFalse(TEXT("DataTable details target does not route to Graph"),
		FBlueprintHelperReviewGraphPresenter::ShouldShowChange(Change));
	TestTrue(TEXT("DataTable target routes to independent DataTable presenter"),
		FBlueprintHelperReviewDataTablePresenter::ShouldShowChange(Change));
	TestFalse(TEXT("DataTable target does not route to Details"),
		FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange(Change));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelDataTableRowConstructsTest,
	"BlueprintHelper.Review.UI.ReviewPanelConstructsWithDataTableRowVisibleChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelDataTableRowConstructsTest::RunTest(const FString& Parameters)
{
	UDataTable* DataTable = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewDataTable(TEXT("ReviewPanelDataTableRow"));
	TestNotNull(TEXT("DataTable fixture exists"), DataTable);
	if (!DataTable)
	{
		return false;
	}

	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_datatable_row_visible");
	Change.AssetPath = DataTable->GetPathName();
	Change.LocationKey = TEXT("datatable_row:DamageSmall");
	Change.LatestTransactionId = TEXT("tx_datatable_row_visible");
	Change.SourceTransactionIds.Add(TEXT("tx_datatable_row_visible"));
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	Change.DisplayLabel = TEXT("DamageSmall Row");
	Change.BeforeSummary = TEXT("DamageSmall before");
	Change.AfterSummary = TEXT("DamageSmall after");

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Change.AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::DataTable;
	Target.TargetKind = TEXT("datatable_row");
	Target.TargetKey = TEXT("datatable_row:DamageSmall");
	Target.DisplayLabel = TEXT("DamageSmall Row");
	Change.AtomicTargets.Add(Target);

	TArray<FBlueprintHelperReviewVisibleChange> InitialChanges;
	InitialChanges.Add(Change);

	TSharedRef<SWidget> Widget = SNew(SBlueprintHelperReviewPanel)
		.InitialChanges(InitialChanges);

	TestTrue(TEXT("review panel widget is constructed for DataTable row"), Widget != SNullWidget::NullWidget);
	TestTrue(TEXT("DataTable row target routes to DataTable presenter"),
		FBlueprintHelperReviewDataTablePresenter::ShouldShowChange(Change));
	TestFalse(TEXT("DataTable row target does not route to Graph"),
		FBlueprintHelperReviewGraphPresenter::ShouldShowChange(Change));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelGenericObjectConstructsTest,
	"BlueprintHelper.Review.UI.ReviewPanelConstructsWithGenericObjectVisibleChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelGenericObjectConstructsTest::RunTest(const FString& Parameters)
{
	UObject* Object = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewGenericObject(TEXT("ReviewPanelGenericObject"));
	TestNotNull(TEXT("generic UObject fixture exists"), Object);
	if (!Object)
	{
		return false;
	}

	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_generic_object_visible");
	Change.AssetPath = Object->GetPathName();
	Change.LocationKey = TEXT("object_property:DisplayName");
	Change.LatestTransactionId = TEXT("tx_generic_object_visible");
	Change.SourceTransactionIds.Add(TEXT("tx_generic_object_visible"));
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	Change.DisplayLabel = TEXT("Generic Object Review");
	Change.BeforeSummary = TEXT("Before");
	Change.AfterSummary = TEXT("After");

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Change.AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::DataAsset;
	Target.TargetKind = TEXT("object_property");
	Target.TargetKey = TEXT("object_property:DisplayName");
	Target.PropertyPath = TEXT("DisplayName");
	Target.DisplayLabel = TEXT("DisplayName");
	Change.AtomicTargets.Add(Target);

	TArray<FBlueprintHelperReviewVisibleChange> InitialChanges;
	InitialChanges.Add(Change);

	TSharedRef<SWidget> Widget = SNew(SBlueprintHelperReviewPanel)
		.InitialChanges(InitialChanges);

	TestTrue(TEXT("review panel widget is constructed for generic UObject"), Widget != SNullWidget::NullWidget);
	TestFalse(TEXT("generic object details target does not route to Graph"),
		FBlueprintHelperReviewGraphPresenter::ShouldShowChange(Change));
	TestTrue(TEXT("generic object target routes to DataAsset presenter"),
		FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange(Change));
	TestFalse(TEXT("generic object target does not route to Details"),
		FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange(Change));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelDataAssetPropertyConstructsTest,
	"BlueprintHelper.Review.UI.ReviewPanelConstructsWithDataAssetPropertyVisibleChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelDataAssetPropertyConstructsTest::RunTest(const FString& Parameters)
{
	UDataAsset* DataAsset = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewDataAsset(TEXT("ReviewPanelDataAssetProperty"));
	TestNotNull(TEXT("DataAsset fixture exists"), DataAsset);
	if (!DataAsset)
	{
		return false;
	}

	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_data_asset_property_visible");
	Change.AssetPath = DataAsset->GetPathName();
	Change.LocationKey = TEXT("data_asset_property:Config.Health");
	Change.LatestTransactionId = TEXT("tx_data_asset_property_visible");
	Change.SourceTransactionIds.Add(TEXT("tx_data_asset_property_visible"));
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::VariableModified;
	Change.DisplayLabel = TEXT("Config.Health");
	Change.BeforeSummary = TEXT("Health before");
	Change.AfterSummary = TEXT("Health after");

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Change.AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::DataAsset;
	Target.TargetKind = TEXT("data_asset_property");
	Target.TargetKey = TEXT("data_asset_property:Config.Health");
	Target.PropertyPath = TEXT("Config.Health");
	Target.DisplayLabel = TEXT("Config.Health");
	Change.AtomicTargets.Add(Target);

	TArray<FBlueprintHelperReviewVisibleChange> InitialChanges;
	InitialChanges.Add(Change);

	TSharedRef<SWidget> Widget = SNew(SBlueprintHelperReviewPanel)
		.InitialChanges(InitialChanges);

	TestTrue(TEXT("review panel widget is constructed for DataAsset property"), Widget != SNullWidget::NullWidget);
	TestTrue(TEXT("DataAsset property target routes to DataAsset presenter"),
		FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange(Change));
	TestFalse(TEXT("DataAsset property target does not route to Graph"),
		FBlueprintHelperReviewGraphPresenter::ShouldShowChange(Change));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelWidgetBlueprintConstructsTest,
	"BlueprintHelper.Review.UI.ReviewPanelConstructsWithWidgetBlueprintVisibleChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelWidgetBlueprintConstructsTest::RunTest(const FString& Parameters)
{
	UWidgetBlueprint* WidgetBlueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewWidgetBlueprint(TEXT("ReviewPanelWidgetBlueprint"));
	TestNotNull(TEXT("WidgetBlueprint fixture exists"), WidgetBlueprint);
	if (!WidgetBlueprint)
	{
		return false;
	}

	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_widget_blueprint_visible");
	Change.AssetPath = WidgetBlueprint->GetPathName();
	Change.LocationKey = TEXT("umg_widget:SmokeText");
	Change.LatestTransactionId = TEXT("tx_widget_blueprint_visible");
	Change.SourceTransactionIds.Add(TEXT("tx_widget_blueprint_visible"));
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
	Change.DisplayLabel = TEXT("SmokeText Widget");
	Change.BeforeSummary = TEXT("Missing");
	Change.AfterSummary = TEXT("TextBlock SmokeText");

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Change.AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::UMGWidgetTree;
	Target.TargetKind = TEXT("umg_widget");
	Target.TargetKey = TEXT("umg_widget:SmokeText");
	Target.DisplayLabel = TEXT("SmokeText Widget");
	Change.AtomicTargets.Add(Target);

	TArray<FBlueprintHelperReviewVisibleChange> InitialChanges;
	InitialChanges.Add(Change);

	TSharedRef<SWidget> Widget = SNew(SBlueprintHelperReviewPanel)
		.InitialChanges(InitialChanges);

	TestTrue(TEXT("review panel widget is constructed for WidgetBlueprint"), Widget != SNullWidget::NullWidget);
	TestTrue(TEXT("WidgetBlueprint UMG target routes to UMG presenter"),
		FBlueprintHelperReviewUMGWidgetTreePresenter::ShouldShowChange(Change));
	TestFalse(TEXT("WidgetBlueprint UMG target does not route to Graph"),
		FBlueprintHelperReviewGraphPresenter::ShouldShowChange(Change));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelMyBlueprintOnlySignatureDoesNotGraphRouteTest,
	"BlueprintHelper.Review.UI.ReviewPanelDoesNotGraphRouteMyBlueprintOnlySignatureChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelMyBlueprintOnlySignatureDoesNotGraphRouteTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("ReviewPanelSignatureOnly"));
	TestNotNull(TEXT("Blueprint fixture exists"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_signature_only_panel");
	Change.AssetPath = Blueprint->GetPathName();
	Change.GraphName = TEXT("EventGraph");
	Change.LocationKey = TEXT("function:ApplyDamage:signature");
	Change.LatestTransactionId = TEXT("tx_signature_only_panel");
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::SignatureModified;
	Change.DisplayLabel = TEXT("ApplyDamage Signature");

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Change.AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
	Target.GraphName = TEXT("EventGraph");
	Target.TargetKind = TEXT("signature");
	Target.TargetKey = TEXT("signature:ApplyDamage");
	Target.DisplayLabel = TEXT("ApplyDamage Signature");
	Change.AtomicTargets.Add(Target);

	TArray<FBlueprintHelperReviewVisibleChange> InitialChanges;
	InitialChanges.Add(Change);

	TSharedRef<SWidget> Widget = SNew(SBlueprintHelperReviewPanel)
		.InitialChanges(InitialChanges);

	TestTrue(TEXT("review panel widget is constructed for MyBlueprint-only signature"), Widget != SNullWidget::NullWidget);
	TestFalse(TEXT("MyBlueprint-only signature does not route to Graph"),
		FBlueprintHelperReviewGraphPresenter::ShouldShowChange(Change));
	TestTrue(TEXT("MyBlueprint-only signature routes to MyBlueprint"),
		FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange(Change));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelKeepsTrueGraphVisibleChangeRoutableTest,
	"BlueprintHelper.Review.UI.ReviewPanelKeepsTrueGraphVisibleChangeRoutable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelKeepsTrueGraphVisibleChangeRoutableTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("ReviewPanelTrueGraph"));
	TestNotNull(TEXT("Blueprint fixture exists"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_1778317276165");
	Change.AssetPath = Blueprint->GetPathName();
	Change.GraphName = TEXT("EventGraph");
	Change.LocationKey = TEXT("graph:EventGraph/node:PrintString");
	Change.LatestTransactionId = TEXT("tx_1778317276165");
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	Change.DisplayLabel = TEXT("Print String Node");

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Change.AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::Graph;
	Target.GraphName = TEXT("EventGraph");
	Target.TargetKind = TEXT("graph_node");
	Target.TargetKey = TEXT("graph:EventGraph/node:PrintString");
	Target.DisplayLabel = TEXT("Print String Node");
	Change.AtomicTargets.Add(Target);

	TArray<FBlueprintHelperReviewVisibleChange> InitialChanges;
	InitialChanges.Add(Change);

	TSharedRef<SWidget> Widget = SNew(SBlueprintHelperReviewPanel)
		.InitialChanges(InitialChanges);

	TestTrue(TEXT("review panel widget is constructed for true Graph change"), Widget != SNullWidget::NullWidget);
	TestTrue(TEXT("true Graph change routes to Graph"),
		FBlueprintHelperReviewGraphPresenter::ShouldShowChange(Change));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewGraphBoundsTargetKeyTest,
	"BlueprintHelper.Review.UI.GraphBounds.UsesTargetKeyCommentStyleBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewGraphBoundsTargetKeyTest::RunTest(const FString& Parameters)
{
	UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage());
	UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph, FName(TEXT("K2Node_CallFunction_1")));
	Node->NodePosX = 100;
	Node->NodePosY = 40;
	Node->NodeWidth = 240;
	Node->NodeHeight = 88;
	Graph->AddNode(Node, false, false);

	FBlueprintHelperReviewAtomicTarget Target;
	Target.Surface = EBlueprintHelperReviewSurface::Graph;
	Target.GraphName = TEXT("EventGraph");
	Target.TargetKey = TEXT("graph:EventGraph/node:K2Node_CallFunction_1");

	TArray<FBlueprintHelperReviewAtomicTarget> Targets;
	Targets.Add(Target);

	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	FString DebugSummary;
	const bool bBuilt = FBlueprintHelperReviewGraphBounds::BuildBoundsForTargets(
		Targets,
		Graph,
		TEXT("EventGraph"),
		nullptr,
		Position,
		Size,
		&DebugSummary);

	TestTrue(TEXT("target key matches graph node"), bBuilt);
	TestTrue(TEXT("comment-style bounds use 20px left padding"),
		FMath::IsNearlyEqual(static_cast<float>(Position.X), 80.0f, 0.01f));
	TestTrue(TEXT("comment-style bounds use 20px top padding"),
		FMath::IsNearlyEqual(static_cast<float>(Position.Y), 20.0f, 0.01f));
	TestTrue(TEXT("comment-style width wraps node plus padding"),
		FMath::IsNearlyEqual(static_cast<float>(Size.X), 280.0f, 0.01f));
	TestTrue(TEXT("comment-style height wraps node plus padding"),
		FMath::IsNearlyEqual(static_cast<float>(Size.Y), 128.0f, 0.01f));
	TestTrue(TEXT("bounds debug reports built bounds"),
		DebugSummary.Contains(TEXT("built=1")));
	TestTrue(TEXT("bounds debug reports fallback node bounds"),
		DebugSummary.Contains(TEXT("fallbackBounds=1")));
	TestTrue(TEXT("bounds debug reports 20px padding"),
		DebugSummary.Contains(TEXT("padding=20.0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewGraphBoundsRequireRealAnchorTest,
	"BlueprintHelper.Review.UI.GraphBounds.RequireNodeOrRecordedBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewGraphBoundsRequireRealAnchorTest::RunTest(const FString& Parameters)
{
	UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage());

	FBlueprintHelperReviewAtomicTarget MissingAnchorTarget;
	MissingAnchorTarget.Surface = EBlueprintHelperReviewSurface::Graph;
	MissingAnchorTarget.GraphName = TEXT("EventGraph");
	MissingAnchorTarget.TargetKind = TEXT("graph_node");
	MissingAnchorTarget.TargetKey = TEXT("graph:EventGraph/node:MissingNode");

	TArray<FBlueprintHelperReviewAtomicTarget> MissingAnchorTargets;
	MissingAnchorTargets.Add(MissingAnchorTarget);

	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	FString MissingDebugSummary;
	TestFalse(TEXT("graph bounds do not create fake geometry without node or recorded bounds"),
		FBlueprintHelperReviewGraphBounds::BuildBoundsForTargets(
			MissingAnchorTargets,
			Graph,
			TEXT("EventGraph"),
			nullptr,
			Position,
			Size,
			&MissingDebugSummary));
	TestTrue(TEXT("missing real anchor debug reports no built bounds"),
		MissingDebugSummary.Contains(TEXT("built=0")));

	FBlueprintHelperReviewAtomicTarget RecordedTarget = MissingAnchorTarget;
	RecordedTarget.bHasGraphBounds = true;
	RecordedTarget.GraphPosition = FVector2D(120.0f, 80.0f);
	RecordedTarget.GraphSize = FVector2D(300.0f, 140.0f);

	TArray<FBlueprintHelperReviewAtomicTarget> RecordedTargets;
	RecordedTargets.Add(RecordedTarget);

	FString RecordedDebugSummary;
	TestTrue(TEXT("recorded graph bounds are accepted as real anchor geometry"),
		FBlueprintHelperReviewGraphBounds::BuildBoundsForTargets(
			RecordedTargets,
			Graph,
			TEXT("EventGraph"),
			nullptr,
			Position,
			Size,
			&RecordedDebugSummary));
	TestTrue(TEXT("recorded graph bounds debug reports record source"),
		RecordedDebugSummary.Contains(TEXT("recordBounds=1")));
	TestTrue(TEXT("recorded graph bounds keep padding"),
		FMath::IsNearlyEqual(static_cast<float>(Position.X), 100.0f, 0.01f));
	TestTrue(TEXT("recorded graph bounds keep padded size"),
		FMath::IsNearlyEqual(static_cast<float>(Size.X), 340.0f, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewDebugCopyTextTest,
	"BlueprintHelper.Review.UI.Debug.BuildsCopyableText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewDebugCopyTextTest::RunTest(const FString& Parameters)
{
	TArray<FString> Messages;
	Messages.Add(TEXT("[01:20:51] newest message"));
	Messages.Add(TEXT("[01:20:50] older message"));

	const FString Expected = FString::Printf(
		TEXT("[01:20:51] newest message%s[01:20:50] older message"),
		LINE_TERMINATOR);
	TestEqual(
		TEXT("copyable debug text preserves visible row order with line breaks"),
		FBlueprintHelperReviewDebugText::BuildCopyableText(Messages),
		Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelDoesNotUseDeferredGeometryRefreshTimerTest,
	"BlueprintHelper.Review.UI.ReviewPanelDoesNotUseDeferredGeometryRefreshTimer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelDoesNotUseDeferredGeometryRefreshTimerTest::RunTest(const FString& Parameters)
{
	bool bInvalidated = false;
	FBlueprintHelperReviewPanelSurfacePresenterArgs Args;
	Args.OnGeometryInvalidated = FBlueprintHelperReviewGeometryInvalidated::CreateLambda(
		[&bInvalidated](EBlueprintHelperReviewSurface Surface)
		{
			bInvalidated = Surface == EBlueprintHelperReviewSurface::MyBlueprint;
		});

	TestTrue(TEXT("surface presenters expose geometry invalidation delegate"), Args.OnGeometryInvalidated.IsBound());
	Args.OnGeometryInvalidated.Execute(EBlueprintHelperReviewSurface::MyBlueprint);
	TestTrue(TEXT("geometry invalidation delegate routes a specific surface"), bInvalidated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewMyBlueprintProbeRefreshesOverlayAfterRowGeometryReadyTest,
	"BlueprintHelper.Review.UI.MyBlueprintProbeRefreshesOverlayAfterRowGeometryReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewMyBlueprintProbeRefreshesOverlayAfterRowGeometryReadyTest::RunTest(const FString& Parameters)
{
	TSharedRef<SWidget> Probe = SNew(SBlueprintHelperReviewGeometryProbe)
		.Surface(EBlueprintHelperReviewSurface::MyBlueprint)
		.TargetKey(TEXT("blueprint_variable:SmokeHP"))
		.OnGeometryInvalidated(FBlueprintHelperReviewGeometryInvalidated::CreateLambda(
			[](EBlueprintHelperReviewSurface)
			{
			}))
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("SmokeHP")))
		];

	TestTrue(TEXT("geometry probe widget constructs"), Probe != SNullWidget::NullWidget);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewGraphResolverMissingRequestedGraphTest,
	"BlueprintHelper.Review.UI.GraphResolver.DoesNotFallbackWhenRequestedGraphIsMissing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewGraphResolverMissingRequestedGraphTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = NewObject<UBlueprint>(GetTransientPackage());
	UEdGraph* EventGraph = NewObject<UEdGraph>(Blueprint, FName(TEXT("EventGraph")));
	Blueprint->UbergraphPages.Add(EventGraph);

	TestTrue(
		TEXT("explicit missing graph does not fall back to EventGraph"),
		FBlueprintHelperReviewGraphResolver::ResolveGraphForReviewSelection(
			Blueprint,
			TEXT("BH_TaskSpecSmoke_20260505_001")) == nullptr);
	TestTrue(
		TEXT("empty requested graph still falls back to EventGraph"),
		FBlueprintHelperReviewGraphResolver::ResolveGraphForReviewSelection(Blueprint, FString()) == EventGraph);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewGraphBoundsBlockMetadataTest,
	"BlueprintHelper.Review.UI.GraphBounds.UsesBlockMetadataBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewGraphBoundsBlockMetadataTest::RunTest(const FString& Parameters)
{
	UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage());
	UEdGraphNode* FirstNode = NewObject<UEdGraphNode>(Graph, FName(TEXT("K2Node_CustomEvent_1")));
	FirstNode->NodePosX = 100;
	FirstNode->NodePosY = 40;
	FirstNode->NodeWidth = 240;
	FirstNode->NodeHeight = 88;
	Graph->AddNode(FirstNode, false, false);

	UEdGraphNode* SecondNode = NewObject<UEdGraphNode>(Graph, FName(TEXT("K2Node_CallFunction_1")));
	SecondNode->NodePosX = 500;
	SecondNode->NodePosY = 120;
	SecondNode->NodeWidth = 260;
	SecondNode->NodeHeight = 96;
	Graph->AddNode(SecondNode, false, false);

	FMetaData& MetaData = GetTransientPackage()->GetMetaData();
	MetaData.SetValue(FirstNode, TEXT("BlueprintHelperOwned"), TEXT("true"));
	MetaData.SetValue(FirstNode, TEXT("BlueprintHelperBlockId"), TEXT("SmokeBlock"));
	MetaData.SetValue(SecondNode, TEXT("BlueprintHelperOwned"), TEXT("true"));
	MetaData.SetValue(SecondNode, TEXT("BlueprintHelperBlockId"), TEXT("SmokeBlock"));

	FBlueprintHelperReviewAtomicTarget Target;
	Target.Surface = EBlueprintHelperReviewSurface::Graph;
	Target.GraphName = TEXT("EventGraph");
	Target.TargetKey = TEXT("graph:EventGraph:block:SmokeBlock");

	TArray<FBlueprintHelperReviewAtomicTarget> Targets;
	Targets.Add(Target);

	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	FString DebugSummary;
	const bool bBuilt = FBlueprintHelperReviewGraphBounds::BuildBoundsForTargets(
		Targets,
		Graph,
		TEXT("EventGraph"),
		nullptr,
		Position,
		Size,
		&DebugSummary);

	TestTrue(TEXT("block id metadata matches graph nodes"), bBuilt);
	TestTrue(TEXT("block metadata bounds include first node left padding"),
		FMath::IsNearlyEqual(static_cast<float>(Position.X), 80.0f, 0.01f));
	TestTrue(TEXT("block metadata bounds include first node top padding"),
		FMath::IsNearlyEqual(static_cast<float>(Position.Y), 20.0f, 0.01f));
	TestTrue(TEXT("block metadata width spans both nodes"),
		FMath::IsNearlyEqual(static_cast<float>(Size.X), 700.0f, 0.01f));
	TestTrue(TEXT("block metadata height spans both nodes"),
		FMath::IsNearlyEqual(static_cast<float>(Size.Y), 216.0f, 0.01f));
	TestTrue(TEXT("debug reports both matched nodes"),
		DebugSummary.Contains(TEXT("matchedNodes=2")));
	TestTrue(TEXT("debug reports metadata block id match"),
		DebugSummary.Contains(TEXT("K2Node_CustomEvent_1")));
	TestTrue(TEXT("debug reports second metadata block id match"),
		DebugSummary.Contains(TEXT("K2Node_CallFunction_1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewGraphBlockTargetNormalizationTest,
	"BlueprintHelper.Review.Store.NormalizesGraphBlockTargetId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewGraphBlockTargetNormalizationTest::RunTest(const FString& Parameters)
{
	const FString GraphName = TEXT("BH_TaskSpecSmoke_20260504_001");
	const FString BlockRef = TEXT("BH_TaskSpecSmokeEvent_20260504_0010");
	const FString FullBlockId = TEXT("BH_TaskSpecSmoke_20260504_001_BH_TaskSpecSmokeEvent_20260504_0010");

	TestEqual(
		TEXT("short block ref is normalized to graph-prefixed block id"),
		FBlueprintHelperReviewStoreService::NormalizeGraphBlockTargetId(GraphName, BlockRef),
		FullBlockId);
	TestEqual(
		TEXT("full block id is preserved"),
		FBlueprintHelperReviewStoreService::NormalizeGraphBlockTargetId(GraphName, FullBlockId),
		FullBlockId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewGraphBoundsFullBlockMetadataTest,
	"BlueprintHelper.Review.UI.GraphBounds.UsesFullBlockMetadataBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewGraphBoundsFullBlockMetadataTest::RunTest(const FString& Parameters)
{
	UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage());
	UEdGraphNode* FirstNode = NewObject<UEdGraphNode>(Graph, FName(TEXT("K2Node_CustomEvent_0")));
	FirstNode->NodePosX = 304;
	FirstNode->NodePosY = -192;
	Graph->AddNode(FirstNode, false, false);

	UEdGraphNode* SecondNode = NewObject<UEdGraphNode>(Graph, FName(TEXT("K2Node_CallFunction_1")));
	SecondNode->NodePosX = 784;
	SecondNode->NodePosY = -176;
	Graph->AddNode(SecondNode, false, false);

	UEdGraphNode* ThirdNode = NewObject<UEdGraphNode>(Graph, FName(TEXT("K2Node_CallFunction_2")));
	ThirdNode->NodePosX = 1392;
	ThirdNode->NodePosY = -176;
	Graph->AddNode(ThirdNode, false, false);

	FMetaData& MetaData = GetTransientPackage()->GetMetaData();
	const FString CurrentBlockId = TEXT("BH_TaskSpecSmoke_20260504_001_BH_TaskSpecSmokeEvent_20260504_0010");
	MetaData.SetValue(FirstNode, TEXT("BlueprintHelperBlockId"), *CurrentBlockId);
	MetaData.SetValue(SecondNode, TEXT("BlueprintHelperBlockId"), *CurrentBlockId);
	MetaData.SetValue(ThirdNode, TEXT("BlueprintHelperBlockId"), *CurrentBlockId);

	FBlueprintHelperReviewAtomicTarget Target;
	Target.Surface = EBlueprintHelperReviewSurface::Graph;
	Target.GraphName = TEXT("BH_TaskSpecSmoke_20260504_001");
	Target.TargetKey = TEXT("graph:BH_TaskSpecSmoke_20260504_001:block:BH_TaskSpecSmoke_20260504_001_BH_TaskSpecSmokeEvent_20260504_0010");

	TArray<FBlueprintHelperReviewAtomicTarget> Targets;
	Targets.Add(Target);

	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	FString DebugSummary;
	const bool bBuilt = FBlueprintHelperReviewGraphBounds::BuildBoundsForTargets(
		Targets,
		Graph,
		TEXT("BH_TaskSpecSmoke_20260504_001"),
		nullptr,
		Position,
		Size,
		&DebugSummary);

	TestTrue(TEXT("full block id matches graph-prefixed block metadata"), bBuilt);
	TestTrue(TEXT("full block metadata matches all nodes"),
		DebugSummary.Contains(TEXT("matchedNodes=3")));
	TestTrue(TEXT("full block metadata wraps full block width"),
		Size.X > 1200.0f);
	return true;
}

#endif
