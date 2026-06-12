#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperOwnedGraphMutationAdapter.h"

#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskPlanLoweringUtils.h"
#include "Systems/ToolClusters/GraphWrite/Policy/BlueprintHelperGraphWriteDomainPolicy.h"

#include "Dom/JsonValue.h"

static bool BlueprintHelperOwnedGraphMutationValidateDomainPolicy(
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
			TEXT("graph_write owned_graph_edit TaskPlan steps require constraints ownership policy."),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("constraints")));
		return false;
	}

	bool bAllowModifyUserNodes = false;
	if (!(*ConstraintsObjectPtr)->TryGetBoolField(TEXT("allow_modify_user_nodes"), bAllowModifyUserNodes))
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("unsupported_scope_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("graph_write owned_graph_edit TaskPlan steps require constraints.allow_modify_user_nodes=false."),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("constraints.allow_modify_user_nodes")));
		return false;
	}

	FString OwnershipScope;
	if (!(*ConstraintsObjectPtr)->TryGetStringField(TEXT("ownership_scope"), OwnershipScope) ||
		OwnershipScope.IsEmpty())
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("unsupported_scope_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("graph_write owned_graph_edit TaskPlan steps require constraints.ownership_scope=blueprinthelper_owned."),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("constraints.ownership_scope")));
		return false;
	}

	FBlueprintHelperGraphWriteDomainPolicyRequest DomainRequest;
	DomainRequest.Domain = EBlueprintHelperGraphWriteTargetDomain::BlueprintHelperOwned;
	DomainRequest.Strategy = TEXT("owned_graph_edit");
	DomainRequest.OwnershipScope = OwnershipScope;
	DomainRequest.bAllowModifyUserNodes = bAllowModifyUserNodes;
	FString DomainPolicyError;
	if (!FBlueprintHelperGraphWriteDomainPolicy::ValidateOwnedRequest(DomainRequest, DomainPolicyError))
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

static bool BlueprintHelperOwnedGraphMutationBuildEnsureEntryLogicSpec(
	const TSharedPtr<FJsonObject>& OpObject,
	int32 OpIndex,
	bool bHasSignatureDependency,
	TSharedPtr<FJsonObject>& OutLogicSpec,
	FBlueprintHelperToolError& OutError)
{
	if (!OpObject.IsValid())
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("invalid_graph_write_ir_op"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("GraphWrite write.ops entry must be an object."),
			BlueprintHelperGraphWriteLowering::BuildOpFieldPath(OpIndex, TEXT("")));
		return false;
	}

	FString OpName;
	OpObject->TryGetStringField(TEXT("op"), OpName);
	if (OpName != TEXT("ensure_entry"))
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("unsupported_graph_write_ir_op"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("GraphWrite owned_graph_edit currently supports ensure_entry operations only."),
			BlueprintHelperGraphWriteLowering::BuildOpFieldPath(OpIndex, TEXT("op")));
		return false;
	}

	FString EntryType;
	OpObject->TryGetStringField(TEXT("entry_type"), EntryType);
	if (EntryType != TEXT("custom_event"))
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("unsupported_graph_write_ir_entry_type"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("GraphWrite ensure_entry currently supports custom_event entries only."),
			BlueprintHelperGraphWriteLowering::BuildOpFieldPath(OpIndex, TEXT("entry_type")));
		return false;
	}

	FString EntryName;
	if (!OpObject->TryGetStringField(TEXT("name"), EntryName) || EntryName.IsEmpty())
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("invalid_graph_write_ir_op"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("GraphWrite ensure_entry requires name."),
			BlueprintHelperGraphWriteLowering::BuildOpFieldPath(OpIndex, TEXT("name")));
		return false;
	}

	const FString EntryId = FString::Printf(TEXT("%s_entry"), *BlueprintHelperGraphWriteLowering::ToIdSegment(EntryName));
	TSharedRef<FJsonObject> EntryObject = MakeShared<FJsonObject>();
	EntryObject->SetStringField(TEXT("kind"), TEXT("custom_event"));
	EntryObject->SetStringField(TEXT("name"), EntryName);
	EntryObject->SetStringField(TEXT("id"), EntryId);
	FString SignatureEvidenceId;
	if (OpObject->TryGetStringField(TEXT("signature_evidence_id"), SignatureEvidenceId) &&
		!SignatureEvidenceId.TrimStartAndEnd().IsEmpty())
	{
		EntryObject->SetStringField(TEXT("signature_evidence_id"), SignatureEvidenceId.TrimStartAndEnd());
	}
	if (bHasSignatureDependency)
	{
		EntryObject->SetBoolField(TEXT("signature_dependency"), true);
		EntryObject->SetStringField(TEXT("source"), TEXT("signature_dependency"));
		EntryObject->SetStringField(TEXT("source_cluster"), TEXT("blueprint_signature"));
	}

	TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject>* LogicSpecObjectPtr = nullptr;
	const TSharedPtr<FJsonObject>* BodyObjectPtr = nullptr;
	if (OpObject->TryGetObjectField(TEXT("logic_spec"), LogicSpecObjectPtr) &&
		LogicSpecObjectPtr && LogicSpecObjectPtr->IsValid())
	{
		BlueprintHelperGraphWriteLowering::CopyObjectFields(*LogicSpecObjectPtr, LogicSpec);
	}
	else if (OpObject->TryGetObjectField(TEXT("body"), BodyObjectPtr) &&
		BodyObjectPtr && BodyObjectPtr->IsValid())
	{
		BlueprintHelperGraphWriteLowering::CopyObjectFields(*BodyObjectPtr, LogicSpec);
	}

	if (!LogicSpec->HasField(TEXT("statements")))
	{
		LogicSpec->SetArrayField(TEXT("statements"), {});
	}
	LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v2"));
	if (!LogicSpec->HasTypedField<EJson::Object>(TEXT("entry")))
	{
		LogicSpec->SetObjectField(TEXT("entry"), EntryObject);
	}

	OutLogicSpec = LogicSpec;
	return true;
}

