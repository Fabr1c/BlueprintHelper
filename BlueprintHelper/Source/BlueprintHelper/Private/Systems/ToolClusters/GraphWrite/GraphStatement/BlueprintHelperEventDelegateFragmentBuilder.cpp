#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.h"

#include "BlueprintNodeBinder.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_BaseMCDelegate.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperDelegateLinkFragmentUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateBindingObjectResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "UObject/UnrealType.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/GraphWriteGraphStatementUtils.h"

bool FBlueprintHelperEventDelegateFragmentBuilder::BuildStatement(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	OutError.Reset();

	const EBlueprintHelperActionSemanticKind SemanticKind = UGraphWriteGraphStatementUtils::ToEventDelegateActionSemanticKind(Statement.Kind);
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Unknown)
	{
		OutError = FString::Printf(TEXT("event_delegate fragment build failed: unsupported statement kind '%s'."), *Statement.PatternName);
		return false;
	}

	const FString StatementId = UGraphWriteGraphStatementUtils::GetEventDelegateStatementId(Statement.StatementId, Statement.Path);
	FBlueprintHelperActionResolutionRequest ActionRequest;
	if (!UGraphWriteGraphStatementUtils::BuildActionRequestEventDelegate(TargetGraph, ActionContextScope, StatementId, ActionRequest, OutError))
	{
		return false;
	}

	FBlueprintHelperActionResolutionResult ActionResult;
	if (!UGraphWriteGraphStatementUtils::ResolveEventDelegateAction(ActionRequest, ActionResult, OutError))
	{
		return false;
	}

	FBlueprintHelperEventDelegateUseSiteEvidence Evidence;
	FString MissingDetail;
	FString MissingMessage;
	if (!FBlueprintHelperEventDelegateUseSiteEvidenceReader::TryRead(
		ActionRequest,
		SemanticKind,
		Evidence,
		MissingDetail,
		MissingMessage))
	{
		OutError = FString::Printf(TEXT("%s: %s"), *MissingDetail, *MissingMessage);
		return false;
	}

	UK2Node* PrimaryNode = UGraphWriteGraphStatementUtils::SpawnResolvedPrimaryNode(
		TargetGraph,
		ActionResult,
		Evidence,
		Statement,
		StatementId,
		OutError);
	if (!PrimaryNode)
	{
		return false;
	}

	OutFragment.FragmentId = StatementId;
	OutFragment.SourceStatementId = StatementId;
	OutFragment.PrimaryNode = PrimaryNode;
	OutFragment.Nodes.Add(PrimaryNode);
	UGraphWriteGraphStatementUtils::PopulatePrimaryPins(PrimaryNode, OutFragment);
	UGraphWriteGraphStatementUtils::PopulateCommonFragmentMetadata(
		StatementId,
		FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind),
		Evidence,
		OutFragment);

	if (SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
		&& !UGraphWriteGraphStatementUtils::ConnectProjectedBindingObjectToPrimaryTarget(TargetGraph, Evidence, PrimaryNode, OutFragment, OutError))
	{
		return false;
	}
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
		&& Evidence.DelegateOperation.Equals(TEXT("call"), ESearchCase::IgnoreCase)
		&& !UGraphWriteGraphStatementUtils::ValidateAndRecordDelegateCallArgs(ActionRequest, Statement, PrimaryNode, OutFragment, OutError))
	{
		return false;
	}

	if (SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
		&& UGraphWriteGraphStatementUtils::IsDelegateReferenceOperation(Evidence.DelegateOperation))
	{
		UK2Node_BaseMCDelegate* PrimaryDelegateNode = Cast<UK2Node_BaseMCDelegate>(PrimaryNode);
		if (!PrimaryDelegateNode)
		{
			OutError = TEXT("delegate reference fragment build failed: primary node is not a multicast delegate node.");
			return false;
		}

		FBlueprintHelperDelegateLinkRequest LinkRequest;
		LinkRequest.FragmentId = StatementId;
		LinkRequest.HandlerName = Evidence.HandlerName;
		LinkRequest.HandlerFunctionPath = Evidence.HandlerFunctionPath;
		LinkRequest.HandlerScopeClassPath = Evidence.HandlerScopeClassPath;
		LinkRequest.SignatureEvidenceId = Evidence.SignatureEvidenceId;
		LinkRequest.CreateDelegateLocation = FVector2D(-220.0, 120.0);
		if (!FBlueprintHelperDelegateLinkFragmentUtils::AttachCreateDelegateToPrimary(
			TargetGraph,
			PrimaryDelegateNode,
			LinkRequest,
			OutFragment,
			OutError))
		{
			return false;
		}
	}

	return true;
}
