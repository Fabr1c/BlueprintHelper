#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteReviewEvidenceBuilder.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/DateTime.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteOwnershipValidator.h"

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::TrimmedPayloadString(
	const TSharedPtr<FJsonObject>& Payload,
	const TCHAR* FieldName)
{
	FString Value;
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(FieldName, Value);
		Value.TrimStartAndEndInline();
	}
	return Value;
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::ReadTargetStringField(
	const TSharedPtr<FJsonObject>& Payload,
	const TCHAR* PrimaryFieldName,
	const TCHAR* AlternateFieldName)
{
	FString Value = TrimmedPayloadString(Payload, PrimaryFieldName);
	if (Value.IsEmpty() && AlternateFieldName)
	{
		Value = TrimmedPayloadString(Payload, AlternateFieldName);
	}
	if (!Value.IsEmpty() || !Payload.IsValid())
	{
		return Value;
	}

	const TSharedPtr<FJsonObject>* TargetObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("target"), TargetObject) && TargetObject && TargetObject->IsValid())
	{
		Value = TrimmedPayloadString(*TargetObject, PrimaryFieldName);
		if (Value.IsEmpty() && AlternateFieldName)
		{
			Value = TrimmedPayloadString(*TargetObject, AlternateFieldName);
		}
	}
	return Value;
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::ReadAssetPath(
	const TSharedPtr<FJsonObject>& Payload)
{
	return ReadTargetStringField(Payload, TEXT("asset_path"), TEXT("blueprint_path"));
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::ReadGraphName(
	const TSharedPtr<FJsonObject>& Payload)
{
	return ReadTargetStringField(Payload, TEXT("graph_name"), TEXT("graph"));
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::SerializePayloadForAnchor(
	const TSharedPtr<FJsonObject>& Payload)
{
	if (!Payload.IsValid())
	{
		return FString();
	}

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);
	return Output;
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::TrimmedObjectStringField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName)
{
	FString Value;
	if (Object.IsValid())
	{
		Object->TryGetStringField(FieldName, Value);
		Value.TrimStartAndEndInline();
	}
	return Value;
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::ReadSignatureEvidenceId(
	const TSharedPtr<FJsonObject>& Payload)
{
	FString SignatureEvidenceId = TrimmedObjectStringField(Payload, TEXT("signature_evidence_id"));
	if (!SignatureEvidenceId.IsEmpty() || !Payload.IsValid())
	{
		return SignatureEvidenceId;
	}

	const TSharedPtr<FJsonObject>* LogicSpec = nullptr;
	if (Payload->TryGetObjectField(TEXT("logic_spec"), LogicSpec) && LogicSpec && LogicSpec->IsValid())
	{
		SignatureEvidenceId = TrimmedObjectStringField(*LogicSpec, TEXT("signature_evidence_id"));
		if (!SignatureEvidenceId.IsEmpty())
		{
			return SignatureEvidenceId;
		}

		const TSharedPtr<FJsonObject>* Entry = nullptr;
		if ((*LogicSpec)->TryGetObjectField(TEXT("entry"), Entry) && Entry && Entry->IsValid())
		{
			SignatureEvidenceId = TrimmedObjectStringField(*Entry, TEXT("signature_evidence_id"));
			if (!SignatureEvidenceId.IsEmpty())
			{
				return SignatureEvidenceId;
			}
		}
	}

	const TSharedPtr<FJsonObject>* Entry = nullptr;
	if (Payload->TryGetObjectField(TEXT("entry"), Entry) && Entry && Entry->IsValid())
	{
		SignatureEvidenceId = TrimmedObjectStringField(*Entry, TEXT("signature_evidence_id"));
	}
	return SignatureEvidenceId;
}

void FBlueprintHelperGraphWriteReviewEvidenceBuilder::ApplySignatureDependencyMetadata(
	FBlueprintHelperReviewAtomicTarget& Target,
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FString& SignatureEvidenceId)
{
	if (SignatureEvidenceId.IsEmpty() || LoweredStep.DependsOn.Num() == 0)
	{
		return;
	}

	Target.SignatureRole = TEXT("dependency");
	Target.SignatureEvidenceId = SignatureEvidenceId;
	Target.DependencyOwnerStepId = LoweredStep.StepId;
	Target.DependentStepId = LoweredStep.DependsOn[0];
}

void FBlueprintHelperGraphWriteReviewEvidenceBuilder::AppendStringArrayField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	TArray<FString>& OutValues)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values) || !Values)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (!Value.IsValid())
		{
			continue;
		}

		FString Text = Value->AsString();
		Text.TrimStartAndEndInline();
		if (!Text.IsEmpty())
		{
			OutValues.AddUnique(Text);
		}
	}
}

