// BlueprintHelper Review target validity DTO implementation.

#include "Systems/Review/BlueprintHelperReviewTargetValidityTypes.h"

const TCHAR* BlueprintHelperReviewInvalidReasonToString(
	EBlueprintHelperReviewInvalidReason Reason)
{
	switch (Reason)
	{
	case EBlueprintHelperReviewInvalidReason::None:
		return TEXT("none");
	case EBlueprintHelperReviewInvalidReason::AssetMissing:
		return TEXT("asset_missing");
	case EBlueprintHelperReviewInvalidReason::GraphMissing:
		return TEXT("graph_missing");
	case EBlueprintHelperReviewInvalidReason::GraphNodeMissing:
		return TEXT("graph_node_missing");
	case EBlueprintHelperReviewInvalidReason::VariableMissingOrRenamed:
		return TEXT("variable_missing_or_renamed");
	case EBlueprintHelperReviewInvalidReason::FunctionMissingOrRenamed:
		return TEXT("function_missing_or_renamed");
	case EBlueprintHelperReviewInvalidReason::ComponentMissingOrRenamed:
		return TEXT("component_missing_or_renamed");
	case EBlueprintHelperReviewInvalidReason::WidgetMissingOrRenamed:
		return TEXT("widget_missing_or_renamed");
	case EBlueprintHelperReviewInvalidReason::DataTableRowMissing:
		return TEXT("datatable_row_missing");
	case EBlueprintHelperReviewInvalidReason::DataAssetPropertyMissing:
		return TEXT("data_asset_property_missing");
	default:
		return TEXT("unknown");
	}
}
