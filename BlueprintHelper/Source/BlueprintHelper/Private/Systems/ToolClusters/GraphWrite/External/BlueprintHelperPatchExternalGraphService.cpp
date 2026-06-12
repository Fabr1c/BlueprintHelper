// BlueprintHelper Service Layer - Patch external graph service.

#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperPatchExternalGraphService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.h"
#include "Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationIntent.h"
#include "Systems/ToolClusters/GraphWrite/UnitOfWork/BlueprintHelperGraphWriteUnitOfWork.h"

namespace BlueprintHelperPatchExternalGraph
{
	static constexpr const TCHAR* OperationName = TEXT("patch_external_graph");

	static FBlueprintHelperDryRunIssue MakeIssue(
		const FString& Code,
		const FString& Message,
		const FString& Target,
		const FString& Source)
	{
		FBlueprintHelperDryRunIssue Issue;
		Issue.Code = Code;
		Issue.Message = Message;
		Issue.Target = Target;
		Issue.Source = Source;
		return Issue;
	}

	static void AddError(
		FBlueprintHelperPatchExternalGraphService::FPatchExternalGraphPreflightResult& Result,
		const FString& Code,
		const FString& Message,
		const FString& Target,
		const FString& Source)
	{
		Result.bPassed = false;
		Result.BlockedBy.AddUnique(Code);
		Result.Errors.Add(MakeIssue(Code, Message, Target, Source));
	}

	static void AddConflict(
		FBlueprintHelperPatchExternalGraphService::FPatchExternalGraphPreflightResult& Result,
		const FString& Code,
		const FString& Message,
		const FString& Target,
		const FString& Source)
	{
		Result.bPassed = false;
		Result.BlockedBy.AddUnique(Code);
		Result.Conflicts.Add(MakeIssue(Code, Message, Target, Source));
	}

	static TArray<TSharedPtr<FJsonValue>> MakeStringArray(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		for (const FString& Value : Values)
		{
			Array.Add(MakeShared<FJsonValueString>(Value));
		}
		return Array;
	}

