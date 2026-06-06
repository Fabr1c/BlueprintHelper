// BlueprintHelper Service Layer - PatchBlueprintGraph implementation

#include "Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteBlockScopedResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDebugData.h"
#include "Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.h"
#include "Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationIntent.h"
#include "Systems/ToolClusters/GraphWrite/Patch/BlueprintHelperOwnedGraphPatchPolicy.h"
#include "Systems/ToolClusters/GraphWrite/Patch/BlueprintHelperPatchOperationHandlerRegistry.h"
#include "Systems/ToolClusters/GraphWrite/UnitOfWork/BlueprintHelperGraphWriteUnitOfWork.h"
#include "Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"
#include "Shared/GraphWrite/BlueprintHelperReplaceGraphTypes.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class FBlueprintHelperPatchBlueprintGraphServiceLocalUtils
{
public:
	static FString MakeStableNodeRef(UEdGraphNode* Node)
	{
		if (!Node)
		{
			return FString();
		}
		return Node->NodeGuid.IsValid()
			? Node->NodeGuid.ToString(EGuidFormats::Digits)
			: Node->GetName();
	}

	static bool DoesNodeRefMatch(UEdGraphNode* Node, const FString& ExpectedRef)
	{
		if (ExpectedRef.IsEmpty())
		{
			return true;
		}
		if (!Node)
		{
			return false;
		}
		if (ExpectedRef.Equals(Node->GetName(), ESearchCase::IgnoreCase))
		{
			return true;
		}
		if (Node->NodeGuid.IsValid() &&
			ExpectedRef.Equals(Node->NodeGuid.ToString(EGuidFormats::Digits), ESearchCase::IgnoreCase))
		{
			return true;
		}
		return ExpectedRef.Equals(Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString(), ESearchCase::IgnoreCase);
	}

	static bool IsP0DOwnedPatchType(const EBlueprintHelperPatchType PatchType)
	{
		return PatchType == EBlueprintHelperPatchType::ConnectPins ||
			PatchType == EBlueprintHelperPatchType::DisconnectLink ||
			PatchType == EBlueprintHelperPatchType::ReplaceLink ||
			PatchType == EBlueprintHelperPatchType::DeleteOwnedNode;
	}

	static bool LooksLikeReadViewArrayLocator(const FString& Value)
	{
		return Value.Contains(TEXT("nodes[")) ||
			Value.Contains(TEXT("pins[")) ||
			Value.Contains(TEXT("links["));
	}
};

FBlueprintHelperPatchBlueprintGraphService::FBlueprintHelperPatchBlueprintGraphService(
	const FBlueprintHelperGraphResolver& InResolver,
	const FBlueprintHelperLogicJsonPathService& InPathService)
	: Resolver(InResolver)
	, PathService(InPathService)
{
}

// 鈹€鈹€鈹€ 鍏叡鍏ュ彛 鈹€鈹€鈹€

FBlueprintHelperToolResultBase FBlueprintHelperPatchBlueprintGraphService::Execute(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FPatchRequest Request = ParseRequest(Payload);

	return FBlueprintHelperGraphWriteUnitOfWork::RunExistingOperation(
		Request.bDryRun
			? EBlueprintHelperGraphWriteUnitOfWorkMode::Preview
			: EBlueprintHelperGraphWriteUnitOfWorkMode::Execute,
		TEXT("patch_blueprint_graph"),
		TEXT("patch_owned_graph"),
		EBlueprintHelperGraphBodyKind::K2BlockImplementation,
		[this, &Request]()
		{
			return Request.bDryRun ? ExecuteDryRun(Request) : ExecuteWrite(Request);
		});
}

// 鈹€鈹€鈹€ 瑙ｆ瀽 鈹€鈹€鈹€

