#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.h"

#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphExistingNodeMapper.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLinker.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLocalVariableService.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeSpawner.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeUtility.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteContext.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintMultiGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphComposer.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectIterator.h"
#include "EdGraphNode_Comment.h"

bool FBlueprintGraphDefaultValueApplier::ApplyPinDefaultValue(
	UEdGraphPin* TargetPin,
	const FString& InValue,
	FString& OutDiagnosticCode,
	FString& OutMessage)
{
	OutDiagnosticCode.Reset();
	OutMessage.Reset();

	if (!TargetPin)
	{
		OutDiagnosticCode = TEXT("default_pin_not_found");
		OutMessage = TEXT("默认值应用失败：目标引脚无效。");
		return false;
	}

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if (!K2Schema)
	{
		OutDiagnosticCode = TEXT("default_value_rejected");
		OutMessage = TEXT("默认值应用失败：K2 Schema 无效。");
		return false;
	}

	if (TargetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object || TargetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
	{
		UObject* DefaultObject = nullptr;
		const bool bNoneValue = InValue.IsEmpty()
			|| InValue.Equals(TEXT("None"), ESearchCase::IgnoreCase)
			|| InValue.Equals(TEXT("null"), ESearchCase::IgnoreCase)
			|| InValue.Equals(TEXT("nullptr"), ESearchCase::IgnoreCase);

		if (!bNoneValue)
		{
			DefaultObject = LoadObject<UObject>(nullptr, *InValue);
			if (!DefaultObject)
			{
				OutDiagnosticCode = TEXT("default_value_object_not_found");
				OutMessage = FString::Printf(TEXT("默认值对象或类无法加载：%s。"), *InValue);
				return false;
			}
		}

		FString ValidationMessage;
		if (!K2Schema->DefaultValueSimpleValidation(TargetPin->PinType, TargetPin->PinName, FString(), DefaultObject, FText::GetEmpty(), &ValidationMessage))
		{
			OutDiagnosticCode = TEXT("default_value_rejected");
			OutMessage = ValidationMessage.IsEmpty()
				? FString::Printf(TEXT("Schema 拒绝默认值：%s。"), *InValue)
				: ValidationMessage;
			return false;
		}

		TargetPin->GetSchema()->TrySetDefaultObject(*TargetPin, DefaultObject);
		return true;
	}

	if (TargetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
	{
		const FText TextValue = FText::FromString(InValue);
		FString ValidationMessage;
		if (!K2Schema->DefaultValueSimpleValidation(TargetPin->PinType, TargetPin->PinName, FString(), nullptr, TextValue, &ValidationMessage))
		{
			OutDiagnosticCode = TEXT("default_value_rejected");
			OutMessage = ValidationMessage.IsEmpty()
				? FString::Printf(TEXT("Schema 拒绝文本默认值：%s。"), *InValue)
				: ValidationMessage;
			return false;
		}

		TargetPin->GetSchema()->TrySetDefaultText(*TargetPin, TextValue);
		return true;
	}

	FString ValidationMessage;
	if (!K2Schema->DefaultValueSimpleValidation(TargetPin->PinType, TargetPin->PinName, InValue, nullptr, FText::GetEmpty(), &ValidationMessage))
	{
		OutDiagnosticCode = TEXT("default_value_rejected");
		OutMessage = ValidationMessage.IsEmpty()
			? FString::Printf(TEXT("Schema 拒绝默认值：%s。"), *InValue)
			: ValidationMessage;
		return false;
	}

	TargetPin->GetSchema()->TrySetDefaultValue(*TargetPin, InValue);
	return true;
}

TArray<FBlueprintGeneratorDiagnostic> FBlueprintGraphDefaultValueApplier::ApplyDefaultValues(
	FBlueprintGraphWriteContext& Context,
	const FString& NodeId,
	const TMap<FString, FString>& DefaultValues)
{
	TArray<FBlueprintGeneratorDiagnostic> Diagnostics;
	for (const TPair<FString, FString>& Pair : DefaultValues)
	{
		UEdGraphPin* Pin = Context.FindPinByAlias(NodeId, Pair.Key);
		if (!Pin)
		{
			Diagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("default_pin_not_found"),
				NodeId,
				Pair.Key,
				FString::Printf(TEXT("默认值引脚未找到：%s。"), *Pair.Key)));
			continue;
		}

		FString DiagnosticCode;
		FString DiagnosticMessage;
		if (!FBlueprintGraphDefaultValueApplier::ApplyPinDefaultValue(Pin, Pair.Value, DiagnosticCode, DiagnosticMessage))
		{
			Diagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				DiagnosticCode.IsEmpty() ? TEXT("default_value_rejected") : DiagnosticCode,
				NodeId,
				Pair.Key,
				DiagnosticMessage.IsEmpty()
					? FString::Printf(TEXT("默认值被拒绝：%s。"), *Pair.Value)
					: DiagnosticMessage));
		}
	}
	return Diagnostics;
}

TArray<FBlueprintGeneratorDiagnostic> FBlueprintGraphDefaultValueApplier::ApplyDefaultValues(
	UK2Node* TargetNode,
	const TMap<FString, FString>& DefaultValues,
	const FString& NodeId)
{
	TArray<FBlueprintGeneratorDiagnostic> Diagnostics;
	if (!TargetNode)
	{
		for (const auto& Pair : DefaultValues)
		{
			Diagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("default_pin_not_found"),
				NodeId,
				Pair.Key,
				FString::Printf(TEXT("默认值 '%s' 无法应用：目标节点无效。"), *Pair.Key)));
		}
		return Diagnostics;
	}

	FBlueprintGraphWriteContext Context;
	Context.Initialize(TargetNode->GetGraph());
	Context.RegisterNode(NodeId.IsEmpty() ? TEXT("__node__") : NodeId, TargetNode, false);
	return FBlueprintGraphDefaultValueApplier::ApplyDefaultValues(
		Context,
		NodeId.IsEmpty() ? TEXT("__node__") : NodeId,
		DefaultValues);
}