static bool BlueprintHelperOwnedGraphMutationBuildAppendPayload(
	const FBlueprintHelperGraphWriteLoweringRequest& Request,
	TSharedPtr<FJsonObject>& OutPayload,
	FBlueprintHelperToolError& OutError)
{
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
			TEXT("unsupported_graph_write_op_batch"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("SemanticIR append graph_write supports one ensure_entry operation per TaskPlan step."),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("write.ops")));
		return false;
	}

	const TSharedPtr<FJsonObject> OpObject =
		BlueprintHelperGraphWriteLowering::AsJsonObjectIfObject((*OpsArray)[0]);
	TSharedPtr<FJsonObject> LogicSpec;
	const bool bHasSignatureDependency =
		BlueprintHelperGraphWriteLowering::ReadStepDependsOn(Request.StepObject).Num() > 0;
	if (!BlueprintHelperOwnedGraphMutationBuildEnsureEntryLogicSpec(
		OpObject,
		0,
		bHasSignatureDependency,
		LogicSpec,
		OutError))
	{
		return false;
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetObjectField(
		TEXT("target"),
		BlueprintHelperGraphWriteLowering::BuildTargetPayload(TargetObject, AssetPath, GraphName));
	Payload->SetBoolField(TEXT("dry_run"), Request.bDryRun);
	Payload->SetBoolField(TEXT("allow_existing_graph"), true);
	if (bHasSignatureDependency)
	{
		Payload->SetBoolField(TEXT("reuse_existing_entries"), true);
	}

	FString FeatureName;
	if (Request.TaskPlan.IsValid() &&
		Request.TaskPlan->TryGetStringField(TEXT("task_name"), FeatureName) &&
		!FeatureName.IsEmpty())
	{
		Payload->SetStringField(TEXT("feature_name"), FeatureName);
	}

	Payload->SetObjectField(TEXT("logic_spec"), LogicSpec);
	OutPayload = Payload;
	return true;
}

static bool BlueprintHelperOwnedGraphMutationBuildReplacePayload(
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
			TEXT("replace_body requires replace_scope."),
			BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("replace_scope")));
		return false;
	}

	TSharedPtr<FJsonObject> Selector;
	TSharedPtr<FJsonObject> LogicSpec;
	if (!BlueprintHelperGraphWriteLowering::TryReadRequiredObject(OpObject, TEXT("selector"), BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("selector")), Selector, OutError) ||
		!BlueprintHelperGraphWriteLowering::TryReadRequiredObject(OpObject, TEXT("logic_spec"), BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("logic_spec")), LogicSpec, OutError))
	{
		return false;
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> BridgeTarget =
		BlueprintHelperGraphWriteLowering::BuildTargetPayload(TargetObject, AssetPath, GraphName);
	BridgeTarget->SetStringField(TEXT("replace_scope"), ReplaceScope);
	Payload->SetObjectField(TEXT("target"), BridgeTarget);
	Payload->SetObjectField(TEXT("selector"), Selector);
	Payload->SetObjectField(TEXT("logic_spec"), LogicSpec);

	TSharedRef<FJsonObject> Options = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject>* OptionsObject = nullptr;
	if (OpObject->TryGetObjectField(TEXT("options"), OptionsObject) &&
		OptionsObject && OptionsObject->IsValid())
	{
		BlueprintHelperGraphWriteLowering::CopyObjectFields(*OptionsObject, Options);
	}
	Options->SetBoolField(TEXT("dry_run"), bDryRun);
	Payload->SetObjectField(TEXT("options"), Options);

	OutPayload = Payload;
	return true;
}

