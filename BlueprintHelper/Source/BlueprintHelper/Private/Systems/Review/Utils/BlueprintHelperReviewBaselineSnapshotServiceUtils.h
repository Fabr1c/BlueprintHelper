// BlueprintHelper Review baseline snapshot service utilities.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

class UBlueprint;
class UEdGraph;
class UObject;
class USCS_Node;
class FJsonObject;
class FJsonValue;

class FBlueprintHelperReviewBaselineSnapshotServiceUtils
{
public:
	static TArray<TSharedPtr<FJsonValue>> MakeStringArray(const TArray<FString>& Values);
	static FString GetObjectPathNameSafe(const UObject* Object);
	static FString GetObjectClassPathNameSafe(const UObject* Object);
	static FString SerializeJsonObject(const TSharedRef<FJsonObject>& Json);
	static FString ExtractTargetName(const FBlueprintHelperReviewAtomicTarget& Target);
	static UObject* ResolveClassDefaultSnapshotObject(UObject* Asset);
	static void SplitWidgetPropertyTarget(
		const FString& TargetName,
		FString& OutWidgetName,
		FString& OutPropertyName);
	static FString FindScsParentComponentName(const UBlueprint* Blueprint, const USCS_Node* ChildNode);
	static FString PinDirectionToString(EEdGraphPinDirection Direction);
	static void AppendGraphs(TArray<UEdGraph*>& OutGraphs, const TArray<UEdGraph*>& InGraphs);
};
