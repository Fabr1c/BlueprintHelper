// BlueprintHelper Service Layer — 蓝图结构查询与操作服务实现

#include "Shared/Services/BlueprintHelperBlueprintStructureService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Tunnel.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "ScopedTransaction.h"
#include "Dom/JsonObject.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintHelperStructure, Log, All);

class FBlueprintHelperBlueprintStructureServiceLocalUtils
{
public:
static bool ParseStableNodeGuid(const FString& NodeId, FGuid& OutGuid)
{
	return FGuid::ParseExact(NodeId, EGuidFormats::Digits, OutGuid)
		|| FGuid::ParseExact(NodeId, EGuidFormats::DigitsWithHyphens, OutGuid);
}

};
namespace
{
static void ReadParsedPinType(const TSharedPtr<FJsonObject>& PinTypeObject, FParsedPinType& OutParsedPinType)
{
	if (!PinTypeObject.IsValid())
	{
		return;
	}

	PinTypeObject->TryGetStringField(TEXT("category"), OutParsedPinType.Category);
	PinTypeObject->TryGetStringField(TEXT("sub_category"), OutParsedPinType.SubCategory);
	PinTypeObject->TryGetStringField(TEXT("object_path"), OutParsedPinType.SubCategoryObjectPath);
	PinTypeObject->TryGetStringField(TEXT("container_type"), OutParsedPinType.ContainerType);
}

static FParsedPinType ParsedPinTypeFromJson(const TSharedPtr<FJsonObject>& PinTypeObject)
{
	FParsedPinType ParsedPinType;
	ReadParsedPinType(PinTypeObject, ParsedPinType);
	return ParsedPinType;
}

static bool TryConvertPinTypeObject(
	const TSharedPtr<FJsonObject>& PinTypeObject,
	FEdGraphPinType& OutPinType,
	FString& OutError)
{
	return FBlueprintGraphWriteFacade::ConvertToEdGraphPinType(ParsedPinTypeFromJson(PinTypeObject), OutPinType, OutError);
}

static void ReadOptionalPinTypeOrDefault(
	const TSharedPtr<FJsonObject>& Payload,
	const FName DefaultCategory,
	FEdGraphPinType& OutPinType)
{
	OutPinType = FEdGraphPinType();
	OutPinType.PinCategory = DefaultCategory;

	const TSharedPtr<FJsonObject>* PinTypeObject = nullptr;
	if (Payload.IsValid() && Payload->TryGetObjectField(TEXT("pin_type"), PinTypeObject) && PinTypeObject && PinTypeObject->IsValid())
	{
		FString ConvertError;
		FBlueprintGraphWriteFacade::ConvertToEdGraphPinType(ParsedPinTypeFromJson(*PinTypeObject), OutPinType, ConvertError);
	}
}

template <typename TNode>
static void AppendUserPins(
	TNode* Node,
	const TArray<TSharedPtr<FJsonValue>>* PinsArray,
	const EEdGraphPinDirection Direction,
	const FName DefaultCategory,
	const bool bSkipExistingNames)
{
	if (!Node || !PinsArray)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& PinValue : *PinsArray)
	{
		const TSharedPtr<FJsonObject> PinObject = PinValue.IsValid() ? PinValue->AsObject() : nullptr;
		if (!PinObject.IsValid())
		{
			continue;
		}

		FString PinName;
		PinObject->TryGetStringField(TEXT("name"), PinName);
		if (PinName.IsEmpty())
		{
			continue;
		}

		const FName PinFName(*PinName);
		if (bSkipExistingNames)
		{
			const bool bPinExists = Node->UserDefinedPins.ContainsByPredicate(
				[&PinFName](const TSharedPtr<FUserPinInfo>& ExistingPin)
				{
					return ExistingPin.IsValid() && ExistingPin->PinName == PinFName;
				});
			if (bPinExists)
			{
				continue;
			}
		}

		FEdGraphPinType PinType;
		ReadOptionalPinTypeOrDefault(PinObject, DefaultCategory, PinType);

		TSharedPtr<FUserPinInfo> NewPin = MakeShared<FUserPinInfo>();
		NewPin->PinName = PinFName;
		NewPin->PinType = PinType;
		NewPin->DesiredPinDirection = Direction;
		Node->UserDefinedPins.Add(NewPin);
	}

