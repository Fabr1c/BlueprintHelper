#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGraphWriteCapabilityMetrics.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace BlueprintHelperGraphWriteCapabilityMetrics
{
static bool IsGraphWritePhase(const FBlueprintHelperGraphWriteCapabilityCaseResult& Result)
{
	return Result.Phase.Equals(TEXT("GraphWrite"), ESearchCase::IgnoreCase);
}

static bool WasRun(const FBlueprintHelperGraphWriteCapabilityCaseResult& Result)
{
	return Result.ErrorKind != EBlueprintHelperGraphWriteCapabilityErrorKind::NotRun;
}

static bool HasResolverEvidence(const FBlueprintHelperGraphWriteCapabilityCaseResult& Result)
{
	return Result.bHasResolverEvidence ||
		!Result.ResolverStatus.IsEmpty() ||
		!Result.SelectedStableId.IsEmpty();
}

static bool HasSpawnEvidence(const FBlueprintHelperGraphWriteCapabilityCaseResult& Result)
{
	return Result.bHasSpawnEvidence ||
		!Result.SelectedSpawnerClass.IsEmpty() ||
		!Result.SpawnedNodeClass.IsEmpty();
}

static FString JoinOrNone(const TArray<FString>& Values)
{
	return Values.Num() > 0 ? FString::Join(Values, TEXT(",")) : FString(TEXT("none"));
}

static int32 PositiveOrFallback(const int32 Value, const int32 Fallback)
{
	return Value > 0 ? Value : Fallback;
}

static bool IsGenericOpsCapability(const FBlueprintHelperGraphWriteCapabilityCaseResult& Result)
{
	return Result.Capability.Contains(TEXT("GenericOps"), ESearchCase::IgnoreCase)
		|| Result.SemanticKind.Contains(TEXT("control|macro|container|transform|create|schedule|struct|select"), ESearchCase::IgnoreCase);
}

static bool IsEventDelegateCapability(const FBlueprintHelperGraphWriteCapabilityCaseResult& Result)
{
	return Result.Capability.Contains(TEXT("EventDelegate"), ESearchCase::IgnoreCase)
		|| Result.SemanticKind.Contains(TEXT("component_bound_event"), ESearchCase::IgnoreCase)
		|| Result.SemanticKind.Contains(TEXT("delegate"), ESearchCase::IgnoreCase)
		|| Result.ClusterKind.Contains(TEXT("EventDelegateAction"), ESearchCase::IgnoreCase);
}

static TArray<TSharedPtr<FJsonValue>> GenericOpsCommonErrorCodes()
{
	const TCHAR* Codes[] = {
		TEXT("missing_evidence"),
		TEXT("schema_rejection"),
		TEXT("wrong_runtime_owner"),
		TEXT("macro_pin_shape_snapshot_missing"),
		TEXT("macro_spawner_unavailable"),
		TEXT("asset_reference_mismatch"),
		TEXT("latent_not_allowed"),
		TEXT("handler_missing"),
		TEXT("wildcard_residual"),
		TEXT("expose_on_spawn_pin_missing"),
		TEXT("select_result_type_unresolved")
	};

	TArray<TSharedPtr<FJsonValue>> Values;
	for (const TCHAR* Code : Codes)
	{
		Values.Add(MakeShared<FJsonValueString>(Code));
	}
	return Values;
}

static TArray<TSharedPtr<FJsonValue>> EventDelegateCommonErrorCodes()
{
	const TCHAR* Codes[] = {
		TEXT("missing_delegate_property_evidence"),
		TEXT("missing_binding_object_evidence"),
		TEXT("missing_handler_evidence"),
		TEXT("missing_signature_evidence"),
		TEXT("invalid_delegate_operation"),
		TEXT("handler_not_allowed_for_clear"),
		TEXT("handler_required_for_unbind"),
		TEXT("delegate_duplicate_binding"),
		TEXT("duplicate_mutation_policy_blocked"),
		TEXT("assign_side_effect_blocked"),
		TEXT("incompatible_graph_type")
	};

	TArray<TSharedPtr<FJsonValue>> Values;
	for (const TCHAR* Code : Codes)
	{
		Values.Add(MakeShared<FJsonValueString>(Code));
	}
	return Values;
}

static TArray<TSharedPtr<FJsonValue>> EventDelegateReadbackFactKeys()
{
	const TCHAR* Keys[] = {
		TEXT("node_class"),
		TEXT("node_guid"),
		TEXT("spawner_or_factory_kind"),
		TEXT("delegate_owner_class_path"),
		TEXT("delegate_property_path"),
		TEXT("delegate_signature_function_path"),
		TEXT("binding_object_kind"),
		TEXT("handler_function_path"),
		TEXT("component_dynamic_binding_target"),
		TEXT("compile_diagnostic_correlation_key"),
		TEXT("pin.call_arg.<name>.default"),
		TEXT("pin.call_arg.<name>.linked_source_pin")
	};

	TArray<TSharedPtr<FJsonValue>> Values;
	for (const TCHAR* Key : Keys)
	{
		Values.Add(MakeShared<FJsonValueString>(Key));
	}
	return Values;
}
}

