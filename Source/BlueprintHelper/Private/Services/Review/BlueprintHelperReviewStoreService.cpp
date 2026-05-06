// BlueprintHelper Review Store service implementation.

#include "Services/Review/BlueprintHelperReviewStoreService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString ExtractReviewNodeIdentifier(const FString& RawNodePath)
	{
		FString Identifier = RawNodePath;

		int32 DotIndex = INDEX_NONE;
		if (Identifier.FindLastChar(TEXT('.'), DotIndex))
		{
			Identifier = Identifier.Mid(DotIndex + 1);
		}

		return Identifier;
	}

	FBlueprintHelperReviewAtomicTarget MakeGraphRecordTarget(
		const FBlueprintHelperReviewTransactionInput& Input,
		const FString& TargetId,
		const FString& TargetPrefix)
	{
		FBlueprintHelperReviewAtomicTarget Target;
		Target.AssetPath = Input.AssetPath;
		Target.GraphName = Input.GraphName;
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
		Target.TargetKey = FString::Printf(TEXT("%s:%s:%s"), *Input.LocationKey, *TargetPrefix, *TargetId);
		Target.VisualGroupKey = Input.LocationKey;
		Target.DisplayLabel = Input.DisplayLabel;
		Target.NodeGuid = TargetId;
		Target.SourceTransactionIds.Add(Input.TransactionId);
		return Target;
	}

	void AddGraphTargetsFromStringArrayField(
		const TSharedPtr<FJsonObject>& Record,
		const TCHAR* FieldName,
		const FString& TargetPrefix,
		bool bExtractNodeName,
		FBlueprintHelperReviewTransactionInput& Input)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Record.IsValid() || !Record->TryGetArrayField(FieldName, Values) || !Values)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			if (!Value.IsValid())
			{
				continue;
			}

			const FString RawValue = Value->AsString();
			const FString TargetId = bExtractNodeName ? ExtractReviewNodeIdentifier(RawValue) : RawValue;
			if (TargetId.IsEmpty())
			{
				continue;
			}

			Input.AtomicTargets.Add(MakeGraphRecordTarget(Input, TargetId, TargetPrefix));
		}
	}

	void AddGraphTargetsFromRollbackData(FBlueprintHelperReviewTransactionInput& Input, const TSharedPtr<FJsonObject>& Record)
	{
		TSharedPtr<FJsonObject> RollbackObject;
		if (!Record.IsValid())
		{
			return;
		}

		FString RollbackDataString;
		if (Record->TryGetStringField(TEXT("rollback_data"), RollbackDataString) && !RollbackDataString.IsEmpty())
		{
			const TSharedRef<TJsonReader<>> RollbackReader = TJsonReaderFactory<>::Create(RollbackDataString);
			FJsonSerializer::Deserialize(RollbackReader, RollbackObject);
		}
		else
		{
			const TSharedPtr<FJsonObject>* RollbackObjectPtr = nullptr;
			if (Record->TryGetObjectField(TEXT("rollback_data"), RollbackObjectPtr) && RollbackObjectPtr)
			{
				RollbackObject = *RollbackObjectPtr;
			}
		}

		if (!RollbackObject.IsValid())
		{
			return;
		}

		AddGraphTargetsFromStringArrayField(
			RollbackObject,
			TEXT("node_guids"),
			TEXT("rollback_node"),
			false,
			Input);
	}
}

TArray<FBlueprintHelperReviewVisibleChange> FBlueprintHelperReviewStoreService::BuildVisibleChanges(
	const TArray<FBlueprintHelperReviewTransactionInput>& Transactions) const
{
	TMap<FString, FBlueprintHelperReviewVisibleChange> AtomicChanges;
	TArray<FString> AtomicOrder;

	for (const FBlueprintHelperReviewTransactionInput& Input : Transactions)
	{
		if (Input.ChangeKind == EBlueprintHelperReviewChangeKind::Renamed)
		{
			FBlueprintHelperReviewTransactionInput Removed = Input;
			Removed.ChangeKind = EBlueprintHelperReviewChangeKind::Removed;
			Removed.LocationKey = Input.LocationKey + TEXT(":rename_removed");
			Removed.AfterSummary = TEXT("");
			for (FBlueprintHelperReviewAtomicTarget& Target : Removed.AtomicTargets)
			{
				Target.TargetKey += TEXT(":rename_removed");
				Target.VisualGroupKey += TEXT(":rename_removed");
			}
			AddAtomicTargetsForInput(Removed, AtomicChanges, AtomicOrder);

			FBlueprintHelperReviewTransactionInput Added = Input;
			Added.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
			Added.LocationKey = Input.LocationKey + TEXT(":rename_added");
			Added.BeforeSummary = TEXT("");
			for (FBlueprintHelperReviewAtomicTarget& Target : Added.AtomicTargets)
			{
				Target.TargetKey += TEXT(":rename_added");
				Target.VisualGroupKey += TEXT(":rename_added");
			}
			AddAtomicTargetsForInput(Added, AtomicChanges, AtomicOrder);
			continue;
		}

		AddAtomicTargetsForInput(Input, AtomicChanges, AtomicOrder);
	}

	TArray<FBlueprintHelperReviewVisibleChange> Changes;
	TMap<FString, int32> GroupToIndex;
	for (const FString& AtomicKey : AtomicOrder)
	{
		if (const FBlueprintHelperReviewVisibleChange* AtomicChange = AtomicChanges.Find(AtomicKey))
		{
			GroupAtomicVisibleChange(*AtomicChange, GroupToIndex, Changes);
		}
	}

	return Changes;
}