FBlueprintHelperPatchBlueprintGraphService::FPatchRequest
FBlueprintHelperPatchBlueprintGraphService::ParseRequest(const TSharedPtr<FJsonObject>& Payload) const
{
	FPatchRequest Req;

	if (!Payload.IsValid()) return Req;

	const TSharedPtr<FJsonObject>* TargetObj = nullptr;
	if (Payload->TryGetObjectField(TEXT("target"), TargetObj) && TargetObj->IsValid())
	{
		(*TargetObj)->TryGetStringField(TEXT("asset_path"), Req.AssetPath);
		(*TargetObj)->TryGetStringField(TEXT("graph"), Req.GraphName);
		FString ScopeStr;
		if ((*TargetObj)->TryGetStringField(TEXT("patch_scope"), ScopeStr))
			ParsePatchScope(ScopeStr, Req.PatchScope);
	}

	FString TypeStr;
	if (Payload->TryGetStringField(TEXT("patch_type"), TypeStr))
		ParsePatchType(TypeStr, Req.PatchType);

	Payload->TryGetBoolField(TEXT("dry_run"), Req.bDryRun);

	const TSharedPtr<FJsonObject>* PatchedRefObj = nullptr;
	if (Payload->TryGetObjectField(TEXT("patched_ref"), PatchedRefObj) && PatchedRefObj->IsValid())
	{
		(*PatchedRefObj)->TryGetStringField(TEXT("block_id"), Req.BlockId);
		(*PatchedRefObj)->TryGetStringField(TEXT("group_entry_node_path"), Req.GroupEntryNodePath);
		(*PatchedRefObj)->TryGetStringField(TEXT("node_ref"), Req.NodeRef);
		(*PatchedRefObj)->TryGetStringField(TEXT("pin_ref"), Req.PinRef);
		(*PatchedRefObj)->TryGetStringField(TEXT("link_ref"), Req.LinkRef);
		(*PatchedRefObj)->TryGetStringField(TEXT("node_path"), Req.NodePath);
		(*PatchedRefObj)->TryGetStringField(TEXT("pin_path"), Req.PinPath);
		(*PatchedRefObj)->TryGetStringField(TEXT("link_path"), Req.LinkPath);
	}

	const TSharedPtr<FJsonObject>* PatchObj = nullptr;
	if (Payload->TryGetObjectField(TEXT("patch"), PatchObj) && PatchObj->IsValid())
	{
		Req.PatchPayload = *PatchObj;
		(*PatchObj)->TryGetStringField(TEXT("source_block_id"), Req.SourceBlockId);
		(*PatchObj)->TryGetStringField(TEXT("source_node_ref"), Req.SourceNodeRef);
		(*PatchObj)->TryGetStringField(TEXT("source_pin_ref"), Req.SourcePinRef);
		(*PatchObj)->TryGetStringField(TEXT("source_node_path"), Req.SourceNodePath);
		(*PatchObj)->TryGetStringField(TEXT("source_pin_path"), Req.SourcePinPath);
		(*PatchObj)->TryGetStringField(TEXT("replacement_block_id"), Req.ReplacementBlockId);
		(*PatchObj)->TryGetStringField(TEXT("replacement_node_ref"), Req.ReplacementNodeRef);
		(*PatchObj)->TryGetStringField(TEXT("replacement_pin_ref"), Req.ReplacementPinRef);
		(*PatchObj)->TryGetStringField(TEXT("replacement_node_path"), Req.ReplacementNodePath);
		(*PatchObj)->TryGetStringField(TEXT("replacement_pin_path"), Req.ReplacementPinPath);
		(*PatchObj)->TryGetBoolField(TEXT("break_links"), Req.bDeleteBreakLinks);
		(*PatchObj)->TryGetBoolField(TEXT("allow_entry_node"), Req.bDeleteAllowEntryNode);
		(*PatchObj)->TryGetBoolField(TEXT("allow_lifecycle_root"), Req.bDeleteAllowLifecycleRoot);

		const TSharedPtr<FJsonObject>* DeletePolicyObj = nullptr;
		if ((*PatchObj)->TryGetObjectField(TEXT("delete_policy"), DeletePolicyObj) && DeletePolicyObj && DeletePolicyObj->IsValid())
		{
			(*DeletePolicyObj)->TryGetBoolField(TEXT("break_links"), Req.bDeleteBreakLinks);
			(*DeletePolicyObj)->TryGetBoolField(TEXT("allow_entry_node"), Req.bDeleteAllowEntryNode);
			(*DeletePolicyObj)->TryGetBoolField(TEXT("allow_lifecycle_root"), Req.bDeleteAllowLifecycleRoot);
		}
	}

	const TSharedPtr<FJsonObject>* LogicSpecObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("logic_spec"), LogicSpecObject) && LogicSpecObject && LogicSpecObject->IsValid())
	{
		Req.LogicSpec = *LogicSpecObject;
	}

	const TSharedPtr<FJsonObject>* ExpectObj = nullptr;
	if (Payload->TryGetObjectField(TEXT("expected_old_state"), ExpectObj) && ExpectObj->IsValid())
	{
		Req.bExpectedOldStateProvided = true;
		(*ExpectObj)->TryGetStringField(TEXT("value"), Req.ExpectedOldValue);
		(*ExpectObj)->TryGetStringField(TEXT("source_node_ref"), Req.ExpectedSourceNodeRef);
		(*ExpectObj)->TryGetStringField(TEXT("source_pin_ref"), Req.ExpectedSourcePinRef);
		(*ExpectObj)->TryGetStringField(TEXT("target_node_ref"), Req.ExpectedTargetNodeRef);
		(*ExpectObj)->TryGetStringField(TEXT("target_pin_ref"), Req.ExpectedTargetPinRef);
		(*ExpectObj)->TryGetStringField(TEXT("node_ref"), Req.ExpectedNodeRef);
		(*ExpectObj)->TryGetStringField(TEXT("node_class"), Req.ExpectedNodeClass);
	}

	return Req;
}

// 鈹€鈹€鈹€ Preflight 鈹€鈹€鈹€