double FBlueprintHelperGraphWriteCapabilitySummary::CapabilityCoverageRate() const
{
	return CapabilityItemsPlanned > 0
		? static_cast<double>(CapabilityItemsCovered) / static_cast<double>(CapabilityItemsPlanned)
		: 0.0;
}

double FBlueprintHelperGraphWriteCapabilitySummary::GraphWriteCorrectRate() const
{
	return GraphWriteCasesRun > 0
		? static_cast<double>(GraphWriteCasesCorrect) / static_cast<double>(GraphWriteCasesRun)
		: 0.0;
}

double FBlueprintHelperGraphWriteCapabilitySummary::CallCorrectRate() const
{
	return CallSamplesRun > 0
		? static_cast<double>(CallSamplesCorrect) / static_cast<double>(CallSamplesRun)
		: 0.0;
}

const TCHAR* FBlueprintHelperGraphWriteCapabilityMetrics::ErrorKindToString(const EBlueprintHelperGraphWriteCapabilityErrorKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphWriteCapabilityErrorKind::None:
		return TEXT("none");
	case EBlueprintHelperGraphWriteCapabilityErrorKind::SetupFailure:
		return TEXT("setup_failure");
	case EBlueprintHelperGraphWriteCapabilityErrorKind::MissingRequiredEvidence:
		return TEXT("missing_required_evidence");
	case EBlueprintHelperGraphWriteCapabilityErrorKind::CandidateThresholdExceeded:
		return TEXT("candidate_threshold_exceeded");
	case EBlueprintHelperGraphWriteCapabilityErrorKind::AmbiguousCandidates:
		return TEXT("ambiguous_candidates");
	case EBlueprintHelperGraphWriteCapabilityErrorKind::NotFound:
		return TEXT("not_found");
	case EBlueprintHelperGraphWriteCapabilityErrorKind::UnsupportedIntent:
		return TEXT("unsupported_intent");
	case EBlueprintHelperGraphWriteCapabilityErrorKind::SpawnOrLinkFailure:
		return TEXT("spawn_or_link_failure");
	case EBlueprintHelperGraphWriteCapabilityErrorKind::SilentWrongGraph:
		return TEXT("silent_wrong_graph");
	case EBlueprintHelperGraphWriteCapabilityErrorKind::NotRun:
		return TEXT("not_run");
	default:
		return TEXT("unknown");
	}
}

FBlueprintHelperGraphWriteCapabilityCaseResult FBlueprintHelperGraphWriteCapabilityMetrics::NormalizeReadbackClassification(
	const FBlueprintHelperGraphWriteCapabilityCaseResult& Result)
{
	FBlueprintHelperGraphWriteCapabilityCaseResult Normalized = Result;
	if (Normalized.ErrorKind == EBlueprintHelperGraphWriteCapabilityErrorKind::SilentWrongGraph)
	{
		Normalized.bGraphWriteCorrect = false;
		Normalized.bCallCorrect = false;
		Normalized.CorrectGraphWriteCheckCount = 0;
		Normalized.CorrectCallSampleCount = 0;
		return Normalized;
	}

	const bool bClaimsSuccess = Normalized.ErrorKind == EBlueprintHelperGraphWriteCapabilityErrorKind::None &&
		(Normalized.bGraphWriteCorrect ||
			Normalized.bCallCorrect ||
			Normalized.CorrectGraphWriteCheckCount > 0 ||
			Normalized.CorrectCallSampleCount > 0);
	if (!BlueprintHelperGraphWriteCapabilityMetrics::IsGraphWritePhase(Normalized) || !bClaimsSuccess)
	{
		return Normalized;
	}

	const bool bHasResolverEvidence = BlueprintHelperGraphWriteCapabilityMetrics::HasResolverEvidence(Normalized);
	const bool bHasSpawnEvidence = BlueprintHelperGraphWriteCapabilityMetrics::HasSpawnEvidence(Normalized);
	if (!bHasResolverEvidence || !bHasSpawnEvidence || Normalized.MissingEvidenceFields.Num() > 0)
	{
		Normalized.ErrorKind = EBlueprintHelperGraphWriteCapabilityErrorKind::MissingRequiredEvidence;
		Normalized.bGraphWriteCorrect = false;
		Normalized.bCallCorrect = false;
		Normalized.CorrectGraphWriteCheckCount = 0;
		Normalized.CorrectCallSampleCount = 0;
		return Normalized;
	}

	if (!Normalized.bReadbackComplete)
	{
		Normalized.ErrorKind = EBlueprintHelperGraphWriteCapabilityErrorKind::SilentWrongGraph;
		Normalized.bGraphWriteCorrect = false;
		Normalized.bCallCorrect = false;
		Normalized.CorrectGraphWriteCheckCount = 0;
		Normalized.CorrectCallSampleCount = 0;
	}
	return Normalized;
}

