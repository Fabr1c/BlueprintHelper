// BlueprintHelper Runtime-owned sequential Review session service.

#pragma once

#include "CoreMinimal.h"

enum class EBlueprintHelperSequentialReviewSessionStatus : uint8
{
	Active,
	NeedsRecovery,
	Accepted,
	Rejected
};

struct BLUEPRINTHELPER_API FBlueprintHelperSequentialReviewSession
{
	FString Schema = TEXT("BlueprintHelper.SequentialReviewSession.v1");
	FString SequentialReviewSessionId;
	TArray<FString> TargetAssets;
	TArray<FString> ArchiveSessionIds;
	TArray<FString> ReviewRecordIds;
	TArray<FString> SourceTaskRunIds;
	FString SessionStartArchiveSessionId;
	FString LastGoodArchiveSessionId;
	FString LastGoodTaskRunId;
	FString LastFailureTaskRunId;
	EBlueprintHelperSequentialReviewSessionStatus Status =
		EBlueprintHelperSequentialReviewSessionStatus::Active;
	bool bHasLastGoodSnapshot = false;
	bool bHasUnresolvedFailedExecute = false;
	bool bHasExternalConflict = false;
	FString CreatedAt;
	FString UpdatedAt;
};

struct BLUEPRINTHELPER_API FBlueprintHelperSequentialReviewSessionLookup
{
	bool bFound = false;
	FBlueprintHelperSequentialReviewSession Session;
};

struct BLUEPRINTHELPER_API FBlueprintHelperSequentialReviewSessionExecuteUpdate
{
	FString SequentialReviewSessionId;
	FString TaskRunId;
	FString ArchiveSessionId;
	TArray<FString> TargetAssets;
	TArray<FString> ReviewRecordIds;
	bool bContinuation = false;
	bool bSucceeded = false;
	bool bFailed = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperSequentialReviewSessionService
{
public:
	static FString MakeSequentialReviewSessionId();

	FBlueprintHelperSequentialReviewSessionLookup FindOpenSessionForTargetAssets(
		const TArray<FString>& TargetAssets) const;

	bool RecordExecuteUpdate(
		const FBlueprintHelperSequentialReviewSessionExecuteUpdate& Update,
		FBlueprintHelperSequentialReviewSession& OutSession,
		FString& OutError) const;

	bool CloseSessionsForReviewRecord(
		const FString& ReviewRecordId,
		EBlueprintHelperSequentialReviewSessionStatus FinalStatus,
		FString& OutError) const;

private:
	TArray<FBlueprintHelperSequentialReviewSession> QuerySessions() const;
	bool LoadSession(
		const FString& SequentialReviewSessionId,
		FBlueprintHelperSequentialReviewSession& OutSession,
		FString& OutError) const;
	bool SaveSession(
		const FBlueprintHelperSequentialReviewSession& Session,
		FString& OutError) const;
	static FString GetSessionsDir();
	static FString MakeSessionPath(const FString& SequentialReviewSessionId);
	static bool HasSameTargetSet(
		const TArray<FString>& Left,
		const TArray<FString>& Right);
};

BLUEPRINTHELPER_API const TCHAR* BlueprintHelperSequentialReviewSessionStatusToString(
	EBlueprintHelperSequentialReviewSessionStatus Status);
BLUEPRINTHELPER_API EBlueprintHelperSequentialReviewSessionStatus BlueprintHelperSequentialReviewSessionStatusFromString(
	const FString& Value);
