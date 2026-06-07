#pragma once

#include "CoreMinimal.h"
#include "Shared/UMGWidget/BlueprintHelperWidgetTypes.h"

class UPanelWidget;
class UWidget;

struct BLUEPRINTHELPER_API FBlueprintHelperWidgetTreePosition
{
	FString ParentName;
	FString SlotName;
	int32 VirtualIndex = 0;
	bool bIsNamedSlot = false;
	bool bIsContentSlot = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperWidgetTreePositionPolicy
{
public:
	static int32 BuildPanelVirtualIndex(const UPanelWidget* ParentPanel, const UWidget* ChildWidget);

	static bool ValidateVirtualIndexForPanel(
		const UPanelWidget* ParentPanel,
		int32 VirtualIndex,
		bool bAllowEndInsert,
		FString& OutErrorCode,
		FString& OutErrorMessage);

	static bool ValidateSingleContentVirtualIndex(
		int32 VirtualIndex,
		FString& OutErrorCode,
		FString& OutErrorMessage);

	static bool ValidateNamedSlotVirtualIndex(
		int32 VirtualIndex,
		FString& OutErrorCode,
		FString& OutErrorMessage);

	static int32 NormalizeOptionalVirtualIndex(TOptional<int32> VirtualIndex);
	static int32 NormalizePanelVirtualIndex(TOptional<int32> VirtualIndex);
	static int32 NormalizeSingleContentVirtualIndex(TOptional<int32> VirtualIndex);
	static int32 NormalizeNamedSlotVirtualIndex(TOptional<int32> VirtualIndex);

	static bool ValidateExpectedPosition(
		const FBlueprintHelperWidgetTreeSummary& Summary,
		const FString& WidgetName,
		const FString& ExpectedParentName,
		const TOptional<int32>& ExpectedVirtualIndex,
		FString& OutErrorCode,
		FString& OutErrorMessage);
};
