// BlueprintHelper Service Layer 鈥?ReplaceBlueprintGraph 鏍稿績鏈嶅姟瀹炵幇

#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/Transactions/BlueprintHelperTransactionJournalService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphSnapshotService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/Review/BlueprintHelperReviewHashService.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDebugData.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"
#include "Shared/GraphWrite/BlueprintHelperReplaceGraphTypes.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

class FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils
{
public:
	static UEdGraphPin* FindFirstExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	static bool NodeMatchesEntryName(UEdGraphNode* Node, const FString& EntryName)
	{
		if (!Node)
		{
			return false;
		}
		if (EntryName.IsEmpty())
		{
			return true;
		}

		if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
		{
			if (CustomEvent->CustomFunctionName.ToString().Equals(EntryName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
		{
			if (EventNode->GetFunctionName().ToString().Equals(EntryName, ESearchCase::IgnoreCase) ||
				EventNode->EventReference.GetMemberName().ToString().Equals(EntryName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		if (UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))
		{
			if (FunctionEntry->FunctionReference.GetMemberName().ToString().Equals(EntryName, ESearchCase::IgnoreCase) ||
				FunctionEntry->CustomGeneratedFunctionName.ToString().Equals(EntryName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		const FString NodeName = Node->GetName();
		const FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		return NodeName.Equals(EntryName, ESearchCase::IgnoreCase) ||
			NodeTitle.Equals(EntryName, ESearchCase::IgnoreCase);
	}

	static bool TryReadBlueprintHelperBlockId(UEdGraphNode* Node, FString& OutBlockId)
	{
		OutBlockId.Reset();
		if (!Node)
		{
			return false;
		}

		UPackage* Package = Node->GetOutermost();
		if (!Package)
		{
			return false;
		}

		FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
		if (MetaData.GetValue(Node, TEXT("BlueprintHelperOwned")) != TEXT("true"))
		{
			return false;
		}

		OutBlockId = MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId"));
		return !OutBlockId.IsEmpty();
	}

	static bool HasInboundExecLinkFromImportedNode(UEdGraphPin* ExecInputPin, const TSet<UEdGraphNode*>& ImportedNodes)
	{
		if (!ExecInputPin)
		{
			return false;
		}

		for (UEdGraphPin* LinkedPin : ExecInputPin->LinkedTo)
		{
			if (!LinkedPin ||
				LinkedPin->Direction != EGPD_Output ||
				LinkedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				continue;
			}

			if (ImportedNodes.Contains(LinkedPin->GetOwningNode()))
			{
				return true;
			}
		}

		return false;
	}

	static UEdGraphNode* FindFirstImportedExecutableBodyNode(
		UEdGraph* Graph,
		const TSet<UEdGraphNode*>& NodesBeforeImport)
	{
		TArray<UEdGraphNode*> ImportedExecutableNodes;
		TSet<UEdGraphNode*> ImportedNodes;

		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node || NodesBeforeImport.Contains(Node))
			{
				continue;
			}

			ImportedNodes.Add(Node);
			if (FindFirstExecPin(Node, EGPD_Input))
			{
				ImportedExecutableNodes.Add(Node);
			}
		}

		for (UEdGraphNode* Node : ImportedExecutableNodes)
		{
			if (!HasInboundExecLinkFromImportedNode(FindFirstExecPin(Node, EGPD_Input), ImportedNodes))
			{
				return Node;
			}
		}

		return ImportedExecutableNodes.Num() > 0 ? ImportedExecutableNodes[0] : nullptr;
	}

	static bool PinsHaveSingleConnectionToEachOther(UEdGraphPin* FirstPin, UEdGraphPin* SecondPin)
	{
		return FirstPin &&
			SecondPin &&
			FirstPin->LinkedTo.Num() == 1 &&
			SecondPin->LinkedTo.Num() == 1 &&
			FirstPin->LinkedTo[0] == SecondPin &&
			SecondPin->LinkedTo[0] == FirstPin;
	}

	static void BreakAllPinLinksWithModify(UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return;
		}

		Pin->Modify();
		Pin->BreakAllPinLinks(true);
	}

	static FBlueprintHelperGraphReviewNodeAnchor MakeReviewNodeAnchor(const UEdGraphNode* Node)
	{
		FBlueprintHelperGraphReviewNodeAnchor Anchor;
		if (!Node)
		{
			return Anchor;
		}

		Anchor.NodePath = Node->GetPathName();
		Anchor.NodeGuid = Node->NodeGuid.IsValid()
			? Node->NodeGuid.ToString(EGuidFormats::Digits)
			: FString();
		Anchor.DisplayLabel = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		if (Anchor.DisplayLabel.IsEmpty())
		{
			Anchor.DisplayLabel = Node->GetName();
		}
		Anchor.GraphPosition = FVector2D(
			static_cast<float>(Node->NodePosX),
			static_cast<float>(Node->NodePosY));
		Anchor.GraphSize = FVector2D(
			Node->NodeWidth > 0 ? static_cast<float>(Node->NodeWidth) : 360.0f,
			Node->NodeHeight > 0 ? static_cast<float>(Node->NodeHeight) : 180.0f);
		Anchor.bHasGraphBounds = true;
		return Anchor;
	}

};

// 鈹€鈹€鈹€ 鏋勯€?鈹€鈹€鈹€

FBlueprintHelperReplaceBlueprintGraphService::FBlueprintHelperReplaceBlueprintGraphService(
	const FBlueprintHelperGraphResolver& InResolver,
	const FBlueprintHelperBlockIdService& InBlockIdService,
	const FBlueprintHelperOwnershipService& InOwnershipService,
	const FBlueprintHelperTransactionJournalService& InJournalService,
	const FBlueprintHelperGraphSnapshotService& InSnapshotService)
	: Resolver(InResolver)
	, BlockIdService(InBlockIdService)
	, OwnershipService(InOwnershipService)
	, JournalService(InJournalService)
	, SnapshotService(InSnapshotService)
{
}

// 鈹€鈹€鈹€ 鍏叡鍏ュ彛 鈹€鈹€鈹€

FBlueprintHelperToolResultBase FBlueprintHelperReplaceBlueprintGraphService::Execute(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FReplaceRequest Request = ParseRequest(Payload);

	if (Request.bDryRun)
	{
		return ExecuteDryRun(Request);
	}

	return ExecuteWrite(Request);
}

// 鈹€鈹€鈹€ 瑙ｆ瀽 鈹€鈹€鈹€

FBlueprintHelperReplaceBlueprintGraphService::FReplaceRequest
FBlueprintHelperReplaceBlueprintGraphService::ParseRequest(const TSharedPtr<FJsonObject>& Payload) const
{
	FReplaceRequest Request;

	if (!Payload.IsValid())
	{
		return Request;
	}

	// target
	const TSharedPtr<FJsonObject>* TargetObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("target"), TargetObject) && TargetObject->IsValid())
	{
		(*TargetObject)->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
		(*TargetObject)->TryGetStringField(TEXT("graph"), Request.GraphName);

		FString ScopeStr;
		if ((*TargetObject)->TryGetStringField(TEXT("replace_scope"), ScopeStr))
		{
			ParseReplaceScope(ScopeStr, Request.Scope);
		}
	}

	// selector
	const TSharedPtr<FJsonObject>* SelectorObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("selector"), SelectorObject) && SelectorObject->IsValid())
	{
		(*SelectorObject)->TryGetStringField(TEXT("block_id"), Request.BlockId);
		(*SelectorObject)->TryGetStringField(TEXT("target_ref"), Request.TargetRef);
		(*SelectorObject)->TryGetStringField(TEXT("entry_name"), Request.EntryName);
		(*SelectorObject)->TryGetStringField(TEXT("node_path"), Request.NodePath);
	}

	const TSharedPtr<FJsonObject>* LogicSpecObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("logic_spec"), LogicSpecObject) && LogicSpecObject && LogicSpecObject->IsValid())
	{
		Request.LogicSpec = *LogicSpecObject;
	}

	// options
	const TSharedPtr<FJsonObject>* OptionsObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("options"), OptionsObject) && OptionsObject->IsValid())
	{
		(*OptionsObject)->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
		(*OptionsObject)->TryGetBoolField(TEXT("strict"), Request.bStrict);
		(*OptionsObject)->TryGetBoolField(TEXT("preserve_layout"), Request.bPreserveLayout);
	}

	return Request;
}

