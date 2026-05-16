// BlueprintHelper Review panel local data utilities implementation.

#include "UI/Review/Utils/BlueprintHelperReviewPanelLocalUtils.h"

#include "Components/ActorComponent.h"
#include "Engine/SCS_Node.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"

void FBlueprintHelperReviewPanelLocalUtils::AddDetailsObjectCandidate(
	TArray<FString>& Candidates,
	const FString& Text)
{
	FString Trimmed = Text;
	Trimmed.TrimStartAndEndInline();
	if (Trimmed.IsEmpty())
	{
		return;
	}

	Candidates.AddUnique(Trimmed);

	int32 LastDot = INDEX_NONE;
	if (Trimmed.FindLastChar(TEXT('.'), LastDot) && LastDot + 1 < Trimmed.Len())
	{
		Candidates.AddUnique(Trimmed.Mid(LastDot + 1));
	}

	int32 LastColon = INDEX_NONE;
	if (Trimmed.FindLastChar(TEXT(':'), LastColon) && LastColon + 1 < Trimmed.Len())
	{
		Candidates.AddUnique(Trimmed.Mid(LastColon + 1));
	}

	int32 LastSlash = INDEX_NONE;
	if (Trimmed.FindLastChar(TEXT('/'), LastSlash) && LastSlash + 1 < Trimmed.Len())
	{
		Candidates.AddUnique(Trimmed.Mid(LastSlash + 1));
	}

	int32 OpenBracket = INDEX_NONE;
	int32 CloseBracket = INDEX_NONE;
	if (Trimmed.FindChar(TEXT('['), OpenBracket)
		&& Trimmed.FindChar(TEXT(']'), CloseBracket)
		&& CloseBracket > OpenBracket + 1)
	{
		Candidates.AddUnique(Trimmed.Mid(OpenBracket + 1, CloseBracket - OpenBracket - 1));
	}
}

FString FBlueprintHelperReviewPanelLocalUtils::NormalizeDetailsObjectCandidate(
	const FString& Text)
{
	FString Normalized;
	for (int32 Index = 0; Index < Text.Len(); ++Index)
	{
		const TCHAR Character = FChar::ToLower(Text[Index]);
		if (FChar::IsAlnum(Character))
		{
			Normalized.AppendChar(Character);
		}
	}
	return Normalized;
}

bool FBlueprintHelperReviewPanelLocalUtils::DetailsObjectCandidateMatches(
	const TArray<FString>& Candidates,
	const FString& ObjectName)
{
	const FString NormalizedObjectName = NormalizeDetailsObjectCandidate(ObjectName);
	if (NormalizedObjectName.IsEmpty())
	{
		return false;
	}

	for (const FString& Candidate : Candidates)
	{
		const FString NormalizedCandidate = NormalizeDetailsObjectCandidate(Candidate);
		if (NormalizedCandidate.IsEmpty())
		{
			continue;
		}

		if (NormalizedCandidate == NormalizedObjectName
			|| NormalizedCandidate.Contains(NormalizedObjectName)
			|| NormalizedObjectName.Contains(NormalizedCandidate))
		{
			return true;
		}
	}

	return false;
}

bool FBlueprintHelperReviewPanelLocalUtils::ChangeLooksLikeComponentDetailsTarget(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	const FString ChangeText = (Change.LocationKey + TEXT(" ") + Change.DisplayLabel).ToLower();
	if (ChangeText.Contains(TEXT("component")) || ChangeText.Contains(TEXT("\u7ec4\u4ef6")))
	{
		return true;
	}

	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		const EBlueprintHelperReviewSurface TargetSurface = BlueprintHelperReviewNormalizeSurfaceForTarget(
			Target.Surface,
			Target.TargetKind,
			Target.TargetKey,
			Target.VisualGroupKey,
			Change.LocationKey);
		const FString TargetText =
			(Target.TargetKind + TEXT(" ") + Target.TargetKey + TEXT(" ") + Target.DisplayLabel
				+ TEXT(" ") + Target.ComponentPath).ToLower();

		if (TargetSurface == EBlueprintHelperReviewSurface::Components
			|| TargetText.Contains(TEXT("component"))
			|| TargetText.Contains(TEXT("\u7ec4\u4ef6"))
			|| !Target.ComponentPath.IsEmpty())
		{
			return true;
		}
	}

	return false;
}

TArray<FString> FBlueprintHelperReviewPanelLocalUtils::BuildDetailsObjectCandidates(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	TArray<FString> Candidates;
	AddDetailsObjectCandidate(Candidates, Change.LocationKey);
	AddDetailsObjectCandidate(Candidates, Change.DisplayLabel);
	AddDetailsObjectCandidate(Candidates, Change.GraphName);

	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		AddDetailsObjectCandidate(Candidates, Target.TargetKey);
		AddDetailsObjectCandidate(Candidates, Target.DisplayLabel);
		AddDetailsObjectCandidate(Candidates, Target.VisualGroupKey);
		AddDetailsObjectCandidate(Candidates, Target.PropertyPath);
		AddDetailsObjectCandidate(Candidates, Target.ComponentPath);
	}

	return Candidates;
}

