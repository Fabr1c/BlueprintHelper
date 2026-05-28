#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericOpsEvidence.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenWrappers.h"

namespace
{
static FString GetEvidenceValue(const FBlueprintHelperActionResolutionRequest& Request, const TCHAR* Key)
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

static void CopyFactsWithPrefix(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& Prefix,
	TMap<FString, FString>& OutFacts)
{
	for (const TPair<FString, FString>& Pair : Request.ContextEvidence)
	{
		const FString Value = Clean(Pair.Value);
		if (Pair.Key.StartsWith(Prefix, ESearchCase::IgnoreCase) && !Value.IsEmpty())
		{
			OutFacts.FindOrAdd(Pair.Key) = Value;
		}
	}
	for (const TPair<FString, FString>& Pair : Request.Semantic.DefaultValues)
	{
		const FString Value = Clean(Pair.Value);
		if (Pair.Key.StartsWith(Prefix, ESearchCase::IgnoreCase) && !Value.IsEmpty())
		{
			OutFacts.FindOrAdd(Pair.Key) = Value;
		}
	}
}

static TArray<FString> SplitList(const FString& Value)
{
	TArray<FString> Parts;
	Value.ParseIntoArray(Parts, TEXT(","), true);
	for (FString& Part : Parts)
	{
		Part = Clean(Part);
	}
	Parts.RemoveAll([](const FString& Part) { return Part.IsEmpty(); });
	return Parts;
}

static bool FailMissing(const TCHAR* Key, FString& OutErrorCode, FString& OutMessage)
{
	OutErrorCode = FString::Printf(TEXT("missing_evidence.%s"), Key);
	OutMessage = FString::Printf(TEXT("GenericOps evidence requires key %s."), Key);
	return false;
}

static bool IsLatentScheduleOperation(const FString& Operation)
{
	return Operation.Equals(TEXT("latent_or_async_node"), ESearchCase::IgnoreCase)
		|| Operation.Equals(TEXT("delay"), ESearchCase::IgnoreCase)
		|| Operation.Equals(TEXT("retriggerable_delay"), ESearchCase::IgnoreCase)
		|| Operation.Equals(TEXT("delay_until_next_tick"), ESearchCase::IgnoreCase)
		|| Operation.Equals(TEXT("generic_latent_function_call"), ESearchCase::IgnoreCase);
}
}

bool FBlueprintHelperControlOperationEvidenceReader::Read(
	const FBlueprintHelperActionResolutionRequest& Request,
	FBlueprintHelperGenericOpsControlOperationEvidence& OutEvidence,
	FString& OutErrorCode,
	FString& OutMessage)
{
	OutEvidence = FBlueprintHelperGenericOpsControlOperationEvidence();
	OutErrorCode.Reset();
	OutMessage.Reset();

	OutEvidence.Operation = NormalizeOperation(FirstNonEmpty(
		GetEvidenceValue(Request, TEXT("generic.control.operation")),
		Request.Semantic.Query));
	if (OutEvidence.Operation.IsEmpty())
	{
		return FailMissing(TEXT("generic.control.operation"), OutErrorCode, OutMessage);
	}

	OutEvidence.CaseValues = SplitList(GetEvidenceValue(Request, TEXT("generic.control.case_values")));
	OutEvidence.DefaultPolicy = GetEvidenceValue(Request, TEXT("generic.control.default_policy"));
	const FString DynamicOutputCount = GetEvidenceValue(Request, TEXT("generic.control.dynamic_output_count"));
	if (!DynamicOutputCount.IsEmpty())
	{
		LexTryParseString(OutEvidence.DynamicOutputCount, *DynamicOutputCount);
	}
	CopyFactsWithPrefix(Request, TEXT("generic.control."), OutEvidence.Facts);
	OutEvidence.Facts.FindOrAdd(TEXT("generic.control.operation")) = OutEvidence.Operation;
	return true;
}

