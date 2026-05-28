// BlueprintHelper Review utility functions implementation.
// Consolidates all anonymous namespace functions from Systems/Review/*.cpp files.

#include "Systems/Review/Utils/BlueprintHelperReviewUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Shared/Review/BlueprintHelperReviewEnumUtils.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h"
#include "Systems/Review/BlueprintHelperReviewConfigResolver.h"
#include "Systems/Review/Utils/BlueprintHelperReviewStoreMergeUtils.h"
#include "Systems/Review/Utils/BlueprintHelperReviewStoreTargetUtils.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintHelperReviewUtils, Log, All);

// ====================================================================
// ConfigResolver
// ====================================================================

FString UBlueprintHelperReviewUtils::BlueprintHelperResolveProjectPath(FString Path, const FString& DefaultRelativePath)
{
	Path.TrimStartAndEndInline();
	if (Path.IsEmpty())
	{
		Path = DefaultRelativePath;
	}

	FString ResolvedPath = FPaths::IsRelative(Path)
		? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Path)
		: Path;
	FPaths::NormalizeFilename(ResolvedPath);
	FPaths::CollapseRelativeDirectories(ResolvedPath);
	return ResolvedPath;
}

FString UBlueprintHelperReviewUtils::BlueprintHelperNormalizeVersion(FString Version)
{
	Version.TrimStartAndEndInline();
	return Version.IsEmpty() ? FString(TEXT("v2")) : Version;
}

// ====================================================================
// StoreService
// ====================================================================

FSimpleMulticastDelegate& UBlueprintHelperReviewUtils::BlueprintHelperReviewPendingReviewChangedDelegate()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}

void UBlueprintHelperReviewUtils::NormalizeReviewTargetSemanticSnapshots(
	const FBlueprintHelperWriteReviewEvidence& Evidence,
	FBlueprintHelperReviewAtomicTarget& Target)
{
	FBlueprintHelperReviewBaselineSnapshotService SnapshotService;

	if (!Target.BeforeSnapshotJson.IsEmpty())
	{
		Target.BaselineHash =
			FBlueprintHelperReviewBaselineSnapshotService::ComputeSemanticSnapshotHash(Target.BeforeSnapshotJson);
	}
	else
	{
		FString BaselineSnapshotJson;
		FString BaselineSnapshotHash;
		FString BaselineSnapshotError;
		if (SnapshotService.TryLoadBaselineTargetSnapshot(
			Evidence.ArchiveSessionId,
			Target,
			BaselineSnapshotJson,
			BaselineSnapshotHash,
			BaselineSnapshotError))
		{
			Target.BeforeSnapshotJson = BaselineSnapshotJson;
			Target.BaselineHash = BaselineSnapshotHash;
		}
		else if (Evidence.ChangeKind == EBlueprintHelperReviewChangeKind::Added
			&& !FBlueprintHelperReviewTargetKindRegistry::SupportsSnapshotRestore(Target.TargetKind))
		{
			FBlueprintHelperReviewBaselineSnapshotService::MakeMissingTargetSnapshot(
				Target,
				true,
				Target.BeforeSnapshotJson,
				Target.BaselineHash);
		}
	}

	if (!Target.AfterSnapshotJson.IsEmpty())
	{
		Target.RecordedAfterHash =
			FBlueprintHelperReviewBaselineSnapshotService::ComputeSemanticSnapshotHash(Target.AfterSnapshotJson);
	}
	else if (Target.RecordedAfterHash.IsEmpty())
	{
		FString AfterSnapshotJson;
		FString AfterSnapshotHash;
		FString AfterSnapshotError;
		if (SnapshotService.CaptureTargetSnapshot(Target, AfterSnapshotJson, AfterSnapshotHash, AfterSnapshotError))
		{
			Target.AfterSnapshotJson = AfterSnapshotJson;
			Target.RecordedAfterHash = AfterSnapshotHash;
		}
	}
}

// ====================================================================
// BaselineSnapshotService
// ====================================================================

