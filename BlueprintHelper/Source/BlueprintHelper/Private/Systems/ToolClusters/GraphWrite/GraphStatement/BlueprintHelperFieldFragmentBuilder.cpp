#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_SetFieldsInStruct.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentBuildUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentUtils.h"

namespace
{
static bool BuildVariableFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	const FBlueprintHelperActionResolutionResult& ActionResult,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	if (!TargetGraph)
	{
		OutError = TEXT("field fragment build failed: target graph is null.");
		return false;
	}
	if (!ActionResult.IsResolved())
	{
		OutError = ActionResult.Message.IsEmpty()
			? FString(TEXT("field fragment build failed: action result is not resolved."))
			: ActionResult.Message;
		return false;
	}

	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = Request.FragmentId;
	SpawnOptions.DefaultValues = Request.DefaultValues;
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		TargetGraph,
		ActionResult,
		FVector2D(Request.Location.X, Request.Location.Y),
		SpawnOptions,
		OutError);
	if (!SpawnedNode)
	{
		return false;
	}

	OutFragment.FragmentId = Request.FragmentId;
	OutFragment.SourceStatementId = Request.SourceStatementId.IsEmpty() ? Request.FragmentId : Request.SourceStatementId;
	OutFragment.PrimaryNode = SpawnedNode;
	OutFragment.Nodes.Add(SpawnedNode);
	FBlueprintHelperActionFragmentBuildUtils::PopulateActionProviderFragmentPins(SpawnedNode, OutFragment);
	FBlueprintHelperActionFragmentBuildUtils::PopulateCommonMetadata(Request, OutFragment);
	if (ActionResult.CandidateActions.Num() > 0)
	{
		const FBlueprintHelperActionCandidate& Candidate = ActionResult.CandidateActions[0];
		FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.capability_id"), Candidate.CapabilityId);
		FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.expected_node_family"), Candidate.ExpectedNodeFamily);
		FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.expected_node_class"), Candidate.ExpectedNodeClassPath);
		for (const TPair<FString, FString>& FactPair : Candidate.ReadbackFacts)
		{
			FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.readback.") + FactPair.Key, FactPair.Value);
		}
	}
	return true;
}

static FString FirstPlanFact(
	const FBlueprintHelperFieldFragmentPlan& Plan,
	const TArray<const TCHAR*>& Keys)
{
	for (const TCHAR* Key : Keys)
	{
		if (const FString* Value = Plan.CapabilityFacts.Find(Key))
		{
			const FString CleanValue = Value->TrimStartAndEnd();
			if (!CleanValue.IsEmpty())
			{
				return CleanValue;
			}
		}
	}
	return FString();
}

static UScriptStruct* ResolveStructType(
	const FBlueprintHelperFieldFragmentPlan& Plan,
	FString& OutError)
{
	const FString StructTypePath = FirstPlanFact(
		Plan,
		{
			TEXT("field.struct_type"),
			TEXT("field.root_struct_type"),
			TEXT("field.owner_type"),
			TEXT("field.target_pin_object_path"),
			TEXT("field.target_pin_type")
		});
	if (StructTypePath.IsEmpty())
	{
		OutError = TEXT("field_struct_type_missing");
		return nullptr;
	}

	UScriptStruct* StructType = FindObject<UScriptStruct>(nullptr, *StructTypePath);
	if (!StructType)
	{
		StructType = LoadObject<UScriptStruct>(nullptr, *StructTypePath);
	}
	if (!StructType)
	{
		OutError = FString::Printf(TEXT("field_struct_type_not_found: %s"), *StructTypePath);
	}
	return StructType;
}

