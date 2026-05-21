#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeUtility.h"

#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphExistingNodeMapper.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLinker.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLocalVariableService.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeSpawner.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeUtility.h"
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

UEdGraph* FBlueprintGraphNodeUtility::ResolveMacroGraph(const FParsedMacroReference& MacroReference, FString& OutErrorMessage)
{
	OutErrorMessage.Reset();
	FString BlueprintPath = MacroReference.MacroAssetPath;
	if (BlueprintPath.IsEmpty() || MacroReference.LibraryType.Equals(TEXT("standard"), ESearchCase::IgnoreCase))
	{
		BlueprintPath = FBlueprintGraphNodeUtility::StandardMacroLibraryPath;
	}

	UBlueprint* MacroLibrary = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!MacroLibrary)
	{
		OutErrorMessage = FString::Printf(TEXT("无法加载宏库蓝图：%s"), *BlueprintPath);
		return nullptr;
	}

	for (UEdGraph* MacroGraph : MacroLibrary->MacroGraphs)
	{
		if (MacroGraph && MacroGraph->GetName().Equals(MacroReference.MacroName, ESearchCase::IgnoreCase))
		{
			return MacroGraph;
		}
	}

	OutErrorMessage = FString::Printf(TEXT("在宏库中未找到宏图：%s"), *MacroReference.MacroName);
	return nullptr;
}

UEdGraphPin* FBlueprintGraphNodeUtility::FindPinByAlias(UK2Node* TargetNode, const FString& RequestedPinName)
{
	if (!TargetNode || RequestedPinName.IsEmpty())
	{
		return nullptr;
	}

	if (UEdGraphPin* ExactPin = TargetNode->FindPin(RequestedPinName))
	{
		return ExactPin;
	}

	const FString NormalizedKey = FBlueprintGraphNodeUtility::NormalizePinKey(RequestedPinName);
	for (UEdGraphPin* Pin : TargetNode->Pins)
	{
		if (!Pin)
		{
			continue;
		}

		if (FBlueprintGraphNodeUtility::NormalizePinKey(Pin->PinName.ToString()) == NormalizedKey)
		{
			return Pin;
		}
	}

	if (NormalizedKey == FBlueprintGraphNodeUtility::NormalizePinKey(TEXT("execute")))
	{
		return TargetNode->FindPin(UEdGraphSchema_K2::PN_Execute);
	}

	if (NormalizedKey == FBlueprintGraphNodeUtility::NormalizePinKey(TEXT("then")))
	{
		return TargetNode->FindPin(UEdGraphSchema_K2::PN_Then);
	}

	if (NormalizedKey == FBlueprintGraphNodeUtility::NormalizePinKey(TEXT("completed")))
	{
		if (UEdGraphPin* CompletedPin = TargetNode->FindPin(UEdGraphSchema_K2::PN_Completed))
		{
			return CompletedPin;
		}
	}

	if (NormalizedKey == FBlueprintGraphNodeUtility::NormalizePinKey(TEXT("loopbody")))
	{
		if (UEdGraphPin* LoopBodyPin = TargetNode->FindPin(TEXT("LoopBody")))
		{
			return LoopBodyPin;
		}
	}

	if (NormalizedKey == FBlueprintGraphNodeUtility::NormalizePinKey(TEXT("firstindex")))
	{
		if (UEdGraphPin* FirstIndexPin = TargetNode->FindPin(TEXT("FirstIndex")))
		{
			return FirstIndexPin;
		}
	}

	if (NormalizedKey == FBlueprintGraphNodeUtility::NormalizePinKey(TEXT("lastindex")))
	{
		if (UEdGraphPin* LastIndexPin = TargetNode->FindPin(TEXT("LastIndex")))
		{
			return LastIndexPin;
		}
	}

	if (NormalizedKey == FBlueprintGraphNodeUtility::NormalizePinKey(TEXT("index")))
	{
		if (UEdGraphPin* IndexPin = TargetNode->FindPin(TEXT("Index")))
		{
			return IndexPin;
		}
	}

	if (NormalizedKey == FBlueprintGraphNodeUtility::NormalizePinKey(TEXT("value")))
	{
		for (UEdGraphPin* Pin : TargetNode->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				continue;
			}

			if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Object || !Pin->PinName.ToString().Equals(TEXT("self"), ESearchCase::IgnoreCase))
			{
				return Pin;
			}
		}
	}

	// ── DynamicCast 别名 ──
	if (NormalizedKey == FBlueprintGraphNodeUtility::NormalizePinKey(TEXT("valid")) || NormalizedKey == FBlueprintGraphNodeUtility::NormalizePinKey(TEXT("cast_succeeded")))
	{
		if (UEdGraphPin* ValidPin = TargetNode->FindPin(UEdGraphSchema_K2::PN_CastSucceeded))
		{
			return ValidPin;
		}
	}

	if (NormalizedKey == FBlueprintGraphNodeUtility::NormalizePinKey(TEXT("invalid")) || NormalizedKey == FBlueprintGraphNodeUtility::NormalizePinKey(TEXT("cast_failed")))
	{
		if (UEdGraphPin* FailedPin = TargetNode->FindPin(TEXT("CastFailed")))
		{
			return FailedPin;
		}
	}

	if (NormalizedKey == FBlueprintGraphNodeUtility::NormalizePinKey(TEXT("cast_result")))
	{
		for (UEdGraphPin* Pin : TargetNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output && Pin->PinName.ToString().StartsWith(TEXT("As")))
			{
				return Pin;
			}
		}
	}

	if (NormalizedKey == FBlueprintGraphNodeUtility::NormalizePinKey(TEXT("success")) || NormalizedKey == FBlueprintGraphNodeUtility::NormalizePinKey(TEXT("bsuccess")) || NormalizedKey == FBlueprintGraphNodeUtility::NormalizePinKey(TEXT("bool_success")))
	{
		if (UEdGraphPin* SuccessPin = TargetNode->FindPin(TEXT("bSuccess")))
		{
			return SuccessPin;
		}
	}

	return nullptr;
}

