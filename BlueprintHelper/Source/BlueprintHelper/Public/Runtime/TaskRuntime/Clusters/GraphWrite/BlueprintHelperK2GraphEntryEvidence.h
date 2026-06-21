#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperK2GraphEntryIdentityResolver.h"

struct BLUEPRINTHELPER_API FBlueprintHelperK2GraphEntryEvidence
{
	FString AssetPath;
	FString GraphName;
	FString OperationKind;
	FBlueprintHelperK2GraphEntryIdentity EntryIdentity;
	FString BodyEntryAnchorJson;
	FString BodyFingerprint;
	FString BeforeBodySnapshotJson;
	FString AfterBodySnapshotJson;
	FString GraphBodyBoundaryJson;
	FString TargetOwnership = TEXT("agent_authored");
};

class BLUEPRINTHELPER_API FBlueprintHelperK2GraphEntryEvidenceProjector
{
public:
	static bool ProjectToAtomicTarget(
		const FBlueprintHelperK2GraphEntryEvidence& Evidence,
		int32 StepIndex,
		int32 AtomicIndex,
		FBlueprintHelperReviewAtomicTarget& OutTarget,
		FString& OutError);
};