void FBlueprintHelperReviewStoreService::AddAtomicTargetsForInput(
	const FBlueprintHelperReviewTransactionInput& Input,
	TMap<FString, FBlueprintHelperReviewVisibleChange>& AtomicChanges,
	TArray<FString>& AtomicOrder) const
{
	TArray<FBlueprintHelperReviewAtomicTarget> AtomicTargets = MakeAtomicTargetsForInput(Input);
	for (FBlueprintHelperReviewAtomicTarget& Target : AtomicTargets)
	{
		Target.AssetPath = Target.AssetPath.IsEmpty() ? Input.AssetPath : Target.AssetPath;
		Target.GraphName = Target.GraphName.IsEmpty() ? Input.GraphName : Target.GraphName;
		Target.DisplayLabel = Target.DisplayLabel.IsEmpty() ? Input.DisplayLabel : Target.DisplayLabel;
		Target.VisualGroupKey = Target.VisualGroupKey.IsEmpty() ? Target.TargetKey : Target.VisualGroupKey;
		Target.LatestTransactionId = Input.TransactionId;

		const FString AtomicKey = FString::Printf(
			TEXT("%s|%s|%s|%s"),
			*Target.AssetPath,
			BlueprintHelperReviewSurfaceToString(Target.Surface),
			*Target.GraphName,
			*Target.TargetKey);

		FBlueprintHelperReviewVisibleChange* Existing = AtomicChanges.Find(AtomicKey);
		if (!Existing)
		{
			FBlueprintHelperReviewVisibleChange AtomicChange = MakeVisibleChange(Input);
			AtomicChange.AssetPath = Target.AssetPath;
			AtomicChange.GraphName = Target.GraphName;
			AtomicChange.LocationKey = Target.VisualGroupKey;
			AtomicChange.DisplayLabel = Target.DisplayLabel.IsEmpty() ? AtomicChange.DisplayLabel : Target.DisplayLabel;
			AtomicChange.AtomicTargets.Reset();
			AtomicChange.AtomicTargets.Add(Target);
			AtomicChange.LatestTransactionIds.Reset();
			AtomicChange.LatestTransactionIds.Add(Input.TransactionId);
			AtomicChanges.Add(AtomicKey, AtomicChange);
			AtomicOrder.Add(AtomicKey);
			continue;
		}

		FBlueprintHelperReviewVisibleChange& AtomicChange = *Existing;
		FBlueprintHelperReviewAtomicTarget& ExistingTarget = AtomicChange.AtomicTargets[0];
		ExistingTarget.LatestTransactionId = Input.TransactionId;
		ExistingTarget.SourceTransactionIds.Add(Input.TransactionId);
		ExistingTarget.GraphName = Target.GraphName;
		ExistingTarget.VisualGroupKey = Target.VisualGroupKey;
		ExistingTarget.DisplayLabel = Target.DisplayLabel;
		ExistingTarget.NodeGuid = Target.NodeGuid;
		ExistingTarget.PinPath = Target.PinPath;
		ExistingTarget.PropertyPath = Target.PropertyPath;
		ExistingTarget.ComponentPath = Target.ComponentPath;
		ExistingTarget.bHasGraphBounds = Target.bHasGraphBounds;
		ExistingTarget.GraphPosition = Target.GraphPosition;
		ExistingTarget.GraphSize = Target.GraphSize;

		AtomicChange.LatestTransactionId = Input.TransactionId;
		AtomicChange.LatestTransactionIds.Reset();
		AtomicChange.LatestTransactionIds.Add(Input.TransactionId);
		AtomicChange.SourceTransactionIds.Add(Input.TransactionId);
		AtomicChange.ChangeKind = Input.ChangeKind;
		AtomicChange.GraphName = Target.GraphName;
		AtomicChange.LocationKey = Target.VisualGroupKey;
		AtomicChange.DisplayLabel = Target.DisplayLabel.IsEmpty() ? AtomicChange.DisplayLabel : Target.DisplayLabel;
		AtomicChange.AfterSummary = Input.AfterSummary;
		AtomicChange.ChangeId = Input.TransactionId;
	}
}