// 鈹€鈹€鈹€ Preflight 鈹€鈹€鈹€

FBlueprintHelperReplaceBlueprintGraphService::FReplacePreflightResult
FBlueprintHelperReplaceBlueprintGraphService::Preflight(const FReplaceRequest& Request) const
{
	FReplacePreflightResult Result;

	// 1. asset_path
	if (Request.AssetPath.IsEmpty())
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("target_blueprint_not_found"));
		Result.Conflicts.Add({TEXT("target_blueprint_not_found"),
			TEXT("Missing target.asset_path."), TEXT("target.asset_path"), TEXT("payload")});
		return Result;
	}

	// 2. graph
	if (Request.GraphName.IsEmpty())
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("target_graph_not_found"));
		Result.Conflicts.Add({TEXT("target_graph_not_found"),
			TEXT("Missing target.graph."), TEXT("target.graph"), TEXT("payload")});
		return Result;
	}

	// 3. replace_scope
	if (Request.Scope == EBlueprintHelperReplaceScope::FunctionDefinition ||
		Request.Scope == EBlueprintHelperReplaceScope::EventDefinition)
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("replace_scope_unsupported"));
		Result.Conflicts.Add({TEXT("replace_scope_unsupported"),
			TEXT("function_definition / event_definition write is not supported; use dry_run."),
			TEXT("target.replace_scope"), TEXT("payload")});
		return Result;
	}


	// 5. 钃濆浘鏍￠獙
	UBlueprint* Blueprint = nullptr;
	if (!PreflightBlueprint(Request.AssetPath, Blueprint, Result))
	{
		return Result;
	}

	if (!PreflightLogicSpec(Request, Blueprint, Result))
	{
		return Result;
	}

	// 6. 鍥捐〃鏍￠獙
	UEdGraph* Graph = nullptr;
	if (!PreflightGraphTarget(Blueprint, Request.GraphName, Request.Scope, Graph, Result))
	{
		return Result;
	}

	// 7. scope 鏍￠獙
	if (!PreflightReplaceScope(Request.Scope, Result))
	{
		return Result;
	}

	// 8. selector 瀛樺湪鎬ф鏌?
	if (Request.Scope == EBlueprintHelperReplaceScope::BlockImplementation)
	{
		if (Request.BlockId.IsEmpty() && Request.TargetRef.IsEmpty() && Request.NodePath.IsEmpty())
		{
			Result.bPassed = false;
			Result.BlockedBy.Add(TEXT("target_block_not_found"));
			Result.Conflicts.Add({TEXT("target_block_not_found"),
				TEXT("block_implementation requires selector.block_id, selector.target_ref, or selector.node_path."),
				TEXT("selector"), TEXT("payload")});
		}
	}

	return Result;
}