FBlueprintHelperPatchBlueprintGraphService::FPatchPreflightResult
FBlueprintHelperPatchBlueprintGraphService::Preflight(
	const FPatchRequest& Request, UBlueprint* Blueprint, UEdGraph* Graph, FBlueprintHelperResolvedPatchTarget& OutTarget) const
{
	FPatchPreflightResult Result;

	if (!PreflightLogicSpec(Request, Blueprint, Result))
	{
		return Result;
	}

	if (Request.PatchType == EBlueprintHelperPatchType::RenameLocalVariableRef ||
		Request.PatchType == EBlueprintHelperPatchType::SetCallTarget)
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("unsupported_patch_type"));
		Result.Conflicts.Add({TEXT("unsupported_patch_type"),
			FString::Printf(TEXT("patch_type '%s' is not supported."), PatchTypeToString(Request.PatchType)),
			TEXT("patch_type"),
			TEXT("preflight")});
		return Result;
	}

	if (!PreflightOwnedPatchContract(Request, Result))
	{
		return Result;
	}

	// Resolve target node.
	FBlueprintHelperGraphWriteAnchorRef Anchor;
	Anchor.BlockId = Request.BlockId;
	Anchor.GroupEntryNodePath = Request.GroupEntryNodePath;
	Anchor.NodeRef = Request.NodeRef;
	Anchor.PinRef = Request.PinRef;
	Anchor.LinkRef = Request.LinkRef;
	Anchor.NodePath = Request.NodePath;
	Anchor.PinPath = Request.PinPath;
	Anchor.LinkPath = Request.LinkPath;

	FBlueprintHelperPatchResolveError NodeError;
	UEdGraphNode* Node = nullptr;
	if (!FBlueprintHelperGraphWriteBlockScopedResolver::ResolveNode(PathService, Graph, Anchor, Node, NodeError))
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(NodeError.Code);
		Result.Conflicts.Add({NodeError.Code, NodeError.Message, NodeError.Target, TEXT("patched_ref")});
		return Result;
	}
	OutTarget.Node = Node;
	OutTarget.PatchedRef.GraphId = Graph->GetName();
	OutTarget.PatchedRef.BlockId = Request.BlockId;
	OutTarget.PatchedRef.NodeRef = Request.NodeRef;
	if (!Request.NodePath.IsEmpty()) { OutTarget.PatchedRef.NodePath = Request.NodePath; }

	auto ApplyPolicyFailure = [&Result](const FBlueprintHelperOwnedGraphPatchPolicyResult& PolicyResult)
	{
		if (PolicyResult.bPassed)
		{
			return false;
		}

		Result.bPassed = false;
		Result.BlockedBy.Add(PolicyResult.Code);
		Result.Conflicts.Add({
			PolicyResult.Code,
			PolicyResult.Message,
			PolicyResult.Field,
			PolicyResult.Field
		});
		return true;
	};

	// Resolve target pin when required.
	if (Request.PatchType == EBlueprintHelperPatchType::SetPinDefault ||
		Request.PatchType == EBlueprintHelperPatchType::ConnectPins ||
		Request.PatchType == EBlueprintHelperPatchType::DisconnectLink ||
		Request.PatchType == EBlueprintHelperPatchType::ReplaceLink)
	{
		FBlueprintHelperPatchResolveError PinError;
		UEdGraphPin* Pin = nullptr;
		if (!FBlueprintHelperGraphWriteBlockScopedResolver::ResolvePin(PathService, Graph, Node, Anchor, Pin, PinError))
		{
			Result.bPassed = false;
			Result.BlockedBy.Add(PinError.Code);
			Result.Conflicts.Add({PinError.Code, PinError.Message, PinError.Target, TEXT("patched_ref")});
			return Result;
		}
		OutTarget.Pin = Pin;
		OutTarget.PatchedRef.PinRef = Request.PinRef;
		if (!Request.PinPath.IsEmpty()) { OutTarget.PatchedRef.PinPath = Request.PinPath; }

		FBlueprintHelperOwnedGraphPatchPolicyResult TargetPinPolicy =
			FBlueprintHelperOwnedGraphPatchPolicy::RequireOwnedPinInBlock(Pin, Request.BlockId, TEXT("patched_ref.pin_ref"));
		if (ApplyPolicyFailure(TargetPinPolicy))
		{
			return Result;
		}
	}

	if (Request.PatchType == EBlueprintHelperPatchType::ConnectPins)
	{
		UEdGraphPin* SourcePin = nullptr;
		FString SourceError;
		FString SourceField;
		FString SourceCode;
		if (!ResolvePatchSourcePin(Graph, Request, SourcePin, SourceError, &SourceField, &SourceCode))
		{
			Result.bPassed = false;
			Result.BlockedBy.Add(SourceCode.IsEmpty() ? TEXT("source_endpoint_invalid") : SourceCode);
			Result.Conflicts.Add({
				SourceCode.IsEmpty() ? TEXT("source_endpoint_invalid") : SourceCode,
				SourceError,
				SourceField.IsEmpty() ? TEXT("patch") : SourceField,
				SourceField.IsEmpty() ? TEXT("patch") : SourceField
			});
			return Result;
		}
		OutTarget.SourcePin = SourcePin;
	}

	// Resolve target link when required.
	if (Request.PatchType == EBlueprintHelperPatchType::DisconnectLink ||
		Request.PatchType == EBlueprintHelperPatchType::ReplaceLink)
	{
		FBlueprintHelperPatchResolveError LinkError;
		if (!FBlueprintHelperGraphWriteBlockScopedResolver::ResolveLink(PathService, Graph, Anchor, OutTarget.Link, LinkError))
		{
			Result.bPassed = false;
			Result.BlockedBy.Add(LinkError.Code);
			Result.Conflicts.Add({LinkError.Code, LinkError.Message, LinkError.Target, TEXT("patched_ref")});
			return Result;
		}
		OutTarget.PatchedRef.LinkRef = Request.LinkRef;
		if (!Request.LinkPath.IsEmpty()) { OutTarget.PatchedRef.LinkPath = Request.LinkPath; }

		FBlueprintHelperOwnedGraphPatchPolicyResult LinkPolicy =
			FBlueprintHelperOwnedGraphPatchPolicy::RequireOwnedLinkInBlock(OutTarget.Link, Request.BlockId, TEXT("patched_ref.link_ref"));
		if (ApplyPolicyFailure(LinkPolicy))
		{
			return Result;
		}

		const bool bExpectedLinkEndpointProvided =
			!Request.ExpectedSourceNodeRef.IsEmpty() ||
			!Request.ExpectedSourcePinRef.IsEmpty() ||
			!Request.ExpectedTargetNodeRef.IsEmpty() ||
			!Request.ExpectedTargetPinRef.IsEmpty();
		if (Request.bExpectedOldStateProvided && bExpectedLinkEndpointProvided)
		{
			const FString CurrentSourceNodeRef =
				FBlueprintHelperPatchBlueprintGraphServiceLocalUtils::MakeStableNodeRef(OutTarget.Link.SourceNode);
			const FString CurrentSourcePinRef = OutTarget.Link.SourcePin ? OutTarget.Link.SourcePin->PinName.ToString() : FString();
			const FString CurrentTargetNodeRef =
				FBlueprintHelperPatchBlueprintGraphServiceLocalUtils::MakeStableNodeRef(OutTarget.Link.TargetNode);
			const FString CurrentTargetPinRef = OutTarget.Link.TargetPin ? OutTarget.Link.TargetPin->PinName.ToString() : FString();
			const bool bMatches =
				FBlueprintHelperPatchBlueprintGraphServiceLocalUtils::DoesNodeRefMatch(OutTarget.Link.SourceNode, Request.ExpectedSourceNodeRef) &&
				(Request.ExpectedSourcePinRef.IsEmpty() || Request.ExpectedSourcePinRef.Equals(CurrentSourcePinRef, ESearchCase::IgnoreCase)) &&
				FBlueprintHelperPatchBlueprintGraphServiceLocalUtils::DoesNodeRefMatch(OutTarget.Link.TargetNode, Request.ExpectedTargetNodeRef) &&
				(Request.ExpectedTargetPinRef.IsEmpty() || Request.ExpectedTargetPinRef.Equals(CurrentTargetPinRef, ESearchCase::IgnoreCase));
			if (!bMatches)
			{
				Result.bPassed = false;
				Result.BlockedBy.Add(TEXT("owned_patch_link_state_mismatch"));
				Result.Conflicts.Add({TEXT("owned_patch_link_state_mismatch"),
					FString::Printf(
						TEXT("expected_old_state link endpoints do not match current link '%s.%s->%s.%s'."),
						*CurrentSourceNodeRef,
						*CurrentSourcePinRef,
						*CurrentTargetNodeRef,
						*CurrentTargetPinRef),
					TEXT("expected_old_state"),
					TEXT("expected_old_state")});
				return Result;
			}
		}
	}

	if (Request.PatchType == EBlueprintHelperPatchType::ReplaceLink)
	{
		UEdGraphPin* ReplacementPin = nullptr;
		FString ReplacementError;
		FString ReplacementField;
		FString ReplacementCode;
		if (!ResolvePatchReplacementPin(Graph, Request, ReplacementPin, ReplacementError, &ReplacementField, &ReplacementCode))
		{
			Result.bPassed = false;
			Result.BlockedBy.Add(ReplacementCode.IsEmpty() ? TEXT("owned_patch_replacement_ref_required") : ReplacementCode);
			Result.Conflicts.Add({
				ReplacementCode.IsEmpty() ? TEXT("owned_patch_replacement_ref_required") : ReplacementCode,
				ReplacementError,
				ReplacementField.IsEmpty() ? TEXT("patch.replacement_ref") : ReplacementField,
				ReplacementField.IsEmpty() ? TEXT("patch.replacement_ref") : ReplacementField
			});
			return Result;
		}
		OutTarget.ReplacementPin = ReplacementPin;
	}

	if (Request.PatchType == EBlueprintHelperPatchType::DeleteOwnedNode)
	{
		FBlueprintHelperOwnedGraphPatchPolicyResult DeletePolicy =
			FBlueprintHelperOwnedGraphPatchPolicy::RequireDeleteAllowed(
				OutTarget.Node,
				Request.BlockId,
				Request.bDeleteBreakLinks,
				Request.bDeleteAllowEntryNode,
				Request.bDeleteAllowLifecycleRoot);
		if (ApplyPolicyFailure(DeletePolicy))
		{
			return Result;
		}
	}

	// expected_old_state 鏍￠獙
	if (Request.bExpectedOldStateProvided && !Request.ExpectedOldValue.IsEmpty())
	{
		FString CurrentValue;
		if (OutTarget.Pin)
		{
			CurrentValue = OutTarget.Pin->DefaultValue;
		}
		Result.BeforeValue = CurrentValue;

		if (CurrentValue != Request.ExpectedOldValue)
		{
			Result.bPassed = false;
			Result.BlockedBy.Add(TEXT("expected_old_state_mismatch"));
			Result.Conflicts.Add({TEXT("expected_old_state_mismatch"),
				FString::Printf(TEXT("expected_old_value '%s' does not match current value '%s'."),
					*Request.ExpectedOldValue, *CurrentValue), TEXT("expected_old_state.value"), TEXT("payload")});
		}
	}

	return Result;
}