TArray<FString> FBlueprintHelperGraphWriteReviewEvidenceBuilder::ReadGraphBlockRefs(
	const FBlueprintHelperToolResultBase& StepResult)
{
	TArray<FString> BlockRefs;
	if (!StepResult.Data.IsValid())
	{
		return BlockRefs;
	}

	const TSharedPtr<FJsonObject>* AppendResult = nullptr;
	if (StepResult.Data->TryGetObjectField(TEXT("append_result"), AppendResult) && AppendResult && AppendResult->IsValid())
	{
		AppendStringArrayField(*AppendResult, TEXT("block_refs"), BlockRefs);
	}
	AppendStringArrayField(StepResult.Data, TEXT("block_refs"), BlockRefs);

	return BlockRefs;
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::MakeGraphBlockTargetKey(
	const FString& GraphName,
	const FString& BlockRef)
{
	const FString FullBlockId = BlockRef.StartsWith(GraphName + TEXT("_"))
		? BlockRef
		: FString::Printf(TEXT("%s_%s"), *GraphName, *BlockRef);
	return FString::Printf(TEXT("graph:%s:block:%s"), *GraphName, *FullBlockId);
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::ReadStringField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName)
{
	FString Value;
	if (Object.IsValid())
	{
		Object->TryGetStringField(FieldName, Value);
		Value.TrimStartAndEndInline();
	}
	return Value;
}

void FBlueprintHelperGraphWriteReviewEvidenceBuilder::NormalizeGraphWriteDiagnostic(
	FBlueprintHelperDiagnosticItem& Item,
	const FString& DefaultGraphName)
{
	if (Item.GraphName.IsEmpty())
	{
		Item.GraphName = DefaultGraphName;
	}
	if (Item.NodeGuid.IsEmpty() && !Item.NodeId.IsEmpty())
	{
		Item.NodeGuid = Item.NodeId;
	}
	if (Item.NodeId.IsEmpty() && !Item.NodeGuid.IsEmpty())
	{
		Item.NodeId = Item.NodeGuid;
	}
	if (Item.NodeName.IsEmpty())
	{
		Item.NodeName = Item.NodeTitle;
	}
	if (Item.CompileDiagnosticCorrelationKey.IsEmpty())
	{
		Item.CompileDiagnosticCorrelationKey = BlueprintHelperDiagnosticCorrelationKey(Item);
	}
}

void FBlueprintHelperGraphWriteReviewEvidenceBuilder::AppendDiagnosticsFromArrayField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& DefaultGraphName,
	TArray<FBlueprintHelperDiagnosticItem>& OutDiagnostics)
{
	TArray<FBlueprintHelperDiagnosticItem> Items;
	BlueprintHelperReadDiagnosticArrayField(Object, FieldName, Items);
	for (FBlueprintHelperDiagnosticItem& Item : Items)
	{
		NormalizeGraphWriteDiagnostic(Item, DefaultGraphName);
		OutDiagnostics.Add(MoveTemp(Item));
	}
}

