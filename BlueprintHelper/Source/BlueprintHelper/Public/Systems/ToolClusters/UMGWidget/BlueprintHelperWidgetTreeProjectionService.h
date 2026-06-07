#pragma once

#include "CoreMinimal.h"
#include "Shared/UMGWidget/BlueprintHelperWidgetTypes.h"

class UWidget;
class UWidgetBlueprint;

class BLUEPRINTHELPER_API FBlueprintHelperWidgetTreeProjectionService
{
public:
	static bool BuildWidgetTreeSummary(
		UWidgetBlueprint* WidgetBlueprint,
		FBlueprintHelperWidgetTreeSummary& OutSummary,
		FString& OutErrorCode,
		FString& OutErrorMessage);

private:
	static FBlueprintHelperWidgetTreeItem BuildWidgetItem(
		UWidgetBlueprint* WidgetBlueprint,
		UWidget* Widget,
		const FString& ParentName,
		const FString& SlotName,
		int32 VirtualIndex);

	static void AppendPanelChildren(
		UWidgetBlueprint* WidgetBlueprint,
		UWidget* Widget,
		FBlueprintHelperWidgetTreeItem& Item);

	static void BuildFlatIndex(
		const FBlueprintHelperWidgetTreeItem& Item,
		TMap<FString, FBlueprintHelperWidgetTreeItem>& OutIndex);

	static void AppendNamedSlotFacts(
		UWidgetBlueprint* WidgetBlueprint,
		FBlueprintHelperWidgetTreeSummary& Summary);
};