TArray<FBlueprintHelperReviewVisibleChange> FBlueprintHelperReviewStoreService::LoadPendingVisibleChanges(
	const FString& AssetPathFilter) const
{
	TArray<FBlueprintHelperReviewTransactionInput> Inputs;
	const FString ReviewDir = FPaths::ProjectSavedDir() / TEXT("BlueprintHelper") / TEXT("Review");

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(ReviewDir / TEXT("*.json")), true, false);

	for (const FString& File : Files)
	{
		const FString Path = ReviewDir / File;
		FString Content;
		if (!FFileHelper::LoadFileToString(Content, *Path))
		{
			continue;
		}

		TSharedPtr<FJsonObject> Record;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		if (!FJsonSerializer::Deserialize(Reader, Record) || !Record.IsValid())
		{
			continue;
		}

		FString ReviewStatus;
		Record->TryGetStringField(TEXT("review_status"), ReviewStatus);
		if (ReviewStatus.Equals(TEXT("accepted"), ESearchCase::IgnoreCase)
			|| ReviewStatus.Equals(TEXT("rejected"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>* TargetAssets = nullptr;
		if (!Record->TryGetArrayField(TEXT("target_assets"), TargetAssets) || TargetAssets->Num() == 0)
		{
			continue;
		}

		FString AssetPath;
		(*TargetAssets)[0]->TryGetString(AssetPath);
		if (AssetPath.IsEmpty() || (!AssetPathFilter.IsEmpty() && AssetPath != AssetPathFilter))
		{
			continue;
		}

		FBlueprintHelperReviewTransactionInput Input;
		Record->TryGetStringField(TEXT("transaction_id"), Input.TransactionId);
		Input.AssetPath = AssetPath;
		Record->TryGetStringField(TEXT("graph"), Input.GraphName);

		FString Tool;
		Record->TryGetStringField(TEXT("tool"), Tool);
		Input.DisplayLabel = Tool.IsEmpty() ? Input.TransactionId : Tool;
		Record->TryGetStringField(TEXT("diff_summary"), Input.AfterSummary);
		if (Input.AfterSummary.IsEmpty())
		{
			Input.AfterSummary = Input.DisplayLabel;
		}

		Input.LocationKey = FString::Printf(
			TEXT("%s:%s:%s"),
			*Input.AssetPath,
			*Input.GraphName,
			*Input.DisplayLabel);

		if (Tool.Contains(TEXT("Append"), ESearchCase::IgnoreCase)
			|| Tool.Contains(TEXT("Create"), ESearchCase::IgnoreCase))
		{
			Input.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
		}
		else if (Tool.Contains(TEXT("Cleanup"), ESearchCase::IgnoreCase)
			|| Tool.Contains(TEXT("Remove"), ESearchCase::IgnoreCase))
		{
			Input.ChangeKind = EBlueprintHelperReviewChangeKind::Removed;
		}
		else
		{
			Input.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
		}

		AddGraphTargetsFromStringArrayField(
			Record,
			TEXT("created_nodes"),
			TEXT("created_node"),
			true,
			Input);
		AddGraphTargetsFromStringArrayField(
			Record,
			TEXT("blocks"),
			TEXT("block"),
			false,
			Input);
		AddGraphTargetsFromRollbackData(Input, Record);

		if (!Input.TransactionId.IsEmpty())
		{
			Inputs.Add(Input);
		}
	}

	return BuildVisibleChanges(Inputs);
}

FBlueprintHelperReviewVisibleChange FBlueprintHelperReviewStoreService::MakeVisibleChange(
	const FBlueprintHelperReviewTransactionInput& Input,
	const FString& ChangeIdSuffix) const
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.AssetPath = Input.AssetPath;
	Change.GraphName = Input.GraphName;
	Change.LocationKey = Input.LocationKey;
	Change.LatestTransactionId = Input.TransactionId;
	Change.LatestTransactionIds.Add(Input.TransactionId);
	Change.SourceTransactionIds.Add(Input.TransactionId);
	Change.ChangeKind = Input.ChangeKind;
	Change.DisplayLabel = Input.DisplayLabel.IsEmpty() ? Input.LocationKey : Input.DisplayLabel;
	Change.BeforeSummary = Input.BeforeSummary;
	Change.AfterSummary = Input.AfterSummary;
	Change.ChangeId = Input.TransactionId;
	if (!ChangeIdSuffix.IsEmpty())
	{
		Change.ChangeId += TEXT("_") + ChangeIdSuffix;
	}
	return Change;
}

void FBlueprintHelperReviewStoreService::GroupAtomicVisibleChange(
	const FBlueprintHelperReviewVisibleChange& AtomicChange,
	TMap<FString, int32>& GroupToIndex,
	TArray<FBlueprintHelperReviewVisibleChange>& OutChanges) const
{
	if (AtomicChange.AtomicTargets.Num() == 0)
	{
		return;
	}

	const FBlueprintHelperReviewAtomicTarget& Target = AtomicChange.AtomicTargets[0];
	const FString GroupKey = FString::Printf(
		TEXT("%s|%s"),
		*AtomicChange.AssetPath,
		*Target.VisualGroupKey);

	if (const int32* ExistingIndex = GroupToIndex.Find(GroupKey))
	{
		FBlueprintHelperReviewVisibleChange& Existing = OutChanges[*ExistingIndex];
		Existing.AtomicTargets.Add(Target);
		for (const FString& SourceTransactionId : AtomicChange.SourceTransactionIds)
		{
			Existing.SourceTransactionIds.AddUnique(SourceTransactionId);
		}
		for (const FString& LatestTransactionId : AtomicChange.LatestTransactionIds)
		{
			Existing.LatestTransactionIds.AddUnique(LatestTransactionId);
		}
		Existing.LatestTransactionId = AtomicChange.LatestTransactionId;
		Existing.ChangeKind = AtomicChange.ChangeKind;
		Existing.GraphName = AtomicChange.GraphName.IsEmpty() ? Existing.GraphName : AtomicChange.GraphName;
		Existing.DisplayLabel = AtomicChange.DisplayLabel.IsEmpty() ? Existing.DisplayLabel : AtomicChange.DisplayLabel;
		Existing.AfterSummary = AtomicChange.AfterSummary;
		Existing.ChangeId = AtomicChange.ChangeId;
		return;
	}

	const int32 NewIndex = OutChanges.Add(AtomicChange);
	GroupToIndex.Add(GroupKey, NewIndex);
}

TArray<FBlueprintHelperReviewAtomicTarget> FBlueprintHelperReviewStoreService::MakeAtomicTargetsForInput(
	const FBlueprintHelperReviewTransactionInput& Input) const
{
	if (Input.AtomicTargets.Num() > 0)
	{
		TArray<FBlueprintHelperReviewAtomicTarget> Targets = Input.AtomicTargets;
		for (FBlueprintHelperReviewAtomicTarget& Target : Targets)
		{
			Target.AssetPath = Target.AssetPath.IsEmpty() ? Input.AssetPath : Target.AssetPath;
			Target.GraphName = Target.GraphName.IsEmpty() ? Input.GraphName : Target.GraphName;
			Target.TargetKey = Target.TargetKey.IsEmpty() ? Input.LocationKey : Target.TargetKey;
			Target.VisualGroupKey = Target.VisualGroupKey.IsEmpty() ? Input.LocationKey : Target.VisualGroupKey;
			Target.DisplayLabel = Target.DisplayLabel.IsEmpty() ? Input.DisplayLabel : Target.DisplayLabel;
			Target.SourceTransactionIds.Add(Input.TransactionId);
		}
		return Targets;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Input.AssetPath;
	Target.GraphName = Input.GraphName;
	Target.TargetKey = Input.LocationKey.IsEmpty() ? Input.DisplayLabel : Input.LocationKey;
	Target.VisualGroupKey = Target.TargetKey;
	Target.DisplayLabel = Input.DisplayLabel;
	Target.SourceTransactionIds.Add(Input.TransactionId);

	FBlueprintHelperReviewVisibleChange TempChange;
	TempChange.LocationKey = Input.LocationKey;
	TempChange.GraphName = Input.GraphName;
	TempChange.DisplayLabel = Input.DisplayLabel;
	TempChange.ChangeKind = Input.ChangeKind;
	if (BlueprintHelperReviewShouldShowInComponents(TempChange))
	{
		Target.Surface = EBlueprintHelperReviewSurface::Components;
	}
	else if (BlueprintHelperReviewShouldShowInDetails(TempChange))
	{
		Target.Surface = EBlueprintHelperReviewSurface::Details;
	}
	else if (BlueprintHelperReviewShouldShowInGraph(TempChange))
	{
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
	}
	else if (BlueprintHelperReviewShouldShowInMyBlueprint(TempChange))
	{
		Target.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
	}
	else
	{
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
	}

	TArray<FBlueprintHelperReviewAtomicTarget> Targets;
	Targets.Add(Target);
	return Targets;
}
