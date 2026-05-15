// BlueprintHelper Review baseline semantic snapshot service.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UObject;
class UBlueprint;
class UDataTable;
class UEdGraph;
class UEdGraphNode;
class UWidgetTree;
struct FBlueprintHelperReviewAtomicTarget;

class BLUEPRINTHELPER_API FBlueprintHelperReviewBaselineSnapshotService
{
public:
	TArray<FString> CaptureSemanticBaselineSnapshots(
		const FString& ArchiveSessionId,
		const TArray<FString>& AssetPaths,
		TArray<FString>* OutWarnings = nullptr) const;

	bool CaptureTargetSnapshot(
		const FBlueprintHelperReviewAtomicTarget& Target,
		FString& OutSnapshotJson,
		FString& OutSnapshotHash,
		FString& OutError) const;

private:
	static FString MakeSnapshotKey(const FString& AssetPath);
	static FString MakeSnapshotDirectory(const FString& ArchiveSessionId, const FString& SnapshotKey);
	static FString MakeSnapshotRef(const FString& ArchiveSessionId, const FString& SnapshotKey);
	static UObject* LoadAssetForSnapshot(const FString& AssetPath);
	static bool WriteSnapshotJson(
		const FString& ArchiveSessionId,
		const FString& SnapshotKey,
		const TSharedRef<FJsonObject>& Snapshot,
		FString& OutError);

	static TSharedRef<FJsonObject> BuildAssetSnapshot(const FString& AssetPath, UObject* Asset);
	static TSharedRef<FJsonObject> BuildBlueprintSnapshot(const UBlueprint* Blueprint);
	static TSharedRef<FJsonObject> BuildDataTableSnapshot(const UDataTable* DataTable);
	static TSharedRef<FJsonObject> BuildGenericObjectSnapshot(UObject* Asset);
	static TSharedRef<FJsonObject> BuildGraphSnapshot(const UEdGraph* Graph, const FString& Surface);
	static TSharedRef<FJsonObject> BuildNodeSnapshot(const UEdGraphNode* Node);
	static TSharedRef<FJsonObject> BuildWidgetTreeSnapshot(UWidgetTree* WidgetTree);
	static TSharedRef<FJsonObject> BuildTargetSnapshot(
		const FBlueprintHelperReviewAtomicTarget& Target,
		UObject* Asset,
		bool bAssetExists);
};
