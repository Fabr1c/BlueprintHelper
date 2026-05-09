// BlueprintHelper Review asset-specific presenters.

#include "UI/Review/BlueprintHelperReviewAssetPresenters.h"

#include <initializer_list>

#include "Engine/DataTable.h"
#include "Systems/ToolClusters/DataTable/BlueprintHelperDataTableService.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenter.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace BlueprintHelperReviewAssetPresentersPrivate
{
	static bool TargetKindEqualsAny(
		const FString& TargetKind,
		std::initializer_list<const TCHAR*> ExpectedKinds)
	{
		FString Normalized = TargetKind;
		Normalized.ToLowerInline();
		for (const TCHAR* ExpectedKind : ExpectedKinds)
		{
			if (Normalized == ExpectedKind)
			{
				return true;
			}
		}
		return false;
	}

	static bool ChangeHasTargetKind(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface,
		std::initializer_list<const TCHAR*> ExpectedKinds)
	{
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (Target.Surface != Surface)
			{
				continue;
			}
			if (TargetKindEqualsAny(Target.TargetKind, ExpectedKinds))
			{
				return true;
			}
		}

		FString Location = Change.LocationKey;
		Location.ToLowerInline();
		for (const TCHAR* ExpectedKind : ExpectedKinds)
		{
			if (Location.Contains(ExpectedKind))
			{
				return true;
			}
		}
		return false;
	}

	static TSharedRef<SWidget> BuildLine(const FString& Text, const FLinearColor& Color)
	{
		return SNew(STextBlock)
			.ColorAndOpacity(FSlateColor(Color))
			.AutoWrapText(true)
			.Text(FText::FromString(Text));
	}

	static TSharedRef<SWidget> BuildRegisteredLine(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& Text,
		const FLinearColor& Color)
	{
		TSharedPtr<SBox> RowBox;
		TSharedRef<SWidget> RowWidget = SAssignNew(RowBox, SBox)
		[
			BuildLine(Text, Color)
		];

		FBlueprintHelperReviewSlateRowGeometryRegistry::RegisterRow(
			AssetPath,
			Surface,
			Text,
			RowWidget);
		return RowWidget;
	}

	static TSharedRef<SWidget> BuildSummaryPanel(
		const FString& Title,
		const TArray<FString>& Lines,
		const FString& AssetPath = FString(),
		EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Unknown)
	{
		TSharedRef<SScrollBox> Scroll = SNew(SScrollBox);
		Scroll->AddSlot()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			BuildLine(Title, FLinearColor(0.84f, 0.84f, 0.84f, 1.0f))
		];
		for (const FString& Line : Lines)
		{
			Scroll->AddSlot()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				BuildRegisteredLine(
					AssetPath,
					Surface,
					Line,
					FLinearColor(0.62f, 0.62f, 0.62f, 1.0f))
			];
		}

		return SNew(SBorder)
			.Padding(8.0f)
			[
				Scroll
			];
	}

	static bool IsSurfaceRoutable(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface)
	{
		return FBlueprintHelperReviewSurfacePresenterRouter::ShouldShowChangeOnSurface(Change, Surface);
	}

	static bool ShouldShowIndependentSurfaceChange(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface,
		std::initializer_list<const TCHAR*> ExpectedKinds)
	{
		if (!IsSurfaceRoutable(Change, Surface))
		{
			return false;
		}

		if (BlueprintHelperReviewHasExplicitTargets(Change))
		{
			return ChangeHasTargetKind(Change, Surface, ExpectedKinds);
		}
		return true;
	}
}

bool FBlueprintHelperReviewUMGWidgetTreePresenter::ShouldShowChange(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	return BlueprintHelperReviewAssetPresentersPrivate::ShouldShowIndependentSurfaceChange(
		Change,
		EBlueprintHelperReviewSurface::UMGWidgetTree,
		{TEXT("umg_widget"), TEXT("umg_widget_property"), TEXT("asset_factory")});
}