static void PopulateFieldPlanTags(
	const FBlueprintHelperFieldFragmentPlan& Plan,
	const FString& FragmentKind,
	FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.OwnershipTags.Add(TEXT("field.capability_id"), Plan.CapabilityId);
	OutFragment.OwnershipTags.Add(TEXT("field.fragment_kind"), FragmentKind);
	OutFragment.OwnershipTags.Add(TEXT("field.expected_node_family"), FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(Plan.CapabilityId));

	const FString PropertyPath = FirstPlanFact(Plan, {TEXT("field.property_path"), TEXT("property_path")});
	FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.property_path"), PropertyPath);
	if (!PropertyPath.IsEmpty())
	{
		TArray<FString> Segments;
		PropertyPath.ParseIntoArray(Segments, TEXT("."), true);
		OutFragment.OwnershipTags.Add(TEXT("field.property_path.segment_count"), FString::FromInt(Segments.Num()));
		if (Segments.Num() > 0)
		{
			FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.property_path.root"), Segments[0]);
			FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.property_path.leaf"), Segments.Last());
		}
	}
}

static TArray<FString> GetPropertyPathSegments(const FBlueprintHelperFieldFragmentPlan& Plan)
{
	const FString PropertyPath = FirstPlanFact(Plan, {TEXT("field.property_path"), TEXT("property_path")});
	TArray<FString> Segments;
	PropertyPath.ParseIntoArray(Segments, TEXT("."), true);
	for (FString& Segment : Segments)
	{
		Segment = Segment.TrimStartAndEnd();
	}
	Segments.RemoveAll([](const FString& Segment)
	{
		return Segment.IsEmpty();
	});

	const FString RootName = FirstPlanFact(Plan, {TEXT("field.root_name"), TEXT("field.root"), TEXT("field.member_name")});
	if (Segments.Num() > 1)
	{
		const FString FirstSegment = Segments[0];
		if ((!Plan.FieldName.IsEmpty() && FirstSegment.Equals(Plan.FieldName, ESearchCase::IgnoreCase))
			|| (!RootName.IsEmpty() && FirstSegment.Equals(RootName, ESearchCase::IgnoreCase)))
		{
			Segments.RemoveAt(0);
		}
	}
	return Segments;
}

static FBlueprintHelperFragmentPinRef MakeFragmentPinRef(
	UEdGraphNode* Node,
	UEdGraphPin* Pin)
{
	FBlueprintHelperFragmentPinRef Ref;
	Ref.NodeId = Node ? Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens) : FString();
	Ref.PinName = Pin ? Pin->PinName.ToString() : FString();
	Ref.Type = Pin ? Pin->PinType.PinCategory.ToString() : FString();
	Ref.Pin = Pin;
	return Ref;
}

static UEdGraphPin* FindDirectionalPin(
	UEdGraphNode* Node,
	const FString& PinName,
	const EEdGraphPinDirection Direction)
{
	if (!Node || PinName.IsEmpty())
	{
		return nullptr;
	}
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == Direction && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			return Pin;
		}
	}
	return nullptr;
}

static UEdGraphPin* FindStructInputPin(UEdGraphNode* Node)
{
	if (!Node)
	{
		return nullptr;
	}
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
		{
			return Pin;
		}
	}
	return nullptr;
}

static void RestoreSetFieldsPinsIfNeeded(UEdGraphNode* Node)
{
}

static void RestoreSetFieldsPinsIfNeeded(UK2Node_SetFieldsInStruct* Node)
{
	if (Node)
	{
		Node->RestoreAllPins();
	}
}

template <typename TNodeType>
static TNodeType* AddStructNode(UEdGraph* TargetGraph, UScriptStruct* StructType)
{
	if (!TargetGraph || !StructType)
	{
		return nullptr;
	}

	TNodeType* StructNode = NewObject<TNodeType>(TargetGraph);
	TargetGraph->AddNode(StructNode, true, false);
	StructNode->CreateNewGuid();
	StructNode->StructType = StructType;
	StructNode->PostPlacedNewNode();
	StructNode->AllocateDefaultPins();
	RestoreSetFieldsPinsIfNeeded(StructNode);
	return StructNode;
}

