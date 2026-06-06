#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyReadbackService.h"

#include "Engine/Blueprint.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2CustomEventBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2EventBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2FunctionBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2MacroBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapter.h"
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
		const FBlueprintHelperGraphBodyBoundaryModel& Boundary)
	{
		if (Ref.Equals(TEXT("FunctionEntry"), ESearchCase::IgnoreCase))
		{
			return Boundary.GraphName.IsEmpty() ? TEXT("Function") : Boundary.GraphName;
		}
		if (Ref.Equals(TEXT("FunctionResult"), ESearchCase::IgnoreCase))
		{
			return TEXT("Return");
		}
		if (Ref.Equals(TEXT("TunnelEntry"), ESearchCase::IgnoreCase))
		{
			return TEXT("Macro In");
		}
		if (Ref.Equals(TEXT("TunnelExit"), ESearchCase::IgnoreCase))
		{
			return TEXT("Macro Out");
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
		const FBlueprintHelperGraphBodyBoundaryModel& Boundary)
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
			Item->SetStringField(TEXT("display_name"), DisplayNameForBoundaryRef(Ref, Boundary));
			Values.Add(MakeShared<FJsonValueObject>(Item));
		}
		return Values;
	}

	static FString ResolveRequestGraphName(const FBlueprintHelperTargetRef& Target)
	{
		if (!Target.Graph.IsEmpty())
		{
			return Target.Graph;
		}
		if (Target.TargetType == EBlueprintHelperTargetType::Function && !Target.Function.IsEmpty())
		{
			return Target.Function;
		}
		return TEXT("EventGraph");
	}

	static FString ResolveRequestEntryName(const FBlueprintHelperTargetRef& Target)
	{
		if (Target.TargetType == EBlueprintHelperTargetType::Function)
		{
			return Target.Function;
		}
		if (Target.TargetType == EBlueprintHelperTargetType::Event
			|| Target.TargetType == EBlueprintHelperTargetType::CustomEvent)
		{
			return Target.Event;
		}
		return TEXT("");
	}

	static FBlueprintHelperGraphBodyRequest MakeRequest(
		const FBlueprintHelperTargetRef& Target,
		UBlueprint* Blueprint)
	{
		FBlueprintHelperGraphBodyRequest Request;
		Request.OperationKind = TEXT("read_context");
		Request.TaskSpecStrategy = TEXT("read_context");
		Request.ReplaceScope = TargetTypeToString(Target.TargetType);
		Request.AssetPath = Target.AssetPath;
		Request.GraphName = ResolveRequestGraphName(Target);
		Request.EntryName = ResolveRequestEntryName(Target);
		Request.Blueprint = Blueprint;
		return Request;
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
		FBlueprintHelperGraphBodyReadbackServiceLocalUtils::MakeRequest(Target, Blueprint);

	if (Target.TargetType == EBlueprintHelperTargetType::Function)
	{
		FBlueprintHelperK2FunctionBodyAdapter Adapter;
		return TryBuildAdapterBoundary(Adapter, Request, OutAdapterBoundaryJson, OutError);
	}
	if (Target.TargetType == EBlueprintHelperTargetType::CustomEvent)
	{
		FBlueprintHelperK2CustomEventBodyAdapter Adapter;
		return TryBuildAdapterBoundary(Adapter, Request, OutAdapterBoundaryJson, OutError);
	}
	if (Target.TargetType == EBlueprintHelperTargetType::Event)
	{
		FBlueprintHelperK2EventBodyAdapter Adapter;
		return TryBuildAdapterBoundary(Adapter, Request, OutAdapterBoundaryJson, OutError);
	}
	if (Target.TargetType == EBlueprintHelperTargetType::Graph)
	{
		FBlueprintHelperK2MacroBodyAdapter MacroAdapter;
		if (TryBuildAdapterBoundary(MacroAdapter, Request, OutAdapterBoundaryJson, OutError))
		{
			return true;
		}
		OutError.Reset();
	}

	return false;
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
		FBlueprintHelperGraphBodyReadbackServiceLocalUtils::BoundaryRefsToJson(Boundary.EntryNodeRefs, Boundary));
	Json->SetArrayField(
		TEXT("exit_boundaries"),
		FBlueprintHelperGraphBodyReadbackServiceLocalUtils::BoundaryRefsToJson(Boundary.ExitNodeRefs, Boundary));
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