void FBlueprintHelperGraphWriteReviewEvidenceBuilder::AppendReadbackCorrelationFromObject(
	const TSharedPtr<FJsonObject>& Object,
	const FString& DefaultGraphName,
	TArray<FBlueprintHelperDiagnosticItem>& OutDiagnostics)
{
	if (!Object.IsValid())
	{
		return;
	}

	FBlueprintHelperDiagnosticItem Item;
	Item.Severity = EBlueprintHelperDiagnosticSeverity::Info;
	Item.Code = ReadStringField(Object, TEXT("code"));
	if (Item.Code.IsEmpty())
	{
		Item.Code = TEXT("graphwrite_readback_correlation");
	}
	Item.Message = ReadStringField(Object, TEXT("message"));
	Item.GraphName = ReadStringField(Object, TEXT("graph_name"));
	Item.NodeGuid = ReadStringField(Object, TEXT("node_guid"));
	Item.NodeId = ReadStringField(Object, TEXT("node_id"));
	Item.NodeTitle = ReadStringField(Object, TEXT("node_title"));
	Item.NodeName = ReadStringField(Object, TEXT("node_name"));
	Item.NodeClass = ReadStringField(Object, TEXT("node_class"));
	Item.BlockRef = ReadStringField(Object, TEXT("block_ref"));
	Item.TargetKey = ReadStringField(Object, TEXT("target_key"));
	Item.CompileDiagnosticCorrelationKey = ReadStringField(Object, TEXT("compile_diagnostic_correlation_key"));
	Item.ErrorType = ReadStringField(Object, TEXT("error_type"));
	NormalizeGraphWriteDiagnostic(Item, DefaultGraphName);

	if (!Item.CompileDiagnosticCorrelationKey.IsEmpty() ||
		!Item.NodeGuid.IsEmpty() ||
		!Item.TargetKey.IsEmpty() ||
		!Item.BlockRef.IsEmpty())
	{
		OutDiagnostics.Add(MoveTemp(Item));
	}
}

void FBlueprintHelperGraphWriteReviewEvidenceBuilder::AppendReadbackCorrelationsFromArrayField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& DefaultGraphName,
	TArray<FBlueprintHelperDiagnosticItem>& OutDiagnostics)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values) || !Values)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		AppendReadbackCorrelationFromObject(Value.IsValid() ? Value->AsObject() : nullptr, DefaultGraphName, OutDiagnostics);
	}
}