	Node->ReconstructNode();
}

static bool AddMemberVariableDirect(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
	if (!Blueprint || !Payload.IsValid())
	{
		OutError = TEXT("add_member_variable failed: invalid Blueprint or payload.");
		return false;
	}

	FString VarName;
	if (!Payload->TryGetStringField(TEXT("name"), VarName) || VarName.IsEmpty())
	{
		OutError = TEXT("add_member_variable failed: missing name field.");
		return false;
	}

	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName == FName(*VarName))
		{
			return true;
		}
	}

	FEdGraphPinType PinType;
	const TSharedPtr<FJsonObject>* PinTypeObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("pin_type"), PinTypeObject) && PinTypeObject && PinTypeObject->IsValid())
	{
		FString ConvertError;
		if (!TryConvertPinTypeObject(*PinTypeObject, PinType, ConvertError))
		{
			OutError = FString::Printf(TEXT("add_member_variable '%s' failed: %s"), *VarName, *ConvertError);
			return false;
		}
	}
	else
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}

	if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VarName), PinType))
	{
		OutError = FString::Printf(TEXT("add_member_variable '%s' failed: AddMemberVariable returned false."), *VarName);
		return false;
	}

	FString DefaultValue;
	if (Payload->TryGetStringField(TEXT("default_value"), DefaultValue) && !DefaultValue.IsEmpty())
	{
		for (FBPVariableDescription& Var : Blueprint->NewVariables)
		{
			if (Var.VarName == FName(*VarName))
			{
				Var.DefaultValue = DefaultValue;
				break;
			}
		}
	}

	FString Category;
	if (Payload->TryGetStringField(TEXT("category"), Category) && !Category.IsEmpty())
	{
		FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*VarName), nullptr, FText::FromString(Category));
	}

	const TSharedPtr<FJsonObject>* FlagsObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("flags"), FlagsObject) && FlagsObject && FlagsObject->IsValid())
	{
		bool bValue = false;
		if ((*FlagsObject)->TryGetBoolField(TEXT("blueprint_read_only"), bValue) && bValue)
		{
			FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, FName(*VarName), nullptr, FBlueprintMetadata::MD_Private, TEXT("true"));
		}

		if ((*FlagsObject)->TryGetBoolField(TEXT("expose_on_spawn"), bValue) && bValue)
		{
			FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, FName(*VarName), nullptr, FBlueprintMetadata::MD_ExposeOnSpawn, TEXT("true"));
		}
	}

	return true;
}

static bool RemoveMemberVariableDirect(UBlueprint* Blueprint, const FString& VarName, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("remove_member_variable failed: invalid Blueprint.");
		return false;
	}

	if (VarName.IsEmpty())
	{
		OutError = TEXT("remove_member_variable failed: missing name field.");
		return false;
	}

	if (FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, FName(*VarName)) == INDEX_NONE)
	{
		return true;
	}

	FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, FName(*VarName));
	return true;
}

