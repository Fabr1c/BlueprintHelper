#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperExternalUserGraphMutationAdapter.h"

#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskPlanLoweringUtils.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/ToolClusters/GraphWrite/Policy/BlueprintHelperExternalGraphWriteOperationPolicy.h"
#include "Systems/ToolClusters/GraphWrite/Policy/BlueprintHelperGraphWriteDomainPolicy.h"

#include "Dom/JsonValue.h"

static bool BlueprintHelperExternalUserGraphMutationValidateDomainPolicy(
	const TSharedPtr<FJsonObject>& StepObject,
	FBlueprintHelperToolError& OutError)
{
	const TSharedPtr<FJsonObject>* ConstraintsObjectPtr = nullptr;
	if (!StepObject.IsValid() ||
		!StepObject->TryGetObjectField(TEXT("constraints"), ConstraintsObjectPtr) ||
		!ConstraintsObjectPtr || !ConstraintsObjectPtr->IsValid())
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("unsupported_scope_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("graph_write external_graph_edit TaskPlan steps require constraints ownership policy."),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("constraints")));
		return false;
	}

	bool bAllowModifyUserNodes = false;
	if (!(*ConstraintsObjectPtr)->TryGetBoolField(TEXT("allow_modify_user_nodes"), bAllowModifyUserNodes) ||
		bAllowModifyUserNodes)
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("unsupported_scope_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("graph_write external_graph_edit TaskPlan steps require constraints.allow_modify_user_nodes=false."),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("constraints.allow_modify_user_nodes")));
		return false;
	}

	FString OwnershipScope;
	if (!(*ConstraintsObjectPtr)->TryGetStringField(TEXT("ownership_scope"), OwnershipScope) ||
		OwnershipScope != TEXT("external_user_authored"))
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("unsupported_scope_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("graph_write external_graph_edit TaskPlan steps require constraints.ownership_scope=external_user_authored."),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("constraints.ownership_scope")));
		return false;
	}

	const TSharedPtr<FJsonObject>* PolicyObjectPtr = nullptr;
	if (!(*ConstraintsObjectPtr)->TryGetObjectField(TEXT("external_mutation_policy"), PolicyObjectPtr) ||
		!PolicyObjectPtr || !PolicyObjectPtr->IsValid())
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("unsupported_scope_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("graph_write external_graph_edit TaskPlan steps require constraints.external_mutation_policy."),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("constraints.external_mutation_policy")));
		return false;
	}

	FString PolicyStrategy;
	if (!(*PolicyObjectPtr)->TryGetStringField(TEXT("strategy"), PolicyStrategy))
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("unsupported_scope_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("external_graph_edit requires constraints.external_mutation_policy.strategy to be merge_external_flow, patch_external_graph, patch_external_links, or replace_external_body."),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("constraints.external_mutation_policy.strategy")));
		return false;
	}

	TArray<FString> ExpectedMutations;
	if (!FBlueprintHelperExternalGraphWriteOperationPolicy::TryBuildExpectedMutationAllowlist(
		PolicyStrategy,
		ExpectedMutations))
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("unsupported_scope_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("external_graph_edit requires constraints.external_mutation_policy.strategy to be merge_external_flow, patch_external_graph, patch_external_links, or replace_external_body."),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("constraints.external_mutation_policy.strategy")));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* MutationsArray = nullptr;
	if (!(*PolicyObjectPtr)->TryGetArrayField(TEXT("allowed_mutations"), MutationsArray) ||
		!MutationsArray || MutationsArray->Num() != ExpectedMutations.Num())
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("unsupported_scope_policy"),
			EBlueprintHelperToolStage::ParseInput,
			FString::Printf(TEXT("%s requires an exact external mutation allowlist."), *PolicyStrategy),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("constraints.external_mutation_policy.allowed_mutations")));
		return false;
	}

	TArray<FString> ActualMutations;
	for (const TSharedPtr<FJsonValue>& MutationValue : *MutationsArray)
	{
		FString Mutation;
		if (!MutationValue.IsValid() || !MutationValue->TryGetString(Mutation))
		{
			OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
				TEXT("unsupported_scope_policy"),
				EBlueprintHelperToolStage::ParseInput,
				FString::Printf(TEXT("%s requires string external mutation allowlist entries."), *PolicyStrategy),
				BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("constraints.external_mutation_policy.allowed_mutations")));
			return false;
		}
		ActualMutations.Add(Mutation);
	}
	ActualMutations.Sort();
	ExpectedMutations.Sort();
	if (ActualMutations != ExpectedMutations)
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("unsupported_scope_policy"),
			EBlueprintHelperToolStage::ParseInput,
			FString::Printf(TEXT("%s requires an exact external mutation allowlist."), *PolicyStrategy),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("constraints.external_mutation_policy.allowed_mutations")));
		return false;
	}

	FBlueprintHelperGraphWriteDomainPolicyRequest DomainRequest;
	DomainRequest.Domain = EBlueprintHelperGraphWriteTargetDomain::ExternalUserAuthored;
	DomainRequest.Strategy = FBlueprintHelperExternalGraphWriteOperationPolicy::ExternalGraphEditStrategy();
	DomainRequest.OwnershipScope = OwnershipScope;
	DomainRequest.bAllowModifyUserNodes = bAllowModifyUserNodes;
	DomainRequest.AllowedExternalMutations = ActualMutations;
	FString DomainPolicyError;
	if (!FBlueprintHelperGraphWriteDomainPolicy::ValidateExternalRequest(DomainRequest, DomainPolicyError))
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("unsupported_scope_policy"),
			EBlueprintHelperToolStage::ParseInput,
			DomainPolicyError,
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("constraints")));
		return false;
	}

	return true;
}