static FString BlueprintHelperOwnedGraphMutationDefaultPatchScope(const FString& OpName)
{
	if (OpName == TEXT("set_node_comment"))
	{
		return TEXT("node_comment");
	}
	if (OpName == TEXT("connect_pins"))
	{
		return TEXT("connect_pins");
	}
	if (OpName == TEXT("disconnect_link"))
	{
		return TEXT("disconnect_link");
	}
	if (OpName == TEXT("replace_link"))
	{
		return TEXT("replace_link");
	}
	if (OpName == TEXT("delete_owned_node"))
	{
		return TEXT("node_delete");
	}
	return TEXT("pin_default");
}

static bool BlueprintHelperOwnedGraphMutationBuildPatchPayload(
	const TSharedPtr<FJsonObject>& TargetObject,
	const FString& AssetPath,
	const FString& GraphName,
	const TSharedPtr<FJsonObject>& OpObject,
	const FString& OpName,
	bool bDryRun,
	TSharedPtr<FJsonObject>& OutPayload,
	FBlueprintHelperToolError& OutError)
{
	TSharedPtr<FJsonObject> PatchedRef;
	TSharedPtr<FJsonObject> Patch;
	if (!BlueprintHelperGraphWriteLowering::TryReadRequiredObject(OpObject, TEXT("patched_ref"), BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("patched_ref")), PatchedRef, OutError) ||
		!BlueprintHelperGraphWriteLowering::TryReadRequiredObject(OpObject, TEXT("patch"), BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("patch")), Patch, OutError))
	{
		return false;
	}

	FString PatchScope;
	if (!OpObject->TryGetStringField(TEXT("patch_scope"), PatchScope) || PatchScope.IsEmpty())
	{
		PatchScope = BlueprintHelperOwnedGraphMutationDefaultPatchScope(OpName);
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> BridgeTarget =
		BlueprintHelperGraphWriteLowering::BuildTargetPayload(TargetObject, AssetPath, GraphName);
	BridgeTarget->SetStringField(TEXT("patch_scope"), PatchScope);
	Payload->SetObjectField(TEXT("target"), BridgeTarget);
	Payload->SetStringField(TEXT("patch_type"), OpName);
	Payload->SetObjectField(TEXT("patched_ref"), PatchedRef);
	Payload->SetObjectField(TEXT("patch"), Patch);
	Payload->SetBoolField(TEXT("dry_run"), bDryRun);

	const TSharedPtr<FJsonObject>* ExpectedOldState = nullptr;
	if (OpObject->TryGetObjectField(TEXT("expected_old_state"), ExpectedOldState) &&
		ExpectedOldState && ExpectedOldState->IsValid())
	{
		Payload->SetObjectField(TEXT("expected_old_state"), *ExpectedOldState);
	}

	OutPayload = Payload;
	return true;
}

