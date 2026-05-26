#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperDelegateLinkFragmentUtils.h"

#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_CreateDelegate.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

namespace
{
static FString MakeDelegatePinDiagnostic(
	const FString& DiagnosticPrefix,
	const TCHAR* CodeSuffix,
	const FString& Message)
{
	const FString Prefix = DiagnosticPrefix.TrimStartAndEnd().IsEmpty()
		? TEXT("delegate")
		: DiagnosticPrefix.TrimStartAndEnd();
	return FString::Printf(TEXT("%s_%s: %s"), *Prefix, CodeSuffix, *Message);
}
}

UK2Node_CreateDelegate* FBlueprintHelperDelegateLinkFragmentUtils::SpawnCreateDelegateNode(
	UEdGraph* TargetGraph,
	const FBlueprintHelperDelegateLinkRequest& Request,
	FString& OutError)
{
	if (!TargetGraph)
	{
		OutError = TEXT("create delegate spawn failed: target graph is invalid.");
		return nullptr;
	}
	if (Request.HandlerName.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("create delegate spawn failed: handler evidence is missing.");
		return nullptr;
	}

	UBlueprintNodeSpawner* CreateDelegateSpawner = UBlueprintNodeSpawner::Create(UK2Node_CreateDelegate::StaticClass());
	if (!CreateDelegateSpawner)
	{
		OutError = TEXT("create delegate spawn failed: node spawner unavailable.");
		return nullptr;
	}

	const FString NodeId = Request.FragmentId + TEXT(":create_delegate");
	FBlueprintHelperActionNodeSpawnOptions Options;
	Options.NodeId = NodeId;
	Options.bReconstructAfterSpawn = false;

	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeNodeSpawner(
		TargetGraph,
		CreateDelegateSpawner,
		Options.NodeId,
		Request.CreateDelegateLocation,
		Options,
		OutError);
	UK2Node_CreateDelegate* CreateDelegateNode = Cast<UK2Node_CreateDelegate>(SpawnedNode);
	if (!CreateDelegateNode)
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("create delegate spawn failed: spawner did not create UK2Node_CreateDelegate.");
		}
		return nullptr;
	}

	CreateDelegateNode->SetFunction(FName(*Request.HandlerName.TrimStartAndEnd()));
	return CreateDelegateNode;
}

UEdGraphPin* FBlueprintHelperDelegateLinkFragmentUtils::ResolveDelegateInputPin(
	UK2Node* PrimaryNode,
	const FString& DelegateInputPinName,
	FString& OutError)
{
	return ResolveDelegateInputPin(PrimaryNode, DelegateInputPinName, TEXT("delegate"), OutError);
}

UEdGraphPin* FBlueprintHelperDelegateLinkFragmentUtils::ResolveDelegateInputPin(
	UK2Node* PrimaryNode,
	const FString& DelegateInputPinName,
	const FString& DiagnosticPrefix,
	FString& OutError)
{
	if (!PrimaryNode)
	{
		OutError = MakeDelegatePinDiagnostic(
			DiagnosticPrefix,
			TEXT("pin_missing"),
			TEXT("create delegate link failed: primary node is invalid."));
		return nullptr;
	}

	const FString TrimmedPinName = DelegateInputPinName.TrimStartAndEnd();
	if (!TrimmedPinName.IsEmpty())
	{
		UEdGraphPin* NamedPin = PrimaryNode->FindPin(FName(*TrimmedPinName), EGPD_Input);
		if (!NamedPin)
		{
			NamedPin = FBlueprintGraphWriteFacade::FindPinByAlias(PrimaryNode, TrimmedPinName);
		}
		if (!NamedPin)
		{
			OutError = MakeDelegatePinDiagnostic(
				DiagnosticPrefix,
				TEXT("pin_missing"),
				FString::Printf(TEXT("create delegate link failed: delegate input pin '%s' was not found."), *TrimmedPinName));
			return nullptr;
		}
		if (NamedPin->Direction != EGPD_Input)
		{
			OutError = MakeDelegatePinDiagnostic(
				DiagnosticPrefix,
				TEXT("pin_missing"),
				FString::Printf(TEXT("create delegate link failed: delegate pin '%s' is not an input pin."), *TrimmedPinName));
			return nullptr;
		}
		return NamedPin;
	}

	if (UK2Node_BaseMCDelegate* PrimaryDelegateNode = Cast<UK2Node_BaseMCDelegate>(PrimaryNode))
	{
		if (UEdGraphPin* DelegatePin = PrimaryDelegateNode->GetDelegatePin())
		{
			return DelegatePin;
		}
	}

	TArray<UEdGraphPin*> DelegatePins;
	for (UEdGraphPin* Pin : PrimaryNode->Pins)
	{
		if (!Pin || Pin->Direction != EGPD_Input)
		{
			continue;
		}
		if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Delegate
			|| Pin->PinName.ToString().Equals(TEXT("delegate"), ESearchCase::IgnoreCase)
			|| Pin->PinName.ToString().Equals(TEXT("event"), ESearchCase::IgnoreCase))
		{
			DelegatePins.Add(Pin);
		}
	}

	if (DelegatePins.Num() == 1)
	{
		return DelegatePins[0];
	}
	if (DelegatePins.Num() > 1)
	{
		OutError = MakeDelegatePinDiagnostic(
			DiagnosticPrefix,
			TEXT("pin_ambiguous"),
			TEXT("create delegate link failed: primary node has multiple delegate input pins; schedule_delegate_pin_name evidence is required."));
		return nullptr;
	}

	OutError = MakeDelegatePinDiagnostic(
		DiagnosticPrefix,
		TEXT("pin_missing"),
		TEXT("create delegate link failed: delegate input pin is missing."));
	return nullptr;
}

