#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentBuildUtils.h"

#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

void FBlueprintHelperActionFragmentBuildUtils::PopulatePins(
	const EBlueprintHelperActionFragmentPinProfile PinProfile,
	UK2Node* Node,
	FBlueprintHelperNodeFragment& OutFragment)
{
	switch (PinProfile)
	{
	case EBlueprintHelperActionFragmentPinProfile::Call:
		PopulateCallFragmentPins(Node, OutFragment);
		break;
	case EBlueprintHelperActionFragmentPinProfile::ActionProvider:
	default:
		PopulateActionProviderFragmentPins(Node, OutFragment);
		break;
	}
}

void FBlueprintHelperActionFragmentBuildUtils::PopulateCallFragmentPins(
	UK2Node* CallNode,
	FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(CallNode, TEXT("execute"));
	OutFragment.ExecExitPin = FBlueprintGraphWriteFacade::FindPinByAlias(CallNode, TEXT("then"));
	OutFragment.PinBindings.Add(TEXT("execute"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("execute"), TEXT("exec"), OutFragment.ExecEntryPin });
	OutFragment.PinBindings.Add(TEXT("then"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("then"), TEXT("exec"), OutFragment.ExecExitPin });
	if (!CallNode)
	{
		return;
	}

	for (UEdGraphPin* Pin : CallNode->Pins)
	{
		if (!Pin || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			continue;
		}

		const FString PinName = Pin->PinName.ToString();
		FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), PinName, Pin->PinType.PinCategory.ToString(), Pin };
		OutFragment.PinBindings.Add(PinName, PinRef);
		if (Pin->Direction == EGPD_Input)
		{
			OutFragment.DataInputs.Add(PinName, PinRef);
		}
		else if (Pin->Direction == EGPD_Output)
		{
			OutFragment.DataOutputs.Add(PinName, PinRef);
			if (!OutFragment.DataOutputs.Contains(TEXT("return")))
			{
				OutFragment.DataOutputs.Add(TEXT("return"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("return"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("return"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("return"), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!OutFragment.DataOutputs.Contains(TEXT("result")))
			{
				OutFragment.DataOutputs.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("result"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("result"), Pin->PinType.PinCategory.ToString(), Pin });
			}
		}
	}
}

void FBlueprintHelperActionFragmentBuildUtils::PopulateActionProviderFragmentPins(
	UK2Node* Node,
	FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(Node, TEXT("execute"));
	OutFragment.ExecExitPin = FBlueprintGraphWriteFacade::FindPinByAlias(Node, TEXT("then"));
	if (OutFragment.ExecEntryPin)
	{
		OutFragment.PinBindings.Add(TEXT("execute"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("execute"), TEXT("exec"), OutFragment.ExecEntryPin });
	}
	if (OutFragment.ExecExitPin)
	{
		OutFragment.PinBindings.Add(TEXT("then"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("then"), TEXT("exec"), OutFragment.ExecExitPin });
	}
	if (!Node)
	{
		return;
	}

	int32 DataInputIndex = 0;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			continue;
		}

		const FString PinName = Pin->PinName.ToString();
		FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), PinName, Pin->PinType.PinCategory.ToString(), Pin };
		OutFragment.PinBindings.Add(PinName, PinRef);
		if (!OutFragment.PinBindings.Contains(PinName.ToLower()))
		{
			OutFragment.PinBindings.Add(PinName.ToLower(), FBlueprintHelperFragmentPinRef{ TEXT("primary"), PinName.ToLower(), Pin->PinType.PinCategory.ToString(), Pin });
		}
		if (Pin->Direction == EGPD_Input)
		{
			OutFragment.DataInputs.Add(PinName, PinRef);
			if (!OutFragment.DataInputs.Contains(PinName.ToLower()))
			{
				OutFragment.DataInputs.Add(PinName.ToLower(), FBlueprintHelperFragmentPinRef{ TEXT("primary"), PinName.ToLower(), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!PinName.Equals(TEXT("self"), ESearchCase::IgnoreCase) && !OutFragment.DataInputs.Contains(TEXT("value")))
			{
				OutFragment.DataInputs.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("value"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("value"), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!PinName.Equals(TEXT("self"), ESearchCase::IgnoreCase))
			{
				if (DataInputIndex == 0 && !OutFragment.DataInputs.Contains(TEXT("left")))
				{
					OutFragment.DataInputs.Add(TEXT("left"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("left"), Pin->PinType.PinCategory.ToString(), Pin });
					OutFragment.PinBindings.Add(TEXT("left"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("left"), Pin->PinType.PinCategory.ToString(), Pin });
					if (!OutFragment.DataInputs.Contains(TEXT("condition")))
					{
						OutFragment.DataInputs.Add(TEXT("condition"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("condition"), Pin->PinType.PinCategory.ToString(), Pin });
						OutFragment.PinBindings.Add(TEXT("condition"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("condition"), Pin->PinType.PinCategory.ToString(), Pin });
					}
				}
				else if (DataInputIndex == 1 && !OutFragment.DataInputs.Contains(TEXT("right")))
				{
					OutFragment.DataInputs.Add(TEXT("right"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("right"), Pin->PinType.PinCategory.ToString(), Pin });
					OutFragment.PinBindings.Add(TEXT("right"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("right"), Pin->PinType.PinCategory.ToString(), Pin });
				}
				++DataInputIndex;
			}
		}
		else if (Pin->Direction == EGPD_Output)
		{
			OutFragment.DataOutputs.Add(PinName, PinRef);
			if (!OutFragment.DataOutputs.Contains(PinName.ToLower()))
			{
				OutFragment.DataOutputs.Add(PinName.ToLower(), FBlueprintHelperFragmentPinRef{ TEXT("primary"), PinName.ToLower(), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!OutFragment.DataOutputs.Contains(TEXT("value")))
			{
				OutFragment.DataOutputs.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("value"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("value"), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!OutFragment.DataOutputs.Contains(TEXT("result")))
			{
				OutFragment.DataOutputs.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("result"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("result"), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!OutFragment.DataOutputs.Contains(TEXT("return")))
			{
				OutFragment.DataOutputs.Add(TEXT("return"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("return"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("return"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("return"), Pin->PinType.PinCategory.ToString(), Pin });
			}
		}
	}
}

void FBlueprintHelperActionFragmentBuildUtils::PopulateCommonMetadata(
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.OwnershipTags.Add(TEXT("statement_id"), Request.FragmentId);
	OutFragment.ReviewTargets.Add(Request.FragmentId);
	// DEPRECATED_LAYOUT: these x/y hints are legacy spawn metadata only.
	// Final node positions must come from the UE-side GraphLayout system.
	OutFragment.LayoutHints.Add(TEXT("x"), LexToString(Request.Location.X));
	OutFragment.LayoutHints.Add(TEXT("y"), LexToString(Request.Location.Y));
}

void FBlueprintHelperActionFragmentBuildUtils::AppendCandidateActionGroup(
	const FString& Target,
	const FBlueprintHelperActionResolutionResult& ResolveResult,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions)
{
	if (!OutCandidateFunctions || ResolveResult.CandidateActions.Num() == 0)
	{
		return;
	}

	FBlueprintHelperCandidateFunctionGroup Group;
	Group.Target = Target;
	Group.Candidates = ResolveResult.CandidateActions;
	OutCandidateFunctions->Add(MoveTemp(Group));
}