FString UBlueprintHelperReviewUtils::BlueprintHelperReviewMakeStableTextKeyForSnapshot(const FText& Text)
{
	if (Text.IsEmptyOrWhitespace() || Text.EqualTo(UEdGraphSchema_K2::VR_DefaultCategory))
	{
		return TEXT("ue_default_variable_category");
	}

	const TOptional<FString> Namespace = FTextInspector::GetNamespace(Text);
	const TOptional<FString> Key = FTextInspector::GetKey(Text);
	if (Namespace.IsSet() && Key.IsSet())
	{
		return FString::Printf(TEXT("loc:%s:%s"), *Namespace.GetValue(), *Key.GetValue());
	}

	if (const FString* Source = FTextInspector::GetSourceString(Text))
	{
		return *Source;
	}

	return FTextInspector::GetDisplayString(Text);
}

TSharedPtr<FJsonObject> UBlueprintHelperReviewUtils::CloneReviewSnapshotObjectForHash(const TSharedPtr<FJsonObject>& Source)
{
	TSharedPtr<FJsonObject> Clone = MakeShared<FJsonObject>();
	if (!Source.IsValid())
	{
		return Clone;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Source->Values)
	{
		if (Field.Key.Equals(TEXT("restore_text"), ESearchCase::IgnoreCase))
		{
			continue;
		}
		Clone->SetField(Field.Key, UBlueprintHelperReviewUtils::CloneReviewSnapshotValueForHash(Field.Value));
	}
	return Clone;
}

TSharedPtr<FJsonValue> UBlueprintHelperReviewUtils::CloneReviewSnapshotValueForHash(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		return MakeShared<FJsonValueNull>();
	}

	switch (Value->Type)
	{
	case EJson::Object:
		return MakeShared<FJsonValueObject>(UBlueprintHelperReviewUtils::CloneReviewSnapshotObjectForHash(Value->AsObject()));
	case EJson::Array:
	{
		TArray<TSharedPtr<FJsonValue>> ClonedArray;
		const TArray<TSharedPtr<FJsonValue>> SourceArray = Value->AsArray();
		for (const TSharedPtr<FJsonValue>& Entry : SourceArray)
		{
			ClonedArray.Add(UBlueprintHelperReviewUtils::CloneReviewSnapshotValueForHash(Entry));
		}
		return MakeShared<FJsonValueArray>(ClonedArray);
	}
	case EJson::String:
		return MakeShared<FJsonValueString>(Value->AsString());
	case EJson::Number:
		return MakeShared<FJsonValueNumber>(Value->AsNumber());
	case EJson::Boolean:
		return MakeShared<FJsonValueBoolean>(Value->AsBool());
	default:
		return MakeShared<FJsonValueNull>();
	}
}