bool FBlueprintHelperReplaceBlueprintGraphService::PreflightBlueprint(
	const FString& AssetPath, UBlueprint*& OutBlueprint, FReplacePreflightResult& OutResult) const
{
	FBlueprintHelperGraphTarget Target;
	Target.BlueprintPath = AssetPath;

	FBlueprintHelperDiagnosticSet Diag;
	OutBlueprint = Resolver.ResolveBlueprint(Target, Diag);

	if (!OutBlueprint || Diag.HasErrors())
	{
		OutResult.bPassed = false;
		OutResult.BlockedBy.Add(TEXT("target_blueprint_not_found"));
		OutResult.Conflicts.Add({TEXT("target_blueprint_not_found"),
			FString::Printf(TEXT("钃濆浘璧勪骇鏈壘鍒帮細%s"), *AssetPath),
			AssetPath, TEXT("target.asset_path")});
		return false;
	}

	return true;
}

bool FBlueprintHelperReplaceBlueprintGraphService::PreflightLogicSpec(
	const FReplaceRequest& Request,
	UBlueprint* Blueprint,
	FReplacePreflightResult& OutResult) const
{
	if (!Request.LogicSpec.IsValid())
	{
		OutResult.bPassed = false;
		OutResult.BlockedBy.Add(TEXT("logic_spec_required"));
		OutResult.Conflicts.Add({TEXT("logic_spec_required"),
			TEXT("replace_blueprint_graph requires logic_spec/SemanticIR input."), TEXT("logic_spec"), TEXT("payload")});
		return false;
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

bool FBlueprintHelperReplaceBlueprintGraphService::PreflightGraphTarget(
	UBlueprint* Blueprint, const FString& GraphName, EBlueprintHelperReplaceScope Scope,
	UEdGraph*& OutGraph, FReplacePreflightResult& OutResult) const
{
	// 鍑芥暟鍥?(function_body)
	if (Scope == EBlueprintHelperReplaceScope::FunctionBody)
	{
		for (UEdGraph* FnGraph : Blueprint->FunctionGraphs)
		{
			if (FnGraph && FnGraph->GetName() == GraphName)
			{
				OutGraph = FnGraph;
				return true;
			}
		}
		OutResult.bPassed = false;
		OutResult.BlockedBy.Add(TEXT("target_function_not_found"));
		OutResult.Conflicts.Add({TEXT("target_function_not_found"),
			FString::Printf(TEXT("Function graph %s was not found."), *GraphName), GraphName, TEXT("target.graph")});
		return false;
	}

	// 浜嬩欢鍥?(block_implementation / event_body / custom_event_body / graph)
	for (UEdGraph* UbergraphPage : Blueprint->UbergraphPages)
	{
		if (UbergraphPage && UbergraphPage->GetName() == GraphName)
		{
			OutGraph = UbergraphPage;
			return true;
		}
	}

	OutResult.bPassed = false;
	OutResult.BlockedBy.Add(TEXT("target_graph_not_found"));
	OutResult.Conflicts.Add({TEXT("target_graph_not_found"),
		FString::Printf(TEXT("Graph %s was not found."), *GraphName), GraphName, TEXT("target.graph")});
	return false;
}

bool FBlueprintHelperReplaceBlueprintGraphService::PreflightReplaceScope(
	EBlueprintHelperReplaceScope Scope, FReplacePreflightResult& OutResult) const
{
	if (Scope == EBlueprintHelperReplaceScope::FunctionDefinition ||
		Scope == EBlueprintHelperReplaceScope::EventDefinition)
	{
		OutResult.bPassed = false;
		OutResult.BlockedBy.Add(TEXT("replace_scope_unsupported"));
		OutResult.Conflicts.Add({TEXT("replace_scope_unsupported"),
			TEXT("This replace_scope write path is not implemented."), TEXT("target.replace_scope"), TEXT("payload")});
		return false;
	}
	return true;
}

// 鈹€鈹€鈹€ DryRun 鈹€鈹€鈹€

FBlueprintHelperToolResultBase FBlueprintHelperReplaceBlueprintGraphService::ExecuteDryRun(
	const FReplaceRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	const FReplacePreflightResult PreflightResult = Preflight(Request);

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
		TEXT("replace_blueprint_graph"), TraceId);

	FBlueprintHelperTargetRef TargetRef;
	TargetRef.AssetPath = Request.AssetPath;
	TargetRef.TargetType = EBlueprintHelperTargetType::Graph;
	TargetRef.Graph = Request.GraphName;
	Result.Target = TargetRef;

	// 鐗规畩锛歊eplace dry_run target 涓嶈緭鍑?target_type锛屼絾 include replace_scope
	// 閫氳繃鐩存帴璁剧疆 JSON 瀛楁瀹炵幇
	TargetRef.TargetType = EBlueprintHelperTargetType::None;

	if (PreflightResult.bPassed)
	{
		FBlueprintHelperReplaceDryRunData DryRunData;
		DryRunData.DryRun.Result = TEXT("passed");
		DryRunData.DryRun.bCanExecute = true;
		Result.Data = DryRunData.ToJson();
		FBlueprintHelperGraphFragmentDebugData::AttachToData(Result.Data, PreflightResult.FragmentDebugData);
	}
	else
	{
		FBlueprintHelperReplaceDryRunData DryRunData;
		DryRunData.DryRun.Result = TEXT("blocked");
		DryRunData.DryRun.bCanExecute = false;
		DryRunData.DryRun.BlockedBy = PreflightResult.BlockedBy;
		DryRunData.DryRun.Conflicts = PreflightResult.Conflicts;
		DryRunData.DryRun.Errors = PreflightResult.Errors;

		const FBlueprintHelperGraphWriteIssue* FirstIssue = PreflightResult.Conflicts.Num() > 0
			? &PreflightResult.Conflicts[0]
			: (PreflightResult.Errors.Num() > 0 ? &PreflightResult.Errors[0] : nullptr);

		FBlueprintHelperToolError Error;
		Error.Code = PreflightResult.BlockedBy.Num() > 0 ? PreflightResult.BlockedBy[0] : TEXT("preflight_failed");
		Error.Stage = EBlueprintHelperToolStage::Preflight;
		Error.Message = FirstIssue && !FirstIssue->Message.IsEmpty()
			? FirstIssue->Message
			: TEXT("Replace dry-run preflight blocked execution.");
		Error.Field = FirstIssue && !FirstIssue->Source.IsEmpty()
			? FirstIssue->Source
			: TEXT("target.graph");
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;

		Result = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("replace_blueprint_graph"), TraceId, Error);
		Result.Target = TargetRef;
		Result.Data = DryRunData.ToJson();
		FBlueprintHelperGraphFragmentDebugData::AttachToData(Result.Data, PreflightResult.FragmentDebugData);
	}

	return Result;
}