// 鈹€鈹€鈹€ DryRun 鈹€鈹€鈹€

bool FBlueprintHelperPatchBlueprintGraphService::PreflightOwnedPatchContract(
	const FPatchRequest& Request,
	FPatchPreflightResult& OutResult) const
{
	if (!FBlueprintHelperPatchBlueprintGraphServiceLocalUtils::IsP0DOwnedPatchType(Request.PatchType))
	{
		return true;
	}

	auto Reject = [&OutResult](const FString& Code, const FString& Message, const FString& Field)
	{
		OutResult.bPassed = false;
		OutResult.BlockedBy.Add(Code);
		OutResult.Conflicts.Add({Code, Message, Field, Field});
		return false;
	};

	if (Request.bExpectedOldStateProvided)
	{
		return Reject(
			TEXT("redundant_owned_patch_expected_old_state"),
			TEXT("P0-D owned graph patches derive old link/node state from target_ref and read_context; expected_old_state is not accepted."),
			TEXT("expected_old_state"));
	}

	auto RejectPathField = [&Reject](const FString& Field, const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return true;
		}
		return Reject(
			TEXT("unsupported_graph_write_anchor"),
			FString::Printf(
				TEXT("P0-D owned graph patches require stable read_context refs; path field '%s' is not accepted."),
				*Field),
			Field);
	};

	if (!RejectPathField(TEXT("patched_ref.node_path"), Request.NodePath) ||
		!RejectPathField(TEXT("patched_ref.pin_path"), Request.PinPath) ||
		!RejectPathField(TEXT("patched_ref.link_path"), Request.LinkPath) ||
		!RejectPathField(TEXT("patch.source_node_path"), Request.SourceNodePath) ||
		!RejectPathField(TEXT("patch.source_pin_path"), Request.SourcePinPath) ||
		!RejectPathField(TEXT("patch.replacement_node_path"), Request.ReplacementNodePath) ||
		!RejectPathField(TEXT("patch.replacement_pin_path"), Request.ReplacementPinPath))
	{
		return false;
	}

	auto RejectArrayLocator = [&Reject](const FString& Field, const FString& Value)
	{
		if (Value.IsEmpty() ||
			!FBlueprintHelperPatchBlueprintGraphServiceLocalUtils::LooksLikeReadViewArrayLocator(Value))
		{
			return true;
		}
		return Reject(
			TEXT("unsupported_graph_write_anchor"),
			FString::Printf(
				TEXT("P0-D owned graph patches require stable read_context refs; read-view array locator '%s' is not accepted."),
				*Value),
			Field);
	};

	return RejectArrayLocator(TEXT("patched_ref.node_ref"), Request.NodeRef) &&
		RejectArrayLocator(TEXT("patched_ref.pin_ref"), Request.PinRef) &&
		RejectArrayLocator(TEXT("patched_ref.link_ref"), Request.LinkRef) &&
		RejectArrayLocator(TEXT("patch.source_node_ref"), Request.SourceNodeRef) &&
		RejectArrayLocator(TEXT("patch.source_pin_ref"), Request.SourcePinRef) &&
		RejectArrayLocator(TEXT("patch.replacement_node_ref"), Request.ReplacementNodeRef) &&
		RejectArrayLocator(TEXT("patch.replacement_pin_ref"), Request.ReplacementPinRef);
}