FString UBlueprintHelperReviewUtils::ExtractReviewSnapshotAnchorName(const FString& TargetKey, const FString& Prefix)
{
	const FString Marker = Prefix + TEXT(":");
	const int32 MarkerPos = TargetKey.Find(Marker, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (MarkerPos != INDEX_NONE)
	{
		return TargetKey.Mid(MarkerPos + Marker.Len());
	}

	int32 LastColon = INDEX_NONE;
	if (TargetKey.FindLastChar(TEXT(':'), LastColon))
	{
		return TargetKey.Mid(LastColon + 1);
	}
	return TargetKey;
}

bool UBlueprintHelperReviewUtils::IsReviewSnapshotIgnoredGraphNode(const UEdGraphNode* Node)
{
	if (!Node || !Node->GetClass())
	{
		return false;
	}

	const FString ClassName = Node->GetClass()->GetName();
	return ClassName.Contains(TEXT("Comment"))
		|| ClassName.Contains(TEXT("K2Node_Knot"))
		|| ClassName.Contains(TEXT("Knot"));
}

FString UBlueprintHelperReviewUtils::GetReviewSnapshotNodeMetadataValue(const UEdGraphNode* Node, const TCHAR* Key)
{
	if (!Node || !Key)
	{
		return FString();
	}

	UPackage* Package = Node->GetOutermost();
	if (!Package)
	{
		return FString();
	}

	FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
	return MetaData.GetValue(Node, Key);
}

FString UBlueprintHelperReviewUtils::GetReviewSnapshotNodeBlockId(const UEdGraphNode* Node)
{
	return UBlueprintHelperReviewUtils::GetReviewSnapshotNodeMetadataValue(Node, TEXT("BlueprintHelperBlockId"));
}

FString UBlueprintHelperReviewUtils::MakeReviewSnapshotNodeSortKey(const UEdGraphNode* Node)
{
	if (!Node)
	{
		return FString();
	}
	return FString::Printf(
		TEXT("%s|%s"),
		*Node->NodeGuid.ToString(EGuidFormats::Digits),
		*Node->GetName());
}

FString UBlueprintHelperReviewUtils::BuildReviewSnapshotRestoreText(const TArray<const UEdGraphNode*>& Nodes)
{
	TSet<UObject*> NodesToExport;
	for (const UEdGraphNode* Node : Nodes)
	{
		if (Node)
		{
			NodesToExport.Add(const_cast<UEdGraphNode*>(Node));
		}
	}
	if (NodesToExport.Num() == 0)
	{
		return FString();
	}

	FString ExportedText;
	FEdGraphUtilities::ExportNodesToText(NodesToExport, ExportedText);
	return ExportedText;
}

void UBlueprintHelperReviewUtils::AppendReviewSnapshotGraphs(TArray<const UEdGraph*>& OutGraphs, const TArray<UEdGraph*>& InGraphs)
{
	for (const UEdGraph* Graph : InGraphs)
	{
		if (Graph)
		{
			OutGraphs.Add(Graph);
		}
	}
}

const UEdGraph* UBlueprintHelperReviewUtils::FindReviewSnapshotGraph(const UBlueprint* Blueprint, const FString& GraphName)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	TArray<const UEdGraph*> Graphs;
	UBlueprintHelperReviewUtils::AppendReviewSnapshotGraphs(Graphs, Blueprint->UbergraphPages);
	UBlueprintHelperReviewUtils::AppendReviewSnapshotGraphs(Graphs, Blueprint->FunctionGraphs);
	UBlueprintHelperReviewUtils::AppendReviewSnapshotGraphs(Graphs, Blueprint->MacroGraphs);
	UBlueprintHelperReviewUtils::AppendReviewSnapshotGraphs(Graphs, Blueprint->DelegateSignatureGraphs);

	for (const UEdGraph* Graph : Graphs)
	{
		if (Graph && (GraphName.IsEmpty() || Graph->GetName() == GraphName))
		{
			return Graph;
		}
	}
	return nullptr;
}

const UEdGraphNode* UBlueprintHelperReviewUtils::FindReviewSnapshotNodeByGuid(const UEdGraph* Graph, const FString& NodeGuid)
{
	if (!Graph || NodeGuid.IsEmpty())
	{
		return nullptr;
	}

	FGuid ParsedGuid;
	const bool bParsedGuid = FGuid::Parse(NodeGuid, ParsedGuid);
	const FString NormalizedNodeGuid = NodeGuid.Replace(TEXT("-"), TEXT(""));
	if (!bParsedGuid && NormalizedNodeGuid.IsEmpty())
	{
		return nullptr;
	}

	for (const UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}
		if (bParsedGuid && Node->NodeGuid == ParsedGuid)
		{
			return Node;
		}
		if (Node->NodeGuid.ToString(EGuidFormats::Digits) == NormalizedNodeGuid)
		{
			return Node;
		}
	}
	return nullptr;
}

const UEdGraphNode* UBlueprintHelperReviewUtils::FindReviewSnapshotNodeByName(const UEdGraph* Graph, const FString& NodeName)
{
	if (!Graph || NodeName.IsEmpty())
	{
		return nullptr;
	}

	for (const UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->GetName() == NodeName)
		{
			return Node;
		}
	}
	return nullptr;
}

TSharedPtr<FJsonObject> UBlueprintHelperReviewUtils::FindBaselineGraphObject(
	const TSharedPtr<FJsonObject>& BlueprintSnapshot,
	const FString& GraphName)
{
	if (!BlueprintSnapshot.IsValid())
	{
		return nullptr;
	}

	const TArray<TSharedPtr<FJsonValue>>* Graphs = nullptr;
	if (!BlueprintSnapshot->TryGetArrayField(TEXT("graphs"), Graphs) || !Graphs)
	{
		return nullptr;
	}

	for (const TSharedPtr<FJsonValue>& GraphValue : *Graphs)
	{
		const TSharedPtr<FJsonObject> GraphObject = GraphValue.IsValid() ? GraphValue->AsObject() : nullptr;
		FString CandidateName;
		if (GraphObject.IsValid()
			&& GraphObject->TryGetStringField(TEXT("name"), CandidateName)
			&& (GraphName.IsEmpty() || CandidateName == GraphName))
		{
			return GraphObject;
		}
	}
	return nullptr;
}

