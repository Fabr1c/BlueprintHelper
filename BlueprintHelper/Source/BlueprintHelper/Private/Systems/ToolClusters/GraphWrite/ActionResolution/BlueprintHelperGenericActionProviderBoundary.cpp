#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.h"

FBlueprintHelperGenericActionProviderBoundary FBlueprintHelperGenericActionProviderBoundaryService::Classify(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperGenericActionProviderBoundary Boundary;
	const bool bHasTypeName = !Request.Semantic.TypeName.TrimStartAndEnd().IsEmpty();
	const bool bStructFamily = Request.Semantic.SemanticFamily == EBlueprintHelperActionSemanticFamily::Struct
		|| Request.Semantic.SemanticFamily == EBlueprintHelperActionSemanticFamily::TypeStructure;
	const bool bConstructOperation = Request.Semantic.TypeOperation == EBlueprintHelperTypeOperation::Construct;
	const bool bDeconstructOperation = Request.Semantic.TypeOperation == EBlueprintHelperTypeOperation::Deconstruct;
	const FString CreateOperation = Request.Semantic.CreateOperation.TrimStartAndEnd();
	const FString TransformOperation = Request.Semantic.TransformOperation.TrimStartAndEnd();
	const FString ScheduleOperation = Request.Semantic.ScheduleOperation.TrimStartAndEnd();

	switch (Request.Semantic.Kind)
	{
	case EBlueprintHelperActionSemanticKind::Construct:
		if (!bStructFamily || !bConstructOperation)
		{
			Boundary.Mode = EBlueprintHelperGenericActionProviderMode::Unsupported;
			Boundary.Reason = TEXT("construct must be projected as Struct or TypeStructure with TypeOperation=Construct before GenericAssetStructControl can resolve it.");
			return Boundary;
		}
		Boundary.Mode = bHasTypeName
			? EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate
			: EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext;
		Boundary.RequiredBuilder = TEXT("StructTypeStructureActionResolver");
		Boundary.Reason = bHasTypeName
			? TEXT("Struct/TypeStructure construct can resolve type-operation NodeSpawner evidence by TypeName.")
			: TEXT("Struct/TypeStructure construct requires Semantic.TypeName before resolving type-operation candidates.");
		return Boundary;

	case EBlueprintHelperActionSemanticKind::Deconstruct:
		if (!bStructFamily || !bDeconstructOperation)
		{
			Boundary.Mode = EBlueprintHelperGenericActionProviderMode::Unsupported;
			Boundary.Reason = TEXT("deconstruct must be projected as Struct or TypeStructure with TypeOperation=Deconstruct before GenericAssetStructControl can resolve it.");
			return Boundary;
		}
		Boundary.Mode = bHasTypeName
			? EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate
			: EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext;
		Boundary.RequiredBuilder = TEXT("StructTypeStructureActionResolver");
		Boundary.Reason = bHasTypeName
			? TEXT("Struct/TypeStructure deconstruct can resolve type-operation NodeSpawner evidence by TypeName.")
			: TEXT("Struct/TypeStructure deconstruct requires Semantic.TypeName before resolving type-operation candidates.");
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

	case EBlueprintHelperActionSemanticKind::Create:
		Boundary.Mode = CreateOperation.IsEmpty()
			? EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext
			: EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate;
		Boundary.RequiredBuilder = TEXT("GenericCreateActionResolver");
		Boundary.Reason = CreateOperation.IsEmpty()
			? TEXT("create requires Semantic.CreateOperation such as spawn_actor, create_widget, construct_object, make_array, make_map, make_set, or asset_action.")
			: TEXT("create resolves through explicit broad-create operation evidence.");
		return Boundary;

	case EBlueprintHelperActionSemanticKind::Convert:
		Boundary.Mode = TransformOperation.IsEmpty()
			? EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext
			: EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate;
		Boundary.RequiredBuilder = TEXT("GenericTransformScheduleActionResolver");
		Boundary.Reason = TransformOperation.IsEmpty()
			? TEXT("Generic Convert requires Semantic.TransformOperation such as dynamic_cast, class_cast, or type_promotion.")
			: TEXT("Generic Convert resolves through explicit transform_operation evidence.");
		return Boundary;

	case EBlueprintHelperActionSemanticKind::Schedule:
		Boundary.Mode = ScheduleOperation.IsEmpty()
			? EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext
			: EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate;
		Boundary.RequiredBuilder = TEXT("GenericTransformScheduleActionResolver");
		Boundary.Reason = ScheduleOperation.IsEmpty()
			? TEXT("Generic Schedule requires Semantic.ScheduleOperation such as timer_delegate_node or latent_or_async_node.")
			: TEXT("Generic Schedule resolves through explicit schedule_operation evidence.");
		return Boundary;

	default:
		Boundary.Mode = EBlueprintHelperGenericActionProviderMode::Unsupported;
		Boundary.Reason = TEXT("semantic kind is not owned by the P3 GenericAssetStructControl provider boundary.");
		return Boundary;
	}
}
