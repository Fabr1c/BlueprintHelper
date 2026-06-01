#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/SceneComponent.h"
#include "Components/TextBlock.h"
#include "Curves/CurveFloat.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/PrimaryAssetLabel.h"
#include "HAL/PlatformTime.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/StructureEditorUtils.h"
#include "HAL/FileManager.h"
#include "ObjectTools.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/Review/Utils/BlueprintHelperReviewActionTargetUtils.h"
#include "Systems/Review/Utils/BlueprintHelperReviewRejectService.h"
#include "Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.h"
#include "Systems/Review/Utils/BlueprintHelperReviewStoreMergeUtils.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"
#include "UI/Review/BlueprintHelperReviewDebugText.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewAssetPresenters.h"
#include "UI/Review/BlueprintHelperReviewDebugBundleService.h"
#include "UI/Review/BlueprintHelperReviewGraphBounds.h"
#include "UI/Review/BlueprintHelperReviewGraphResolver.h"
#include "UI/Review/BlueprintHelperReviewPanelCommandService.h"
#include "UI/Review/BlueprintHelperReviewPanelPresenter.h"
#include "UI/Review/BlueprintHelperReviewPanelStateService.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenter.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "UI/Review/SBlueprintHelperReviewPanel.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
#include "StructUtils/UserDefinedStruct.h"
#else
#include "Engine/UserDefinedStruct.h"
#endif
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Package.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "WidgetBlueprint.h"

class FBlueprintHelperReviewStoreServiceTestsLocalUtils
{
public:
	static FBlueprintHelperReviewAtomicTarget MakeReviewTestTarget(
		const FString& TargetKey,
		const FString& VisualGroupKey,
		const FString& EvidenceId,
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
		Target.LatestEvidenceId = EvidenceId;
		Target.SourceEvidenceIds.Add(EvidenceId);
		Target.RecordedAfterHash = RecordedAfterHash;
		Target.BaselineHash = TEXT("baseline_hash");
		Target.BeforeSnapshotJson = TEXT("{\"schema\":\"BlueprintHelper.ReviewTargetSnapshot.v2\",\"exists\":true}");
		Target.Ownership = TEXT("blueprinthelper_owned");
		return Target;
	}

	static FBlueprintHelperReviewAtomicTarget MakeReviewAssetFactoryTarget(
		const FString& TargetKey,
		const FString& EvidenceId,
		const FString& AssetPath,
		const FString& RecordedAfterHash = TEXT("after_asset_factory"))
	{
		FBlueprintHelperReviewAtomicTarget Target = MakeReviewTestTarget(
			TargetKey,
			TargetKey,
			EvidenceId,
			RecordedAfterHash);
		Target.Surface = EBlueprintHelperReviewSurface::Details;
		Target.AssetPath = AssetPath;
		Target.GraphName.Reset();
		Target.TargetKind = TEXT("asset_factory");
		Target.DisplayLabel = TEXT("BP_Door asset");
		return Target;
	}

	static FBlueprintHelperWriteReviewEvidence MakeReviewTestEvidence(
		const FString& ArchiveSessionId,
		const FString& TaskRunId,
		const FString& EvidenceId,
		const FString& AssetPath,
		const FBlueprintHelperReviewAtomicTarget& Target)
	{
		FBlueprintHelperWriteReviewEvidence Evidence;
		Evidence.ArchiveSessionId = ArchiveSessionId;
		Evidence.TaskRunId = TaskRunId;
		Evidence.EvidenceId = EvidenceId;
		Evidence.AssetPath = AssetPath;
		Evidence.OperationKind = TEXT("append_blueprint_graph");
		Evidence.DisplayLabel = TEXT("Door flow");
		Evidence.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
		Evidence.AtomicTargets.Add(Target);
		return Evidence;
	}