static bool AddFunctionGraphDirect(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
	if (!Blueprint || !Payload.IsValid())
	{
		OutError = TEXT("add_function_graph failed: invalid Blueprint or payload.");
		return false;
	}

	FString FuncName;
	if (!Payload->TryGetStringField(TEXT("name"), FuncName) || FuncName.IsEmpty())
	{
		OutError = TEXT("add_function_graph failed: missing name field.");
		return false;
	}

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*FuncName))
		{
			return true;
		}
	}

	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*FuncName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass());
	if (!NewGraph)
	{
		OutError = FString::Printf(TEXT("add_function_graph '%s' failed: could not create graph."), *FuncName);
		return false;
	}

	FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, NewGraph, true, nullptr);

	UK2Node_FunctionEntry* EntryNode = nullptr;
	for (UEdGraphNode* Node : NewGraph->Nodes)
	{
		EntryNode = Cast<UK2Node_FunctionEntry>(Node);
		if (EntryNode)
		{
			break;
		}
	}

	bool bIsPure = false;
	if (Payload->TryGetBoolField(TEXT("is_pure"), bIsPure) && bIsPure && EntryNode)
	{
		EntryNode->SetExtraFlags(EntryNode->GetExtraFlags() | FUNC_BlueprintPure);
	}

	const TArray<TSharedPtr<FJsonValue>>* InputsArray = nullptr;
	if (Payload->TryGetArrayField(TEXT("inputs"), InputsArray) && InputsArray && EntryNode)
	{
		AppendUserPins(EntryNode, InputsArray, EGPD_Output, UEdGraphSchema_K2::PC_Boolean, false);
	}

	const TArray<TSharedPtr<FJsonValue>>* OutputsArray = nullptr;
	if (Payload->TryGetArrayField(TEXT("outputs"), OutputsArray) && OutputsArray && OutputsArray->Num() > 0)
	{
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

		AppendUserPins(ResultNode, OutputsArray, EGPD_Input, UEdGraphSchema_K2::PC_Boolean, false);
	}

	return true;
}

static bool AddMacroGraphDirect(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
	if (!Blueprint || !Payload.IsValid())
	{
		OutError = TEXT("add_macro_graph failed: invalid Blueprint or payload.");
		return false;
	}

	FString MacroName;
	if (!Payload->TryGetStringField(TEXT("name"), MacroName) || MacroName.IsEmpty())
	{
		OutError = TEXT("add_macro_graph failed: missing name field.");
		return false;
	}

	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*MacroName))
		{
			return true;
		}
	}

	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*MacroName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass());
	if (!NewGraph)
	{
		OutError = FString::Printf(TEXT("add_macro_graph '%s' failed: could not create graph."), *MacroName);
		return false;
	}

	FBlueprintEditorUtils::AddMacroGraph(Blueprint, NewGraph, true, nullptr);

	UK2Node_Tunnel* InputNode = nullptr;
	UK2Node_Tunnel* OutputNode = nullptr;
	for (UEdGraphNode* Node : NewGraph->Nodes)
	{
		UK2Node_Tunnel* TunnelNode = Cast<UK2Node_Tunnel>(Node);
		if (!TunnelNode)
		{
			continue;
		}

		if (TunnelNode->bCanHaveOutputs && !InputNode)
		{
			InputNode = TunnelNode;
		}
		else if (TunnelNode->bCanHaveInputs && !OutputNode)
		{
			OutputNode = TunnelNode;
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* InputsArray = nullptr;
	if (Payload->TryGetArrayField(TEXT("inputs"), InputsArray) && InputsArray && InputNode)
	{
		AppendUserPins(InputNode, InputsArray, EGPD_Output, UEdGraphSchema_K2::PC_Exec, false);
	}

	const TArray<TSharedPtr<FJsonValue>>* OutputsArray = nullptr;
	if (Payload->TryGetArrayField(TEXT("outputs"), OutputsArray) && OutputsArray && OutputNode)
	{
		AppendUserPins(OutputNode, OutputsArray, EGPD_Input, UEdGraphSchema_K2::PC_Exec, false);
	}

	return true;
}

static bool RemoveGraphDirect(UBlueprint* Blueprint, const FString& GraphName, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("remove_graph failed: invalid Blueprint.");
		return false;
	}

	if (GraphName.IsEmpty())
	{
		OutError = TEXT("remove_graph failed: missing name field.");
		return false;
	}

	if (GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		OutError = TEXT("remove_graph failed: deleting EventGraph is not allowed.");
		return false;
	}

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
			return true;
		}
	}

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
			return true;
		}
	}

	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
			return true;
		}
	}

	return true;
}