FBlueprintHelperCallFunctionResolveResult FBlueprintGraphNodeUtility::ResolveFunctionForGraph(
	UEdGraph* TargetGraph,
	const FString& FunctionQuery,
	const TMap<FString, FString>& DefaultValues)
{
	FBlueprintHelperCallFunctionResolveRequest Request;
	Request.Blueprint = TargetGraph ? FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph) : nullptr;
	Request.Graph = TargetGraph;
	Request.Query = FunctionQuery;
	DefaultValues.GetKeys(Request.ArgumentNames);
	return FBlueprintHelperCallFunctionResolver::Resolve(Request);
}

TArray<TSharedPtr<FEngineFunctionItem>> FBlueprintGraphNodeUtility::GetAllBlueprintFunctions()
{
	TArray<TSharedPtr<FEngineFunctionItem>> Result;

	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (!Class || Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		for (TFieldIterator<UFunction> FuncIt(Class, EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt)
		{
			UFunction* Function = *FuncIt;
			if (!Function || !Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure))
			{
				continue;
			}

			TSharedPtr<FEngineFunctionItem> Item = MakeShared<FEngineFunctionItem>();
			Item->FunctionPtr = Function;
			Item->NativeFunctionName = Function->GetName();
			Item->FunctionName = Item->NativeFunctionName;
			if (Function->HasMetaData(TEXT("DisplayName")))
			{
				Item->FunctionName = Function->GetMetaData(TEXT("DisplayName"));
			}
			Item->Category = Function->HasMetaData(TEXT("Category")) ? Function->GetMetaData(TEXT("Category")) : TEXT("Default");
			Result.Add(Item);
		}
	}

	Result.Sort([](const TSharedPtr<FEngineFunctionItem>& Left, const TSharedPtr<FEngineFunctionItem>& Right)
	{
		if (!Left.IsValid() || !Right.IsValid())
		{
			return Left.IsValid();
		}

		return Left->FunctionName < Right->FunctionName;
	});

	return Result;
}