UActorComponent* FBlueprintHelperReviewPanelLocalUtils::GetSCSNodeComponentTemplate(
	USCS_Node* Node)
{
	if (!Node)
	{
		return nullptr;
	}

	if (FObjectProperty* ComponentTemplateProperty =
		FindFProperty<FObjectProperty>(USCS_Node::StaticClass(), TEXT("ComponentTemplate")))
	{
		return Cast<UActorComponent>(
			ComponentTemplateProperty->GetObjectPropertyValue_InContainer(Node));
	}

	return nullptr;
}

FString FBlueprintHelperReviewPanelLocalUtils::MakeAssetTreeKey(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return AssetPath;
	}

	FString PackageName;
	if (FPackageName::TryConvertFilenameToLongPackageName(AssetPath, PackageName))
	{
		return PackageName;
	}

	if (AssetPath.StartsWith(TEXT("/")))
	{
		FString Normalized = AssetPath;
		const int32 ObjectPathIndex = Normalized.Find(TEXT("."));
		if (ObjectPathIndex != INDEX_NONE)
		{
			Normalized.LeftInline(ObjectPathIndex);
		}
		return Normalized;
	}

	return AssetPath;
}

bool FBlueprintHelperReviewPanelLocalUtils::ExtractRollbackTransactionId(
	const FString& RollbackDataRef,
	FString& OutTransactionId)
{
	const FString Prefix = TEXT("transaction://");
	const FString Suffix = TEXT("/rollback_data");
	if (!RollbackDataRef.StartsWith(Prefix) || !RollbackDataRef.EndsWith(Suffix))
	{
		return false;
	}

	OutTransactionId = RollbackDataRef.Mid(Prefix.Len());
	OutTransactionId.LeftChopInline(Suffix.Len());
	return !OutTransactionId.IsEmpty();
}

FBlueprintHelperReviewPreparedRollbackJournal FBlueprintHelperReviewPanelLocalUtils::PrepareRollbackJournal(
	const FString& TransactionId)
{
	FBlueprintHelperReviewPreparedRollbackJournal Prepared;
	Prepared.TransactionId = TransactionId;

	const FString ActivePath = FPaths::ProjectSavedDir()
		/ TEXT("BlueprintHelper")
		/ TEXT("Transactions")
		/ TEXT("Active")
		/ FString::Printf(TEXT("%s.json"), *TransactionId);
	const FString ReviewPath = FPaths::ProjectSavedDir()
		/ TEXT("BlueprintHelper")
		/ TEXT("Review")
		/ FString::Printf(TEXT("%s.json"), *TransactionId);

	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *ActivePath)
		&& !FFileHelper::LoadFileToString(Content, *ReviewPath))
	{
		Prepared.Error = FString::Printf(TEXT("rollback_ref_not_found:%s"), *TransactionId);
		return Prepared;
	}

	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
	{
		Prepared.Error = FString::Printf(TEXT("rollback_ref_parse_failed:%s"), *TransactionId);
		return Prepared;
	}

	if (!Json->TryGetStringField(TEXT("tool"), Prepared.Tool) || Prepared.Tool.IsEmpty())
	{
		Prepared.Error = FString::Printf(TEXT("rollback_journal_tool_missing:%s"), *TransactionId);
		return Prepared;
	}

	TSharedPtr<FJsonObject> RollbackData;
	const TSharedPtr<FJsonObject>* RollbackDataObject = nullptr;
	if (Json->TryGetObjectField(TEXT("rollback_data"), RollbackDataObject)
		&& RollbackDataObject
		&& RollbackDataObject->IsValid())
	{
		RollbackData = *RollbackDataObject;
	}
	else
	{
		FString RollbackDataString;
		if (Json->TryGetStringField(TEXT("rollback_data"), RollbackDataString)
			&& !RollbackDataString.IsEmpty())
		{
			TSharedRef<TJsonReader<>> RollbackReader =
				TJsonReaderFactory<>::Create(RollbackDataString);
			FJsonSerializer::Deserialize(RollbackReader, RollbackData);
		}
	}

	if (RollbackData.IsValid())
	{
		Prepared.bHasRollbackData = true;
		RollbackData->TryGetStringField(TEXT("exported_text"), Prepared.ExportedText);
		RollbackData->TryGetStringField(TEXT("entry_identity"), Prepared.EntryIdentity);
		RollbackData->TryGetStringField(TEXT("replace_scope"), Prepared.ReplaceScope);
		RollbackData->TryGetStringField(TEXT("owner_block_id"), Prepared.OwnerBlockId);
	}

	return Prepared;
}

FBlueprintHelperReviewRejectOptions FBlueprintHelperReviewPanelLocalUtils::PrepareRejectOptions(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	FBlueprintHelperReviewRejectOptions Options;
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		FString TransactionId;
		if (!Target.RollbackDataRef.IsEmpty()
			&& ExtractRollbackTransactionId(Target.RollbackDataRef, TransactionId)
			&& !Options.PreparedRollbackJournalsByTransactionId.Contains(TransactionId))
		{
			Options.PreparedRollbackJournalsByTransactionId.Add(
				TransactionId,
				PrepareRollbackJournal(TransactionId));
		}
	}
	return Options;
}