static bool AddEventDispatcherDirect(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
	if (!Blueprint || !Payload.IsValid())
	{
		OutError = TEXT("add_event_dispatcher failed: invalid Blueprint or payload.");
		return false;
	}

	FString DispatcherName;
	if (!Payload->TryGetStringField(TEXT("name"), DispatcherName) || DispatcherName.IsEmpty())
	{
		OutError = TEXT("add_event_dispatcher failed: missing name field.");
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
		OutError = FString::Printf(TEXT("add_event_dispatcher '%s' failed: a non-dispatcher variable with the same name exists."), *DispatcherName);
		return false;
	}

	bool bCreatedVariableThisCall = false;
	if (!bDispatcherExists)
	{
		FEdGraphPinType DelegatePinType;
		DelegatePinType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

		if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, DispatcherFName, DelegatePinType))
		{
			OutError = FString::Printf(TEXT("add_event_dispatcher '%s' failed: could not create dispatcher variable."), *DispatcherName);
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

			OutError = FString::Printf(TEXT("add_event_dispatcher '%s' failed: could not create delegate signature graph."), *DispatcherName);
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
	if (Payload->TryGetArrayField(TEXT("params"), ParamsArray) && ParamsArray)
	{
		for (UEdGraphNode* Node : DelegateSignatureGraph->Nodes)
		{
			UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node);
			if (!EntryNode)
			{
				continue;
			}

			AppendUserPins(EntryNode, ParamsArray, EGPD_Output, UEdGraphSchema_K2::PC_Boolean, true);
			break;
		}
	}

	return true;
}
}

FBlueprintHelperBlueprintStructureService::FBlueprintHelperBlueprintStructureService(
	const FBlueprintHelperGraphResolver& InResolver)
	: Resolver(InResolver)
{
}

// ─── 辅助 ───

UBlueprint* FBlueprintHelperBlueprintStructureService::ResolveBP(
	const FBlueprintHelperGraphTarget& Target, FString& OutError) const
{
	FBlueprintHelperDiagnosticSet Diag;
	UBlueprint* BP = Resolver.ResolveBlueprint(Target, Diag);
	if (!BP && Diag.Items.Num() > 0)
	{
		OutError = Diag.Items[0].Message;
	}
	return BP;
}

// ─── ListGraphs ───

FBlueprintHelperListGraphsResult FBlueprintHelperBlueprintStructureService::ListGraphs(
	const FBlueprintHelperGraphTarget& Target) const
{
	FBlueprintHelperListGraphsResult Result;

	FString Error;
	UBlueprint* BP = ResolveBP(Target, Error);
	if (!BP)
	{
		Result.ErrorMessage = Error;
		return Result;
	}

	auto AddGraphInfo = [&Result](UEdGraph* Graph, const FString& Type)
	{
		if (!Graph) return;
		FBlueprintHelperGraphInfo Info;
		Info.Name = Graph->GetName();
		Info.GraphType = Type;
		Info.NodeCount = Graph->Nodes.Num();

		// 检查是否 Pure 函数
		if (Type == TEXT("Function"))
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
				{
					Info.bIsPure = (Entry->GetExtraFlags() & FUNC_BlueprintPure) != 0;
					break;
				}
			}
		}

		Result.Graphs.Add(Info);
	};

	// UbergraphPages（EventGraph 等）
	for (UEdGraph* Graph : BP->UbergraphPages)
	{
		AddGraphInfo(Graph, TEXT("EventGraph"));
	}

	// FunctionGraphs
	for (UEdGraph* Graph : BP->FunctionGraphs)
	{
		AddGraphInfo(Graph, TEXT("Function"));
	}

	// MacroGraphs
	for (UEdGraph* Graph : BP->MacroGraphs)
	{
		AddGraphInfo(Graph, TEXT("Macro"));
	}

	Result.bSuccess = true;
	return Result;
}

// ─── ListVariables ───

