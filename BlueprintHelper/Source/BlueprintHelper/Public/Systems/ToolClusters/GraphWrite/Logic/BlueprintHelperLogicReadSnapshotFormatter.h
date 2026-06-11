// Pure formatter for logic read snapshots.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicGroupBuilder.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotTypes.h"

using FBlueprintHelperLogicSnapshotFormatHandler =
	TFunction<bool(const FBlueprintHelperLogicReadSnapshot&, TSharedPtr<FJsonObject>&, FString&)>;

class BLUEPRINTHELPER_API FBlueprintHelperLogicReadSnapshotFormatter
{
public:
	FBlueprintHelperLogicReadSnapshotFormatter();

	bool BuildFormattedPayload(
		const FString& Format,
		const FBlueprintHelperLogicReadSnapshot& Snapshot,
		TSharedPtr<FJsonObject>& OutPayload,
		FString& OutError) const;

	FBlueprintHelperLogicJsonData BuildLogicJsonData(
		const FBlueprintHelperLogicReadSnapshot& Snapshot) const;

private:
	bool BuildLogicJsonPayload(
		const FBlueprintHelperLogicReadSnapshot& Snapshot,
		TSharedPtr<FJsonObject>& OutPayload,
		FString& OutError) const;

	FBlueprintHelperLogicGroupBuilder GroupBuilder;
	TMap<FString, FBlueprintHelperLogicSnapshotFormatHandler> FormatHandlers;
};