	static TArray<TSharedPtr<FJsonValue>> MakeIssueArray(const TArray<FBlueprintHelperDryRunIssue>& Issues)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		for (const FBlueprintHelperDryRunIssue& Issue : Issues)
		{
			Array.Add(MakeShared<FJsonValueObject>(Issue.ToJson()));
		}
		return Array;
	}

	static FBlueprintHelperToolError MakeToolError(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field,
		EBlueprintHelperRollbackResult RollbackResult = EBlueprintHelperRollbackResult::NotNeeded)
	{
		FBlueprintHelperToolError Error;
		Error.Code = Code;
		Error.Stage = Stage;
		Error.Message = Message;
		Error.Field = Field;
		Error.bRetryable = false;
		Error.RollbackResult = RollbackResult;
		return Error;
	}

	static FBlueprintHelperToolError MakeErrorFromPreflight(
		const FBlueprintHelperPatchExternalGraphService::FPatchExternalGraphPreflightResult& Preflight)
	{
		const FBlueprintHelperDryRunIssue* FirstIssue = Preflight.Conflicts.Num() > 0
			? &Preflight.Conflicts[0]
			: (Preflight.Errors.Num() > 0 ? &Preflight.Errors[0] : nullptr);
		return MakeToolError(
			Preflight.BlockedBy.Num() > 0 ? Preflight.BlockedBy[0] : TEXT("preflight_failed"),
			EBlueprintHelperToolStage::Preflight,
			FirstIssue && !FirstIssue->Message.IsEmpty()
				? FirstIssue->Message
				: TEXT("patch_external_graph preflight blocked execution."),
			FirstIssue && !FirstIssue->Source.IsEmpty()
				? FirstIssue->Source
				: TEXT("payload"));
	}

	static TSharedRef<FJsonObject> MakeDryRunData(
		const FBlueprintHelperPatchExternalGraphService::FPatchExternalGraphPreflightResult& Preflight,
		const FBlueprintHelperPatchExternalGraphService::FPatchExternalGraphRequest& Request,
		const FBlueprintHelperPatchExternalGraphService::FPatchExternalGraphContext& Context)
	{
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("PatchExternalGraphDryRun.v1"));

		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		DryRun->SetStringField(TEXT("result"), Preflight.bPassed ? TEXT("passed") : TEXT("blocked"));
		DryRun->SetBoolField(TEXT("can_execute"), Preflight.bPassed);
		DryRun->SetArrayField(TEXT("blocked_by"), MakeStringArray(Preflight.BlockedBy));
		DryRun->SetArrayField(TEXT("conflicts"), MakeIssueArray(Preflight.Conflicts));
		DryRun->SetArrayField(TEXT("errors"), MakeIssueArray(Preflight.Errors));
		Data->SetObjectField(TEXT("dry_run"), DryRun);

		TSharedRef<FJsonObject> Patch = MakeShared<FJsonObject>();
		Patch->SetStringField(TEXT("patch_type"), Request.PatchType);
		const FString NodeGuid = Context.Node ? Context.Node->NodeGuid.ToString(EGuidFormats::Digits) : Request.Anchor.NodeGuid;
		Patch->SetStringField(TEXT("node_guid"), NodeGuid);
		Patch->SetStringField(TEXT("pin_name"), Request.Anchor.PinName);
		if (!Request.PropertyDescriptorId.IsEmpty())
		{
			Patch->SetStringField(TEXT("property_descriptor_id"), Request.PropertyDescriptorId);
		}
		if (Context.bHasPropertyDescriptor)
		{
			Patch->SetStringField(TEXT("field_kind"), Context.PropertyDescriptor.FieldKind);
		}
		Patch->SetStringField(TEXT("before_value"), Context.BeforeValue);
		Patch->SetStringField(TEXT("after_value"), Request.Value);
		Data->SetObjectField(TEXT("external_patch"), Patch);
		return Data;
	}

	static UBlueprint* FindBlueprint(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return nullptr;
		}
		if (UBlueprint* Blueprint = FindObject<UBlueprint>(nullptr, *AssetPath))
		{
			return Blueprint;
		}
		return Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *AssetPath));
	}

	static void AddGraphIfValid(TArray<UEdGraph*>& Graphs, UEdGraph* Graph)
	{
		if (Graph)
		{
			Graphs.Add(Graph);
		}
	}

	static UEdGraph* FindGraphByName(UBlueprint* Blueprint, const FString& GraphName)
	{
		if (!Blueprint || GraphName.IsEmpty())
		{
			return nullptr;
		}

		TArray<UEdGraph*> Graphs;
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			AddGraphIfValid(Graphs, Graph);
		}
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			AddGraphIfValid(Graphs, Graph);
		}
		for (UEdGraph* Graph : Blueprint->MacroGraphs)
		{
			AddGraphIfValid(Graphs, Graph);
		}
		for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
		{
			AddGraphIfValid(Graphs, Graph);
		}

		for (UEdGraph* Graph : Graphs)
		{
			if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
			{
				return Graph;
			}
		}
		return nullptr;
	}

	static FString PinDirectionToString(const EEdGraphPinDirection Direction)
	{
		return Direction == EGPD_Input ? TEXT("input") : (Direction == EGPD_Output ? TEXT("output") : TEXT("unknown"));
	}

	static UEdGraphPin* FindPinByAnchor(UEdGraphNode* Node, const FBlueprintHelperExternalGraphAnchor& Anchor)
	{
		if (!Node || Anchor.PinName.IsEmpty())
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || !Pin->PinName.ToString().Equals(Anchor.PinName, ESearchCase::IgnoreCase))
			{
				continue;
			}
			if (!Anchor.PinDirection.IsEmpty() &&
				!PinDirectionToString(Pin->Direction).Equals(Anchor.PinDirection, ESearchCase::IgnoreCase))
			{
				continue;
			}
			return Pin;
		}
		return nullptr;
	}

	static bool ReadStringValueField(const TSharedRef<FJsonObject>& Payload, FString& OutValue)
	{
		if (Payload->TryGetStringField(TEXT("value"), OutValue))
		{
			return true;
		}

		const TSharedPtr<FJsonValue> Value = FBlueprintHelperVersionCompat::FindJsonValue(Payload, TEXT("value"));
		if (Value.IsValid())
		{
			OutValue = Value->AsString();
			return !OutValue.IsEmpty() || Value->Type == EJson::String;
		}
		return false;
	}
}