// 鈹€鈹€鈹€ 姝ｅ紡鍐欏叆 鈹€鈹€鈹€

FBlueprintHelperToolResultBase FBlueprintHelperReplaceBlueprintGraphService::ExecuteWrite(
	const FReplaceRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	const FString TransactionId = JournalService.GenerateTransactionId();

	// 1. Preflight
	FReplacePreflightResult PreflightResult = Preflight(Request);
	if (!PreflightResult.bPassed)
	{
		FBlueprintHelperToolError Error;
		Error.Code = PreflightResult.BlockedBy.Num() > 0 ? PreflightResult.BlockedBy[0] : TEXT("preflight_failed");
		Error.Stage = EBlueprintHelperToolStage::Preflight;
		Error.Message = PreflightResult.Conflicts.Num() > 0
			? PreflightResult.Conflicts[0].Message : TEXT("Preflight failed.");
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		FBlueprintHelperToolResultBase FailResult = FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
		FBlueprintHelperGraphFragmentDebugData::AttachToData(FailResult.Data, PreflightResult.FragmentDebugData);
		return FailResult;
	}

	// 2. 瑙ｆ瀽钃濆浘
	FBlueprintHelperGraphTarget BPTarget;
	BPTarget.BlueprintPath = Request.AssetPath;
	FBlueprintHelperDiagnosticSet Diag;
	UBlueprint* Blueprint = Resolver.ResolveBlueprint(BPTarget, Diag);
	if (!Blueprint)
	{
		FBlueprintHelperToolError Error;
		Error.Code = TEXT("target_blueprint_not_found");
		Error.Stage = EBlueprintHelperToolStage::ResolveTarget;
		Error.Message = FString::Printf(TEXT("Blueprint %s was not found."), *Request.AssetPath);
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
	}

	// 3. 瑙ｆ瀽鏇挎崲鐩爣
	FResolvedReplaceTarget Resolved;
	FString ResolveError;
	if (!ResolveReplaceTarget(Request, Blueprint, Resolved, ResolveError))
	{
		FBlueprintHelperToolError Error;
		Error.Code = ResolveError.Contains(TEXT("block")) ? TEXT("target_block_not_found")
			: (ResolveError.Contains(TEXT("function")) ? TEXT("target_function_not_found") : TEXT("target_not_found"));
		Error.Stage = EBlueprintHelperToolStage::ResolveTarget;
		Error.Message = ResolveError;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
	}

	// 4. 鎹曡幏 before snapshot
	FBlueprintHelperGraphSnapshot BeforeSnapshot = SnapshotService.CaptureNodeSnapshot(
		Resolved.Graph, Resolved.NodesToDelete);
	BeforeSnapshot.OwnerBlockId = Resolved.OriginalBlockId;
	BeforeSnapshot.EntryIdentity = Request.EntryName.IsEmpty() ? Resolved.TargetRef : Request.EntryName;
	BeforeSnapshot.ReplaceScope = ReplaceScopeToString(Request.Scope);
	const FString ReviewBlockTargetKey = !Resolved.OriginalBlockId.IsEmpty()
		? FString::Printf(TEXT("graph:%s:block:%s"), *Request.GraphName, *Resolved.OriginalBlockId)
		: FString();
	FString BeforeBlockHash;
	if (!ReviewBlockTargetKey.IsEmpty())
	{
		FBlueprintHelperReviewAtomicTarget BeforeBlockTarget;
		BeforeBlockTarget.AssetPath = Request.AssetPath;
		BeforeBlockTarget.GraphName = Request.GraphName;
		BeforeBlockTarget.TargetKind = TEXT("graph_block");
		BeforeBlockTarget.TargetKey = ReviewBlockTargetKey;
		FString HashError;
		FBlueprintHelperReviewHashService::ComputeAtomicTargetHash(BeforeBlockTarget, BeforeBlockHash, HashError);
	}

	// 5. 寮€濮嬩慨鏀?
	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Replace Blueprint Graph")), Blueprint);
	Mutation.Modify(Resolved.Graph);

	// 6. 鍒犻櫎鏃у疄鐜?
	if (!DeleteOldImplementation(Blueprint, Resolved.Graph, Resolved.NodesToDelete))
	{
		Mutation.Rollback();

		FBlueprintHelperToolError Error;
		Error.Code = TEXT("node_create_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = TEXT("Failed to delete old implementation.");
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::RolledBack;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
	}

	TSet<UEdGraphNode*> NodesBeforeImport;
	for (UEdGraphNode* Node : Resolved.Graph->Nodes)
	{
		if (Node)
		{
			NodesBeforeImport.Add(Node);
		}
	}

	// 7. 閫氳繃 AgentImportService 鍒涘缓鏂拌妭鐐?杩炵嚎
	const FString GraphWritePayload = BuildSemanticGraphWritePayload(Request);
	TArray<TSharedPtr<FUnresolvedNodeItem>> UnresolvedNodes;
	const FBlueprintGenerateResult GenerateResult =
		FBlueprintGraphGenerationPipeline::GenerateBlueprintFromJson(Resolved.Graph, GraphWritePayload, UnresolvedNodes);

	if (!GenerateResult.bSucceed)
	{
		Mutation.Rollback();

		FString ErrorMessage = GenerateResult.Message.IsEmpty()
			? TEXT("Failed to create replacement implementation through SemanticIR.")
			: GenerateResult.Message;
		if (UnresolvedNodes.Num() > 0 && UnresolvedNodes[0].IsValid())
		{
			ErrorMessage += FString::Printf(
				TEXT(" First unresolved: %s - %s"),
				*UnresolvedNodes[0]->DisplayText,
				*UnresolvedNodes[0]->Reason);
		}

		FBlueprintHelperToolError Error;
		Error.Code = TEXT("semantic_graph_write_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = ErrorMessage;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::RolledBack;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
	}
	FString ReconnectError;
	if (!ReconnectPreservedEntryToNewBody(Request, Resolved, NodesBeforeImport, ReconnectError))
	{
		Mutation.Rollback();

		FBlueprintHelperToolError Error;
		Error.Code = TEXT("entry_reconnect_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = ReconnectError.IsEmpty()
			? TEXT("Failed to rebuild entry exec link after replacement.") : ReconnectError;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::RolledBack;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
	}

	// 8. 鍐欏叆 ownership锛堝鏋滅洰鏍囦负 owned block锛?
	TArray<UEdGraphNode*> NewNodes;
	for (UEdGraphNode* Node : Resolved.Graph->Nodes)
	{
		if (Node && !NodesBeforeImport.Contains(Node))
		{
			NewNodes.Add(Node);
		}
	}

	if (Resolved.bIsBlueprintHelperOwned)
	{
		if (NewNodes.Num() > 0)
		{
			FString OwnershipError;
			if (!OwnershipService.WriteBlockOwnership(
				Blueprint, NewNodes, Resolved.OriginalBlockId,
				TransactionId, TEXT("Replace"), OwnershipError))
			{
				Mutation.Rollback();

				FBlueprintHelperToolError Error;
				Error.Code = TEXT("ownership_write_failed");
				Error.Stage = EBlueprintHelperToolStage::Execute;
				Error.Message = OwnershipError;
				Error.bRetryable = false;
				Error.RollbackResult = EBlueprintHelperRollbackResult::RolledBack;
				return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
			}
		}
	}

	// 9. 鍐欏叆 Journal
	FBlueprintHelperAppendJournalRecord JournalRecord;
	JournalRecord.TransactionId = TransactionId;
	JournalRecord.Tool = TEXT("ReplaceBlueprintGraph");
	JournalRecord.Status = TEXT("applied");
	JournalRecord.TargetAssets.Add(Request.AssetPath);
	JournalRecord.GraphId = Request.GraphName;
	JournalRecord.GraphName = Request.GraphName;
	if (!Resolved.OriginalBlockId.IsEmpty())
	{
		JournalRecord.BlockIds.Add(Resolved.OriginalBlockId);
	}
	if (!ReviewBlockTargetKey.IsEmpty() && !BeforeBlockHash.IsEmpty())
	{
		FBlueprintHelperReviewAtomicTarget AfterBlockTarget;
		AfterBlockTarget.AssetPath = Request.AssetPath;
		AfterBlockTarget.GraphName = Request.GraphName;
		AfterBlockTarget.TargetKind = TEXT("graph_block");
		AfterBlockTarget.TargetKey = ReviewBlockTargetKey;
		FString AfterBlockHash;
		FString HashError;
		if (FBlueprintHelperReviewHashService::ComputeAtomicTargetHash(AfterBlockTarget, AfterBlockHash, HashError))
		{
			JournalRecord.BaselineHashesByTargetKey.Add(ReviewBlockTargetKey, BeforeBlockHash);
			JournalRecord.RecordedAfterHashesByTargetKey.Add(ReviewBlockTargetKey, AfterBlockHash);
		}
	}
	for (UEdGraphNode* Node : NewNodes)
	{
		if (!Node)
		{
			continue;
		}
		JournalRecord.CreatedNodePaths.Add(FString::Printf(TEXT("/%s"), *Node->GetPathName()));
		if (Node->NodeGuid.IsValid())
		{
			JournalRecord.CreatedNodePaths.Add(Node->NodeGuid.ToString(EGuidFormats::Digits));
		}
		JournalRecord.CreatedNodeAnchors.Add(
			FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::MakeReviewNodeAnchor(Node));
		if (Node->NodeGuid.IsValid())
		{
			const FString NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
			const FString NodeTargetKey = FString::Printf(
				TEXT("graph:%s:node:%s"),
				*Request.GraphName,
				*NodeGuid);
			FBlueprintHelperReviewAtomicTarget NodeTarget;
			NodeTarget.AssetPath = Request.AssetPath;
			NodeTarget.GraphName = Request.GraphName;
			NodeTarget.TargetKind = TEXT("graph_node");
			NodeTarget.TargetKey = NodeTargetKey;
			NodeTarget.NodeGuid = NodeGuid;
			FString NodeAfterHash;
			FString HashError;
			if (FBlueprintHelperReviewHashService::ComputeAtomicTargetHash(NodeTarget, NodeAfterHash, HashError))
			{
				JournalRecord.BaselineHashesByTargetKey.Add(NodeTargetKey, NodeAfterHash);
				JournalRecord.RecordedAfterHashesByTargetKey.Add(NodeTargetKey, NodeAfterHash);
			}
		}
	}
	JournalRecord.RollbackData = BeforeSnapshot.ToJsonString();

	FString JournalError;
	if (!JournalService.WriteAppendJournal(JournalRecord, JournalError))
	{
		Mutation.Rollback();

		FBlueprintHelperToolError Error;
		Error.Code = TEXT("journal_write_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = JournalError;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::RolledBack;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
	}

	// 10. 鏍囪淇敼
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	if (Blueprint->GetOutermost())
	{
		Blueprint->GetOutermost()->MarkPackageDirty();
	}
	Mutation.Commit();

	// 11. 鎴愬姛缁撴灉
	FBlueprintHelperToolResultBase SuccessResult = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("replace_blueprint_graph"), TraceId);

	FBlueprintHelperTargetRef SuccessTarget;
	SuccessTarget.AssetPath = Request.AssetPath;
	SuccessTarget.TargetType = EBlueprintHelperTargetType::None; // 涓嶈緭鍑?target_type
	SuccessTarget.Graph = Request.GraphName;
	SuccessResult.Target = SuccessTarget;

	FBlueprintHelperReplaceGraphResultData Data;
	Data.ReplaceResult.ReplacedRef.GraphId = Resolved.GraphId.IsEmpty() ? Request.GraphName : Resolved.GraphId;
	Data.ReplaceResult.ReplacedRef.TargetRef = Resolved.TargetRef;
	Data.WriteRef.TransactionId = TransactionId;
	Data.WriteRef.bJournalRecorded = true;
	SuccessResult.Data = Data.ToJson();
	FBlueprintHelperGraphFragmentDebugData::AttachToData(SuccessResult.Data, PreflightResult.FragmentDebugData);

	FBlueprintHelperValidationSummary Validation;
	Validation.bShouldCompile = true;
	Validation.bShouldSave = true;
	SuccessResult.Validation = Validation;

	FBlueprintHelperGraphLayoutCoordinator::RecordGeneratedNodes(Resolved.Graph, NewNodes);

	return SuccessResult;
}

// 鈹€鈹€鈹€ 鐩爣瑙ｆ瀽 鈹€鈹€鈹€

bool FBlueprintHelperReplaceBlueprintGraphService::ResolveReplaceTarget(
	const FReplaceRequest& Request, UBlueprint* Blueprint, FResolvedReplaceTarget& OutTarget, FString& OutError) const
{
	// 鏌ユ壘鍥捐〃
	UEdGraph* Graph = nullptr;
	if (Request.Scope == EBlueprintHelperReplaceScope::FunctionBody)
	{
		for (UEdGraph* FnGraph : Blueprint->FunctionGraphs)
		{
			if (FnGraph && FnGraph->GetName() == Request.GraphName)
			{
				Graph = FnGraph;
				break;
			}
		}
	}
	else
	{
		for (UEdGraph* Page : Blueprint->UbergraphPages)
		{
			if (Page && Page->GetName() == Request.GraphName)
			{
				Graph = Page;
				break;
			}
		}
	}

	if (!Graph)
	{
		OutError = FString::Printf(TEXT("Graph %s was not found."), *Request.GraphName);
		return false;
	}

	OutTarget.Blueprint = Blueprint;
	OutTarget.Graph = Graph;
	OutTarget.Scope = Request.Scope;
	OutTarget.AssetPath = Request.AssetPath;
	OutTarget.GraphName = Request.GraphName;
	OutTarget.GraphId = Request.GraphName;

	if (Request.Scope == EBlueprintHelperReplaceScope::BlockImplementation)
	{
		return ResolveBlockImplementation(Graph, Request, OutTarget, OutError);
	}

	if (Request.Scope == EBlueprintHelperReplaceScope::FunctionBody)
	{
		OutTarget.TargetRef = Request.GraphName;
		// 鏀堕泦 body 鑺傜偣锛堜繚鐣?FunctionEntry/Result锛?
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && !Node->IsA<UK2Node_FunctionEntry>())
			{
				OutTarget.NodesToDelete.Add(Node);
			}
			else if (Node)
			{
				OutTarget.NodesToPreserve.Add(Node);
			}
		}
		OutTarget.bExternalDependentsMayBreak = false;
		return true;
	}

	// event_body / custom_event_body / graph: 鍥為€€鍒?block_implementation 璇箟
	if (Request.Scope == EBlueprintHelperReplaceScope::CustomEventBody ||
		Request.Scope == EBlueprintHelperReplaceScope::EventBody ||
		Request.Scope == EBlueprintHelperReplaceScope::Graph)
	{
		// 绠€鍖栵細鍒犻櫎鎵€鏈夐潪 entry 鑺傜偣
		OutTarget.TargetRef = Request.EntryName.IsEmpty() ? Request.GraphName : Request.EntryName;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node)
			{
				bool bIsCustomEventNode = Node->GetClass()->GetName().Contains(TEXT("K2Node_CustomEvent"));
				bool bIsEventNode = Node->GetClass()->GetName().Contains(TEXT("K2Node_Event"));
				if (bIsCustomEventNode || bIsEventNode)
				{
					OutTarget.NodesToPreserve.Add(Node);
					if (FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::NodeMatchesEntryName(Node, Request.EntryName))
					{
						FString EntryBlockId;
						if (FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::TryReadBlueprintHelperBlockId(Node, EntryBlockId))
						{
							OutTarget.OriginalBlockId = EntryBlockId;
							OutTarget.OriginalBlockRef = Request.EntryName.IsEmpty() ? Request.GraphName : Request.EntryName;
							OutTarget.TargetRef = OutTarget.OriginalBlockRef;
							OutTarget.bIsBlueprintHelperOwned = true;
						}
					}
				}
				else
				{
					OutTarget.NodesToDelete.Add(Node);
				}
			}
		}
		return true;
	}

	OutError = TEXT("Unsupported replace_scope.");
	return false;
}