TSharedPtr<FJsonObject> UBlueprintHelperReviewUtils::FindBaselineNodeObject(
	const TSharedPtr<FJsonObject>& GraphObject,
	const FString& NodeGuid,
	const FString& NodeName)
{
	if (!GraphObject.IsValid())
	{
		return nullptr;
	}

	const FString NormalizedNodeGuid = NodeGuid.Replace(TEXT("-"), TEXT(""));
	const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
	if (!GraphObject->TryGetArrayField(TEXT("nodes"), Nodes) || !Nodes)
	{
		return nullptr;
	}

	for (const TSharedPtr<FJsonValue>& NodeValue : *Nodes)
	{
		const TSharedPtr<FJsonObject> NodeObject = NodeValue.IsValid() ? NodeValue->AsObject() : nullptr;
		if (!NodeObject.IsValid())
		{
			continue;
		}

		FString CandidateGuid;
		FString CandidateName;
		NodeObject->TryGetStringField(TEXT("guid"), CandidateGuid);
		NodeObject->TryGetStringField(TEXT("name"), CandidateName);
		if ((!NormalizedNodeGuid.IsEmpty() && CandidateGuid == NormalizedNodeGuid)
			|| (!NodeName.IsEmpty() && CandidateName == NodeName))
		{
			return NodeObject;
		}
	}
	return nullptr;
}

bool UBlueprintHelperReviewUtils::BaselineJsonObjectStringFieldEquals(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& Expected)
{
	FString Value;
	return Object.IsValid()
		&& Object->TryGetStringField(FieldName, Value)
		&& Value == Expected;
}

TArray<TSharedPtr<FJsonValue>> UBlueprintHelperReviewUtils::CopyBaselineJsonArray(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (Object.IsValid() && Object->TryGetArrayField(FieldName, Values) && Values)
	{
		return *Values;
	}
	return {};
}

// ====================================================================
// StoreMergeUtils
// ====================================================================

FString UBlueprintHelperReviewUtils::BlueprintHelperReviewNormalizeCollapseSegment(FString Value)
{
	Value.TrimStartAndEndInline();
	Value.ToLowerInline();
	return Value;
}

bool UBlueprintHelperReviewUtils::BlueprintHelperReviewIsActiveVisibleChangeState(const FBlueprintHelperReviewVisibleChange& Change)
{
	return Change.Status == EBlueprintHelperReviewChangeStatus::Pending
		|| Change.Status == EBlueprintHelperReviewChangeStatus::NeedsAction
		|| Change.Status == EBlueprintHelperReviewChangeStatus::RejectFailed;
}

void UBlueprintHelperReviewUtils::BlueprintHelperReviewIndexVisibleChangeCollapseKeys(
	const FBlueprintHelperReviewVisibleChange& Change,
	int32 Index,
	TMap<FString, int32>& ExistingIndexByChangeId,
	TMap<FString, int32>& ExistingIndexByLifecycleRoot)
{
	const FString ChangeIdKey = FBlueprintHelperReviewStoreMergeUtils::MakeLoadedVisibleChangeChangeIdCollapseKey(Change);
	if (!ChangeIdKey.IsEmpty())
	{
		ExistingIndexByChangeId.Add(ChangeIdKey, Index);
	}

	const FString LifecycleRootKey = FBlueprintHelperReviewStoreMergeUtils::MakeLoadedVisibleChangeLifecycleRootCollapseKey(Change);
	if (!LifecycleRootKey.IsEmpty())
	{
		ExistingIndexByLifecycleRoot.Add(LifecycleRootKey, Index);
	}

}