bool FBlueprintHelperMacroInstanceEvidenceReader::Read(
	const FBlueprintHelperActionResolutionRequest& Request,
	FBlueprintHelperGenericOpsMacroInstanceEvidence& OutEvidence,
	FString& OutErrorCode,
	FString& OutMessage)
{
	OutEvidence = FBlueprintHelperGenericOpsMacroInstanceEvidence();
	OutErrorCode.Reset();
	OutMessage.Reset();

	OutEvidence.Operation = NormalizeOperation(FirstNonEmpty(
		GetEvidenceValue(Request, TEXT("generic.control.operation")),
		Request.Semantic.Query));
	OutEvidence.MacroGraphPath = GetEvidenceValue(Request, TEXT("generic.macro.graph_path"));
	OutEvidence.MacroPinShapeSnapshot = GetEvidenceValue(Request, TEXT("generic.macro.pin_shape_snapshot"));
	OutEvidence.WorldContextPolicy = GetEvidenceValue(Request, TEXT("generic.macro.world_context_policy"));
	if (OutEvidence.Operation.IsEmpty())
	{
		return FailMissing(TEXT("generic.control.operation"), OutErrorCode, OutMessage);
	}
	if (OutEvidence.MacroGraphPath.IsEmpty())
	{
		return FailMissing(TEXT("generic.macro.graph_path"), OutErrorCode, OutMessage);
	}
	if (OutEvidence.MacroPinShapeSnapshot.IsEmpty())
	{
		return FailMissing(TEXT("generic.macro.pin_shape_snapshot"), OutErrorCode, OutMessage);
	}

	CopyFactsWithPrefix(Request, TEXT("generic.control."), OutEvidence.Facts);
	CopyFactsWithPrefix(Request, TEXT("generic.macro."), OutEvidence.Facts);
	OutEvidence.Facts.FindOrAdd(TEXT("generic.control.operation")) = OutEvidence.Operation;
	return true;
}

bool FBlueprintHelperGenericCreateEvidenceReader::Read(
	const FBlueprintHelperActionResolutionRequest& Request,
	FBlueprintHelperGenericOpsCreateEvidence& OutEvidence,
	FString& OutErrorCode,
	FString& OutMessage)
{
	OutEvidence = FBlueprintHelperGenericOpsCreateEvidence();
	OutErrorCode.Reset();
	OutMessage.Reset();

	OutEvidence.Operation = NormalizeOperation(FirstNonEmpty(
		GetEvidenceValue(Request, TEXT("generic.create.operation")),
		Request.Semantic.CreateOperation,
		Request.Semantic.Query));
	OutEvidence.ClassPath = FirstNonEmpty(
		GetEvidenceValue(Request, TEXT("generic.create.class_path")),
		Request.Semantic.ClassPath);
	OutEvidence.AssetPath = FirstNonEmpty(
		GetEvidenceValue(Request, TEXT("generic.create.asset_path")),
		Request.Semantic.AssetPath);
	OutEvidence.ExposeOnSpawnEvidence = GetEvidenceValue(Request, TEXT("generic.create.expose_on_spawn"));
	if (OutEvidence.Operation.IsEmpty())
	{
		return FailMissing(TEXT("generic.create.operation"), OutErrorCode, OutMessage);
	}
	if (OutEvidence.ClassPath.IsEmpty() && OutEvidence.AssetPath.IsEmpty())
	{
		return FailMissing(TEXT("generic.create.class_path"), OutErrorCode, OutMessage);
	}

	CopyFactsWithPrefix(Request, TEXT("generic.create."), OutEvidence.Facts);
	OutEvidence.Facts.FindOrAdd(TEXT("generic.create.operation")) = OutEvidence.Operation;
	return true;
}

bool FBlueprintHelperGenericTransformEvidenceReader::Read(
	const FBlueprintHelperActionResolutionRequest& Request,
	FBlueprintHelperGenericOpsTransformEvidence& OutEvidence,
	FString& OutErrorCode,
	FString& OutMessage)
{
	OutEvidence = FBlueprintHelperGenericOpsTransformEvidence();
	OutErrorCode.Reset();
	OutMessage.Reset();

	OutEvidence.Operation = NormalizeOperation(FirstNonEmpty(
		GetEvidenceValue(Request, TEXT("generic.transform.operation")),
		Request.Semantic.TransformOperation,
		Request.Semantic.Query));
	OutEvidence.SourcePinType = FirstNonEmpty(
		GetEvidenceValue(Request, TEXT("generic.transform.source_pin_type")),
		Request.Semantic.ExpectedReturnType);
	OutEvidence.TargetPinType = FirstNonEmpty(
		GetEvidenceValue(Request, TEXT("generic.transform.target_pin_type")),
		Request.Semantic.ClassPath);
	OutEvidence.CastPolicy = GetEvidenceValue(Request, TEXT("generic.transform.cast_policy"));
	if (OutEvidence.Operation.IsEmpty())
	{
		return FailMissing(TEXT("generic.transform.operation"), OutErrorCode, OutMessage);
	}
	if (OutEvidence.SourcePinType.IsEmpty())
	{
		return FailMissing(TEXT("generic.transform.source_pin_type"), OutErrorCode, OutMessage);
	}
	if (OutEvidence.TargetPinType.IsEmpty())
	{
		return FailMissing(TEXT("generic.transform.target_pin_type"), OutErrorCode, OutMessage);
	}

	CopyFactsWithPrefix(Request, TEXT("generic.transform."), OutEvidence.Facts);
	OutEvidence.Facts.FindOrAdd(TEXT("generic.transform.operation")) = OutEvidence.Operation;
	return true;
}

