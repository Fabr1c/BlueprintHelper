#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericOpsEvidence.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenWrappers.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionEvidenceUtils.h"

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
		UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.control.operation")),
		Request.Semantic.Query));
	if (OutEvidence.Operation.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::FailMissing(TEXT("generic.control.operation"), OutErrorCode, OutMessage);
	}

	OutEvidence.CaseValues = UGraphWriteActionEvidenceUtils::SplitList(UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.control.case_values")));
	OutEvidence.DefaultPolicy = UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.control.default_policy"));
	const FString DynamicOutputCount = UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.control.dynamic_output_count"));
	if (!DynamicOutputCount.IsEmpty())
	{
		LexTryParseString(OutEvidence.DynamicOutputCount, *DynamicOutputCount);
	}
	UGraphWriteActionEvidenceUtils::CopyFactsWithPrefix(Request, TEXT("generic.control."), OutEvidence.Facts);
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
		UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.control.operation")),
		Request.Semantic.Query));
	OutEvidence.MacroGraphPath = UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.macro.graph_path"));
	OutEvidence.MacroPinShapeSnapshot = UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.macro.pin_shape_snapshot"));
	OutEvidence.WorldContextPolicy = UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.macro.world_context_policy"));
	if (OutEvidence.Operation.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::FailMissing(TEXT("generic.control.operation"), OutErrorCode, OutMessage);
	}
	if (OutEvidence.MacroGraphPath.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::FailMissing(TEXT("generic.macro.graph_path"), OutErrorCode, OutMessage);
	}
	if (OutEvidence.MacroPinShapeSnapshot.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::FailMissing(TEXT("generic.macro.pin_shape_snapshot"), OutErrorCode, OutMessage);
	}

	UGraphWriteActionEvidenceUtils::CopyFactsWithPrefix(Request, TEXT("generic.control."), OutEvidence.Facts);
	UGraphWriteActionEvidenceUtils::CopyFactsWithPrefix(Request, TEXT("generic.macro."), OutEvidence.Facts);
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
		UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.create.operation")),
		Request.Semantic.CreateOperation,
		Request.Semantic.Query));
	OutEvidence.ClassPath = FirstNonEmpty(
		UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.create.class_path")),
		Request.Semantic.ClassPath);
	OutEvidence.AssetPath = FirstNonEmpty(
		UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.create.asset_path")),
		Request.Semantic.AssetPath);
	OutEvidence.ExposeOnSpawnEvidence = UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.create.expose_on_spawn"));
	if (OutEvidence.Operation.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::FailMissing(TEXT("generic.create.operation"), OutErrorCode, OutMessage);
	}
	if (OutEvidence.ClassPath.IsEmpty() && OutEvidence.AssetPath.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::FailMissing(TEXT("generic.create.class_path"), OutErrorCode, OutMessage);
	}

	UGraphWriteActionEvidenceUtils::CopyFactsWithPrefix(Request, TEXT("generic.create."), OutEvidence.Facts);
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
		UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.transform.operation")),
		Request.Semantic.TransformOperation,
		Request.Semantic.Query));
	OutEvidence.SourcePinType = FirstNonEmpty(
		UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.transform.source_pin_type")),
		Request.Semantic.ExpectedReturnType);
	OutEvidence.TargetPinType = FirstNonEmpty(
		UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.transform.target_pin_type")),
		Request.Semantic.ClassPath);
	OutEvidence.CastPolicy = UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.transform.cast_policy"));
	if (OutEvidence.Operation.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::FailMissing(TEXT("generic.transform.operation"), OutErrorCode, OutMessage);
	}
	if (OutEvidence.SourcePinType.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::FailMissing(TEXT("generic.transform.source_pin_type"), OutErrorCode, OutMessage);
	}
	if (OutEvidence.TargetPinType.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::FailMissing(TEXT("generic.transform.target_pin_type"), OutErrorCode, OutMessage);
	}

	UGraphWriteActionEvidenceUtils::CopyFactsWithPrefix(Request, TEXT("generic.transform."), OutEvidence.Facts);
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
		UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.schedule.operation")),
		Request.Semantic.ScheduleOperation,
		Request.Semantic.Query));
	OutEvidence.GraphLatentAllowed = FirstNonEmpty(
		UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.schedule.graph_latent_allowed")),
		UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("graph_latent_allowed")));
	OutEvidence.HandlerEvidenceId = FirstNonEmpty(
		UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.schedule.handler_evidence_id")),
		UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("signature_evidence_id")));
	if (OutEvidence.Operation.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::FailMissing(TEXT("generic.schedule.operation"), OutErrorCode, OutMessage);
	}
	if (UGraphWriteActionEvidenceUtils::IsLatentScheduleOperation(OutEvidence.Operation)
		&& !OutEvidence.GraphLatentAllowed.Equals(TEXT("true"), ESearchCase::IgnoreCase))
	{
		OutErrorCode = TEXT("latent_not_allowed");
		OutMessage = TEXT("Generic schedule latent operation requires graph_latent_allowed=true evidence.");
		return false;
	}
	if (OutEvidence.Operation.Equals(TEXT("timer_delegate_node"), ESearchCase::IgnoreCase)
		&& OutEvidence.HandlerEvidenceId.IsEmpty()
		&& UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("handler_name")).IsEmpty())
	{
		OutErrorCode = TEXT("handler_missing");
		OutMessage = TEXT("Generic schedule timer delegate node requires handler/signature evidence.");
		return false;
	}

	UGraphWriteActionEvidenceUtils::CopyFactsWithPrefix(Request, TEXT("generic.schedule."), OutEvidence.Facts);
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
		UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.struct.struct_path")),
		Request.Semantic.StructPath,
		Request.Semantic.TypeName);
	OutEvidence.SelectedFieldPaths = UGraphWriteActionEvidenceUtils::SplitList(UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.struct.selected_field_paths")));
	OutEvidence.OptionalPinPolicy = UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.struct.optional_pin_policy"));
	OutEvidence.ResultTypeProof = UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.select.result_type_proof"));
	const FString Operation = NormalizeOperation(FirstNonEmpty(
		UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.struct.operation")),
		UGraphWriteActionEvidenceUtils::GetEvidenceValue(Request, TEXT("generic.select.operation")),
		Request.Semantic.Query));
	if (!Operation.Equals(TEXT("select"), ESearchCase::IgnoreCase)
		&& OutEvidence.StructPath.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::FailMissing(TEXT("generic.struct.struct_path"), OutErrorCode, OutMessage);
	}
	if (Operation.Equals(TEXT("set_fields_in_struct"), ESearchCase::IgnoreCase)
		&& OutEvidence.SelectedFieldPaths.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::FailMissing(TEXT("generic.struct.selected_field_paths"), OutErrorCode, OutMessage);
	}
	if (Operation.Equals(TEXT("select"), ESearchCase::IgnoreCase)
		&& OutEvidence.ResultTypeProof.IsEmpty())
	{
		OutErrorCode = TEXT("select_result_type_unresolved");
		OutMessage = TEXT("GenericOps select requires result type proof evidence.");
		return false;
	}

	UGraphWriteActionEvidenceUtils::CopyFactsWithPrefix(Request, TEXT("generic.struct."), OutEvidence.Facts);
	UGraphWriteActionEvidenceUtils::CopyFactsWithPrefix(Request, TEXT("generic.select."), OutEvidence.Facts);
	return true;
}
