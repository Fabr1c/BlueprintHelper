#include "TextToBlueprintGenerator.h"

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
#include "ScopedTransaction.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectIterator.h"

namespace
{
	/** 标准宏库资产路径。 */
	const TCHAR* StandardMacroLibraryPath = TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros");

	/**
	 * 归一化引脚名称，便于做别名比较。
	 */
	FString NormalizePinKey(const FString& InPinName)
	{
		FString Result = InPinName;
		Result.TrimStartAndEndInline();
		Result.ReplaceInline(TEXT(" "), TEXT(""));
		Result.ReplaceInline(TEXT("_"), TEXT(""));
		Result.ReplaceInline(TEXT("-"), TEXT(""));
		return Result.ToLower();
	}

	/**
	 * 返回标准 K2 实数子分类。
	 */
	FName ResolveRealSubCategory(const FString& Category)
	{
		if (Category.Equals(TEXT("double"), ESearchCase::IgnoreCase))
		{
			return UEdGraphSchema_K2::PC_Double;
		}

		return UEdGraphSchema_K2::PC_Float;
	}
}

EParsedBlueprintNodeType TextToBlueprintGenerator::ResolveNodeType(const TSharedPtr<FJsonObject>& NodeObject)
{
	if (!NodeObject.IsValid())
	{
		return EParsedBlueprintNodeType::Unknown;
	}

	FString NodeTypeString;
	NodeObject->TryGetStringField(TEXT("type"), NodeTypeString);

	if (NodeTypeString.Equals(TEXT("K2Node_CallFunction"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::CallFunction;
	}

	if (NodeTypeString.Equals(TEXT("K2Node_VariableGet"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::VariableGet;
	}

	if (NodeTypeString.Equals(TEXT("K2Node_VariableSet"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::VariableSet;
	}

	if (NodeTypeString.Equals(TEXT("K2Node_MacroInstance"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::MacroInstance;
	}

	if (NodeObject->HasField(TEXT("macro")))
	{
		return EParsedBlueprintNodeType::MacroInstance;
	}

	if (NodeObject->HasField(TEXT("variable")))
	{
		if (NodeObject->HasField(TEXT("set")))
		{
			return NodeObject->GetBoolField(TEXT("set")) ? EParsedBlueprintNodeType::VariableSet : EParsedBlueprintNodeType::VariableGet;
		}

		if (NodeTypeString.Contains(TEXT("set"), ESearchCase::IgnoreCase))
		{
			return EParsedBlueprintNodeType::VariableSet;
		}

		return EParsedBlueprintNodeType::VariableGet;
	}

	return EParsedBlueprintNodeType::CallFunction;
}

FString TextToBlueprintGenerator::ConvertJsonValueToString(const TSharedPtr<FJsonValue>& JsonValue)
{
	if (!JsonValue.IsValid())
	{
		return TEXT("");
	}

	switch (JsonValue->Type)
	{
	case EJson::String:
		return JsonValue->AsString();

	case EJson::Number:
		return LexToString(JsonValue->AsNumber());

	case EJson::Boolean:
		return JsonValue->AsBool() ? TEXT("true") : TEXT("false");

	case EJson::Null:
		return TEXT("");

	case EJson::Array:
	{
		FString CombinedValue;
		const TArray<TSharedPtr<FJsonValue>>& ArrayValues = JsonValue->AsArray();
		for (int32 ValueIndex = 0; ValueIndex < ArrayValues.Num(); ++ValueIndex)
		{
			if (ValueIndex > 0)
			{
				CombinedValue += TEXT(",");
			}
			CombinedValue += ConvertJsonValueToString(ArrayValues[ValueIndex]);
		}
		return CombinedValue;
	}

	default:
		return TEXT("");
	}
}

FString TextToBlueprintGenerator::ResolveNodeFunctionName(const TSharedPtr<FJsonObject>& NodeObject)
{
	if (!NodeObject.IsValid())
	{
		return TEXT("");
	}

	FString FunctionName;
	if (NodeObject->TryGetStringField(TEXT("function_name"), FunctionName) && !FunctionName.IsEmpty())
	{
		return FunctionName;
	}

	if (NodeObject->TryGetStringField(TEXT("name"), FunctionName) && !FunctionName.IsEmpty())
	{
		return FunctionName;
	}

	NodeObject->TryGetStringField(TEXT("display_name"), FunctionName);
	return FunctionName;
}

FParsedPinType TextToBlueprintGenerator::ResolvePinType(const TSharedPtr<FJsonObject>& PinTypeObject)
{
	FParsedPinType Result;
	if (!PinTypeObject.IsValid())
	{
		return Result;
	}

	PinTypeObject->TryGetStringField(TEXT("category"), Result.Category);
	PinTypeObject->TryGetStringField(TEXT("sub_category"), Result.SubCategory);
	PinTypeObject->TryGetStringField(TEXT("sub_category_object_path"), Result.SubCategoryObjectPath);
	PinTypeObject->TryGetStringField(TEXT("object_path"), Result.SubCategoryObjectPath);
	PinTypeObject->TryGetStringField(TEXT("container"), Result.ContainerType);
	PinTypeObject->TryGetBoolField(TEXT("is_reference"), Result.bIsReference);
	PinTypeObject->TryGetBoolField(TEXT("is_const"), Result.bIsConst);
	return Result;
}

FParsedVariableReference TextToBlueprintGenerator::ResolveVariableReference(const TSharedPtr<FJsonObject>& NodeObject)
{
	FParsedVariableReference Result;
	if (!NodeObject.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* VariableObject = nullptr;
	if (NodeObject->TryGetObjectField(TEXT("variable"), VariableObject) && VariableObject && VariableObject->IsValid())
	{
		(*VariableObject)->TryGetStringField(TEXT("scope"), Result.ScopeType);
		(*VariableObject)->TryGetStringField(TEXT("name"), Result.VariableName);
		(*VariableObject)->TryGetStringField(TEXT("owner_class_path"), Result.OwnerClassPath);
		(*VariableObject)->TryGetStringField(TEXT("scope_graph_name"), Result.ScopeGraphName);
		(*VariableObject)->TryGetStringField(TEXT("default_value"), Result.DefaultValue);
		(*VariableObject)->TryGetBoolField(TEXT("self_context"), Result.bSelfContext);
		(*VariableObject)->TryGetBoolField(TEXT("ensure_exists"), Result.bEnsureExists);

		const TSharedPtr<FJsonObject>* PinTypeObject = nullptr;
		if ((*VariableObject)->TryGetObjectField(TEXT("pin_type"), PinTypeObject) && PinTypeObject)
		{
			Result.PinType = ResolvePinType(*PinTypeObject);
		}
	}

	if (Result.VariableName.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("name"), Result.VariableName);
	}

	if (Result.ScopeType.IsEmpty())
	{
		Result.ScopeType = TEXT("member");
	}

	return Result;
}

FParsedMacroReference TextToBlueprintGenerator::ResolveMacroReference(const TSharedPtr<FJsonObject>& NodeObject)
{
	FParsedMacroReference Result;
	if (!NodeObject.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* MacroObject = nullptr;
	if (NodeObject->TryGetObjectField(TEXT("macro"), MacroObject) && MacroObject && MacroObject->IsValid())
	{
		(*MacroObject)->TryGetStringField(TEXT("library"), Result.LibraryType);
		(*MacroObject)->TryGetStringField(TEXT("name"), Result.MacroName);
		(*MacroObject)->TryGetStringField(TEXT("asset_path"), Result.MacroAssetPath);
	}

	if (Result.MacroName.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("name"), Result.MacroName);
	}

	if (Result.LibraryType.IsEmpty())
	{
		Result.LibraryType = TEXT("standard");
	}

	return Result;
}

void TextToBlueprintGenerator::ResolveLocalVariableDeclarations(const TSharedPtr<FJsonObject>& JsonObject, TArray<FParsedLocalVariableDeclaration>& OutDeclarations)
{
	OutDeclarations.Empty();
	if (!JsonObject.IsValid())
	{
		return;
	}

	const TSharedPtr<FJsonObject>* DeclarationsObject = nullptr;
	if (!JsonObject->TryGetObjectField(TEXT("declarations"), DeclarationsObject) || !DeclarationsObject || !DeclarationsObject->IsValid())
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* LocalVariableArray = nullptr;
	if (!(*DeclarationsObject)->TryGetArrayField(TEXT("local_variables"), LocalVariableArray) || !LocalVariableArray)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& DeclarationValue : *LocalVariableArray)
	{
		const TSharedPtr<FJsonObject> DeclarationObject = DeclarationValue->AsObject();
		if (!DeclarationObject.IsValid())
		{
			continue;
		}

		FParsedLocalVariableDeclaration Declaration;
		DeclarationObject->TryGetStringField(TEXT("name"), Declaration.Name);
		DeclarationObject->TryGetStringField(TEXT("default_value"), Declaration.DefaultValue);
		DeclarationObject->TryGetBoolField(TEXT("ensure_exists"), Declaration.bEnsureExists);

		const TSharedPtr<FJsonObject>* PinTypeObject = nullptr;
		if (DeclarationObject->TryGetObjectField(TEXT("pin_type"), PinTypeObject) && PinTypeObject)
		{
			Declaration.PinType = ResolvePinType(*PinTypeObject);
		}

		if (!Declaration.Name.IsEmpty())
		{
			OutDeclarations.Add(Declaration);
		}
	}
}

bool TextToBlueprintGenerator::ConvertToEdGraphPinType(const FParsedPinType& InPinType, FEdGraphPinType& OutPinType, FString& OutErrorMessage)
{
	if (!InPinType.IsValid())
	{
		OutErrorMessage = TEXT("引脚类型无效：缺少 category。");
		return false;
	}

	OutPinType = FEdGraphPinType();
	const FString Category = InPinType.Category.ToLower();

	if (Category == TEXT("bool") || Category == TEXT("boolean"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (Category == TEXT("byte"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
	}
	else if (Category == TEXT("int") || Category == TEXT("int32"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (Category == TEXT("int64"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
	}
	else if (Category == TEXT("float") || Category == TEXT("double") || Category == TEXT("real"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = ResolveRealSubCategory(Category);
	}
	else if (Category == TEXT("name"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
	}
	else if (Category == TEXT("string"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (Category == TEXT("text"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
	}
	else if (Category == TEXT("object"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
	}
	else if (Category == TEXT("class"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Class;
	}
	else if (Category == TEXT("struct"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	}
	else if (Category == TEXT("enum"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
	}
	else
	{
		OutErrorMessage = FString::Printf(TEXT("暂不支持的引脚类型：%s"), *InPinType.Category);
		return false;
	}

	if (!InPinType.SubCategory.IsEmpty() && Category != TEXT("float") && Category != TEXT("double") && Category != TEXT("real"))
	{
		OutPinType.PinSubCategory = FName(*InPinType.SubCategory);
	}

	if (!InPinType.SubCategoryObjectPath.IsEmpty())
	{
		if (UObject* SubCategoryObject = LoadObject<UObject>(nullptr, *InPinType.SubCategoryObjectPath))
		{
			OutPinType.PinSubCategoryObject = SubCategoryObject;
		}
		else
		{
			OutErrorMessage = FString::Printf(TEXT("无法加载引脚子分类对象：%s"), *InPinType.SubCategoryObjectPath);
			return false;
		}
	}

	const FString ContainerType = InPinType.ContainerType.ToLower();
	if (ContainerType == TEXT("array"))
	{
		OutPinType.ContainerType = EPinContainerType::Array;
	}
	else if (ContainerType == TEXT("set"))
	{
		OutPinType.ContainerType = EPinContainerType::Set;
	}
	else if (ContainerType == TEXT("map"))
	{
		OutPinType.ContainerType = EPinContainerType::Map;
	}

	OutPinType.bIsReference = InPinType.bIsReference;
	OutPinType.bIsConst = InPinType.bIsConst;
	return true;
}

bool TextToBlueprintGenerator::EnsureLocalVariableExists(UEdGraph* TargetGraph, const FParsedLocalVariableDeclaration& Declaration, FString& OutErrorMessage)
{
	OutErrorMessage.Reset();
	if (!TargetGraph)
	{
		OutErrorMessage = TEXT("本地变量创建失败：目标图表无效。");
		return false;
	}

	if (Declaration.Name.IsEmpty())
	{
		OutErrorMessage = TEXT("本地变量创建失败：变量名为空。");
		return false;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	UEdGraph* ScopeGraph = FBlueprintEditorUtils::GetTopLevelGraph(TargetGraph);
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!Blueprint || !ScopeGraph || !Schema || Schema->GetGraphType(ScopeGraph) != GT_Function)
	{
		OutErrorMessage = TEXT("本地变量仅支持在函数图中创建。请先聚焦一个函数图。");
		return false;
	}

	if (FBlueprintEditorUtils::FindLocalVariable(Blueprint, ScopeGraph, *Declaration.Name))
	{
		return true;
	}

	FEdGraphPinType VariablePinType;
	if (!ConvertToEdGraphPinType(Declaration.PinType, VariablePinType, OutErrorMessage))
	{
		return false;
	}

	if (!FBlueprintEditorUtils::AddLocalVariable(Blueprint, ScopeGraph, *Declaration.Name, VariablePinType, Declaration.DefaultValue))
	{
		OutErrorMessage = FString::Printf(TEXT("无法创建本地变量：%s"), *Declaration.Name);
		return false;
	}

	return true;
}

UStruct* TextToBlueprintGenerator::ResolveLocalVariableScope(UEdGraph* TargetGraph, FString& OutErrorMessage)
{
	OutErrorMessage.Reset();
	if (!TargetGraph)
	{
		OutErrorMessage = TEXT("无法解析本地变量作用域：目标图表无效。");
		return nullptr;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	UEdGraph* ScopeGraph = FBlueprintEditorUtils::GetTopLevelGraph(TargetGraph);
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!Blueprint || !ScopeGraph || !Schema || Schema->GetGraphType(ScopeGraph) != GT_Function)
	{
		OutErrorMessage = TEXT("当前图表不是函数图，无法解析本地变量作用域。");
		return nullptr;
	}

	if (!Blueprint->SkeletonGeneratedClass)
	{
		OutErrorMessage = TEXT("蓝图骨架类无效，无法解析本地变量作用域。");
		return nullptr;
	}

	if (UFunction* ScopeFunction = Blueprint->SkeletonGeneratedClass->FindFunctionByName(ScopeGraph->GetFName()))
	{
		return ScopeFunction;
	}

	OutErrorMessage = FString::Printf(TEXT("未在蓝图骨架类中找到作用域函数：%s"), *ScopeGraph->GetName());
	return nullptr;
}

UStruct* TextToBlueprintGenerator::ResolveVariableSource(UEdGraph* TargetGraph, const FParsedVariableReference& VariableReference, FString& OutErrorMessage)
{
	OutErrorMessage.Reset();
	if (VariableReference.IsLocalVariable())
	{
		return ResolveLocalVariableScope(TargetGraph, OutErrorMessage);
	}

	if (VariableReference.bSelfContext || VariableReference.OwnerClassPath.IsEmpty() || VariableReference.OwnerClassPath.Equals(TEXT("self"), ESearchCase::IgnoreCase))
	{
		return nullptr;
	}

	if (UClass* OwnerClass = LoadObject<UClass>(nullptr, *VariableReference.OwnerClassPath))
	{
		return OwnerClass;
	}

	OutErrorMessage = FString::Printf(TEXT("无法加载成员变量所属类：%s"), *VariableReference.OwnerClassPath);
	return nullptr;
}

UEdGraph* TextToBlueprintGenerator::ResolveMacroGraph(const FParsedMacroReference& MacroReference, FString& OutErrorMessage)
{
	OutErrorMessage.Reset();
	FString BlueprintPath = MacroReference.MacroAssetPath;
	if (BlueprintPath.IsEmpty() || MacroReference.LibraryType.Equals(TEXT("standard"), ESearchCase::IgnoreCase))
	{
		BlueprintPath = StandardMacroLibraryPath;
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

UEdGraphPin* TextToBlueprintGenerator::FindPinByAlias(UK2Node* TargetNode, const FString& RequestedPinName)
{
	if (!TargetNode || RequestedPinName.IsEmpty())
	{
		return nullptr;
	}

	if (UEdGraphPin* ExactPin = TargetNode->FindPin(RequestedPinName))
	{
		return ExactPin;
	}

	const FString NormalizedKey = NormalizePinKey(RequestedPinName);
	for (UEdGraphPin* Pin : TargetNode->Pins)
	{
		if (!Pin)
		{
			continue;
		}

		if (NormalizePinKey(Pin->PinName.ToString()) == NormalizedKey)
		{
			return Pin;
		}
	}

	if (NormalizedKey == NormalizePinKey(TEXT("execute")))
	{
		return TargetNode->FindPin(UEdGraphSchema_K2::PN_Execute);
	}

	if (NormalizedKey == NormalizePinKey(TEXT("then")))
	{
		return TargetNode->FindPin(UEdGraphSchema_K2::PN_Then);
	}

	if (NormalizedKey == NormalizePinKey(TEXT("completed")))
	{
		if (UEdGraphPin* CompletedPin = TargetNode->FindPin(UEdGraphSchema_K2::PN_Completed))
		{
			return CompletedPin;
		}
	}

	if (NormalizedKey == NormalizePinKey(TEXT("loopbody")))
	{
		if (UEdGraphPin* LoopBodyPin = TargetNode->FindPin(TEXT("LoopBody")))
		{
			return LoopBodyPin;
		}
	}

	if (NormalizedKey == NormalizePinKey(TEXT("firstindex")))
	{
		if (UEdGraphPin* FirstIndexPin = TargetNode->FindPin(TEXT("FirstIndex")))
		{
			return FirstIndexPin;
		}
	}

	if (NormalizedKey == NormalizePinKey(TEXT("lastindex")))
	{
		if (UEdGraphPin* LastIndexPin = TargetNode->FindPin(TEXT("LastIndex")))
		{
			return LastIndexPin;
		}
	}

	if (NormalizedKey == NormalizePinKey(TEXT("index")))
	{
		if (UEdGraphPin* IndexPin = TargetNode->FindPin(TEXT("Index")))
		{
			return IndexPin;
		}
	}

	if (NormalizedKey == NormalizePinKey(TEXT("value")))
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

	return nullptr;
}

bool TextToBlueprintGenerator::ApplyPinDefaultValue(UEdGraphPin* TargetPin, const FString& InValue)
{
	if (!TargetPin)
	{
		return false;
	}

	if (TargetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object || TargetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
	{
		if (!InValue.IsEmpty())
		{
			if (UObject* DefaultObject = LoadObject<UObject>(nullptr, *InValue))
			{
				TargetPin->DefaultObject = DefaultObject;
				return true;
			}
		}

		TargetPin->DefaultValue = InValue;
		return true;
	}

	if (TargetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
	{
		TargetPin->DefaultTextValue = FText::FromString(InValue);
		return true;
	}

	TargetPin->DefaultValue = InValue;
	return true;
}

UFunction* TextToBlueprintGenerator::FindFunctionByName(const FString& FuncName)
{
	if (FuncName.IsEmpty())
	{
		return nullptr;
	}

	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (!Class || Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		if (UFunction* DirectFunction = Class->FindFunctionByName(*FuncName))
		{
			return DirectFunction;
		}

		for (TFieldIterator<UFunction> FuncIt(Class, EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt)
		{
			UFunction* Function = *FuncIt;
			if (!Function || !Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure))
			{
				continue;
			}

			const FString NativeName = Function->GetName();
			const FString DisplayName = Function->HasMetaData(TEXT("DisplayName")) ? Function->GetMetaData(TEXT("DisplayName")) : NativeName;
			if (NativeName.Equals(FuncName, ESearchCase::IgnoreCase) || DisplayName.Equals(FuncName, ESearchCase::IgnoreCase))
			{
				return Function;
			}
		}
	}
	return nullptr;
}

FBlueprintGenerateResult TextToBlueprintGenerator::GenerateBlueprintFromJson(UEdGraph* TargetGraph, const FString& JsonString,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes)
{
	FBlueprintGenerateResult Result;
	Result.Message = TEXT("生成失败。") ;

	if (!TargetGraph)
	{
		Result.Message = TEXT("目标蓝图图表无效。");
		return Result;
	}

	OutUnresolvedNodes.Empty();
	const FString TrimmedJsonString = JsonString.TrimStartAndEnd();
	if (TrimmedJsonString.IsEmpty())
	{
		Result.Message = TEXT("JSON 文本为空，请先执行蓝图转 JSON 或粘贴符合规则的 JSON。");
		return Result;
	}

	if (!TrimmedJsonString.StartsWith(TEXT("{")))
	{
		Result.Message = TEXT("主文本区不是有效 JSON，请先点击“从蓝图文本/剪贴板转换为JSON”。");
		return Result;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(TrimmedJsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		Result.Message = FString::Printf(TEXT("JSON 解析失败：%s"), *Reader->GetErrorMessage());
		return Result;
	}

	TArray<FParsedNode> ParsedNodes;
	TArray<FParsedLink> ParsedLinks;
	TArray<FParsedLocalVariableDeclaration> ParsedLocalVariableDeclarations;
	ResolveLocalVariableDeclarations(JsonObject, ParsedLocalVariableDeclarations);

	const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("nodes"), NodesArray) && NodesArray)
	{
		for (const TSharedPtr<FJsonValue>& NodeValue : *NodesArray)
		{
			const TSharedPtr<FJsonObject> NodeObject = NodeValue->AsObject();
			if (!NodeObject.IsValid())
			{
				continue;
			}

			FParsedNode ParsedNode;
			ParsedNode.Id = NodeObject->GetStringField(TEXT("id"));
			NodeObject->TryGetStringField(TEXT("type"), ParsedNode.SourceType);
			ParsedNode.NodeType = ResolveNodeType(NodeObject);
			ParsedNode.FunctionName = ResolveNodeFunctionName(NodeObject);
			ParsedNode.X = NodeObject->HasField(TEXT("x")) ? static_cast<float>(NodeObject->GetNumberField(TEXT("x"))) : 0.0f;
			ParsedNode.Y = NodeObject->HasField(TEXT("y")) ? static_cast<float>(NodeObject->GetNumberField(TEXT("y"))) : 0.0f;
			ParsedNode.VariableReference = ResolveVariableReference(NodeObject);
			ParsedNode.MacroReference = ResolveMacroReference(NodeObject);

			const TSharedPtr<FJsonObject>* PositionObject = nullptr;
			if (NodeObject->TryGetObjectField(TEXT("position"), PositionObject) && PositionObject && PositionObject->IsValid())
			{
				ParsedNode.X = (*PositionObject)->HasField(TEXT("x")) ? static_cast<float>((*PositionObject)->GetNumberField(TEXT("x"))) : ParsedNode.X;
				ParsedNode.Y = (*PositionObject)->HasField(TEXT("y")) ? static_cast<float>((*PositionObject)->GetNumberField(TEXT("y"))) : ParsedNode.Y;
			}

			const TSharedPtr<FJsonObject>* InputsObject = nullptr;
			if (NodeObject->TryGetObjectField(TEXT("inputs"), InputsObject) && InputsObject && InputsObject->IsValid())
			{
				for (const auto& Pair : (*InputsObject)->Values)
				{
					ParsedNode.DefaultValues.Add(Pair.Key, TextToBlueprintGenerator::ConvertJsonValueToString(Pair.Value));
				}
			}

			const TSharedPtr<FJsonObject>* DefaultValuesObject = nullptr;
			if (NodeObject->TryGetObjectField(TEXT("default_values"), DefaultValuesObject) && DefaultValuesObject && DefaultValuesObject->IsValid())
			{
				for (const auto& Pair : (*DefaultValuesObject)->Values)
				{
					ParsedNode.DefaultValues.FindOrAdd(Pair.Key) = TextToBlueprintGenerator::ConvertJsonValueToString(Pair.Value);
				}
			}

			ParsedNodes.Add(ParsedNode);
		}
	}
	else
	{
		Result.Message = TEXT("JSON 中缺少 nodes 数组。");
		return Result;
	}

	const TArray<TSharedPtr<FJsonValue>>* LinksArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("links"), LinksArray) && LinksArray)
	{
		for (const TSharedPtr<FJsonValue>& LinkValue : *LinksArray)
		{
			const TSharedPtr<FJsonObject> LinkObject = LinkValue->AsObject();
			if (!LinkObject.IsValid())
			{
				continue;
			}

			FParsedLink ParsedLink;
			ParsedLink.FromId = LinkObject->GetStringField(TEXT("from_id"));
			ParsedLink.FromPin = LinkObject->GetStringField(TEXT("from_pin"));
			ParsedLink.ToId = LinkObject->GetStringField(TEXT("to_id"));
			ParsedLink.ToPin = LinkObject->GetStringField(TEXT("to_pin"));
			ParsedLinks.Add(ParsedLink);
		}
	}

	const FScopedTransaction Transaction(FText::FromString(TEXT("Generate Blueprint from JSON")));
	TargetGraph->Modify();

	for (const FParsedLocalVariableDeclaration& Declaration : ParsedLocalVariableDeclarations)
	{
		if (!Declaration.bEnsureExists)
		{
			continue;
		}

		FString EnsureErrorMessage;
		if (!EnsureLocalVariableExists(TargetGraph, Declaration, EnsureErrorMessage))
		{
			TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
			UnresolvedItem->DisplayText = FString::Printf(TEXT("LocalVariable %s"), *Declaration.Name);
			UnresolvedItem->Reason = EnsureErrorMessage;
			OutUnresolvedNodes.Add(UnresolvedItem);
		}
	}

	TMap<FString, UK2Node*> IdToSpawnedNode;
	int32 GeneratedNodeCount = 0;
	for (const FParsedNode& ParsedNode : ParsedNodes)
	{
		UK2Node* SpawnedNode = nullptr;
		FString SpawnErrorMessage;

		if (ParsedNode.NodeType == EParsedBlueprintNodeType::CallFunction)
		{
			UFunction* TargetFunction = FindFunctionByName(ParsedNode.FunctionName);
			if (!TargetFunction)
			{
				SpawnErrorMessage = FString::Printf(TEXT("未找到蓝图函数：%s"), *ParsedNode.FunctionName);
			}
			else
			{
				SpawnedNode = SpawnFunctionNode(TargetGraph, TargetFunction, ParsedNode);
			}
		}
		else if (ParsedNode.NodeType == EParsedBlueprintNodeType::VariableGet)
		{
			if (ParsedNode.VariableReference.IsLocalVariable() && ParsedNode.VariableReference.bEnsureExists)
			{
				FParsedLocalVariableDeclaration LocalDeclaration;
				LocalDeclaration.Name = ParsedNode.VariableReference.VariableName;
				LocalDeclaration.PinType = ParsedNode.VariableReference.PinType;
				LocalDeclaration.DefaultValue = ParsedNode.VariableReference.DefaultValue;
				LocalDeclaration.bEnsureExists = true;
				EnsureLocalVariableExists(TargetGraph, LocalDeclaration, SpawnErrorMessage);
			}

			if (SpawnErrorMessage.IsEmpty())
			{
				SpawnedNode = SpawnVariableGetNode(TargetGraph, ParsedNode, SpawnErrorMessage);
			}
		}
		else if (ParsedNode.NodeType == EParsedBlueprintNodeType::VariableSet)
		{
			if (ParsedNode.VariableReference.IsLocalVariable() && ParsedNode.VariableReference.bEnsureExists)
			{
				FParsedLocalVariableDeclaration LocalDeclaration;
				LocalDeclaration.Name = ParsedNode.VariableReference.VariableName;
				LocalDeclaration.PinType = ParsedNode.VariableReference.PinType;
				LocalDeclaration.DefaultValue = ParsedNode.VariableReference.DefaultValue;
				LocalDeclaration.bEnsureExists = true;
				EnsureLocalVariableExists(TargetGraph, LocalDeclaration, SpawnErrorMessage);
			}

			if (SpawnErrorMessage.IsEmpty())
			{
				SpawnedNode = SpawnVariableSetNode(TargetGraph, ParsedNode, SpawnErrorMessage);
			}
		}
		else if (ParsedNode.NodeType == EParsedBlueprintNodeType::MacroInstance)
		{
			SpawnedNode = SpawnMacroNode(TargetGraph, ParsedNode, SpawnErrorMessage);
		}

		if (SpawnedNode)
		{
			IdToSpawnedNode.Add(ParsedNode.Id, SpawnedNode);
			++GeneratedNodeCount;
			continue;
		}

		TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
		UnresolvedItem->NodeData = ParsedNode;
		UnresolvedItem->DisplayText = ParsedNode.FunctionName.IsEmpty()
			? FString::Printf(TEXT("%s (%s)"), *ParsedNode.SourceType, *ParsedNode.Id)
			: FString::Printf(TEXT("%s (%s)"), *ParsedNode.FunctionName, *ParsedNode.Id);
		UnresolvedItem->Reason = SpawnErrorMessage.IsEmpty() ? TEXT("不支持的节点类型或配置不完整。") : SpawnErrorMessage;
		OutUnresolvedNodes.Add(UnresolvedItem);
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	for (const FParsedLink& ParsedLink : ParsedLinks)
	{
		if (!Schema || !IdToSpawnedNode.Contains(ParsedLink.FromId) || !IdToSpawnedNode.Contains(ParsedLink.ToId))
		{
			continue;
		}

		UK2Node* FromNode = IdToSpawnedNode[ParsedLink.FromId];
		UK2Node* ToNode = IdToSpawnedNode[ParsedLink.ToId];
		UEdGraphPin* FromPin = FindPinByAlias(FromNode, ParsedLink.FromPin);
		UEdGraphPin* ToPin = FindPinByAlias(ToNode, ParsedLink.ToPin);
		if (FromPin && ToPin)
		{
			Schema->TryCreateConnection(FromPin, ToPin);
		}
	}

	for (const auto& Pair : IdToSpawnedNode)
	{
		if (Pair.Value)
		{
			TargetGraph->GetSchema()->ReconstructNode(*Pair.Value);
		}
	}
	TargetGraph->NotifyGraphChanged();

	Result.bSucceed = GeneratedNodeCount > 0 || OutUnresolvedNodes.Num() > 0;
	Result.GeneratedNodeCount = GeneratedNodeCount;
	Result.UnresolvedNodeCount = OutUnresolvedNodes.Num();
	Result.Message = Result.bSucceed
		? FString::Printf(TEXT("生成完成：成功 %d 个，未匹配 %d 个。"), Result.GeneratedNodeCount, Result.UnresolvedNodeCount)
		: TEXT("未生成任何节点，请检查 JSON 内容是否符合规则。");
	return Result;
}

TArray<TSharedPtr<FEngineFunctionItem>> TextToBlueprintGenerator::GetAllBlueprintFunctions()
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

UK2Node* TextToBlueprintGenerator::SpawnVariableGetNode(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutErrorMessage)
{
	OutErrorMessage.Reset();
	if (!TargetGraph)
	{
		OutErrorMessage = TEXT("变量读取节点生成失败：目标图表无效。");
		return nullptr;
	}

	if (NodeData.VariableReference.VariableName.IsEmpty())
	{
		OutErrorMessage = TEXT("变量读取节点生成失败：变量名为空。");
		return nullptr;
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!Schema)
	{
		OutErrorMessage = TEXT("变量读取节点生成失败：K2 Schema 无效。");
		return nullptr;
	}

	UStruct* VariableSource = ResolveVariableSource(TargetGraph, NodeData.VariableReference, OutErrorMessage);
	if (!OutErrorMessage.IsEmpty())
	{
		return nullptr;
	}

	if (UK2Node_VariableGet* VariableNode = Schema->SpawnVariableGetNode(FVector2D(NodeData.X, NodeData.Y), TargetGraph, *NodeData.VariableReference.VariableName, VariableSource))
	{
		ApplyDefaultValues(VariableNode, NodeData.DefaultValues);
		return VariableNode;
	}

	OutErrorMessage = FString::Printf(TEXT("无法生成变量读取节点：%s"), *NodeData.VariableReference.VariableName);
	return nullptr;
}

UK2Node* TextToBlueprintGenerator::SpawnVariableSetNode(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutErrorMessage)
{
	OutErrorMessage.Reset();
	if (!TargetGraph)
	{
		OutErrorMessage = TEXT("变量写入节点生成失败：目标图表无效。");
		return nullptr;
	}

	if (NodeData.VariableReference.VariableName.IsEmpty())
	{
		OutErrorMessage = TEXT("变量写入节点生成失败：变量名为空。");
		return nullptr;
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!Schema)
	{
		OutErrorMessage = TEXT("变量写入节点生成失败：K2 Schema 无效。");
		return nullptr;
	}

	UStruct* VariableSource = ResolveVariableSource(TargetGraph, NodeData.VariableReference, OutErrorMessage);
	if (!OutErrorMessage.IsEmpty())
	{
		return nullptr;
	}

	if (UK2Node_VariableSet* VariableNode = Schema->SpawnVariableSetNode(FVector2D(NodeData.X, NodeData.Y), TargetGraph, *NodeData.VariableReference.VariableName, VariableSource))
	{
		ApplyDefaultValues(VariableNode, NodeData.DefaultValues);
		return VariableNode;
	}

	OutErrorMessage = FString::Printf(TEXT("无法生成变量写入节点：%s"), *NodeData.VariableReference.VariableName);
	return nullptr;
}

UK2Node* TextToBlueprintGenerator::SpawnMacroNode(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutErrorMessage)
{
	OutErrorMessage.Reset();
	if (!TargetGraph)
	{
		OutErrorMessage = TEXT("宏节点生成失败：目标图表无效。");
		return nullptr;
	}

	if (NodeData.MacroReference.MacroName.IsEmpty())
	{
		OutErrorMessage = TEXT("宏节点生成失败：宏名称为空。");
		return nullptr;
	}

	UEdGraph* MacroGraph = ResolveMacroGraph(NodeData.MacroReference, OutErrorMessage);
	if (!MacroGraph)
	{
		return nullptr;
	}

	UK2Node_MacroInstance* MacroNode = NewObject<UK2Node_MacroInstance>(TargetGraph);
	TargetGraph->AddNode(MacroNode, true, false);
	MacroNode->CreateNewGuid();
	MacroNode->SetMacroGraph(MacroGraph);
	MacroNode->PostPlacedNewNode();
	MacroNode->NodePosX = static_cast<int32>(NodeData.X);
	MacroNode->NodePosY = static_cast<int32>(NodeData.Y);
	MacroNode->AllocateDefaultPins();
	ApplyDefaultValues(MacroNode, NodeData.DefaultValues);
	TargetGraph->GetSchema()->ReconstructNode(*MacroNode);
	return MacroNode;
}

UK2Node_CallFunction* TextToBlueprintGenerator::SpawnFunctionNode(UEdGraph* TargetGraph, UFunction* TargetFunction, const FParsedNode& NodeData)
{
	if (!TargetGraph || !TargetFunction)
	{
		return nullptr;
	}

	UK2Node_CallFunction* CallFunctionNode = NewObject<UK2Node_CallFunction>(TargetGraph);
	TargetGraph->AddNode(CallFunctionNode, true, false);
	CallFunctionNode->CreateNewGuid();
	CallFunctionNode->PostPlacedNewNode();
	CallFunctionNode->SetFromFunction(TargetFunction);
	CallFunctionNode->NodePosX = static_cast<int32>(NodeData.X);
	CallFunctionNode->NodePosY = static_cast<int32>(NodeData.Y);
	CallFunctionNode->AllocateDefaultPins();
	ApplyDefaultValues(CallFunctionNode, NodeData.DefaultValues);
	TargetGraph->GetSchema()->ReconstructNode(*CallFunctionNode);
	return CallFunctionNode;
}

void TextToBlueprintGenerator::ApplyDefaultValues(UK2Node* TargetNode, const TMap<FString, FString>& DefaultValues)
{
	if (!TargetNode)
	{
		return;
	}

	for (const auto& Pair : DefaultValues)
	{
		if (UEdGraphPin* Pin = FindPinByAlias(TargetNode, Pair.Key))
		{
			ApplyPinDefaultValue(Pin, Pair.Value);
		}
	}
}
