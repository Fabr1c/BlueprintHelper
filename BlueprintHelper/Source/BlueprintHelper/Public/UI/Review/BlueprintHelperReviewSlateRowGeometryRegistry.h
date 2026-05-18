// BlueprintHelper Review slate row geometry registry.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FBlueprintHelperReviewSlateRowLifecycleChanged,
	const FString& /* AssetPath */,
	EBlueprintHelperReviewSurface /* Surface */);

/**
 * Slate 行几何注册表，用于注册和解析行 widget 的几何信息。
 */
class BLUEPRINTHELPER_API FBlueprintHelperReviewSlateRowGeometryRegistry
{
public:
	static void RegisterRow(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText,
		const TSharedRef<SWidget>& RowWidget,
		const TCHAR* DebugMode = TEXT("slate_row"));

	static FDelegateHandle AddRowsChangedHandler(
		const FBlueprintHelperReviewSlateRowLifecycleChanged::FDelegate& Handler);
	static void RemoveRowsChangedHandler(FDelegateHandle& Handle);

	static bool ResolveRowGeometry(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& TargetText,
		const TSharedPtr<SWidget>& OverlayWidget,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor);

private:
	struct FSlateRowGeometryRecord
	{
		FString AssetPath;
		EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Unknown;
		FString SearchText;
		FString DebugMode;
		TWeakPtr<SWidget> RowWidget;
	};

	static TArray<FSlateRowGeometryRecord>& GetRecords();
	static FBlueprintHelperReviewSlateRowLifecycleChanged& GetRowsChangedDelegate();
	static FString NormalizeGeometrySearchText(FString Text);
	static void AddGeometrySearchTerms(const FString& RawText, TArray<FString>& OutTerms);
	static bool GeometrySearchTextMatches(const FString& RowSearchText, const FString& TargetText);
};