TSharedRef<SWidget> FBlueprintHelperReviewUMGWidgetTreePresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext& Context)
{
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Asset: %s"), *Context.AssetPath));
	Lines.Add(FString::Printf(TEXT("Kind: %s"), BlueprintHelperReviewAssetKindToString(Context.AssetKind)));

	FBlueprintHelperWidgetService WidgetService;
	const FString WidgetAssetPath = Context.ObjectPath.IsEmpty() ? Context.AssetPath : Context.ObjectPath;
	const FBlueprintHelperWidgetTreeResult Result = WidgetService.GetWidgetTree(WidgetAssetPath);
	if (!Result.bSuccess)
	{
		Lines.Add(FString::Printf(TEXT("WidgetTree: unavailable (%s)"), *Result.ErrorMessage));
		return BlueprintHelperReviewAssetPresentersPrivate::BuildSummaryPanel(
			TEXT("UMG Widget Tree"),
			Lines,
			Context.AssetPath,
			EBlueprintHelperReviewSurface::UMGWidgetTree);
	}

	Lines.Add(FString::Printf(TEXT("Root: %s"), Result.RootWidgetName.IsEmpty() ? TEXT("<none>") : *Result.RootWidgetName));
	Lines.Add(FString::Printf(TEXT("Widget count: %d"), Result.Widgets.Num()));
	for (const FBlueprintHelperWidgetInfo& Widget : Result.Widgets)
	{
		FString Indent;
		for (int32 DepthIndex = 0; DepthIndex < Widget.Depth; ++DepthIndex)
		{
			Indent += TEXT("  ");
		}
		Lines.Add(FString::Printf(
			TEXT("%s- %s : %s children=%d"),
			*Indent,
			*Widget.Name,
			*Widget.WidgetClass,
			Widget.ChildCount));
	}

	return BlueprintHelperReviewAssetPresentersPrivate::BuildSummaryPanel(
		TEXT("UMG Widget Tree"),
		Lines,
		Context.AssetPath,
		EBlueprintHelperReviewSurface::UMGWidgetTree);
}

TSharedRef<SWidget> FBlueprintHelperReviewUMGWidgetTreePresenter::BuildOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
{
	return FBlueprintHelperReviewSurfaceFrameBuilder::BuildReviewListOverlay(
		Args,
		EBlueprintHelperReviewSurface::UMGWidgetTree,
		&FBlueprintHelperReviewUMGWidgetTreePresenter::ShouldShowChange);
}

bool FBlueprintHelperReviewDataTablePresenter::ShouldShowChange(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	return BlueprintHelperReviewAssetPresentersPrivate::ShouldShowIndependentSurfaceChange(
		Change,
		EBlueprintHelperReviewSurface::DataTable,
		{TEXT("datatable_row"), TEXT("data_table"), TEXT("asset_factory")});
}