bool FBlueprintHelperReplaceBlueprintGraphService::ResolveBlockImplementation(
	UEdGraph* Graph, const FReplaceRequest& Request, FResolvedReplaceTarget& OutTarget, FString& OutError) const
{
	// 鎵弿鍥捐〃涓殑 BlueprintHelper-owned 鑺傜偣
	FString SearchBlockId = Request.BlockId;
	if (SearchBlockId.IsEmpty() && !Request.TargetRef.IsEmpty())
	{
		SearchBlockId = FString::Printf(TEXT("%s_%s"), *Request.GraphName, *Request.TargetRef);
	}

	TArray<UEdGraphNode*> OwnedNodes;
	FString FoundBlockRef;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;

		UPackage* Package = Node->GetOutermost();
		if (!Package) continue;

		FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
		const FString OwnedStr = MetaData.GetValue(Node, TEXT("BlueprintHelperOwned"));
		if (OwnedStr != TEXT("true")) continue;

		const FString NodeBlockId = MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId"));
		if (!SearchBlockId.IsEmpty() && NodeBlockId != SearchBlockId) continue;

		OwnedNodes.Add(Node);
		if (FoundBlockRef.IsEmpty())
		{
			// 浠?block_id 鎻愬彇 block_ref
			const FString Prefix = Request.GraphName + TEXT("_");
			if (NodeBlockId.StartsWith(Prefix))
			{
				FoundBlockRef = NodeBlockId.Mid(Prefix.Len());
			}
		}
	}

	if (OwnedNodes.Num() == 0)
	{
		if (SearchBlockId.IsEmpty())
		{
			OutError = TEXT("No BlueprintHelper-owned node found. Provide selector.block_id or selector.target_ref.");
		}
		else
		{
			OutError = FString::Printf(TEXT("Target block %s was not found or is not owned by BlueprintHelper."), *SearchBlockId);
		}
		return false;
	}

	OutTarget.NodesToDelete = OwnedNodes;
	OutTarget.ExistingOwnedNodes = OwnedNodes;
	OutTarget.OriginalBlockId = SearchBlockId;
	OutTarget.OriginalBlockRef = FoundBlockRef.IsEmpty() ? Request.TargetRef : FoundBlockRef;
	OutTarget.TargetRef = FoundBlockRef.IsEmpty() ? Request.TargetRef : FoundBlockRef;
	OutTarget.bIsBlueprintHelperOwned = true;

	return true;
}

