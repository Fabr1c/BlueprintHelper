// BlueprintHelper Review panel local data utilities.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"

class UActorComponent;
class USCS_Node;

class FBlueprintHelperReviewPanelLocalUtils
{
public:
	static void AddDetailsObjectCandidate(TArray<FString>& Candidates, const FString& Text);
	static FString NormalizeDetailsObjectCandidate(const FString& Text);
	static bool DetailsObjectCandidateMatches(const TArray<FString>& Candidates, const FString& ObjectName);
	static bool ChangeLooksLikeComponentDetailsTarget(const FBlueprintHelperReviewVisibleChange& Change);
	static TArray<FString> BuildDetailsObjectCandidates(const FBlueprintHelperReviewVisibleChange& Change);
	static UActorComponent* GetSCSNodeComponentTemplate(USCS_Node* Node);
	static FString MakeAssetTreeKey(const FString& AssetPath);
	static FBlueprintHelperReviewRejectOptions PrepareRejectOptions(
		const FBlueprintHelperReviewVisibleChange& Change);
};
