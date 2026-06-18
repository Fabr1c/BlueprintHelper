#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_SetFieldsInStruct.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentBuildUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/GraphWriteGraphStatementUtils.h"

namespace BlueprintHelperFieldFragmentBuilderLocal
{
	static FString FirstNonEmpty(const TArray<FString>& Values)
	{
		for (const FString& Value : Values)
		{
			const FString CleanValue = Value.TrimStartAndEnd();
			if (!CleanValue.IsEmpty())
			{
				return CleanValue;
			}
		}
		return FString();
	}

	static FString ReadCandidateFact(
		const FBlueprintHelperActionResolutionResult& ActionResult,
		const FString& Key)
	{
		if (ActionResult.CandidateActions.Num() == 0 || Key.IsEmpty())
		{
			return FString();
		}

		const FBlueprintHelperActionCandidate& Candidate = ActionResult.CandidateActions[0];
		if (const FString* Value = Candidate.CapabilityFacts.Find(Key))
		{
			return Value->TrimStartAndEnd();
		}
		if (const FString* Value = Candidate.ReadbackFacts.Find(Key))
		{
			return Value->TrimStartAndEnd();
		}
		return FString();
	}

	static FString ResolveEntryScope(
		UEdGraph* TargetGraph,
		const FBlueprintHelperGraphFragmentBuildRequest& Request,
		const FBlueprintHelperActionResolutionResult& ActionResult)
	{
		return FirstNonEmpty({
			Request.CapabilityFacts.FindRef(TEXT("field.function_name")),
			Request.CapabilityFacts.FindRef(TEXT("field.local_scope")),
			ReadCandidateFact(ActionResult, TEXT("field.function_name")),
			ReadCandidateFact(ActionResult, TEXT("function_name")),
			ReadCandidateFact(ActionResult, TEXT("field.local_scope")),
			ReadCandidateFact(ActionResult, TEXT("local_scope")),
			TargetGraph ? TargetGraph->GetName() : FString()
		});
	}

	static FString ResolveEntryParamName(
		const FBlueprintHelperGraphFragmentBuildRequest& Request,
		const FBlueprintHelperActionResolutionResult& ActionResult)
	{
		return FirstNonEmpty({
			Request.CapabilityFacts.FindRef(TEXT("field.member_name")),
			ReadCandidateFact(ActionResult, TEXT("field.member_name")),
			ReadCandidateFact(ActionResult, TEXT("member_name")),
			Request.Target,
			Request.Query,
			Request.PropertyPath
		});
	}

	static bool IsFunctionParamGetRequest(
		const FBlueprintHelperGraphFragmentBuildRequest& Request,
		const FBlueprintHelperActionResolutionResult& ActionResult)
	{
		if (Request.CapabilityId.Equals(TEXT("field.function_param_get"), ESearchCase::IgnoreCase))
		{
			return true;
		}
		if (ActionResult.CandidateActions.Num() == 0)
		{
			return false;
		}

		const FBlueprintHelperActionCandidate& Candidate = ActionResult.CandidateActions[0];
		return Candidate.CapabilityId.Equals(TEXT("field.function_param_get"), ESearchCase::IgnoreCase)
			|| Candidate.CapabilityFacts.FindRef(TEXT("field.kind")).Equals(TEXT("function_param"), ESearchCase::IgnoreCase)
			|| Candidate.ReadbackFacts.FindRef(TEXT("field_kind")).Equals(TEXT("function_param"), ESearchCase::IgnoreCase);
	}

	static bool EventEntryMatchesScope(const UK2Node_Event* EventNode, const FString& ScopeName)
	{
		if (!EventNode)
		{
			return false;
		}
		if (ScopeName.IsEmpty())
		{
			return true;
		}
		if (const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(EventNode))
		{
			return CustomEvent->CustomFunctionName.ToString().Equals(ScopeName, ESearchCase::IgnoreCase);
		}

		const FString EventMemberName = EventNode->EventReference.GetMemberName().ToString();
		return EventMemberName.Equals(ScopeName, ESearchCase::IgnoreCase)
			|| EventNode->GetNodeTitle(ENodeTitleType::ListView).ToString().Contains(ScopeName);
	}

	static UK2Node_Event* FindGraphEntryNode(UEdGraph* TargetGraph, const FString& ScopeName)
	{
		if (!TargetGraph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : TargetGraph->Nodes)
		{
			UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
			if (EventEntryMatchesScope(EventNode, ScopeName))
			{
				return EventNode;
			}
		}
		return nullptr;
	}

