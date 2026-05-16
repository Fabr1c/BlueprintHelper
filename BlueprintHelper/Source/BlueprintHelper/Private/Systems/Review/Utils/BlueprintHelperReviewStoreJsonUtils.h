// BlueprintHelper Review FBlueprintHelperReviewStoreJsonUtils declarations.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h"

class FBlueprintHelperReviewStoreJsonUtils
{
public:
	static EBlueprintHelperReviewChangeStatus ParseReviewChangeStatus(const FString& Status);
	static EBlueprintHelperReviewChangeKind ParseReviewChangeKind(const FString& ChangeKind);
	static EBlueprintHelperReviewSurface ParseReviewSurface(const FString& Surface);
	static void ReadReviewStringArray(
				const TSharedPtr<FJsonObject>& Json,
				const TCHAR* FieldName,
				TArray<FString>& OutValues);
	static TArray<TSharedPtr<FJsonValue>> MakeReviewJsonStringArray(const TArray<FString>& Values);
	static TSharedRef<FJsonObject> MakeReviewJsonVector2D(const FVector2D& Value);
	static bool ReadReviewJsonVector2D(
				const TSharedPtr<FJsonObject>& Json,
				const TCHAR* FieldName,
				FVector2D& OutValue);
	static TSharedRef<FJsonObject> ReviewAtomicTargetToJson(const FBlueprintHelperReviewAtomicTarget& Target);
	static TSharedRef<FJsonObject> ReviewVisibleChangeToJson(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<FJsonObject> ReviewActionRecordToJson(const FBlueprintHelperReviewActionRecord& Action);
	static TSharedRef<FJsonObject> ReviewRecordToJson(const FBlueprintHelperReviewRecord& Record);
	static TSharedRef<FJsonObject> ReviewArchiveSessionToJson(const FBlueprintHelperReviewArchiveSession& ArchiveSession);
	static bool ReadReviewArchiveSessionFromJson(
				const TSharedPtr<FJsonObject>& Json,
				FBlueprintHelperReviewArchiveSession& OutArchiveSession);
	static bool ReadReviewRecordFromJson(const TSharedPtr<FJsonObject>& Json, FBlueprintHelperReviewRecord& OutRecord);
};