	static FBlueprintHelperReviewVisibleChange MakeReviewTestVisibleChange(
		const FString& ChangeId,
		const FString& AssetPath)
	{
		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = ChangeId;
		Change.AssetPath = AssetPath;
		Change.GraphName = TEXT("EventGraph");
		Change.LocationKey = TEXT("graph:EventGraph:block:DoorFlow");
		Change.LatestEvidenceId = ChangeId;
		Change.SourceEvidenceIds.Add(ChangeId);
		Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
		Change.Status = EBlueprintHelperReviewChangeStatus::Pending;
		Change.DisplayLabel = TEXT("Door flow");

		FBlueprintHelperReviewAtomicTarget Target = MakeReviewTestTarget(
			TEXT("graph_node:N1"),
			TEXT("graph:EventGraph:block:DoorFlow"),
			ChangeId);
		Target.AssetPath = AssetPath;
		Change.AtomicTargets.Add(Target);
		return Change;
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

	static USCS_Node* AddReviewSceneComponentNode(
		UBlueprint* Blueprint,
		const FString& ComponentName,
		USCS_Node* ParentNode = nullptr)
	{
		if (!Blueprint || !Blueprint->SimpleConstructionScript || ComponentName.IsEmpty())
		{
			return nullptr;
		}

		USCS_Node* Node = Blueprint->SimpleConstructionScript->CreateNode(
			USceneComponent::StaticClass(),
			FName(*ComponentName));
		if (!Node)
		{
			return nullptr;
		}

		if (ParentNode)
		{
			ParentNode->AddChildNode(Node);
		}
		else
		{
			Blueprint->SimpleConstructionScript->AddNode(Node);
		}
		return Node;
	}

	static FString MakeComponentBeforeAddedSnapshot(const FString& ComponentName)
	{
		return FString::Printf(
			TEXT("{\"schema\":\"BlueprintHelper.ReviewTargetSnapshot.v2\",\"target_kind\":\"component\",\"target_key\":\"component:%s\",\"exists\":false,\"name\":\"%s\"}"),
			*ComponentName,
			*ComponentName);
	}

	static FString MakeComponentAfterAddedSnapshot(
		const FString& ComponentName,
		const FString& ParentComponentName = FString())
	{
		if (ParentComponentName.IsEmpty())
		{
			return FString::Printf(
				TEXT("{\"schema\":\"BlueprintHelper.ReviewTargetSnapshot.v2\",\"target_kind\":\"component\",\"target_key\":\"component:%s\",\"exists\":true,\"name\":\"%s\"}"),
				*ComponentName,
				*ComponentName);
		}

		return FString::Printf(
			TEXT("{\"schema\":\"BlueprintHelper.ReviewTargetSnapshot.v2\",\"target_kind\":\"component\",\"target_key\":\"component:%s\",\"exists\":true,\"name\":\"%s\",\"parent_component\":\"%s\"}"),
			*ComponentName,
			*ComponentName,
			*ParentComponentName);
	}

	static FBlueprintHelperReviewAtomicTarget MakeAddedComponentTarget(
		UBlueprint* Blueprint,
		const FString& ComponentName,
		const FString& EvidenceId,
		const FString& ParentComponentName = FString())
	{
		FBlueprintHelperReviewAtomicTarget Target;
		Target.Surface = EBlueprintHelperReviewSurface::Components;
		Target.AssetPath = Blueprint ? Blueprint->GetPathName() : FString();
		Target.TargetKind = TEXT("component");
		Target.TargetKey = FString::Printf(TEXT("component:%s"), *ComponentName);
		Target.VisualGroupKey = Target.TargetKey;
		Target.DisplayLabel = ComponentName;
		Target.ComponentPath = ComponentName;
		Target.LatestEvidenceId = EvidenceId;
		Target.SourceEvidenceIds.Add(EvidenceId);
		Target.BaselineHash = FString::Printf(TEXT("before_%s"), *ComponentName);
		Target.RecordedAfterHash = FString::Printf(TEXT("after_%s"), *ComponentName);
		Target.BeforeSnapshotJson = MakeComponentBeforeAddedSnapshot(ComponentName);
		Target.AfterSnapshotJson = MakeComponentAfterAddedSnapshot(ComponentName, ParentComponentName);
		if (!ParentComponentName.IsEmpty())
		{
			Target.AnchorJson = FString::Printf(
				TEXT("{\"component_name\":\"%s\",\"parent_component\":\"%s\"}"),
				*ComponentName,
				*ParentComponentName);
		}
		return Target;
	}

	static FBlueprintHelperReviewVisibleChange MakeAddedComponentChange(
		const FString& ChangeId,
		const FString& EvidenceId,
		const FBlueprintHelperReviewAtomicTarget& Target,
		int32 ExecutionOrder = INDEX_NONE)
	{
		FBlueprintHelperReviewVisibleChange Change = MakeReviewVisibleChangeForTarget(
			ChangeId,
			EvidenceId,
			Target,
			EBlueprintHelperReviewChangeKind::Added);
		Change.ExecutionOrder = ExecutionOrder;
		Change.bIsAssetLifecycleRoot = true;
		Change.bRejectRemovesChildren = true;
		Change.ParentChangeId.Reset();
		return Change;
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

	static UUserDefinedStruct* MakeReviewUserDefinedStruct(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperReview/%s_%s"),
			*Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		if (!Package)
		{
			return nullptr;
		}

		UUserDefinedStruct* Structure = FStructureEditorUtils::CreateUserDefinedStruct(
			Package,
			*FString::Printf(TEXT("ST_%s"), *Prefix),
			RF_Public | RF_Standalone | RF_Transactional);
		if (!Structure)
		{
			return nullptr;
		}

		FEdGraphPinType IntPinType;
		IntPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		FStructureEditorUtils::AddVariable(Structure, IntPinType);
		if (TArray<FStructVariableDescription>* Variables = FStructureEditorUtils::GetVarDescPtr(Structure))
		{
			if (Variables->Num() > 0)
			{
				(*Variables)[0].FriendlyName = TEXT("SmokeValue");
				(*Variables)[0].VarName = FName(TEXT("SmokeValue"));
			}
		}
		FStructureEditorUtils::CompileStructure(Structure);
		Package->SetDirtyFlag(false);
		return Structure;
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

	static bool PopulateReviewNodeCommentTargetFromSnapshot(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		UEdGraphNode* Node,
		const FString& EvidenceId,
		FBlueprintHelperReviewAtomicTarget& OutTarget,
		FString& OutError)
	{
		if (!Blueprint || !Graph || !Node)
		{
			OutError = TEXT("invalid_node_comment_target_fixture");
			return false;
		}

		OutTarget = FBlueprintHelperReviewAtomicTarget();
		OutTarget.Surface = EBlueprintHelperReviewSurface::Graph;
		OutTarget.AssetPath = Blueprint->GetPathName();
		OutTarget.GraphName = Graph->GetName();
		OutTarget.TargetKind = TEXT("graph_external_node");
		OutTarget.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
		OutTarget.TargetKey = FString::Printf(
			TEXT("graph_external_node:%s:node:%s:field:node_comment"),
			*Graph->GetName(),
			*OutTarget.NodeGuid);
		OutTarget.PropertyPath = TEXT("node_comment");
		OutTarget.VisualGroupKey = OutTarget.TargetKey;
		OutTarget.DisplayLabel = TEXT("Review node comment");
		OutTarget.LatestEvidenceId = EvidenceId;
		OutTarget.SourceEvidenceIds.Add(EvidenceId);
		OutTarget.Ownership = TEXT("blueprinthelper_owned");

		FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
		FString BaselineHash;
		Node->NodeComment = FString::Printf(TEXT("before_%s"), *EvidenceId);
		if (!SnapshotService.CaptureTargetSnapshot(
			OutTarget,
			OutTarget.BeforeSnapshotJson,
			BaselineHash,
			OutError))
		{
			return false;
		}
		if (OutTarget.BeforeSnapshotJson.IsEmpty() || BaselineHash.IsEmpty())
		{
			OutError = TEXT("empty_node_comment_snapshot_fixture");
			return false;
		}

		OutTarget.BaselineHash = BaselineHash;
		Node->NodeComment = FString::Printf(TEXT("after_%s"), *EvidenceId);
		if (!SnapshotService.CaptureTargetSnapshot(
			OutTarget,
			OutTarget.AfterSnapshotJson,
			OutTarget.RecordedAfterHash,
			OutError))
		{
			return false;
		}
		if (OutTarget.AfterSnapshotJson.IsEmpty() || OutTarget.RecordedAfterHash.IsEmpty())
		{
			OutError = TEXT("empty_node_comment_after_snapshot_fixture");
			return false;
		}
		return true;
	}

	static bool PopulateReviewVariableTargetFromSnapshot(
		UBlueprint* Blueprint,
		const FString& VariableName,
		const FString& EvidenceId,
		FBlueprintHelperReviewAtomicTarget& OutTarget,
		FString& OutError)
	{
		if (!Blueprint || VariableName.IsEmpty())
		{
			OutError = TEXT("invalid_variable_target_fixture");
			return false;
		}

		const FName VariableFName(*VariableName);
		FEdGraphPinType IntPinType;
		IntPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		if (FBlueprintHelperReviewSnapshotRestoreService::FindBlueprintVariableIndex(Blueprint, VariableFName) == INDEX_NONE
			&& !FBlueprintEditorUtils::AddMemberVariable(Blueprint, VariableFName, IntPinType))
		{
			OutError = TEXT("variable_fixture_add_failed");
			return false;
		}

		const int32 VariableIndex =
			FBlueprintHelperReviewSnapshotRestoreService::FindBlueprintVariableIndex(Blueprint, VariableFName);
		if (VariableIndex == INDEX_NONE)
		{
			OutError = TEXT("variable_fixture_missing_after_add");
			return false;
		}

		OutTarget = FBlueprintHelperReviewAtomicTarget();
		OutTarget.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
		OutTarget.AssetPath = Blueprint->GetPathName();
		OutTarget.TargetKind = TEXT("blueprint_variable");
		OutTarget.TargetKey = FString::Printf(TEXT("blueprint_variable:%s"), *VariableName);
		OutTarget.VisualGroupKey = OutTarget.TargetKey;
		OutTarget.DisplayLabel = VariableName;
		OutTarget.LatestEvidenceId = EvidenceId;
		OutTarget.SourceEvidenceIds.Add(EvidenceId);

		FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
		FString BaselineHash;
		Blueprint->NewVariables[VariableIndex].DefaultValue = TEXT("11");
		if (!SnapshotService.CaptureTargetSnapshot(
			OutTarget,
			OutTarget.BeforeSnapshotJson,
			BaselineHash,
			OutError))
		{
			return false;
		}
		if (OutTarget.BeforeSnapshotJson.IsEmpty() || BaselineHash.IsEmpty())
		{
			OutError = TEXT("empty_variable_before_snapshot_fixture");
			return false;
		}

		OutTarget.BaselineHash = BaselineHash;
		Blueprint->NewVariables[VariableIndex].DefaultValue = TEXT("22");
		if (!SnapshotService.CaptureTargetSnapshot(
			OutTarget,
			OutTarget.AfterSnapshotJson,
			OutTarget.RecordedAfterHash,
			OutError))
		{
			return false;
		}
		if (OutTarget.AfterSnapshotJson.IsEmpty() || OutTarget.RecordedAfterHash.IsEmpty())
		{
			OutError = TEXT("empty_variable_after_snapshot_fixture");
			return false;
		}
		return true;
	}

	static bool PopulateReviewGraphBlockTargetFromSnapshot(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FString& BlockLabel,
		const FString& EvidenceId,
		FBlueprintHelperReviewAtomicTarget& OutTarget,
		FString& OutError)
	{
		if (!Blueprint || !Graph || BlockLabel.IsEmpty())
		{
			OutError = TEXT("invalid_graph_block_target_fixture");
			return false;
		}

		const FString BlockId =
			FBlueprintHelperReviewStoreService::NormalizeGraphBlockTargetId(Graph->GetName(), BlockLabel);
		UK2Node_CustomEvent* FirstNode = AddReviewConversionEventNode(Graph, BlockLabel + TEXT("_A"));
		UK2Node_CustomEvent* SecondNode = AddReviewConversionEventNode(Graph, BlockLabel + TEXT("_B"));
		if (!FirstNode || !SecondNode)
		{
			OutError = TEXT("graph_block_fixture_node_create_failed");
			return false;
		}
		MarkReviewNodeAsBlueprintHelperOwned(FirstNode, BlockId);
		MarkReviewNodeAsBlueprintHelperOwned(SecondNode, BlockId);

		OutTarget = FBlueprintHelperReviewAtomicTarget();
		OutTarget.Surface = EBlueprintHelperReviewSurface::Graph;
		OutTarget.AssetPath = Blueprint->GetPathName();
		OutTarget.GraphName = Graph->GetName();
		OutTarget.TargetKind = TEXT("graph_block");
		OutTarget.TargetKey = FString::Printf(TEXT("graph:%s:block:%s"), *Graph->GetName(), *BlockId);
		OutTarget.VisualGroupKey = OutTarget.TargetKey;
		OutTarget.DisplayLabel = BlockLabel;
		OutTarget.LatestEvidenceId = EvidenceId;
		OutTarget.SourceEvidenceIds.Add(EvidenceId);
		OutTarget.Ownership = TEXT("blueprinthelper_owned");

		FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
		FString BaselineHash;
		if (!SnapshotService.CaptureTargetSnapshot(
			OutTarget,
			OutTarget.BeforeSnapshotJson,
			BaselineHash,
			OutError))
		{
			return false;
		}
		if (OutTarget.BeforeSnapshotJson.IsEmpty() || BaselineHash.IsEmpty())
		{
			OutError = TEXT("empty_graph_block_before_snapshot_fixture");
			return false;
		}

		OutTarget.BaselineHash = BaselineHash;
		UK2Node_CustomEvent* ThirdNode = AddReviewConversionEventNode(Graph, BlockLabel + TEXT("_C"));
		if (!ThirdNode)
		{
			OutError = TEXT("graph_block_fixture_after_node_create_failed");
			return false;
		}
		MarkReviewNodeAsBlueprintHelperOwned(ThirdNode, BlockId);
		if (!SnapshotService.CaptureTargetSnapshot(
			OutTarget,
			OutTarget.AfterSnapshotJson,
			OutTarget.RecordedAfterHash,
			OutError))
		{
			return false;
		}
		if (OutTarget.AfterSnapshotJson.IsEmpty() || OutTarget.RecordedAfterHash.IsEmpty())
		{
			OutError = TEXT("empty_graph_block_after_snapshot_fixture");
			return false;
		}
		return true;
	}

	static FBlueprintHelperReviewVisibleChange MakeReviewVisibleChangeForTarget(
		const FString& ChangeId,
		const FString& EvidenceId,
		const FBlueprintHelperReviewAtomicTarget& Target,
		EBlueprintHelperReviewChangeKind ChangeKind = EBlueprintHelperReviewChangeKind::Modified)
	{
		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = ChangeId;
		Change.AssetPath = Target.AssetPath;
		Change.GraphName = Target.GraphName;
		Change.LocationKey = Target.TargetKey;
		Change.LatestEvidenceId = EvidenceId;
		Change.LatestEvidenceIds.Add(EvidenceId);
		Change.SourceEvidenceIds.Add(EvidenceId);
		Change.ChangeKind = ChangeKind;
		Change.Status = EBlueprintHelperReviewChangeStatus::Pending;
		Change.DisplayLabel = Target.DisplayLabel;
		Change.BeforeHash = Target.BaselineHash;
		Change.AfterHash = Target.RecordedAfterHash;
		Change.BeforeSnapshotJson = Target.BeforeSnapshotJson;
		Change.AfterSnapshotJson = Target.AfterSnapshotJson;
		Change.AtomicTargets.Add(Target);
		return Change;
	}

	static FBlueprintHelperReviewRecord MakeReviewRecordForVisibleChanges(
		const FString& ArchiveSessionId,
		const FString& AssetPath,
		const TArray<FBlueprintHelperReviewVisibleChange>& Changes)
	{
		FBlueprintHelperReviewRecord Record;
		Record.ReviewRecordId = FBlueprintHelperReviewStoreService::MakeReviewRecordId(ArchiveSessionId, AssetPath);
		Record.ArchiveSessionId = ArchiveSessionId;
		Record.AssetPath = AssetPath;
		Record.Status = EBlueprintHelperReviewChangeStatus::Pending;
		Record.SourceReviewSummary.AssetPaths.AddUnique(AssetPath);
		for (const FBlueprintHelperReviewVisibleChange& Change : Changes)
		{
			Record.SourceReviewSummary.EvidenceIds.AddUnique(Change.LatestEvidenceId);
			Record.VisibleChanges.Add(Change);
		}
		Record.SourceReviewSummary.EvidenceCount = Record.SourceReviewSummary.EvidenceIds.Num();
		return Record;
	}

	static UK2Node_CallFunction* AddReviewDestroyActorCallNode(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UFunction* Function = AActor::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(AActor, K2_DestroyActor));
		if (!Function)
		{
			return nullptr;
		}

		UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
		Graph->AddNode(CallNode, true, false);
		CallNode->CreateNewGuid();
		CallNode->SetFromFunction(Function);
		CallNode->PostPlacedNewNode();
		CallNode->AllocateDefaultPins();
		return CallNode;
	}

	static UEdGraphPin* FindReviewExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	static bool ConnectReviewExecPins(UEdGraphPin* FromPin, UEdGraphPin* ToPin)
	{
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		return FromPin && ToPin && Schema && Schema->TryCreateConnection(FromPin, ToPin);
	}

	static void MarkReviewNodeAsBlueprintHelperOwned(UEdGraphNode* Node, const FString& BlockId)
	{
		if (!Node)
		{
			return;
		}
		if (UPackage* Package = Node->GetOutermost())
		{
			FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
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
			FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
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
	FBlueprintHelperReviewBaselineSemanticHashCapturesGraphNodeTest,
	"BlueprintHelper.Review.Baseline.SemanticHashCapturesGraphNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewBaselineSemanticHashCapturesGraphNodeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("SemanticHashGraphNode"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Node = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("SemanticHashNode"));
	TestNotNull(TEXT("graph node created"), Node);
	if (!Node)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Blueprint->GetPathName();
	Target.GraphName = Graph->GetName();
	Target.TargetKind = TEXT("graph_node");
	Target.TargetKey = FString::Printf(TEXT("graph:%s:node:%s"), *Graph->GetName(), *Node->NodeGuid.ToString(EGuidFormats::Digits));
	Target.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);

	FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
	FString SnapshotJson;
	FString SnapshotHash;
	FString SnapshotError;
	TestTrue(TEXT("graph node target snapshot captured"),
		SnapshotService.CaptureTargetSnapshot(Target, SnapshotJson, SnapshotHash, SnapshotError));
	TestFalse(TEXT("graph node snapshot hash emitted"), SnapshotHash.IsEmpty());
	TestTrue(TEXT("graph node snapshot marks existing target"), SnapshotJson.Contains(TEXT("\"exists\": true")));
	TestTrue(TEXT("graph node snapshot carries node object"), SnapshotJson.Contains(TEXT("\"node\"")));
	TestEqual(TEXT("semantic hash is canonical rehash of snapshot"),
		SnapshotHash,
		FBlueprintHelperReviewBaselineSnapshotService::ComputeSemanticSnapshotHash(SnapshotJson));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewBaselineSemanticHashCapturesGraphBlockTest,
	"BlueprintHelper.Review.Baseline.SemanticHashCapturesGraphBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewBaselineSemanticHashCapturesGraphBlockTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("SemanticHashGraphBlock"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString BlockId = FBlueprintHelperReviewStoreService::NormalizeGraphBlockTargetId(Graph->GetName(), TEXT("SemanticBlock"));
	UK2Node_CustomEvent* FirstNode = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("SemanticBlockA"));
	UK2Node_CustomEvent* SecondNode = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("SemanticBlockB"));
	TestNotNull(TEXT("first block node created"), FirstNode);
	TestNotNull(TEXT("second block node created"), SecondNode);
	if (!FirstNode || !SecondNode)
	{
		return false;
	}
	FBlueprintHelperReviewStoreServiceTestsLocalUtils::MarkReviewNodeAsBlueprintHelperOwned(FirstNode, BlockId);
	FBlueprintHelperReviewStoreServiceTestsLocalUtils::MarkReviewNodeAsBlueprintHelperOwned(SecondNode, BlockId);

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Blueprint->GetPathName();
	Target.GraphName = Graph->GetName();
	Target.TargetKind = TEXT("graph_block");
	Target.TargetKey = FString::Printf(TEXT("graph:%s:block:%s"), *Graph->GetName(), *BlockId);

	FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
	FString FirstSnapshotJson;
	FString FirstSnapshotHash;
	FString SnapshotError;
	TestTrue(TEXT("graph block target snapshot captured"),
		SnapshotService.CaptureTargetSnapshot(Target, FirstSnapshotJson, FirstSnapshotHash, SnapshotError));
	FString SecondSnapshotJson;
	FString SecondSnapshotHash;
	TestTrue(TEXT("graph block target snapshot is recapturable"),
		SnapshotService.CaptureTargetSnapshot(Target, SecondSnapshotJson, SecondSnapshotHash, SnapshotError));
	TestFalse(TEXT("graph block snapshot hash emitted"), FirstSnapshotHash.IsEmpty());
	TestEqual(TEXT("graph block hash is stable across captures"), FirstSnapshotHash, SecondSnapshotHash);
	TestTrue(TEXT("graph block snapshot carries block id"), FirstSnapshotJson.Contains(BlockId));
	TestTrue(TEXT("graph block snapshot carries nodes"), FirstSnapshotJson.Contains(TEXT("\"nodes\"")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewExternalBoundarySnapshotRestoreOnlyRewritesBoundaryPinTest,
	"BlueprintHelper.Review.Baseline.ExternalBoundarySnapshotRestoreOnlyRewritesBoundaryPin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewExternalBoundarySnapshotRestoreOnlyRewritesBoundaryPinTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("ExternalBoundaryRestore"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* EventNode = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("BoundaryEntry"));
	UK2Node_CallFunction* OriginalCall = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewDestroyActorCallNode(Graph);
	UK2Node_CallFunction* ReplacementCall = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewDestroyActorCallNode(Graph);
	UEdGraphPin* SourcePin = FBlueprintHelperReviewStoreServiceTestsLocalUtils::FindReviewExecPin(EventNode, EGPD_Output);
	UEdGraphPin* OriginalTargetPin = FBlueprintHelperReviewStoreServiceTestsLocalUtils::FindReviewExecPin(OriginalCall, EGPD_Input);
	UEdGraphPin* ReplacementTargetPin = FBlueprintHelperReviewStoreServiceTestsLocalUtils::FindReviewExecPin(ReplacementCall, EGPD_Input);
	TestTrue(TEXT("initial exec pins connect"),
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::ConnectReviewExecPins(SourcePin, OriginalTargetPin));
	if (!EventNode || !SourcePin || !OriginalTargetPin || !ReplacementTargetPin)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Blueprint->GetPathName();
	Target.GraphName = Graph->GetName();
	Target.TargetKind = TEXT("graph_external_boundary");
	Target.TargetKey = FString::Printf(
		TEXT("graph_external_boundary:%s:node:%s:pin:%s"),
		*Graph->GetName(),
		*EventNode->NodeGuid.ToString(EGuidFormats::Digits),
		*SourcePin->PinName.ToString());
	Target.NodeGuid = EventNode->NodeGuid.ToString(EGuidFormats::Digits);
	Target.PinPath = SourcePin->PinName.ToString();

	FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
	FString SnapshotJson;
	FString SnapshotHash;
	FString SnapshotError;
	TestTrue(TEXT("external boundary target snapshot captured"),
		SnapshotService.CaptureTargetSnapshot(Target, SnapshotJson, SnapshotHash, SnapshotError));
	TestFalse(TEXT("external boundary snapshot hash emitted"), SnapshotHash.IsEmpty());
	TestTrue(TEXT("external boundary snapshot carries single node object"), SnapshotJson.Contains(TEXT("\"node\"")));
	TestFalse(TEXT("external boundary snapshot does not import graph text"), SnapshotJson.Contains(TEXT("\"restore_text\"")));

	SourcePin->BreakAllPinLinks(true);
	TestTrue(TEXT("replacement exec pins connect"),
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::ConnectReviewExecPins(SourcePin, ReplacementTargetPin));
	TestTrue(TEXT("replacement link exists before restore"), SourcePin->LinkedTo.Contains(ReplacementTargetPin));
	TestFalse(TEXT("original link removed before restore"), SourcePin->LinkedTo.Contains(OriginalTargetPin));

	TSharedPtr<FJsonObject> SnapshotObject;
	FString ParseError;
	TestTrue(TEXT("external boundary snapshot parses"),
		FBlueprintHelperReviewSnapshotRestoreService::ParseReviewSnapshotJson(SnapshotJson, SnapshotObject, ParseError));

	FString RestoreError;
	TestTrue(TEXT("external boundary snapshot restores"),
		FBlueprintHelperReviewSnapshotRestoreService::RestoreExternalBoundaryFromSnapshot(Target, SnapshotObject, RestoreError));
	TestEqual(TEXT("boundary source has one restored link"), SourcePin->LinkedTo.Num(), 1);
	TestTrue(TEXT("original link restored"), SourcePin->LinkedTo.Contains(OriginalTargetPin));
	TestFalse(TEXT("replacement link removed"), SourcePin->LinkedTo.Contains(ReplacementTargetPin));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewExternalBoundarySnapshotRestoreFailsWhenBoundaryPinMissingTest,
	"BlueprintHelper.Review.Baseline.ExternalBoundarySnapshotRestoreFailsWhenBoundaryPinMissing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewExternalBoundarySnapshotRestoreFailsWhenBoundaryPinMissingTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("ExternalBoundaryMissingPin"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* EventNode = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("BoundaryEntryMissingPin"));
	UK2Node_CallFunction* OriginalCall = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewDestroyActorCallNode(Graph);
	UEdGraphPin* SourcePin = FBlueprintHelperReviewStoreServiceTestsLocalUtils::FindReviewExecPin(EventNode, EGPD_Output);
	UEdGraphPin* OriginalTargetPin = FBlueprintHelperReviewStoreServiceTestsLocalUtils::FindReviewExecPin(OriginalCall, EGPD_Input);
	TestTrue(TEXT("initial exec pins connect"),
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::ConnectReviewExecPins(SourcePin, OriginalTargetPin));
	if (!EventNode || !SourcePin || !OriginalTargetPin)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Blueprint->GetPathName();
	Target.GraphName = Graph->GetName();
	Target.TargetKind = TEXT("graph_external_boundary");
	Target.TargetKey = FString::Printf(
		TEXT("graph_external_boundary:%s:node:%s:pin:%s"),
		*Graph->GetName(),
		*EventNode->NodeGuid.ToString(EGuidFormats::Digits),
		*SourcePin->PinName.ToString());
	Target.NodeGuid = EventNode->NodeGuid.ToString(EGuidFormats::Digits);
	Target.PinPath = SourcePin->PinName.ToString();

	FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
	FString SnapshotJson;
	FString SnapshotHash;
	FString SnapshotError;
	TestTrue(TEXT("external boundary target snapshot captured"),
		SnapshotService.CaptureTargetSnapshot(Target, SnapshotJson, SnapshotHash, SnapshotError));

	SourcePin->PinName = FName(TEXT("RenamedThenPin"));

	TSharedPtr<FJsonObject> SnapshotObject;
	FString ParseError;
	TestTrue(TEXT("external boundary snapshot parses"),
		FBlueprintHelperReviewSnapshotRestoreService::ParseReviewSnapshotJson(SnapshotJson, SnapshotObject, ParseError));

	FString RestoreError;
	TestFalse(TEXT("external boundary restore fails when boundary pin is missing"),
		FBlueprintHelperReviewSnapshotRestoreService::RestoreExternalBoundaryFromSnapshot(Target, SnapshotObject, RestoreError));
	TestTrue(TEXT("restore error reports missing pin"), RestoreError.Contains(TEXT("pin")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewExternalNodeRejectRestoresSelectedFieldOnlyTest,
	"BlueprintHelper.Review.ExternalNode.RejectRestoresSelectedFieldOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewExternalNodeRejectRestoresSelectedFieldOnlyTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("ExternalNodeFieldRestore"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* EventNode = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("ExternalNodeFieldEntry"));
	UK2Node_CallFunction* OriginalCall = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewDestroyActorCallNode(Graph);
	UK2Node_CallFunction* ReplacementCall = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewDestroyActorCallNode(Graph);
	UEdGraphPin* SourcePin = FBlueprintHelperReviewStoreServiceTestsLocalUtils::FindReviewExecPin(EventNode, EGPD_Output);
	UEdGraphPin* OriginalTargetPin = FBlueprintHelperReviewStoreServiceTestsLocalUtils::FindReviewExecPin(OriginalCall, EGPD_Input);
	UEdGraphPin* ReplacementTargetPin = FBlueprintHelperReviewStoreServiceTestsLocalUtils::FindReviewExecPin(ReplacementCall, EGPD_Input);
	TestTrue(TEXT("initial exec pins connect"),
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::ConnectReviewExecPins(SourcePin, OriginalTargetPin));
	if (!EventNode || !SourcePin || !OriginalTargetPin || !ReplacementTargetPin)
	{
		return false;
	}

	EventNode->NodeComment = TEXT("before comment");

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Blueprint->GetPathName();
	Target.GraphName = Graph->GetName();
	Target.TargetKind = TEXT("graph_external_node");
	Target.TargetKey = FString::Printf(
		TEXT("graph_external_node:%s:node:%s:field:node_comment"),
		*Graph->GetName(),
		*EventNode->NodeGuid.ToString(EGuidFormats::Digits));
	Target.NodeGuid = EventNode->NodeGuid.ToString(EGuidFormats::Digits);
	Target.PropertyPath = TEXT("node_comment");

	FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
	FString SnapshotJson;
	FString SnapshotHash;
	FString SnapshotError;
	TestTrue(TEXT("external node field target snapshot captured"),
		SnapshotService.CaptureTargetSnapshot(Target, SnapshotJson, SnapshotHash, SnapshotError));
	TestFalse(TEXT("external node field snapshot hash emitted"), SnapshotHash.IsEmpty());
	TestTrue(TEXT("external node field snapshot carries value"), SnapshotJson.Contains(TEXT("before comment")));

	EventNode->NodeComment = TEXT("after comment");
	SourcePin->BreakAllPinLinks(true);
	TestTrue(TEXT("replacement exec pins connect"),
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::ConnectReviewExecPins(SourcePin, ReplacementTargetPin));

	TSharedPtr<FJsonObject> SnapshotObject;
	FString ParseError;
	TestTrue(TEXT("external node field snapshot parses"),
		FBlueprintHelperReviewSnapshotRestoreService::ParseReviewSnapshotJson(SnapshotJson, SnapshotObject, ParseError));

	FString RestoreError;
	TestTrue(TEXT("external node field snapshot restores"),
		FBlueprintHelperReviewSnapshotRestoreService::RestoreExternalNodeFromSnapshot(Target, SnapshotObject, RestoreError));
	TestEqual(TEXT("comment restored"), EventNode->NodeComment, FString(TEXT("before comment")));
	TestTrue(TEXT("replacement link remains"), SourcePin->LinkedTo.Contains(ReplacementTargetPin));
	TestFalse(TEXT("original link not restored by field handler"), SourcePin->LinkedTo.Contains(OriginalTargetPin));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewStoreUsesSemanticHashForGraphTargetTest,
	"BlueprintHelper.Review.Store.UsesSemanticHashForGraphTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewStoreUsesSemanticHashForGraphTargetTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("StoreSemanticGraph"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Node = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("StoreSemanticNode"));
	TestNotNull(TEXT("graph node created"), Node);
	if (!Node)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Blueprint->GetPathName();
	Target.GraphName = Graph->GetName();
	Target.Surface = EBlueprintHelperReviewSurface::Graph;
	Target.TargetKind = TEXT("graph_node");
	Target.TargetKey = FString::Printf(TEXT("graph:%s:node:%s"), *Graph->GetName(), *Node->NodeGuid.ToString(EGuidFormats::Digits));
	Target.VisualGroupKey = Target.TargetKey;
	Target.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);

	FBlueprintHelperWriteReviewEvidence Evidence;
	Evidence.ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_store_semantic"));
	Evidence.TaskRunId = TEXT("task_store_semantic");
	Evidence.EvidenceId = TEXT("tx_store_semantic");
	Evidence.AssetPath = Blueprint->GetPathName();
	Evidence.OperationKind = TEXT("append_blueprint_graph");
	Evidence.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
	Evidence.AtomicTargets.Add(Target);

	FBlueprintHelperReviewStoreService Store;
	const TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence({Evidence});
	TestEqual(TEXT("one semantic review record built"), Records.Num(), 1);
	if (Records.Num() != 1 || Records[0].VisibleChanges.Num() != 1 || Records[0].VisibleChanges[0].AtomicTargets.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& StoredTarget = Records[0].VisibleChanges[0].AtomicTargets[0];
	TestFalse(TEXT("store emits semantic baseline hash"), StoredTarget.BaselineHash.IsEmpty());
	TestFalse(TEXT("store emits semantic recorded-after hash"), StoredTarget.RecordedAfterHash.IsEmpty());
	TestTrue(TEXT("store writes before snapshot"), StoredTarget.BeforeSnapshotJson.Contains(TEXT("\"exists\": false")));
	TestTrue(TEXT("store writes after snapshot"), StoredTarget.AfterSnapshotJson.Contains(TEXT("\"exists\": true")));
	TestEqual(TEXT("recorded-after hash matches canonical target snapshot"),
		StoredTarget.RecordedAfterHash,
		FBlueprintHelperReviewBaselineSnapshotService::ComputeSemanticSnapshotHash(StoredTarget.AfterSnapshotJson));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectGraphTargetUsesSemanticHashGuardTest,
	"BlueprintHelper.Review.Reject.GraphTargetUsesSemanticHashGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectGraphTargetUsesSemanticHashGuardTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("RejectSemanticGuard"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Node = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("RejectSemanticNode"));
	TestNotNull(TEXT("graph node created"), Node);
	if (!Node)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Blueprint->GetPathName();
	Target.GraphName = Graph->GetName();
	Target.TargetKind = TEXT("graph_node");
	Target.TargetKey = FString::Printf(TEXT("graph:%s:node:%s"), *Graph->GetName(), *Node->NodeGuid.ToString(EGuidFormats::Digits));
	Target.VisualGroupKey = Target.TargetKey;
	Target.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
	Target.RecordedAfterHash = TEXT("legacy_graph_hash");
	Target.BaselineHash = TEXT("legacy_baseline_hash");

	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("change_reject_semantic");
	Change.AssetPath = Blueprint->GetPathName();
	Change.GraphName = Graph->GetName();
	Change.LatestEvidenceId = TEXT("tx_reject_semantic");
	Change.AtomicTargets.Add(Target);

	FBlueprintHelperReviewRejectOptions Options;
	const FBlueprintHelperReviewActionResult DirectResult =
		FBlueprintHelperReviewRejectService::RejectVisibleChangeWithDefaultDispatcher(Change, &Options);
	TestFalse(TEXT("legacy graph record without recoverable snapshot still needs action"), DirectResult.bSucceeded);
	TestEqual(TEXT("legacy graph record without recoverable snapshot enters needs action"),
		DirectResult.NewStatus,
		EBlueprintHelperReviewChangeStatus::NeedsAction);
	TestEqual(TEXT("semantic hash drift is diagnostic, not the blocking reason"),
		DirectResult.Message,
		FString(TEXT("missing_recoverable_snapshot")));
	TestEqual(TEXT("semantic guard still records target key"),
		DirectResult.HashGuardTargetKey,
		Target.TargetKey);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectNeedsActionWithoutRecoverableBeforeSnapshotTest,
	"BlueprintHelper.Review.Reject.NeedsActionWithoutRecoverableBeforeSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectNeedsActionWithoutRecoverableBeforeSnapshotTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewAtomicTarget Target =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
			TEXT("blueprint_variable:Health"),
			TEXT("blueprint_variable:Health"),
			TEXT("tx_missing_recoverable"),
			TEXT("after_semantic"));
	Target.TargetKind = TEXT("blueprint_variable");
	Target.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
	Target.BeforeSnapshotJson.Reset();

	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("change_missing_recoverable");
	Change.AssetPath = Target.AssetPath;
	Change.LatestEvidenceId = Target.LatestEvidenceId;
	Change.AtomicTargets.Add(Target);

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(Target.TargetKey, Target.RecordedAfterHash);

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.RejectVisibleChange(Change, Options);
	TestFalse(TEXT("snapshot-restorable target without before snapshot is blocked"), Result.bSucceeded);
	TestEqual(TEXT("missing recoverable before snapshot enters needs action"),
		Result.NewStatus,
		EBlueprintHelperReviewChangeStatus::NeedsAction);
	TestEqual(TEXT("reason is explicit"),
		Result.Message,
		FString(TEXT("missing_recoverable_snapshot")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewDebugBundleSummarizesSemanticHashSourceTest,
	"BlueprintHelper.Review.DebugBundle.SummarizesSemanticHashSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewDebugBundleSummarizesSemanticHashSourceTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBlueprintHelperReviewVisibleChange> Change = MakeShared<FBlueprintHelperReviewVisibleChange>();
	Change->ChangeId = TEXT("change_debug_semantic");
	Change->AssetPath = TEXT("/Game/BP_DebugSemantic");
	Change->LatestEvidenceId = TEXT("tx_debug_semantic");
	Change->BeforeHash = TEXT("crc32_before");
	Change->AfterHash = TEXT("crc32_after");
	FBlueprintHelperReviewAtomicTarget Target =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
			TEXT("graph:EventGraph:node:DebugSemantic"),
			TEXT("graph:EventGraph:node:DebugSemantic"),
			TEXT("tx_debug_semantic"),
			TEXT("crc32_after"));
	Target.NodeGuid = TEXT("00112233445566778899aabbccddeeff");
	Change->AtomicTargets.Add(Target);

	const TSharedRef<FJsonObject> Event = FBlueprintHelperReviewDebugBundleService::BuildLogEvent(
		TEXT("session_debug_semantic"),
		TEXT("debug"),
		Change,
		Change->AssetPath);
	const TSharedPtr<FJsonObject>* SelectedChangePtr = nullptr;
	TestTrue(TEXT("debug event carries selected change"),
		Event->TryGetObjectField(TEXT("selected_change"), SelectedChangePtr) && SelectedChangePtr && SelectedChangePtr->IsValid());
	if (!SelectedChangePtr || !SelectedChangePtr->IsValid())
	{
		return false;
	}

	FString HashSource;
	FString SnapshotSchema;
	TestTrue(TEXT("selected change exposes semantic hash source"),
		(*SelectedChangePtr)->TryGetStringField(TEXT("hash_source"), HashSource));
	TestEqual(TEXT("hash source is semantic target snapshot"),
		HashSource,
		FString(TEXT("semantic_target_snapshot")));
	TestTrue(TEXT("selected change exposes target snapshot schema"),
		(*SelectedChangePtr)->TryGetStringField(TEXT("snapshot_schema"), SnapshotSchema));
	TestEqual(TEXT("snapshot schema is target snapshot"),
		SnapshotSchema,
		FString(TEXT("BlueprintHelper.ReviewTargetSnapshot.v2")));

	FString SerializedEvent;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SerializedEvent);
	FJsonSerializer::Serialize(Event, Writer);
	TestFalse(TEXT("agent-facing debug summary does not expose guid field"), SerializedEvent.Contains(TEXT("\"guid\"")));
	TestFalse(TEXT("agent-facing debug summary does not expose node_guid field"), SerializedEvent.Contains(TEXT("node_guid")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewDebugBundleRejectTimingEventTest,
	"BlueprintHelper.Review.DebugBundle.RejectTimingEvent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewDebugBundleRejectTimingEventTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBlueprintHelperReviewVisibleChange> Change = MakeShared<FBlueprintHelperReviewVisibleChange>();
	Change->ChangeId = TEXT("change_reject_timing");
	Change->AssetPath = TEXT("/Game/BP_RejectTiming");
	Change->LatestEvidenceId = TEXT("tx_reject_timing");

	const TSharedRef<FJsonObject> Event = FBlueprintHelperReviewDebugBundleService::BuildRejectTimingEvent(
		TEXT("session_reject_timing"),
		TEXT("success_feedback_shown"),
		Change->ChangeId,
		Change,
		Change->AssetPath,
		12.5,
		123.75,
		TEXT("rejected"));

	FString EventType;
	TestTrue(TEXT("event type present"), Event->TryGetStringField(TEXT("event_type"), EventType));
	TestEqual(TEXT("event type is reject timing"),
		EventType,
		FString(TEXT("review_reject_timing")));

	FString Stage;
	TestTrue(TEXT("stage present"), Event->TryGetStringField(TEXT("stage"), Stage));
	TestEqual(TEXT("stage captured"), Stage, FString(TEXT("success_feedback_shown")));

	FString ChangeId;
	TestTrue(TEXT("change id present"), Event->TryGetStringField(TEXT("change_id"), ChangeId));
	TestEqual(TEXT("change id captured"), ChangeId, Change->ChangeId);

	double StageMs = 0.0;
	double TotalMs = 0.0;
	TestTrue(TEXT("stage ms present"), Event->TryGetNumberField(TEXT("stage_ms"), StageMs));
	TestTrue(TEXT("total ms present"), Event->TryGetNumberField(TEXT("total_ms"), TotalMs));
	TestEqual(TEXT("stage ms captured"), StageMs, 12.5);
	TestEqual(TEXT("total ms captured"), TotalMs, 123.75);

	const TSharedPtr<FJsonObject>* SelectedChangePtr = nullptr;
	TestTrue(TEXT("debug event carries selected change"),
		Event->TryGetObjectField(TEXT("selected_change"), SelectedChangePtr)
		&& SelectedChangePtr
		&& SelectedChangePtr->IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewStoreUsesSemanticHashForSnapshotRestoreTargetTest,
	"BlueprintHelper.Review.Store.UsesSemanticHashForSnapshotRestoreTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewStoreUsesSemanticHashForSnapshotRestoreTargetTest::RunTest(const FString& Parameters)
{
	UDataTable* DataTable = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewDataTable(TEXT("StoreSemanticDataTable"));
	TestNotNull(TEXT("test data table created"), DataTable);
	if (!DataTable)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = DataTable->GetPathName();
	Target.Surface = EBlueprintHelperReviewSurface::DataTable;
	Target.TargetKind = TEXT("datatable_row");
	Target.TargetKey = TEXT("datatable_row:DamageSmall");
	Target.VisualGroupKey = Target.TargetKey;
	Target.DisplayLabel = TEXT("DamageSmall");

	FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
	FString BeforeSnapshotJson;
	FString BeforeSnapshotHash;
	FString SnapshotError;
	TestTrue(TEXT("datatable before snapshot captured"),
		SnapshotService.CaptureTargetSnapshot(Target, BeforeSnapshotJson, BeforeSnapshotHash, SnapshotError));
	Target.BeforeSnapshotJson = BeforeSnapshotJson;
	Target.BaselineHash = BeforeSnapshotHash;

	const FVector UpdatedValue(9.0, 8.0, 7.0);
	TMap<FName, const uint8*> RawRows;
	RawRows.Add(FName(TEXT("DamageSmall")), reinterpret_cast<const uint8*>(&UpdatedValue));
	DataTable->CreateTableFromRawData(RawRows, TBaseStructure<FVector>::Get());

	FBlueprintHelperWriteReviewEvidence Evidence;
	Evidence.ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_store_restore"));
	Evidence.TaskRunId = TEXT("task_store_restore");
	Evidence.EvidenceId = TEXT("tx_store_restore");
	Evidence.AssetPath = DataTable->GetPathName();
	Evidence.OperationKind = TEXT("datatable_update_row");
	Evidence.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	Evidence.AtomicTargets.Add(Target);

	FBlueprintHelperReviewStoreService Store;
	const TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence({Evidence});
	TestEqual(TEXT("one snapshot-restore semantic record built"), Records.Num(), 1);
	if (Records.Num() != 1 || Records[0].VisibleChanges.Num() != 1 || Records[0].VisibleChanges[0].AtomicTargets.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& StoredTarget = Records[0].VisibleChanges[0].AtomicTargets[0];
	TestFalse(TEXT("snapshot-restore target remains pending with before snapshot"),
		StoredTarget.Status == EBlueprintHelperReviewChangeStatus::NeedsAction);
	TestFalse(TEXT("snapshot-restore after snapshot is captured"), StoredTarget.AfterSnapshotJson.IsEmpty());
	TestEqual(TEXT("snapshot-restore baseline hash is semantic"),
		StoredTarget.BaselineHash,
		FBlueprintHelperReviewBaselineSnapshotService::ComputeSemanticSnapshotHash(StoredTarget.BeforeSnapshotJson));
	TestEqual(TEXT("snapshot-restore recorded-after hash is semantic"),
		StoredTarget.RecordedAfterHash,
		FBlueprintHelperReviewBaselineSnapshotService::ComputeSemanticSnapshotHash(StoredTarget.AfterSnapshotJson));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectBlocksWhenSemanticHashChangedTest,
	"BlueprintHelper.Review.Reject.BlocksWhenSemanticHashChanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectBlocksWhenSemanticHashChangedTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("RejectSemanticMismatch"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Node = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("RejectSemanticMismatchNode"));
	TestNotNull(TEXT("graph node created"), Node);
	if (!Node)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Blueprint->GetPathName();
	Target.GraphName = Graph->GetName();
	Target.TargetKind = TEXT("graph_node");
	Target.TargetKey = FString::Printf(TEXT("graph:%s:node:%s"), *Graph->GetName(), *Node->NodeGuid.ToString(EGuidFormats::Digits));
	Target.VisualGroupKey = Target.TargetKey;
	Target.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
	Target.RecordedAfterHash = TEXT("crc32_00000000");

	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("change_reject_semantic_mismatch");
	Change.AssetPath = Blueprint->GetPathName();
	Change.LatestEvidenceId = TEXT("tx_reject_semantic_mismatch");
	Change.AtomicTargets.Add(Target);

	const FBlueprintHelperReviewActionResult Result =
		FBlueprintHelperReviewRejectService::RejectVisibleChangeWithDefaultDispatcher(Change, nullptr);
	TestFalse(TEXT("missing recoverable snapshot blocks reject"), Result.bSucceeded);
	TestEqual(TEXT("missing recoverable snapshot enters needs action"),
		Result.NewStatus,
		EBlueprintHelperReviewChangeStatus::NeedsAction);
	TestEqual(TEXT("semantic hash drift is diagnostic, not the blocking reason"),
		Result.Message,
		FString(TEXT("missing_recoverable_snapshot")));
	TestEqual(TEXT("semantic guard still records target key"),
		Result.HashGuardTargetKey,
		Target.TargetKey);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAgentFacingSummaryDoesNotExposeGuidTest,
	"BlueprintHelper.Review.Summary.AgentFacingSummaryDoesNotExposeGuid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAgentFacingSummaryDoesNotExposeGuidTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewRecord Record;
	Record.ReviewRecordId = TEXT("review_no_guid");
	Record.ArchiveSessionId = TEXT("archive_no_guid");
	Record.AssetPath = TEXT("/Game/BP_NoGuid");

	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("change_no_guid");
	Change.AssetPath = Record.AssetPath;
	Change.Status = EBlueprintHelperReviewChangeStatus::Pending;
	FBlueprintHelperReviewAtomicTarget Target =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
			TEXT("graph:EventGraph:node:NoGuid"),
			TEXT("graph:EventGraph:node:NoGuid"),
			TEXT("tx_no_guid"));
	Target.NodeGuid = TEXT("00112233445566778899aabbccddeeff");
	Change.AtomicTargets.Add(Target);
	Record.VisibleChanges.Add(Change);

	FBlueprintHelperReviewStoreService Store;
	const TSharedRef<FJsonObject> Summary = Store.BuildReviewRecordSummaryArtifact(Record);
	FString SummaryJson;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SummaryJson);
	FJsonSerializer::Serialize(Summary, Writer);
	TestFalse(TEXT("summary does not expose guid field"), SummaryJson.Contains(TEXT("\"guid\"")));
	TestFalse(TEXT("summary does not expose node_guid field"), SummaryJson.Contains(TEXT("node_guid")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewOldLegacyHashRecordNeedsActionTest,
	"BlueprintHelper.Review.Reject.OldLegacyHashRecordNeedsAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewOldLegacyHashRecordNeedsActionTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("RejectLegacyHash"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Node = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("RejectLegacyHashNode"));
	TestNotNull(TEXT("graph node created"), Node);
	if (!Node)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Blueprint->GetPathName();
	Target.GraphName = Graph->GetName();
	Target.TargetKind = TEXT("graph_node");
	Target.TargetKey = FString::Printf(TEXT("graph:%s:node:%s"), *Graph->GetName(), *Node->NodeGuid.ToString(EGuidFormats::Digits));
	Target.VisualGroupKey = Target.TargetKey;
	Target.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
	Target.RecordedAfterHash = TEXT("legacy_graph_v1_hash");
	Target.BaselineHash = TEXT("legacy_graph_v1_baseline");

	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("change_legacy_hash");
	Change.AssetPath = Blueprint->GetPathName();
	Change.LatestEvidenceId = TEXT("tx_legacy_hash");
	Change.AtomicTargets.Add(Target);

	const FBlueprintHelperReviewActionResult Result =
		FBlueprintHelperReviewRejectService::RejectVisibleChangeWithDefaultDispatcher(Change, nullptr);
	TestFalse(TEXT("legacy hash record without recoverable snapshot is not auto-compatible"), Result.bSucceeded);
	TestEqual(TEXT("legacy hash record enters needs action"),
		Result.NewStatus,
		EBlueprintHelperReviewChangeStatus::NeedsAction);
	TestEqual(TEXT("legacy hash drift is diagnostic, not the blocking reason"),
		Result.Message,
		FString(TEXT("missing_recoverable_snapshot")));
	TestEqual(TEXT("legacy hash diagnostic records target key"),
		Result.HashGuardTargetKey,
		Target.TargetKey);
	return true;
}

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
	TestFalse(TEXT("untargeted component text does not route without explicit targets"),
		BlueprintHelperReviewShouldShowInComponents(ComponentChange));
	TestFalse(TEXT("untargeted component text does not route as My Blueprint"),
		BlueprintHelperReviewShouldShowInMyBlueprint(ComponentChange));
	FBlueprintHelperReviewAtomicTarget ComponentTarget;
	ComponentTarget.Surface = EBlueprintHelperReviewSurface::Components;
	ComponentTarget.TargetKind = TEXT("component");
	ComponentTarget.TargetKey = TEXT("component:FakeDiffComponent");
	ComponentChange.AtomicTargets.Add(ComponentTarget);
	TestTrue(TEXT("explicit component target renders in Components"),
		BlueprintHelperReviewShouldShowInComponents(ComponentChange));

	FBlueprintHelperReviewVisibleChange GraphChange;
	GraphChange.GraphName = TEXT("FakeDiffGraph");
	GraphChange.LocationKey = TEXT("graph:FakeDiffGraph/node:PrintString");
	TestFalse(TEXT("untargeted graph text does not route without explicit targets"),
		BlueprintHelperReviewShouldShowInGraph(GraphChange));
	TestFalse(TEXT("untargeted graph text does not route to My Blueprint"),
		BlueprintHelperReviewShouldShowInMyBlueprint(GraphChange));

	FBlueprintHelperReviewAtomicTarget GraphTarget;
	GraphTarget.Surface = EBlueprintHelperReviewSurface::Graph;
	GraphTarget.TargetKey = TEXT("node:PrintString");
	GraphChange.AtomicTargets.Add(GraphTarget);
	TestTrue(TEXT("explicit graph atom renders in the graph page"),
		BlueprintHelperReviewShouldShowInGraph(GraphChange));
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
	TestFalse(TEXT("untargeted signature text does not route to My Blueprint"),
		BlueprintHelperReviewShouldShowInMyBlueprint(SignatureChange));
	TestFalse(TEXT("untargeted signature text does not route to Details"),
		BlueprintHelperReviewShouldShowInDetails(SignatureChange));
	FBlueprintHelperReviewAtomicTarget SignatureTarget;
	SignatureTarget.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
	SignatureTarget.TargetKind = TEXT("signature");
	SignatureTarget.TargetKey = TEXT("signature:FakeDiffFunction");
	SignatureChange.AtomicTargets.Add(SignatureTarget);
	TestTrue(TEXT("explicit signature target renders in My Blueprint"),
		BlueprintHelperReviewShouldShowInMyBlueprint(SignatureChange));
	TestTrue(TEXT("explicit signature target renders in Details"),
		BlueprintHelperReviewShouldShowInDetails(SignatureChange));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewV2RequiresExplicitSurfaceTargetsTest,
	"BlueprintHelper.Review.V2.RequiresExplicitSurfaceTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewV2RequiresExplicitSurfaceTargetsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("change_without_targets");
	Change.DisplayLabel = TEXT("component DoorFrame");
	Change.LocationKey = TEXT("component DoorFrame");
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
	Change.GraphName.Reset();
	Change.AtomicTargets.Reset();

	TestFalse(TEXT("Components surface requires explicit target"),
		BlueprintHelperReviewShouldShowInComponents(Change));
	TestFalse(TEXT("MyBlueprint surface requires explicit target"),
		BlueprintHelperReviewShouldShowInMyBlueprint(Change));
	TestFalse(TEXT("Graph surface requires explicit target"),
		BlueprintHelperReviewShouldShowInGraph(Change));
	TestFalse(TEXT("Details surface requires explicit target"),
		BlueprintHelperReviewShouldShowInDetails(Change));

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
		Args.OnReviewActionIntent = [](const FBlueprintHelperReviewActionIntent&)
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
				TEXT("ReviewRowHighlight change=tx_%s_overlay_0 surface=%s target=\"%s:Target0\" result=pending reason=row_not_visible"),
				*OverlayCase.ChangePrefix,
				*OverlayCase.DebugSurfaceName,
				*OverlayCase.TargetKind));
			bSawRow1 |= Message.Contains(FString::Printf(
				TEXT("ReviewRowHighlight change=tx_%s_overlay_1 surface=%s target=\"%s:Target1\" result=pending reason=row_not_visible"),
				*OverlayCase.ChangePrefix,
				*OverlayCase.DebugSurfaceName,
				*OverlayCase.TargetKind));
		}
		TestTrue(FString::Printf(TEXT("%s overlay logs route debug"), *OverlayCase.DebugSurfaceName), bSawRoute);
		TestFalse(FString::Printf(TEXT("%s overlay does not log fake geometry"), *OverlayCase.DebugSurfaceName), bSawFallbackGeometry);
		TestTrue(FString::Printf(TEXT("%s row highlight logs pending row 0"), *OverlayCase.DebugSurfaceName), bSawRow0);
		TestTrue(FString::Printf(TEXT("%s row highlight logs pending row 1"), *OverlayCase.DebugSurfaceName), bSawRow1);
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
		Args.OnReviewActionIntent = [](const FBlueprintHelperReviewActionIntent&)
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
		bool bSawPendingRowHighlight = false;
		bool bSawShownReviewList = false;
		for (const FString& Message : DebugMessages)
		{
			bSawRoute |= Message.Contains(FString::Printf(
				TEXT("ReviewRoute change=tx_%s_no_geometry surface=%s"),
				*OverlayCase.ChangePrefix,
				*OverlayCase.DebugSurfaceName));
			bSawPendingRowHighlight |= Message.Contains(FString::Printf(
				TEXT("ReviewRowHighlight change=tx_%s_no_geometry surface=%s target=\"%s:Target\" result=pending reason=row_not_visible"),
				*OverlayCase.ChangePrefix,
				*OverlayCase.DebugSurfaceName,
				*OverlayCase.TargetKind));
			bSawShownReviewList |= Message.Contains(TEXT("mode=review_list result=shown"));
		}
		TestTrue(FString::Printf(TEXT("%s overlay logs route debug"), *OverlayCase.DebugSurfaceName), bSawRoute);
		TestTrue(FString::Printf(TEXT("%s overlay logs pending no-geometry state"), *OverlayCase.DebugSurfaceName), bSawPendingRowHighlight);
		TestFalse(FString::Printf(TEXT("%s overlay does not show text review-list fallback"), *OverlayCase.DebugSurfaceName), bSawShownReviewList);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewNonGraphPanelsDoNotUseAnchorOverlayTest,
	"BlueprintHelper.Review.UI.NonGraphPanelsDoNotUseAnchorOverlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewNonGraphPanelsDoNotUseAnchorOverlayTest::RunTest(const FString& Parameters)
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
			TEXT("component_row_highlight"),
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
			TEXT("myblueprint_row_highlight"),
			TEXT("blueprint_variable"),
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
			TEXT("details_row_highlight"),
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
			TEXT("umg_row_highlight"),
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
		Change.ChangeId = FString::Printf(TEXT("tx_%s"), *OverlayCase.ChangePrefix);
		Change.AssetPath = TEXT("/Game/BlueprintHelper/Smoke/ReviewAsset");
		Change.DisplayLabel = TEXT("SmokeTarget");
		Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;

		FBlueprintHelperReviewAtomicTarget Target;
		Target.Surface = OverlayCase.Surface;
		Target.TargetKind = OverlayCase.TargetKind;
		Target.TargetKey = FString::Printf(TEXT("%s:SmokeTarget"), *OverlayCase.TargetKind);
		Target.DisplayLabel = TEXT("SmokeTarget");
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
		Args.OnReviewActionIntent = [](const FBlueprintHelperReviewActionIntent&)
		{
			return FReply::Handled();
		};
		Args.GetChangeColor = [](EBlueprintHelperReviewChangeKind)
		{
			return FSlateColor(FLinearColor::Yellow);
		};
		Args.GetSelectedDiffColor = []()
		{
			return FSlateColor(FLinearColor::Yellow);
		};
		Args.ResolveRowGeometry.BindLambda([](
			const FBlueprintHelperReviewVisibleChange& ChangeToResolve,
			EBlueprintHelperReviewSurface,
			FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
		{
			OutAnchor.bIsValid = true;
			OutAnchor.Position = FVector2D(12.0f, 24.0f);
			OutAnchor.Size = FVector2D(260.0f, 32.0f);
			OutAnchor.TargetText = ChangeToResolve.DisplayLabel;
			OutAnchor.Reason = TEXT("test_stable_row");
			return true;
		});

		TSharedRef<SWidget> Overlay = OverlayCase.BuildOverlay(Args);
		TestTrue(FString::Printf(TEXT("%s row highlight does not render overlay widget"), *OverlayCase.DebugSurfaceName),
			&Overlay.Get() == &SNullWidget::NullWidget.Get());

		bool bSawRowHighlight = false;
		bool bSawFrameGeometry = false;
		bool bSawReviewList = false;
		for (const FString& Message : DebugMessages)
		{
			bSawRowHighlight |= Message.Contains(FString::Printf(
				TEXT("ReviewRowHighlight change=tx_%s surface=%s target=\"%s:SmokeTarget\" result=shown mode=row_background"),
				*OverlayCase.ChangePrefix,
				*OverlayCase.DebugSurfaceName,
				*OverlayCase.TargetKind));
			bSawFrameGeometry |= Message.Contains(TEXT("ReviewFrameGeometry"));
			bSawReviewList |= Message.Contains(TEXT("mode=review_list"));
		}

		TestTrue(FString::Printf(TEXT("%s logs row background highlight"), *OverlayCase.DebugSurfaceName), bSawRowHighlight);
		TestFalse(FString::Printf(TEXT("%s does not log frame geometry"), *OverlayCase.DebugSurfaceName), bSawFrameGeometry);
		TestFalse(FString::Printf(TEXT("%s does not show review-list fallback"), *OverlayCase.DebugSurfaceName), bSawReviewList);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewMissingRowLogsRowHighlightPendingTest,
	"BlueprintHelper.Review.UI.MissingRowLogsRowHighlightPending",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewMissingRowLogsRowHighlightPendingTest::RunTest(const FString& Parameters)
{
	TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Items;
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_myblueprint_missing_row_highlight");
	Change.AssetPath = TEXT("/Game/BlueprintHelper/Smoke/ReviewAsset");
	Change.DisplayLabel = TEXT("SmokeHP");
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;

	FBlueprintHelperReviewAtomicTarget Target;
	Target.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
	Target.TargetKind = TEXT("blueprint_variable");
	Target.TargetKey = TEXT("blueprint_variable:SmokeHP");
	Target.DisplayLabel = TEXT("SmokeHP");
	Change.AtomicTargets.Add(Target);
	Items.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(Change));

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
	Args.GetChangeColor = [](EBlueprintHelperReviewChangeKind)
	{
		return FSlateColor(FLinearColor::Yellow);
	};
	Args.GetSelectedDiffColor = []()
	{
		return FSlateColor(FLinearColor::Yellow);
	};
	Args.ResolveRowGeometry.BindLambda([](
		const FBlueprintHelperReviewVisibleChange&,
		EBlueprintHelperReviewSurface,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
	{
		OutAnchor.Reason = TEXT("row_not_visible");
		return false;
	});

	TSharedRef<SWidget> Overlay = FBlueprintHelperReviewMyBlueprintPresenter::BuildOverlay(Args);
	TestTrue(TEXT("missing row does not render overlay widget"),
		&Overlay.Get() == &SNullWidget::NullWidget.Get());

	bool bSawPendingRowHighlight = false;
	bool bSawReviewList = false;
	for (const FString& Message : DebugMessages)
	{
		bSawPendingRowHighlight |= Message.Contains(
			TEXT("ReviewRowHighlight change=tx_myblueprint_missing_row_highlight surface=MyBlueprint target=\"blueprint_variable:SmokeHP\" result=pending reason=row_not_visible"));
		bSawReviewList |= Message.Contains(TEXT("mode=review_list"));
	}

	TestTrue(TEXT("missing row logs row highlight pending"), bSawPendingRowHighlight);
	TestFalse(TEXT("missing row does not render review-list fallback"), bSawReviewList);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewDebugDedupesRepeatedGeometryPendingMessagesTest,
	"BlueprintHelper.Review.UI.DebugDedupesRepeatedGeometryPendingMessages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewDebugDedupesRepeatedGeometryPendingMessagesTest::RunTest(const FString& Parameters)
{
	TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Items;
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_dedupe_row_highlight_pending");
	Change.AssetPath = TEXT("/Game/BlueprintHelper/Smoke/ReviewAsset");
	Change.DisplayLabel = TEXT("SmokeHP");
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;

	FBlueprintHelperReviewAtomicTarget Target;
	Target.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
	Target.TargetKind = TEXT("blueprint_variable");
	Target.TargetKey = TEXT("blueprint_variable:SmokeHP");
	Target.DisplayLabel = TEXT("SmokeHP");
	Change.AtomicTargets.Add(Target);
	Items.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(Change));

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
	Args.GetChangeColor = [](EBlueprintHelperReviewChangeKind)
	{
		return FSlateColor(FLinearColor::Yellow);
	};
	Args.GetSelectedDiffColor = []()
	{
		return FSlateColor(FLinearColor::Yellow);
	};
	Args.ResolveRowGeometry.BindLambda([](
		const FBlueprintHelperReviewVisibleChange&,
		EBlueprintHelperReviewSurface,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
	{
		OutAnchor.Reason = TEXT("row_not_visible");
		return false;
	});

	FBlueprintHelperReviewMyBlueprintPresenter::BuildOverlay(Args);
	FBlueprintHelperReviewMyBlueprintPresenter::BuildOverlay(Args);

	int32 PendingMessageCount = 0;
	for (const FString& Message : DebugMessages)
	{
		if (Message.Contains(
			TEXT("ReviewRowHighlight change=tx_dedupe_row_highlight_pending surface=MyBlueprint target=\"blueprint_variable:SmokeHP\" result=pending reason=row_not_visible")))
		{
			++PendingMessageCount;
		}
	}

	TestEqual(TEXT("duplicate pending row highlight debug message is emitted once"), PendingMessageCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRowHighlightSkipsUnchangedStateBroadcastTest,
	"BlueprintHelper.Review.UI.RowHighlightSkipsUnchangedStateBroadcast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRowHighlightSkipsUnchangedStateBroadcastTest::RunTest(const FString& Parameters)
{
	const FString AssetPath = TEXT("/Game/BlueprintHelper/Smoke/ReviewAssetRevisionLoopGuard");
	TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Items;
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_revision_loop_guard");
	Change.AssetPath = AssetPath;
	Change.DisplayLabel = TEXT("SmokeHP");
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;

	FBlueprintHelperReviewAtomicTarget Target;
	Target.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
	Target.TargetKind = TEXT("blueprint_variable");
	Target.TargetKey = TEXT("blueprint_variable:SmokeHP");
	Target.DisplayLabel = TEXT("SmokeHP");
	Change.AtomicTargets.Add(Target);
	Items.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(Change));

	FBlueprintHelperReviewAssetContext Context;
	Context.AssetPath = AssetPath;
	Context.AssetKind = EBlueprintHelperReviewAssetKind::Blueprint;
	FBlueprintHelperReviewPanelSurfacePresenterArgs Args;
	Args.AssetContext = &Context;
	Args.ChangeItems = &Items;
	Args.SelectedChange = Items[0];
	Args.GetChangeColor = [](EBlueprintHelperReviewChangeKind)
	{
		return FSlateColor(FLinearColor::Yellow);
	};

	int32 BroadcastCount = 0;
	FDelegateHandle Handle = FBlueprintHelperReviewRowHighlightModel::AddStateChangedHandler(
		FBlueprintHelperReviewRowHighlightStateChanged::FDelegate::CreateLambda(
			[&BroadcastCount, &AssetPath](
				const FString& ChangedAssetPath,
				EBlueprintHelperReviewSurface Surface,
				uint64)
			{
				if (ChangedAssetPath == AssetPath && Surface == EBlueprintHelperReviewSurface::MyBlueprint)
				{
					++BroadcastCount;
				}
			}));

	FBlueprintHelperReviewRowHighlightModel::RebuildSurfaceState(
		Args,
		EBlueprintHelperReviewSurface::MyBlueprint,
		&FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange,
		AssetPath);
	FBlueprintHelperReviewRowHighlightModel::RebuildSurfaceState(
		Args,
		EBlueprintHelperReviewSurface::MyBlueprint,
		&FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange,
		AssetPath);
	FBlueprintHelperReviewRowHighlightModel::RemoveStateChangedHandler(Handle);

	TestEqual(TEXT("unchanged row highlight state broadcasts once"), BroadcastCount, 1);
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
		Args.OnReviewActionIntent = [](const FBlueprintHelperReviewActionIntent&)
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
		TestTrue(FString::Printf(TEXT("%s row highlight overlay remains widget-free"), *OverlayCase.DebugSurfaceName),
			&Overlay.Get() == &SNullWidget::NullWidget.Get());

		bool bSawRow0 = false;
		bool bSawRow1 = false;
		bool bSawReviewList = false;
		for (const FString& Message : DebugMessages)
		{
			bSawReviewList |= Message.Contains(TEXT("mode=review_list"));
			bSawRow0 |= Message.Contains(FString::Printf(
				TEXT("ReviewRowHighlight change=tx_%s_slate_row_0 surface=%s target=\"%s:Target0\" result=shown mode=row_background"),
				*OverlayCase.ChangePrefix,
				*OverlayCase.DebugSurfaceName,
				*OverlayCase.TargetKind));
			bSawRow1 |= Message.Contains(FString::Printf(
				TEXT("ReviewRowHighlight change=tx_%s_slate_row_1 surface=%s target=\"%s:Target1\" result=shown mode=row_background"),
				*OverlayCase.ChangePrefix,
				*OverlayCase.DebugSurfaceName,
				*OverlayCase.TargetKind));
		}

		TestFalse(FString::Printf(TEXT("%s stable geometry does not fall back to review-list"), *OverlayCase.DebugSurfaceName), bSawReviewList);
		TestTrue(FString::Printf(TEXT("%s logs row background highlight 0"), *OverlayCase.DebugSurfaceName), bSawRow0);
		TestTrue(FString::Printf(TEXT("%s logs row background highlight 1"), *OverlayCase.DebugSurfaceName), bSawRow1);
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
	FBlueprintHelperReviewRowHighlightAlphaIsPointSixTest,
	"BlueprintHelper.Review.UI.RowHighlightAlphaIsPointSix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRowHighlightAlphaIsPointSixTest::RunTest(const FString& Parameters)
{
	const FLinearColor SourceColor(0.05f, 0.75f, 0.22f, 1.0f);
	const FLinearColor FillColor =
		FBlueprintHelperReviewRowHighlightModel::GetRowHighlightFillColor(SourceColor);

	TestTrue(TEXT("row highlight fill alpha is exactly 0.6"),
		FMath::IsNearlyEqual(FillColor.A, 0.6f));
	TestTrue(TEXT("row highlight preserves change color channels"),
		FMath::IsNearlyEqual(FillColor.R, SourceColor.R)
		&& FMath::IsNearlyEqual(FillColor.G, SourceColor.G)
		&& FMath::IsNearlyEqual(FillColor.B, SourceColor.B));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewSelectedRowShowsAcceptRejectActionsTest,
	"BlueprintHelper.Review.UI.SelectedRowShowsAcceptRejectActions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewSelectedRowShowsAcceptRejectActionsTest::RunTest(const FString& Parameters)
{
	TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Items;
	for (const TCHAR* Name : {TEXT("SmokeHP"), TEXT("SmokeMP")})
	{
		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = FString::Printf(TEXT("tx_action_%s"), Name);
		Change.AssetPath = TEXT("/Game/BlueprintHelper/Smoke/ReviewAsset");
		Change.DisplayLabel = Name;
		Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
		FBlueprintHelperReviewAtomicTarget Target;
		Target.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
		Target.TargetKind = TEXT("blueprint_variable");
		Target.TargetKey = FString::Printf(TEXT("blueprint_variable:%s"), Name);
		Target.DisplayLabel = Name;
		Change.AtomicTargets.Add(Target);
		Items.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(Change));
	}

	FBlueprintHelperReviewAssetContext Context;
	Context.AssetPath = TEXT("/Game/BlueprintHelper/Smoke/ReviewAsset");
	Context.AssetKind = EBlueprintHelperReviewAssetKind::Blueprint;
	FBlueprintHelperReviewPanelSurfacePresenterArgs Args;
	Args.AssetContext = &Context;
	Args.ChangeItems = &Items;
	Args.SelectedChange = Items[0];
	Args.GetChangeColor = [](EBlueprintHelperReviewChangeKind)
	{
		return FSlateColor(FLinearColor::Yellow);
	};
	Args.ResolveRowGeometry.BindLambda([](
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
	{
		OutAnchor.bIsValid = true;
		OutAnchor.TargetText = Change.DisplayLabel;
		OutAnchor.Size = FVector2D(200.0f, 24.0f);
		return true;
	});

	FBlueprintHelperReviewMyBlueprintPresenter::BuildOverlay(Args);

	TestTrue(TEXT("selected row exposes actions"),
		FBlueprintHelperReviewRowHighlightModel::GetRowActionsVisibility(
			Context.AssetPath,
			EBlueprintHelperReviewSurface::MyBlueprint,
			TEXT("blueprint_variable:SmokeHP")) == EVisibility::Visible);
	TestTrue(TEXT("unselected row hides actions"),
		FBlueprintHelperReviewRowHighlightModel::GetRowActionsVisibility(
			Context.AssetPath,
			EBlueprintHelperReviewSurface::MyBlueprint,
			TEXT("blueprint_variable:SmokeMP")) == EVisibility::Collapsed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRowActionsRemainSelectedChangeOnlyTest,
	"BlueprintHelper.Review.UI.RowActionsRemainSelectedChangeOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRowActionsRemainSelectedChangeOnlyTest::RunTest(const FString& Parameters)
{
	const FString AssetPath = TEXT("/Game/BlueprintHelper/Smoke/ReviewAssetSelection");
	TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Items;
	auto AddVariableChange = [&Items, &AssetPath](const FString& Name)
	{
		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = FString::Printf(TEXT("tx_action_reselect_%s"), *Name);
		Change.AssetPath = AssetPath;
		Change.DisplayLabel = Name;
		Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;

		FBlueprintHelperReviewAtomicTarget Target;
		Target.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
		Target.TargetKind = TEXT("blueprint_variable");
		Target.TargetKey = FString::Printf(TEXT("blueprint_variable:%s"), *Name);
		Target.DisplayLabel = Name;
		Change.AtomicTargets.Add(Target);
		Items.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(Change));
	};
	AddVariableChange(TEXT("SmokeHP"));
	AddVariableChange(TEXT("SmokeMP"));

	FBlueprintHelperReviewAssetContext Context;
	Context.AssetPath = AssetPath;
	Context.AssetKind = EBlueprintHelperReviewAssetKind::Blueprint;

	auto BuildForSelected = [&Context, &Items](const TSharedPtr<FBlueprintHelperReviewVisibleChange>& SelectedChange)
	{
		FBlueprintHelperReviewPanelSurfacePresenterArgs Args;
		Args.AssetContext = &Context;
		Args.ChangeItems = &Items;
		Args.SelectedChange = SelectedChange;
		Args.GetChangeColor = [](EBlueprintHelperReviewChangeKind)
		{
			return FSlateColor(FLinearColor::Yellow);
		};
		FBlueprintHelperReviewMyBlueprintPresenter::BuildOverlay(Args);
	};

	BuildForSelected(Items[0]);
	TestTrue(TEXT("first selected row exposes actions"),
		FBlueprintHelperReviewRowHighlightModel::GetRowActionsVisibility(
			AssetPath,
			EBlueprintHelperReviewSurface::MyBlueprint,
			TEXT("blueprint_variable:SmokeHP")) == EVisibility::Visible);
	TestTrue(TEXT("second row hides actions before reselection"),
		FBlueprintHelperReviewRowHighlightModel::GetRowActionsVisibility(
			AssetPath,
			EBlueprintHelperReviewSurface::MyBlueprint,
			TEXT("blueprint_variable:SmokeMP")) == EVisibility::Collapsed);

	BuildForSelected(Items[1]);
	TestTrue(TEXT("previously selected row hides actions after reselection"),
		FBlueprintHelperReviewRowHighlightModel::GetRowActionsVisibility(
			AssetPath,
			EBlueprintHelperReviewSurface::MyBlueprint,
			TEXT("blueprint_variable:SmokeHP")) == EVisibility::Collapsed);
	TestTrue(TEXT("newly selected row exposes actions after reselection"),
		FBlueprintHelperReviewRowHighlightModel::GetRowActionsVisibility(
			AssetPath,
			EBlueprintHelperReviewSurface::MyBlueprint,
			TEXT("blueprint_variable:SmokeMP")) == EVisibility::Visible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewEmptySearchTextDoesNotMatchAnyReviewTargetTest,
	"BlueprintHelper.Review.UI.EmptySearchTextDoesNotMatchAnyReviewTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewEmptySearchTextDoesNotMatchAnyReviewTargetTest::RunTest(const FString& Parameters)
{
	TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Items;
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_empty_search_guard");
	Change.AssetPath = TEXT("/Game/BlueprintHelper/Smoke/ReviewAsset");
	Change.DisplayLabel = TEXT("SmokeHP");
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	FBlueprintHelperReviewAtomicTarget Target;
	Target.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
	Target.TargetKind = TEXT("blueprint_variable");
	Target.TargetKey = TEXT("blueprint_variable:SmokeHP");
	Target.DisplayLabel = TEXT("SmokeHP");
	Change.AtomicTargets.Add(Target);
	Items.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(Change));

	FBlueprintHelperReviewAssetContext Context;
	Context.AssetPath = Change.AssetPath;
	Context.AssetKind = EBlueprintHelperReviewAssetKind::Blueprint;
	FBlueprintHelperReviewPanelSurfacePresenterArgs Args;
	Args.AssetContext = &Context;
	Args.ChangeItems = &Items;
	Args.SelectedChange = Items[0];
	Args.GetChangeColor = [](EBlueprintHelperReviewChangeKind)
	{
		return FSlateColor(FLinearColor::Yellow);
	};
	FBlueprintHelperReviewMyBlueprintPresenter::BuildOverlay(Args);

	TestTrue(TEXT("empty row search text has transparent background"),
		FBlueprintHelperReviewRowHighlightModel::GetRowBackgroundColor(
			Context.AssetPath,
			EBlueprintHelperReviewSurface::MyBlueprint,
			FString()).GetSpecifiedColor().Equals(FLinearColor::Transparent));
	TestTrue(TEXT("empty row search text has no actions"),
		FBlueprintHelperReviewRowHighlightModel::GetRowActionsVisibility(
			Context.AssetPath,
			EBlueprintHelperReviewSurface::MyBlueprint,
			FString()) == EVisibility::Collapsed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewSectionRowsNeverHighlightFromEmptySearchTextTest,
	"BlueprintHelper.Review.UI.SectionRowsNeverHighlightFromEmptySearchText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewSectionRowsNeverHighlightFromEmptySearchTextTest::RunTest(const FString& Parameters)
{
	const FString AssetPath = TEXT("/Game/BlueprintHelper/Smoke/ReviewAssetSection");
	TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Items;
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_section_guard");
	Change.AssetPath = AssetPath;
	Change.DisplayLabel = TEXT("SmokeHP");
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	FBlueprintHelperReviewAtomicTarget Target;
	Target.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
	Target.TargetKind = TEXT("blueprint_variable");
	Target.TargetKey = TEXT("blueprint_variable:SmokeHP");
	Change.AtomicTargets.Add(Target);
	Items.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(Change));

	FBlueprintHelperReviewAssetContext Context;
	Context.AssetPath = AssetPath;
	Context.AssetKind = EBlueprintHelperReviewAssetKind::Blueprint;
	FBlueprintHelperReviewPanelSurfacePresenterArgs Args;
	Args.AssetContext = &Context;
	Args.ChangeItems = &Items;
	Args.SelectedChange = Items[0];
	Args.GetChangeColor = [](EBlueprintHelperReviewChangeKind)
	{
		return FSlateColor(FLinearColor::Yellow);
	};
	FBlueprintHelperReviewMyBlueprintPresenter::BuildOverlay(Args);

	TestTrue(TEXT("section rows use empty search text and remain transparent to review matching"),
		FBlueprintHelperReviewRowHighlightModel::GetRowBackgroundColor(
			AssetPath,
			EBlueprintHelperReviewSurface::MyBlueprint,
			FString()).GetSpecifiedColor().Equals(FLinearColor::Transparent));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAssetRootSelectionHighlightsSameAssetRowsTest,
	"BlueprintHelper.Review.UI.AssetRootSelectionHighlightsSameAssetRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAssetRootSelectionHighlightsSameAssetRowsTest::RunTest(const FString& Parameters)
{
	const FString AssetPath = TEXT("/Game/BlueprintHelper/Smoke/BP_SmokeActor");
	TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Items;

	FBlueprintHelperReviewVisibleChange RootChange;
	RootChange.ChangeId = TEXT("tx_asset_root");
	RootChange.AssetPath = AssetPath;
	RootChange.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
	RootChange.bIsAssetLifecycleRoot = true;
	FBlueprintHelperReviewAtomicTarget RootTarget;
	RootTarget.Surface = EBlueprintHelperReviewSurface::Details;
	RootTarget.TargetKind = TEXT("asset_factory");
	RootTarget.TargetKey = TEXT("asset_factory:create_asset");
	RootTarget.AssetPath = AssetPath;
	RootChange.AtomicTargets.Add(RootTarget);
	Items.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(RootChange));

	FBlueprintHelperReviewVisibleChange VariableChange;
	VariableChange.ChangeId = TEXT("tx_same_asset_variable");
	VariableChange.AssetPath = AssetPath;
	VariableChange.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	FBlueprintHelperReviewAtomicTarget VariableTarget;
	VariableTarget.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
	VariableTarget.TargetKind = TEXT("blueprint_variable");
	VariableTarget.TargetKey = TEXT("blueprint_variable:SmokeHP");
	VariableTarget.DisplayLabel = TEXT("SmokeHP");
	VariableChange.AtomicTargets.Add(VariableTarget);
	Items.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(VariableChange));

	FBlueprintHelperReviewVisibleChange OtherAssetChange = VariableChange;
	OtherAssetChange.ChangeId = TEXT("tx_other_asset_variable");
	OtherAssetChange.AssetPath = TEXT("/Game/BlueprintHelper/Smoke/BP_OtherActor");
	Items.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(OtherAssetChange));

	FBlueprintHelperReviewAssetContext Context;
	Context.AssetPath = AssetPath;
	Context.AssetKind = EBlueprintHelperReviewAssetKind::Blueprint;
	FBlueprintHelperReviewPanelSurfacePresenterArgs Args;
	Args.AssetContext = &Context;
	Args.ChangeItems = &Items;
	Args.SelectedChange = Items[0];
	Args.GetChangeColor = [](EBlueprintHelperReviewChangeKind)
	{
		return FSlateColor(FLinearColor::Yellow);
	};
	FBlueprintHelperReviewMyBlueprintPresenter::BuildOverlay(Args);

	TestTrue(TEXT("selected asset root still highlights same-asset MyBlueprint rows"),
		FBlueprintHelperReviewRowHighlightModel::GetRowBackgroundColor(
			AssetPath,
			EBlueprintHelperReviewSurface::MyBlueprint,
			TEXT("SmokeHP")).GetSpecifiedColor().A > 0.0f);
	TestTrue(TEXT("same-asset child row actions stay hidden because root is selected"),
		FBlueprintHelperReviewRowHighlightModel::GetRowActionsVisibility(
			AssetPath,
			EBlueprintHelperReviewSurface::MyBlueprint,
			TEXT("SmokeHP")) == EVisibility::Collapsed);
	TestTrue(TEXT("other asset has no highlight state"),
		FBlueprintHelperReviewRowHighlightModel::GetRowBackgroundColor(
			TEXT("/Game/BlueprintHelper/Smoke/BP_OtherActor"),
			EBlueprintHelperReviewSurface::MyBlueprint,
			TEXT("SmokeHP")).GetSpecifiedColor().Equals(FLinearColor::Transparent));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewWidgetTreeAssetRootSelectionHighlightsWidgetRowsTest,
	"BlueprintHelper.Review.UI.WidgetTreeAssetRootSelectionHighlightsWidgetRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewWidgetTreeAssetRootSelectionHighlightsWidgetRowsTest::RunTest(const FString& Parameters)
{
	const FString AssetPath = TEXT("/Game/BlueprintHelper/Smoke/WBP_Smoke");
	TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Items;

	FBlueprintHelperReviewVisibleChange RootChange;
	RootChange.ChangeId = TEXT("tx_widget_asset_root");
	RootChange.AssetPath = AssetPath;
	RootChange.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
	RootChange.bIsAssetLifecycleRoot = true;
	FBlueprintHelperReviewAtomicTarget RootTarget;
	RootTarget.Surface = EBlueprintHelperReviewSurface::Details;
	RootTarget.TargetKind = TEXT("asset_factory");
	RootTarget.TargetKey = TEXT("asset_factory:create_asset");
	RootTarget.AssetPath = AssetPath;
	RootChange.AtomicTargets.Add(RootTarget);
	Items.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(RootChange));

	FBlueprintHelperReviewVisibleChange WidgetChange;
	WidgetChange.ChangeId = TEXT("tx_same_widget_tree_row");
	WidgetChange.AssetPath = AssetPath;
	WidgetChange.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	FBlueprintHelperReviewAtomicTarget WidgetTarget;
	WidgetTarget.Surface = EBlueprintHelperReviewSurface::UMGWidgetTree;
	WidgetTarget.TargetKind = TEXT("umg_widget");
	WidgetTarget.TargetKey = TEXT("umg_widget:SmokeText");
	WidgetTarget.DisplayLabel = TEXT("SmokeText");
	WidgetChange.AtomicTargets.Add(WidgetTarget);
	Items.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(WidgetChange));

	FBlueprintHelperReviewAssetContext Context;
	Context.AssetPath = AssetPath;
	Context.AssetKind = EBlueprintHelperReviewAssetKind::WidgetBlueprint;
	FBlueprintHelperReviewPanelSurfacePresenterArgs Args;
	Args.AssetContext = &Context;
	Args.ChangeItems = &Items;
	Args.SelectedChange = Items[0];
	Args.GetChangeColor = [](EBlueprintHelperReviewChangeKind)
	{
		return FSlateColor(FLinearColor::Yellow);
	};
	FBlueprintHelperReviewUMGWidgetTreePresenter::BuildOverlay(Args);

	TestTrue(TEXT("selected Widget Blueprint asset root still highlights same-asset widget rows"),
		FBlueprintHelperReviewRowHighlightModel::GetRowBackgroundColor(
			AssetPath,
			EBlueprintHelperReviewSurface::UMGWidgetTree,
			TEXT("SmokeText")).GetSpecifiedColor().A > 0.0f);
	TestTrue(TEXT("same-asset widget row actions stay hidden because root is selected"),
		FBlueprintHelperReviewRowHighlightModel::GetRowActionsVisibility(
			AssetPath,
			EBlueprintHelperReviewSurface::UMGWidgetTree,
			TEXT("SmokeText")) == EVisibility::Collapsed);
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
	FBlueprintHelperReviewAssetFactoryTitleStripsPackagePrefixTest,
	"BlueprintHelper.Review.UI.AssetFactoryTitleStripsPackagePrefix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAssetFactoryTitleStripsPackagePrefixTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
	Change.AssetPath = TEXT("/Game/BlueprintHelper/Smoke/BP_SmokeActor");
	Change.bIsAssetLifecycleRoot = true;
	FBlueprintHelperReviewAtomicTarget Target;
	Target.Surface = EBlueprintHelperReviewSurface::Details;
	Target.TargetKind = TEXT("asset_factory");
	Target.TargetKey = TEXT("asset_factory:_Game_BlueprintHelper_Smoke_BP_SmokeActor");
	Target.DisplayLabel = TEXT("_Game_BlueprintHelper_Smoke_BP_SmokeActor");
	Change.AtomicTargets.Add(Target);

	const FString Title = FBlueprintHelperReviewSurfaceFrameBuilder::BuildReadableChangeTitle(Change);
	TestTrue(TEXT("asset factory title uses short asset name"), Title.Contains(TEXT("[BP_SmokeActor]")));
	TestFalse(TEXT("asset factory title strips encoded package prefix"),
		Title.Contains(TEXT("_Game_BlueprintHelper_Smoke")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAssetFactoryTitleUsesAssetKindSuffixTest,
	"BlueprintHelper.Review.UI.AssetFactoryTitleUsesAssetKindSuffix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAssetFactoryTitleUsesAssetKindSuffixTest::RunTest(const FString& Parameters)
{
	struct FCase
	{
		FString AssetPath;
		FString ExpectedSuffix;
	};
	const TArray<FCase> Cases =
	{
		{TEXT("/Game/BlueprintHelper/Smoke/BP_SmokeActor"), TEXT("\u84dd\u56fe\u8d44\u4ea7")},
		{TEXT("/Game/BlueprintHelper/Smoke/WBP_Smoke"), TEXT("Widget Blueprint \u8d44\u4ea7")},
		{TEXT("/Game/BlueprintHelper/Smoke/DT_Smoke"), TEXT("DataTable \u8d44\u4ea7")},
		{TEXT("/Game/BlueprintHelper/Smoke/ST_SmokeRow"), TEXT("Structure \u8d44\u4ea7")}
	};

	for (const FCase& Case : Cases)
	{
		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
		Change.AssetPath = Case.AssetPath;
		Change.bIsAssetLifecycleRoot = true;
		FBlueprintHelperReviewAtomicTarget Target;
		Target.Surface = EBlueprintHelperReviewSurface::Details;
		Target.TargetKind = TEXT("asset_factory");
		Target.TargetKey = TEXT("asset_factory:create_asset");
		Change.AtomicTargets.Add(Target);

		const FString Title = FBlueprintHelperReviewSurfaceFrameBuilder::BuildReadableChangeTitle(Change);
		TestTrue(FString::Printf(TEXT("%s uses asset-kind suffix"), *Case.AssetPath),
			Title.EndsWith(Case.ExpectedSuffix));
		TestFalse(FString::Printf(TEXT("%s asset factory title is not a variable"), *Case.AssetPath),
			Title.EndsWith(TEXT("\u53d8\u91cf")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewMyBlueprintPresenterBuildsNativeParitySectionsTest,
	"BlueprintHelper.Review.UI.MyBlueprintPresenterBuildsNativeParitySections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewMyBlueprintPresenterBuildsNativeParitySectionsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("MyBlueprintSections"));
	if (!Blueprint)
	{
		return false;
	}

	UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		TEXT("BH_SmokeFunc"),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, FunctionGraph, true, nullptr);

	UEdGraph* MacroGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		TEXT("BH_SmokeMacro"),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddMacroGraph(Blueprint, MacroGraph, true, nullptr);

	FEdGraphPinType IntPinType;
	IntPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("SmokeHP"), IntPinType);

	FBPVariableDescription DispatcherDescription;
	DispatcherDescription.VarName = TEXT("OnSmoke");
	DispatcherDescription.VarType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;
	Blueprint->NewVariables.Add(DispatcherDescription);

	FBlueprintHelperReviewAssetContext Context;
	Context.AssetPath = Blueprint->GetOutermost()->GetName();
	Context.AssetKind = EBlueprintHelperReviewAssetKind::Blueprint;
	Context.AssetObject = Blueprint;
	Context.Blueprint = Blueprint;

	TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Changes;
	FBlueprintHelperReviewMyBlueprintPresenter::FState State;
	TSharedRef<SWidget> Content =
		FBlueprintHelperReviewMyBlueprintPresenter::BuildContent(Context, State, Changes);
	TestTrue(TEXT("content widget builds"), Content != SNullWidget::NullWidget);

	TSet<FString> Sections;
	TMap<FString, TArray<FString>> SectionChildren;
	for (const TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>& Root : State.RootItems)
	{
		if (!Root.IsValid())
		{
			continue;
		}
		const FString SectionName = Root->Label.ToString();
		Sections.Add(SectionName);
		for (const TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>& Child : Root->Children)
		{
			if (Child.IsValid())
			{
				SectionChildren.FindOrAdd(SectionName).Add(Child->Label.ToString());
			}
		}
	}

	TestTrue(TEXT("Graphs section exists"), Sections.Contains(TEXT("Graphs")));
	TestTrue(TEXT("Functions section exists"), Sections.Contains(TEXT("Functions")));
	TestTrue(TEXT("Macros section exists"), Sections.Contains(TEXT("Macros")));
	TestTrue(TEXT("Variables section exists"), Sections.Contains(TEXT("Variables")));
	TestTrue(TEXT("Event Dispatchers section exists"), Sections.Contains(TEXT("Event Dispatchers")));
	TestTrue(TEXT("function graph appears under Functions"),
		SectionChildren.FindRef(TEXT("Functions")).Contains(TEXT("BH_SmokeFunc")));
	TestTrue(TEXT("macro graph appears under Macros"),
		SectionChildren.FindRef(TEXT("Macros")).Contains(TEXT("BH_SmokeMacro")));
	TestTrue(TEXT("variable appears under Variables"),
		SectionChildren.FindRef(TEXT("Variables")).Contains(TEXT("SmokeHP")));
	TestTrue(TEXT("dispatcher appears under Event Dispatchers"),
		SectionChildren.FindRef(TEXT("Event Dispatchers")).Contains(TEXT("OnSmoke")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewMyBlueprintPresenterClassifiesFunctionsMacrosVariablesDispatchersTest,
	"BlueprintHelper.Review.UI.MyBlueprintPresenterClassifiesFunctionsMacrosVariablesDispatchers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewMyBlueprintPresenterClassifiesFunctionsMacrosVariablesDispatchersTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("MyBlueprintClassification"));
	if (!Blueprint)
	{
		return false;
	}

	UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		TEXT("BH_ClassifiedFunc"),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, FunctionGraph, true, nullptr);

	UEdGraph* MacroGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		TEXT("BH_ClassifiedMacro"),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddMacroGraph(Blueprint, MacroGraph, true, nullptr);

	FEdGraphPinType IntPinType;
	IntPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("ClassifiedHP"), IntPinType);

	FBPVariableDescription DispatcherDescription;
	DispatcherDescription.VarName = TEXT("OnClassified");
	DispatcherDescription.VarType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;
	Blueprint->NewVariables.Add(DispatcherDescription);

	FBlueprintHelperReviewAssetContext Context;
	Context.AssetPath = Blueprint->GetOutermost()->GetName();
	Context.AssetKind = EBlueprintHelperReviewAssetKind::Blueprint;
	Context.AssetObject = Blueprint;
	Context.Blueprint = Blueprint;

	TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Changes;
	FBlueprintHelperReviewMyBlueprintPresenter::FState State;
	FBlueprintHelperReviewMyBlueprintPresenter::BuildContent(Context, State, Changes);

	auto FindChild = [&State](
		const FString& SectionName,
		const FString& ChildName) -> TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>
	{
		for (const TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>& Root : State.RootItems)
		{
			if (!Root.IsValid() || Root->Label.ToString() != SectionName)
			{
				continue;
			}
			for (const TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>& Child : Root->Children)
			{
				if (Child.IsValid() && Child->Label.ToString() == ChildName)
				{
					return Child;
				}
			}
		}
		return nullptr;
	};

	const TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem> FunctionRow =
		FindChild(TEXT("Functions"), TEXT("BH_ClassifiedFunc"));
	const TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem> MacroRow =
		FindChild(TEXT("Macros"), TEXT("BH_ClassifiedMacro"));
	const TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem> VariableRow =
		FindChild(TEXT("Variables"), TEXT("ClassifiedHP"));
	const TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem> DispatcherRow =
		FindChild(TEXT("Event Dispatchers"), TEXT("OnClassified"));

	TestTrue(TEXT("function row kind"), FunctionRow.IsValid()
		&& FunctionRow->Kind == FBlueprintHelperReviewMyBlueprintPresenter::ERowKind::Function);
	TestTrue(TEXT("macro row kind"), MacroRow.IsValid()
		&& MacroRow->Kind == FBlueprintHelperReviewMyBlueprintPresenter::ERowKind::Macro);
	TestTrue(TEXT("variable row kind"), VariableRow.IsValid()
		&& VariableRow->Kind == FBlueprintHelperReviewMyBlueprintPresenter::ERowKind::Variable);
	TestTrue(TEXT("dispatcher row kind"), DispatcherRow.IsValid()
		&& DispatcherRow->Kind == FBlueprintHelperReviewMyBlueprintPresenter::ERowKind::Dispatcher);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewMyBlueprintPresenterKeepsEmptyDispatcherSectionTest,
	"BlueprintHelper.Review.UI.MyBlueprintPresenterKeepsEmptyDispatcherSection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewMyBlueprintPresenterKeepsEmptyDispatcherSectionTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("MyBlueprintEmptyDispatcherSection"));
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperReviewAssetContext Context;
	Context.AssetPath = Blueprint->GetOutermost()->GetName();
	Context.AssetKind = EBlueprintHelperReviewAssetKind::Blueprint;
	Context.AssetObject = Blueprint;
	Context.Blueprint = Blueprint;

	TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Changes;
	FBlueprintHelperReviewMyBlueprintPresenter::FState State;
	FBlueprintHelperReviewMyBlueprintPresenter::BuildContent(Context, State, Changes);

	bool bFoundDispatcherSection = false;
	for (const TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>& Root : State.RootItems)
	{
		bFoundDispatcherSection |= Root.IsValid()
			&& Root->Label.ToString() == TEXT("Event Dispatchers")
			&& Root->Kind == FBlueprintHelperReviewMyBlueprintPresenter::ERowKind::Section;
	}

	TestTrue(TEXT("empty Event Dispatchers section is still visible"), bFoundDispatcherSection);
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
		TEXT("Structure main workspace owns DataAsset presenter"),
		static_cast<int32>(FBlueprintHelperReviewSurfacePresenterRouter::GetMainWorkspaceSurfaceForAssetKind(
			EBlueprintHelperReviewAssetKind::Structure)),
		static_cast<int32>(EBlueprintHelperReviewSurface::DataAsset));
	TestEqual(
		TEXT("GenericObject main workspace owns DataAsset presenter"),
		static_cast<int32>(FBlueprintHelperReviewSurfacePresenterRouter::GetMainWorkspaceSurfaceForAssetKind(
			EBlueprintHelperReviewAssetKind::GenericObject)),
		static_cast<int32>(EBlueprintHelperReviewSurface::DataAsset));
	TestFalse(TEXT("Details panel uses row-highlight model instead of overlay ownership"),
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
	FBlueprintHelperReviewDataAssetAndTableUseRowHighlightSurfaceTest,
	"BlueprintHelper.Review.UI.DataAssetAndTableUseRowHighlightSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewDataAssetAndTableUseRowHighlightSurfaceTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("DataTable uses row background highlights"),
		FBlueprintHelperReviewRowHighlightModel::IsRowHighlightSurface(EBlueprintHelperReviewSurface::DataTable));
	TestTrue(TEXT("DataAsset uses row background highlights"),
		FBlueprintHelperReviewRowHighlightModel::IsRowHighlightSurface(EBlueprintHelperReviewSurface::DataAsset));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewComponentsRowHighlightUsesAssetContextScopeTest,
	"BlueprintHelper.Review.UI.ComponentsRowHighlightUsesAssetContextScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewComponentsRowHighlightUsesAssetContextScopeTest::RunTest(const FString& Parameters)
{
	const FString ContextAssetPath = TEXT("/Game/BlueprintHelper/Smoke/BP_ComponentSmoke");
	const FString ChangeAssetPath = TEXT("/Game/BlueprintHelperSmoke/BP_ComponentSmoke");

	TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> Items;
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_component_asset_scope");
	Change.AssetPath = ChangeAssetPath;
	Change.DisplayLabel = TEXT("SmokeMesh");
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
	FBlueprintHelperReviewAtomicTarget Target;
	Target.Surface = EBlueprintHelperReviewSurface::Components;
	Target.TargetKind = TEXT("component");
	Target.TargetKey = TEXT("component:SmokeMesh");
	Target.DisplayLabel = TEXT("SmokeMesh");
	Change.AtomicTargets.Add(Target);
	Items.Add(MakeShared<FBlueprintHelperReviewVisibleChange>(Change));

	FBlueprintHelperReviewAssetContext Context;
	Context.AssetPath = ContextAssetPath;
	Context.AssetKind = EBlueprintHelperReviewAssetKind::Blueprint;
	FBlueprintHelperReviewPanelSurfacePresenterArgs Args;
	Args.AssetContext = &Context;
	Args.ChangeItems = &Items;
	Args.SelectedChange = Items[0];
	Args.GetChangeColor = [](EBlueprintHelperReviewChangeKind)
	{
		return FSlateColor(FLinearColor::Green);
	};
	Args.ResolveRowGeometry.BindLambda([](
		const FBlueprintHelperReviewVisibleChange&,
		EBlueprintHelperReviewSurface,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
	{
		OutAnchor.bIsValid = true;
		OutAnchor.Position = FVector2D(12.0f, 24.0f);
		OutAnchor.Size = FVector2D(260.0f, 32.0f);
		OutAnchor.Reason = TEXT("test_stable_slate_row_geometry");
		return true;
	});

	FBlueprintHelperReviewBlueprintComponentsPresenter::BuildOverlay(Args);

	TestTrue(TEXT("component row highlight is scoped to loaded asset context"),
		FBlueprintHelperReviewRowHighlightModel::GetRowBackgroundColor(
			ContextAssetPath,
			EBlueprintHelperReviewSurface::Components,
			TEXT("SmokeMesh")).GetSpecifiedColor().A > 0.0f);
	TestTrue(TEXT("selected component row actions are scoped to loaded asset context"),
		FBlueprintHelperReviewRowHighlightModel::GetRowActionsVisibility(
			ContextAssetPath,
			EBlueprintHelperReviewSurface::Components,
			TEXT("SmokeMesh")) == EVisibility::Visible);
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
	Args.OnReviewActionIntent = [](const FBlueprintHelperReviewActionIntent&)
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
	TestTrue(TEXT("component partial geometry uses row background instead of overlay host"),
		&Overlay.Get() == &SNullWidget::NullWidget.Get());
	TestTrue(TEXT("component partial geometry keeps selected-row actions addressable"),
		FBlueprintHelperReviewRowHighlightModel::GetRowActionsVisibility(
			Items[0]->AssetPath,
			EBlueprintHelperReviewSurface::Components,
			TEXT("component:Target0")) == EVisibility::Visible);

	bool bSawShownRowHighlight0 = false;
	bool bSawPendingRow1 = false;
	bool bSawShownReviewList = false;
	bool bSawFrameGeometry = false;
	for (const FString& Message : DebugMessages)
	{
		bSawShownRowHighlight0 |= Message.Contains(
			TEXT("ReviewRowHighlight change=tx_component_partial_geometry_0 surface=Components target=\"component:Target0\" result=shown mode=row_background"));
		bSawPendingRow1 |= Message.Contains(
			TEXT("ReviewRowHighlight change=tx_component_partial_geometry_1 surface=Components target=\"component:Target1\" result=pending reason=test_missing_slate_row_geometry"));
		bSawShownReviewList |= Message.Contains(TEXT("mode=review_list result=shown"));
		bSawFrameGeometry |= Message.Contains(TEXT("ReviewFrameGeometry"));
	}

	TestTrue(TEXT("partial geometry reports stable row 0 as row background"), bSawShownRowHighlight0);
	TestTrue(TEXT("partial geometry leaves row 1 pending"), bSawPendingRow1);
	TestFalse(TEXT("partial geometry does not show text review-list fallback"), bSawShownReviewList);
	TestFalse(TEXT("partial geometry does not draw row overlay frames"), bSawFrameGeometry);
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

	FBlueprintHelperReviewEvidenceInput Rename;
	Rename.EvidenceId = TEXT("tx_rename");
	Rename.AssetPath = TEXT("/Game/BP_Door");
	Rename.LocationKey = TEXT("my_blueprint:function:OpenDoor");
	Rename.ChangeKind = EBlueprintHelperReviewChangeKind::Renamed;
	Rename.DisplayLabel = TEXT("OpenDoor -> OpenDoorFast");
	Rename.BeforeSummary = TEXT("OpenDoor");
	Rename.AfterSummary = TEXT("OpenDoorFast");

	TArray<FBlueprintHelperReviewEvidenceInput> RenameInputs;
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
	"BlueprintHelper.Review.VisibleChange.CollapsesSupersededEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewVisibleChangeCollapseTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;

	FBlueprintHelperReviewEvidenceInput T1;
	T1.EvidenceId = TEXT("tx_t1");
	T1.AssetPath = TEXT("/Game/BP_Door");
	T1.GraphName = TEXT("EventGraph");
	T1.LocationKey = TEXT("graph:EventGraph/node:PrintString/input:InString");
	T1.ChangeKind = EBlueprintHelperReviewChangeKind::VariableModified;
	T1.DisplayLabel = TEXT("PrintString InString");
	T1.BeforeSummary = TEXT("Open");
	T1.AfterSummary = TEXT("Opening");

	FBlueprintHelperReviewEvidenceInput T2 = T1;
	T2.EvidenceId = TEXT("tx_t2");
	T2.BeforeSummary = TEXT("Opening");
	T2.AfterSummary = TEXT("Door Opened");

	TArray<FBlueprintHelperReviewEvidenceInput> CollapseInputs;
	CollapseInputs.Add(T1);
	CollapseInputs.Add(T2);
	const TArray<FBlueprintHelperReviewVisibleChange> Changes = Store.BuildVisibleChanges(CollapseInputs);

	TestEqual(TEXT("one final visible change remains"), Changes.Num(), 1);
	if (Changes.Num() == 1)
	{
		const FBlueprintHelperReviewVisibleChange& Change = Changes[0];
		TestEqual(TEXT("latest evidence wins"), Change.LatestEvidenceId, FString(TEXT("tx_t2")));
		TestEqual(TEXT("baseline before summary is preserved"), Change.BeforeSummary, FString(TEXT("Open")));
		TestEqual(TEXT("final after summary is preserved"), Change.AfterSummary, FString(TEXT("Door Opened")));
		TestEqual(TEXT("source evidence chain kept"), Change.SourceEvidenceIds.Num(), 2);
		if (Change.SourceEvidenceIds.Num() == 2)
		{
			TestEqual(TEXT("first source is T1"), Change.SourceEvidenceIds[0], FString(TEXT("tx_t1")));
			TestEqual(TEXT("second source is T2"), Change.SourceEvidenceIds[1], FString(TEXT("tx_t2")));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewV2DoesNotCollapseDifferentChangeIdsByScopeIdentityTest,
	"BlueprintHelper.Review.V2.DoesNotCollapseDifferentChangeIdsByScopeIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewV2DoesNotCollapseDifferentChangeIdsByScopeIdentityTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange First;
	First.ChangeId = TEXT("change_a");
	First.AssetPath = TEXT("/Game/BP_Door");
	First.GraphName = TEXT("EventGraph");
	First.LocationKey = TEXT("graph:EventGraph/node:DoorFlow");
	First.LatestEvidenceId = TEXT("tx_a");
	First.SourceEvidenceIds.Add(TEXT("tx_a"));
	First.ScopeIdentity = TEXT("graph|EventGraph|node:DoorFlow");
	First.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	First.Status = EBlueprintHelperReviewChangeStatus::Pending;
	First.DisplayLabel = TEXT("Door flow A");
	FBlueprintHelperReviewAtomicTarget Target;
	Target.Surface = EBlueprintHelperReviewSurface::Graph;
	Target.GraphName = TEXT("EventGraph");
	Target.TargetKind = TEXT("graph_node");
	Target.TargetKey = TEXT("node:DoorFlow");
	Target.ScopeIdentity = First.ScopeIdentity;
	First.AtomicTargets.Add(Target);

	FBlueprintHelperReviewVisibleChange Second = First;
	Second.ChangeId = TEXT("change_b");
	Second.LatestEvidenceId = TEXT("tx_b");
	Second.SourceEvidenceIds.Reset();
	Second.SourceEvidenceIds.Add(TEXT("tx_b"));
	Second.DisplayLabel = TEXT("Door flow B");

	TArray<FBlueprintHelperReviewVisibleChange> LoadedChanges;
	LoadedChanges.Add(First);
	LoadedChanges.Add(Second);
	FBlueprintHelperReviewStoreMergeUtils::CollapseVisibleChangesLatestWins(LoadedChanges);
	TestEqual(TEXT("loaded pending changes keep distinct ChangeIds"), LoadedChanges.Num(), 2);

	FBlueprintHelperReviewRecord ExistingRecord;
	ExistingRecord.VisibleChanges.Add(First);
	FBlueprintHelperReviewRecord IncomingRecord;
	IncomingRecord.VisibleChanges.Add(Second);
	FBlueprintHelperReviewStoreMergeUtils::MergeReviewRecord(ExistingRecord, IncomingRecord);
	TestEqual(TEXT("record merge keeps distinct ChangeIds"), ExistingRecord.VisibleChanges.Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAtomicIntersectionCollapseTest,
	"BlueprintHelper.Review.VisibleChange.AtomicIntersectionUsesLatestEvidence",
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

	FBlueprintHelperReviewEvidenceInput T1;
	T1.EvidenceId = TEXT("tx_t1");
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

	FBlueprintHelperReviewEvidenceInput T2 = T1;
	T2.EvidenceId = TEXT("tx_t2");
	T2.BeforeSummary = TEXT("B");
	T2.AfterSummary = TEXT("C");
	T2.AtomicTargets.Reset();
	T2.AtomicTargets.Add(T2Node2);
	T2.AtomicTargets.Add(T2Node3);

	TArray<FBlueprintHelperReviewEvidenceInput> Inputs;
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
	TestTrue(TEXT("leaf records both latest evidence ids"),
		Change.LatestEvidenceIds.Contains(TEXT("tx_t1"))
		&& Change.LatestEvidenceIds.Contains(TEXT("tx_t2")));

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
		TestEqual(TEXT("N1 belongs to T1"), Node1->LatestEvidenceId, FString(TEXT("tx_t1")));
		TestEqual(TEXT("N2 intersection belongs to T2"), Node2->LatestEvidenceId, FString(TEXT("tx_t2")));
		TestEqual(TEXT("N3 belongs to T2"), Node3->LatestEvidenceId, FString(TEXT("tx_t2")));
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

	FBlueprintHelperReviewEvidenceInput ComponentChange;
	ComponentChange.EvidenceId = TEXT("tx_component");
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

	FBlueprintHelperReviewEvidenceInput PropertyChange;
	PropertyChange.EvidenceId = TEXT("tx_property");
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

	TArray<FBlueprintHelperReviewEvidenceInput> Inputs;
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
		TestEqual(TEXT("source summary counts one evidence"), DoorRecord->SourceReviewSummary.EvidenceCount, 1);
		TestEqual(TEXT("record storage starts active"),
			DoorRecord->StorageStatus,
			EBlueprintHelperReviewStorageStatus::Active);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRecordSourceReviewSummaryPersistsCreatedAtBoundsTest,
	"BlueprintHelper.Review.Record.SourceReviewSummaryPersistsCreatedAtBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRecordSourceReviewSummaryPersistsCreatedAtBoundsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_source_summary_created"));
	const FString RecordId = FBlueprintHelperReviewStoreService::MakeReviewRecordId(
		ArchiveSessionId,
		TEXT("/Game/BP_Door"));

	FBlueprintHelperReviewRecord Record;
	Record.ReviewRecordId = RecordId;
	Record.ArchiveSessionId = ArchiveSessionId;
	Record.AssetPath = TEXT("/Game/BP_Door");
	Record.SourceTaskRunIds.Add(TEXT("task_review_source_summary"));
	Record.SourceReviewSummary.EvidenceCount = 2;
	Record.SourceReviewSummary.TaskRunIds.Add(TEXT("task_review_source_summary"));
	Record.SourceReviewSummary.OperationKinds.Add(TEXT("append_blueprint_graph"));
	Record.SourceReviewSummary.OperationKinds.Add(TEXT("replace_blueprint_graph"));
	Record.SourceReviewSummary.AssetPaths.Add(TEXT("/Game/BP_Door"));
	Record.SourceReviewSummary.EvidenceIds.Add(TEXT("tx_review_source_1"));
	Record.SourceReviewSummary.EvidenceIds.Add(TEXT("tx_review_source_2"));
	Record.SourceReviewSummary.CreatedAtFirst = TEXT("2026-05-12T01:02:03Z");
	Record.SourceReviewSummary.CreatedAtLast = TEXT("2026-05-12T01:04:05Z");
	Record.VisibleChanges.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestVisibleChange(
		TEXT("tx_review_source_visible"),
		Record.AssetPath));

	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(RecordId);
	FString SaveError;
	TestTrue(TEXT("record with source summary created-at bounds saves"),
		Store.SaveReviewRecord(Record, SaveError));

	FBlueprintHelperReviewRecord Loaded;
	FString LoadError;
	TestTrue(TEXT("record with source summary created-at bounds reloads"),
		Store.LoadReviewRecordById(RecordId, Loaded, LoadError));
	TestEqual(TEXT("created_at_first round-trips"),
		Loaded.SourceReviewSummary.CreatedAtFirst,
		FString(TEXT("2026-05-12T01:02:03Z")));
	TestEqual(TEXT("created_at_last round-trips"),
		Loaded.SourceReviewSummary.CreatedAtLast,
		FString(TEXT("2026-05-12T01:04:05Z")));

	const TSharedRef<FJsonObject> SummaryArtifact = Store.BuildReviewRecordSummaryArtifact(Loaded);
	const TSharedPtr<FJsonObject>* SourceSummary = nullptr;
	TestTrue(TEXT("summary artifact exposes source evidence summary"),
		SummaryArtifact->TryGetObjectField(TEXT("source_review_summary"), SourceSummary)
		&& SourceSummary
		&& SourceSummary->IsValid());
	if (SourceSummary && SourceSummary->IsValid())
	{
		TestEqual(TEXT("summary artifact exposes created_at_first"),
			(*SourceSummary)->GetStringField(TEXT("created_at_first")),
			FString(TEXT("2026-05-12T01:02:03Z")));
		TestEqual(TEXT("summary artifact exposes created_at_last"),
			(*SourceSummary)->GetStringField(TEXT("created_at_last")),
			FString(TEXT("2026-05-12T01:04:05Z")));
	}

	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(RecordId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRecordQueryFiltersByTaskRunIdTest,
	"BlueprintHelper.Review.Record.QueryFiltersByTaskRunId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRecordQueryFiltersByTaskRunIdTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString MatchingArchiveSessionId =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_task_filter_match"));
	const FString OtherArchiveSessionId =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_task_filter_other"));

	FBlueprintHelperReviewRecord MatchingRecord;
	MatchingRecord.ReviewRecordId = FBlueprintHelperReviewStoreService::MakeReviewRecordId(
		MatchingArchiveSessionId,
		TEXT("/Game/BP_TaskFilterMatch"));
	MatchingRecord.ArchiveSessionId = MatchingArchiveSessionId;
	MatchingRecord.AssetPath = TEXT("/Game/BP_TaskFilterMatch");
	MatchingRecord.SourceTaskRunIds.Add(TEXT("task_filter_target"));
	MatchingRecord.SourceReviewSummary.TaskRunIds.Add(TEXT("task_filter_target"));
	MatchingRecord.SourceReviewSummary.EvidenceIds.Add(TEXT("tx_filter_target"));
	MatchingRecord.SourceReviewSummary.EvidenceCount = 1;
	FBlueprintHelperReviewVisibleChange MatchingChange;
	MatchingChange.AssetPath = MatchingRecord.AssetPath;
	MatchingChange.LocationKey = TEXT("graph:EventGraph:block:TaskFilterMatch");
	MatchingChange.DisplayLabel = TEXT("TaskFilterMatch");
	MatchingChange.Status = EBlueprintHelperReviewChangeStatus::Pending;
	MatchingRecord.VisibleChanges.Add(MatchingChange);

	FBlueprintHelperReviewRecord OtherRecord;
	OtherRecord.ReviewRecordId = FBlueprintHelperReviewStoreService::MakeReviewRecordId(
		OtherArchiveSessionId,
		TEXT("/Game/BP_TaskFilterOther"));
	OtherRecord.ArchiveSessionId = OtherArchiveSessionId;
	OtherRecord.AssetPath = TEXT("/Game/BP_TaskFilterOther");
	OtherRecord.SourceTaskRunIds.Add(TEXT("task_filter_other"));
	OtherRecord.SourceReviewSummary.TaskRunIds.Add(TEXT("task_filter_other"));
	OtherRecord.SourceReviewSummary.EvidenceIds.Add(TEXT("tx_filter_other"));
	OtherRecord.SourceReviewSummary.EvidenceCount = 1;
	FBlueprintHelperReviewVisibleChange OtherChange;
	OtherChange.AssetPath = OtherRecord.AssetPath;
	OtherChange.LocationKey = TEXT("graph:EventGraph:block:TaskFilterOther");
	OtherChange.DisplayLabel = TEXT("TaskFilterOther");
	OtherChange.Status = EBlueprintHelperReviewChangeStatus::Pending;
	OtherRecord.VisibleChanges.Add(OtherChange);

	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(MatchingRecord.ReviewRecordId);
	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(OtherRecord.ReviewRecordId);

	TArray<FBlueprintHelperReviewRecord> RecordsToSave;
	RecordsToSave.Add(MatchingRecord);
	RecordsToSave.Add(OtherRecord);
	FString SaveError;
	TestTrue(TEXT("task filter records save"), Store.SaveReviewRecords(RecordsToSave, SaveError));

	FBlueprintHelperReviewRecordQuery Query;
	Query.bPendingOnly = false;
	Query.TaskRunIdFilter = TEXT("task_filter_target");
	const TArray<FBlueprintHelperReviewRecord> LoadedRecords = Store.QueryReviewRecords(Query);

	TestEqual(TEXT("task_run_id filter returns only matching review record"),
		LoadedRecords.Num(),
		1);
	if (LoadedRecords.Num() == 1)
	{
		TestEqual(TEXT("task_run_id filter keeps matching source task"),
			LoadedRecords[0].SourceTaskRunIds[0],
			FString(TEXT("task_filter_target")));
		TestEqual(TEXT("task_run_id filter keeps matching asset"),
			LoadedRecords[0].AssetPath,
			FString(TEXT("/Game/BP_TaskFilterMatch")));
	}

	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(MatchingRecord.ReviewRecordId);
	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(OtherRecord.ReviewRecordId);
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
	DataTableTarget.Ownership = TEXT("blueprinthelper_owned");

	FBlueprintHelperReviewAtomicTarget DataAssetTarget = DataTableTarget;
	DataAssetTarget.Surface = EBlueprintHelperReviewSurface::DataAsset;
	DataAssetTarget.AssetPath = TEXT("/Game/BlueprintHelper/Smoke/DA_SmokeTuning");
	DataAssetTarget.TargetKind = TEXT("data_asset_property");
	DataAssetTarget.TargetKey = TEXT("data_asset_property:SmokeHealth");
	DataAssetTarget.DisplayLabel = TEXT("SmokeHealth");
	DataAssetTarget.PropertyPath = TEXT("SmokeHealth");
	DataAssetTarget.RecordedAfterHash = TEXT("after_hash_da");

	FBlueprintHelperWriteReviewEvidence Evidence;
	Evidence.ArchiveSessionId = TEXT("archive_cross_asset_stage5");
	Evidence.TaskRunId = TEXT("task_cross_asset_stage5");
	Evidence.EvidenceId = TEXT("tx_widget_save_cross_asset");
	Evidence.AssetPath = TEXT("/Game/BlueprintHelper/Smoke/WBP_WidgetSmoke");
	Evidence.OperationKind = TEXT("save_widget_blueprint");
	Evidence.DisplayLabel = TEXT("Widget save with referenced data changes");
	Evidence.AtomicTargets.Add(DataTableTarget);
	Evidence.AtomicTargets.Add(DataAssetTarget);

	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(Evidence);

	const TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	TestEqual(TEXT("one widget record still owns the evidence envelope"), Records.Num(), 1);
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
	T2.LatestEvidenceId = TEXT("tx_2");
	T2.SourceEvidenceIds.Reset();
	T2.SourceEvidenceIds.Add(TEXT("tx_2"));
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
	TestEqual(TEXT("latest evidence wins for an atomic target"),
		Target.LatestEvidenceId,
		FString(TEXT("tx_2")));
	TestEqual(TEXT("latest recorded_after_hash is preserved"),
		Target.RecordedAfterHash,
		FString(TEXT("after_2")));
	TestEqual(TEXT("source evidence chain is retained"),
		Target.SourceEvidenceIds.Num(),
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
		FString EvidenceId;
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
		Target.Ownership = TEXT("blueprinthelper_owned");

		FBlueprintHelperWriteReviewEvidence Evidence;
		Evidence.ArchiveSessionId = TEXT("archive_legacy_surface");
		Evidence.TaskRunId = TEXT("task_legacy_surface");
		Evidence.EvidenceId = SurfaceCase.EvidenceId;
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
		Change.LatestEvidenceId = SurfaceCase.ChangeId;
		Change.SourceEvidenceIds.Add(SurfaceCase.ChangeId);
		Change.DisplayLabel = SurfaceCase.TargetKey;

		FBlueprintHelperReviewAtomicTarget Target;
		Target.Surface = SurfaceCase.Surface;
		Target.AssetPath = SurfaceCase.AssetPath;
		Target.TargetKind = SurfaceCase.TargetKind;
		Target.TargetKey = SurfaceCase.TargetKey;
		Target.VisualGroupKey = SurfaceCase.TargetKey;
		Target.DisplayLabel = SurfaceCase.TargetKey;
		Target.LatestEvidenceId = SurfaceCase.ChangeId;
		Target.SourceEvidenceIds.Add(SurfaceCase.ChangeId);
		Target.RecordedAfterHash = TEXT("after_hash");
		Target.BaselineHash = TEXT("baseline_hash");
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
	Record.VisibleChanges.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestVisibleChange(
		TEXT("tx_debug_case_ids_visible"),
		Record.AssetPath));

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
	Existing.VisibleChanges.Add(FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestVisibleChange(
		TEXT("tx_debug_case_merge_visible"),
		Existing.AssetPath));

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
	FBlueprintHelperReviewAssetFactoryChangeIsLifecycleRootTest,
	"BlueprintHelper.Review.Record.AssetFactoryChangeIsLifecycleRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAssetFactoryChangeIsLifecycleRootTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString AssetPath = TEXT("/Game/BP_DoorLifecycleRoot");
	FBlueprintHelperReviewAtomicTarget Target =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewAssetFactoryTarget(
			TEXT("asset_factory:create_asset"),
			TEXT("tx_asset_factory_root"),
			AssetPath);

	FBlueprintHelperWriteReviewEvidence Evidence =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
			TEXT("archive_asset_factory_root"),
			TEXT("task_asset_factory_root"),
			TEXT("tx_asset_factory_root"),
			AssetPath,
			Target);
	Evidence.OperationKind = TEXT("asset_factory");
	Evidence.ChangeKind = EBlueprintHelperReviewChangeKind::Added;

	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(Evidence);
	const TArray<FBlueprintHelperReviewRecord> Records =
		Store.BuildReviewRecordsFromEvidence(Evidences);
	TestEqual(TEXT("one asset factory record is built"), Records.Num(), 1);
	if (Records.Num() != 1 || Records[0].VisibleChanges.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewVisibleChange& Change = Records[0].VisibleChanges[0];
	TestTrue(TEXT("asset factory added change is lifecycle root"), Change.bIsAssetLifecycleRoot);
	TestTrue(TEXT("asset factory root reject removes children"), Change.bRejectRemovesChildren);
	TestTrue(TEXT("asset factory root has no parent"), Change.ParentChangeId.IsEmpty());
	TestEqual(TEXT("asset factory root keeps added kind"),
		Change.ChangeKind,
		EBlueprintHelperReviewChangeKind::Added);
	TestTrue(TEXT("asset factory target remains asset factory"),
		Change.AtomicTargets.Num() == 1 && Change.AtomicTargets[0].TargetKind == TEXT("asset_factory"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingChangesLinkUnderLifecycleRootTest,
	"BlueprintHelper.Review.Record.PendingChangesLinkUnderLifecycleRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPendingChangesLinkUnderLifecycleRootTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_lifecycle_link"));
	const FString AssetPath = FString::Printf(
		TEXT("/Game/BP_Lifecycle_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString OtherAssetPath = FString::Printf(
		TEXT("/Game/BP_LifecycleOther_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));

	FBlueprintHelperReviewAtomicTarget RootTarget =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewAssetFactoryTarget(
			TEXT("asset_factory:create_asset"),
			TEXT("tx_lifecycle_root"),
			AssetPath);
	FBlueprintHelperWriteReviewEvidence RootEvidence =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
			ArchiveSessionId,
			TEXT("task_lifecycle_link"),
			TEXT("tx_lifecycle_root"),
			AssetPath,
			RootTarget);
	RootEvidence.OperationKind = TEXT("asset_factory");
	RootEvidence.ChangeKind = EBlueprintHelperReviewChangeKind::Added;

	FBlueprintHelperReviewAtomicTarget ChildTarget =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
			TEXT("component:SmokeComp"),
			TEXT("component:SmokeComp"),
			TEXT("tx_lifecycle_child"),
			TEXT("after_child"));
	ChildTarget.AssetPath = AssetPath;
	ChildTarget.Surface = EBlueprintHelperReviewSurface::Components;
	ChildTarget.TargetKind = TEXT("component");
	FBlueprintHelperWriteReviewEvidence ChildEvidence =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
			ArchiveSessionId,
			TEXT("task_lifecycle_link"),
			TEXT("tx_lifecycle_child"),
			AssetPath,
			ChildTarget);

	FBlueprintHelperReviewAtomicTarget OtherAssetTarget = ChildTarget;
	OtherAssetTarget.AssetPath = OtherAssetPath;
	OtherAssetTarget.TargetKey = TEXT("component:OtherComp");
	OtherAssetTarget.VisualGroupKey = TEXT("component:OtherComp");
	OtherAssetTarget.LatestEvidenceId = TEXT("tx_lifecycle_other_asset");
	OtherAssetTarget.SourceEvidenceIds.Reset();
	OtherAssetTarget.SourceEvidenceIds.Add(TEXT("tx_lifecycle_other_asset"));
	FBlueprintHelperWriteReviewEvidence OtherAssetEvidence =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
			ArchiveSessionId,
			TEXT("task_lifecycle_link"),
			TEXT("tx_lifecycle_other_asset"),
			AssetPath,
			OtherAssetTarget);

	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(RootEvidence);
	Evidences.Add(ChildEvidence);
	Evidences.Add(OtherAssetEvidence);
	TArray<FBlueprintHelperReviewRecord> Records =
		Store.BuildReviewRecordsFromEvidence(Evidences);
	FString SaveError;
	TestTrue(TEXT("lifecycle link record saves"), Store.SaveReviewRecords(Records, SaveError));

	const TArray<FBlueprintHelperReviewVisibleChange> PendingChanges =
		Store.LoadPendingVisibleChanges(AssetPath);
	const FBlueprintHelperReviewVisibleChange* Root = PendingChanges.FindByPredicate(
		[](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.bIsAssetLifecycleRoot;
		});
	TestNotNull(TEXT("pending lifecycle root loads"), Root);
	if (!Root)
	{
		return false;
	}

	const FBlueprintHelperReviewVisibleChange* Child = PendingChanges.FindByPredicate(
		[](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.AtomicTargets.ContainsByPredicate(
				[](const FBlueprintHelperReviewAtomicTarget& Target)
				{
					return Target.TargetKey == TEXT("component:SmokeComp");
				});
		});
	const FBlueprintHelperReviewVisibleChange* OtherAssetChild = PendingChanges.FindByPredicate(
		[](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.AtomicTargets.ContainsByPredicate(
				[](const FBlueprintHelperReviewAtomicTarget& Target)
				{
					return Target.TargetKey == TEXT("component:OtherComp");
				});
		});

	TestNotNull(TEXT("same asset child loads"), Child);
	TestNotNull(TEXT("different asset child in record loads"), OtherAssetChild);
	if (Child)
	{
		TestEqual(TEXT("same asset child links to root"),
			Child->ParentChangeId,
			Root->ChangeId);
	}
	if (OtherAssetChild)
	{
		TestTrue(TEXT("different asset child remains unparented"),
			OtherAssetChild->ParentChangeId.IsEmpty());
	}

	for (const FBlueprintHelperReviewRecord& Record : Records)
	{
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(Record.ReviewRecordId);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewLegacyVisibleChangeDefaultsToNoLifecycleRootTest,
	"BlueprintHelper.Review.Record.LegacyVisibleChangeDefaultsToNoLifecycleRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewLegacyVisibleChangeDefaultsToNoLifecycleRootTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_lifecycle_legacy"));

	FBlueprintHelperReviewAtomicTarget Target =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
			TEXT("graph_node:N1"),
			TEXT("graph:EventGraph:block:DoorFlow"),
			TEXT("tx_lifecycle_legacy"));
	FBlueprintHelperWriteReviewEvidence Evidence =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
			ArchiveSessionId,
			TEXT("task_lifecycle_legacy"),
			TEXT("tx_lifecycle_legacy"),
			TEXT("/Game/BP_Door"),
			Target);

	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(Evidence);
	TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	FString SaveError;
	TestTrue(TEXT("legacy default record saves"), Store.SaveReviewRecords(Records, SaveError));

	FBlueprintHelperReviewRecord Loaded;
	FString LoadError;
	TestTrue(TEXT("legacy default record reloads"),
		Store.LoadReviewRecordById(Records[0].ReviewRecordId, Loaded, LoadError));
	if (Loaded.VisibleChanges.Num() != 1)
	{
		return false;
	}

	TestTrue(TEXT("legacy non-root change has no parent"),
		Loaded.VisibleChanges[0].ParentChangeId.IsEmpty());
	TestFalse(TEXT("legacy non-root change is not lifecycle root"),
		Loaded.VisibleChanges[0].bIsAssetLifecycleRoot);
	TestFalse(TEXT("legacy non-root change does not remove children"),
		Loaded.VisibleChanges[0].bRejectRemovesChildren);

	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(Records[0].ReviewRecordId);
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
	FBlueprintHelperReviewLoadPendingVisibleChangesIncludesGraphWriteBlockAsGraphBodyTest,
	"BlueprintHelper.Review.UI.LoadPendingVisibleChangesIncludesGraphWriteBlockAsGraphBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewLoadPendingVisibleChangesIncludesGraphWriteBlockAsGraphBodyTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_ui_graph_body"));
	const FString AssetPath = FString::Printf(
		TEXT("/Game/BP_GraphBody_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString GraphName = TEXT("EventGraph");
	const FString TargetKey = TEXT("graph:EventGraph:block:EventGraph_CE_DumpGlobalStateForReview0");
	const FString VisualGroupKey = TEXT("graph_body|EventGraph");

	FBlueprintHelperReviewAtomicTarget GraphBlockTarget =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
			TargetKey,
			VisualGroupKey,
			TEXT("tx_ui_graph_body"),
			TEXT("after_graph_body"));
	GraphBlockTarget.AssetPath = AssetPath;
	GraphBlockTarget.Surface = EBlueprintHelperReviewSurface::Graph;
	GraphBlockTarget.GraphName = GraphName;
	GraphBlockTarget.TargetKind = TEXT("graph_block");
	GraphBlockTarget.DisplayLabel = TEXT("append_blueprint_graph EventGraph");
	GraphBlockTarget.Ownership = TEXT("graph_write");

	FBlueprintHelperWriteReviewEvidence Evidence =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
			ArchiveSessionId,
			TEXT("task_ui_graph_body"),
			TEXT("tx_ui_graph_body"),
			AssetPath,
			GraphBlockTarget);

	TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence({Evidence});
	TestEqual(TEXT("one graph body UI query record is built"), Records.Num(), 1);
	if (Records.Num() != 1)
	{
		return false;
	}
	TestEqual(TEXT("graph block evidence becomes one graph body visible change"),
		Records[0].VisibleChanges.Num(),
		1);
	if (Records[0].VisibleChanges.Num() != 1 || Records[0].VisibleChanges[0].AtomicTargets.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewVisibleChange& BuiltChange = Records[0].VisibleChanges[0];
	TestEqual(TEXT("graph body change keeps graph group location"), BuiltChange.LocationKey, VisualGroupKey);
	TestEqual(TEXT("graph body change keeps graph name"), BuiltChange.GraphName, GraphName);
	TestEqual(TEXT("graph body atomic target keeps concrete block key"),
		BuiltChange.AtomicTargets[0].TargetKey,
		TargetKey);
	TestEqual(TEXT("graph body atomic target keeps graph block kind"),
		BuiltChange.AtomicTargets[0].TargetKind,
		FString(TEXT("graph_block")));

	FString SaveError;
	TestTrue(TEXT("graph body UI query record saves"), Store.SaveReviewRecords(Records, SaveError));

	const TArray<FBlueprintHelperReviewVisibleChange> PendingChanges =
		Store.LoadPendingVisibleChanges(AssetPath);
	const bool bContainsGraphBody = PendingChanges.ContainsByPredicate(
		[&TargetKey, &VisualGroupKey, &GraphName](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.LocationKey == VisualGroupKey
				&& Change.GraphName == GraphName
				&& Change.AtomicTargets.ContainsByPredicate(
					[&TargetKey](const FBlueprintHelperReviewAtomicTarget& Target)
					{
						return Target.TargetKind == TEXT("graph_block")
							&& Target.TargetKey == TargetKey;
					});
		});

	TestTrue(TEXT("pending query exposes graph write block as graph body change"), bContainsGraphBody);

	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(Records[0].ReviewRecordId);
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

	FBlueprintHelperReviewEvidenceInput T1;
	T1.EvidenceId = TEXT("tx_t1");
	T1.AssetPath = TEXT("/Game/BP_Door");
	T1.LocationKey = TEXT("function:OpenDoor:signature");
	T1.ChangeKind = EBlueprintHelperReviewChangeKind::SignatureModified;
	T1.DisplayLabel = TEXT("OpenDoor signature");
	T1.BeforeSummary = TEXT("OpenDoor()");
	T1.AfterSummary = TEXT("OpenDoor(Input)");

	FBlueprintHelperReviewEvidenceInput T2 = T1;
	T2.EvidenceId = TEXT("tx_t2");
	T2.AfterSummary = TEXT("OpenDoor(Input, Speed)");

	TArray<FBlueprintHelperReviewEvidenceInput> CollapseInputs;
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
	TestEqual(TEXT("accept targets latest evidence"), Result.TargetEvidenceId, FString(TEXT("tx_t2")));
	TestEqual(TEXT("accept marks change accepted"), Result.NewStatus, EBlueprintHelperReviewChangeStatus::Accepted);
	TestTrue(TEXT("superseded evidence data is compaction eligible"), Result.bSupersededDataCompactionEligible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectVisibleChangeTest,
	"BlueprintHelper.Review.Action.RejectReportsPersistedTargetsNotFound",
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
	Change.LatestEvidenceId = TEXT("tx_t2");
	Change.LatestEvidenceIds.Add(TEXT("tx_t1"));
	Change.LatestEvidenceIds.Add(TEXT("tx_t2"));
	Change.SourceEvidenceIds.Add(TEXT("tx_t1"));
	Change.SourceEvidenceIds.Add(TEXT("tx_t2"));
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
	Change.LatestEvidenceId = TEXT("tx_t2");
	Change.SourceEvidenceIds.Add(TEXT("tx_t1"));
	Change.SourceEvidenceIds.Add(TEXT("tx_t2"));
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::VariableModified;
	Change.BeforeSummary = TEXT("Open");
	Change.AfterSummary = TEXT("Door Opened");

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.RejectVisibleChange(Change);

	TestFalse(TEXT("reject without persisted targets does not fake a completed rollback"), Result.bSucceeded);
	TestEqual(TEXT("reject without persisted targets enters needs action"),
		Result.NewStatus,
		EBlueprintHelperReviewChangeStatus::NeedsAction);
	TestEqual(TEXT("reject without persisted targets reports the data-model integrity gap"),
		Result.Message,
		FString(TEXT("persisted_review_targets_not_found")));
	TestTrue(TEXT("reject without persisted targets does not select archive rollback mode"),
		Result.RollbackMode.IsEmpty());
	TestFalse(TEXT("reject no longer reports Review UI slice placeholder"),
		Result.Message.Contains(TEXT("Review UI slice")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelRejectServiceUnavailableTest,
	"BlueprintHelper.Review.PanelCommand.RejectRequiresActionServiceDiagnostic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelRejectServiceUnavailableTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("tx_panel_reject_missing_service");
	Change.AssetPath = TEXT("/Game/BP_Door");
	Change.LatestEvidenceId = TEXT("tx_panel_reject_missing_service");
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;

	const FBlueprintHelperReviewPanelCommandService CommandService(nullptr);
	const FBlueprintHelperReviewActionResult Result =
		CommandService.RejectVisibleChange(Change, FBlueprintHelperReviewRejectOptions());

	TestFalse(TEXT("panel reject without action service does not fake rollback"), Result.bSucceeded);
	TestEqual(TEXT("panel reject without action service enters needs action"),
		Result.NewStatus,
		EBlueprintHelperReviewChangeStatus::NeedsAction);
	TestEqual(TEXT("panel reject without action service reports service diagnostic"),
		Result.Message,
		FString(TEXT("review_action_service_unavailable")));
	TestTrue(TEXT("panel reject without action service does not select rollback mode"),
		Result.RollbackMode.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelRejectLifecycleRootServiceUnavailableTest,
	"BlueprintHelper.Review.PanelCommand.RejectLifecycleRootRequiresActionServiceDiagnostic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelRejectLifecycleRootServiceUnavailableTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Root;
	Root.ChangeId = TEXT("tx_panel_root_missing_service");
	Root.AssetPath = TEXT("/Game/BP_Door");
	Root.LatestEvidenceId = TEXT("tx_panel_root_missing_service");
	Root.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	Root.bIsAssetLifecycleRoot = true;

	const FBlueprintHelperReviewPanelCommandService CommandService(nullptr);
	const FBlueprintHelperReviewCascadeActionResult Result =
		CommandService.RejectLifecycleRootVisibleChange(
			Root,
			FBlueprintHelperReviewRejectOptions());

	TestFalse(TEXT("panel root reject without action service does not fake rollback"), Result.RootResult.bSucceeded);
	TestEqual(TEXT("panel root reject without action service enters needs action"),
		Result.RootResult.NewStatus,
		EBlueprintHelperReviewChangeStatus::NeedsAction);
	TestEqual(TEXT("panel root reject without action service reports service diagnostic"),
		Result.RootResult.Message,
		FString(TEXT("review_action_service_unavailable")));
	TestTrue(TEXT("panel root reject without action service does not select rollback mode"),
		Result.RootResult.RollbackMode.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPanelRejectLifecycleRootLoadsFullPendingGraphTest,
	"BlueprintHelper.Review.PanelCommand.RejectLifecycleRootLoadsFullPendingGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPanelRejectLifecycleRootLoadsFullPendingGraphTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewActionService ActionService;
	FBlueprintHelperReviewPanelCommandService CommandService(&ActionService, &Store);

	const FString ArchiveSessionId =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_panel_lifecycle_full_graph"));
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewNamedBlueprint(
		TEXT("PanelLifecycleFullGraph"));
	TestNotNull(TEXT("panel lifecycle blueprint created"), Blueprint);
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		return false;
	}

	USCS_Node* ParentNode = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewSceneComponentNode(
		Blueprint,
		TEXT("PanelParent"));
	USCS_Node* ChildNode = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewSceneComponentNode(
		Blueprint,
		TEXT("PanelChild"),
		ParentNode);
	TestNotNull(TEXT("panel parent component exists"), ParentNode);
	TestNotNull(TEXT("panel child component exists"), ChildNode);
	if (!ParentNode || !ChildNode)
	{
		return false;
	}

	const FBlueprintHelperReviewVisibleChange ParentChange =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeAddedComponentChange(
			TEXT("change_panel_parent"),
			TEXT("tx_panel_parent"),
			FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeAddedComponentTarget(
				Blueprint,
				TEXT("PanelParent"),
				TEXT("tx_panel_parent")),
			1);
	const FBlueprintHelperReviewVisibleChange ChildChange =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeAddedComponentChange(
			TEXT("change_panel_child"),
			TEXT("tx_panel_child"),
			FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeAddedComponentTarget(
				Blueprint,
				TEXT("PanelChild"),
				TEXT("tx_panel_child"),
				TEXT("PanelParent")),
			2);

	FBlueprintHelperReviewRecord ParentRecord =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewRecordForVisibleChanges(
			ArchiveSessionId + TEXT("_parent"),
			Blueprint->GetPathName(),
			{ParentChange});
	FBlueprintHelperReviewRecord ChildRecord =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewRecordForVisibleChanges(
			ArchiveSessionId + TEXT("_child"),
			Blueprint->GetPathName(),
			{ChildChange});
	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(ParentRecord.ReviewRecordId);
	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(ChildRecord.ReviewRecordId);

	FString SaveError;
	TestTrue(TEXT("panel parent record saves"), Store.SaveReviewRecord(ParentRecord, SaveError));
	TestTrue(TEXT("panel child record saves"), Store.SaveReviewRecord(ChildRecord, SaveError));

	const TArray<FBlueprintHelperReviewVisibleChange> FullPendingChanges =
		Store.LoadPendingVisibleChanges(Blueprint->GetPathName());
	const FBlueprintHelperReviewVisibleChange* Root = FullPendingChanges.FindByPredicate(
		[](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.ChangeId == TEXT("change_panel_parent");
		});
	TestNotNull(TEXT("panel lifecycle root is available"), Root);
	if (!Root)
	{
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(ParentRecord.ReviewRecordId);
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(ChildRecord.ReviewRecordId);
		return false;
	}

	const FBlueprintHelperReviewActionIntent Intent = FBlueprintHelperReviewActionIntent::Reject(
		FBlueprintHelperReviewPanelStateService::MakeChangeBinding(
			*Root,
			EBlueprintHelperReviewSurface::Components,
			Root->LocationKey),
		TEXT("test"));
	const FBlueprintHelperReviewCommandResult Result =
		CommandService.ExecuteActionIntent(Intent, { *Root });

	TestTrue(TEXT("panel lifecycle command uses cascade result"), Result.bCascade);
	TestTrue(TEXT("panel lifecycle root reject succeeds"), Result.CascadeActionResult.RootResult.bSucceeded);
	TestTrue(TEXT("panel command removed child from full pending graph"),
		Result.CascadeActionResult.RemovedChildChangeIds.Contains(TEXT("change_panel_child")));
	TestNull(TEXT("panel parent component removed"),
		FBlueprintHelperReviewSnapshotRestoreService::FindScsNodeByName(Blueprint, TEXT("PanelParent")));
	TestNull(TEXT("panel child component removed"),
		FBlueprintHelperReviewSnapshotRestoreService::FindScsNodeByName(Blueprint, TEXT("PanelChild")));

	FBlueprintHelperReviewRecord LoadedChildRecord;
	FString LoadError;
	TestFalse(TEXT("single-child record is pruned"),
		Store.LoadReviewRecordById(ChildRecord.ReviewRecordId, LoadedChildRecord, LoadError));

	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(ParentRecord.ReviewRecordId);
	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(ChildRecord.ReviewRecordId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectVisibleChangeSuccessTest,
	"BlueprintHelper.Review.Action.RejectSucceedsWithMatchingHashAndSnapshotBaseline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectVisibleChangeSuccessTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("RejectVisibleSuccess"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Node = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("RejectVisibleSuccessNode"));
	TestNotNull(TEXT("graph node created"), Node);
	if (!Node)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Blueprint->GetPathName();
	Target.GraphName = Graph->GetName();
	Target.TargetKind = TEXT("graph_node");
	Target.TargetKey = FString::Printf(TEXT("graph:%s:node:%s"), *Graph->GetName(), *Node->NodeGuid.ToString(EGuidFormats::Digits));
	Target.VisualGroupKey = Target.TargetKey;
	Target.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
	Target.LatestEvidenceId = TEXT("tx_2");

	FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
	FString SnapshotError;
	TestTrue(TEXT("reject success target has recoverable before snapshot"),
		SnapshotService.CaptureTargetSnapshot(Target, Target.BeforeSnapshotJson, Target.RecordedAfterHash, SnapshotError));
	if (Target.BeforeSnapshotJson.IsEmpty() || Target.RecordedAfterHash.IsEmpty())
	{
		return false;
	}
	Target.BaselineHash = Target.RecordedAfterHash;

	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("change_1");
	Change.AssetPath = Blueprint->GetPathName();
	Change.GraphName = Graph->GetName();
	Change.LatestEvidenceId = TEXT("tx_2");
	Change.Status = EBlueprintHelperReviewChangeStatus::Pending;
	Change.AtomicTargets.Add(Target);

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(Target.TargetKey, Target.RecordedAfterHash);

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.RejectVisibleChange(Change, Options);

	TestTrue(TEXT("reject succeeds after strict hash match and rollback"), Result.bSucceeded);
	TestEqual(TEXT("reject marks change rejected"),
		Result.NewStatus,
		EBlueprintHelperReviewChangeStatus::Rejected);
	TestEqual(TEXT("reject targets latest evidence"),
		Result.TargetEvidenceId,
		FString(TEXT("tx_2")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectVisibleChangeToctouMismatchTest,
	"BlueprintHelper.Review.Action.RejectBlocksCurrentStateMismatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectVisibleChangeToctouMismatchTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("RejectVisibleHashDiagnostic"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Node = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("RejectVisibleHashDiagnosticNode"));
	TestNotNull(TEXT("graph node created"), Node);
	if (!Node)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Blueprint->GetPathName();
	Target.GraphName = Graph->GetName();
	Target.TargetKind = TEXT("graph_node");
	Target.TargetKey = FString::Printf(TEXT("graph:%s:node:%s"), *Graph->GetName(), *Node->NodeGuid.ToString(EGuidFormats::Digits));
	Target.VisualGroupKey = Target.TargetKey;
	Target.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
	Target.LatestEvidenceId = TEXT("tx_2");

	FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
	FString SnapshotError;
	TestTrue(TEXT("reject hash diagnostic target has recoverable before snapshot"),
		SnapshotService.CaptureTargetSnapshot(Target, Target.BeforeSnapshotJson, Target.RecordedAfterHash, SnapshotError));
	if (Target.BeforeSnapshotJson.IsEmpty() || Target.RecordedAfterHash.IsEmpty())
	{
		return false;
	}
	Target.BaselineHash = Target.RecordedAfterHash;
	Target.AfterSnapshotJson = Target.BeforeSnapshotJson;

	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("change_1");
	Change.AssetPath = Blueprint->GetPathName();
	Change.GraphName = Graph->GetName();
	Change.LatestEvidenceId = TEXT("tx_2");
	Change.AtomicTargets.Add(Target);

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(Target.TargetKey, TEXT("user_changed"));

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.RejectVisibleChange(Change, Options);

	TestTrue(TEXT("reject uses evidence-before snapshot even when current hash drift is diagnostic"), Result.bSucceeded);
	TestEqual(TEXT("hash drift diagnostic still rejects after rollback"),
		Result.NewStatus,
		EBlueprintHelperReviewChangeStatus::Rejected);
	TestEqual(TEXT("hash guard records target key"),
		Result.HashGuardTargetKey,
		Target.TargetKey);
	TestEqual(TEXT("hash guard records expected latest hash"),
		Result.HashGuardExpectedHash,
		Target.RecordedAfterHash);
	TestEqual(TEXT("hash guard records current drift hash"),
		Result.HashGuardCurrentHash,
		FString(TEXT("user_changed")));
	TestFalse(TEXT("hash guard carries recorded after snapshot for diagnostics"),
		Result.HashGuardRecordedAfterSnapshotJson.IsEmpty());
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
	FBlueprintHelperReviewRejectTargetsPurgesReviewRecordTest,
	"BlueprintHelper.Review.Action.RejectTargetsPurgesReviewRecord",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectTargetsPurgesReviewRecordTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_reject_targets"));
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("RejectTargetsPurge"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Node = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("RejectTargetsPurgeNode"));
	TestNotNull(TEXT("graph node created"), Node);
	if (!Node)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	FString TargetError;
	TestTrue(TEXT("reject targets purge fixture has recoverable snapshot"),
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::PopulateReviewNodeCommentTargetFromSnapshot(
			Blueprint,
			Graph,
			Node,
			TEXT("tx_reject_targets"),
			Target,
			TargetError));
	if (Target.BeforeSnapshotJson.IsEmpty() || Target.RecordedAfterHash.IsEmpty())
	{
		if (!TargetError.IsEmpty())
		{
			AddError(TargetError);
		}
		return false;
	}
	FBlueprintHelperReviewRecord Record =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewRecordForVisibleChanges(
			ArchiveSessionId,
			Target.AssetPath,
			{
				FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewVisibleChangeForTarget(
					TEXT("change_reject_targets"),
					TEXT("tx_reject_targets"),
					Target)
			});
	FString SaveError;
	TestTrue(TEXT("record saved before reject targets"), Store.SaveReviewRecord(Record, SaveError));

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(Target.TargetKey, Target.RecordedAfterHash);

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.RejectReviewTargets(
		Record.ReviewRecordId,
		{Target.TargetKey},
		Options);
	TestTrue(TEXT("persisted reject succeeds"), Result.bSucceeded);

	FBlueprintHelperReviewRecord Loaded;
	FString LoadError;
	TestFalse(TEXT("successful reject physically removes the review record"),
		Store.LoadReviewRecordById(Record.ReviewRecordId, Loaded, LoadError));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectVariableBenchmarkTest,
	"BlueprintHelper.Review.Performance.RejectVariableBenchmark",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectVariableBenchmarkTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_reject_variable_benchmark"));
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(
		TEXT("RejectVariableBenchmark"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	FString TargetError;
	TestTrue(TEXT("variable benchmark target has recoverable snapshot"),
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::PopulateReviewVariableTargetFromSnapshot(
			Blueprint,
			TEXT("RejectPerfValue"),
			TEXT("tx_reject_variable_benchmark"),
			Target,
			TargetError));
	if (Target.BeforeSnapshotJson.IsEmpty() || Target.RecordedAfterHash.IsEmpty())
	{
		if (!TargetError.IsEmpty())
		{
			AddError(TargetError);
		}
		return false;
	}

	FBlueprintHelperReviewRecord Record =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewRecordForVisibleChanges(
			ArchiveSessionId,
			Target.AssetPath,
			{
				FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewVisibleChangeForTarget(
					TEXT("change_reject_variable_benchmark"),
					TEXT("tx_reject_variable_benchmark"),
					Target,
					EBlueprintHelperReviewChangeKind::VariableModified)
			});
	FString SaveError;
	TestTrue(TEXT("record saved before variable benchmark"), Store.SaveReviewRecord(Record, SaveError));

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(Target.TargetKey, Target.RecordedAfterHash);

	FBlueprintHelperReviewActionService ActionService;
	const double StartedAtSeconds = FPlatformTime::Seconds();
	const FBlueprintHelperReviewActionResult Result = ActionService.RejectReviewTargets(
		Record.ReviewRecordId,
		{Target.TargetKey},
		Options);
	const double RejectMs = (FPlatformTime::Seconds() - StartedAtSeconds) * 1000.0;
	UE_LOG(LogTemp, Warning,
		TEXT("BlueprintHelperRejectBenchmark sample=variable reject_targets_ms=%.2f succeeded=%d message=\"%s\""),
		RejectMs,
		Result.bSucceeded ? 1 : 0,
		*Result.Message);

	TestTrue(TEXT("variable benchmark reject succeeds"), Result.bSucceeded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectGraphBlockBenchmarkTest,
	"BlueprintHelper.Review.Performance.RejectGraphBlockBenchmark",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectGraphBlockBenchmarkTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_reject_graph_block_benchmark"));
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(
		TEXT("RejectGraphBlockBenchmark"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	FBlueprintHelperReviewAtomicTarget Target;
	FString TargetError;
	TestTrue(TEXT("graph block benchmark target has recoverable snapshot"),
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::PopulateReviewGraphBlockTargetFromSnapshot(
			Blueprint,
			Graph,
			TEXT("RejectPerfBlock"),
			TEXT("tx_reject_graph_block_benchmark"),
			Target,
			TargetError));
	if (Target.BeforeSnapshotJson.IsEmpty() || Target.RecordedAfterHash.IsEmpty())
	{
		if (!TargetError.IsEmpty())
		{
			AddError(TargetError);
		}
		return false;
	}

	FBlueprintHelperReviewRecord Record =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewRecordForVisibleChanges(
			ArchiveSessionId,
			Target.AssetPath,
			{
				FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewVisibleChangeForTarget(
					TEXT("change_reject_graph_block_benchmark"),
					TEXT("tx_reject_graph_block_benchmark"),
					Target)
			});
	FString SaveError;
	TestTrue(TEXT("record saved before graph block benchmark"), Store.SaveReviewRecord(Record, SaveError));

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(Target.TargetKey, Target.RecordedAfterHash);

	FBlueprintHelperReviewActionService ActionService;
	const double StartedAtSeconds = FPlatformTime::Seconds();
	const FBlueprintHelperReviewActionResult Result = ActionService.RejectReviewTargets(
		Record.ReviewRecordId,
		{Target.TargetKey},
		Options);
	const double RejectMs = (FPlatformTime::Seconds() - StartedAtSeconds) * 1000.0;
	UE_LOG(LogTemp, Warning,
		TEXT("BlueprintHelperRejectBenchmark sample=graph_block reject_targets_ms=%.2f succeeded=%d message=\"%s\""),
		RejectMs,
		Result.bSucceeded ? 1 : 0,
		*Result.Message);

	TestTrue(TEXT("graph block benchmark reject succeeds"), Result.bSucceeded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectTargetsPurgesLinkedDebugCasesTest,
	"BlueprintHelper.Review.Action.RejectTargetsPurgesLinkedDebugCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectTargetsPurgesLinkedDebugCasesTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperDebugCaseStoreService DebugStore;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_reject_targets_debug"));
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("RejectTargetsDebugPurge"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Node = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("RejectTargetsDebugPurgeNode"));
	TestNotNull(TEXT("graph node created"), Node);
	if (!Node)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	FString TargetError;
	TestTrue(TEXT("reject targets debug purge fixture has recoverable snapshot"),
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::PopulateReviewNodeCommentTargetFromSnapshot(
			Blueprint,
			Graph,
			Node,
			TEXT("tx_reject_targets_debug"),
			Target,
			TargetError));
	if (Target.BeforeSnapshotJson.IsEmpty() || Target.RecordedAfterHash.IsEmpty())
	{
		if (!TargetError.IsEmpty())
		{
			AddError(TargetError);
		}
		return false;
	}
	FBlueprintHelperReviewRecord Record =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewRecordForVisibleChanges(
			ArchiveSessionId,
			Target.AssetPath,
			{
				FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewVisibleChangeForTarget(
					TEXT("change_reject_targets_debug"),
					TEXT("tx_reject_targets_debug"),
					Target)
			});

	const FString DebugCaseId = TEXT("dbg_reject_targets_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Record.DebugCaseIds.Add(DebugCaseId);
	FString SaveError;
	TestTrue(TEXT("record with linked debug case saves"), Store.SaveReviewRecord(Record, SaveError));

	FBlueprintHelperDebugCase DebugCase;
	DebugCase.DebugCaseId = DebugCaseId;
	DebugCase.CreatedAt = FDateTime::UtcNow().ToIso8601();
	DebugCase.UpdatedAt = DebugCase.CreatedAt;
	DebugCase.Source = TEXT("review_test");
	DebugCase.Operation = TEXT("reject_review_targets");
	DebugCase.Stage = TEXT("test");
	DebugCase.AssetPaths.Add(Target.AssetPath);
	DebugCase.ReviewRecordIds.Add(Record.ReviewRecordId);
	FString DebugSaveError;
	TestTrue(TEXT("linked debug case saves"), DebugStore.SaveCase(DebugCase, &DebugSaveError));

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(Target.TargetKey, Target.RecordedAfterHash);

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.RejectReviewTargets(
		Record.ReviewRecordId,
		{Target.TargetKey},
		Options);
	TestTrue(TEXT("persisted reject succeeds"), Result.bSucceeded);

	FBlueprintHelperReviewRecord Loaded;
	FString LoadError;
	TestFalse(TEXT("successful reject removes the review record"),
		Store.LoadReviewRecordById(Record.ReviewRecordId, Loaded, LoadError));
	FBlueprintHelperDebugCase LoadedDebugCase;
	FString DebugLoadError;
	TestFalse(TEXT("successful reject removes linked debug case"),
		DebugStore.LoadCase(DebugCaseId, LoadedDebugCase, &DebugLoadError));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewComponentSnapshotRestoreRefusesParentWithChildrenTest,
	"BlueprintHelper.Review.Action.ComponentSnapshotRestoreRefusesParentWithChildren",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewComponentSnapshotRestoreRefusesParentWithChildrenTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewNamedBlueprint(
		TEXT("ComponentRestoreGuard"));
	TestNotNull(TEXT("component restore guard blueprint created"), Blueprint);
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		return false;
	}

	USCS_Node* ParentNode = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewSceneComponentNode(
		Blueprint,
		TEXT("GuardParent"));
	USCS_Node* ChildNode = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewSceneComponentNode(
		Blueprint,
		TEXT("GuardChild"),
		ParentNode);
	TestNotNull(TEXT("parent component node exists"), ParentNode);
	TestNotNull(TEXT("child component node exists"), ChildNode);
	if (!ParentNode || !ChildNode)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeAddedComponentTarget(
			Blueprint,
			TEXT("GuardParent"),
			TEXT("tx_component_restore_guard"));
	TSharedPtr<FJsonObject> Snapshot;
	FString Error;
	TestTrue(TEXT("component before snapshot parses"),
		FBlueprintHelperReviewSnapshotRestoreService::ParseReviewSnapshotJson(
			Target.BeforeSnapshotJson,
			Snapshot,
			Error));

	const bool bRestored = FBlueprintHelperReviewSnapshotRestoreService::RestoreComponentFromSnapshot(
		Target,
		Snapshot,
		Error);
	TestFalse(TEXT("component restore refuses to remove parent with children"), bRestored);
	TestTrue(TEXT("error reports children guard"),
		Error.Contains(TEXT("snapshot_restore_component_has_children")));
	TestNotNull(TEXT("parent component remains after refused restore"),
		FBlueprintHelperReviewSnapshotRestoreService::FindScsNodeByName(Blueprint, TEXT("GuardParent")));
	TestNotNull(TEXT("child component remains after refused restore"),
		FBlueprintHelperReviewSnapshotRestoreService::FindScsNodeByName(Blueprint, TEXT("GuardChild")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectComponentLifecycleRootKeepsUnrelatedMixedRecordBranchTest,
	"BlueprintHelper.Review.Action.RejectComponentLifecycleRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewCollectLifecycleDescendantsDeepestFirstTest,
	"BlueprintHelper.Review.Action.CollectLifecycleDescendantsDeepestFirst",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewCollectLifecycleDescendantsDeepestFirstTest::RunTest(const FString& Parameters)
{
	const FString AssetPath = TEXT("/Game/BlueprintHelperReview/BP_DescendantOrder");
	FBlueprintHelperReviewVisibleChange Root =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestVisibleChange(
			TEXT("change_order_root"),
			AssetPath);
	Root.DisplayLabel = TEXT("Root");
	Root.bIsAssetLifecycleRoot = true;
	Root.bRejectRemovesChildren = true;
	Root.ExecutionOrder = 1;

	FBlueprintHelperReviewVisibleChange Child =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestVisibleChange(
			TEXT("change_order_child"),
			AssetPath);
	Child.DisplayLabel = TEXT("Child");
	Child.ParentChangeId = Root.ChangeId;
	Child.bIsAssetLifecycleRoot = true;
	Child.bRejectRemovesChildren = true;
	Child.ExecutionOrder = 2;

	FBlueprintHelperReviewVisibleChange Grandchild =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestVisibleChange(
			TEXT("change_order_grandchild"),
			AssetPath);
	Grandchild.DisplayLabel = TEXT("Grandchild");
	Grandchild.ParentChangeId = Child.ChangeId;
	Grandchild.bIsAssetLifecycleRoot = true;
	Grandchild.bRejectRemovesChildren = true;
	Grandchild.Status = EBlueprintHelperReviewChangeStatus::RejectFailed;
	Grandchild.ExecutionOrder = 3;

	const TArray<FBlueprintHelperReviewVisibleChange> Descendants =
		FBlueprintHelperReviewRejectService::CollectLifecycleDescendantsDeepestFirst(
			Root,
			{ Child, Grandchild, Root });

	TestEqual(TEXT("two descendants collected"), Descendants.Num(), 2);
	if (Descendants.Num() == 2)
	{
		TestEqual(TEXT("grandchild is rejected before child"),
			Descendants[0].ChangeId,
			FString(TEXT("change_order_grandchild")));
		TestEqual(TEXT("child is rejected after grandchild"),
			Descendants[1].ChangeId,
			FString(TEXT("change_order_child")));
	}
	return true;
}

bool FBlueprintHelperReviewRejectComponentLifecycleRootKeepsUnrelatedMixedRecordBranchTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_reject_component_lifecycle"));
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewNamedBlueprint(
		TEXT("RejectComponentLifecycle"));
	TestNotNull(TEXT("component lifecycle blueprint created"), Blueprint);
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		return false;
	}

	USCS_Node* ParentA = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewSceneComponentNode(
		Blueprint,
		TEXT("ParentA"));
	USCS_Node* ParentB = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewSceneComponentNode(
		Blueprint,
		TEXT("ParentB"));
	USCS_Node* ChildA = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewSceneComponentNode(
		Blueprint,
		TEXT("ChildA"),
		ParentA);
	USCS_Node* ChildB = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewSceneComponentNode(
		Blueprint,
		TEXT("ChildB"),
		ParentB);
	TestNotNull(TEXT("ParentA component exists"), ParentA);
	TestNotNull(TEXT("ParentB component exists"), ParentB);
	TestNotNull(TEXT("ChildA component exists"), ChildA);
	TestNotNull(TEXT("ChildB component exists"), ChildB);
	if (!ParentA || !ParentB || !ChildA || !ChildB)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget ParentATarget =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeAddedComponentTarget(
			Blueprint,
			TEXT("ParentA"),
			TEXT("tx_component_parent_a"));
	const FBlueprintHelperReviewAtomicTarget ChildATarget =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeAddedComponentTarget(
			Blueprint,
			TEXT("ChildA"),
			TEXT("tx_component_child_a"),
			TEXT("ParentA"));
	const FBlueprintHelperReviewAtomicTarget ChildBTarget =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeAddedComponentTarget(
			Blueprint,
			TEXT("ChildB"),
			TEXT("tx_component_child_b"),
			TEXT("ParentB"));

	const FBlueprintHelperReviewVisibleChange ParentAChange =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeAddedComponentChange(
			TEXT("change_component_parent_a"),
			TEXT("tx_component_parent_a"),
			ParentATarget,
			1);
	const FBlueprintHelperReviewVisibleChange ChildAChange =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeAddedComponentChange(
			TEXT("change_component_child_a"),
			TEXT("tx_component_child_a"),
			ChildATarget,
			2);
	const FBlueprintHelperReviewVisibleChange ChildBChange =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeAddedComponentChange(
			TEXT("change_component_child_b"),
			TEXT("tx_component_child_b"),
			ChildBTarget,
			3);

	FBlueprintHelperReviewRecord ParentRecord =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewRecordForVisibleChanges(
			ArchiveSessionId + TEXT("_parent"),
			Blueprint->GetPathName(),
			{ParentAChange});
	FBlueprintHelperReviewRecord ChildRecord =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewRecordForVisibleChanges(
			ArchiveSessionId + TEXT("_children"),
			Blueprint->GetPathName(),
			{ChildAChange, ChildBChange});
	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(ParentRecord.ReviewRecordId);
	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(ChildRecord.ReviewRecordId);

	FString SaveError;
	TestTrue(TEXT("component lifecycle parent record saves"), Store.SaveReviewRecord(ParentRecord, SaveError));
	TestTrue(TEXT("component lifecycle child record saves"), Store.SaveReviewRecord(ChildRecord, SaveError));

	const TArray<FBlueprintHelperReviewVisibleChange> PendingChanges =
		Store.LoadPendingVisibleChanges(Blueprint->GetPathName());
	const FBlueprintHelperReviewVisibleChange* Root = PendingChanges.FindByPredicate(
		[](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.DisplayLabel == TEXT("ParentA");
		});
	TestNotNull(TEXT("component lifecycle root is available"), Root);
	if (!Root)
	{
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(ParentRecord.ReviewRecordId);
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(ChildRecord.ReviewRecordId);
		return false;
	}

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewCascadeActionResult Result =
		ActionService.RejectLifecycleRootVisibleChange(*Root, PendingChanges);
	TestTrue(TEXT("component lifecycle root reject succeeds"), Result.RootResult.bSucceeded);
	TestTrue(TEXT("ChildA removed by cascade"), Result.RemovedChildChangeIds.Contains(TEXT("change_component_child_a")));
	TestFalse(TEXT("ChildB remains outside rejected branch"), Result.RemovedChildChangeIds.Contains(TEXT("change_component_child_b")));

	FBlueprintHelperReviewRecord LoadedChildRecord;
	FString LoadError;
	TestTrue(TEXT("mixed child record remains after branch reject"),
		Store.LoadReviewRecordById(ChildRecord.ReviewRecordId, LoadedChildRecord, LoadError));
	const bool bChildAStillPending = LoadedChildRecord.VisibleChanges.ContainsByPredicate(
		[](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.ChangeId == TEXT("change_component_child_a");
		});
	const bool bChildBStillPending = LoadedChildRecord.VisibleChanges.ContainsByPredicate(
		[](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.ChangeId == TEXT("change_component_child_b");
		});
	TestFalse(TEXT("ChildA review event is pruned"), bChildAStillPending);
	TestTrue(TEXT("ChildB review event remains"), bChildBStillPending);
	TestNull(TEXT("ParentA component removed"),
		FBlueprintHelperReviewSnapshotRestoreService::FindScsNodeByName(Blueprint, TEXT("ParentA")));
	TestNull(TEXT("ChildA component removed"),
		FBlueprintHelperReviewSnapshotRestoreService::FindScsNodeByName(Blueprint, TEXT("ChildA")));
	TestNotNull(TEXT("ParentB component remains"),
		FBlueprintHelperReviewSnapshotRestoreService::FindScsNodeByName(Blueprint, TEXT("ParentB")));
	TestNotNull(TEXT("ChildB component remains"),
		FBlueprintHelperReviewSnapshotRestoreService::FindScsNodeByName(Blueprint, TEXT("ChildB")));

	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(ParentRecord.ReviewRecordId);
	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(ChildRecord.ReviewRecordId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectLifecycleRootRemovesChildrenTest,
	"BlueprintHelper.Review.Action.RejectLifecycleRootRemovesChildren",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectLifecycleRootRemovesChildrenTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_reject_lifecycle_root"));
	const FString PackageName = FString::Printf(
		TEXT("/Game/BlueprintHelperReview/LifecycleReject_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString AssetName = FPaths::GetBaseFilename(PackageName);
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		return false;
	}
	UObject* CreatedObject = NewObject<UCurveFloat>(
		Package,
		*AssetName,
		RF_Public | RF_Standalone | RF_Transactional);
	if (!CreatedObject)
	{
		return false;
	}
	const FString ObjectPath = CreatedObject->GetPathName();
	Package->SetDirtyFlag(false);
	const FString AssetPath = PackageName;

	FBlueprintHelperReviewAtomicTarget RootTarget =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewAssetFactoryTarget(
			TEXT("asset_factory:create_asset"),
			TEXT("tx_reject_lifecycle_root"),
			AssetPath,
			TEXT("after_root"));
	FBlueprintHelperWriteReviewEvidence RootEvidence =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
			ArchiveSessionId,
			TEXT("task_reject_lifecycle_root"),
			TEXT("tx_reject_lifecycle_root"),
			AssetPath,
			RootTarget);
	RootEvidence.OperationKind = TEXT("asset_factory");
	RootEvidence.ChangeKind = EBlueprintHelperReviewChangeKind::Added;

	FBlueprintHelperReviewAtomicTarget ChildTarget =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
			TEXT("component:SmokeComp"),
			TEXT("component:SmokeComp"),
			TEXT("tx_reject_lifecycle_child"),
			TEXT("after_child"));
	ChildTarget.AssetPath = AssetPath;
	ChildTarget.Surface = EBlueprintHelperReviewSurface::Components;
	ChildTarget.TargetKind = TEXT("component");
	FBlueprintHelperWriteReviewEvidence ChildEvidence =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
			ArchiveSessionId,
			TEXT("task_reject_lifecycle_root"),
			TEXT("tx_reject_lifecycle_child"),
			AssetPath,
			ChildTarget);

	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(RootEvidence);
	Evidences.Add(ChildEvidence);
	TArray<FBlueprintHelperReviewRecord> Records =
		Store.BuildReviewRecordsFromEvidence(Evidences);
	FString SaveError;
	TestTrue(TEXT("lifecycle root reject record saves"), Store.SaveReviewRecords(Records, SaveError));

	const TArray<FBlueprintHelperReviewVisibleChange> PendingChanges =
		Store.LoadPendingVisibleChanges(AssetPath);
	const FBlueprintHelperReviewVisibleChange* Root = PendingChanges.FindByPredicate(
		[](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.bIsAssetLifecycleRoot;
		});
	TestNotNull(TEXT("lifecycle root is available for cascade reject"), Root);
	if (!Root)
	{
		TArray<UObject*> ObjectsToDelete;
		ObjectsToDelete.Add(CreatedObject);
		ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);
		return false;
	}
	const FBlueprintHelperReviewVisibleChange* Child = PendingChanges.FindByPredicate(
		[](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.LatestEvidenceId == TEXT("tx_reject_lifecycle_child");
		});
	TestNotNull(TEXT("lifecycle child is available for cascade reject"), Child);
	if (!Child)
	{
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(Records[0].ReviewRecordId);
		TArray<UObject*> ObjectsToDelete;
		ObjectsToDelete.Add(CreatedObject);
		ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);
		return false;
	}
	const FString ChildChangeId = Child->ChangeId;

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(TEXT("asset_factory:create_asset"), TEXT("after_root"));

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewCascadeActionResult Result =
		ActionService.RejectLifecycleRootVisibleChange(*Root, PendingChanges);
	TestTrue(TEXT("root reject succeeds"), Result.RootResult.bSucceeded);
	TestTrue(TEXT("cascade reports child removal"), Result.bChildrenRemoved);
	TestTrue(TEXT("child change id is returned"),
		Result.RemovedChildChangeIds.Contains(ChildChangeId));

	FBlueprintHelperReviewRecord Loaded;
	FString LoadError;
	TestFalse(TEXT("successful lifecycle root reject physically removes the review record"),
		Store.LoadReviewRecordById(Records[0].ReviewRecordId, Loaded, LoadError));

	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(Records[0].ReviewRecordId);
	if (UObject* RemainingObject = FindObject<UObject>(nullptr, *ObjectPath))
	{
		TArray<UObject*> ObjectsToDelete;
		ObjectsToDelete.Add(RemainingObject);
		ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAssetRootRejectUsesLifecycleCascadeTest,
	"BlueprintHelper.Review.Action.AssetRootRejectUsesLifecycleCascade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAssetRootRejectUsesLifecycleCascadeTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_reject_asset_root_default"));
	const FString PackageName = FString::Printf(
		TEXT("/Game/BlueprintHelperReview/AssetRootReject_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString AssetName = FPaths::GetBaseFilename(PackageName);
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		return false;
	}
	UObject* CreatedObject = NewObject<UCurveFloat>(
		Package,
		*AssetName,
		RF_Public | RF_Standalone | RF_Transactional);
	if (!CreatedObject)
	{
		return false;
	}
	const FString ObjectPath = CreatedObject->GetPathName();
	Package->SetDirtyFlag(false);

	FBlueprintHelperReviewAtomicTarget RootTarget =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewAssetFactoryTarget(
			TEXT("asset_factory:create_asset"),
			TEXT("tx_reject_asset_root_default"),
			PackageName,
			TEXT("after_root"));
	FBlueprintHelperWriteReviewEvidence RootEvidence =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
			ArchiveSessionId,
			TEXT("task_reject_asset_root_default"),
			TEXT("tx_reject_asset_root_default"),
			PackageName,
			RootTarget);
	RootEvidence.OperationKind = TEXT("asset_factory");
	RootEvidence.ChangeKind = EBlueprintHelperReviewChangeKind::Added;

	FBlueprintHelperReviewAtomicTarget ChildTarget =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
			TEXT("component:SmokeComp"),
			TEXT("component:SmokeComp"),
			TEXT("tx_reject_asset_root_default_child"),
			TEXT("after_child"));
	ChildTarget.AssetPath = PackageName;
	ChildTarget.Surface = EBlueprintHelperReviewSurface::Components;
	ChildTarget.TargetKind = TEXT("component");
	FBlueprintHelperWriteReviewEvidence ChildEvidence =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
			ArchiveSessionId,
			TEXT("task_reject_asset_root_default"),
			TEXT("tx_reject_asset_root_default_child"),
			PackageName,
			ChildTarget);

	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(RootEvidence);
	Evidences.Add(ChildEvidence);
	TArray<FBlueprintHelperReviewRecord> Records = Store.BuildReviewRecordsFromEvidence(Evidences);
	FString SaveError;
	TestTrue(TEXT("asset root default reject record saves"), Store.SaveReviewRecords(Records, SaveError));

	const TArray<FBlueprintHelperReviewVisibleChange> PendingChanges =
		Store.LoadPendingVisibleChanges(PackageName);
	const FBlueprintHelperReviewVisibleChange* Root = PendingChanges.FindByPredicate(
		[](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.bIsAssetLifecycleRoot;
		});
	TestNotNull(TEXT("asset lifecycle root is available for default reject"), Root);
	if (!Root)
	{
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(Records[0].ReviewRecordId);
		TArray<UObject*> ObjectsToDelete;
		ObjectsToDelete.Add(CreatedObject);
		ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);
		return false;
	}

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewCascadeActionResult Result =
		ActionService.RejectLifecycleRootVisibleChange(*Root, PendingChanges);
	TestTrue(TEXT("default asset root reject succeeds"), Result.RootResult.bSucceeded);
	TestTrue(TEXT("default asset root reject removes children"), Result.bChildrenRemoved);
	TestTrue(TEXT("created object is deleted by root reject"),
		FindObject<UObject>(nullptr, *ObjectPath) == nullptr);

	FBlueprintHelperReviewRecord Loaded;
	FString LoadError;
	TestFalse(TEXT("default asset root reject physically removes the review record"),
		Store.LoadReviewRecordById(Records[0].ReviewRecordId, Loaded, LoadError));

	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(Records[0].ReviewRecordId);
	if (UObject* RemainingObject = FindObject<UObject>(nullptr, *ObjectPath))
	{
		TArray<UObject*> ObjectsToDelete;
		ObjectsToDelete.Add(RemainingObject);
		ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectLifecycleRootFailureKeepsChildrenTest,
	"BlueprintHelper.Review.Action.RejectLifecycleRootFailureKeepsChildren",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectLifecycleRootFailureKeepsChildrenTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_reject_lifecycle_failure"));
	const FString AssetPath = TEXT("/Game/BP_DoorLifecycleRejectFailure");

	FBlueprintHelperReviewAtomicTarget RootTarget =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewAssetFactoryTarget(
			TEXT("asset_factory:create_asset"),
			TEXT("tx_reject_lifecycle_failure_root"),
			AssetPath,
			TEXT("after_root"));
	FBlueprintHelperWriteReviewEvidence RootEvidence =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
			ArchiveSessionId,
			TEXT("task_reject_lifecycle_failure"),
			TEXT("tx_reject_lifecycle_failure_root"),
			AssetPath,
			RootTarget);
	RootEvidence.OperationKind = TEXT("asset_factory");
	RootEvidence.ChangeKind = EBlueprintHelperReviewChangeKind::Added;

	FBlueprintHelperReviewAtomicTarget ChildTarget =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
			TEXT("component:SmokeComp"),
			TEXT("component:SmokeComp"),
			TEXT("tx_reject_lifecycle_failure_child"),
			TEXT("after_child"));
	ChildTarget.AssetPath = AssetPath;
	ChildTarget.Surface = EBlueprintHelperReviewSurface::Components;
	ChildTarget.TargetKind = TEXT("component");
	FBlueprintHelperWriteReviewEvidence ChildEvidence =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
			ArchiveSessionId,
			TEXT("task_reject_lifecycle_failure"),
			TEXT("tx_reject_lifecycle_failure_child"),
			AssetPath,
			ChildTarget);

	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(RootEvidence);
	Evidences.Add(ChildEvidence);
	TArray<FBlueprintHelperReviewRecord> Records =
		Store.BuildReviewRecordsFromEvidence(Evidences);
	FString SaveError;
	TestTrue(TEXT("lifecycle root failure record saves"), Store.SaveReviewRecords(Records, SaveError));

	const TArray<FBlueprintHelperReviewVisibleChange> PendingChanges =
		Store.LoadPendingVisibleChanges(AssetPath);
	const FBlueprintHelperReviewVisibleChange* Root = PendingChanges.FindByPredicate(
		[](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.bIsAssetLifecycleRoot;
		});
	TestNotNull(TEXT("lifecycle root is available for failed cascade reject"), Root);
	if (!Root)
	{
		return false;
	}
	const FBlueprintHelperReviewVisibleChange* Child = PendingChanges.FindByPredicate(
		[](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.LatestEvidenceId == TEXT("tx_reject_lifecycle_failure_child");
		});
	TestNotNull(TEXT("lifecycle child is available before failed cascade reject"), Child);
	if (!Child)
	{
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(Records[0].ReviewRecordId);
		return false;
	}
	const FString ChildChangeId = Child->ChangeId;

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(TEXT("asset_factory:create_asset"), TEXT("changed_after_root"));

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewCascadeActionResult Result =
		ActionService.RejectLifecycleRootVisibleChange(*Root, PendingChanges, Options);
	TestFalse(TEXT("root reject failure reports non-success"), Result.RootResult.bSucceeded);
	TestFalse(TEXT("failed root reject removes no children"), Result.bChildrenRemoved);
	TestEqual(TEXT("no child ids are returned after root failure"), Result.RemovedChildChangeIds.Num(), 0);

	FBlueprintHelperReviewRecord Loaded;
	FString LoadError;
	TestTrue(TEXT("failed cascade record reloads"),
		Store.LoadReviewRecordById(Records[0].ReviewRecordId, Loaded, LoadError));
	const FBlueprintHelperReviewVisibleChange* LoadedChild = Loaded.VisibleChanges.FindByPredicate(
		[&ChildChangeId](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.ChangeId == ChildChangeId;
		});
	TestNotNull(TEXT("loaded child exists after root failure"), LoadedChild);
	if (LoadedChild)
	{
		TestEqual(TEXT("child remains pending after root failure"),
			LoadedChild->Status,
			EBlueprintHelperReviewChangeStatus::Pending);
		TestTrue(TEXT("child has no cascade reason after root failure"),
			LoadedChild->NeedsActionReason.IsEmpty());
	}

	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(Records[0].ReviewRecordId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewAcceptLifecycleRootDoesNotAcceptChildrenTest,
	"BlueprintHelper.Review.UI.AcceptLifecycleRootDoesNotAcceptChildren",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAcceptLifecycleRootDoesNotAcceptChildrenTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_accept_lifecycle_root"));
	const FString AssetPath = TEXT("/Game/BP_DoorLifecycleAccept");

	FBlueprintHelperReviewAtomicTarget RootTarget =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewAssetFactoryTarget(
			TEXT("asset_factory:create_asset"),
			TEXT("tx_accept_lifecycle_root"),
			AssetPath);
	FBlueprintHelperWriteReviewEvidence RootEvidence =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
			ArchiveSessionId,
			TEXT("task_accept_lifecycle_root"),
			TEXT("tx_accept_lifecycle_root"),
			AssetPath,
			RootTarget);
	RootEvidence.OperationKind = TEXT("asset_factory");
	RootEvidence.ChangeKind = EBlueprintHelperReviewChangeKind::Added;

	FBlueprintHelperReviewAtomicTarget ChildTarget =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestTarget(
			TEXT("component:SmokeComp"),
			TEXT("component:SmokeComp"),
			TEXT("tx_accept_lifecycle_child"));
	ChildTarget.AssetPath = AssetPath;
	ChildTarget.Surface = EBlueprintHelperReviewSurface::Components;
	ChildTarget.TargetKind = TEXT("component");
	FBlueprintHelperWriteReviewEvidence ChildEvidence =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestEvidence(
			ArchiveSessionId,
			TEXT("task_accept_lifecycle_root"),
			TEXT("tx_accept_lifecycle_child"),
			AssetPath,
			ChildTarget);

	TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
	Evidences.Add(RootEvidence);
	Evidences.Add(ChildEvidence);
	TArray<FBlueprintHelperReviewRecord> Records =
		Store.BuildReviewRecordsFromEvidence(Evidences);
	FString SaveError;
	TestTrue(TEXT("lifecycle root accept record saves"), Store.SaveReviewRecords(Records, SaveError));

	const TArray<FBlueprintHelperReviewVisibleChange> PendingChanges =
		Store.LoadPendingVisibleChanges(AssetPath);
	const FBlueprintHelperReviewVisibleChange* Root = PendingChanges.FindByPredicate(
		[](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.bIsAssetLifecycleRoot;
		});
	TestNotNull(TEXT("lifecycle root is available for accept"), Root);
	if (!Root)
	{
		return false;
	}
	const FBlueprintHelperReviewVisibleChange* Child = PendingChanges.FindByPredicate(
		[](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.LatestEvidenceId == TEXT("tx_accept_lifecycle_child");
		});
	TestNotNull(TEXT("lifecycle child is available for accept"), Child);
	if (!Child)
	{
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(Records[0].ReviewRecordId);
		return false;
	}
	const FString RootChangeId = Root->ChangeId;
	const FString ChildChangeId = Child->ChangeId;

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.AcceptVisibleChange(*Root);
	TestTrue(TEXT("root accept succeeds"), Result.bSucceeded);

	FBlueprintHelperReviewRecord Loaded;
	FString LoadError;
	TestTrue(TEXT("accepted lifecycle root record reloads"),
		Store.LoadReviewRecordById(Records[0].ReviewRecordId, Loaded, LoadError));
	const FBlueprintHelperReviewVisibleChange* LoadedRoot = Loaded.VisibleChanges.FindByPredicate(
		[&RootChangeId](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.ChangeId == RootChangeId;
		});
	const FBlueprintHelperReviewVisibleChange* LoadedChild = Loaded.VisibleChanges.FindByPredicate(
		[&ChildChangeId](const FBlueprintHelperReviewVisibleChange& Change)
		{
			return Change.ChangeId == ChildChangeId;
		});
	TestNotNull(TEXT("accepted root exists"), LoadedRoot);
	TestNotNull(TEXT("child exists after root accept"), LoadedChild);
	if (LoadedRoot)
	{
		TestEqual(TEXT("root is accepted"),
			LoadedRoot->Status,
			EBlueprintHelperReviewChangeStatus::Accepted);
	}
	if (LoadedChild)
	{
		TestEqual(TEXT("child remains pending after root accept"),
			LoadedChild->Status,
			EBlueprintHelperReviewChangeStatus::Pending);
	}

	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(Records[0].ReviewRecordId);
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
	Target.TargetKind = TEXT("unsupported_review_target");
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
	TestEqual(TEXT("debug case links source evidence summary"), DebugCase.EvidenceLinks.Num(), 1);
	if (DebugCase.EvidenceLinks.Num() == 1)
	{
		TestEqual(TEXT("debug case links source evidence id"),
			DebugCase.EvidenceLinks[0].EvidenceId,
			FString(TEXT("tx_reject_debug_needs_action")));
		TestEqual(TEXT("debug case marks evidence link role"),
			DebugCase.EvidenceLinks[0].Role,
			FString(TEXT("review_reject_failed")));
	}
	TestTrue(TEXT("debug case message carries reject reason"),
		DebugCase.Error.Message.Contains(TEXT("snapshot_restore_unsupported_target_kind")));
	TestFalse(TEXT("debug case message does not treat hash drift as blocking"),
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
	TestEqual(TEXT("reject-failed debug case links source evidence summary"), DebugCase.EvidenceLinks.Num(), 1);
	if (DebugCase.EvidenceLinks.Num() == 1)
	{
		TestEqual(TEXT("reject-failed debug case links source evidence id"),
			DebugCase.EvidenceLinks[0].EvidenceId,
			FString(TEXT("tx_reject_debug_failed")));
		TestEqual(TEXT("reject-failed debug case marks evidence link role"),
			DebugCase.EvidenceLinks[0].Role,
			FString(TEXT("review_reject_failed")));
	}
	TestTrue(TEXT("debug case message carries rollback failure"),
		DebugCase.Error.Message.Contains(TEXT("rollback_backend_failed")));
	IFileManager::Get().Delete(*FBlueprintHelperDebugCaseStoreService::GetCasePath(Loaded.DebugCaseIds[0]), false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewRejectAllRecordsHashDiagnosticTest,
	"BlueprintHelper.Review.Action.RejectAllRecordsHashDiagnostic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewRejectAllRecordsHashDiagnosticTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveSessionId = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_reject_all"));
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("RejectAllHashDiagnostic"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* Node = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("RejectAllHashDiagnosticNode"));
	TestNotNull(TEXT("graph node created"), Node);
	if (!Node)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	FString TargetError;
	TestTrue(TEXT("reject all hash diagnostic fixture has recoverable snapshot"),
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::PopulateReviewNodeCommentTargetFromSnapshot(
			Blueprint,
			Graph,
			Node,
			TEXT("tx_reject_all"),
			Target,
			TargetError));
	if (Target.BeforeSnapshotJson.IsEmpty() || Target.RecordedAfterHash.IsEmpty())
	{
		if (!TargetError.IsEmpty())
		{
			AddError(TargetError);
		}
		return false;
	}
	FBlueprintHelperReviewRecord Record =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewRecordForVisibleChanges(
			ArchiveSessionId,
			Target.AssetPath,
			{
				FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewVisibleChangeForTarget(
					TEXT("change_reject_all"),
					TEXT("tx_reject_all"),
					Target)
			});
	FString SaveError;
	TestTrue(TEXT("record saved before reject all"), Store.SaveReviewRecord(Record, SaveError));

	FBlueprintHelperReviewRecordQuery Query;
	Query.ArchiveSessionIdFilter = ArchiveSessionId;
	Query.bPendingOnly = true;

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(Target.TargetKey, TEXT("user_changed"));

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.RejectAll(Query, Options);
	TestTrue(TEXT("reject all succeeds when current hash drift is diagnostic"), Result.bSucceeded);
	TestEqual(TEXT("reject all reports rejected after snapshot restore"),
		Result.NewStatus,
		EBlueprintHelperReviewChangeStatus::Rejected);

	FBlueprintHelperReviewRecordQuery LoadQuery;
	LoadQuery.ArchiveSessionIdFilter = ArchiveSessionId;
	LoadQuery.bPendingOnly = false;
	const TArray<FBlueprintHelperReviewRecord> LoadedRecords = Store.QueryReviewRecords(LoadQuery);
	TestEqual(TEXT("successful reject all removes completed review record"), LoadedRecords.Num(), 0);
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
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(TEXT("RejectAllIterates"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* FirstNode = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("RejectAllIteratesFirst"));
	UK2Node_CustomEvent* SecondNode = FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("RejectAllIteratesSecond"));
	TestNotNull(TEXT("first graph node created"), FirstNode);
	TestNotNull(TEXT("second graph node created"), SecondNode);
	if (!FirstNode || !SecondNode)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget FirstTarget;
	FBlueprintHelperReviewAtomicTarget SecondTarget;
	FString FirstTargetError;
	FString SecondTargetError;
	TestTrue(TEXT("first reject all target has recoverable snapshot"),
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::PopulateReviewNodeCommentTargetFromSnapshot(
			Blueprint,
			Graph,
			FirstNode,
			TEXT("tx_reject_all_iterates"),
			FirstTarget,
			FirstTargetError));
	TestTrue(TEXT("second reject all target has recoverable snapshot"),
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::PopulateReviewNodeCommentTargetFromSnapshot(
			Blueprint,
			Graph,
			SecondNode,
			TEXT("tx_reject_all_iterates"),
			SecondTarget,
			SecondTargetError));
	if (FirstTarget.BeforeSnapshotJson.IsEmpty()
		|| FirstTarget.RecordedAfterHash.IsEmpty()
		|| SecondTarget.BeforeSnapshotJson.IsEmpty()
		|| SecondTarget.RecordedAfterHash.IsEmpty())
	{
		if (!FirstTargetError.IsEmpty())
		{
			AddError(FirstTargetError);
		}
		if (!SecondTargetError.IsEmpty())
		{
			AddError(SecondTargetError);
		}
		return false;
	}
	FBlueprintHelperReviewRecord Record =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewRecordForVisibleChanges(
			ArchiveSessionId,
			FirstTarget.AssetPath,
			{
				FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewVisibleChangeForTarget(
					TEXT("change_reject_all_iterates_first"),
					TEXT("tx_reject_all_iterates"),
					FirstTarget),
				FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewVisibleChangeForTarget(
					TEXT("change_reject_all_iterates_second"),
					TEXT("tx_reject_all_iterates"),
					SecondTarget)
			});
	FString SaveError;
	TestTrue(TEXT("record saved before reject all iteration"), Store.SaveReviewRecord(Record, SaveError));

	FBlueprintHelperReviewRecordQuery Query;
	Query.ArchiveSessionIdFilter = ArchiveSessionId;
	Query.bPendingOnly = true;

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(FirstTarget.TargetKey, FirstTarget.RecordedAfterHash);
	Options.CurrentHashesByTargetKey.Add(SecondTarget.TargetKey, SecondTarget.RecordedAfterHash);

	FBlueprintHelperReviewActionService ActionService;
	const FBlueprintHelperReviewActionResult Result = ActionService.RejectAll(Query, Options);
	TestTrue(TEXT("reject all succeeds when every pending target rolls back"), Result.bSucceeded);
	TestEqual(TEXT("reject all reports rejected"),
		Result.NewStatus,
		EBlueprintHelperReviewChangeStatus::Rejected);

	FBlueprintHelperReviewRecordQuery LoadQuery;
	LoadQuery.ArchiveSessionIdFilter = ArchiveSessionId;
	LoadQuery.bPendingOnly = false;
	TestEqual(TEXT("successful reject all physically removes matching records"),
		Store.QueryReviewRecords(LoadQuery).Num(),
		0);
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
	Change.LatestEvidenceId = TEXT("tx_visible");
	Change.SourceEvidenceIds.Add(TEXT("tx_visible"));
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
	FBlueprintHelperReviewTreeNestsChangesUnderLifecycleRootTest,
	"BlueprintHelper.Review.UI.TreeNestsChangesUnderLifecycleRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewTreeNestsChangesUnderLifecycleRootTest::RunTest(const FString& Parameters)
{
	const FString AssetPath = TEXT("/Game/BP_DoorTreeLifecycle");

	FBlueprintHelperReviewVisibleChange Root;
	Root.ChangeId = TEXT("tx_tree_root");
	Root.AssetPath = AssetPath;
	Root.LatestEvidenceId = TEXT("tx_tree_root");
	Root.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
	Root.DisplayLabel = TEXT("BP_DoorTreeLifecycle asset");
	Root.bIsAssetLifecycleRoot = true;
	Root.bRejectRemovesChildren = true;

	FBlueprintHelperReviewVisibleChange Child;
	Child.ChangeId = TEXT("tx_tree_child");
	Child.AssetPath = AssetPath;
	Child.LatestEvidenceId = TEXT("tx_tree_child");
	Child.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	Child.DisplayLabel = TEXT("SmokeComp");
	Child.ParentChangeId = Root.ChangeId;

	FBlueprintHelperReviewVisibleChange DirectChild;
	DirectChild.ChangeId = TEXT("tx_tree_direct");
	DirectChild.AssetPath = AssetPath;
	DirectChild.LatestEvidenceId = TEXT("tx_tree_direct");
	DirectChild.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	DirectChild.DisplayLabel = TEXT("Unparented");

	TArray<FBlueprintHelperReviewVisibleChange> InitialChanges;
	InitialChanges.Add(Root);
	InitialChanges.Add(Child);
	InitialChanges.Add(DirectChild);

	const TArray<SBlueprintHelperReviewPanel::FReviewTreeSnapshotEntry> Snapshot =
		SBlueprintHelperReviewPanel::BuildReviewTreeSnapshotForTesting(InitialChanges);
	TestEqual(TEXT("asset header plus root, nested child, direct child"),
		Snapshot.Num(),
		4);
	if (Snapshot.Num() != 4)
	{
		return false;
	}

	TestTrue(TEXT("first entry is asset header"), Snapshot[0].bIsAssetHeader);
	TestEqual(TEXT("asset header depth"), Snapshot[0].Depth, 0);
	TestEqual(TEXT("root depth"), Snapshot[1].Depth, 1);
	TestEqual(TEXT("root change id"), Snapshot[1].ChangeId, Root.ChangeId);
	TestEqual(TEXT("child nests under root"), Snapshot[2].Depth, 2);
	TestEqual(TEXT("nested child id"), Snapshot[2].ChangeId, Child.ChangeId);
	TestEqual(TEXT("unparented child stays directly under asset"), Snapshot[3].Depth, 1);
	TestEqual(TEXT("direct child id"), Snapshot[3].ChangeId, DirectChild.ChangeId);
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
	FBlueprintHelperReviewAssetContextLoadsStructureTest,
	"BlueprintHelper.Review.UI.AssetContextLoadsStructure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewAssetContextLoadsStructureTest::RunTest(const FString& Parameters)
{
	UUserDefinedStruct* Structure =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewUserDefinedStruct(TEXT("ReviewAssetContextStructure"));
	TestNotNull(TEXT("Structure fixture exists"), Structure);
	if (!Structure)
	{
		return false;
	}

	const FBlueprintHelperReviewAssetContext Context =
		FBlueprintHelperReviewAssetContext::LoadForAssetPath(Structure->GetPathName());
	TestTrue(TEXT("Structure context is valid"), Context.IsValid());
	TestEqual(TEXT("Structure context kind"),
		static_cast<int32>(Context.AssetKind),
		static_cast<int32>(EBlueprintHelperReviewAssetKind::Structure));
	TestTrue(TEXT("Structure context object matches fixture"),
		Context.Structure.Get() == Structure);
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
	FBlueprintHelperReviewDataTablePresenterState State;
	TSharedRef<SWidget> Widget = FBlueprintHelperReviewDataTablePresenter::BuildContent(Context, State);
	TestTrue(TEXT("DataTable presenter content is constructed"), Widget != SNullWidget::NullWidget);
	TestTrue(TEXT("DataTable presenter uses cached native columns"), State.Columns.Num() > 0);
	TestTrue(TEXT("DataTable presenter uses cached native rows"), State.Rows.Num() > 0);
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
	FBlueprintHelperReviewDataAssetPresenterState State;
	TSharedRef<SWidget> Widget = FBlueprintHelperReviewDataAssetPresenter::BuildContent(Context, State);
	TestTrue(TEXT("DataAsset presenter content is constructed"), Widget != SNullWidget::NullWidget);
	TestTrue(TEXT("DataAsset presenter owns native property rows"), State.Rows.Num() > 0);
	TestTrue(TEXT("DataAsset presenter owns property row generator"), State.PropertyRowGenerator.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewStructurePresenterBuildsReadonlyRowsTest,
	"BlueprintHelper.Review.UI.StructurePresenterBuildsReadonlyRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewStructurePresenterBuildsReadonlyRowsTest::RunTest(const FString& Parameters)
{
	UUserDefinedStruct* Structure =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewUserDefinedStruct(TEXT("ReviewStructurePresenterContent"));
	TestNotNull(TEXT("Structure fixture exists"), Structure);
	if (!Structure)
	{
		return false;
	}

	const FBlueprintHelperReviewAssetContext Context =
		FBlueprintHelperReviewAssetContext::LoadForAssetPath(Structure->GetPathName());
	FBlueprintHelperReviewDataAssetPresenterState State;
	TSharedRef<SWidget> Widget = FBlueprintHelperReviewDataAssetPresenter::BuildContent(Context, State);
	TestTrue(TEXT("Structure presenter content is constructed"), Widget != SNullWidget::NullWidget);
	TestEqual(TEXT("Structure presenter asset kind"),
		static_cast<int32>(Context.AssetKind),
		static_cast<int32>(EBlueprintHelperReviewAssetKind::Structure));
	TestTrue(TEXT("Structure presenter owns structure rows"), State.Rows.Num() > 0);
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
	Change.LatestEvidenceId = TEXT("tx_object_visible");
	Change.SourceEvidenceIds.Add(TEXT("tx_object_visible"));
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
	Change.LatestEvidenceId = TEXT("tx_datatable_visible");
	Change.SourceEvidenceIds.Add(TEXT("tx_datatable_visible"));
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
	Change.LatestEvidenceId = TEXT("tx_datatable_row_visible");
	Change.SourceEvidenceIds.Add(TEXT("tx_datatable_row_visible"));
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
	Change.LatestEvidenceId = TEXT("tx_generic_object_visible");
	Change.SourceEvidenceIds.Add(TEXT("tx_generic_object_visible"));
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
	Change.LatestEvidenceId = TEXT("tx_data_asset_property_visible");
	Change.SourceEvidenceIds.Add(TEXT("tx_data_asset_property_visible"));
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
	Change.LatestEvidenceId = TEXT("tx_widget_blueprint_visible");
	Change.SourceEvidenceIds.Add(TEXT("tx_widget_blueprint_visible"));
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
	Change.LatestEvidenceId = TEXT("tx_signature_only_panel");
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
	Change.LatestEvidenceId = TEXT("tx_1778317276165");
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
	TestTrue(TEXT("recorded graph bounds without structured anchor reports no anchor source"),
		RecordedDebugSummary.Contains(TEXT("anchorSource=none")));
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

	FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(GetTransientPackage());
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

	FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(GetTransientPackage());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPresenterTypedStoreChangedEventTest,
	"BlueprintHelper.Review.Panel.Presenter.ForwardsTypedStoreChangedEvent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPresenterTypedStoreChangedEventTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewPanelPresenter Presenter(&Store, nullptr);

	TArray<FBlueprintHelperReviewStoreChangedEvent> Events;
	FDelegateHandle Handle = Presenter.AddPendingReviewChangedEventHandler(
		FBlueprintHelperReviewStoreChangedMulticast::FDelegate::CreateLambda(
			[&Events](const FBlueprintHelperReviewStoreChangedEvent& Event)
			{
				Events.Add(Event);
			}));

	const FBlueprintHelperReviewStoreChangedEvent SourceEvent =
		FBlueprintHelperReviewStoreChangedEvent::RecordsChanged(
			{ TEXT("record_typed_event") },
			{ TEXT("change_typed_event") },
			{ TEXT("/Game/BlueprintHelperReview/BP_TypedEvent") });
	Store.NotifyPendingReviewChanged(SourceEvent);
	Presenter.RemovePendingReviewChangedEventHandler(Handle);

	TestEqual(TEXT("typed event is forwarded once"), Events.Num(), 1);
	if (Events.Num() == 1)
	{
		TestFalse(TEXT("typed event is not downgraded to full reload"), Events[0].bRequiresFullReload);
		TestEqual(TEXT("typed event keeps record id"), Events[0].ReviewRecordIds[0], FString(TEXT("record_typed_event")));
		TestEqual(TEXT("typed event keeps change id"), Events[0].ChangeIds[0], FString(TEXT("change_typed_event")));
		TestEqual(TEXT("typed event keeps asset path"),
			Events[0].AssetPaths[0],
			FString(TEXT("/Game/BlueprintHelperReview/BP_TypedEvent")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewCommandSingleAcceptPublishesIncrementalEventTest,
	"BlueprintHelper.Review.Panel.Command.SingleAcceptPublishesIncrementalEvent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewCommandSingleAcceptPublishesIncrementalEventTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewActionService ActionService;
	FBlueprintHelperReviewPanelCommandService CommandService(&ActionService, &Store);

	const FString ArchiveId =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_single_accept_event"));
	const FString AssetPath = FString::Printf(
		TEXT("/Game/BlueprintHelperReview/BP_SingleAcceptEvent_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	FBlueprintHelperReviewVisibleChange Change =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestVisibleChange(
			TEXT("change_single_accept_event"),
			AssetPath);

	FBlueprintHelperReviewRecord Record;
	Record.ReviewRecordId = FBlueprintHelperReviewStoreService::MakeReviewRecordId(ArchiveId, AssetPath);
	Record.ArchiveSessionId = ArchiveId;
	Record.AssetPath = AssetPath;
	Record.Status = EBlueprintHelperReviewChangeStatus::Pending;
	Record.StorageStatus = EBlueprintHelperReviewStorageStatus::Active;
	Record.VisibleChanges.Add(Change);
	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(Record.ReviewRecordId);

	FString SaveError;
	TestTrue(TEXT("record saves before single accept"), Store.SaveReviewRecord(Record, SaveError));

	TArray<FBlueprintHelperReviewStoreChangedEvent> Events;
	FDelegateHandle Handle = Store.AddPendingReviewChangedEventHandler(
		FBlueprintHelperReviewStoreChangedMulticast::FDelegate::CreateLambda(
			[&Events](const FBlueprintHelperReviewStoreChangedEvent& Event)
			{
				Events.Add(Event);
			}));

	const FBlueprintHelperReviewActionIntent Intent = FBlueprintHelperReviewActionIntent::Accept(
		FBlueprintHelperReviewPanelStateService::MakeChangeBinding(
			Change,
			EBlueprintHelperReviewSurface::Unknown,
			Change.LocationKey),
		TEXT("test"));
	const FBlueprintHelperReviewCommandResult Result =
		CommandService.ExecuteActionIntent(Intent, { Change });
	Store.RemovePendingReviewChangedEventHandler(Handle);

	TestTrue(TEXT("single accept succeeds"), Result.ActionResult.bSucceeded);
	TestEqual(TEXT("single accept publishes one event"), Events.Num(), 1);
	if (Events.Num() == 1)
	{
		TestFalse(TEXT("single accept event is incremental"), Events[0].bRequiresFullReload);
		TestTrue(TEXT("single accept event includes record id"),
			Events[0].ReviewRecordIds.Contains(Record.ReviewRecordId));
		TestTrue(TEXT("single accept event includes change id"),
			Events[0].ChangeIds.Contains(Change.ChangeId));
		TestTrue(TEXT("single accept event includes asset path"),
			Events[0].AssetPaths.Contains(AssetPath));
	}

	FString DeleteError;
	Store.DeleteReviewRecord(Record.ReviewRecordId, DeleteError);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewCommandAssetAcceptUsesPendingIndexTest,
	"BlueprintHelper.Review.Panel.Command.AssetAcceptUsesPendingIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewCommandAssetAcceptUsesPendingIndexTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewActionService ActionService;
	FBlueprintHelperReviewPanelCommandService CommandService(&ActionService, &Store);

	const FString ArchiveId =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_asset_accept_index"));
	const FString AssetPath = FString::Printf(
		TEXT("/Game/BlueprintHelperReview/BP_AssetAcceptIndex_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));

	TArray<FBlueprintHelperReviewVisibleChange> Changes;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FBlueprintHelperReviewVisibleChange Change =
			FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestVisibleChange(
				FString::Printf(TEXT("change_asset_accept_index_%d"), Index),
				AssetPath);
		const FString TargetKey = FString::Printf(TEXT("graph_node:AssetAcceptIndex_%d"), Index);
		Change.LocationKey = TargetKey;
		Change.AtomicTargets[0].TargetKey = TargetKey;
		Change.AtomicTargets[0].VisualGroupKey = TargetKey;
		Changes.Add(Change);
	}

	FBlueprintHelperReviewRecord Record;
	Record.ReviewRecordId = FBlueprintHelperReviewStoreService::MakeReviewRecordId(ArchiveId, AssetPath);
	Record.ArchiveSessionId = ArchiveId;
	Record.AssetPath = AssetPath;
	Record.Status = EBlueprintHelperReviewChangeStatus::Pending;
	Record.StorageStatus = EBlueprintHelperReviewStorageStatus::Active;
	Record.VisibleChanges = Changes;
	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(Record.ReviewRecordId);

	FString SaveError;
	TestTrue(TEXT("record saves before asset accept index batch"), Store.SaveReviewRecord(Record, SaveError));

	TArray<FBlueprintHelperReviewStoreChangedEvent> Events;
	FDelegateHandle Handle = Store.AddPendingReviewChangedEventHandler(
		FBlueprintHelperReviewStoreChangedMulticast::FDelegate::CreateLambda(
			[&Events](const FBlueprintHelperReviewStoreChangedEvent& Event)
			{
				Events.Add(Event);
			}));

	const FBlueprintHelperReviewCommandBatchResult Result =
		CommandService.AcceptPendingVisibleChangesForAsset(AssetPath);
	Store.RemovePendingReviewChangedEventHandler(Handle);

	TestTrue(TEXT("asset accept index batch succeeds"), Result.BatchActionResult.bSucceeded);
	TestEqual(TEXT("asset accept index batch covers unloaded pending changes"),
		Result.BatchActionResult.RequestedCount,
		3);
	TestEqual(TEXT("asset accept index batch accepts all changes"),
		Result.BatchActionResult.SucceededCount,
		3);
	TestEqual(TEXT("asset accept index batch emits one store event"), Events.Num(), 1);
	if (Events.Num() == 1)
	{
		for (const FBlueprintHelperReviewVisibleChange& Change : Changes)
		{
			TestTrue(TEXT("asset accept index event includes each change id"),
				Events[0].ChangeIds.Contains(Change.ChangeId));
		}
	}
	TestEqual(TEXT("asset accept index batch clears pending changes"),
		Store.LoadPendingVisibleChanges(AssetPath).Num(),
		0);
	FString DeleteError;
	Store.DeleteReviewRecord(Record.ReviewRecordId, DeleteError);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewCommandAssetRejectUsesPendingIndexTest,
	"BlueprintHelper.Review.Panel.Command.AssetRejectUsesPendingIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewCommandAssetRejectUsesPendingIndexTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveId =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_asset_reject_index"));
	UBlueprint* Blueprint = FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewConversionTestBlueprint(
		TEXT("AssetRejectIndex"));
	TestNotNull(TEXT("test blueprint created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	UK2Node_CustomEvent* FirstNode =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("AssetRejectIndexFirst"));
	UK2Node_CustomEvent* SecondNode =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::AddReviewConversionEventNode(Graph, TEXT("AssetRejectIndexSecond"));
	TestNotNull(TEXT("first graph node created"), FirstNode);
	TestNotNull(TEXT("second graph node created"), SecondNode);
	if (!FirstNode || !SecondNode)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget FirstTarget;
	FBlueprintHelperReviewAtomicTarget SecondTarget;
	FString FirstTargetError;
	FString SecondTargetError;
	TestTrue(TEXT("first asset reject index target has recoverable snapshot"),
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::PopulateReviewNodeCommentTargetFromSnapshot(
			Blueprint,
			Graph,
			FirstNode,
			TEXT("tx_asset_reject_index"),
			FirstTarget,
			FirstTargetError));
	TestTrue(TEXT("second asset reject index target has recoverable snapshot"),
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::PopulateReviewNodeCommentTargetFromSnapshot(
			Blueprint,
			Graph,
			SecondNode,
			TEXT("tx_asset_reject_index"),
			SecondTarget,
			SecondTargetError));
	if (FirstTarget.BeforeSnapshotJson.IsEmpty()
		|| FirstTarget.RecordedAfterHash.IsEmpty()
		|| SecondTarget.BeforeSnapshotJson.IsEmpty()
		|| SecondTarget.RecordedAfterHash.IsEmpty())
	{
		if (!FirstTargetError.IsEmpty())
		{
			AddError(FirstTargetError);
		}
		if (!SecondTargetError.IsEmpty())
		{
			AddError(SecondTargetError);
		}
		return false;
	}

	FBlueprintHelperReviewRecord Record =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewRecordForVisibleChanges(
			ArchiveId,
			FirstTarget.AssetPath,
			{
				FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewVisibleChangeForTarget(
					TEXT("change_asset_reject_index_first"),
					TEXT("tx_asset_reject_index"),
					FirstTarget),
				FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewVisibleChangeForTarget(
					TEXT("change_asset_reject_index_second"),
					TEXT("tx_asset_reject_index"),
					SecondTarget)
			});
	FString SaveError;
	TestTrue(TEXT("record saves before asset reject index batch"), Store.SaveReviewRecord(Record, SaveError));

	FBlueprintHelperReviewRejectOptions Options;
	Options.CurrentHashesByTargetKey.Add(FirstTarget.TargetKey, FirstTarget.RecordedAfterHash);
	Options.CurrentHashesByTargetKey.Add(SecondTarget.TargetKey, SecondTarget.RecordedAfterHash);

	FBlueprintHelperReviewActionService ActionService;
	FBlueprintHelperReviewPanelCommandService CommandService(&ActionService, &Store);
	const FBlueprintHelperReviewCommandBatchResult Result =
		CommandService.RejectPendingVisibleChangesForAsset(FirstTarget.AssetPath, Options);

	TestTrue(TEXT("asset reject index batch succeeds"), Result.BatchActionResult.bSucceeded);
	TestEqual(TEXT("asset reject index batch covers unloaded pending changes"),
		Result.BatchActionResult.RequestedCount,
		2);
	TestEqual(TEXT("asset reject index batch rejects all changes"),
		Result.BatchActionResult.SucceededCount,
		2);
	TestEqual(TEXT("asset reject index batch clears pending changes"),
		Store.LoadPendingVisibleChanges(FirstTarget.AssetPath).Num(),
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewActionTargetBatchResolveTest,
	"BlueprintHelper.Review.Panel.Command.BatchResolveUsesSingleIndexPass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewActionTargetBatchResolveTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveId =
		FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeUniqueReviewArchiveId(TEXT("archive_batch_resolve"));
	const FString AssetPath = FString::Printf(
		TEXT("/Game/BlueprintHelperReview/BP_BatchResolve_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));

	TArray<FBlueprintHelperReviewVisibleChange> Changes;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FBlueprintHelperReviewVisibleChange Change =
			FBlueprintHelperReviewStoreServiceTestsLocalUtils::MakeReviewTestVisibleChange(
				FString::Printf(TEXT("change_batch_resolve_%d"), Index),
				AssetPath);
		Change.AtomicTargets[0].TargetKey = FString::Printf(TEXT("graph_node:BatchResolve_%d"), Index);
		Changes.Add(Change);
	}

	FBlueprintHelperReviewRecord Record;
	Record.ReviewRecordId = FBlueprintHelperReviewStoreService::MakeReviewRecordId(ArchiveId, AssetPath);
	Record.ArchiveSessionId = ArchiveId;
	Record.AssetPath = AssetPath;
	Record.Status = EBlueprintHelperReviewChangeStatus::Pending;
	Record.StorageStatus = EBlueprintHelperReviewStorageStatus::Active;
	Record.VisibleChanges = Changes;
	FBlueprintHelperReviewStoreServiceTestsLocalUtils::DeleteReviewRecordFile(Record.ReviewRecordId);

	FString SaveError;
	TestTrue(TEXT("record saves before batch resolve"), Store.SaveReviewRecord(Record, SaveError));

	const TArray<FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch> Matches =
		FBlueprintHelperReviewActionTargetUtils::ResolvePersistedReviewTargetMatchesBatch(Changes);
	TestEqual(TEXT("batch resolve returns one record match"), Matches.Num(), 1);
	if (Matches.Num() == 1)
	{
		TestEqual(TEXT("batch resolve keeps record id"), Matches[0].ReviewRecordId, Record.ReviewRecordId);
		TestEqual(TEXT("batch resolve keeps all target keys"), Matches[0].TargetKeys.Num(), 3);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			TestTrue(
				FString::Printf(TEXT("batch resolve includes target %d"), Index),
				Matches[0].TargetKeys.Contains(FString::Printf(TEXT("graph_node:BatchResolve_%d"), Index)));
		}
	}

	FString DeleteError;
	Store.DeleteReviewRecord(Record.ReviewRecordId, DeleteError);
	return true;
}

#endif
