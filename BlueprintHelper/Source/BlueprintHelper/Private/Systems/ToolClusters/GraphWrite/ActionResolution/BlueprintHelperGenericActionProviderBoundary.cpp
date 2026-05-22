#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.h"

FBlueprintHelperGenericActionProviderBoundary FBlueprintHelperGenericActionProviderBoundaryService::Classify(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperGenericActionProviderBoundary Boundary;
	const bool bHasTypeName = !Request.Semantic.TypeName.TrimStartAndEnd().IsEmpty();

	switch (Request.Semantic.Kind)
	{
	case EBlueprintHelperActionSemanticKind::Construct:
		Boundary.Mode = bHasTypeName
			? EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate
			: EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext;
		Boundary.RequiredBuilder = TEXT("ConstructFragmentBuilder");
		Boundary.Reason = bHasTypeName
			? TEXT("construct can query struct or generic NodeSpawner candidates by TypeName.")
			: TEXT("construct requires Semantic.TypeName before resolving struct or generic action candidates.");
		return Boundary;

	case EBlueprintHelperActionSemanticKind::Deconstruct:
		Boundary.Mode = bHasTypeName
			? EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate
			: EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext;
		Boundary.RequiredBuilder = TEXT("DeconstructFragmentBuilder");
		Boundary.Reason = bHasTypeName
			? TEXT("deconstruct can query struct or generic NodeSpawner candidates by TypeName.")
			: TEXT("deconstruct requires Semantic.TypeName before resolving struct or generic action candidates.");
		return Boundary;

	case EBlueprintHelperActionSemanticKind::Select:
		Boundary.Mode = EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate;
		Boundary.RequiredBuilder = TEXT("SelectFragmentBuilder");
		Boundary.Reason = TEXT("select resolves through the singleton control-flow evidence provider; SelectFragmentBuilder only performs post-spawn pin normalization.");
		return Boundary;

	case EBlueprintHelperActionSemanticKind::Control:
		Boundary.Mode = Request.Semantic.Query.TrimStartAndEnd().IsEmpty()
			? EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext
			: EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate;
		Boundary.RequiredBuilder = TEXT("ControlFragmentBuilder");
		Boundary.Reason = Request.Semantic.Query.TrimStartAndEnd().IsEmpty()
			? TEXT("control requires Semantic.Query such as branch, return, or sequence.")
			: TEXT("control resolves through the singleton control-flow evidence provider; ControlFragmentBuilder only performs flow composition and pin binding.");
		return Boundary;

	default:
		Boundary.Mode = EBlueprintHelperGenericActionProviderMode::Unsupported;
		Boundary.Reason = TEXT("semantic kind is not owned by the P3 GenericAssetStructControl provider boundary.");
		return Boundary;
	}
}
