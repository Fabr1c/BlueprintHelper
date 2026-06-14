// BlueprintHelper Review target kind registry.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

enum class EBlueprintHelperReviewTargetHandlerKind : uint8
{
	Unsupported,
	GraphNode,
	GraphBlock,
	GraphExternalBoundary,
	GraphExternalLink,
	GraphExternalNode,
	GraphExternalBody,
	BlueprintVariable,
	Component,
	Signature,
	UMGWidget,
	UMGWidgetProperty,
	DataTableRow,
	StructField,
	ObjectProperty,
	MaterialGraph,
	AssetFactory
};

struct FBlueprintHelperReviewTargetKindDefinition
{
	const TCHAR* TargetKind = TEXT("");
	EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Unknown;
	bool bCanRouteToDetails = false;
	const TCHAR* DisplayKind = TEXT("");
	EBlueprintHelperReviewTargetHandlerKind HandlerKind = EBlueprintHelperReviewTargetHandlerKind::Unsupported;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewTargetKindRegistry
{
public:
	static EBlueprintHelperReviewSurface NormalizeSurfaceForTarget(
		EBlueprintHelperReviewSurface ExplicitSurface,
		const FString& TargetKind,
		const FString& TargetKey,
		const FString& VisualGroupKey = FString(),
		const FString& LocationKey = FString());

	static bool CanRouteToDetails(const FString& TargetKind);

	static bool IsComponentTargetKind(const FString& TargetKind);
	static bool IsPropertyTargetKind(const FString& TargetKind);
	static bool IsClassDefaultPropertyTargetKind(const FString& TargetKind);
	static bool IsAssetFactoryTargetKind(const FString& TargetKind);
	static bool IsGraphNodeTarget(const FString& TargetKind, const FString& TargetKey);
	static bool IsGraphBlockTarget(const FString& TargetKind, const FString& TargetKey);
	static bool ShouldAggregateAsGraphBody(const FBlueprintHelperReviewAtomicTarget& Target);

	static EBlueprintHelperReviewSurface ResolveAssetFactorySurface(const FString& AssetType);
	static bool IsStructureAssetType(const FString& AssetType);
	static EBlueprintHelperReviewTargetHandlerKind GetHandlerKind(const FString& TargetKind);
	static bool SupportsSnapshotRestore(const FString& TargetKind);

	static const FBlueprintHelperReviewTargetKindDefinition* FindDefinition(const FString& TargetKind);
};
