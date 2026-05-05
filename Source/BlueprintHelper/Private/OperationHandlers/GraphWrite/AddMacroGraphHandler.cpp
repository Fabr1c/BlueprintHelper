#include "OperationHandlers/GraphWrite/AddMacroGraphHandler.h"

#include "GraphWrite/TextToBlueprintGenerator.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_Tunnel.h"
#include "EdGraph/EdGraph.h"
#include "Dom/JsonObject.h"

bool FAddMacroGraphHandler::CanHandle(const FString& OpName) const
{
	return OpName.Equals(TEXT("add_macro_graph"), ESearchCase::IgnoreCase);
}

bool FAddMacroGraphHandler::Execute(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& OpPayload, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("add_macro_graph 失败：蓝图无效。");
		return false;
	}

	FString MacroName;
	if (!OpPayload->TryGetStringField(TEXT("name"), MacroName) || MacroName.IsEmpty())
	{
		OutError = TEXT("add_macro_graph 失败：缺。name 字段。");
		return false;
	}

	// 检查宏图是否已存在
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*MacroName))
		{
			return true;
		}
	}

	// 创建宏图
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*MacroName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);
	if (!NewGraph)
	{
		OutError = FString::Printf(TEXT("add_macro_graph '%s' 失败：无法创建新图表。"), *MacroName);
		return false;
	}

	FBlueprintEditorUtils::AddMacroGraph(Blueprint, NewGraph, /*bIsUserCreated=*/ true, nullptr);

	// 查找 Tunnel 入口 / 出口节点并添加用户定义引脚
	UK2Node_Tunnel* InputNode = nullptr;
	UK2Node_Tunnel* OutputNode = nullptr;
	for (UEdGraphNode* Node : NewGraph->Nodes)
	{
		UK2Node_Tunnel* TunnelNode = Cast<UK2Node_Tunnel>(Node);
		if (!TunnelNode)
		{
			continue;
		}

		// 宏图会有两个 Tunnel 节点：Inputs (bCanHaveOutputs=true) 。Outputs (bCanHaveInputs=true)
		if (TunnelNode->bCanHaveOutputs && !InputNode)
		{
			InputNode = TunnelNode;
		}
		else if (TunnelNode->bCanHaveInputs && !OutputNode)
		{
			OutputNode = TunnelNode;
		}
	}

	// 添加输入引脚（宏入口 Tunnel 上是 output pin）
	const TArray<TSharedPtr<FJsonValue>>* InputsArray = nullptr;
	if (OpPayload->TryGetArrayField(TEXT("inputs"), InputsArray) && InputsArray && InputNode)
	{
		for (const TSharedPtr<FJsonValue>& InputValue : *InputsArray)
		{
			const TSharedPtr<FJsonObject> InputObj = InputValue->AsObject();
			if (!InputObj.IsValid())
			{
				continue;
			}

			FString ParamName;
			InputObj->TryGetStringField(TEXT("name"), ParamName);
			if (ParamName.IsEmpty())
			{
				continue;
			}

			const TSharedPtr<FJsonObject>* PinTypeObj = nullptr;
			FEdGraphPinType PinType;
			PinType.PinCategory = UEdGraphSchema_K2::PC_Exec;

			if (InputObj->TryGetObjectField(TEXT("pin_type"), PinTypeObj) && PinTypeObj && PinTypeObj->IsValid())
			{
				FParsedPinType ParsedPinType;
				(*PinTypeObj)->TryGetStringField(TEXT("category"), ParsedPinType.Category);
				(*PinTypeObj)->TryGetStringField(TEXT("sub_category"), ParsedPinType.SubCategory);
				(*PinTypeObj)->TryGetStringField(TEXT("object_path"), ParsedPinType.SubCategoryObjectPath);
				(*PinTypeObj)->TryGetStringField(TEXT("container_type"), ParsedPinType.ContainerType);

				FString ConvertError;
				TextToBlueprintGenerator::ConvertToEdGraphPinType(ParsedPinType, PinType, ConvertError);
			}

			TSharedPtr<FUserPinInfo> NewPin = MakeShared<FUserPinInfo>();
			NewPin->PinName = FName(*ParamName);
			NewPin->PinType = PinType;
			NewPin->DesiredPinDirection = EGPD_Output;
			InputNode->UserDefinedPins.Add(NewPin);
		}

		InputNode->ReconstructNode();
	}

	// 添加输出引脚（宏出口 Tunnel 上是 input pin）
	const TArray<TSharedPtr<FJsonValue>>* OutputsArray = nullptr;
	if (OpPayload->TryGetArrayField(TEXT("outputs"), OutputsArray) && OutputsArray && OutputNode)
	{
		for (const TSharedPtr<FJsonValue>& OutputValue : *OutputsArray)
		{
			const TSharedPtr<FJsonObject> OutputObj = OutputValue->AsObject();
			if (!OutputObj.IsValid())
			{
				continue;
			}

			FString ParamName;
			OutputObj->TryGetStringField(TEXT("name"), ParamName);
			if (ParamName.IsEmpty())
			{
				continue;
			}

			const TSharedPtr<FJsonObject>* PinTypeObj = nullptr;
			FEdGraphPinType PinType;
			PinType.PinCategory = UEdGraphSchema_K2::PC_Exec;

			if (OutputObj->TryGetObjectField(TEXT("pin_type"), PinTypeObj) && PinTypeObj && PinTypeObj->IsValid())
			{
				FParsedPinType ParsedPinType;
				(*PinTypeObj)->TryGetStringField(TEXT("category"), ParsedPinType.Category);
				(*PinTypeObj)->TryGetStringField(TEXT("sub_category"), ParsedPinType.SubCategory);
				(*PinTypeObj)->TryGetStringField(TEXT("object_path"), ParsedPinType.SubCategoryObjectPath);
				(*PinTypeObj)->TryGetStringField(TEXT("container_type"), ParsedPinType.ContainerType);

				FString ConvertError;
				TextToBlueprintGenerator::ConvertToEdGraphPinType(ParsedPinType, PinType, ConvertError);
			}

			TSharedPtr<FUserPinInfo> NewPin = MakeShared<FUserPinInfo>();
			NewPin->PinName = FName(*ParamName);
			NewPin->PinType = PinType;
			NewPin->DesiredPinDirection = EGPD_Input;
			OutputNode->UserDefinedPins.Add(NewPin);
		}

		OutputNode->ReconstructNode();
	}

	return true;
}