FBlueprintHelperPatchExternalGraphService::FPatchExternalGraphRequest
FBlueprintHelperPatchExternalGraphService::ParseRequest(const TSharedRef<FJsonObject>& Payload) const
{
	FPatchExternalGraphRequest Request;

	const TSharedPtr<FJsonObject>* TargetObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("target"), TargetObject) && TargetObject && TargetObject->IsValid())
	{
		(*TargetObject)->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
		(*TargetObject)->TryGetStringField(TEXT("graph"), Request.GraphName);
		if (Request.GraphName.IsEmpty())
		{
			(*TargetObject)->TryGetStringField(TEXT("graph_name"), Request.GraphName);
		}
	}

	Payload->TryGetStringField(TEXT("patch_type"), Request.PatchType);
	Payload->TryGetStringField(TEXT("property_descriptor_id"), Request.PropertyDescriptorId);
	Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
	BlueprintHelperPatchExternalGraph::ReadStringValueField(Payload, Request.Value);

	const TSharedPtr<FJsonObject>* AnchorObject = nullptr;
	if (!Payload->TryGetObjectField(TEXT("anchor"), AnchorObject) ||
		!AnchorObject || !AnchorObject->IsValid() ||
		!FBlueprintHelperExternalGraphAnchor::FromJson(*AnchorObject, Request.Anchor, Request.AnchorParseError))
	{
		FString CompactAnchorParseError;
		if (AnchorObject && AnchorObject->IsValid() &&
			FBlueprintHelperExternalCompactAnchor::FromJson(*AnchorObject, Request.CompactAnchor, CompactAnchorParseError))
		{
			Request.bHasCompactAnchor = true;
			Request.AnchorParseError.Reset();
		}
		else if (Request.AnchorParseError.IsEmpty())
		{
			Request.AnchorParseError = CompactAnchorParseError.IsEmpty()
				? TEXT("external_anchor_schema_unsupported")
				: CompactAnchorParseError;
		}
	}
	else
	{
		Request.bHasExpandedAnchor = true;
	}

	const TSharedPtr<FJsonObject>* ExpectedOldState = nullptr;
	if (Payload->TryGetObjectField(TEXT("expected_old_state"), ExpectedOldState) && ExpectedOldState && ExpectedOldState->IsValid())
	{
		Request.bExpectedOldStateProvided = (*ExpectedOldState)->TryGetStringField(TEXT("value"), Request.ExpectedOldValue);
	}

	return Request;
}

