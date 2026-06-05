// BlueprintHelper source-control service.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UPackage;

struct BLUEPRINTHELPER_API FBlueprintHelperSourceControlFileState
{
	FString Input;
	FString Filename;
	FString PackageName;
	FString Status;
	FString AgentHint;
	FString CheckedOutOther;
	FString DisplayName;
	FString DisplayTooltip;
	bool bValid = false;
	bool bUnknown = false;
	bool bSourceControlled = false;
	bool bCanCheckOut = false;
	bool bCheckedOut = false;
	bool bCurrent = false;
	bool bAdded = false;
	bool bDeleted = false;
	bool bIgnored = false;
	bool bCanEdit = false;
	bool bModified = false;
	bool bCanAdd = false;
	bool bConflicted = false;
	bool bCanRevert = false;
	bool bCheckedOutOther = false;
	bool bCheckedOutInOtherBranch = false;
	bool bModifiedInOtherBranch = false;

	TSharedRef<FJsonObject> ToJson() const;
};

struct BLUEPRINTHELPER_API FBlueprintHelperSourceControlResult
{
	bool bSuccess = false;
	bool bModified = false;
	bool bEnabled = false;
	bool bAvailable = false;
	FString Provider;
	FString Status;
	FString ErrorCode;
	FString ErrorMessage;
	FString AgentMessage;
	FString RecommendedAction;
	TArray<FBlueprintHelperSourceControlFileState> Files;

	bool HasFileStatus(const FString& InStatus) const;
	bool BlocksAgentEdit() const;
	TSharedRef<FJsonObject> ToJson() const;
};

class BLUEPRINTHELPER_API FBlueprintHelperSourceControlService
{
public:
	FBlueprintHelperSourceControlResult QueryStatus(
		TConstArrayView<FString> Inputs,
		bool bUpdateStatus = true) const;

	FBlueprintHelperSourceControlResult Checkout(
		TConstArrayView<FString> Inputs,
		bool bUpdateStatus = true) const;

	FBlueprintHelperSourceControlResult QueryDirtyPackages(
		TConstArrayView<UPackage*> Packages,
		bool bUpdateStatus = true) const;

	static TArray<FString> InputsFromDirtyPackages(TConstArrayView<UPackage*> Packages);
	static void ClassifyFileStateForAgent(FBlueprintHelperSourceControlFileState& State);
};