FBlueprintHelperListVariablesResult FBlueprintHelperBlueprintStructureService::ListVariables(
	const FBlueprintHelperGraphTarget& Target) const
{
	FBlueprintHelperListVariablesResult Result;

	FString Error;
	UBlueprint* BP = ResolveBP(Target, Error);
	if (!BP)
	{
		Result.ErrorMessage = Error;
		return Result;
	}

	for (const FBPVariableDescription& Var : BP->NewVariables)
	{
		// 跳过事件分发器（MC Delegate 类型）
		if (Var.VarType.PinCategory == UEdGraphSchema_K2::PC_MCDelegate)
		{
			continue;
		}

		FBlueprintHelperVariableInfo Info;
		Info.Name = Var.VarName.ToString();
		Info.TypeCategory = Var.VarType.PinCategory.ToString();

		if (Var.VarType.PinSubCategoryObject.IsValid())
		{
			Info.SubCategoryObject = Var.VarType.PinSubCategoryObject->GetPathName();
		}

		if (Var.VarType.IsArray())
		{
			Info.ContainerType = TEXT("Array");
		}
		else if (Var.VarType.IsSet())
		{
			Info.ContainerType = TEXT("Set");
		}
		else if (Var.VarType.IsMap())
		{
			Info.ContainerType = TEXT("Map");
		}
		else
		{
			Info.ContainerType = TEXT("None");
		}

		Info.DefaultValue = Var.DefaultValue;
		Info.Category = Var.Category.ToString();
		Info.bIsEditable = !(Var.PropertyFlags & CPF_DisableEditOnInstance);

		Result.Variables.Add(Info);
	}

	Result.bSuccess = true;
	return Result;
}

// ─── ListEventDispatchers ───

FBlueprintHelperListDispatchersResult FBlueprintHelperBlueprintStructureService::ListEventDispatchers(
	const FBlueprintHelperGraphTarget& Target) const
{
	FBlueprintHelperListDispatchersResult Result;

	FString Error;
	UBlueprint* BP = ResolveBP(Target, Error);
	if (!BP)
	{
		Result.ErrorMessage = Error;
		return Result;
	}

	for (const FBPVariableDescription& Var : BP->NewVariables)
	{
		if (Var.VarType.PinCategory != UEdGraphSchema_K2::PC_MCDelegate)
		{
			continue;
		}

		FBlueprintHelperEventDispatcherInfo Info;
		Info.Name = Var.VarName.ToString();

		// 查找委托签名图获取参数
		UEdGraph* SigGraph = FBlueprintEditorUtils::GetDelegateSignatureGraphByName(BP, Var.VarName);
		if (SigGraph)
		{
			for (UEdGraphNode* Node : SigGraph->Nodes)
			{
				UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node);
				if (!Entry) continue;
				for (const TSharedPtr<FUserPinInfo>& Pin : Entry->UserDefinedPins)
				{
					if (Pin.IsValid())
					{
						Info.Params.Add(FString::Printf(TEXT("%s:%s"),
							*Pin->PinName.ToString(), *Pin->PinType.PinCategory.ToString()));
					}
				}
				break;
			}
		}

		Result.Dispatchers.Add(Info);
	}

	Result.bSuccess = true;
	return Result;
}

// ─── AddVariable ───

bool FBlueprintHelperBlueprintStructureService::AddVariable(
	const FBlueprintHelperGraphTarget& Target, const TSharedPtr<FJsonObject>& Params, FString& OutError) const
{
	UBlueprint* BP = ResolveBP(Target, OutError);
	if (!BP) return false;

	FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Add Variable")));
	const bool bOk = AddMemberVariableDirect(BP, Params, OutError);
	if (bOk)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}
	return bOk;
}

// ─── RemoveVariable ───

bool FBlueprintHelperBlueprintStructureService::RemoveVariable(
	const FBlueprintHelperGraphTarget& Target, const FString& VarName, FString& OutError) const
{
	UBlueprint* BP = ResolveBP(Target, OutError);
	if (!BP) return false;

	FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Remove Variable")));
	const bool bOk = RemoveMemberVariableDirect(BP, VarName, OutError);
	if (bOk)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}
	return bOk;
}

// ─── AddGraph ───

bool FBlueprintHelperBlueprintStructureService::AddGraph(
	const FBlueprintHelperGraphTarget& Target, const TSharedPtr<FJsonObject>& Params, FString& OutError) const
{
	UBlueprint* BP = ResolveBP(Target, OutError);
	if (!BP) return false;

	FString GraphType;
	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("graph_type"), GraphType);
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Add Graph")));
	const bool bOk = GraphType.Equals(TEXT("Macro"), ESearchCase::IgnoreCase)
		? AddMacroGraphDirect(BP, Params, OutError)
		: AddFunctionGraphDirect(BP, Params, OutError);
	if (bOk)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}
	return bOk;
}

