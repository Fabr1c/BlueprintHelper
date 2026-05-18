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

FBlueprintHelperReviewRejectOptions FBlueprintHelperReviewPanelLocalUtils::PrepareRejectOptions(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	return FBlueprintHelperReviewRejectOptions();
}