TSharedRef<SWidget> FBlueprintHelperReviewDataTablePresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext& Context)
{
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Asset: %s"), *Context.AssetPath));
	Lines.Add(FString::Printf(TEXT("Kind: %s"), BlueprintHelperReviewAssetKindToString(Context.AssetKind)));

	FBlueprintHelperDataTableService DataTableService;
	const FString DataTableAssetPath = Context.ObjectPath.IsEmpty() ? Context.AssetPath : Context.ObjectPath;
	const FBlueprintHelperDataTableRowsResult Result = DataTableService.GetDataTableRows(DataTableAssetPath);
	if (!Result.bSuccess)
	{
		Lines.Add(FString::Printf(TEXT("DataTable: unavailable (%s)"), *Result.ErrorMessage));
		return BlueprintHelperReviewAssetPresentersPrivate::BuildSummaryPanel(
			TEXT("DataTable Summary"),
			Lines,
			Context.AssetPath,
			EBlueprintHelperReviewSurface::DataTable);
	}

	Lines.Add(FString::Printf(TEXT("Row struct: %s"), Result.RowStructName.IsEmpty() ? TEXT("<none>") : *Result.RowStructName));
	Lines.Add(FString::Printf(TEXT("Column count: %d"), Result.Columns.Num()));
	Lines.Add(FString::Printf(TEXT("Row count: %d"), Result.Rows.Num()));

	for (const FBlueprintHelperDataTableColumnInfo& Column : Result.Columns)
	{
		Lines.Add(FString::Printf(TEXT("Column: %s : %s"), *Column.Name, *Column.TypeName));
	}

	for (const FBlueprintHelperDataTableRowInfo& Row : Result.Rows)
	{
		FString RowLine = FString::Printf(TEXT("Row: %s"), *Row.RowName.ToString());
		for (const TPair<FString, FString>& Field : Row.Fields)
		{
			RowLine += FString::Printf(TEXT(" %s=%s"), *Field.Key, *Field.Value);
		}
		Lines.Add(RowLine);
	}

	return BlueprintHelperReviewAssetPresentersPrivate::BuildSummaryPanel(
		TEXT("DataTable Summary"),
		Lines,
		Context.AssetPath,
		EBlueprintHelperReviewSurface::DataTable);
}

TSharedRef<SWidget> FBlueprintHelperReviewDataTablePresenter::BuildOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
{
	return FBlueprintHelperReviewSurfaceFrameBuilder::BuildReviewListOverlay(
		Args,
		EBlueprintHelperReviewSurface::DataTable,
		&FBlueprintHelperReviewDataTablePresenter::ShouldShowChange);
}

bool FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	return BlueprintHelperReviewAssetPresentersPrivate::ShouldShowIndependentSurfaceChange(
		Change,
		EBlueprintHelperReviewSurface::DataAsset,
		{TEXT("object_property"), TEXT("data_asset_property"), TEXT("asset_factory")});
}

TSharedRef<SWidget> FBlueprintHelperReviewDataAssetPresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext& Context)
{
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Asset: %s"), *Context.AssetPath));
	Lines.Add(FString::Printf(TEXT("Kind: %s"), BlueprintHelperReviewAssetKindToString(Context.AssetKind)));

	FBlueprintHelperPropertyReflectionService PropertyService;
	const FString ObjectAssetPath = Context.ObjectPath.IsEmpty() ? Context.AssetPath : Context.ObjectPath;
	const FBlueprintHelperObjectPropertiesResult Result = PropertyService.GetObjectProperties(ObjectAssetPath);
	if (!Result.bSuccess)
	{
		Lines.Add(FString::Printf(TEXT("Object properties: unavailable (%s)"), *Result.ErrorMessage));
		return BlueprintHelperReviewAssetPresentersPrivate::BuildSummaryPanel(
			TEXT("Object Details Summary"),
			Lines,
			Context.AssetPath,
			EBlueprintHelperReviewSurface::DataAsset);
	}

	Lines.Add(FString::Printf(TEXT("Class: %s"), Result.ClassName.IsEmpty() ? TEXT("<none>") : *Result.ClassName));
	Lines.Add(FString::Printf(TEXT("Property count: %d"), Result.Properties.Num()));
	for (const FBlueprintHelperObjectPropertyInfo& Property : Result.Properties)
	{
		Lines.Add(FString::Printf(
			TEXT("- %s : %s = %s"),
			*Property.Name,
			*Property.TypeName,
			*Property.Value));
	}

	return BlueprintHelperReviewAssetPresentersPrivate::BuildSummaryPanel(
		TEXT("Object Details Summary"),
		Lines,
		Context.AssetPath,
		EBlueprintHelperReviewSurface::DataAsset);
}

TSharedRef<SWidget> FBlueprintHelperReviewDataAssetPresenter::BuildOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
{
	return FBlueprintHelperReviewSurfaceFrameBuilder::BuildReviewListOverlay(
		Args,
		EBlueprintHelperReviewSurface::DataAsset,
		&FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange);
}