bool UBlueprintHelperReviewUtils::BlueprintHelperReviewFindCollapseReason(
	const FBlueprintHelperReviewVisibleChange& Existing,
	const FBlueprintHelperReviewVisibleChange& Incoming,
	FString& OutReason)
{
	const FString ExistingChangeIdKey =
		FBlueprintHelperReviewStoreMergeUtils::MakeLoadedVisibleChangeChangeIdCollapseKey(Existing);
	const FString IncomingChangeIdKey =
		FBlueprintHelperReviewStoreMergeUtils::MakeLoadedVisibleChangeChangeIdCollapseKey(Incoming);
	if (!ExistingChangeIdKey.IsEmpty() && ExistingChangeIdKey == IncomingChangeIdKey)
	{
		OutReason = TEXT("change_id");
		return true;
	}

	const FString ExistingLifecycleRootKey =
		FBlueprintHelperReviewStoreMergeUtils::MakeLoadedVisibleChangeLifecycleRootCollapseKey(Existing);
	const FString IncomingLifecycleRootKey =
		FBlueprintHelperReviewStoreMergeUtils::MakeLoadedVisibleChangeLifecycleRootCollapseKey(Incoming);
	if (!ExistingLifecycleRootKey.IsEmpty() && ExistingLifecycleRootKey == IncomingLifecycleRootKey)
	{
		OutReason = TEXT("active_lifecycle_root");
		return true;
	}

	OutReason.Reset();
	return false;
}

void UBlueprintHelperReviewUtils::BlueprintHelperReviewLogFoldedVisibleChange(
	const TCHAR* Context,
	const FString& Reason,
	const FBlueprintHelperReviewVisibleChange& Existing,
	const FBlueprintHelperReviewVisibleChange& Incoming)
{
	UE_LOG(
		LogBlueprintHelperReviewUtils,
		Verbose,
		TEXT("Folded duplicated visible change context=%s reason=%s existing_change_id=%s incoming_change_id=%s asset=%s existing_kind=%s incoming_kind=%s"),
		Context ? Context : TEXT("unknown"),
		*Reason,
		*Existing.ChangeId,
		*Incoming.ChangeId,
		*(Incoming.AssetPath.IsEmpty() ? Existing.AssetPath : Incoming.AssetPath),
		BlueprintHelperReviewChangeKindToString(Existing.ChangeKind),
		BlueprintHelperReviewChangeKindToString(Incoming.ChangeKind));
}

// ====================================================================
// RejectService
// ====================================================================

void UBlueprintHelperReviewUtils::CaptureReviewRejectCurrentStateDiagnostic(
	const FBlueprintHelperReviewAtomicTarget& Target,
	FBlueprintHelperReviewActionResult& InOutDiagnostic)
{
	if (!InOutDiagnostic.HashGuardTargetKey.IsEmpty())
	{
		return;
	}

	if (Target.RecordedAfterHash.IsEmpty())
	{
		InOutDiagnostic.HashGuardTargetKey = Target.TargetKey;
		InOutDiagnostic.HashGuardExpectedHash = TEXT("<missing_recorded_after_hash>");
		return;
	}

	FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
	FString CurrentSnapshotJson;
	FString CurrentHash;
	FString SnapshotError;
	if (!SnapshotService.CaptureTargetSnapshot(Target, CurrentSnapshotJson, CurrentHash, SnapshotError))
	{
		InOutDiagnostic.HashGuardTargetKey = Target.TargetKey;
		InOutDiagnostic.HashGuardExpectedHash = Target.RecordedAfterHash;
		InOutDiagnostic.HashGuardCurrentHash = FString::Printf(TEXT("<current_hash_unavailable:%s>"), *SnapshotError);
		InOutDiagnostic.HashGuardRecordedAfterSnapshotJson = Target.AfterSnapshotJson;
		return;
	}

	if (!CurrentHash.Equals(Target.RecordedAfterHash, ESearchCase::CaseSensitive))
	{
		InOutDiagnostic.HashGuardTargetKey = Target.TargetKey;
		InOutDiagnostic.HashGuardExpectedHash = Target.RecordedAfterHash;
		InOutDiagnostic.HashGuardCurrentHash = CurrentHash;
		InOutDiagnostic.HashGuardCurrentSnapshotJson = CurrentSnapshotJson;
		InOutDiagnostic.HashGuardRecordedAfterSnapshotJson = Target.AfterSnapshotJson;
	}
}