static bool BuildStructNodeFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperFieldFragmentPlan& Plan,
	const FString& FragmentKind,
	const bool bWrite,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	if (!TargetGraph)
	{
		OutError = FString::Printf(TEXT("%s requires a target graph."), *FragmentKind);
		return false;
	}
	if (!Cast<UEdGraphSchema_K2>(TargetGraph->GetSchema()) || !FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph))
	{
		OutError = FString::Printf(TEXT("%s requires a Blueprint-owned K2 graph."), *FragmentKind);
		return false;
	}

	UScriptStruct* StructType = ResolveStructType(Plan, OutError);
	if (!StructType)
	{
		return false;
	}

	UK2Node* StructNode = bWrite
		? Cast<UK2Node>(AddStructNode<UK2Node_SetFieldsInStruct>(TargetGraph, StructType))
		: Cast<UK2Node>(AddStructNode<UK2Node_BreakStruct>(TargetGraph, StructType));
	if (!StructNode)
	{
		OutError = FString::Printf(TEXT("%s failed to create struct node."), *FragmentKind);
		return false;
	}

	OutFragment.FragmentId = Plan.CapabilityId;
	OutFragment.SourceStatementId = Plan.CapabilityId;
	OutFragment.PrimaryNode = StructNode;
	OutFragment.Nodes.Add(StructNode);
	FBlueprintHelperActionFragmentBuildUtils::PopulateActionProviderFragmentPins(StructNode, OutFragment);
	PopulateFieldPlanTags(Plan, FragmentKind, OutFragment);
	OutFragment.OwnershipTags.Add(TEXT("field.struct_type"), StructType->GetPathName());
	const TArray<FString> Segments = GetPropertyPathSegments(Plan);
	if (Segments.Num() > 0)
	{
		const EEdGraphPinDirection LeafDirection = bWrite ? EGPD_Input : EGPD_Output;
		UEdGraphPin* LeafPin = FindDirectionalPin(StructNode, Segments.Last(), LeafDirection);
		if (LeafPin)
		{
			FBlueprintHelperFragmentPinRef LeafRef = MakeFragmentPinRef(StructNode, LeafPin);
			const FString LeafKey = Segments.Last();
			if (bWrite)
			{
				OutFragment.DataInputs.Add(LeafKey, LeafRef);
			}
			else
			{
				OutFragment.DataOutputs.Add(LeafKey, LeafRef);
			}
			OutFragment.PinBindings.Add(TEXT("field.leaf"), LeafRef);
			FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.leaf_pin"), LeafPin->PinName.ToString());
		}
	}
	return true;
}

static bool BuildNestedStructBreakPathFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperFieldFragmentPlan& Plan,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	if (!TargetGraph)
	{
		OutError = TEXT("nested_property_path requires a target graph.");
		return false;
	}
	const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(TargetGraph->GetSchema());
	if (!K2Schema || !FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph))
	{
		OutError = TEXT("nested_property_path requires a Blueprint-owned K2 graph.");
		return false;
	}

	UScriptStruct* RootStructType = ResolveStructType(Plan, OutError);
	if (!RootStructType)
	{
		return false;
	}

	const TArray<FString> Segments = GetPropertyPathSegments(Plan);
	if (Segments.Num() < 2)
	{
		OutError = TEXT("field_nested_property_path_requires_multiple_segments");
		return false;
	}

	UK2Node_BreakStruct* RootNode = AddStructNode<UK2Node_BreakStruct>(TargetGraph, RootStructType);
	if (!RootNode)
	{
		OutError = TEXT("nested_property_path failed to create root break struct node.");
		return false;
	}

	OutFragment.FragmentId = Plan.CapabilityId;
	OutFragment.SourceStatementId = Plan.CapabilityId;
	OutFragment.PrimaryNode = RootNode;
	OutFragment.Nodes.Add(RootNode);
	FBlueprintHelperActionFragmentBuildUtils::PopulateActionProviderFragmentPins(RootNode, OutFragment);
	PopulateFieldPlanTags(Plan, TEXT("nested_property_path"), OutFragment);
	OutFragment.OwnershipTags.Add(TEXT("field.struct_type"), RootStructType->GetPathName());

	UEdGraphNode* CurrentNode = RootNode;
	for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
	{
		const FString& Segment = Segments[SegmentIndex];
		UEdGraphPin* SegmentPin = FindDirectionalPin(CurrentNode, Segment, EGPD_Output);
		if (!SegmentPin)
		{
			OutError = FString::Printf(TEXT("unknown_struct_property_path: %s"), *Segment);
			return false;
		}

		if (SegmentIndex == Segments.Num() - 1)
		{
			FBlueprintHelperFragmentPinRef LeafRef = MakeFragmentPinRef(CurrentNode, SegmentPin);
			OutFragment.DataOutputs.Add(Segment, LeafRef);
			OutFragment.DataOutputs.Add(TEXT("field.path.leaf"), LeafRef);
			OutFragment.PinBindings.Add(TEXT("field.leaf"), LeafRef);
			FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.leaf_pin"), SegmentPin->PinName.ToString());
			OutFragment.OwnershipTags.Add(TEXT("field.path.break_node_count"), FString::FromInt(OutFragment.Nodes.Num()));
			OutFragment.OwnershipTags.Add(TEXT("field.path.link_count"), FString::FromInt(OutFragment.InternalLinks.Num()));
			return true;
		}

		UScriptStruct* NextStructType = Cast<UScriptStruct>(SegmentPin->PinType.PinSubCategoryObject.Get());
		if (!NextStructType)
		{
			OutError = FString::Printf(TEXT("unknown_struct_property_path: %s is not a struct segment"), *Segment);
			return false;
		}

		UK2Node_BreakStruct* NextNode = AddStructNode<UK2Node_BreakStruct>(TargetGraph, NextStructType);
		UEdGraphPin* NextInputPin = FindStructInputPin(NextNode);
		if (!NextNode || !NextInputPin)
		{
			OutError = FString::Printf(TEXT("nested_property_path failed to create break node for segment: %s"), *Segment);
			return false;
		}
		if (!K2Schema->TryCreateConnection(SegmentPin, NextInputPin))
		{
			OutError = FString::Printf(TEXT("nested_property_path failed to link segment: %s"), *Segment);
			return false;
		}

		FBlueprintHelperFragmentLink Link;
		Link.From = MakeFragmentPinRef(CurrentNode, SegmentPin);
		Link.To = MakeFragmentPinRef(NextNode, NextInputPin);
		OutFragment.InternalLinks.Add(Link);
		OutFragment.Nodes.Add(NextNode);
		CurrentNode = NextNode;
	}

	OutError = TEXT("nested_property_path did not resolve a leaf segment.");
	return false;
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
	return BuildVariableFragment(TargetGraph, Request, ActionResult, OutFragment, OutError);
}

bool FBlueprintHelperFieldFragmentBuilder::BuildVariableSetFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	const FBlueprintHelperActionResolutionResult& ActionResult,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	return BuildVariableFragment(TargetGraph, Request, ActionResult, OutFragment, OutError);
}

bool FBlueprintHelperFieldFragmentBuilder::BuildStructReadFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperFieldFragmentPlan& Plan,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	return BuildStructNodeFragment(TargetGraph, Plan, TEXT("struct_read"), false, OutFragment, OutError);
}

bool FBlueprintHelperFieldFragmentBuilder::BuildStructWriteFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperFieldFragmentPlan& Plan,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	return BuildStructNodeFragment(TargetGraph, Plan, TEXT("struct_write"), true, OutFragment, OutError);
}

bool FBlueprintHelperFieldFragmentBuilder::BuildNestedPropertyPathFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperFieldFragmentPlan& Plan,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	return BuildNestedStructBreakPathFragment(TargetGraph, Plan, OutFragment, OutError);
}