static bool BlueprintHelperExternalUserGraphMutationBuildMergePayload(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const TSharedPtr<FJsonObject>& StepObject,
	const TSharedPtr<FJsonObject>& TargetObject,
	const FString& AssetPath,
	const FString& GraphName,
	const TSharedPtr<FJsonObject>& OpObject,
	bool bDryRun,
	TSharedPtr<FJsonObject>& OutPayload,
	FBlueprintHelperToolError& OutError)
{
	FString InsertStrategy;
	if (!OpObject.IsValid() ||
		!OpObject->TryGetStringField(TEXT("insert_strategy"), InsertStrategy) ||
		InsertStrategy.IsEmpty())
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("invalid_graph_write_ir_op"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("insert_external_flow requires insert_strategy."),
			BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("insert_strategy")));
		return false;
	}

	TSharedPtr<FJsonObject> Anchor;
	TSharedPtr<FJsonObject> Inserted;
	if (!BlueprintHelperGraphWriteLowering::TryReadRequiredObject(OpObject, TEXT("anchor"), BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("anchor")), Anchor, OutError) ||
		!BlueprintHelperGraphWriteLowering::TryReadRequiredObject(OpObject, TEXT("inserted"), BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("inserted")), Inserted, OutError))
	{
		return false;
	}

	TSharedRef<FJsonObject> InsertedPayload = MakeShared<FJsonObject>();
	BlueprintHelperGraphWriteLowering::CopyObjectFields(Inserted, InsertedPayload);
	FString InsertedBlockId;
	if (!InsertedPayload->TryGetStringField(TEXT("block_id"), InsertedBlockId) || InsertedBlockId.IsEmpty())
	{
		FString StepId;
		if (!StepObject.IsValid() || !StepObject->TryGetStringField(TEXT("step_id"), StepId) || StepId.IsEmpty())
		{
			StepId = TEXT("step_001");
		}
		InsertedPayload->SetStringField(
			TEXT("block_id"),
			FString::Printf(TEXT("ExternalMerge_%s"), *BlueprintHelperGraphWriteLowering::ToIdSegment(StepId)));
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetObjectField(
		TEXT("target"),
		BlueprintHelperGraphWriteLowering::BuildTargetPayload(TargetObject, AssetPath, GraphName));
	Payload->SetStringField(TEXT("insert_strategy"), InsertStrategy);
	Payload->SetObjectField(TEXT("anchor"), Anchor);
	Payload->SetObjectField(TEXT("inserted"), InsertedPayload);
	Payload->SetBoolField(TEXT("dry_run"), bDryRun);

	FString FeatureName;
	if (TaskPlan.IsValid() && TaskPlan->TryGetStringField(TEXT("task_name"), FeatureName) && !FeatureName.IsEmpty())
	{
		Payload->SetStringField(TEXT("feature_name"), FeatureName);
	}

	const TArray<TSharedPtr<FJsonValue>>* SequenceOrder = nullptr;
	if (OpObject->TryGetArrayField(TEXT("sequence_order"), SequenceOrder) && SequenceOrder)
	{
		Payload->SetArrayField(TEXT("sequence_order"), *SequenceOrder);
	}

	OutPayload = Payload;
	return true;
}

