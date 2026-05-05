#include "OperationHandlers/GraphWrite/AddEventDispatcherHandler.h"

#include "K2Node_FunctionEntry.h"
#include "GraphWrite/TextToBlueprintGenerator.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Dom/JsonObject.h"

bool FAddEventDispatcherHandler::CanHandle(const FString& OpName) const
{
	return OpName.Equals(TEXT("add_event_dispatcher"), ESearchCase::IgnoreCase);
}

bool FAddEventDispatcherHandler::Execute(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& OpPayload, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("add_event_dispatcher 失败：蓝图无效。");
		return false;
	}

	FString DispatcherName;
	if (!OpPayload->TryGetStringField(TEXT("name"), DispatcherName) || DispatcherName.IsEmpty())
	{
		OutError = TEXT("add_event_dispatcher 失败：缺。name 字段。");
		return false;
	}

	const FName DispatcherFName(*DispatcherName);
	const int32 ExistingVarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, DispatcherFName);
	UEdGraph* DelegateSignatureGraph = FBlueprintEditorUtils::GetDelegateSignatureGraphByName(Blueprint, DispatcherFName);
	bool bDispatcherExists = false;

	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName == DispatcherFName && Var.VarType.PinCategory == UEdGraphSchema_K2::PC_MCDelegate)
		{
			bDispatcherExists = true;
			break;
		}
	}

	if (!bDispatcherExists && ExistingVarIndex != INDEX_NONE)
	{
		OutError = FString::Printf(TEXT("add_event_dispatcher '%s' 失败：同名变量已存在且不是事件分发器类型。"), *DispatcherName);
		return false;
	}

	bool bCreatedVariableThisCall = false;
	if (!bDispatcherExists)
	{
		FEdGraphPinType DelegatePinType;
		DelegatePinType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

		const bool bAdded = FBlueprintEditorUtils::AddMemberVariable(Blueprint, DispatcherFName, DelegatePinType);
		if (!bAdded)
		{
			OutError = FString::Printf(TEXT("add_event_dispatcher '%s' 失败：无法创建事件分发器。"), *DispatcherName);
			return false;
		}

		bCreatedVariableThisCall = true;
	}

	if (!DelegateSignatureGraph)
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		check(K2Schema != nullptr);

		DelegateSignatureGraph = FBlueprintEditorUtils::CreateNewGraph(
			Blueprint,
			DispatcherFName,
			UEdGraph::StaticClass(),
			UEdGraphSchema_K2::StaticClass());
		if (!DelegateSignatureGraph)
		{
			if (bCreatedVariableThisCall)
			{
				FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, DispatcherFName);
			}

			OutError = FString::Printf(TEXT("add_event_dispatcher '%s' 失败：无法创建委托签名图。"), *DispatcherName);
			return false;
		}

		DelegateSignatureGraph->bEditable = false;
		K2Schema->CreateDefaultNodesForGraph(*DelegateSignatureGraph);
		K2Schema->CreateFunctionGraphTerminators(*DelegateSignatureGraph, (UClass*)nullptr);
		K2Schema->AddExtraFunctionFlags(DelegateSignatureGraph, (FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_Public));
		K2Schema->MarkFunctionEntryAsEditable(DelegateSignatureGraph, true);

		Blueprint->DelegateSignatureGraphs.Add(DelegateSignatureGraph);
	}

	const TArray<TSharedPtr<FJsonValue>>* ParamsArray = nullptr;
	if (OpPayload->TryGetArrayField(TEXT("params"), ParamsArray) && ParamsArray)
	{
		for (UEdGraphNode* Node : DelegateSignatureGraph->Nodes)
		{
			UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node);
			if (!EntryNode)
			{
				continue;
			}

			for (const TSharedPtr<FJsonValue>& ParamValue : *ParamsArray)
			{
				const TSharedPtr<FJsonObject> ParamObj = ParamValue->AsObject();
				if (!ParamObj.IsValid())
				{
					continue;
				}

				FString ParamName;
				ParamObj->TryGetStringField(TEXT("name"), ParamName);
				if (ParamName.IsEmpty())
				{
					continue;
				}

				const FName ParamFName(*ParamName);
				const bool bPinExists = EntryNode->UserDefinedPins.ContainsByPredicate(
					[&ParamFName](const TSharedPtr<FUserPinInfo>& ExistingPin)
					{
						return ExistingPin.IsValid() && ExistingPin->PinName == ParamFName;
					});
				if (bPinExists)
				{
					continue;
				}

				const TSharedPtr<FJsonObject>* PinTypeObj = nullptr;
				FEdGraphPinType PinType;
				PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

				if (ParamObj->TryGetObjectField(TEXT("pin_type"), PinTypeObj) && PinTypeObj && PinTypeObj->IsValid())
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
				NewPin->PinName = ParamFName;
				NewPin->PinType = PinType;
				NewPin->DesiredPinDirection = EGPD_Output;
				EntryNode->UserDefinedPins.Add(NewPin);
			}

			EntryNode->ReconstructNode();
			break;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	return true;
}