// 鈹€鈹€鈹€ 鍒犻櫎鏃у疄鐜?鈹€鈹€鈹€

bool FBlueprintHelperReplaceBlueprintGraphService::DeleteOldImplementation(
	UBlueprint* Blueprint, UEdGraph* Graph, const TArray<UEdGraphNode*>& NodesToDelete) const
{
	if (!Blueprint || !Graph)
	{
		return false;
	}

	for (UEdGraphNode* Node : NodesToDelete)
	{
		if (Node)
		{
			FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
		}
	}

	Graph->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return true;
}

bool FBlueprintHelperReplaceBlueprintGraphService::ReconnectPreservedEntryToNewBody(
	const FReplaceRequest& Request,
	const FResolvedReplaceTarget& Resolved,
	const TSet<UEdGraphNode*>& NodesBeforeImport,
	FString& OutError) const
{
	if (Request.Scope != EBlueprintHelperReplaceScope::FunctionBody &&
		Request.Scope != EBlueprintHelperReplaceScope::EventBody &&
		Request.Scope != EBlueprintHelperReplaceScope::CustomEventBody)
	{
		return true;
	}

	if (!Resolved.Graph)
	{
		OutError = TEXT("Replacement graph is null; cannot rebuild entry exec link.");
		return false;
	}

	UEdGraphNode* EntryNode = nullptr;
	for (UEdGraphNode* Node : Resolved.NodesToPreserve)
	{
		if (!Node)
		{
			continue;
		}

		const bool bMatchesScope =
			(Request.Scope == EBlueprintHelperReplaceScope::FunctionBody && Node->IsA<UK2Node_FunctionEntry>()) ||
			(Request.Scope != EBlueprintHelperReplaceScope::FunctionBody && FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::FindFirstExecPin(Node, EGPD_Output) != nullptr);
		if (bMatchesScope && FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::NodeMatchesEntryName(Node, Request.EntryName))
		{
			EntryNode = Node;
			break;
		}
	}

	if (!EntryNode)
	{
		OutError = Request.EntryName.IsEmpty()
			? TEXT("No preserved entry node was found for reconnection.")
			: FString::Printf(TEXT("No preserved entry node was found for reconnection: %s."), *Request.EntryName);
		return false;
	}

	UEdGraphPin* EntryExecOut = FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::FindFirstExecPin(EntryNode, EGPD_Output);
	UEdGraphNode* FirstBodyNode = FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::FindFirstImportedExecutableBodyNode(Resolved.Graph, NodesBeforeImport);
	if (!EntryExecOut)
	{
		OutError = TEXT("Entry node or replacement body first node is missing an Exec pin.");
		return false;
	}

	if (!FirstBodyNode)
	{
		if (EntryExecOut->LinkedTo.Num() > 0)
		{
			FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::BreakAllPinLinksWithModify(EntryExecOut);
			Resolved.Graph->NotifyGraphChanged();
		}
		return true;
	}

	UEdGraphPin* BodyExecIn = FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::FindFirstExecPin(FirstBodyNode, EGPD_Input);
	if (!BodyExecIn)
	{
		OutError = TEXT("Replacement body first node is missing an Exec input pin.");
		return false;
	}

	if (FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::PinsHaveSingleConnectionToEachOther(EntryExecOut, BodyExecIn))
	{
		return true;
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!Schema)
	{
		OutError = TEXT("K2 schema is unavailable; cannot rebuild entry exec link.");
		return false;
	}

	FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::BreakAllPinLinksWithModify(EntryExecOut);
	FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::BreakAllPinLinksWithModify(BodyExecIn);
	if (!Schema->TryCreateConnection(EntryExecOut, BodyExecIn) ||
		!FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::PinsHaveSingleConnectionToEachOther(EntryExecOut, BodyExecIn))
	{
		OutError = FString::Printf(TEXT("Cannot connect entry %s to replacement body first node %s."),
			*EntryNode->GetName(), *FirstBodyNode->GetName());
		return false;
	}

	Resolved.Graph->NotifyGraphChanged();
	return true;
}

