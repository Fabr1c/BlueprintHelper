#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.h"

namespace
{
static FString Clean(const FString& Value)
{
	return Value.TrimStartAndEnd();
}

static FString NormalizeOperation(const FString& Operation)
{
	return Clean(Operation).ToLower();
}

static FString EvidenceValue(const FBlueprintHelperActionResolutionRequest& Request, const TCHAR* Key)
{
	if (const FString* Value = Request.ContextEvidence.Find(Key))
	{
		return Clean(*Value);
	}
	if (const FString* Value = Request.Semantic.DefaultValues.Find(Key))
	{
		return Clean(*Value);
	}
	return FString();
}

static FString ControlOperation(const FBlueprintHelperActionResolutionRequest& Request)
{
	return NormalizeOperation(
		!EvidenceValue(Request, TEXT("generic.control.operation")).IsEmpty()
			? EvidenceValue(Request, TEXT("generic.control.operation"))
			: Request.Semantic.Query);
}

static bool IsSingletonControlOperation(const FString& Operation)
{
	return Operation == TEXT("branch")
		|| Operation == TEXT("sequence")
		|| Operation == TEXT("return");
}

static bool IsDedicatedControlFlowOperation(const FString& Operation)
{
	return Operation == TEXT("switch_int")
		|| Operation == TEXT("switch_string")
		|| Operation == TEXT("switch_name")
		|| Operation == TEXT("switch_enum")
		|| Operation == TEXT("multi_gate");
}

static bool IsStandardMacroControlOperation(const FString& Operation)
{
	return Operation == TEXT("do_once")
		|| Operation == TEXT("do_n")
		|| Operation == TEXT("gate")
		|| Operation == TEXT("flip_flop")
		|| Operation == TEXT("for_loop")
		|| Operation == TEXT("for_loop_with_break")
		|| Operation == TEXT("foreach_loop")
		|| Operation == TEXT("foreach_loop_with_break")
		|| Operation == TEXT("while_loop");
}

static FBlueprintHelperGenericActionProviderBoundary MakeNeedsContext(
	const FString& RequiredBuilder,
	const FString& Reason)
{
	FBlueprintHelperGenericActionProviderBoundary Boundary;
	Boundary.Mode = EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext;
	Boundary.RequiredBuilder = RequiredBuilder;
	Boundary.Reason = Reason;
	return Boundary;
}

static FBlueprintHelperGenericActionProviderBoundary MakeDedicated(
	const FString& RequiredBuilder,
	const FString& Reason)
{
	FBlueprintHelperGenericActionProviderBoundary Boundary;
	Boundary.Mode = EBlueprintHelperGenericActionProviderMode::DedicatedFragmentBuilderRequired;
	Boundary.RequiredBuilder = RequiredBuilder;
	Boundary.Reason = Reason;
	return Boundary;
}
}

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
		{
			const FString Operation = ControlOperation(Request);
			if (Operation.IsEmpty())
			{
				Boundary.Mode = EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext;
				Boundary.RequiredBuilder = TEXT("ControlFragmentBuilder");
				Boundary.Reason = TEXT("control requires Semantic.Query or generic.control.operation such as branch, return, sequence, switch_enum, or StandardMacros operation.");
				return Boundary;
			}
			if (IsSingletonControlOperation(Operation))
			{
				Boundary.Mode = EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate;
				Boundary.RequiredBuilder = TEXT("ControlFragmentBuilder");
				Boundary.Reason = TEXT("control resolves through the singleton control-flow evidence provider; ControlFragmentBuilder only performs flow composition and pin binding.");
				return Boundary;
			}
			if (IsDedicatedControlFlowOperation(Operation))
			{
				if (Operation == TEXT("multi_gate") && EvidenceValue(Request, TEXT("generic.control.dynamic_output_count")).IsEmpty())
				{
					return MakeNeedsContext(
						TEXT("ControlFlowFragmentBuilder"),
						TEXT("multi_gate requires generic.control.dynamic_output_count before the dedicated control-flow builder can run."));
				}
				if (Operation.StartsWith(TEXT("switch_")) && EvidenceValue(Request, TEXT("generic.control.case_values")).IsEmpty())
				{
					return MakeNeedsContext(
						TEXT("ControlFlowFragmentBuilder"),
						TEXT("switch control requires generic.control.case_values before the dedicated control-flow builder can run."));
				}
				return MakeDedicated(
					TEXT("ControlFlowFragmentBuilder"),
					FString::Printf(TEXT("control operation '%s' requires the dedicated control-flow fragment builder."), *Operation));
			}
			if (IsStandardMacroControlOperation(Operation))
			{
				if (EvidenceValue(Request, TEXT("generic.macro.graph_path")).IsEmpty())
				{
					return MakeNeedsContext(
						TEXT("MacroControlFragmentBuilder"),
						TEXT("StandardMacros control requires generic.macro.graph_path evidence."));
				}
				if (EvidenceValue(Request, TEXT("generic.macro.pin_shape_snapshot")).IsEmpty())
				{
					return MakeNeedsContext(
						TEXT("MacroControlFragmentBuilder"),
						TEXT("StandardMacros control requires generic.macro.pin_shape_snapshot evidence."));
				}
				return MakeDedicated(
					TEXT("MacroControlFragmentBuilder"),
					FString::Printf(TEXT("StandardMacros control operation '%s' requires the macro control fragment builder."), *Operation));
			}
		}
		Boundary.Mode = Request.Semantic.Query.TrimStartAndEnd().IsEmpty()
			? EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext
			: EBlueprintHelperGenericActionProviderMode::Unsupported;
		Boundary.RequiredBuilder = TEXT("ControlFragmentBuilder");
		Boundary.Reason = Request.Semantic.Query.TrimStartAndEnd().IsEmpty()
			? TEXT("control requires Semantic.Query such as branch, return, or sequence.")
			: TEXT("control operation is not in the GenericOps control vocabulary.");
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