static bool BlueprintHelperExternalUserGraphMutationBuildPatchPayload(
	const TSharedPtr<FJsonObject>& TargetObject,
	const FString& AssetPath,
	const FString& GraphName,
	const TSharedPtr<FJsonObject>& OpObject,
	const FString& OpName,
	bool bDryRun,
	TSharedPtr<FJsonObject>& OutPayload,
	FBlueprintHelperToolError& OutError)
{
	TSharedPtr<FJsonObject> Anchor;
	TSharedPtr<FJsonObject> ExpectedOldState;
	if (!BlueprintHelperGraphWriteLowering::TryReadRequiredObject(OpObject, TEXT("anchor"), BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("anchor")), Anchor, OutError) ||
		!BlueprintHelperGraphWriteLowering::TryReadRequiredObject(OpObject, TEXT("expected_old_state"), BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("expected_old_state")), ExpectedOldState, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonValue> Value = FBlueprintHelperVersionCompat::FindJsonValue(OpObject, TEXT("value"));
	if (!Value.IsValid())
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("invalid_graph_write_ir_op"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("External GraphWrite patch requires value."),
			BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("value")));
		return false;
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetObjectField(
		TEXT("target"),
		BlueprintHelperGraphWriteLowering::BuildTargetPayload(TargetObject, AssetPath, GraphName));
	Payload->SetStringField(TEXT("patch_type"), OpName);
	Payload->SetObjectField(TEXT("anchor"), Anchor);
	Payload->SetField(TEXT("value"), Value);
	Payload->SetObjectField(TEXT("expected_old_state"), ExpectedOldState);
	Payload->SetBoolField(TEXT("dry_run"), bDryRun);
	FString PropertyDescriptorId;
	if (OpObject->TryGetStringField(TEXT("property_descriptor_id"), PropertyDescriptorId) &&
		!PropertyDescriptorId.IsEmpty())
	{
		Payload->SetStringField(TEXT("property_descriptor_id"), PropertyDescriptorId);
	}

	OutPayload = Payload;
	return true;
}

static bool BlueprintHelperExternalUserGraphMutationBuildLinkPatchPayload(
	const TSharedPtr<FJsonObject>& TargetObject,
	const FString& AssetPath,
	const FString& GraphName,
	const TSharedPtr<FJsonObject>& OpObject,
	const FString& OpName,
	bool bDryRun,
	TSharedPtr<FJsonObject>& OutPayload,
	FBlueprintHelperToolError& OutError)
{
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetObjectField(
		TEXT("target"),
		BlueprintHelperGraphWriteLowering::BuildTargetPayload(TargetObject, AssetPath, GraphName));
	Payload->SetBoolField(TEXT("dry_run"), bDryRun);

	if (OpName == TEXT("connect_external_pins"))
	{
		TSharedPtr<FJsonObject> SourceAnchor;
		TSharedPtr<FJsonObject> TargetAnchor;
		if (!BlueprintHelperGraphWriteLowering::TryReadRequiredObject(OpObject, TEXT("source_anchor"), BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("source_anchor")), SourceAnchor, OutError) ||
			!BlueprintHelperGraphWriteLowering::TryReadRequiredObject(OpObject, TEXT("target_anchor"), BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("target_anchor")), TargetAnchor, OutError))
		{
			return false;
		}
		Payload->SetStringField(TEXT("patch_type"), TEXT("connect_pins"));
		Payload->SetObjectField(TEXT("source_anchor"), SourceAnchor);
		Payload->SetObjectField(TEXT("target_anchor"), TargetAnchor);
		OutPayload = Payload;
		return true;
	}

	if (OpName == TEXT("disconnect_external_link"))
	{
		TSharedPtr<FJsonObject> LinkAnchor;
		if (!BlueprintHelperGraphWriteLowering::TryReadRequiredObject(OpObject, TEXT("link_anchor"), BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("link_anchor")), LinkAnchor, OutError))
		{
			return false;
		}
		Payload->SetStringField(TEXT("patch_type"), TEXT("disconnect_link"));
		Payload->SetObjectField(TEXT("link_anchor"), LinkAnchor);
		OutPayload = Payload;
		return true;
	}

	if (OpName == TEXT("replace_external_link"))
	{
		TSharedPtr<FJsonObject> LinkAnchor;
		TSharedPtr<FJsonObject> ReplacementAnchor;
		if (!BlueprintHelperGraphWriteLowering::TryReadRequiredObject(OpObject, TEXT("link_anchor"), BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("link_anchor")), LinkAnchor, OutError) ||
			!BlueprintHelperGraphWriteLowering::TryReadRequiredObject(OpObject, TEXT("replacement_anchor"), BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("replacement_anchor")), ReplacementAnchor, OutError))
		{
			return false;
		}
		Payload->SetStringField(TEXT("patch_type"), TEXT("replace_link"));
		Payload->SetObjectField(TEXT("link_anchor"), LinkAnchor);
		Payload->SetObjectField(TEXT("replacement_anchor"), ReplacementAnchor);
		OutPayload = Payload;
		return true;
	}

	OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
		TEXT("unsupported_graph_write_ir_op"),
		EBlueprintHelperToolStage::ParseInput,
		TEXT("patch_external_links supports connect_external_pins, disconnect_external_link, and replace_external_link."),
		BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("op")));
	return false;
}

