#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetTreePositionPolicy.h"

#include "Components/PanelWidget.h"
#include "Components/Widget.h"

class FBlueprintHelperWidgetTreePositionPolicyLocalUtils
{
public:
	static bool Reject(
		const TCHAR* ErrorCode,
		const FString& ErrorMessage,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		OutErrorCode = ErrorCode;
		OutErrorMessage = ErrorMessage;
		return false;
	}

	static bool FindProjectedPosition(
		const FBlueprintHelperWidgetTreeSummary& Summary,
		const FString& WidgetName,
		FString& OutParentName,
		int32& OutVirtualIndex)
	{
		if (const FBlueprintHelperWidgetTreeItem* Item = Summary.Index.Find(WidgetName))
		{
			OutParentName = Item->ParentName;
			OutVirtualIndex = Item->VirtualIndex;
			return true;
		}

		for (const FBlueprintHelperNamedSlotEntry& NamedSlot : Summary.NamedSlots)
		{
			if (NamedSlot.ContentWidgetName == WidgetName)
			{
				OutParentName = NamedSlot.HostWidgetName;
				OutVirtualIndex = NamedSlot.VirtualIndex;
				return true;
			}
		}

		return false;
	}
};

int32 FBlueprintHelperWidgetTreePositionPolicy::BuildPanelVirtualIndex(
	const UPanelWidget* ParentPanel,
	const UWidget* ChildWidget)
{
	if (!ParentPanel || !ChildWidget)
	{
		return INDEX_NONE;
	}

	return ParentPanel->GetChildIndex(ChildWidget);
}

bool FBlueprintHelperWidgetTreePositionPolicy::ValidateVirtualIndexForPanel(
	const UPanelWidget* ParentPanel,
	int32 VirtualIndex,
	bool bAllowEndInsert,
	FString& OutErrorCode,
	FString& OutErrorMessage)
{
	OutErrorCode.Reset();
	OutErrorMessage.Reset();

	if (!ParentPanel)
	{
		return FBlueprintHelperWidgetTreePositionPolicyLocalUtils::Reject(
			TEXT("invalid_parent_widget"),
			TEXT("Parent panel is required for panel virtual_index validation."),
			OutErrorCode,
			OutErrorMessage);
	}

	if (VirtualIndex < 0)
	{
		return FBlueprintHelperWidgetTreePositionPolicyLocalUtils::Reject(
			TEXT("invalid_virtual_index"),
			TEXT("virtual_index must be non-negative."),
			OutErrorCode,
			OutErrorMessage);
	}

	const int32 ChildCount = ParentPanel->GetChildrenCount();
	const int32 MaxAllowedIndex = bAllowEndInsert ? ChildCount : ChildCount - 1;
	if (VirtualIndex > MaxAllowedIndex)
	{
		return FBlueprintHelperWidgetTreePositionPolicyLocalUtils::Reject(
			TEXT("invalid_virtual_index"),
			FString::Printf(
				TEXT("virtual_index %d is outside the valid panel range 0..%d."),
				VirtualIndex,
				FMath::Max(MaxAllowedIndex, 0)),
			OutErrorCode,
			OutErrorMessage);
	}

	return true;
}

bool FBlueprintHelperWidgetTreePositionPolicy::ValidateSingleContentVirtualIndex(
	int32 VirtualIndex,
	FString& OutErrorCode,
	FString& OutErrorMessage)
{
	OutErrorCode.Reset();
	OutErrorMessage.Reset();

	if (VirtualIndex != 0)
	{
		return FBlueprintHelperWidgetTreePositionPolicyLocalUtils::Reject(
			TEXT("invalid_virtual_index"),
			TEXT("Single-content slots only accept virtual_index 0."),
			OutErrorCode,
			OutErrorMessage);
	}

	return true;
}

bool FBlueprintHelperWidgetTreePositionPolicy::ValidateNamedSlotVirtualIndex(
	int32 VirtualIndex,
	FString& OutErrorCode,
	FString& OutErrorMessage)
{
	return ValidateSingleContentVirtualIndex(VirtualIndex, OutErrorCode, OutErrorMessage);
}

int32 FBlueprintHelperWidgetTreePositionPolicy::NormalizeOptionalVirtualIndex(
	TOptional<int32> VirtualIndex)
{
	return VirtualIndex.IsSet() ? VirtualIndex.GetValue() : 0;
}

int32 FBlueprintHelperWidgetTreePositionPolicy::NormalizePanelVirtualIndex(
	TOptional<int32> VirtualIndex)
{
	return NormalizeOptionalVirtualIndex(VirtualIndex);
}

int32 FBlueprintHelperWidgetTreePositionPolicy::NormalizeSingleContentVirtualIndex(
	TOptional<int32> VirtualIndex)
{
	return NormalizeOptionalVirtualIndex(VirtualIndex);
}

int32 FBlueprintHelperWidgetTreePositionPolicy::NormalizeNamedSlotVirtualIndex(
	TOptional<int32> VirtualIndex)
{
	return NormalizeOptionalVirtualIndex(VirtualIndex);
}

bool FBlueprintHelperWidgetTreePositionPolicy::ValidateExpectedPosition(
	const FBlueprintHelperWidgetTreeSummary& Summary,
	const FString& WidgetName,
	const FString& ExpectedParentName,
	const TOptional<int32>& ExpectedVirtualIndex,
	FString& OutErrorCode,
	FString& OutErrorMessage)
{
	OutErrorCode.Reset();
	OutErrorMessage.Reset();

	if (WidgetName.IsEmpty())
	{
		return FBlueprintHelperWidgetTreePositionPolicyLocalUtils::Reject(
			TEXT("invalid_widget_name"),
			TEXT("widget_name is required for expected position validation."),
			OutErrorCode,
			OutErrorMessage);
	}

	FString ActualParentName;
	int32 ActualVirtualIndex = INDEX_NONE;
	if (!FBlueprintHelperWidgetTreePositionPolicyLocalUtils::FindProjectedPosition(
		Summary,
		WidgetName,
		ActualParentName,
		ActualVirtualIndex))
	{
		return FBlueprintHelperWidgetTreePositionPolicyLocalUtils::Reject(
			TEXT("widget_not_found"),
			FString::Printf(TEXT("Widget '%s' was not found in the projected widget tree."), *WidgetName),
			OutErrorCode,
			OutErrorMessage);
	}

	if (!ExpectedParentName.IsEmpty() && ActualParentName != ExpectedParentName)
	{
		return FBlueprintHelperWidgetTreePositionPolicyLocalUtils::Reject(
			TEXT("expected_parent_mismatch"),
			FString::Printf(
				TEXT("Expected parent '%s' but current parent is '%s'."),
				*ExpectedParentName,
				*ActualParentName),
			OutErrorCode,
			OutErrorMessage);
	}

	if (ExpectedVirtualIndex.IsSet() && ActualVirtualIndex != ExpectedVirtualIndex.GetValue())
	{
		return FBlueprintHelperWidgetTreePositionPolicyLocalUtils::Reject(
			TEXT("expected_virtual_index_mismatch"),
			FString::Printf(
				TEXT("Expected virtual_index %d but current virtual_index is %d."),
				ExpectedVirtualIndex.GetValue(),
				ActualVirtualIndex),
			OutErrorCode,
			OutErrorMessage);
	}

	return true;
}