// 鈹€鈹€鈹€ AgentImport payload 鏋勫缓 鈹€鈹€鈹€

FString FBlueprintHelperReplaceBlueprintGraphService::BuildSemanticGraphWritePayload(
	const FReplaceRequest& Request) const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.AgentImportGraph"));
	Root->SetStringField(TEXT("version"), TEXT("1.0"));
	Root->SetStringField(TEXT("target_blueprint"), Request.AssetPath);
	Root->SetStringField(TEXT("target_graph"), Request.GraphName);
	Root->SetStringField(TEXT("mode"), TEXT("append"));
	Root->SetStringField(TEXT("layout"), TEXT("auto"));

	TSharedRef<FJsonObject> Options = MakeShared<FJsonObject>();
	Options->SetBoolField(TEXT("compile"), false);
	Options->SetBoolField(TEXT("save"), false);
	Options->SetBoolField(TEXT("strict"), true);
	Options->SetBoolField(TEXT("dry_run"), false);
	Options->SetBoolField(TEXT("create_missing_variables"), false);
	Options->SetBoolField(TEXT("reconstruct_existing_nodes"), false);
	Root->SetObjectField(TEXT("options"), Options);

	if (Request.LogicSpec.IsValid())
	{
		Root->SetObjectField(TEXT("logic_spec"), Request.LogicSpec);
	}
	FString JsonText;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	FJsonSerializer::Serialize(Root, Writer);
	return JsonText;
}