FBlueprintHelperGraphWriteCapabilitySummary FBlueprintHelperGraphWriteCapabilityMetrics::Summarize(
	const TArray<FBlueprintHelperGraphWriteCapabilityCaseResult>& Results)
{
	FBlueprintHelperGraphWriteCapabilitySummary Summary;

	for (const FBlueprintHelperGraphWriteCapabilityCaseResult& Result : Results)
	{
		const FBlueprintHelperGraphWriteCapabilityCaseResult Normalized = NormalizeReadbackClassification(Result);
		const int32 PlannedCapabilityItems =
			BlueprintHelperGraphWriteCapabilityMetrics::PositiveOrFallback(Normalized.CapabilityItemCount, 1);
		Summary.CapabilityItemsPlanned += PlannedCapabilityItems;

		int32 CoveredCapabilityItems = Normalized.CoveredCapabilityItemCount;
		if (CoveredCapabilityItems <= 0 && BlueprintHelperGraphWriteCapabilityMetrics::WasRun(Normalized))
		{
			CoveredCapabilityItems = PlannedCapabilityItems;
		}
		Summary.CapabilityItemsCovered += FMath::Clamp(CoveredCapabilityItems, 0, PlannedCapabilityItems);

		if (Normalized.ErrorKind == EBlueprintHelperGraphWriteCapabilityErrorKind::SilentWrongGraph)
		{
			++Summary.SilentWrongGraphCount;
		}

		if (!BlueprintHelperGraphWriteCapabilityMetrics::IsGraphWritePhase(Normalized) ||
			!BlueprintHelperGraphWriteCapabilityMetrics::WasRun(Normalized))
		{
			continue;
		}

		const bool bHasExplicitGraphWriteCounts =
			Normalized.GraphWriteCheckCount > 0 || Normalized.CorrectGraphWriteCheckCount > 0;
		const int32 GraphWriteChecksRun = bHasExplicitGraphWriteCounts
			? FMath::Max(0, Normalized.GraphWriteCheckCount)
			: 1;
		const int32 GraphWriteChecksCorrect = bHasExplicitGraphWriteCounts
			? FMath::Clamp(Normalized.CorrectGraphWriteCheckCount, 0, GraphWriteChecksRun)
			: (Normalized.bGraphWriteCorrect ? 1 : 0);
		Summary.GraphWriteCasesRun += GraphWriteChecksRun;
		Summary.GraphWriteCasesCorrect += GraphWriteChecksCorrect;

		const bool bHasExplicitCallCounts =
			Normalized.bUseExplicitCallSampleCounts ||
			Normalized.CallSampleCount > 0 ||
			Normalized.CorrectCallSampleCount > 0;
		if (bHasExplicitCallCounts)
		{
			const int32 CallSamplesRun = FMath::Max(0, Normalized.CallSampleCount);
			Summary.CallSamplesRun += CallSamplesRun;
			Summary.CallSamplesCorrect += FMath::Clamp(Normalized.CorrectCallSampleCount, 0, CallSamplesRun);
		}
		else
		{
			++Summary.CallSamplesRun;
			if (Normalized.bCallCorrect)
			{
				++Summary.CallSamplesCorrect;
			}
		}
	}

	return Summary;
}