// ====================================================================
// BaselineSnapshotServiceUtils
// ====================================================================

bool UBlueprintHelperReviewUtils::ShouldOmitCanonicalReviewSnapshotField(const FString& Key)
{
	return Key.Equals(TEXT("captured_at"), ESearchCase::CaseSensitive)
		|| Key.Equals(TEXT("warnings"), ESearchCase::CaseSensitive)
		|| Key.Equals(TEXT("debug"), ESearchCase::CaseSensitive)
		|| Key.Equals(TEXT("debug_only"), ESearchCase::CaseSensitive);
}

void UBlueprintHelperReviewUtils::AppendCanonicalJsonString(const FString& Value, FString& Out)
{
	Out.AppendChar(TEXT('"'));
	for (const TCHAR Ch : Value)
	{
		switch (Ch)
		{
		case TEXT('"'):
			Out += TEXT("\\\"");
			break;
		case TEXT('\\'):
			Out += TEXT("\\\\");
			break;
		case TEXT('\b'):
			Out += TEXT("\\b");
			break;
		case TEXT('\f'):
			Out += TEXT("\\f");
			break;
		case TEXT('\n'):
			Out += TEXT("\\n");
			break;
		case TEXT('\r'):
			Out += TEXT("\\r");
			break;
		case TEXT('\t'):
			Out += TEXT("\\t");
			break;
		default:
			if (Ch < 0x20)
			{
				Out += FString::Printf(TEXT("\\u%04x"), static_cast<uint32>(Ch));
			}
			else
			{
				Out.AppendChar(Ch);
			}
			break;
		}
	}
	Out.AppendChar(TEXT('"'));
}

void UBlueprintHelperReviewUtils::AppendCanonicalJsonObject(const TSharedPtr<FJsonObject>& Object, FString& Out)
{
	if (!Object.IsValid())
	{
		Out += TEXT("null");
		return;
	}

	TArray<FString> Keys;
	Object->Values.GetKeys(Keys);
	Keys.RemoveAll([](const FString& Key)
	{
		return UBlueprintHelperReviewUtils::ShouldOmitCanonicalReviewSnapshotField(Key);
	});
	Keys.Sort();

	Out.AppendChar(TEXT('{'));
	bool bFirst = true;
	for (const FString& Key : Keys)
	{
		const TSharedPtr<FJsonValue>* FieldValue = Object->Values.Find(Key);
		if (!FieldValue)
		{
			continue;
		}

		if (!bFirst)
		{
			Out.AppendChar(TEXT(','));
		}
		bFirst = false;
		UBlueprintHelperReviewUtils::AppendCanonicalJsonString(Key, Out);
		Out.AppendChar(TEXT(':'));
		UBlueprintHelperReviewUtils::AppendCanonicalJsonValue(*FieldValue, Out);
	}
	Out.AppendChar(TEXT('}'));
}

void UBlueprintHelperReviewUtils::AppendCanonicalJsonValue(const TSharedPtr<FJsonValue>& Value, FString& Out)
{
	if (!Value.IsValid() || Value->Type == EJson::Null)
	{
		Out += TEXT("null");
		return;
	}

	switch (Value->Type)
	{
	case EJson::String:
		UBlueprintHelperReviewUtils::AppendCanonicalJsonString(Value->AsString(), Out);
		break;
	case EJson::Number:
		Out += LexToString(Value->AsNumber());
		break;
	case EJson::Boolean:
		Out += Value->AsBool() ? TEXT("true") : TEXT("false");
		break;
	case EJson::Array:
		Out.AppendChar(TEXT('['));
		{
			bool bFirst = true;
			for (const TSharedPtr<FJsonValue>& ArrayValue : Value->AsArray())
			{
				if (!bFirst)
				{
					Out.AppendChar(TEXT(','));
				}
				bFirst = false;
				UBlueprintHelperReviewUtils::AppendCanonicalJsonValue(ArrayValue, Out);
			}
		}
		Out.AppendChar(TEXT(']'));
		break;
	case EJson::Object:
		UBlueprintHelperReviewUtils::AppendCanonicalJsonObject(Value->AsObject(), Out);
		break;
	default:
		Out += TEXT("null");
		break;
	}
}
