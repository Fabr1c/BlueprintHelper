#include "Systems/ToolClusters/GraphWrite/NodeHandlers/CallFunctionNodeHandler.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.h"

namespace
{
static bool TryParseExplicitObjectCall(
	const FBlueprintHelperCallFunctionResolveResult& ResolveResult,
	const FString& FunctionQuery,
	FString& OutObjectName,
	FString& OutFunctionName)
{
	OutObjectName.Reset();
	OutFunctionName.Reset();

	if (!ResolveResult.ErrorCode.Equals(TEXT("explicit_member_call_not_supported"), ESearchCase::IgnoreCase))
	{
		return false;
	}

	if (!FBlueprintHelperCallFunctionResolver::TryParseQualifiedQuery(FunctionQuery, OutObjectName, OutFunctionName))
	{
		return false;
	}

	OutObjectName.TrimStartAndEndInline();
	OutFunctionName.TrimStartAndEndInline();
	return !OutObjectName.IsEmpty()
		&& !OutFunctionName.IsEmpty()
		&& !OutObjectName.StartsWith(TEXT("/Script/"));
}

static UEdGraphPin* FindFirstDataOutputPin(UK2Node* Node)
{
	if (!Node)
	{
		return nullptr;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
		{
			return Pin;
		}
	}
	return nullptr;
}

static UEdGraphPin* FindCallTargetPin(UK2Node* CallNode)
{
	if (!CallNode)
	{
		return nullptr;
	}

	const TCHAR* AliasCandidates[] = {
		TEXT("self"),
		TEXT("target"),
		TEXT("Target"),
		TEXT("object"),
		TEXT("Object"),
	};
	for (const TCHAR* Alias : AliasCandidates)
	{
		if (UEdGraphPin* Pin = TextToBlueprintGenerator::FindPinByAlias(CallNode, Alias))
		{
			if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				return Pin;
			}
		}
	}

	for (UEdGraphPin* Pin : CallNode->Pins)
	{
		if (Pin
			&& Pin->Direction == EGPD_Input
			&& Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec
			&& (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object
				|| Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Interface))
		{
			return Pin;
		}
	}
	return nullptr;
}

static UK2Node* SpawnExplicitObjectCall(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	const FString& ObjectName,
	const FString& FunctionName,
	FString& OutError)
{
	const FBlueprintHelperCallFunctionResolveResult ObjectCallResolveResult =
		TextToBlueprintGenerator::ResolveFunctionForGraph(TargetGraph, FunctionName, NodeData.DefaultValues);
	if (!ObjectCallResolveResult.IsResolved())
	{
		OutError = ObjectCallResolveResult.Message.IsEmpty()
			? FString::Printf(TEXT("explicit object call resolve failed: %s"), *FunctionName)
			: ObjectCallResolveResult.Message;
		return nullptr;
	}

	UK2Node* CallNode = FBlueprintHelperCallFunctionResolver::SpawnResolvedNode(
		TargetGraph,
		ObjectCallResolveResult.Selected,
		FVector2D(NodeData.X, NodeData.Y),
		OutError);
	if (!CallNode)
	{
		return nullptr;
	}

	TextToBlueprintGenerator::ApplyDefaultValues(CallNode, NodeData.DefaultValues, NodeData.Id);

	FParsedNode ObjectGetterData;
	ObjectGetterData.Id = NodeData.Id + TEXT("_target");
	ObjectGetterData.NodeType = EParsedBlueprintNodeType::VariableGet;
	ObjectGetterData.SourceType = TEXT("K2Node_VariableGet");
	ObjectGetterData.X = NodeData.X - 260.0f;
	ObjectGetterData.Y = NodeData.Y;
	ObjectGetterData.VariableReference.ScopeType = TEXT("member");
	ObjectGetterData.VariableReference.VariableName = ObjectName;
	ObjectGetterData.VariableReference.bSelfContext = true;

	FString ObjectGetterError;
	UK2Node* ObjectGetterNode = TextToBlueprintGenerator::SpawnVariableGetNode(TargetGraph, ObjectGetterData, ObjectGetterError);
	if (!ObjectGetterNode)
	{
		OutError = ObjectGetterError.IsEmpty()
			? FString::Printf(TEXT("explicit object call target not found: %s"), *ObjectName)
			: ObjectGetterError;
		return nullptr;
	}

	UEdGraphPin* ObjectOutputPin = FindFirstDataOutputPin(ObjectGetterNode);
	UEdGraphPin* CallTargetPin = FindCallTargetPin(CallNode);
	if (!ObjectOutputPin || !CallTargetPin)
	{
		OutError = FString::Printf(
			TEXT("explicit object call target pin not found: %s.%s"),
			*ObjectName,
			*FunctionName);
		return nullptr;
	}

	const UEdGraphSchema* Schema = TargetGraph ? TargetGraph->GetSchema() : nullptr;
	if (!Schema)
	{
		OutError = TEXT("explicit object call connection failed: graph schema is invalid.");
		return nullptr;
	}

	const FPinConnectionResponse ConnectionResponse = Schema->CanCreateConnection(ObjectOutputPin, CallTargetPin);
	if (!Schema->TryCreateConnection(ObjectOutputPin, CallTargetPin))
	{
		OutError = ConnectionResponse.Message.IsEmpty()
			? FString::Printf(TEXT("explicit object call connection rejected: %s.%s"), *ObjectName, *FunctionName)
			: ConnectionResponse.Message.ToString();
		return nullptr;
	}

	return CallNode;
}
}

bool FCallFunctionNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::CallFunction;
}

UK2Node* FCallFunctionNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	const FBlueprintHelperCallFunctionResolveResult ResolveResult =
		TextToBlueprintGenerator::ResolveFunctionForGraph(TargetGraph, NodeData.FunctionName, NodeData.DefaultValues);

	if (!ResolveResult.IsResolved())
	{
		FString ObjectName;
		FString FunctionName;
		if (TryParseExplicitObjectCall(ResolveResult, NodeData.FunctionName, ObjectName, FunctionName))
		{
			return SpawnExplicitObjectCall(TargetGraph, NodeData, ObjectName, FunctionName, OutError);
		}

		OutError = ResolveResult.Message.IsEmpty()
			? FString::Printf(TEXT("call_function resolve failed: %s"), *NodeData.FunctionName)
			: ResolveResult.Message;
		return nullptr;
	}

	UK2Node* SpawnedNode = FBlueprintHelperCallFunctionResolver::SpawnResolvedNode(
		TargetGraph,
		ResolveResult.Selected,
		FVector2D(NodeData.X, NodeData.Y),
		OutError);

	if (SpawnedNode)
	{
		TextToBlueprintGenerator::ApplyDefaultValues(SpawnedNode, NodeData.DefaultValues, NodeData.Id);
	}
	return SpawnedNode;
}