FBlueprintHelperToolResultBase FBlueprintHelperPatchBlueprintGraphService::ExecuteDryRun(
	const FPatchRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();

	// 瑙ｆ瀽
	FBlueprintHelperGraphTarget Target;
	Target.BlueprintPath = Request.AssetPath;
	FBlueprintHelperDiagnosticSet Diag;
	UBlueprint* BP = Resolver.ResolveBlueprint(Target, Diag, FBlueprintHelperResolvePolicy::Mutation());
	UEdGraph* Graph = nullptr;
	if (BP)
	{
		for (UEdGraph* Page : BP->UbergraphPages)
		{
			if (Page && Page->GetName() == Request.GraphName) { Graph = Page; break; }
		}
		if (!Graph)
		{
			for (UEdGraph* Fn : BP->FunctionGraphs)
			{
				if (Fn && Fn->GetName() == Request.GraphName) { Graph = Fn; break; }
			}
		}
	}

	FBlueprintHelperResolvedPatchTarget ResolvedTarget;
	FPatchPreflightResult PreflightResult;
	if (BP && Graph)
	{
		PreflightResult = Preflight(Request, BP, Graph, ResolvedTarget);
	}
	else
	{
		PreflightResult.bPassed = false;
		PreflightResult.BlockedBy.Add(Graph ? TEXT("target_blueprint_not_found") : TEXT("target_graph_not_found"));
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
		TEXT("patch_blueprint_graph"), TraceId);

	FBlueprintHelperTargetRef TargetRef;
	TargetRef.AssetPath = Request.AssetPath;
	TargetRef.Graph = Request.GraphName;
	Result.Target = TargetRef;

	if (PreflightResult.bPassed)
	{
		FBlueprintHelperPatchDryRunData Data;
		Data.DryRun.Result = TEXT("passed");
		Data.DryRun.bCanExecute = true;
		Result.Data = Data.ToJson();
		FBlueprintHelperGraphFragmentDebugData::AttachToData(Result.Data, PreflightResult.FragmentDebugData);
	}
	else
	{
		FBlueprintHelperPatchDryRunData Data;
		Data.DryRun.Result = TEXT("blocked");
		Data.DryRun.bCanExecute = false;
		Data.DryRun.BlockedBy = PreflightResult.BlockedBy;
		Data.DryRun.Conflicts = PreflightResult.Conflicts;
		Data.DryRun.Errors = PreflightResult.Errors;

		const FBlueprintHelperGraphWriteIssue* FirstIssue = PreflightResult.Conflicts.Num() > 0
			? &PreflightResult.Conflicts[0]
			: (PreflightResult.Errors.Num() > 0 ? &PreflightResult.Errors[0] : nullptr);

		FBlueprintHelperToolError Error;
		Error.Code = PreflightResult.BlockedBy.Num() > 0 ? PreflightResult.BlockedBy[0] : TEXT("preflight_failed");
		Error.Stage = EBlueprintHelperToolStage::Preflight;
		Error.Message = FirstIssue && !FirstIssue->Message.IsEmpty()
			? FirstIssue->Message
			: TEXT("Patch dry-run preflight blocked execution.");
		Error.Field = FirstIssue && !FirstIssue->Source.IsEmpty()
			? FirstIssue->Source
			: (Error.Code == TEXT("target_blueprint_not_found") ? TEXT("target.asset_path") : TEXT("target.graph"));
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;

		Result = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("patch_blueprint_graph"), TraceId, Error);
		Result.Target = TargetRef;
		Result.Data = Data.ToJson();
		FBlueprintHelperGraphFragmentDebugData::AttachToData(Result.Data, PreflightResult.FragmentDebugData);
	}

	return Result;
}

// 鈹€鈹€鈹€ 姝ｅ紡鍐欏叆 鈹€鈹€鈹€