// ─── RemoveGraph ───

bool FBlueprintHelperBlueprintStructureService::RemoveGraph(
	const FBlueprintHelperGraphTarget& Target, const FString& GraphName, FString& OutError) const
{
	UBlueprint* BP = ResolveBP(Target, OutError);
	if (!BP) return false;

	FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Remove Graph")));
	const bool bOk = RemoveGraphDirect(BP, GraphName, OutError);
	if (bOk)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}
	return bOk;
}

// ─── AddEventDispatcher ───

bool FBlueprintHelperBlueprintStructureService::AddEventDispatcher(
	const FBlueprintHelperGraphTarget& Target, const TSharedPtr<FJsonObject>& Params, FString& OutError) const
{
	UBlueprint* BP = ResolveBP(Target, OutError);
	if (!BP) return false;

	FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Add Event Dispatcher")));
	const bool bOk = AddEventDispatcherDirect(BP, Params, OutError);
	if (bOk)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}
	return bOk;
}

// ─── DeleteNodes ───

bool FBlueprintHelperBlueprintStructureService::DeleteNodes(
	const FBlueprintHelperGraphTarget& Target, const TArray<FString>& NodeIds,
	int32& OutDeletedCount, FString& OutError) const
{
	OutDeletedCount = 0;

	FBlueprintHelperDiagnosticSet Diag;
	UEdGraph* Graph = Resolver.ResolveGraph(Target, Diag);
	if (!Graph)
	{
		OutError = Diag.Items.Num() > 0 ? Diag.Items[0].Message : TEXT("未找到目标图表。");
		return false;
	}

	UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if (!BP)
	{
		OutError = TEXT("未找到图表所属蓝图。");
		return false;
	}

	TMap<FGuid, UEdGraphNode*> GuidToNode;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}
		GuidToNode.Add(Node->NodeGuid, Node);
	}

	TArray<UEdGraphNode*> NodesToRemove;
	TSet<FGuid> SeenGuids;
	for (const FString& NodeId : NodeIds)
	{
		if (NodeId.StartsWith(TEXT("Node_")))
		{
			OutError = FString::Printf(TEXT("delete_nodes 不再接受不稳定 ID '%s'。请使用 node_guid。"), *NodeId);
			return false;
		}

		FGuid NodeGuid;
		if (!FBlueprintHelperBlueprintStructureServiceLocalUtils::ParseStableNodeGuid(NodeId, NodeGuid))
		{
			OutError = FString::Printf(TEXT("delete_nodes 只接受稳定 node_guid，收到: %s"), *NodeId);
			return false;
		}

		if (SeenGuids.Contains(NodeGuid))
		{
			OutError = FString::Printf(TEXT("delete_nodes 包含重复 node_guid: %s"), *NodeId);
			return false;
		}
		SeenGuids.Add(NodeGuid);

		UEdGraphNode** Found = GuidToNode.Find(NodeGuid);
		if (!Found || !*Found)
		{
			OutError = FString::Printf(TEXT("未找到 node_guid: %s"), *NodeId);
			return false;
		}

		if (Cast<UK2Node_FunctionEntry>(*Found) || Cast<UK2Node_FunctionResult>(*Found))
		{
			OutError = FString::Printf(TEXT("受保护节点不能删除: %s"), *NodeId);
			return false;
		}

		NodesToRemove.Add(*Found);
	}

	if (NodesToRemove.Num() != NodeIds.Num())
	{
		OutError = TEXT("delete_nodes 目标数量与解析数量不一致。");
		return false;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Delete Nodes")), BP);
	Mutation.Modify(Graph);

	for (UEdGraphNode* Node : NodesToRemove)
	{
		FBlueprintEditorUtils::RemoveNode(BP, Node, true);
		++OutDeletedCount;
	}

	if (OutDeletedCount > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}

	Mutation.Commit();
	return true;
}