	static UEdGraphPin* FindEntryOutputPin(UK2Node_Event* EventNode, const FString& ParamName)
	{
		if (!EventNode || ParamName.IsEmpty())
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : EventNode->Pins)
		{
			if (Pin
				&& Pin->Direction == EGPD_Output
				&& Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec
				&& Pin->PinName.ToString().Equals(ParamName, ESearchCase::IgnoreCase))
			{
				return Pin;
			}
		}
		return nullptr;
	}

	static void AddDataOutputAlias(
		FBlueprintHelperNodeFragment& Fragment,
		const FString& Alias,
		const FBlueprintHelperFragmentPinRef& PinRef)
	{
		const FString CleanAlias = Alias.TrimStartAndEnd();
		if (CleanAlias.IsEmpty())
		{
			return;
		}

		if (!Fragment.DataOutputs.Contains(CleanAlias))
		{
			FBlueprintHelperFragmentPinRef AliasRef = PinRef;
			AliasRef.PinName = CleanAlias;
			Fragment.DataOutputs.Add(CleanAlias, AliasRef);
		}
		if (!Fragment.PinBindings.Contains(CleanAlias))
		{
			FBlueprintHelperFragmentPinRef AliasRef = PinRef;
			AliasRef.PinName = CleanAlias;
			Fragment.PinBindings.Add(CleanAlias, AliasRef);
		}
	}

	static bool BuildGraphEntryParamFragment(
		UEdGraph* TargetGraph,
		const FBlueprintHelperGraphFragmentBuildRequest& Request,
		const FBlueprintHelperActionResolutionResult& ActionResult,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError)
	{
		OutFragment = FBlueprintHelperNodeFragment();
		if (!TargetGraph)
		{
			OutError = TEXT("graph entry param fragment build failed: target graph is null.");
			return false;
		}

		const FString ScopeName = ResolveEntryScope(TargetGraph, Request, ActionResult);
		const FString ParamName = ResolveEntryParamName(Request, ActionResult);
		UK2Node_Event* EntryNode = FindGraphEntryNode(TargetGraph, ScopeName);
		UEdGraphPin* ParamPin = FindEntryOutputPin(EntryNode, ParamName);
		if (!EntryNode || !ParamPin)
		{
			OutError = FString::Printf(
				TEXT("graph entry param fragment build failed: scope=%s param=%s."),
				*ScopeName,
				*ParamName);
			return false;
		}

		OutFragment.FragmentId = Request.FragmentId;
		OutFragment.SourceStatementId = Request.SourceStatementId.IsEmpty() ? Request.FragmentId : Request.SourceStatementId;
		OutFragment.PrimaryNode = EntryNode;
		OutFragment.Nodes.Add(EntryNode);
		FBlueprintHelperActionFragmentBuildUtils::PopulateCommonMetadata(Request, OutFragment);

		const FBlueprintHelperFragmentPinRef PinRef{
			Request.FragmentId,
			ParamPin->PinName.ToString(),
			ParamPin->PinType.PinCategory.ToString(),
			ParamPin
		};
		AddDataOutputAlias(OutFragment, ParamPin->PinName.ToString(), PinRef);
		AddDataOutputAlias(OutFragment, ParamPin->PinName.ToString().ToLower(), PinRef);
		AddDataOutputAlias(OutFragment, TEXT("value"), PinRef);
		AddDataOutputAlias(OutFragment, TEXT("result"), PinRef);
		AddDataOutputAlias(OutFragment, TEXT("return"), PinRef);

		if (ActionResult.CandidateActions.Num() > 0)
		{
			const FBlueprintHelperActionCandidate& Candidate = ActionResult.CandidateActions[0];
			FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.capability_id"), Candidate.CapabilityId);
			FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.expected_node_family"), TEXT("graph_entry_param"));
			FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.expected_node_class"), EntryNode->GetClass()->GetPathName());
			FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.entry_scope"), ScopeName);
			FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.member_name"), ParamPin->PinName.ToString());
			for (const TPair<FString, FString>& FactPair : Candidate.ReadbackFacts)
			{
				FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.readback.") + FactPair.Key, FactPair.Value);
			}
		}
		return true;
	}
}

FString FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(const FString& CapabilityId)
{
	const FBlueprintHelperFieldCapabilitySpec* Spec = FBlueprintHelperFieldCapabilityRegistry::FindById(CapabilityId);
	return Spec ? Spec->ExpectedNodeFamily : FString();
}

bool FBlueprintHelperFieldFragmentBuilder::DoesCapabilityProduceExecPins(const FString& CapabilityId)
{
	const FBlueprintHelperFieldCapabilitySpec* Spec = FBlueprintHelperFieldCapabilityRegistry::FindById(CapabilityId);
	return Spec && Spec->bProducesExecPins;
}

bool FBlueprintHelperFieldFragmentBuilder::BuildVariableGetFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	const FBlueprintHelperActionResolutionResult& ActionResult,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	if (BlueprintHelperFieldFragmentBuilderLocal::IsFunctionParamGetRequest(Request, ActionResult))
	{
		FString GraphEntryParamError;
		if (BlueprintHelperFieldFragmentBuilderLocal::BuildGraphEntryParamFragment(
			TargetGraph,
			Request,
			ActionResult,
			OutFragment,
			GraphEntryParamError))
		{
			return true;
		}
		if (TargetGraph && !FBlueprintEditorUtils::DoesSupportLocalVariables(TargetGraph))
		{
			OutError = GraphEntryParamError;
			return false;
		}
	}
	return UGraphWriteGraphStatementUtils::BuildVariableFragment(TargetGraph, Request, ActionResult, OutFragment, OutError);
}

bool FBlueprintHelperFieldFragmentBuilder::BuildVariableSetFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	const FBlueprintHelperActionResolutionResult& ActionResult,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	return UGraphWriteGraphStatementUtils::BuildVariableFragment(TargetGraph, Request, ActionResult, OutFragment, OutError);
}

bool FBlueprintHelperFieldFragmentBuilder::BuildStructReadFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperFieldFragmentPlan& Plan,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	return UGraphWriteGraphStatementUtils::BuildStructNodeFragment(TargetGraph, Plan, TEXT("struct_read"), false, OutFragment, OutError);
}

bool FBlueprintHelperFieldFragmentBuilder::BuildStructWriteFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperFieldFragmentPlan& Plan,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	return UGraphWriteGraphStatementUtils::BuildStructNodeFragment(TargetGraph, Plan, TEXT("struct_write"), true, OutFragment, OutError);
}

bool FBlueprintHelperFieldFragmentBuilder::BuildNestedPropertyPathFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperFieldFragmentPlan& Plan,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	return UGraphWriteGraphStatementUtils::BuildNestedStructBreakPathFragment(TargetGraph, Plan, OutFragment, OutError);
}
