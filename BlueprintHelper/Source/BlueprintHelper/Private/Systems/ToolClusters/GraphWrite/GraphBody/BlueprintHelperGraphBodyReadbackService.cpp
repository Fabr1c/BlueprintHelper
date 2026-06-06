#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyReadbackService.h"

#include "Engine/Blueprint.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapterResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyRequest.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyTarget.h"

class FBlueprintHelperGraphBodyReadbackServiceLocalUtils
{
public:
	static TArray<TSharedPtr<FJsonValue>> StringsToJson(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> JsonValues;
		for (const FString& Value : Values)
		{
			if (!Value.IsEmpty())
			{
				JsonValues.Add(MakeShared<FJsonValueString>(Value));
			}
		}
		return JsonValues;
	}

	static FString DisplayNameForBoundaryRef(
		const FString& Ref,
		const FBlueprintHelperGraphBodyReadbackProjection& Projection)
	{
		if (const FString* DisplayName = Projection.BoundaryDisplayNames.Find(Ref))
		{
			return *DisplayName;
		}
		if (Ref.StartsWith(TEXT("CustomEvent:")))
		{
			return Ref.RightChop(12);
		}
		if (Ref.StartsWith(TEXT("Event:")))
		{
			return Ref.RightChop(6);
		}
		return Ref;
	}

	static TArray<TSharedPtr<FJsonValue>> BoundaryRefsToJson(
		const TArray<FString>& Refs,
		const FBlueprintHelperGraphBodyReadbackProjection& Projection)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FString& Ref : Refs)
		{
			if (Ref.IsEmpty())
			{
				continue;
			}

			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("node_ref"), Ref);
			Item->SetStringField(TEXT("display_name"), DisplayNameForBoundaryRef(Ref, Projection));
			Values.Add(MakeShared<FJsonValueObject>(Item));
		}
		return Values;
	}

};

bool FBlueprintHelperGraphBodyReadbackService::BuildAdapterBoundaryForTarget(
	const FBlueprintHelperTargetRef& Target,
	TSharedPtr<FJsonObject>& OutAdapterBoundaryJson,
	FString& OutError) const
{
	OutAdapterBoundaryJson.Reset();
	OutError.Reset();

	if (Target.AssetPath.IsEmpty())
	{
		OutError = TEXT("Readback adapter boundary requires an asset path.");
		return false;
	}

	UBlueprint* Blueprint = FindObject<UBlueprint>(nullptr, *Target.AssetPath);
	if (!Blueprint)
	{
		Blueprint = LoadObject<UBlueprint>(nullptr, *Target.AssetPath);
	}
	if (!Blueprint)
	{
		OutError = FString::Printf(TEXT("Unable to load Blueprint for adapter boundary: %s."), *Target.AssetPath);
		return false;
	}

	FBlueprintHelperGraphBodyRequest Request =
		FBlueprintHelperGraphBodyAdapterResolver::MakeReadRequestForTarget(Target, Blueprint);

	TUniquePtr<IBlueprintHelperGraphBodyAdapter> Adapter;
	return FBlueprintHelperGraphBodyAdapterResolver::TryCreateForReadTarget(Target, Adapter, OutError) &&
		Adapter.IsValid() &&
		TryBuildAdapterBoundary(*Adapter, Request, OutAdapterBoundaryJson, OutError);
}

TSharedRef<FJsonObject> FBlueprintHelperGraphBodyReadbackService::BuildAdapterBoundaryJson(
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary,
	const FBlueprintHelperGraphBodyReadbackProjection& Projection) const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("runtime_adapter_id"), Boundary.RuntimeAdapterId);
	Json->SetStringField(TEXT("body_kind"), FBlueprintHelperGraphBodyBoundaryModelUtils::BodyKindToString(Boundary.BodyKind));
	Json->SetStringField(TEXT("graph_name"), Boundary.GraphName);
	Json->SetArrayField(
		TEXT("entry_boundaries"),
		FBlueprintHelperGraphBodyReadbackServiceLocalUtils::BoundaryRefsToJson(Boundary.EntryNodeRefs, Projection));
	Json->SetArrayField(
		TEXT("exit_boundaries"),
		FBlueprintHelperGraphBodyReadbackServiceLocalUtils::BoundaryRefsToJson(Boundary.ExitNodeRefs, Projection));
	Json->SetArrayField(
		TEXT("folded_boundary_node_refs"),
		FBlueprintHelperGraphBodyReadbackServiceLocalUtils::StringsToJson(Projection.FoldedBoundaryNodeRefs));
	Json->SetArrayField(
		TEXT("visible_boundary_node_refs"),
		FBlueprintHelperGraphBodyReadbackServiceLocalUtils::StringsToJson(Projection.VisibleBoundaryNodeRefs));
	return Json;
}

bool FBlueprintHelperGraphBodyReadbackService::TryBuildAdapterBoundary(
	const IBlueprintHelperGraphBodyAdapter& Adapter,
	const FBlueprintHelperGraphBodyRequest& Request,
	TSharedPtr<FJsonObject>& OutAdapterBoundaryJson,
	FString& OutError) const
{
	FBlueprintHelperGraphBodyTarget Target;
	if (!Adapter.ResolveTarget(Request, Target, OutError))
	{
		return false;
	}

	const FBlueprintHelperGraphBodyBoundaryModel Boundary = Adapter.BuildBoundaryModel(Target, Request);
	const FBlueprintHelperGraphBodyReadbackProjection Projection =
		Adapter.BuildReadbackProjection(Target, Boundary);
	OutAdapterBoundaryJson = BuildAdapterBoundaryJson(Boundary, Projection);
	OutError.Reset();
	return true;
}