FBlueprintHelperToolResultBase FBlueprintHelperPatchBlueprintGraphService::ExecuteWrite(
	const FPatchRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();

	// 1-2. 瑙ｆ瀽钃濆浘鍜屽浘琛?
	FBlueprintHelperGraphTarget Target;
	Target.BlueprintPath = Request.AssetPath;
	FBlueprintHelperDiagnosticSet Diag;
	UBlueprint* BP = Resolver.ResolveBlueprint(Target, Diag, FBlueprintHelperResolvePolicy::Mutation());
	if (!BP)
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("patch_blueprint_graph"), TraceId,
			{TEXT("target_blueprint_not_found"), EBlueprintHelperToolStage::ResolveTarget,
			 TEXT("target blueprint not found."), false, EBlueprintHelperRollbackResult::NotNeeded});

	UEdGraph* Graph = nullptr;
	for (UEdGraph* Page : BP->UbergraphPages)
		if (Page && Page->GetName() == Request.GraphName) { Graph = Page; break; }
	if (!Graph)
		for (UEdGraph* Fn : BP->FunctionGraphs)
			if (Fn && Fn->GetName() == Request.GraphName) { Graph = Fn; break; }
	if (!Graph)
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("patch_blueprint_graph"), TraceId,
			{TEXT("target_graph_not_found"), EBlueprintHelperToolStage::ResolveTarget,
			 FString::Printf(TEXT("graph %s not found."), *Request.GraphName), false, EBlueprintHelperRollbackResult::NotNeeded});

	// 3. Preflight
	FBlueprintHelperResolvedPatchTarget ResolvedTarget;
	FPatchPreflightResult PreflightResult = Preflight(Request, BP, Graph, ResolvedTarget);
	if (!PreflightResult.bPassed)
	{
		FBlueprintHelperToolError Error;
		Error.Code = PreflightResult.BlockedBy.Num() > 0 ? PreflightResult.BlockedBy[0] : TEXT("preflight_failed");
		Error.Stage = EBlueprintHelperToolStage::Preflight;
		const FBlueprintHelperGraphWriteIssue* FirstIssue = PreflightResult.Conflicts.Num() > 0
			? &PreflightResult.Conflicts[0]
			: (PreflightResult.Errors.Num() > 0 ? &PreflightResult.Errors[0] : nullptr);
		Error.Message = FirstIssue && !FirstIssue->Message.IsEmpty()
			? FirstIssue->Message
			: TEXT("Preflight failed.");
		Error.Field = FirstIssue && !FirstIssue->Source.IsEmpty()
			? FirstIssue->Source
			: (Error.Code == TEXT("target_blueprint_not_found") ? TEXT("target.asset_path") : TEXT("target.graph"));
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		FBlueprintHelperToolResultBase FailResult = FBlueprintHelperToolResultBuilder::Failure(TEXT("patch_blueprint_graph"), TraceId, Error);
		FBlueprintHelperGraphFragmentDebugData::AttachToData(FailResult.Data, PreflightResult.FragmentDebugData);
		return FailResult;
	}

	// 4. 寮€濮嬩慨鏀?
	const bool bShouldRecordReview = Request.PatchType != EBlueprintHelperPatchType::SetNodeComment;
	FString PatchTargetKey;
	FString PatchBeforeSnapshotJson;
	if (bShouldRecordReview && ResolvedTarget.Node)
	{
		const FString NodeGuid = ResolvedTarget.Node->NodeGuid.IsValid()
			? ResolvedTarget.Node->NodeGuid.ToString(EGuidFormats::Digits)
			: FString();
		const FString NodeId = NodeGuid.IsEmpty() ? ResolvedTarget.Node->GetName() : NodeGuid;
		PatchTargetKey = FString::Printf(TEXT("graph:%s:node:%s"), *Request.GraphName, *NodeId);

		FBlueprintHelperReviewAtomicTarget SnapshotTarget;
		SnapshotTarget.AssetPath = Request.AssetPath;
		SnapshotTarget.GraphName = Request.GraphName;
		SnapshotTarget.Surface = EBlueprintHelperReviewSurface::Graph;
		SnapshotTarget.TargetKey = PatchTargetKey;
		SnapshotTarget.TargetKind = TEXT("graph_node");
		SnapshotTarget.VisualGroupKey = PatchTargetKey;
		SnapshotTarget.DisplayLabel = ResolvedTarget.Node->GetName();
		SnapshotTarget.NodeGuid = NodeGuid;

		FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
		FString PatchBeforeSnapshotHash;
		FString PatchBeforeSnapshotError;
		if (!SnapshotService.CaptureTargetSnapshot(SnapshotTarget, PatchBeforeSnapshotJson, PatchBeforeSnapshotHash, PatchBeforeSnapshotError))
		{
			FBlueprintHelperToolError Error;
			Error.Code = TEXT("review_baseline_snapshot_failed");
			Error.Stage = EBlueprintHelperToolStage::Preflight;
			Error.Message = PatchBeforeSnapshotError;
			Error.bRetryable = false;
			Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
			return FBlueprintHelperToolResultBuilder::Failure(TEXT("patch_blueprint_graph"), TraceId, Error);
		}
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Patch Graph")), BP);
	Mutation.Modify(Graph);

	// 5. Apply patch
	bool bChanged = false;
	FString ApplyError;
	if (!ApplyPatch(BP, Graph, Request, ResolvedTarget, bChanged, ApplyError))
	{
		Mutation.Rollback();

		FBlueprintHelperToolError Error;
		Error.Code = Request.PatchType == EBlueprintHelperPatchType::DeleteOwnedNode
			? TEXT("owned_delete_failed")
			: TEXT("link_create_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = ApplyError;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::RolledBack;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("patch_blueprint_graph"), TraceId, Error);
	}

	// 7. 鏍囪淇敼
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	if (BP->GetOutermost()) BP->GetOutermost()->MarkPackageDirty();
	Mutation.Commit();

	// 8. 鎴愬姛缁撴灉
	FBlueprintHelperToolResultBase Success = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("patch_blueprint_graph"), TraceId);

	FBlueprintHelperTargetRef STarget;
	STarget.AssetPath = Request.AssetPath;
	STarget.Graph = Request.GraphName;
	Success.Target = STarget;

	FBlueprintHelperPatchGraphResultData Data;
	Data.PatchResult.PatchedRef = ResolvedTarget.PatchedRef;
	Data.PatchResult.Patch.PatchType = PatchTypeToString(Request.PatchType);
	Data.PatchResult.Patch.bExpectedOldStateProvided = Request.bExpectedOldStateProvided;
	Data.PatchResult.Patch.bChanged = bChanged;
	if (!Request.BlockId.IsEmpty())
	{
		Data.BlockRefs.Add(Request.BlockId);
	}
	Success.Data = Data.ToJson();
	FBlueprintHelperGraphFragmentDebugData::AttachToData(Success.Data, PreflightResult.FragmentDebugData);

	if (!bChanged)
	{
		Success.bModified = false;
		Success.Status = EBlueprintHelperToolStatus::Applied;
	}

	FBlueprintHelperValidationSummary Validation;
	Validation.bShouldCompile = bChanged;
	Validation.bShouldSave = bChanged;
	Success.Validation = Validation;

	return Success;
}

// 鈹€鈹€鈹€ ApplyPatch 鍒嗗彂 鈹€鈹€鈹€

bool FBlueprintHelperPatchBlueprintGraphService::PreflightLogicSpec(
	const FPatchRequest& Request,
	UBlueprint* Blueprint,
	FPatchPreflightResult& OutResult) const
{
	if (!Request.LogicSpec.IsValid())
	{
		return true;
	}

	OutResult.FragmentDebugData = FBlueprintHelperGraphFragmentDebugData::BuildFromLogicSpec(Request.LogicSpec, Blueprint);

	FBlueprintHelperGraphSemanticIR SemanticIR;
	FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(Request.LogicSpec, Blueprint, SemanticIR);
	for (const FBlueprintHelperGraphSemanticDiagnostic& Diagnostic : SemanticIR.Diagnostics)
	{
		if (Diagnostic.Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase))
		{
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(Diagnostic.Code);
			OutResult.Errors.Add({Diagnostic.Code, Diagnostic.Message, Diagnostic.Path, TEXT("logic_spec")});
		}
	}
	return OutResult.bPassed;
}