bool FBlueprintHelperPatchExternalGraphService::Preflight(
	const FPatchExternalGraphRequest& Request,
	FPatchExternalGraphContext& Context,
	FPatchExternalGraphPreflightResult& OutResult) const
{
	if (Request.AssetPath.IsEmpty())
	{
		BlueprintHelperPatchExternalGraph::AddError(
			OutResult,
			TEXT("target_blueprint_not_found"),
			TEXT("patch_external_graph requires target.asset_path."),
			TEXT("target.asset_path"),
			TEXT("payload.target.asset_path"));
		return false;
	}
	if (Request.GraphName.IsEmpty())
	{
		BlueprintHelperPatchExternalGraph::AddError(
			OutResult,
			TEXT("target_graph_not_found"),
			TEXT("patch_external_graph requires target.graph."),
			TEXT("target.graph"),
			TEXT("payload.target.graph"));
		return false;
	}
	if (Request.PatchType != TEXT("set_external_pin_default") &&
		Request.PatchType != TEXT("set_external_node_comment") &&
		Request.PatchType != TEXT("set_external_node_property"))
	{
		BlueprintHelperPatchExternalGraph::AddError(
			OutResult,
			TEXT("unsupported_external_patch_type"),
			TEXT("patch_external_graph only supports set_external_pin_default, set_external_node_comment, and set_external_node_property."),
			TEXT("patch_type"),
			TEXT("payload.patch_type"));
		return false;
	}
	if (!Request.bHasExpandedAnchor && !Request.bHasCompactAnchor)
	{
		BlueprintHelperPatchExternalGraph::AddError(
			OutResult,
			Request.AnchorParseError,
			TEXT("patch_external_graph requires a BlueprintHelper.ExternalGraphAnchor.v1 anchor or compact external_node anchor."),
			TEXT("anchor"),
			TEXT("payload.anchor"));
		return false;
	}
	if (Request.bHasExpandedAnchor && Request.Anchor.SemanticRole != EBlueprintHelperExternalGraphAnchorRole::Node)
	{
		BlueprintHelperPatchExternalGraph::AddError(
			OutResult,
			TEXT("external_anchor_role_unsupported"),
			TEXT("patch_external_graph requires a semantic_role=node external anchor."),
			TEXT("anchor.semantic_role"),
			TEXT("payload.anchor.semantic_role"));
		return false;
	}
	if (Request.bHasCompactAnchor && Request.CompactAnchor.Type != EBlueprintHelperExternalCompactAnchorType::Node)
	{
		BlueprintHelperPatchExternalGraph::AddError(
			OutResult,
			TEXT("external_anchor_ref_unsupported"),
			TEXT("patch_external_graph compact anchors must use anchor_type=external_node."),
			TEXT("anchor.anchor_type"),
			TEXT("payload.anchor.anchor_type"));
		return false;
	}
	if (Request.bHasCompactAnchor && Request.PatchType == TEXT("set_external_pin_default"))
	{
		BlueprintHelperPatchExternalGraph::AddError(
			OutResult,
			TEXT("anchor_pin_required"),
			TEXT("set_external_pin_default requires a node external anchor with pin_name."),
			TEXT("anchor.pin_name"),
			TEXT("payload.anchor.pin_name"));
		return false;
	}
	if (Request.bHasExpandedAnchor &&
		(!Request.Anchor.AssetPath.Equals(Request.AssetPath, ESearchCase::IgnoreCase) ||
			!Request.Anchor.GraphName.Equals(Request.GraphName, ESearchCase::IgnoreCase)))
	{
		BlueprintHelperPatchExternalGraph::AddError(
			OutResult,
			TEXT("external_anchor_target_mismatch"),
			TEXT("patch_external_graph target must match the external anchor asset_path and graph_name."),
			TEXT("anchor"),
			TEXT("payload.anchor"));
		return false;
	}
	if (!Request.bExpectedOldStateProvided)
	{
		BlueprintHelperPatchExternalGraph::AddError(
			OutResult,
			TEXT("expected_old_state_required"),
			TEXT("patch_external_graph requires expected_old_state.value."),
			TEXT("expected_old_state.value"),
			TEXT("payload.expected_old_state.value"));
		return false;
	}

	Context.Blueprint = BlueprintHelperPatchExternalGraph::FindBlueprint(Request.AssetPath);
	Context.Graph = BlueprintHelperPatchExternalGraph::FindGraphByName(Context.Blueprint, Request.GraphName);
	if (!Context.Blueprint || !Context.Graph)
	{
		BlueprintHelperPatchExternalGraph::AddError(
			OutResult,
			Context.Blueprint ? TEXT("target_graph_not_found") : TEXT("target_blueprint_not_found"),
			Context.Blueprint ? TEXT("Target graph was not found.") : TEXT("Target blueprint was not found."),
			Context.Blueprint ? TEXT("target.graph") : TEXT("target.asset_path"),
			Context.Blueprint ? TEXT("payload.target.graph") : TEXT("payload.target.asset_path"));
		return false;
	}

	FString AnchorResolveError;
	const FBlueprintHelperExternalGraphAnchorResolver AnchorResolver;
	const bool bResolvedNode = Request.bHasExpandedAnchor
		? AnchorResolver.ResolveNode(Request.Anchor, Context.Node, AnchorResolveError)
		: AnchorResolver.ResolveCompactNode(Request.AssetPath, Request.GraphName, Request.CompactAnchor, Context.Node, AnchorResolveError);
	if (!bResolvedNode)
	{
		BlueprintHelperPatchExternalGraph::AddConflict(
			OutResult,
			AnchorResolveError.IsEmpty() ? TEXT("external_anchor_resolve_failed") : AnchorResolveError,
			TEXT("External node anchor is stale or cannot be resolved."),
			TEXT("anchor"),
			TEXT("payload.anchor"));
		return false;
	}

	if (Request.PatchType == TEXT("set_external_node_property"))
	{
		if (Request.PropertyDescriptorId.IsEmpty())
		{
			BlueprintHelperPatchExternalGraph::AddError(
				OutResult,
				TEXT("external_property_descriptor_not_found"),
				TEXT("set_external_node_property requires property_descriptor_id."),
				TEXT("property_descriptor_id"),
				TEXT("payload.property_descriptor_id"));
			return false;
		}

		const FBlueprintHelperExternalNodePropertyDescriptor* Descriptor =
			FBlueprintHelperExternalNodePropertyDescriptorRegistry::Find(Request.PropertyDescriptorId);
		if (!Descriptor)
		{
			BlueprintHelperPatchExternalGraph::AddError(
				OutResult,
				TEXT("external_property_descriptor_not_found"),
				TEXT("External node property descriptor was not found."),
				TEXT("property_descriptor_id"),
				TEXT("payload.property_descriptor_id"));
			return false;
		}
		if (!Descriptor->IsWritable())
		{
			BlueprintHelperPatchExternalGraph::AddError(
				OutResult,
				TEXT("external_property_descriptor_not_allowed"),
				Descriptor->DisabledReason.IsEmpty()
					? TEXT("External node property descriptor is not writable.")
					: Descriptor->DisabledReason,
				TEXT("property_descriptor_id"),
				TEXT("payload.property_descriptor_id"));
			return false;
		}
		Context.PropertyDescriptor = *Descriptor;
		Context.bHasPropertyDescriptor = true;
	}
	else if (!Request.PropertyDescriptorId.IsEmpty())
	{
		BlueprintHelperPatchExternalGraph::AddError(
			OutResult,
			TEXT("external_property_descriptor_not_allowed"),
			TEXT("property_descriptor_id is only valid for set_external_node_property."),
			TEXT("property_descriptor_id"),
			TEXT("payload.property_descriptor_id"));
		return false;
	}

	if (Request.PatchType == TEXT("set_external_pin_default"))
	{
		if (Request.Anchor.PinName.IsEmpty())
		{
			BlueprintHelperPatchExternalGraph::AddError(
				OutResult,
				TEXT("anchor_pin_required"),
				TEXT("set_external_pin_default requires anchor.pin_name."),
				TEXT("anchor.pin_name"),
				TEXT("payload.anchor.pin_name"));
			return false;
		}
		Context.Pin = BlueprintHelperPatchExternalGraph::FindPinByAnchor(Context.Node, Request.Anchor);
		if (!Context.Pin)
		{
			BlueprintHelperPatchExternalGraph::AddConflict(
				OutResult,
				TEXT("external_anchor_pin_not_found"),
				TEXT("set_external_pin_default target pin was not found."),
				TEXT("anchor.pin_name"),
				TEXT("payload.anchor.pin_name"));
			return false;
		}
		Context.BeforeValue = Context.Pin->DefaultValue;
	}
	else if (Request.PatchType == TEXT("set_external_node_comment") ||
		(Context.bHasPropertyDescriptor && Context.PropertyDescriptor.FieldKind == TEXT("node_comment")))
	{
		Context.BeforeValue = Context.Node ? Context.Node->NodeComment : FString();
	}
	else
	{
		BlueprintHelperPatchExternalGraph::AddError(
			OutResult,
			TEXT("external_property_descriptor_not_allowed"),
			TEXT("External node property descriptor has no active mutation handler."),
			TEXT("property_descriptor_id"),
			TEXT("payload.property_descriptor_id"));
		return false;
	}

	if (Context.BeforeValue != Request.ExpectedOldValue)
	{
		BlueprintHelperPatchExternalGraph::AddConflict(
			OutResult,
			TEXT("expected_old_state_mismatch"),
			FString::Printf(
				TEXT("expected_old_state.value '%s' does not match current value '%s'."),
				*Request.ExpectedOldValue,
				*Context.BeforeValue),
			TEXT("expected_old_state.value"),
			TEXT("payload.expected_old_state.value"));
		return false;
	}

	return OutResult.bPassed;
}