TArray<FBlueprintHelperDiagnosticItem> FBlueprintHelperGraphWriteReviewEvidenceBuilder::ReadReviewDiagnostics(
	const FBlueprintHelperToolResultBase& StepResult,
	const FString& DefaultGraphName)
{
	TArray<FBlueprintHelperDiagnosticItem> Diagnostics;
	if (!StepResult.Data.IsValid())
	{
		return Diagnostics;
	}

	AppendDiagnosticsFromArrayField(StepResult.Data, TEXT("diagnostics"), DefaultGraphName, Diagnostics);
	AppendDiagnosticsFromArrayField(StepResult.Data, TEXT("compiler_results"), DefaultGraphName, Diagnostics);

	const TSharedPtr<FJsonObject>* CompileResult = nullptr;
	if (StepResult.Data->TryGetObjectField(TEXT("compile_result"), CompileResult) && CompileResult && CompileResult->IsValid())
	{
		AppendDiagnosticsFromArrayField(*CompileResult, TEXT("compiler_results"), DefaultGraphName, Diagnostics);
		AppendDiagnosticsFromArrayField(*CompileResult, TEXT("diagnostics"), DefaultGraphName, Diagnostics);
	}

	AppendReadbackCorrelationsFromArrayField(StepResult.Data, TEXT("generated_nodes"), DefaultGraphName, Diagnostics);
	AppendReadbackCorrelationsFromArrayField(StepResult.Data, TEXT("readback_nodes"), DefaultGraphName, Diagnostics);
	AppendReadbackCorrelationsFromArrayField(StepResult.Data, TEXT("readback_entries"), DefaultGraphName, Diagnostics);

	const TSharedPtr<FJsonObject>* Readback = nullptr;
	if (StepResult.Data->TryGetObjectField(TEXT("readback"), Readback) && Readback && Readback->IsValid())
	{
		AppendDiagnosticsFromArrayField(*Readback, TEXT("diagnostics"), DefaultGraphName, Diagnostics);
		AppendReadbackCorrelationsFromArrayField(*Readback, TEXT("generated_nodes"), DefaultGraphName, Diagnostics);
		AppendReadbackCorrelationsFromArrayField(*Readback, TEXT("nodes"), DefaultGraphName, Diagnostics);
		AppendReadbackCorrelationFromObject(*Readback, DefaultGraphName, Diagnostics);
	}

	return Diagnostics;
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::TargetKeyForDiagnostic(
	const FBlueprintHelperDiagnosticItem& Diagnostic,
	const FString& DefaultGraphName)
{
	if (!Diagnostic.TargetKey.IsEmpty())
	{
		return Diagnostic.TargetKey;
	}
	const FString GraphName = Diagnostic.GraphName.IsEmpty() ? DefaultGraphName : Diagnostic.GraphName;
	if (!GraphName.IsEmpty() && !Diagnostic.BlockRef.IsEmpty())
	{
		return MakeGraphBlockTargetKey(GraphName, Diagnostic.BlockRef);
	}
	return FString();
}

bool FBlueprintHelperGraphWriteReviewEvidenceBuilder::DiagnosticMatchesTarget(
	const FBlueprintHelperDiagnosticItem& Diagnostic,
	const FBlueprintHelperReviewAtomicTarget& Target)
{
	const FString DiagnosticTargetKey = TargetKeyForDiagnostic(Diagnostic, Target.GraphName);
	if (!DiagnosticTargetKey.IsEmpty())
	{
		return DiagnosticTargetKey == Target.TargetKey;
	}

	const bool bDiagnosticHasNodeIdentity =
		!Diagnostic.NodeGuid.IsEmpty() ||
		!Diagnostic.CompileDiagnosticCorrelationKey.IsEmpty();
	if (bDiagnosticHasNodeIdentity)
	{
		return !Diagnostic.NodeGuid.IsEmpty() &&
			!Target.NodeGuid.IsEmpty() &&
			Diagnostic.NodeGuid.Equals(Target.NodeGuid, ESearchCase::IgnoreCase) &&
			(Diagnostic.GraphName.IsEmpty() || Diagnostic.GraphName == Target.GraphName);
	}

	return Diagnostic.GraphName.IsEmpty() || Diagnostic.GraphName == Target.GraphName;
}

void FBlueprintHelperGraphWriteReviewEvidenceBuilder::AttachDiagnosticsToEvidence(
	const TArray<FBlueprintHelperDiagnosticItem>& Diagnostics,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	OutEvidence.Diagnostics = Diagnostics;
	for (FBlueprintHelperReviewAtomicTarget& Target : OutEvidence.AtomicTargets)
	{
		for (FBlueprintHelperDiagnosticItem Diagnostic : Diagnostics)
		{
			NormalizeGraphWriteDiagnostic(Diagnostic, Target.GraphName);
			if (Diagnostic.TargetKey.IsEmpty())
			{
				Diagnostic.TargetKey = TargetKeyForDiagnostic(Diagnostic, Target.GraphName);
			}
			if (DiagnosticMatchesTarget(Diagnostic, Target))
			{
				Target.Diagnostics.Add(MoveTemp(Diagnostic));
			}
		}
	}
}

FString FBlueprintHelperGraphWriteReviewEvidenceBuilder::BuildScopeIdentity(
	const FString& AssetPath,
	const FString& GraphName,
	const FString& TargetKey)
{
	if (AssetPath.IsEmpty() || GraphName.IsEmpty() || TargetKey.IsEmpty())
	{
		return FString();
	}
	return FString::Printf(TEXT("%s|%s|%s"), *AssetPath, *GraphName, *TargetKey);
}

bool FBlueprintHelperGraphWriteReviewEvidenceBuilder::Build(
	const FBlueprintHelperGraphWriteReviewEvidenceBuildInput& Input,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	if (!Input.StepResult.bOk || !Input.LoweredStep.Payload.IsValid())
	{
		return false;
	}

	FString AssetPath = Input.BoundaryModel.TargetAssetPath;
	if (AssetPath.IsEmpty())
	{
		AssetPath = ReadAssetPath(Input.LoweredStep.Payload);
	}
	FString GraphName = Input.BoundaryModel.GraphName;
	if (GraphName.IsEmpty())
	{
		GraphName = ReadGraphName(Input.LoweredStep.Payload);
	}
	if (AssetPath.IsEmpty() || GraphName.IsEmpty())
	{
		return false;
	}

	const FString OperationKind = Input.LoweredStep.AdapterOperation.IsEmpty()
		? Input.LoweredStep.RuntimeOperation
		: Input.LoweredStep.AdapterOperation;

	OutEvidence = FBlueprintHelperWriteReviewEvidence();
	OutEvidence.ArchiveSessionId = Input.ArchiveSessionId;
	OutEvidence.TaskRunId = Input.TaskRunId;
	OutEvidence.EvidenceId = FString::Printf(TEXT("task_step_%s_%d"), *Input.TaskRunId, Input.StepIndex);
	OutEvidence.CreatedAt = FDateTime::UtcNow().ToIso8601();
	OutEvidence.AssetPath = AssetPath;
	OutEvidence.OperationKind = OperationKind;
	OutEvidence.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	OutEvidence.DisplayLabel = OperationKind;
	OutEvidence.TaskStepIndex = Input.StepIndex;

	const TArray<FString> BlockRefs = ReadGraphBlockRefs(Input.StepResult);
	TArray<FString> TargetKeys;
	for (const FString& BlockRef : BlockRefs)
	{
		TargetKeys.AddUnique(MakeGraphBlockTargetKey(GraphName, BlockRef));
	}
	if (TargetKeys.Num() == 0)
	{
		TargetKeys.Add(FString::Printf(TEXT("graph_block:%s"), *GraphName));
	}

	const FString AnchorJson = SerializePayloadForAnchor(Input.LoweredStep.Payload);
	const FString SignatureEvidenceId = ReadSignatureEvidenceId(Input.LoweredStep.Payload);
	for (int32 TargetIndex = 0; TargetIndex < TargetKeys.Num(); ++TargetIndex)
	{
		FBlueprintHelperReviewAtomicTarget Target;
		Target.AssetPath = AssetPath;
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
		Target.GraphName = GraphName;
		Target.TargetKind = TEXT("graph_block");
		Target.TargetSubKind = FBlueprintHelperGraphBodyBoundaryModelUtils::BodyKindToString(Input.BoundaryModel.BodyKind);
		Target.TargetKey = TargetKeys[TargetIndex];
		Target.ScopeIdentity = BuildScopeIdentity(AssetPath, GraphName, Target.TargetKey);
		Target.LifecycleObjectKey = Target.TargetKey;
		Target.VisualGroupKey = FString::Printf(TEXT("graph_body|%s"), *GraphName);
		Target.DisplayLabel = FString::Printf(TEXT("%s %s"), *OperationKind, *GraphName);
		Target.LatestEvidenceId = OutEvidence.EvidenceId;
		Target.SourceEvidenceIds.Add(OutEvidence.EvidenceId);
		Target.Ownership = TEXT("graph_write");
		Target.AnchorJson = AnchorJson;
		Target.ExecutionOrder = Input.StepIndex;
		Target.TaskStepIndex = Input.StepIndex;
		Target.AtomicIndex = TargetIndex;
		ApplySignatureDependencyMetadata(
			Target,
			Input.LoweredStep,
			SignatureEvidenceId);
		OutEvidence.AtomicTargets.Add(Target);
	}

	const TArray<FBlueprintHelperDiagnosticItem> Diagnostics =
		ReadReviewDiagnostics(Input.StepResult, GraphName);
	AttachDiagnosticsToEvidence(Diagnostics, OutEvidence);

	FBlueprintHelperGraphWriteOwnershipValidationInput OwnershipValidationInput;
	OwnershipValidationInput.GeneratedBlockRefs = BlockRefs;
	OwnershipValidationInput.AtomicTargets = OutEvidence.AtomicTargets;
	const FBlueprintHelperGraphWriteOwnershipValidationResult OwnershipValidation =
		FBlueprintHelperGraphWriteOwnershipValidator::Validate(OwnershipValidationInput);
	return OutEvidence.AtomicTargets.Num() > 0 && OwnershipValidation.bPassed;
}