bool FBlueprintHelperPatchBlueprintGraphService::ResolvePatchSourcePin(
	UEdGraph* Graph,
	const FPatchRequest& Request,
	UEdGraphPin*& OutPin,
	FString& OutError,
	FString* OutField,
	FString* OutCode) const
{
	OutPin = nullptr;
	OutError.Reset();
	if (OutField)
	{
		OutField->Reset();
	}
	if (OutCode)
	{
		OutCode->Reset();
	}

	if (!Graph)
	{
		OutError = TEXT("target_graph_invalid");
		if (OutField)
		{
			*OutField = TEXT("target.graph");
		}
		if (OutCode)
		{
			*OutCode = TEXT("target_graph_invalid");
		}
		return false;
	}

	if (Request.SourceBlockId.IsEmpty())
	{
		OutError = TEXT("connect_pins requires patch.source_block_id.");
		if (OutField)
		{
			*OutField = TEXT("patch.source_block_id");
		}
		if (OutCode)
		{
			*OutCode = TEXT("owned_patch_source_ref_required");
		}
		return false;
	}

	if (!Request.SourceBlockId.Equals(Request.BlockId, ESearchCase::IgnoreCase))
	{
		OutError = FString::Printf(
			TEXT("connect_pins source block '%s' does not match target block '%s'."),
			*Request.SourceBlockId,
			*Request.BlockId);
		if (OutField)
		{
			*OutField = TEXT("patch.source_block_id");
		}
		if (OutCode)
		{
			*OutCode = TEXT("owned_patch_cross_block_disallowed");
		}
		return false;
	}

	if (Request.SourceNodeRef.IsEmpty() && Request.SourceNodePath.IsEmpty())
	{
		OutError = TEXT("connect_pins requires patch.source_node_ref or patch.source_node_path.");
		if (OutField)
		{
			*OutField = TEXT("patch.source_node_ref");
		}
		if (OutCode)
		{
			*OutCode = TEXT("source_node_required");
		}
		return false;
	}

	if (Request.SourcePinRef.IsEmpty() && Request.SourcePinPath.IsEmpty())
	{
		OutError = TEXT("connect_pins requires patch.source_pin_ref or patch.source_pin_path.");
		if (OutField)
		{
			*OutField = TEXT("patch.source_pin_ref");
		}
		if (OutCode)
		{
			*OutCode = TEXT("source_pin_required");
		}
		return false;
	}

	const bool bResolvingNodePath = !Request.SourceNodePath.IsEmpty();
	const bool bResolvingPinPath = !Request.SourcePinPath.IsEmpty();
	const FString SourceNodeIdentifier = bResolvingNodePath ? Request.SourceNodePath : Request.SourceNodeRef;
	const FString SourcePinIdentifier = bResolvingPinPath ? Request.SourcePinPath : Request.SourcePinRef;
	FBlueprintHelperGraphWriteAnchorRef SourceAnchor;
	SourceAnchor.BlockId = Request.SourceBlockId;
	SourceAnchor.GroupEntryNodePath = Request.GroupEntryNodePath;
	SourceAnchor.NodeRef = bResolvingNodePath ? FString() : Request.SourceNodeRef;
	SourceAnchor.PinRef = bResolvingPinPath ? FString() : Request.SourcePinRef;
	SourceAnchor.NodePath = Request.SourceNodePath;
	SourceAnchor.PinPath = Request.SourcePinPath;

	FBlueprintHelperPatchResolveError ResolveError;
	UEdGraphNode* SourceNode = nullptr;
	if (!FBlueprintHelperGraphWriteBlockScopedResolver::ResolveNode(PathService, Graph, SourceAnchor, SourceNode, ResolveError))
	{
		OutError = FString::Printf(TEXT("Unable to resolve source node: %s"), *SourceNodeIdentifier);
		if (OutField)
		{
			*OutField = bResolvingNodePath ? TEXT("patch.source_node_path") : TEXT("patch.source_node_ref");
		}
		if (OutCode)
		{
			*OutCode = TEXT("source_node_not_found");
		}
		return false;
	}

	if (!FBlueprintHelperGraphWriteBlockScopedResolver::ResolvePin(PathService, Graph, SourceNode, SourceAnchor, OutPin, ResolveError))
	{
		OutError = FString::Printf(TEXT("Unable to resolve source pin: %s"), *SourcePinIdentifier);
		if (OutField)
		{
			*OutField = bResolvingPinPath ? TEXT("patch.source_pin_path") : TEXT("patch.source_pin_ref");
		}
		if (OutCode)
		{
			*OutCode = TEXT("source_pin_not_found");
		}
		return false;
	}

	FBlueprintHelperOwnedGraphPatchPolicyResult SourcePolicy =
		FBlueprintHelperOwnedGraphPatchPolicy::RequireOwnedPinInBlock(OutPin, Request.BlockId, TEXT("patch.source_ref"));
	if (!SourcePolicy.bPassed)
	{
		OutError = SourcePolicy.Message;
		if (OutField)
		{
			*OutField = SourcePolicy.Field;
		}
		if (OutCode)
		{
			*OutCode = SourcePolicy.Code;
		}
		return false;
	}

	return true;
}

