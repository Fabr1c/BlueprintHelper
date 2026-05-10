#include "Systems/ToolClusters/GraphWrite/OperationHandlers/AddFunctionGraphHandler.h"

#include "Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraph/EdGraph.h"
#include "Dom/JsonObject.h"

bool FAddFunctionGraphHandler::CanHandle(const FString& OpName) const
{
	return OpName.Equals(TEXT("add_function_graph"), ESearchCase::IgnoreCase);
}

bool FAddFunctionGraphHandler::Execute(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& OpPayload, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("add_function_graph 失败：蓝图无效。");
		return false;
	}

	FString FuncName;
	if (!OpPayload->TryGetStringField(TEXT("name"), FuncName) || FuncName.IsEmpty())
	{
		OutError = TEXT("add_function_graph 失败：缺。name 字段。");
		return false;
	}

	// 检查函数图是否已存在
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*FuncName))
		{
			// 已存在，幂等成功
			return true;
		}
	}

	// 创建函数图
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*FuncName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);
	if (!NewGraph)
	{
		OutError = FString::Printf(TEXT("add_function_graph '%s' 失败：无法创建新图表。"), *FuncName);
		return false;
	}

	FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, NewGraph, /*bIsUserCreated=*/ true, nullptr);

	// 查找入口节点
	UK2Node_FunctionEntry* EntryNode = nullptr;
	for (UEdGraphNode* Node : NewGraph->Nodes)
	{
		EntryNode = Cast<UK2Node_FunctionEntry>(Node);
		if (EntryNode)
		{
			break;
		}
	}

	// 设置 is_pure 标记
	bool bIsPure = false;
	if (OpPayload->TryGetBoolField(TEXT("is_pure"), bIsPure) && bIsPure && EntryNode)
	{
		EntryNode->SetExtraFlags(EntryNode->GetExtraFlags() | FUNC_BlueprintPure);
	}

	// 添加输入引脚
	const TArray<TSharedPtr<FJsonValue>>* InputsArray = nullptr;
	if (OpPayload->TryGetArrayField(TEXT("inputs"), InputsArray) && InputsArray && EntryNode)
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
			PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean; // 默认

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
			EntryNode->UserDefinedPins.Add(NewPin);
		}

		EntryNode->ReconstructNode();
	}

	// 添加输出引脚（查找或创建 Result 节点）
	const TArray<TSharedPtr<FJsonValue>>* OutputsArray = nullptr;
	if (OpPayload->TryGetArrayField(TEXT("outputs"), OutputsArray) && OutputsArray && OutputsArray->Num() > 0)
	{
		// 查找已有 Result 节点，没有则创建
		UK2Node_FunctionResult* ResultNode = nullptr;
		for (UEdGraphNode* Node : NewGraph->Nodes)
		{
			ResultNode = Cast<UK2Node_FunctionResult>(Node);
			if (ResultNode)
			{
				break;
			}
		}

		if (!ResultNode)
		{
			FGraphNodeCreator<UK2Node_FunctionResult> NodeCreator(*NewGraph);
			ResultNode = NodeCreator.CreateNode(true);
			ResultNode->NodePosX = 600;
			ResultNode->NodePosY = 0;
			NodeCreator.Finalize();
		}

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
			PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

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
			ResultNode->UserDefinedPins.Add(NewPin);
		}

		ResultNode->ReconstructNode();
	}

	return true;
}