FBlueprintHelperToolResultBase FBlueprintHelperPatchExternalGraphService::Execute(const TSharedRef<FJsonObject>& Payload) const
{
	const FPatchExternalGraphRequest Request = ParseRequest(Payload);
	return FBlueprintHelperGraphWriteUnitOfWork::RunExistingOperation(
		Request.bDryRun
			? EBlueprintHelperGraphWriteUnitOfWorkMode::Preview
			: EBlueprintHelperGraphWriteUnitOfWorkMode::Execute,
		TEXT("patch_external_graph"),
		TEXT("patch_external_graph"),
		EBlueprintHelperGraphBodyKind::K2ExternalBody,
		[this, &Request]()
		{
			return Request.bDryRun ? ExecuteDryRun(Request) : ExecuteWrite(Request);
		});
}

FBlueprintHelperToolResultBase FBlueprintHelperPatchExternalGraphService::ExecuteDryRun(
	const FPatchExternalGraphRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FPatchExternalGraphContext Context;
	FPatchExternalGraphPreflightResult PreflightResult;
	Preflight(Request, Context, PreflightResult);

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
		BlueprintHelperPatchExternalGraph::OperationName,
		TraceId);
	Result.Data = BlueprintHelperPatchExternalGraph::MakeDryRunData(PreflightResult, Request, Context);
	Result.bOk = PreflightResult.bPassed;
	if (!PreflightResult.bPassed)
	{
		Result.Status = EBlueprintHelperToolStatus::Failed;
		Result.Error = BlueprintHelperPatchExternalGraph::MakeErrorFromPreflight(PreflightResult);
	}
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperPatchExternalGraphService::ExecuteWrite(
	const FPatchExternalGraphRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FPatchExternalGraphContext Context;
	FPatchExternalGraphPreflightResult PreflightResult;
	if (!Preflight(Request, Context, PreflightResult))
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			BlueprintHelperPatchExternalGraph::OperationName,
			TraceId,
			BlueprintHelperPatchExternalGraph::MakeErrorFromPreflight(PreflightResult));
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Patch External Graph")),
		Context.Blueprint);
	Mutation.Modify(Context.Graph);
	if (Context.Node)
	{
		Context.Node->Modify();
	}

	bool bChanged = false;
	FString ApplyError;
	if (!ApplyPatch(Request, Context, bChanged, ApplyError))
	{
		Mutation.Rollback();
		return FBlueprintHelperToolResultBuilder::Failure(
			BlueprintHelperPatchExternalGraph::OperationName,
			TraceId,
			BlueprintHelperPatchExternalGraph::MakeToolError(
				TEXT("external_patch_apply_failed"),
				EBlueprintHelperToolStage::Execute,
				ApplyError.IsEmpty() ? TEXT("patch_external_graph apply failed.") : ApplyError,
				TEXT("payload"),
				EBlueprintHelperRollbackResult::RolledBack));
	}

	if (bChanged)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Context.Blueprint);
		if (Context.Blueprint && Context.Blueprint->GetOutermost())
		{
			Context.Blueprint->GetOutermost()->MarkPackageDirty();
		}
	}
	Mutation.Commit();

	FBlueprintHelperToolResultBase Result = bChanged
		? FBlueprintHelperToolResultBuilder::Applied(BlueprintHelperPatchExternalGraph::OperationName, TraceId)
		: FBlueprintHelperToolResultBuilder::NoOp(BlueprintHelperPatchExternalGraph::OperationName, TraceId);
	Result.Data = BlueprintHelperPatchExternalGraph::MakeDryRunData(PreflightResult, Request, Context);
	FBlueprintHelperValidationSummary Validation;
	Validation.bShouldCompile = bChanged;
	Validation.bShouldSave = bChanged;
	Result.Validation = Validation;
	return Result;
}