bool FBlueprintHelperPatchBlueprintGraphService::ResolvePatchReplacementPin(
	UEdGraph* Graph,
	const FPatchRequest& Request,
	UEdGraphPin*& OutPin,
	FString& OutError,
	FString* OutField,
	FString* OutCode) const
{
	OutPin = nullptr;
	OutError.Reset();
	if (OutField)
	{
		OutField->Reset();
	}
	if (OutCode)
	{
		OutCode->Reset();
	}

	if (!Graph)
	{
		OutError = TEXT("target_graph_invalid");
		if (OutField)
		{
			*OutField = TEXT("target.graph");
		}
		if (OutCode)
		{
			*OutCode = TEXT("target_graph_invalid");
		}
		return false;
	}

	if (Request.ReplacementBlockId.IsEmpty())
	{
		OutError = TEXT("replace_link requires patch.replacement_block_id.");
		if (OutField)
		{
			*OutField = TEXT("patch.replacement_block_id");
		}
		if (OutCode)
		{
			*OutCode = TEXT("owned_patch_replacement_ref_required");
		}
		return false;
	}

	if (!Request.ReplacementBlockId.Equals(Request.BlockId, ESearchCase::IgnoreCase))
	{
		OutError = FString::Printf(
			TEXT("replace_link replacement block '%s' does not match target block '%s'."),
			*Request.ReplacementBlockId,
			*Request.BlockId);
		if (OutField)
		{
			*OutField = TEXT("patch.replacement_block_id");
		}
		if (OutCode)
		{
			*OutCode = TEXT("owned_patch_cross_block_disallowed");
		}
		return false;
	}

	if (Request.ReplacementNodeRef.IsEmpty() && Request.ReplacementNodePath.IsEmpty())
	{
		OutError = TEXT("replace_link requires patch.replacement_node_ref or patch.replacement_node_path.");
		if (OutField)
		{
			*OutField = TEXT("patch.replacement_node_ref");
		}
		if (OutCode)
		{
			*OutCode = TEXT("owned_patch_replacement_ref_required");
		}
		return false;
	}

	if (Request.ReplacementPinRef.IsEmpty() && Request.ReplacementPinPath.IsEmpty())
	{
		OutError = TEXT("replace_link requires patch.replacement_pin_ref or patch.replacement_pin_path.");
		if (OutField)
		{
			*OutField = TEXT("patch.replacement_pin_ref");
		}
		if (OutCode)
		{
			*OutCode = TEXT("owned_patch_replacement_ref_required");
		}
		return false;
	}

	const bool bResolvingNodePath = !Request.ReplacementNodePath.IsEmpty();
	const bool bResolvingPinPath = !Request.ReplacementPinPath.IsEmpty();
	const FString ReplacementNodeIdentifier = bResolvingNodePath ? Request.ReplacementNodePath : Request.ReplacementNodeRef;
	const FString ReplacementPinIdentifier = bResolvingPinPath ? Request.ReplacementPinPath : Request.ReplacementPinRef;
	FBlueprintHelperGraphWriteAnchorRef ReplacementAnchor;
	ReplacementAnchor.BlockId = Request.ReplacementBlockId;
	ReplacementAnchor.GroupEntryNodePath = Request.GroupEntryNodePath;
	ReplacementAnchor.NodeRef = bResolvingNodePath ? FString() : Request.ReplacementNodeRef;
	ReplacementAnchor.PinRef = bResolvingPinPath ? FString() : Request.ReplacementPinRef;
	ReplacementAnchor.NodePath = Request.ReplacementNodePath;
	ReplacementAnchor.PinPath = Request.ReplacementPinPath;

	FBlueprintHelperPatchResolveError ResolveError;
	UEdGraphNode* ReplacementNode = nullptr;
	if (!FBlueprintHelperGraphWriteBlockScopedResolver::ResolveNode(PathService, Graph, ReplacementAnchor, ReplacementNode, ResolveError))
	{
		OutError = FString::Printf(TEXT("Unable to resolve replacement node: %s"), *ReplacementNodeIdentifier);
		if (OutField)
		{
			*OutField = bResolvingNodePath ? TEXT("patch.replacement_node_path") : TEXT("patch.replacement_node_ref");
		}
		if (OutCode)
		{
			*OutCode = TEXT("target_node_not_found");
		}
		return false;
	}

	if (!FBlueprintHelperGraphWriteBlockScopedResolver::ResolvePin(PathService, Graph, ReplacementNode, ReplacementAnchor, OutPin, ResolveError))
	{
		OutError = FString::Printf(TEXT("Unable to resolve replacement pin: %s"), *ReplacementPinIdentifier);
		if (OutField)
		{
			*OutField = bResolvingPinPath ? TEXT("patch.replacement_pin_path") : TEXT("patch.replacement_pin_ref");
		}
		if (OutCode)
		{
			*OutCode = TEXT("target_pin_not_found");
		}
		return false;
	}

	FBlueprintHelperOwnedGraphPatchPolicyResult ReplacementPolicy =
		FBlueprintHelperOwnedGraphPatchPolicy::RequireOwnedPinInBlock(OutPin, Request.BlockId, TEXT("patch.replacement_ref"));
	if (!ReplacementPolicy.bPassed)
	{
		OutError = ReplacementPolicy.Message;
		if (OutField)
		{
			*OutField = ReplacementPolicy.Field;
		}
		if (OutCode)
		{
			*OutCode = ReplacementPolicy.Code;
		}
		return false;
	}

	return true;
}

bool FBlueprintHelperPatchBlueprintGraphService::ApplyPatch(
	UBlueprint* Blueprint, UEdGraph* Graph,
	const FPatchRequest& Request, const FBlueprintHelperResolvedPatchTarget& Target,
	bool& bOutChanged, FString& OutError) const
{
	const FString PatchKind = PatchTypeToString(Request.PatchType);
	const IBlueprintHelperPatchOperationHandler* Handler =
		FBlueprintHelperPatchOperationHandlerRegistry::FindHandler(PatchKind);
	if (!Handler)
	{
		OutError = FString::Printf(TEXT("Unsupported patch_type: %s"), *PatchKind);
		return false;
	}

	const TSharedPtr<FJsonObject> PatchJson =
		Request.PatchPayload.IsValid() ? Request.PatchPayload : MakeShared<FJsonObject>();
	FBlueprintHelperToolError ValidationError;
	if (!Handler->ValidateRequest(PatchJson.ToSharedRef(), ValidationError))
	{
		OutError = ValidationError.Message.IsEmpty() ? ValidationError.Code : ValidationError.Message;
		return false;
	}

	FBlueprintHelperPatchOperationApplyContext Context;
	Context.Blueprint = Blueprint;
	Context.Graph = Graph;
	Context.TargetNode = Target.Node;
	Context.TargetPin = Target.Pin;
	Context.SourcePin = Target.SourcePin;
	Context.ReplacementPin = Target.ReplacementPin;
	Context.Link = Target.Link;
	Context.bDeleteBreakLinks = Request.bDeleteBreakLinks;
	Context.PatchJson = PatchJson;
	Context.ExecuteMutationIntent =
		[this, Graph](const FBlueprintHelperGraphWriteMutationIntent& Intent, bool& bInnerChanged, FString& InnerError)
		{
			return ExecuteMutationIntent(Graph, Intent, bInnerChanged, InnerError);
		};

	return Handler->ApplyResolvedPatch(Context, bOutChanged, OutError);

}

// 鈹€鈹€鈹€ set_pin_default 鈹€鈹€鈹€

bool FBlueprintHelperPatchBlueprintGraphService::ExecuteMutationIntent(
	UEdGraph* Graph,
	const FBlueprintHelperGraphWriteMutationIntent& Intent,
	bool& bOutChanged,
	FString& OutError) const
{
	TArray<FString> Unresolved;
	const FBlueprintGenerateResult Result =
		FBlueprintHelperGraphWriteMutationCoordinator::ExecuteIntents(Graph, {Intent}, Unresolved);
	bOutChanged = Result.AppliedDefaultValueCount > 0
		|| Result.CreatedConnectionCount > 0
		|| Result.GeneratedNodeCount > 0;
	if (!Result.bSucceed)
	{
		OutError = Unresolved.Num() > 0 ? Unresolved[0] : Result.Message;
		return false;
	}
	return true;
}
// 鈹€鈹€鈹€ connect_pins 鈹€鈹€鈹€