static bool BlueprintHelperExternalUserGraphMutationBuildReplaceBodyPayload(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const TSharedPtr<FJsonObject>& TargetObject,
	const FString& AssetPath,
	const FString& GraphName,
	const TSharedPtr<FJsonObject>& OpObject,
	bool bDryRun,
	TSharedPtr<FJsonObject>& OutPayload,
	FBlueprintHelperToolError& OutError)
{
	FString ReplaceScope;
	if (!OpObject.IsValid() ||
		!OpObject->TryGetStringField(TEXT("replace_scope"), ReplaceScope) ||
		ReplaceScope.IsEmpty())
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("invalid_graph_write_ir_op"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("replace_external_body requires replace_scope."),
			BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("replace_scope")));
		return false;
	}

	TSharedPtr<FJsonObject> Anchor;
	TSharedPtr<FJsonObject> LogicSpec;
	if (!BlueprintHelperGraphWriteLowering::TryReadRequiredObject(OpObject, TEXT("anchor"), BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("anchor")), Anchor, OutError) ||
		!BlueprintHelperGraphWriteLowering::TryReadRequiredObject(OpObject, TEXT("logic_spec"), BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("logic_spec")), LogicSpec, OutError))
	{
		return false;
	}

	FString ExpectedBodyFingerprint;
	if (!OpObject->TryGetStringField(TEXT("expected_body_fingerprint"), ExpectedBodyFingerprint) ||
		ExpectedBodyFingerprint.IsEmpty())
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("invalid_graph_write_ir_op"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("replace_external_body requires expected_body_fingerprint."),
			BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("expected_body_fingerprint")));
		return false;
	}

	bool bRequireFullDryRun = false;
	if (!OpObject->TryGetBoolField(TEXT("require_full_dry_run"), bRequireFullDryRun) ||
		!bRequireFullDryRun)
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("invalid_graph_write_ir_op"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("replace_external_body requires require_full_dry_run=true."),
			BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("require_full_dry_run")));
		return false;
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetObjectField(
		TEXT("target"),
		BlueprintHelperGraphWriteLowering::BuildTargetPayload(TargetObject, AssetPath, GraphName));
	Payload->SetStringField(TEXT("scope"), ReplaceScope);
	Payload->SetObjectField(TEXT("anchor"), Anchor);
	Payload->SetObjectField(TEXT("body"), LogicSpec);
	Payload->SetStringField(TEXT("expected_body_fingerprint"), ExpectedBodyFingerprint);
	Payload->SetBoolField(TEXT("require_full_dry_run"), true);
	Payload->SetBoolField(TEXT("dry_run"), bDryRun);

	FString FeatureName;
	if (TaskPlan.IsValid() && TaskPlan->TryGetStringField(TEXT("task_name"), FeatureName) && !FeatureName.IsEmpty())
	{
		Payload->SetStringField(TEXT("feature_name"), FeatureName);
	}

	OutPayload = Payload;
	return true;
}