FString FBlueprintHelperGraphWriteCapabilityMetrics::ToMarkdownRow(
	const FBlueprintHelperGraphWriteCapabilityCaseResult& Result)
{
	const FBlueprintHelperGraphWriteCapabilityCaseResult Normalized = NormalizeReadbackClassification(Result);
	return FString::Printf(
		TEXT("| %s | %s | %s | run_complete | %s | %s | %s | %s | %s | %s | %s | %d | %s | %s | %s | %s | %s | %s |"),
		*Normalized.CaseName,
		*Normalized.Phase,
		*Normalized.Capability,
		ErrorKindToString(Normalized.ErrorKind),
		Normalized.bCallCorrect ? TEXT("yes") : TEXT("no"),
		Normalized.bGraphWriteCorrect ? TEXT("yes") : TEXT("no"),
		Normalized.SemanticKind.IsEmpty() ? TEXT("unknown") : *Normalized.SemanticKind,
		Normalized.ClusterKind.IsEmpty() ? TEXT("unknown") : *Normalized.ClusterKind,
		Normalized.ResolverStatus.IsEmpty() ? TEXT("unknown") : *Normalized.ResolverStatus,
		Normalized.SelectedStableId.IsEmpty() ? TEXT("none") : *Normalized.SelectedStableId,
		Normalized.CandidateCount,
		*BlueprintHelperGraphWriteCapabilityMetrics::JoinOrNone(Normalized.MissingEvidenceFields),
		Normalized.SelectedSpawnerClass.IsEmpty() ? TEXT("none") : *Normalized.SelectedSpawnerClass,
		Normalized.SpawnedNodeClass.IsEmpty() ? TEXT("none") : *Normalized.SpawnedNodeClass,
		Normalized.PinDefaultLinkReadbackSummary.IsEmpty() ? TEXT("not_recorded") : *Normalized.PinDefaultLinkReadbackSummary,
		Normalized.DebugBundlePath.IsEmpty() ? TEXT("not_generated") : *Normalized.DebugBundlePath,
		Normalized.GapUpdate.IsEmpty() ? TEXT("not_needed") : *Normalized.GapUpdate);
}

TSharedRef<FJsonObject> FBlueprintHelperGraphWriteCapabilityMetrics::ToDebugBundleFailureSummary(
	const FBlueprintHelperGraphWriteCapabilityCaseResult& Result)
{
	const FBlueprintHelperGraphWriteCapabilityCaseResult Normalized = NormalizeReadbackClassification(Result);
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("case_name"), Normalized.CaseName);
	Json->SetStringField(TEXT("phase"), Normalized.Phase);
	Json->SetStringField(TEXT("capability"), Normalized.Capability);
	Json->SetStringField(TEXT("error_kind"), ErrorKindToString(Normalized.ErrorKind));
	Json->SetStringField(TEXT("semantic_kind"), Normalized.SemanticKind);
	Json->SetStringField(TEXT("cluster_kind"), Normalized.ClusterKind);
	Json->SetStringField(TEXT("resolver_status"), Normalized.ResolverStatus);
	Json->SetStringField(TEXT("selected_stable_id"), Normalized.SelectedStableId);
	Json->SetStringField(TEXT("selected_spawner_class"), Normalized.SelectedSpawnerClass);
	Json->SetNumberField(TEXT("candidate_count"), Normalized.CandidateCount);

	TArray<TSharedPtr<FJsonValue>> MissingEvidenceValues;
	for (const FString& MissingEvidenceField : Normalized.MissingEvidenceFields)
	{
		MissingEvidenceValues.Add(MakeShared<FJsonValueString>(MissingEvidenceField));
	}
	Json->SetArrayField(TEXT("missing_evidence_fields"), MissingEvidenceValues);
	Json->SetStringField(TEXT("spawned_node_class"), Normalized.SpawnedNodeClass);
	Json->SetStringField(TEXT("pin_default_link_readback_summary"), Normalized.PinDefaultLinkReadbackSummary);
	Json->SetStringField(TEXT("debug_bundle_path"), Normalized.DebugBundlePath);
	Json->SetStringField(TEXT("gap_update"), Normalized.GapUpdate);
	Json->SetBoolField(TEXT("call_correct"), Normalized.bCallCorrect);
	Json->SetBoolField(TEXT("graph_write_correct"), Normalized.bGraphWriteCorrect);
	if (BlueprintHelperGraphWriteCapabilityMetrics::IsGenericOpsCapability(Normalized))
	{
		Json->SetArrayField(
			TEXT("generic_ops_common_error_codes"),
			BlueprintHelperGraphWriteCapabilityMetrics::GenericOpsCommonErrorCodes());
	}
	if (BlueprintHelperGraphWriteCapabilityMetrics::IsEventDelegateCapability(Normalized))
	{
		Json->SetArrayField(
			TEXT("event_delegate_common_error_codes"),
			BlueprintHelperGraphWriteCapabilityMetrics::EventDelegateCommonErrorCodes());
		Json->SetArrayField(
			TEXT("event_delegate_readback_fact_keys"),
			BlueprintHelperGraphWriteCapabilityMetrics::EventDelegateReadbackFactKeys());
	}
	return Json;
}