static bool BlueprintHelperOwnedGraphMutationBuildMergePayload(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const TSharedPtr<FJsonObject>& TargetObject,
	const FString& AssetPath,
	const FString& GraphName,
	const TSharedPtr<FJsonObject>& OpObject,
	bool bDryRun,
	TSharedPtr<FJsonObject>& OutPayload,
	FBlueprintHelperToolError& OutError)
{
	FString MergeScope;
	FString InsertStrategy;
	if (!OpObject.IsValid() ||
		!OpObject->TryGetStringField(TEXT("merge_scope"), MergeScope) ||
		MergeScope.IsEmpty())
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("invalid_graph_write_ir_op"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("insert_flow requires merge_scope."),
			BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("merge_scope")));
		return false;
	}
	if (!OpObject->TryGetStringField(TEXT("insert_strategy"), InsertStrategy) ||
		InsertStrategy.IsEmpty())
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("invalid_graph_write_ir_op"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("insert_flow requires insert_strategy."),
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

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> BridgeTarget =
		BlueprintHelperGraphWriteLowering::BuildTargetPayload(TargetObject, AssetPath, GraphName);
	BridgeTarget->SetStringField(TEXT("merge_scope"), MergeScope);
	BridgeTarget->SetStringField(TEXT("insert_strategy"), InsertStrategy);
	Payload->SetObjectField(TEXT("target"), BridgeTarget);
	Payload->SetObjectField(TEXT("anchor"), Anchor);
	Payload->SetObjectField(TEXT("inserted"), Inserted);
	Payload->SetBoolField(TEXT("dry_run"), bDryRun);

	bool bAllowCompileBeforeCall = false;
	if (BlueprintHelperGraphWriteLowering::TryReadExecutionPolicyBool(TaskPlan, TEXT("should_compile"), bAllowCompileBeforeCall))
	{
		Payload->SetBoolField(TEXT("allow_compile_before_call"), bAllowCompileBeforeCall);
	}

	const TArray<TSharedPtr<FJsonValue>>* SequenceOrder = nullptr;
	if (OpObject->TryGetArrayField(TEXT("sequence_order"), SequenceOrder) && SequenceOrder)
	{
		Payload->SetArrayField(TEXT("sequence_order"), *SequenceOrder);
	}

	OutPayload = Payload;
	return true;
}

bool FBlueprintHelperOwnedGraphMutationAdapter::TryLower(
	const FBlueprintHelperGraphWriteLoweringRequest& Request,
	FBlueprintHelperGraphWriteLoweringResult& OutResult,
	FBlueprintHelperToolError& OutError)
{
	OutResult = FBlueprintHelperGraphWriteLoweringResult();
	OutError = FBlueprintHelperToolError();

	if (!BlueprintHelperOwnedGraphMutationValidateDomainPolicy(Request.StepObject, OutError))
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
	if (OpName == TEXT("ensure_entry"))
	{
		AdapterOperation = TEXT("append_blueprint_graph");
		if (!BlueprintHelperOwnedGraphMutationBuildAppendPayload(Request, Payload, OutError))
		{
			return false;
		}
	}
	else
	{
		if (OpsArray->Num() != 1)
		{
			OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
				TEXT("unsupported_graph_write_ir_op_batch"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("GraphWrite replace/patch/merge IR supports one structural op per TaskPlan step."),
				BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("write.ops")));
			return false;
		}

		if (OpName == TEXT("replace_body"))
		{
			AdapterOperation = TEXT("replace_blueprint_graph");
			if (!BlueprintHelperOwnedGraphMutationBuildReplacePayload(TargetObject, AssetPath, GraphName, FirstOpObject, Request.bDryRun, Payload, OutError))
			{
				return false;
			}
		}
		else if (OpName == TEXT("set_pin_default") ||
			OpName == TEXT("set_node_comment") ||
			OpName == TEXT("connect_pins") ||
			OpName == TEXT("disconnect_link") ||
			OpName == TEXT("replace_link") ||
			OpName == TEXT("delete_owned_node"))
		{
			AdapterOperation = TEXT("patch_blueprint_graph");
			if (!BlueprintHelperOwnedGraphMutationBuildPatchPayload(TargetObject, AssetPath, GraphName, FirstOpObject, OpName, Request.bDryRun, Payload, OutError))
			{
				return false;
			}
		}
		else if (OpName == TEXT("insert_flow"))
		{
			AdapterOperation = TEXT("merge_blueprint_graph");
			if (!BlueprintHelperOwnedGraphMutationBuildMergePayload(Request.TaskPlan, TargetObject, AssetPath, GraphName, FirstOpObject, Request.bDryRun, Payload, OutError))
			{
				return false;
			}
		}
		else
		{
			OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
				TEXT("unsupported_graph_write_ir_op"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("GraphWrite Task Runtime does not support this structural op."),
				BlueprintHelperGraphWriteLowering::BuildOpFieldPath(0, TEXT("op")));
			return false;
		}
	}

	OutResult.LoweredStep.Capability = TEXT("graph_write");
	OutResult.LoweredStep.RuntimeOperation = TEXT("graph_write");
	OutResult.LoweredStep.AdapterOperation = AdapterOperation;
	OutResult.LoweredStep.Payload = Payload;
	return true;
}
