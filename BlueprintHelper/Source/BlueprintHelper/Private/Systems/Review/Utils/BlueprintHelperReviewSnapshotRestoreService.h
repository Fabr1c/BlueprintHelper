// BlueprintHelper Review FBlueprintHelperReviewSnapshotRestoreService declarations.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/DataTable.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"

class FBlueprintHelperReviewSnapshotRestoreService
{
public:
	static bool IsAssetFactoryTarget(const FBlueprintHelperReviewAtomicTarget& Target);
	static FString ExtractTargetName(const FBlueprintHelperReviewAtomicTarget& Target);
	static void SplitWidgetPropertyTarget(
				const FString& TargetName,
				FString& OutWidgetName,
				FString& OutPropertyName);
	static bool ParseReviewSnapshotJson(
				const FString& SnapshotJson,
				TSharedPtr<FJsonObject>& OutSnapshot,
				FString& OutError);
	static int32 FindBlueprintVariableIndex(UBlueprint* Blueprint, const FName VariableName);
	static USCS_Node* FindScsNodeByName(UBlueprint* Blueprint, const FString& ComponentName);
	static void MarkBlueprintReviewRestoreModified(UBlueprint* Blueprint);
	static bool RestoreBlueprintVariableFromSnapshot(
				const FBlueprintHelperReviewAtomicTarget& Target,
				const TSharedPtr<FJsonObject>& Snapshot,
				FString& OutError);
	static bool RestoreComponentPropertiesFromSnapshot(
				UObject* ComponentTemplate,
				const TSharedPtr<FJsonObject>& Snapshot,
				FString& OutError);
	static bool RestoreComponentFromSnapshot(
				const FBlueprintHelperReviewAtomicTarget& Target,
				const TSharedPtr<FJsonObject>& Snapshot,
				FString& OutError);
	static UObject* LoadReviewTargetAsset(const FString& AssetPath);
	static bool RestoreDataTableRowFromSnapshot(
				const FBlueprintHelperReviewAtomicTarget& Target,
				const TSharedPtr<FJsonObject>& Snapshot,
				FString& OutError);
	static FStructVariableDescription* FindStructFieldByReviewName(
				UUserDefinedStruct* Struct,
				const FString& FieldName);
	static bool RestoreStructFieldFromSnapshot(
				const FBlueprintHelperReviewAtomicTarget& Target,
				const TSharedPtr<FJsonObject>& Snapshot,
				FString& OutError);
	static bool ParseSnapshotBoolValue(const FString& ValueText, bool& OutValue);
	static bool RestoreObjectPropertyFromSnapshot(
				const FBlueprintHelperReviewAtomicTarget& Target,
				const TSharedPtr<FJsonObject>& Snapshot,
				FString& OutError);
	static bool RestoreWidgetFromSnapshot(
				const FBlueprintHelperReviewAtomicTarget& Target,
				const TSharedPtr<FJsonObject>& Snapshot,
				FString& OutError);
	static bool RestoreGraphFromSnapshot(
				const FBlueprintHelperReviewAtomicTarget& Target,
				const TSharedPtr<FJsonObject>& Snapshot,
				FString& OutError);
	static bool RestoreSignatureFromSnapshot(
				const FBlueprintHelperReviewAtomicTarget& Target,
				const TSharedPtr<FJsonObject>& Snapshot,
				FString& OutError);
	static bool ExecuteSnapshotRestore(
				const FBlueprintHelperReviewAtomicTarget& Target,
				FString& OutError);
	static bool ShouldUseSnapshotRestore(const FBlueprintHelperReviewAtomicTarget& Target);
	static FString MakeObjectPathFromAssetPath(FString AssetPath);
	static FBlueprintHelperReviewActionResult RejectAssetFactoryTargetWithDefaultDispatcher(
				const FBlueprintHelperReviewVisibleChange& Change,
				const FBlueprintHelperReviewAtomicTarget& Target);
};