bool FBlueprintHelperGenericScheduleEvidenceReader::Read(
	const FBlueprintHelperActionResolutionRequest& Request,
	FBlueprintHelperGenericOpsScheduleEvidence& OutEvidence,
	FString& OutErrorCode,
	FString& OutMessage)
{
	OutEvidence = FBlueprintHelperGenericOpsScheduleEvidence();
	OutErrorCode.Reset();
	OutMessage.Reset();

	OutEvidence.Operation = NormalizeOperation(FirstNonEmpty(
		GetEvidenceValue(Request, TEXT("generic.schedule.operation")),
		Request.Semantic.ScheduleOperation,
		Request.Semantic.Query));
	OutEvidence.GraphLatentAllowed = FirstNonEmpty(
		GetEvidenceValue(Request, TEXT("generic.schedule.graph_latent_allowed")),
		GetEvidenceValue(Request, TEXT("graph_latent_allowed")));
	OutEvidence.HandlerEvidenceId = FirstNonEmpty(
		GetEvidenceValue(Request, TEXT("generic.schedule.handler_evidence_id")),
		GetEvidenceValue(Request, TEXT("signature_evidence_id")));
	if (OutEvidence.Operation.IsEmpty())
	{
		return FailMissing(TEXT("generic.schedule.operation"), OutErrorCode, OutMessage);
	}
	if (IsLatentScheduleOperation(OutEvidence.Operation)
		&& !OutEvidence.GraphLatentAllowed.Equals(TEXT("true"), ESearchCase::IgnoreCase))
	{
		OutErrorCode = TEXT("latent_not_allowed");
		OutMessage = TEXT("Generic schedule latent operation requires graph_latent_allowed=true evidence.");
		return false;
	}
	if (OutEvidence.Operation.Equals(TEXT("timer_delegate_node"), ESearchCase::IgnoreCase)
		&& OutEvidence.HandlerEvidenceId.IsEmpty()
		&& GetEvidenceValue(Request, TEXT("handler_name")).IsEmpty())
	{
		OutErrorCode = TEXT("handler_missing");
		OutMessage = TEXT("Generic schedule timer delegate node requires handler/signature evidence.");
		return false;
	}

	CopyFactsWithPrefix(Request, TEXT("generic.schedule."), OutEvidence.Facts);
	OutEvidence.Facts.FindOrAdd(TEXT("generic.schedule.operation")) = OutEvidence.Operation;
	return true;
}

bool FBlueprintHelperStructFieldPolicyEvidenceReader::Read(
	const FBlueprintHelperActionResolutionRequest& Request,
	FBlueprintHelperGenericOpsStructFieldPolicyEvidence& OutEvidence,
	FString& OutErrorCode,
	FString& OutMessage)
{
	OutEvidence = FBlueprintHelperGenericOpsStructFieldPolicyEvidence();
	OutErrorCode.Reset();
	OutMessage.Reset();

	OutEvidence.StructPath = FirstNonEmpty(
		GetEvidenceValue(Request, TEXT("generic.struct.struct_path")),
		Request.Semantic.StructPath,
		Request.Semantic.TypeName);
	OutEvidence.SelectedFieldPaths = SplitList(GetEvidenceValue(Request, TEXT("generic.struct.selected_field_paths")));
	OutEvidence.OptionalPinPolicy = GetEvidenceValue(Request, TEXT("generic.struct.optional_pin_policy"));
	OutEvidence.ResultTypeProof = GetEvidenceValue(Request, TEXT("generic.select.result_type_proof"));
	const FString Operation = NormalizeOperation(FirstNonEmpty(
		GetEvidenceValue(Request, TEXT("generic.struct.operation")),
		GetEvidenceValue(Request, TEXT("generic.select.operation")),
		Request.Semantic.Query));
	if (!Operation.Equals(TEXT("select"), ESearchCase::IgnoreCase)
		&& OutEvidence.StructPath.IsEmpty())
	{
		return FailMissing(TEXT("generic.struct.struct_path"), OutErrorCode, OutMessage);
	}
	if (Operation.Equals(TEXT("set_fields_in_struct"), ESearchCase::IgnoreCase)
		&& OutEvidence.SelectedFieldPaths.IsEmpty())
	{
		return FailMissing(TEXT("generic.struct.selected_field_paths"), OutErrorCode, OutMessage);
	}
	if (Operation.Equals(TEXT("select"), ESearchCase::IgnoreCase)
		&& OutEvidence.ResultTypeProof.IsEmpty())
	{
		OutErrorCode = TEXT("select_result_type_unresolved");
		OutMessage = TEXT("GenericOps select requires result type proof evidence.");
		return false;
	}

	CopyFactsWithPrefix(Request, TEXT("generic.struct."), OutEvidence.Facts);
	CopyFactsWithPrefix(Request, TEXT("generic.select."), OutEvidence.Facts);
	return true;
}
