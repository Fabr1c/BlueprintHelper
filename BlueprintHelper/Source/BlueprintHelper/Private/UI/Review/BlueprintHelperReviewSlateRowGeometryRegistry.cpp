// BlueprintHelper Review slate row geometry registry.

#include "UI/Review/BlueprintHelperReviewSlateRowGeometryRegistry.h"
#include "UI/Review/BlueprintHelperReviewGeometrySearchService.h"
#include "Widgets/SWidget.h"

void FBlueprintHelperReviewSlateRowGeometryRegistry::RegisterRow(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText,
	const TSharedRef<SWidget>& RowWidget,
	const TCHAR* DebugMode)
{
	if (AssetPath.IsEmpty() || SearchText.IsEmpty() || Surface == EBlueprintHelperReviewSurface::Unknown)
	{
		return;
	}

	TArray<FSlateRowGeometryRecord>& Records = GetRecords();
	Records.RemoveAll([](const FSlateRowGeometryRecord& Record)
	{
		return !Record.RowWidget.IsValid();
	});

	FSlateRowGeometryRecord Record;
	Record.AssetPath = AssetPath;
	Record.Surface = Surface;
	Record.SearchText = SearchText;
	Record.DebugMode = DebugMode ? DebugMode : TEXT("slate_row");
	Record.RowWidget = RowWidget;
	Records.Add(Record);

	GetRowsChangedDelegate().Broadcast(AssetPath, Surface);
}

FDelegateHandle FBlueprintHelperReviewSlateRowGeometryRegistry::AddRowsChangedHandler(
	const FBlueprintHelperReviewSlateRowLifecycleChanged::FDelegate& Handler)
{
	return GetRowsChangedDelegate().Add(Handler);
}

void FBlueprintHelperReviewSlateRowGeometryRegistry::RemoveRowsChangedHandler(FDelegateHandle& Handle)
{
	if (Handle.IsValid())
	{
		GetRowsChangedDelegate().Remove(Handle);
		Handle.Reset();
	}
}

bool FBlueprintHelperReviewSlateRowGeometryRegistry::ResolveRowGeometry(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& TargetText,
	const TSharedPtr<SWidget>& OverlayWidget,
	FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
{
	if (AssetPath.IsEmpty() || TargetText.IsEmpty())
	{
		OutAnchor.Reason = TEXT("missing_geometry_target");
		return false;
	}
	if (!OverlayWidget.IsValid())
	{
		OutAnchor.Reason = TEXT("overlay_geometry_unavailable");
		return false;
	}

	TArray<FSlateRowGeometryRecord>& Records = GetRecords();
	for (int32 Index = Records.Num() - 1; Index >= 0; --Index)
	{
		FSlateRowGeometryRecord& Record = Records[Index];
		TSharedPtr<SWidget> RowWidget = Record.RowWidget.Pin();
		if (!RowWidget.IsValid())
		{
			Records.RemoveAt(Index);
			continue;
		}
		if (Record.Surface != Surface || Record.AssetPath != AssetPath)
		{
			continue;
		}
		if (!GeometrySearchTextMatches(Record.SearchText, TargetText))
		{
			continue;
		}

		const FGeometry& RowGeometry = RowWidget->GetTickSpaceGeometry();
		const FGeometry& OverlayGeometry = OverlayWidget->GetTickSpaceGeometry();
		const FVector2D RowLocalSize = RowGeometry.GetLocalSize();
		const FVector2D OverlayLocalSize = OverlayGeometry.GetLocalSize();
		if (RowLocalSize.X <= 0.0f || RowLocalSize.Y <= 0.0f
			|| OverlayLocalSize.X <= 0.0f || OverlayLocalSize.Y <= 0.0f)
		{
			OutAnchor.Reason = TEXT("slate_row_geometry_not_ready");
			return false;
		}

		const FVector2D AbsoluteTopLeft = RowGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D AbsoluteBottomRight = RowGeometry.LocalToAbsolute(RowLocalSize);
		const FVector2D LocalTopLeft = OverlayGeometry.AbsoluteToLocal(AbsoluteTopLeft);
		const FVector2D LocalBottomRight = OverlayGeometry.AbsoluteToLocal(AbsoluteBottomRight);
		const FVector2D LocalSize = LocalBottomRight - LocalTopLeft;
		if (LocalSize.X <= 0.0f || LocalSize.Y <= 0.0f)
		{
			OutAnchor.Reason = TEXT("invalid_slate_row_geometry");
			return false;
		}

		OutAnchor.bIsValid = true;
		OutAnchor.Position = LocalTopLeft;
		OutAnchor.Size = LocalSize;
		OutAnchor.HostSize = OverlayLocalSize;
		OutAnchor.TargetText = Record.SearchText;
		OutAnchor.Reason = TEXT("stable_slate_row_geometry");
		OutAnchor.DebugMode = Record.DebugMode.IsEmpty() ? TEXT("slate_row") : Record.DebugMode;
		return true;
	}

	OutAnchor.Reason = TEXT("no_matching_slate_row_geometry");
	return false;
}

TArray<FBlueprintHelperReviewSlateRowGeometryRegistry::FSlateRowGeometryRecord>&
FBlueprintHelperReviewSlateRowGeometryRegistry::GetRecords()
{
	static TArray<FSlateRowGeometryRecord> Records;
	return Records;
}

FBlueprintHelperReviewSlateRowLifecycleChanged&
FBlueprintHelperReviewSlateRowGeometryRegistry::GetRowsChangedDelegate()
{
	static FBlueprintHelperReviewSlateRowLifecycleChanged Delegate;
	return Delegate;
}

FString FBlueprintHelperReviewSlateRowGeometryRegistry::NormalizeGeometrySearchText(FString Text)
{
	return FBlueprintHelperReviewGeometrySearchService::NormalizeSearchText(MoveTemp(Text));
}

void FBlueprintHelperReviewSlateRowGeometryRegistry::AddGeometrySearchTerms(
	const FString& RawText,
	TArray<FString>& OutTerms)
{
	FBlueprintHelperReviewGeometrySearchService::AddGeometrySearchTerms(RawText, OutTerms);
}

bool FBlueprintHelperReviewSlateRowGeometryRegistry::GeometrySearchTextMatches(
	const FString& RowSearchText,
	const FString& TargetText)
{
	return FBlueprintHelperReviewGeometrySearchService::GeometrySearchTextMatches(RowSearchText, TargetText);
}