bool FBlueprintHelperDelegateLinkFragmentUtils::ConnectCreateDelegateToPin(
	UEdGraph* TargetGraph,
	UEdGraphPin* DelegateInPin,
	UK2Node_CreateDelegate* CreateDelegateNode,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	if (!TargetGraph || !TargetGraph->GetSchema())
	{
		OutError = TEXT("create delegate link failed: graph schema is invalid.");
		return false;
	}
	if (!DelegateInPin || !CreateDelegateNode)
	{
		OutError = TEXT("create delegate link failed: delegate nodes are invalid.");
		return false;
	}

	UEdGraphPin* DelegateOutPin = CreateDelegateNode->GetDelegateOutPin();
	if (!DelegateOutPin)
	{
		OutError = TEXT("create delegate link failed: create delegate output pin is missing.");
		return false;
	}
	if (!TargetGraph->GetSchema()->TryCreateConnection(DelegateOutPin, DelegateInPin))
	{
		OutError = TEXT("create delegate link failed: schema rejected delegate pin connection.");
		return false;
	}

	FBlueprintHelperFragmentLink Link;
	Link.From = FBlueprintHelperFragmentPinRef{ TEXT("create_delegate"), DelegateOutPin->PinName.ToString(), DelegateOutPin->PinType.PinCategory.ToString(), DelegateOutPin };
	Link.To = FBlueprintHelperFragmentPinRef{ TEXT("primary"), DelegateInPin->PinName.ToString(), DelegateInPin->PinType.PinCategory.ToString(), DelegateInPin };
	OutFragment.InternalLinks.Add(Link);
	OutFragment.PinBindings.Add(TEXT("create_delegate.event"), Link.From);
	OutFragment.PinBindings.Add(TEXT("delegate.event"), Link.To);
	return true;
}

bool FBlueprintHelperDelegateLinkFragmentUtils::AttachCreateDelegateToPrimary(
	UEdGraph* TargetGraph,
	UK2Node* PrimaryNode,
	const FBlueprintHelperDelegateLinkRequest& Request,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	UEdGraphPin* DelegateInPin = ResolveDelegateInputPin(
		PrimaryNode,
		Request.DelegateInputPinName,
		Request.DiagnosticPrefix,
		OutError);
	if (!DelegateInPin)
	{
		return false;
	}

	UK2Node_CreateDelegate* CreateDelegateNode = SpawnCreateDelegateNode(TargetGraph, Request, OutError);
	if (!CreateDelegateNode)
	{
		return false;
	}

	OutFragment.Nodes.Add(CreateDelegateNode);
	if (!Request.HandlerFunctionPath.IsEmpty())
	{
		OutFragment.OwnershipTags.Add(TEXT("handler_function_path"), Request.HandlerFunctionPath);
	}
	if (!Request.HandlerScopeClassPath.IsEmpty())
	{
		OutFragment.OwnershipTags.Add(TEXT("handler_scope_class_path"), Request.HandlerScopeClassPath);
	}
	if (!Request.SignatureEvidenceId.IsEmpty())
	{
		OutFragment.OwnershipTags.Add(TEXT("signature_evidence_id"), Request.SignatureEvidenceId);
	}
	return ConnectCreateDelegateToPin(TargetGraph, DelegateInPin, CreateDelegateNode, OutFragment, OutError);
}