bool FBlueprintHelperExternalUserGraphMutationAdapter::TryLower(
	const FBlueprintHelperGraphWriteLoweringRequest& Request,
	FBlueprintHelperGraphWriteLoweringResult& OutResult,
	FBlueprintHelperToolError& OutError)
{
	OutResult = FBlueprintHelperGraphWriteLoweringResult();
	OutError = FBlueprintHelperToolError();

	if (!BlueprintHelperExternalUserGraphMutationValidateDomainPolicy(Request.StepObject, OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> TargetObject;
	FString AssetPath;
	FString GraphName;
	if (!BlueprintHelperGraphWriteLowering::TryReadStepTarget(
		Request.StepObject,
		TargetObject,
		AssetPath,
		GraphName,
		OutError))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
	if (!BlueprintHelperGraphWriteLowering::TryReadWriteOps(Request.StepObject, OpsArray, OutError))
	{
		return false;
	}
	if (OpsArray->Num() != 1)
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("unsupported_graph_write_ir_op_batch"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("external_graph_edit supports one operation per TaskPlan step."),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("write.ops")));
		return false;
	}

	const TSharedPtr<FJsonObject> FirstOpObject =
		BlueprintHelperGraphWriteLowering::AsJsonObjectIfObject((*OpsArray)[0]);
	FString OpName;
	if (!FirstOpObject.IsValid() ||
		!FirstOpObject->TryGetStringField(TEXT("op"), OpName) ||
		OpName.IsEmpty())
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("invalid_graph_write_ir_op"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("GraphWrite write.ops entry requires op."),
			BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("op")));
		return false;
	}

	TSharedPtr<FJsonObject> Payload;
	FString AdapterOperation;
	EBlueprintHelperExternalGraphWriteAdapterOperationKind AdapterOperationKind =
		EBlueprintHelperExternalGraphWriteAdapterOperationKind::Unknown;
	if (!FBlueprintHelperExternalGraphWriteOperationPolicy::TryResolveTaskOpAdapterOperation(
		OpName,
		AdapterOperation,
		AdapterOperationKind))
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("unsupported_graph_write_ir_op"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("external_graph_edit supports insert_external_flow, external field patches, external link patches, and replace_external_body only."),
			BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("op")));
		return false;
	}

	if (AdapterOperationKind == EBlueprintHelperExternalGraphWriteAdapterOperationKind::MergeFlow)
	{
		if (!BlueprintHelperExternalUserGraphMutationBuildMergePayload(Request.TaskPlan, Request.StepObject, TargetObject, AssetPath, GraphName, FirstOpObject, Request.bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (AdapterOperationKind == EBlueprintHelperExternalGraphWriteAdapterOperationKind::PropertyPatch)
	{
		if (!BlueprintHelperExternalUserGraphMutationBuildPatchPayload(TargetObject, AssetPath, GraphName, FirstOpObject, OpName, Request.bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (AdapterOperationKind == EBlueprintHelperExternalGraphWriteAdapterOperationKind::LinkPatch)
	{
		if (!BlueprintHelperExternalUserGraphMutationBuildLinkPatchPayload(TargetObject, AssetPath, GraphName, FirstOpObject, OpName, Request.bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else if (AdapterOperationKind == EBlueprintHelperExternalGraphWriteAdapterOperationKind::BodyReplace)
	{
		if (!BlueprintHelperExternalUserGraphMutationBuildReplaceBodyPayload(Request.TaskPlan, TargetObject, AssetPath, GraphName, FirstOpObject, Request.bDryRun, Payload, OutError))
		{
			return false;
		}
	}

	OutResult.LoweredStep.Capability = TEXT("graph_write");
	OutResult.LoweredStep.RuntimeOperation = TEXT("graph_write");
	OutResult.LoweredStep.AdapterOperation = AdapterOperation;
	OutResult.LoweredStep.Payload = Payload;
	return true;
}
