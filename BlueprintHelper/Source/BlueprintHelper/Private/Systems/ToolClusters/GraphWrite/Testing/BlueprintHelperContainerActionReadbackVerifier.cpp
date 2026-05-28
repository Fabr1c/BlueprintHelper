#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperContainerActionReadbackVerifier.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"
#include "Systems/ToolClusters/GraphWrite/Testing/Utils/GraphWriteTestingUtils.h"

bool FBlueprintHelperContainerActionReadbackVerifier::Verify(
	const UBlueprint* Blueprint,
	const UEdGraph* Graph,
	const FBlueprintHelperContainerActionReadbackExpectation& Expectation,
	FString& OutFailure)
{
	OutFailure.Reset();
	if (!Blueprint)
	{
		OutFailure = TEXT("container_action readback failed: Blueprint is null.");
		return false;
	}
	if (!Graph)
	{
		OutFailure = TEXT("container_action readback failed: Graph is null.");
		return false;
	}

	const FString OperationId = UGraphWriteTestingUtils::MakeOperationId(Expectation);
	const FBlueprintHelperContainerActionSpec* Spec =
		FBlueprintHelperContainerActionVocabulary::Find(Expectation.ContainerKind, Expectation.ContainerOperation);
	if (!Spec)
	{
		OutFailure = FString::Printf(TEXT("container_action readback failed: unsupported operation %s."), *OperationId);
		return false;
	}
	if (!Spec->OperationId.Equals(OperationId, ESearchCase::IgnoreCase))
	{
		OutFailure = FString::Printf(
			TEXT("container_action readback failed: operation id mismatch, expected %s but vocabulary has %s."),
			*OperationId,
			*Spec->OperationId);
		return false;
	}

	UK2Node_CallFunction* CallNode = UGraphWriteTestingUtils::FindContainerActionNode(Graph, UGraphWriteTestingUtils::ExtractFunctionName(Spec->StableUFunctionPath.IsEmpty() ? Spec->FunctionQuery : Spec->StableUFunctionPath));
	if (!CallNode)
	{
		OutFailure = FString::Printf(TEXT("container_action readback failed: generated node not found for %s."), *OperationId);
		return false;
	}

	const TArray<FString>& RolesToVerify = Expectation.RequiredRoles.Num() > 0
		? Expectation.RequiredRoles
		: Spec->ReadbackPinRoles;
	for (const FString& Role : RolesToVerify)
	{
		const FBlueprintHelperContainerActionRoleBinding* Binding = UGraphWriteTestingUtils::FindRoleBinding(*Spec, Role);
		if (!Binding)
		{
			OutFailure = FString::Printf(TEXT("container_action readback failed: role %s has no vocabulary binding."), *Role);
			return false;
		}

		UEdGraphPin* RolePin = UGraphWriteTestingUtils::FindPinByName(CallNode, Binding->FunctionPinName);
		if (!RolePin)
		{
			OutFailure = FString::Printf(
				TEXT("container_action readback failed: role %s pin %s is missing."),
				*Role,
				*Binding->FunctionPinName);
			return false;
		}
		if (UGraphWriteTestingUtils::IsWildcardPin(RolePin))
		{
			OutFailure = FString::Printf(
				TEXT("container_action readback failed: role %s pin %s stayed wildcard."),
				*Role,
				*Binding->FunctionPinName);
			return false;
		}
		FExpectedContainerActionPinType ExpectedRoleType;
		if (UGraphWriteTestingUtils::TryBuildExpectedRolePinType(Expectation, Role, ExpectedRoleType))
		{
			FString TypeReason;
			if (!UGraphWriteTestingUtils::PinMatchesExpectedType(RolePin, ExpectedRoleType, TypeReason))
			{
				OutFailure = FString::Printf(
					TEXT("container_action readback failed: role %s pin %s type mismatch: %s."),
					*Role,
					*Binding->FunctionPinName,
					*TypeReason);
				return false;
			}
		}
	}

	if (!Expectation.TargetName.TrimStartAndEnd().IsEmpty())
	{
		const FBlueprintHelperContainerActionRoleBinding* TargetBinding = UGraphWriteTestingUtils::FindRoleBinding(*Spec, TEXT("target"));
		UEdGraphPin* TargetPin = TargetBinding ? UGraphWriteTestingUtils::FindPinByName(CallNode, TargetBinding->FunctionPinName) : nullptr;
		if (!TargetPin)
		{
			OutFailure = TEXT("container_action readback failed: target pin is missing.");
			return false;
		}
		if (!UGraphWriteTestingUtils::TargetPinLinksToVariable(TargetPin, Expectation.TargetName))
		{
			OutFailure = FString::Printf(
				TEXT("container_action readback failed: target pin is not linked to %s."),
				*Expectation.TargetName);
			return false;
		}
		const EPinContainerType ExpectedType = UGraphWriteTestingUtils::ExpectedContainerType(Expectation.ContainerKind);
		if (ExpectedType != EPinContainerType::None && TargetPin->PinType.ContainerType != ExpectedType)
		{
			OutFailure = FString::Printf(
				TEXT("container_action readback failed: target pin container type mismatch for %s."),
				*Expectation.ContainerKind);
			return false;
		}
	}

	if (Expectation.bRequiresOutput && !UGraphWriteTestingUtils::HasNonWildcardOutput(CallNode))
	{
		OutFailure = FString::Printf(TEXT("container_action readback failed: output pin missing or wildcard for %s."), *OperationId);
		return false;
	}
	if (Expectation.bRequiresOutput)
	{
		UEdGraphPin* OutputPin = UGraphWriteTestingUtils::FindFirstNonWildcardOutput(CallNode);
		FExpectedContainerActionPinType ExpectedResultType;
		if (UGraphWriteTestingUtils::TryBuildExpectedResultPinType(Expectation, ExpectedResultType))
		{
			FString TypeReason;
			if (!UGraphWriteTestingUtils::PinMatchesExpectedType(OutputPin, ExpectedResultType, TypeReason))
			{
				OutFailure = FString::Printf(
					TEXT("container_action readback failed: output pin type mismatch for %s: %s."),
					*OperationId,
					*TypeReason);
				return false;
			}
		}
	}
	if (Expectation.bRequiresExecFlow && !UGraphWriteTestingUtils::HasLinkedExecFlow(CallNode))
	{
		OutFailure = FString::Printf(TEXT("container_action readback failed: exec flow is not fully linked for %s."), *OperationId);
		return false;
	}

	return true;
}