bool FBlueprintHelperPatchExternalGraphService::ApplyPatch(
	const FPatchExternalGraphRequest& Request,
	const FPatchExternalGraphContext& Context,
	bool& bOutChanged,
	FString& OutError) const
{
	bOutChanged = false;
	if (Request.PatchType == TEXT("set_external_pin_default"))
	{
		FBlueprintHelperGraphWriteMutationIntent Intent;
		Intent.Kind = EBlueprintHelperGraphWriteMutationIntentKind::SetPinDefault;
		Intent.IntentId = TEXT("patch_external_pin_default");
		Intent.Target.Pin = Context.Pin;
		Intent.DefaultValue = Request.Value;

		TArray<FString> Unresolved;
		const FBlueprintGenerateResult Result =
			FBlueprintHelperGraphWriteMutationCoordinator::ExecuteIntents(Context.Graph, {Intent}, Unresolved);
		bOutChanged = Result.AppliedDefaultValueCount > 0;
		if (!Result.bSucceed)
		{
			OutError = Unresolved.Num() > 0 ? Unresolved[0] : Result.Message;
			return false;
		}
		return true;
	}

	if (Request.PatchType == TEXT("set_external_node_comment") ||
		(Request.PatchType == TEXT("set_external_node_property") &&
			Context.bHasPropertyDescriptor &&
			Context.PropertyDescriptor.FieldKind == TEXT("node_comment")))
	{
		if (!Context.Node)
		{
			OutError = TEXT("external_anchor_node_not_found");
			return false;
		}
		if (Context.Node->NodeComment == Request.Value)
		{
			return true;
		}
		Context.Node->Modify();
		Context.Node->NodeComment = Request.Value;
		bOutChanged = true;
		if (Context.Graph)
		{
			Context.Graph->NotifyGraphChanged();
		}
		return true;
	}

	OutError = TEXT("unsupported_external_patch_type");
	return false;
}
