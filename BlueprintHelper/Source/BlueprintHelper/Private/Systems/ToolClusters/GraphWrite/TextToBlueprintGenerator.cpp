#include "Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.h"

#include "Systems/ToolClusters/GraphWrite/NodeHandlers/BlueprintNodeHandler.h"
#include "Systems/ToolClusters/GraphWrite/OperationHandlers/BlueprintOperationHandler.h"
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

class FTextToBlueprintGeneratorLocalUtils
{
public:
	/** 标准宏库资产路径。 */
	inline static const TCHAR* StandardMacroLibraryPath = TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros");

	/**
	 * 归一化节点类型名称，统一转成 K2Node_xxx 形式。
	 */
	static FString NormalizeNodeTypeName(const FString& InNodeType)
	{
		FString Result = InNodeType;
		Result.TrimStartAndEndInline();
		Result.ReplaceInline(TEXT("\""), TEXT(""));

		const int32 LastSlashIndex = Result.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (LastSlashIndex != INDEX_NONE)
		{
			Result = Result.Mid(LastSlashIndex + 1);
		}

		const int32 LastDotIndex = Result.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (LastDotIndex != INDEX_NONE)
		{
			Result = Result.Mid(LastDotIndex + 1);
		}

		return Result.TrimStartAndEnd();
	}

	/**
	 * 判断宏名称是否仍是占位符。
	 */
	static bool IsPlaceholderMacroName(const FString& InMacroName)
	{
		return InMacroName.Equals(TEXT("BlueprintGraph.MacroInstance"), ESearchCase::IgnoreCase)
			|| InMacroName.Equals(TEXT("K2Node_MacroInstance"), ESearchCase::IgnoreCase)
			|| InMacroName.Equals(TEXT("MacroInstance"), ESearchCase::IgnoreCase);
	}

	/**
	 * 归一化引脚名称，便于做别名比较。
	 */
	static FString NormalizePinKey(const FString& InPinName)
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
	static FName ResolveRealSubCategory(const FString& Category)
	{
		if (Category.Equals(TEXT("double"), ESearchCase::IgnoreCase))
		{
			return UEdGraphSchema_K2::PC_Double;
		}

		return UEdGraphSchema_K2::PC_Float;
	}

	static FBlueprintGeneratorDiagnostic MakeGeneratorDiagnostic(
		const FString& Code,
		const FString& NodeId,
		const FString& PinName,
		const FString& Message,
		const FString& Severity = TEXT("error"))
	{
		FBlueprintGeneratorDiagnostic Diagnostic;
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.NodeId = NodeId;
		Diagnostic.PinName = PinName;
		Diagnostic.Message = Message;
		return Diagnostic;
	}

	static bool IsInvalidPinTypeFailure(const FString& ErrorMessage)
	{
		return ErrorMessage.Contains(TEXT("类型转换失败"))
			|| ErrorMessage.Contains(TEXT("引脚类型无效"))
			|| ErrorMessage.Contains(TEXT("暂不支持的引脚类型"))
			|| ErrorMessage.Contains(TEXT("无法加载引脚子分类对象"));
	}

	static FString FindDiagnosticPinName(const FParsedNode& NodeData, const FString& ErrorMessage)
	{
		for (const FParsedEventParam& Param : NodeData.EventReference.Params)
		{
			if (!Param.Name.IsEmpty() && ErrorMessage.Contains(Param.Name))
			{
				return Param.Name;
			}
		}

		return TEXT("");
	}

	static int32 CountRequestedPinTypes(const FParsedNode& NodeData)
	{
		int32 Count = 0;
		if (NodeData.VariableReference.PinType.IsValid())
		{
			++Count;
		}
		if (NodeData.ContainerReference.ElementType.IsValid())
		{
			++Count;
		}
		if (NodeData.ContainerReference.KeyType.IsValid())
		{
			++Count;
		}
		if (NodeData.ContainerReference.ValueType.IsValid())
		{
			++Count;
		}
		for (const FParsedEventParam& Param : NodeData.EventReference.Params)
		{
			if (Param.PinType.IsValid())
			{
				++Count;
			}
		}
		return Count;
	}

};

EParsedBlueprintNodeType TextToBlueprintGenerator::ResolveNodeType(const TSharedPtr<FJsonObject>& NodeObject)
{
	if (!NodeObject.IsValid())
	{
		return EParsedBlueprintNodeType::Unknown;
	}

	FString NodeTypeString;
	NodeObject->TryGetStringField(TEXT("type"), NodeTypeString);
	const FString NormalizedNodeType = FTextToBlueprintGeneratorLocalUtils::NormalizeNodeTypeName(NodeTypeString);

	if (NormalizedNodeType.Equals(TEXT("K2Node_CallFunction"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::CallFunction;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_VariableGet"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::VariableGet;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_VariableSet"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::VariableSet;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_MacroInstance"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::MacroInstance;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_IfThenElse"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("Branch"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::Branch;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_ExecutionSequence"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("Sequence"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::Sequence;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_CustomEvent"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("CustomEvent"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::CustomEvent;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_Event"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("Event"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::Event;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_CallDelegate"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("CallDelegate"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::CallDelegate;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_AddDelegate"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("AddDelegate"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("BindEvent"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::AddDelegate;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_RemoveDelegate"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("RemoveDelegate"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("UnbindEvent"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::RemoveDelegate;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_ClearDelegate"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("ClearDelegate"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("UnbindAll"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::ClearDelegate;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_AssignDelegate"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("AssignDelegate"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::AssignDelegate;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_CreateDelegate"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("CreateDelegate"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("CreateEvent"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::CreateDelegate;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_MakeArray"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("MakeArray"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::MakeArray;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_MakeMap"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("MakeMap"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::MakeMap;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_MakeSet"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("MakeSet"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::MakeSet;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_MakeStruct"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("MakeStruct"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::MakeStruct;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_BreakStruct"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("BreakStruct"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::BreakStruct;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_Self"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("Self"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::Self;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_DynamicCast"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("DynamicCast"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("Cast"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::DynamicCast;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_SpawnActorFromClass"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("SpawnActorFromClass"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("SpawnActor"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::SpawnActorFromClass;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_FormatText"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("FormatText"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::FormatText;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_GetArrayItem"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("GetArrayItem"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::GetArrayItem;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_Timeline"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("Timeline"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::Timeline;
	}

	// v2.3 — Knot 族（别名: "Reroute"）
	if (NormalizedNodeType.Equals(TEXT("K2Node_Knot"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("Knot"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("Reroute"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::Knot;
	}

	// v2.3 — Comment 族
	if (NormalizedNodeType.Equals(TEXT("EdGraphNode_Comment"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("Comment"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::Comment;
	}

	// v2.3 — Literal 族
	if (NormalizedNodeType.Equals(TEXT("K2Node_Literal"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("Literal"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::Literal;
	}

	// v2.3 — GetEnumeratorName 族
	if (NormalizedNodeType.Equals(TEXT("K2Node_GetEnumeratorName"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("GetEnumeratorName"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::GetEnumeratorName;
	}

	// v2.3 — GetEnumeratorNameAsString 族
	if (NormalizedNodeType.Equals(TEXT("K2Node_GetEnumeratorNameAsString"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("GetEnumeratorNameAsString"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::GetEnumeratorNameAsString;
	}

	// v2.3 — ComponentBoundEvent 族
	if (NormalizedNodeType.Equals(TEXT("K2Node_ComponentBoundEvent"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("ComponentBoundEvent"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::ComponentBoundEvent;
	}

	// v2.9 — Enhanced Input Action
	if (NormalizedNodeType.Equals(TEXT("K2Node_EnhancedInputAction"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("EnhancedInputAction"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("InputAction"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::EnhancedInputAction;
	}

	// v2.9 — PromotableOperator（加减乘除、比较等可提升运算符）
	if (NormalizedNodeType.Equals(TEXT("K2Node_PromotableOperator"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("PromotableOperator"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::PromotableOperator;
	}

	// v2.9 — CommutativeAssociativeBinaryOperator（交换结合律二元运算符）
	if (NormalizedNodeType.Equals(TEXT("K2Node_CommutativeAssociativeBinaryOperator"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("CommutativeAssociativeBinaryOperator"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::CommutativeAssociativeBinaryOperator;
	}

	// v2.9 — Switch 族
	if (NormalizedNodeType.Equals(TEXT("K2Node_SwitchInteger"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("SwitchInteger"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("SwitchOnInt"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::SwitchInteger;
	}
	if (NormalizedNodeType.Equals(TEXT("K2Node_SwitchString"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("SwitchString"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("SwitchOnString"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::SwitchString;
	}
	if (NormalizedNodeType.Equals(TEXT("K2Node_SwitchName"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("SwitchName"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("SwitchOnName"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::SwitchName;
	}
	if (NormalizedNodeType.Equals(TEXT("K2Node_SwitchEnum"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("SwitchEnum"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("SwitchOnEnum"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::SwitchEnum;
	}

	// v2.9 — Select
	if (NormalizedNodeType.Equals(TEXT("K2Node_Select"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("Select"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::Select;
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

		if (NormalizedNodeType.Contains(TEXT("set"), ESearchCase::IgnoreCase))
		{
			return EParsedBlueprintNodeType::VariableSet;
		}

		return EParsedBlueprintNodeType::VariableGet;
	}

	if (NormalizedNodeType.IsEmpty() || NormalizedNodeType.Equals(TEXT("K2Node_CallFunction"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::CallFunction;
	}

	return EParsedBlueprintNodeType::Unknown;
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

	if (Result.MacroName.IsEmpty() || FTextToBlueprintGeneratorLocalUtils::IsPlaceholderMacroName(Result.MacroName))
	{
		NodeObject->TryGetStringField(TEXT("function_name"), Result.MacroName);
	}

	if (FTextToBlueprintGeneratorLocalUtils::IsPlaceholderMacroName(Result.MacroName))
	{
		Result.MacroName.Reset();
	}

	if (Result.LibraryType.IsEmpty())
	{
		Result.LibraryType = TEXT("standard");
	}

	return Result;
}

FParsedEventReference TextToBlueprintGenerator::ResolveEventReference(const TSharedPtr<FJsonObject>& NodeObject)
{
	FParsedEventReference Result;
	if (!NodeObject.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* EventObject = nullptr;
	if (NodeObject->TryGetObjectField(TEXT("event"), EventObject) && EventObject && EventObject->IsValid())
	{
		(*EventObject)->TryGetStringField(TEXT("event_name"), Result.EventName);

		const TArray<TSharedPtr<FJsonValue>>* ParamsArray = nullptr;
		if ((*EventObject)->TryGetArrayField(TEXT("params"), ParamsArray) && ParamsArray)
		{
			for (const TSharedPtr<FJsonValue>& ParamValue : *ParamsArray)
			{
				const TSharedPtr<FJsonObject> ParamObject = ParamValue->AsObject();
				if (!ParamObject.IsValid())
				{
					continue;
				}

				FParsedEventParam Param;
				ParamObject->TryGetStringField(TEXT("name"), Param.Name);
				const TSharedPtr<FJsonObject>* PinTypeObject = nullptr;
				if (ParamObject->TryGetObjectField(TEXT("pin_type"), PinTypeObject) && PinTypeObject)
				{
					Param.PinType = ResolvePinType(*PinTypeObject);
				}
				if (!Param.Name.IsEmpty())
				{
					Result.Params.Add(Param);
				}
			}
		}
	}

	if (Result.EventName.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("name"), Result.EventName);
	}

	return Result;
}

FParsedDelegateReference TextToBlueprintGenerator::ResolveDelegateReference(const TSharedPtr<FJsonObject>& NodeObject)
{
	FParsedDelegateReference Result;
	if (!NodeObject.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* DelegateObject = nullptr;
	if (NodeObject->TryGetObjectField(TEXT("delegate"), DelegateObject) && DelegateObject && DelegateObject->IsValid())
	{
		(*DelegateObject)->TryGetStringField(TEXT("property_name"), Result.DelegatePropertyName);
		(*DelegateObject)->TryGetStringField(TEXT("function_name"), Result.FunctionName);
	}

	if (Result.DelegatePropertyName.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("delegate_property"), Result.DelegatePropertyName);
	}

	if (Result.DelegatePropertyName.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("name"), Result.DelegatePropertyName);
	}

	return Result;
}

FParsedContainerReference TextToBlueprintGenerator::ResolveContainerReference(const TSharedPtr<FJsonObject>& NodeObject)
{
	FParsedContainerReference Result;
	if (!NodeObject.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* ContainerObject = nullptr;
	if (NodeObject->TryGetObjectField(TEXT("container"), ContainerObject) && ContainerObject && ContainerObject->IsValid())
	{
		if ((*ContainerObject)->HasField(TEXT("num_inputs")))
		{
			Result.NumInputs = static_cast<int32>((*ContainerObject)->GetNumberField(TEXT("num_inputs")));
		}
		if ((*ContainerObject)->HasField(TEXT("num_pairs")))
		{
			Result.NumPairs = static_cast<int32>((*ContainerObject)->GetNumberField(TEXT("num_pairs")));
		}

		const TSharedPtr<FJsonObject>* ElementTypeObject = nullptr;
		if ((*ContainerObject)->TryGetObjectField(TEXT("element_type"), ElementTypeObject) && ElementTypeObject && ElementTypeObject->IsValid())
		{
			Result.ElementType = ResolvePinType(*ElementTypeObject);
		}

		const TSharedPtr<FJsonObject>* KeyTypeObject = nullptr;
		if ((*ContainerObject)->TryGetObjectField(TEXT("key_type"), KeyTypeObject) && KeyTypeObject && KeyTypeObject->IsValid())
		{
			Result.KeyType = ResolvePinType(*KeyTypeObject);
		}

		const TSharedPtr<FJsonObject>* ValueTypeObject = nullptr;
		if ((*ContainerObject)->TryGetObjectField(TEXT("value_type"), ValueTypeObject) && ValueTypeObject && ValueTypeObject->IsValid())
		{
			Result.ValueType = ResolvePinType(*ValueTypeObject);
		}
	}

	return Result;
}

FParsedStructReference TextToBlueprintGenerator::ResolveStructReference(const TSharedPtr<FJsonObject>& NodeObject)
{
	FParsedStructReference Result;
	if (!NodeObject.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* StructObject = nullptr;
	if (NodeObject->TryGetObjectField(TEXT("struct"), StructObject) && StructObject && StructObject->IsValid())
	{
		(*StructObject)->TryGetStringField(TEXT("struct_path"), Result.StructPath);
	}

	if (Result.StructPath.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("struct_path"), Result.StructPath);
	}

	return Result;
}

FParsedCastReference TextToBlueprintGenerator::ResolveCastReference(const TSharedPtr<FJsonObject>& NodeObject)
{
	FParsedCastReference Result;
	if (!NodeObject.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* CastObject = nullptr;
	if (NodeObject->TryGetObjectField(TEXT("cast"), CastObject) && CastObject && CastObject->IsValid())
	{
		(*CastObject)->TryGetStringField(TEXT("target_class_path"), Result.TargetClassPath);
	}

	if (Result.TargetClassPath.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("target_class_path"), Result.TargetClassPath);
	}

	return Result;
}

FParsedSpawnReference TextToBlueprintGenerator::ResolveSpawnReference(const TSharedPtr<FJsonObject>& NodeObject)
{
	FParsedSpawnReference Result;
	if (!NodeObject.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* SpawnObject = nullptr;
	if (NodeObject->TryGetObjectField(TEXT("spawn"), SpawnObject) && SpawnObject && SpawnObject->IsValid())
	{
		(*SpawnObject)->TryGetStringField(TEXT("class_path"), Result.ClassPath);
	}

	if (Result.ClassPath.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("class_path"), Result.ClassPath);
	}

	return Result;
}

FParsedFormatTextReference TextToBlueprintGenerator::ResolveFormatTextReference(const TSharedPtr<FJsonObject>& NodeObject)
{
	FParsedFormatTextReference Result;
	if (!NodeObject.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* FormatObject = nullptr;
	if (NodeObject->TryGetObjectField(TEXT("format_text"), FormatObject) && FormatObject && FormatObject->IsValid())
	{
		(*FormatObject)->TryGetStringField(TEXT("format_string"), Result.FormatString);
	}

	if (Result.FormatString.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("format_string"), Result.FormatString);
	}

	return Result;
}

FParsedTimelineReference TextToBlueprintGenerator::ResolveTimelineReference(const TSharedPtr<FJsonObject>& NodeObject)
{
	FParsedTimelineReference Result;
	if (!NodeObject.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* TLObject = nullptr;
	if (NodeObject->TryGetObjectField(TEXT("timeline"), TLObject) && TLObject && TLObject->IsValid())
	{
		(*TLObject)->TryGetStringField(TEXT("name"), Result.TimelineName);
		(*TLObject)->TryGetBoolField(TEXT("auto_play"), Result.bAutoPlay);
		(*TLObject)->TryGetBoolField(TEXT("loop"), Result.bLoop);

		const TArray<TSharedPtr<FJsonValue>>* FloatArray = nullptr;
		if ((*TLObject)->TryGetArrayField(TEXT("float_tracks"), FloatArray) && FloatArray)
		{
			for (const TSharedPtr<FJsonValue>& V : *FloatArray)
			{
				Result.FloatTracks.Add(V->AsString());
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* VectorArray = nullptr;
		if ((*TLObject)->TryGetArrayField(TEXT("vector_tracks"), VectorArray) && VectorArray)
		{
			for (const TSharedPtr<FJsonValue>& V : *VectorArray)
			{
				Result.VectorTracks.Add(V->AsString());
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* EventArray = nullptr;
		if ((*TLObject)->TryGetArrayField(TEXT("event_tracks"), EventArray) && EventArray)
		{
			for (const TSharedPtr<FJsonValue>& V : *EventArray)
			{
				Result.EventTracks.Add(V->AsString());
			}
		}
	}

	return Result;
}

FParsedLiteralReference TextToBlueprintGenerator::ResolveLiteralReference(const TSharedPtr<FJsonObject>& NodeObject)
{
	FParsedLiteralReference Result;
	if (!NodeObject.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* LiteralObject = nullptr;
	if (NodeObject->TryGetObjectField(TEXT("literal"), LiteralObject) && LiteralObject && LiteralObject->IsValid())
	{
		(*LiteralObject)->TryGetStringField(TEXT("object_path"), Result.ObjectPath);
	}

	if (Result.ObjectPath.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("object_path"), Result.ObjectPath);
	}

	return Result;
}

FParsedComponentBoundEventReference TextToBlueprintGenerator::ResolveComponentBoundEventReference(const TSharedPtr<FJsonObject>& NodeObject)
{
	FParsedComponentBoundEventReference Result;
	if (!NodeObject.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* EventObject = nullptr;
	if (NodeObject->TryGetObjectField(TEXT("component_event"), EventObject) && EventObject && EventObject->IsValid())
	{
		(*EventObject)->TryGetStringField(TEXT("delegate_property"), Result.DelegatePropertyName);
		(*EventObject)->TryGetStringField(TEXT("delegate_owner_class"), Result.DelegateOwnerClassPath);
		(*EventObject)->TryGetStringField(TEXT("component_property"), Result.ComponentPropertyName);
	}

	if (Result.DelegatePropertyName.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("delegate_property"), Result.DelegatePropertyName);
	}
	if (Result.ComponentPropertyName.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("component_property"), Result.ComponentPropertyName);
	}
	if (Result.DelegateOwnerClassPath.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("delegate_owner_class"), Result.DelegateOwnerClassPath);
	}

	return Result;
}

FParsedCommentReference TextToBlueprintGenerator::ResolveCommentReference(const TSharedPtr<FJsonObject>& NodeObject)
{
	FParsedCommentReference Result;
	if (!NodeObject.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* CommentObject = nullptr;
	if (NodeObject->TryGetObjectField(TEXT("comment"), CommentObject) && CommentObject && CommentObject->IsValid())
	{
		(*CommentObject)->TryGetStringField(TEXT("text"), Result.CommentText);
		(*CommentObject)->TryGetStringField(TEXT("color"), Result.CommentColor);
		if ((*CommentObject)->HasField(TEXT("width")))
		{
			Result.Width = static_cast<float>((*CommentObject)->GetNumberField(TEXT("width")));
		}
		if ((*CommentObject)->HasField(TEXT("height")))
		{
			Result.Height = static_cast<float>((*CommentObject)->GetNumberField(TEXT("height")));
		}
		if ((*CommentObject)->HasField(TEXT("font_size")))
		{
			Result.FontSize = static_cast<int32>((*CommentObject)->GetNumberField(TEXT("font_size")));
		}
	}

	if (Result.CommentText.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("comment_text"), Result.CommentText);
	}

	return Result;
}

// v2.9 — Enhanced Input Action 引用
FParsedEnhancedInputActionReference TextToBlueprintGenerator::ResolveEnhancedInputActionReference(const TSharedPtr<FJsonObject>& NodeObject)
{
	FParsedEnhancedInputActionReference Result;
	if (!NodeObject.IsValid())
	{
		return Result;
	}

	// 尝试从 "input_action" 对象读取
	const TSharedPtr<FJsonObject>* InputActionObject = nullptr;
	if (NodeObject->TryGetObjectField(TEXT("input_action"), InputActionObject) && InputActionObject && InputActionObject->IsValid())
	{
		(*InputActionObject)->TryGetStringField(TEXT("path"), Result.InputActionPath);
	}

	// 回退到直接字段
	if (Result.InputActionPath.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("input_action_path"), Result.InputActionPath);
	}
	if (Result.InputActionPath.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("input_action"), Result.InputActionPath);
	}

	return Result;
}

// v2.9 — Switch 引用
FParsedSwitchReference TextToBlueprintGenerator::ResolveSwitchReference(const TSharedPtr<FJsonObject>& NodeObject)
{
	FParsedSwitchReference Result;
	if (!NodeObject.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* SwitchObject = nullptr;
	if (NodeObject->TryGetObjectField(TEXT("switch"), SwitchObject) && SwitchObject && SwitchObject->IsValid())
	{
		// case_values 数组
		const TArray<TSharedPtr<FJsonValue>>* CaseArray = nullptr;
		if ((*SwitchObject)->TryGetArrayField(TEXT("case_values"), CaseArray) && CaseArray)
		{
			for (const TSharedPtr<FJsonValue>& CaseValue : *CaseArray)
			{
				Result.CaseValues.Add(CaseValue->AsString());
			}
		}

		(*SwitchObject)->TryGetBoolField(TEXT("has_default"), Result.bHasDefaultPin);
		(*SwitchObject)->TryGetStringField(TEXT("enum_path"), Result.EnumPath);
		if ((*SwitchObject)->HasField(TEXT("start_index")))
		{
			Result.StartIndex = static_cast<int32>((*SwitchObject)->GetNumberField(TEXT("start_index")));
		}
	}

	// 回退：直接从顶层字段读取
	if (Result.CaseValues.Num() == 0)
	{
		const TArray<TSharedPtr<FJsonValue>>* CaseArray = nullptr;
		if (NodeObject->TryGetArrayField(TEXT("case_values"), CaseArray) && CaseArray)
		{
			for (const TSharedPtr<FJsonValue>& CaseValue : *CaseArray)
			{
				Result.CaseValues.Add(CaseValue->AsString());
			}
		}
	}
	if (Result.EnumPath.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("enum_path"), Result.EnumPath);
	}

	return Result;
}

// v2.9 — Select 引用
FParsedSelectReference TextToBlueprintGenerator::ResolveSelectReference(const TSharedPtr<FJsonObject>& NodeObject)
{
	FParsedSelectReference Result;
	if (!NodeObject.IsValid())
	{
		return Result;
	}

	const TSharedPtr<FJsonObject>* SelectObject = nullptr;
	if (NodeObject->TryGetObjectField(TEXT("select"), SelectObject) && SelectObject && SelectObject->IsValid())
	{
		if ((*SelectObject)->HasField(TEXT("num_options")))
		{
			Result.NumOptions = static_cast<int32>((*SelectObject)->GetNumberField(TEXT("num_options")));
		}
		(*SelectObject)->TryGetStringField(TEXT("enum_path"), Result.EnumPath);
	}

	// 回退到直接字段
	if (Result.EnumPath.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("enum_path"), Result.EnumPath);
	}
	if (NodeObject->HasField(TEXT("num_options")))
	{
		Result.NumOptions = static_cast<int32>(NodeObject->GetNumberField(TEXT("num_options")));
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
		OutPinType.PinSubCategory = FTextToBlueprintGeneratorLocalUtils::ResolveRealSubCategory(Category);
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
		BlueprintPath = FTextToBlueprintGeneratorLocalUtils::StandardMacroLibraryPath;
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

	const FString NormalizedKey = FTextToBlueprintGeneratorLocalUtils::NormalizePinKey(RequestedPinName);
	for (UEdGraphPin* Pin : TargetNode->Pins)
	{
		if (!Pin)
		{
			continue;
		}

		if (FTextToBlueprintGeneratorLocalUtils::NormalizePinKey(Pin->PinName.ToString()) == NormalizedKey)
		{
			return Pin;
		}
	}

	if (NormalizedKey == FTextToBlueprintGeneratorLocalUtils::NormalizePinKey(TEXT("execute")))
	{
		return TargetNode->FindPin(UEdGraphSchema_K2::PN_Execute);
	}

	if (NormalizedKey == FTextToBlueprintGeneratorLocalUtils::NormalizePinKey(TEXT("then")))
	{
		return TargetNode->FindPin(UEdGraphSchema_K2::PN_Then);
	}

	if (NormalizedKey == FTextToBlueprintGeneratorLocalUtils::NormalizePinKey(TEXT("completed")))
	{
		if (UEdGraphPin* CompletedPin = TargetNode->FindPin(UEdGraphSchema_K2::PN_Completed))
		{
			return CompletedPin;
		}
	}

	if (NormalizedKey == FTextToBlueprintGeneratorLocalUtils::NormalizePinKey(TEXT("loopbody")))
	{
		if (UEdGraphPin* LoopBodyPin = TargetNode->FindPin(TEXT("LoopBody")))
		{
			return LoopBodyPin;
		}
	}

	if (NormalizedKey == FTextToBlueprintGeneratorLocalUtils::NormalizePinKey(TEXT("firstindex")))
	{
		if (UEdGraphPin* FirstIndexPin = TargetNode->FindPin(TEXT("FirstIndex")))
		{
			return FirstIndexPin;
		}
	}

	if (NormalizedKey == FTextToBlueprintGeneratorLocalUtils::NormalizePinKey(TEXT("lastindex")))
	{
		if (UEdGraphPin* LastIndexPin = TargetNode->FindPin(TEXT("LastIndex")))
		{
			return LastIndexPin;
		}
	}

	if (NormalizedKey == FTextToBlueprintGeneratorLocalUtils::NormalizePinKey(TEXT("index")))
	{
		if (UEdGraphPin* IndexPin = TargetNode->FindPin(TEXT("Index")))
		{
			return IndexPin;
		}
	}

	if (NormalizedKey == FTextToBlueprintGeneratorLocalUtils::NormalizePinKey(TEXT("value")))
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
	if (NormalizedKey == FTextToBlueprintGeneratorLocalUtils::NormalizePinKey(TEXT("valid")) || NormalizedKey == FTextToBlueprintGeneratorLocalUtils::NormalizePinKey(TEXT("cast_succeeded")))
	{
		if (UEdGraphPin* ValidPin = TargetNode->FindPin(UEdGraphSchema_K2::PN_CastSucceeded))
		{
			return ValidPin;
		}
	}

	if (NormalizedKey == FTextToBlueprintGeneratorLocalUtils::NormalizePinKey(TEXT("invalid")) || NormalizedKey == FTextToBlueprintGeneratorLocalUtils::NormalizePinKey(TEXT("cast_failed")))
	{
		if (UEdGraphPin* FailedPin = TargetNode->FindPin(TEXT("CastFailed")))
		{
			return FailedPin;
		}
	}

	if (NormalizedKey == FTextToBlueprintGeneratorLocalUtils::NormalizePinKey(TEXT("cast_result")))
	{
		for (UEdGraphPin* Pin : TargetNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output && Pin->PinName.ToString().StartsWith(TEXT("As")))
			{
				return Pin;
			}
		}
	}

	if (NormalizedKey == FTextToBlueprintGeneratorLocalUtils::NormalizePinKey(TEXT("success")) || NormalizedKey == FTextToBlueprintGeneratorLocalUtils::NormalizePinKey(TEXT("bsuccess")) || NormalizedKey == FTextToBlueprintGeneratorLocalUtils::NormalizePinKey(TEXT("bool_success")))
	{
		if (UEdGraphPin* SuccessPin = TargetNode->FindPin(TEXT("bSuccess")))
		{
			return SuccessPin;
		}
	}

	return nullptr;
}

bool TextToBlueprintGenerator::ApplyPinDefaultValue(
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

UFunction* TextToBlueprintGenerator::FindFunctionByName(const FString& FuncName)
{
	// Legacy fallback for older internal node handlers. TaskSpec call_function should use
	// ResolveFunctionForGraph so graph compatibility and ambiguity checks run before spawning.
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

FBlueprintHelperCallFunctionResolveResult TextToBlueprintGenerator::ResolveFunctionForGraph(
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

	// v2.1 多图 JSON 需要走 Blueprint 级入口，否则 graphs 数组中的节点不会被分发到对应图表。
	if (JsonObject->HasField(TEXT("graphs")))
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
		if (!Blueprint)
		{
			Result.Message = TEXT("无法从目标图表获取蓝图对象，graphs 数组无法执行。");
			return Result;
		}

		return GenerateMultiGraphFromJson(Blueprint, TrimmedJsonString, OutUnresolvedNodes);
	}

	// === Schema 2.0：蓝图级操作 ===
	const TArray<TSharedPtr<FJsonValue>>* BlueprintOpsArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("blueprint_operations"), BlueprintOpsArray) && BlueprintOpsArray && BlueprintOpsArray->Num() > 0)
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
		if (!Blueprint)
		{
			Result.Message = TEXT("无法从目标图表获取蓝图对象，blueprint_operations 无法执行。");
			return Result;
		}

		for (const TSharedPtr<FJsonValue>& OpValue : *BlueprintOpsArray)
		{
			const TSharedPtr<FJsonObject> OpObject = OpValue->AsObject();
			if (!OpObject.IsValid())
			{
				continue;
			}

			FString OpName;
			OpObject->TryGetStringField(TEXT("op"), OpName);
			if (OpName.IsEmpty())
			{
				continue;
			}

			IBlueprintOperationHandler* OpHandler = FBlueprintOperationHandlerRegistry::Get().FindHandler(OpName);
			if (!OpHandler)
			{
				TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
				UnresolvedItem->DisplayText = FString::Printf(TEXT("BlueprintOp: %s"), *OpName);
				UnresolvedItem->Reason = FString::Printf(TEXT("未识别的蓝图级操作：%s"), *OpName);
				OutUnresolvedNodes.Add(UnresolvedItem);
				continue;
			}

			FString OpError;
			if (!OpHandler->Execute(Blueprint, OpObject, OpError))
			{
				TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
				UnresolvedItem->DisplayText = FString::Printf(TEXT("BlueprintOp: %s"), *OpName);
				UnresolvedItem->Reason = OpError;
				OutUnresolvedNodes.Add(UnresolvedItem);
			}
		}

		// 蓝图级操作完成后编译骨架，确保后续节点可引用新创建的变量/函数/分发器
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	TArray<FParsedNode> ParsedNodes;
	TArray<FParsedLink> ParsedLinks;
	TArray<FParsedLocalVariableDeclaration> ParsedLocalVariableDeclarations;
	TArray<FBlueprintGeneratorDiagnostic> DefaultValueDiagnostics;
	TArray<FBlueprintGeneratorDiagnostic> PinTypeDiagnostics;
	TArray<FBlueprintGeneratorDiagnostic> ConnectionDiagnostics;
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
			ParsedNode.SourceType = FTextToBlueprintGeneratorLocalUtils::NormalizeNodeTypeName(ParsedNode.SourceType);
			ParsedNode.NodeType = ResolveNodeType(NodeObject);
			ParsedNode.FunctionName = ResolveNodeFunctionName(NodeObject);
			ParsedNode.X = NodeObject->HasField(TEXT("x")) ? static_cast<float>(NodeObject->GetNumberField(TEXT("x"))) : 0.0f;
			ParsedNode.Y = NodeObject->HasField(TEXT("y")) ? static_cast<float>(NodeObject->GetNumberField(TEXT("y"))) : 0.0f;
			ParsedNode.VariableReference = ResolveVariableReference(NodeObject);
			ParsedNode.MacroReference = ResolveMacroReference(NodeObject);
			ParsedNode.EventReference = ResolveEventReference(NodeObject);
			ParsedNode.DelegateReference = ResolveDelegateReference(NodeObject);
			ParsedNode.ContainerReference = ResolveContainerReference(NodeObject);
			ParsedNode.StructReference = ResolveStructReference(NodeObject);
			ParsedNode.CastReference = ResolveCastReference(NodeObject);
			ParsedNode.SpawnReference = ResolveSpawnReference(NodeObject);
			ParsedNode.FormatTextReference = ResolveFormatTextReference(NodeObject);
			ParsedNode.TimelineReference = ResolveTimelineReference(NodeObject);
			ParsedNode.LiteralReference = ResolveLiteralReference(NodeObject);
			ParsedNode.ComponentBoundEventReference = ResolveComponentBoundEventReference(NodeObject);
			ParsedNode.CommentReference = ResolveCommentReference(NodeObject);
			ParsedNode.EnhancedInputActionReference = ResolveEnhancedInputActionReference(NodeObject);
			ParsedNode.SwitchReference = ResolveSwitchReference(NodeObject);
			ParsedNode.SelectReference = ResolveSelectReference(NodeObject);

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

	int32 RequestedDefaultValueCount = 0;
	int32 RequestedPinTypeCount = 0;
	for (const FParsedLocalVariableDeclaration& Declaration : ParsedLocalVariableDeclarations)
	{
		if (Declaration.PinType.IsValid())
		{
			++RequestedPinTypeCount;
		}
	}
	for (const FParsedNode& ParsedNode : ParsedNodes)
	{
		RequestedDefaultValueCount += ParsedNode.DefaultValues.Num();
		RequestedPinTypeCount += FTextToBlueprintGeneratorLocalUtils::CountRequestedPinTypes(ParsedNode);
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
			if (FTextToBlueprintGeneratorLocalUtils::IsInvalidPinTypeFailure(EnsureErrorMessage))
			{
				PinTypeDiagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
					TEXT("invalid_pin_type"),
					Declaration.Name,
					Declaration.Name,
					EnsureErrorMessage));
			}

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
		// v2.9 — 跳过虚拟入口/结果节点（导出不生成它们，但 AI 可能手动写入；导入时从图表中自动发现）
		if (ParsedNode.Id == TEXT("__function_entry__") || ParsedNode.Id == TEXT("__function_result__")
			|| ParsedNode.SourceType.Equals(TEXT("K2Node_FunctionEntry"), ESearchCase::IgnoreCase)
			|| ParsedNode.SourceType.Equals(TEXT("K2Node_FunctionResult"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		// v2.3 — Comment 节点特殊处理（UEdGraphNode_Comment 不是 UK2Node）
		if (ParsedNode.NodeType == EParsedBlueprintNodeType::Comment)
		{
			UEdGraphNode_Comment* CommentNode = NewObject<UEdGraphNode_Comment>(TargetGraph);
			TargetGraph->AddNode(CommentNode, true, false);
			CommentNode->CreateNewGuid();
			CommentNode->NodePosX = static_cast<int32>(ParsedNode.X);
			CommentNode->NodePosY = static_cast<int32>(ParsedNode.Y);
			CommentNode->NodeComment = ParsedNode.CommentReference.CommentText;
			CommentNode->FontSize = ParsedNode.CommentReference.FontSize;
			CommentNode->NodeWidth = static_cast<int32>(ParsedNode.CommentReference.Width);
			CommentNode->NodeHeight = static_cast<int32>(ParsedNode.CommentReference.Height);
			if (!ParsedNode.CommentReference.CommentColor.IsEmpty())
			{
				FLinearColor Color;
				if (Color.InitFromString(ParsedNode.CommentReference.CommentColor))
				{
					CommentNode->CommentColor = Color;
				}
			}
			++GeneratedNodeCount;
			continue;
		}

		UK2Node* SpawnedNode = nullptr;
		FString SpawnErrorMessage;

		IBlueprintNodeHandler* Handler = FBlueprintNodeHandlerRegistry::Get().FindHandler(ParsedNode.NodeType);
		if (Handler)
		{
			SpawnedNode = Handler->Spawn(TargetGraph, ParsedNode, SpawnErrorMessage);
		}
		else
		{
			SpawnErrorMessage = ParsedNode.SourceType.IsEmpty()
				? TEXT("未识别的节点类型，且缺少可用的函数/变量/宏描述。")
				: FString::Printf(TEXT("未识别的节点类型：%s"), *ParsedNode.SourceType);
		}

		if (SpawnedNode)
		{
			IdToSpawnedNode.Add(ParsedNode.Id, SpawnedNode);
			++GeneratedNodeCount;
			continue;
		}

		if (FTextToBlueprintGeneratorLocalUtils::IsInvalidPinTypeFailure(SpawnErrorMessage))
		{
			PinTypeDiagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
				TEXT("invalid_pin_type"),
				ParsedNode.Id,
				FTextToBlueprintGeneratorLocalUtils::FindDiagnosticPinName(ParsedNode, SpawnErrorMessage),
				SpawnErrorMessage));
		}

		TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
		UnresolvedItem->NodeData = ParsedNode;
		UnresolvedItem->DisplayText = ParsedNode.FunctionName.IsEmpty()
			? FString::Printf(TEXT("%s (%s)"), *ParsedNode.SourceType, *ParsedNode.Id)
			: FString::Printf(TEXT("%s (%s)"), *ParsedNode.FunctionName, *ParsedNode.Id);
		UnresolvedItem->Reason = SpawnErrorMessage.IsEmpty() ? TEXT("不支持的节点类型或配置不完整。") : SpawnErrorMessage;
		OutUnresolvedNodes.Add(UnresolvedItem);
	}

	// 将图中已有的 FunctionEntry / FunctionResult 注入 ID 映射，以便连线恢复
	for (UEdGraphNode* ExistingNode : TargetGraph->Nodes)
	{
		if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(ExistingNode))
		{
			IdToSpawnedNode.FindOrAdd(TEXT("__function_entry__"), Entry);
		}
		else if (UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(ExistingNode))
		{
			IdToSpawnedNode.FindOrAdd(TEXT("__function_result__"), ResultNode);
		}
	}

	// v2.9 — existing_node_refs：允许增量导入引用图中已有节点
	const TArray<TSharedPtr<FJsonValue>>* ExistingRefsArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("existing_node_refs"), ExistingRefsArray) && ExistingRefsArray)
	{
		for (const TSharedPtr<FJsonValue>& RefValue : *ExistingRefsArray)
		{
			const TSharedPtr<FJsonObject> RefObject = RefValue->AsObject();
			if (!RefObject.IsValid()) continue;

			FString RefId;
			RefObject->TryGetStringField(TEXT("id"), RefId);
			if (RefId.IsEmpty()) continue;

			FString MatchTitle;
			RefObject->TryGetStringField(TEXT("node_title"), MatchTitle);
			FString MatchGuid;
			RefObject->TryGetStringField(TEXT("node_guid"), MatchGuid);

			for (UEdGraphNode* RefCandidate : TargetGraph->Nodes)
			{
				UK2Node* K2Existing = Cast<UK2Node>(RefCandidate);
				if (!K2Existing) continue;
				if (IdToSpawnedNode.FindKey(K2Existing)) continue; // 已经被映射

				bool bMatched = false;
				if (!MatchGuid.IsEmpty())
				{
					bMatched = RefCandidate->NodeGuid.ToString(EGuidFormats::Digits) == MatchGuid;
				}
				else if (!MatchTitle.IsEmpty())
				{
					const FString Title = RefCandidate->GetNodeTitle(ENodeTitleType::ListView).ToString();
					bMatched = Title.Contains(MatchTitle);
				}

				if (bMatched)
				{
					IdToSpawnedNode.Add(RefId, K2Existing);
					break;
				}
			}
		}
	}

	// v2.9 — 先 Reconstruct 新生成的节点以确保引脚完整，再连线（避免连线后 Reconstruct 破坏连接）
	for (const auto& Pair : IdToSpawnedNode)
	{
		if (Pair.Value)
		{
			TargetGraph->GetSchema()->ReconstructNode(*Pair.Value);
		}
	}

	int32 AppliedDefaultValueCount = 0;
	for (const FParsedNode& ParsedNode : ParsedNodes)
	{
		UK2Node** SpawnedNodePtr = IdToSpawnedNode.Find(ParsedNode.Id);
		if (!SpawnedNodePtr || !*SpawnedNodePtr)
		{
			continue;
		}

		TArray<FBlueprintGeneratorDiagnostic> NodeDiagnostics = ApplyDefaultValues(*SpawnedNodePtr, ParsedNode.DefaultValues, ParsedNode.Id);
		AppliedDefaultValueCount += FMath::Max(0, ParsedNode.DefaultValues.Num() - NodeDiagnostics.Num());
		DefaultValueDiagnostics.Append(NodeDiagnostics);
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	int32 CreatedConnectionCount = 0;
	for (const FParsedLink& ParsedLink : ParsedLinks)
	{
		if (!Schema)
		{
			ConnectionDiagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
				TEXT("link_connection_rejected"),
				ParsedLink.FromId,
				ParsedLink.FromPin,
				TEXT("连线创建失败：K2 Schema 无效。")));
			continue;
		}

		UK2Node** FromNodePtr = IdToSpawnedNode.Find(ParsedLink.FromId);
		UK2Node** ToNodePtr = IdToSpawnedNode.Find(ParsedLink.ToId);
		if (!FromNodePtr || !*FromNodePtr)
		{
			ConnectionDiagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
				TEXT("link_node_not_found"),
				ParsedLink.FromId,
				ParsedLink.FromPin,
				FString::Printf(TEXT("连线来源节点未找到：%s。"), *ParsedLink.FromId)));
			continue;
		}
		if (!ToNodePtr || !*ToNodePtr)
		{
			ConnectionDiagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
				TEXT("link_node_not_found"),
				ParsedLink.ToId,
				ParsedLink.ToPin,
				FString::Printf(TEXT("连线目标节点未找到：%s。"), *ParsedLink.ToId)));
			continue;
		}

		UK2Node* FromNode = *FromNodePtr;
		UK2Node* ToNode = *ToNodePtr;
		UEdGraphPin* FromPin = FindPinByAlias(FromNode, ParsedLink.FromPin);
		UEdGraphPin* ToPin = FindPinByAlias(ToNode, ParsedLink.ToPin);
		if (!FromPin)
		{
			ConnectionDiagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
				TEXT("link_pin_not_found"),
				ParsedLink.FromId,
				ParsedLink.FromPin,
				FString::Printf(TEXT("连线来源引脚未找到：%s.%s。"), *ParsedLink.FromId, *ParsedLink.FromPin)));
			continue;
		}
		if (!ToPin)
		{
			ConnectionDiagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
				TEXT("link_pin_not_found"),
				ParsedLink.ToId,
				ParsedLink.ToPin,
				FString::Printf(TEXT("连线目标引脚未找到：%s.%s。"), *ParsedLink.ToId, *ParsedLink.ToPin)));
			continue;
		}

		const FPinConnectionResponse ConnectionResponse = Schema->CanCreateConnection(FromPin, ToPin);
		if (Schema->TryCreateConnection(FromPin, ToPin))
		{
			++CreatedConnectionCount;
		}
		else
		{
			ConnectionDiagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
				TEXT("link_connection_rejected"),
				ParsedLink.FromId,
				ParsedLink.FromPin,
				ConnectionResponse.Message.IsEmpty()
					? FString::Printf(TEXT("Schema 拒绝连线：%s.%s -> %s.%s。"),
						*ParsedLink.FromId, *ParsedLink.FromPin, *ParsedLink.ToId, *ParsedLink.ToPin)
					: ConnectionResponse.Message.ToString()));
		}
	}

	TargetGraph->NotifyGraphChanged();

	Result.bSucceed = GeneratedNodeCount > 0 || CreatedConnectionCount > 0;
	Result.GeneratedNodeCount = GeneratedNodeCount;
	Result.RequestedDefaultValueCount = RequestedDefaultValueCount;
	Result.AppliedDefaultValueCount = AppliedDefaultValueCount;
	Result.DefaultValueDiagnostics = MoveTemp(DefaultValueDiagnostics);
	Result.RequestedPinTypeCount = RequestedPinTypeCount;
	Result.ResolvedPinTypeCount = FMath::Max(0, RequestedPinTypeCount - PinTypeDiagnostics.Num());
	Result.PinTypeDiagnostics = MoveTemp(PinTypeDiagnostics);
	Result.RequestedConnectionCount = ParsedLinks.Num();
	Result.CreatedConnectionCount = CreatedConnectionCount;
	Result.ConnectionDiagnostics = MoveTemp(ConnectionDiagnostics);
	Result.UnresolvedNodeCount = OutUnresolvedNodes.Num();
	if (Result.bSucceed)
	{
		Result.Message = FString::Printf(TEXT("生成完成：成功 %d 个节点，建立 %d 条连线，未匹配 %d 个。"), Result.GeneratedNodeCount, CreatedConnectionCount, Result.UnresolvedNodeCount);
	}
	else if (Result.UnresolvedNodeCount > 0)
	{
		Result.Message = FString::Printf(TEXT("未生成任何节点：共有 %d 个节点未匹配，请检查 JSON 类型与描述字段。"), Result.UnresolvedNodeCount);
	}
	else
	{
		Result.Message = TEXT("未生成任何节点，请检查 JSON 内容是否符合规则。");
	}
	return Result;
}

// ============================================================================
// v2.1 — 多图支持
// ============================================================================

UEdGraph* TextToBlueprintGenerator::FindGraphByName(UBlueprint* Blueprint, const FString& GraphName)
{
	if (!Blueprint || GraphName.IsEmpty())
	{
		return nullptr;
	}

	// EventGraph：搜索 UbergraphPages
	if (GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (Graph)
			{
				return Graph;
			}
		}
		return nullptr;
	}

	// 也在 UbergraphPages 中按精确名称搜索
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			return Graph;
		}
	}

	// 函数图
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			return Graph;
		}
	}

	// 宏图
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			return Graph;
		}
	}

	// 委托签名图
	for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			return Graph;
		}
	}

	return nullptr;
}

FBlueprintGenerateResult TextToBlueprintGenerator::GenerateNodesAndLinksForGraph(
	UEdGraph* TargetGraph, const TSharedPtr<FJsonObject>& GraphJsonObject,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes)
{
	FBlueprintGenerateResult Result;
	Result.Message = TEXT("生成失败。");

	if (!TargetGraph || !GraphJsonObject.IsValid())
	{
		Result.Message = TEXT("目标图表或 JSON 对象无效。");
		return Result;
	}

	// 解析本地变量声明
	TArray<FParsedLocalVariableDeclaration> ParsedLocalVariableDeclarations;
	TArray<FBlueprintGeneratorDiagnostic> DefaultValueDiagnostics;
	TArray<FBlueprintGeneratorDiagnostic> PinTypeDiagnostics;
	TArray<FBlueprintGeneratorDiagnostic> ConnectionDiagnostics;
	ResolveLocalVariableDeclarations(GraphJsonObject, ParsedLocalVariableDeclarations);

	// 解析节点
	TArray<FParsedNode> ParsedNodes;
	const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
	if (GraphJsonObject->TryGetArrayField(TEXT("nodes"), NodesArray) && NodesArray)
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
			ParsedNode.SourceType = FTextToBlueprintGeneratorLocalUtils::NormalizeNodeTypeName(ParsedNode.SourceType);
			ParsedNode.NodeType = ResolveNodeType(NodeObject);
			ParsedNode.FunctionName = ResolveNodeFunctionName(NodeObject);
			ParsedNode.X = NodeObject->HasField(TEXT("x")) ? static_cast<float>(NodeObject->GetNumberField(TEXT("x"))) : 0.0f;
			ParsedNode.Y = NodeObject->HasField(TEXT("y")) ? static_cast<float>(NodeObject->GetNumberField(TEXT("y"))) : 0.0f;
			ParsedNode.VariableReference = ResolveVariableReference(NodeObject);
			ParsedNode.MacroReference = ResolveMacroReference(NodeObject);
			ParsedNode.EventReference = ResolveEventReference(NodeObject);
			ParsedNode.DelegateReference = ResolveDelegateReference(NodeObject);
			ParsedNode.ContainerReference = ResolveContainerReference(NodeObject);
			ParsedNode.StructReference = ResolveStructReference(NodeObject);
			ParsedNode.CastReference = ResolveCastReference(NodeObject);
			ParsedNode.SpawnReference = ResolveSpawnReference(NodeObject);
			ParsedNode.FormatTextReference = ResolveFormatTextReference(NodeObject);
			ParsedNode.TimelineReference = ResolveTimelineReference(NodeObject);
			ParsedNode.LiteralReference = ResolveLiteralReference(NodeObject);
			ParsedNode.ComponentBoundEventReference = ResolveComponentBoundEventReference(NodeObject);
			ParsedNode.CommentReference = ResolveCommentReference(NodeObject);
			ParsedNode.EnhancedInputActionReference = ResolveEnhancedInputActionReference(NodeObject);
			ParsedNode.SwitchReference = ResolveSwitchReference(NodeObject);
			ParsedNode.SelectReference = ResolveSelectReference(NodeObject);

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
					ParsedNode.DefaultValues.Add(Pair.Key, ConvertJsonValueToString(Pair.Value));
				}
			}

			const TSharedPtr<FJsonObject>* DefaultValuesObject = nullptr;
			if (NodeObject->TryGetObjectField(TEXT("default_values"), DefaultValuesObject) && DefaultValuesObject && DefaultValuesObject->IsValid())
			{
				for (const auto& Pair : (*DefaultValuesObject)->Values)
				{
					ParsedNode.DefaultValues.FindOrAdd(Pair.Key) = ConvertJsonValueToString(Pair.Value);
				}
			}

			ParsedNodes.Add(ParsedNode);
		}
	}

	// 解析连线
	TArray<FParsedLink> ParsedLinks;
	const TArray<TSharedPtr<FJsonValue>>* LinksArray = nullptr;
	if (GraphJsonObject->TryGetArrayField(TEXT("links"), LinksArray) && LinksArray)
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

	int32 RequestedDefaultValueCount = 0;
	int32 RequestedPinTypeCount = 0;
	for (const FParsedLocalVariableDeclaration& Declaration : ParsedLocalVariableDeclarations)
	{
		if (Declaration.PinType.IsValid())
		{
			++RequestedPinTypeCount;
		}
	}
	for (const FParsedNode& ParsedNode : ParsedNodes)
	{
		RequestedDefaultValueCount += ParsedNode.DefaultValues.Num();
		RequestedPinTypeCount += FTextToBlueprintGeneratorLocalUtils::CountRequestedPinTypes(ParsedNode);
	}

	if (ParsedNodes.Num() == 0)
	{
		Result.bSucceed = true;
		Result.Message = TEXT("图表无节点数据，跳过。");
		Result.RequestedDefaultValueCount = RequestedDefaultValueCount;
		Result.RequestedPinTypeCount = RequestedPinTypeCount;
		Result.ResolvedPinTypeCount = RequestedPinTypeCount;
		Result.RequestedConnectionCount = ParsedLinks.Num();
		return Result;
	}

	const FScopedTransaction Transaction(FText::FromString(TEXT("Generate Graph Nodes from JSON")));
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
			if (FTextToBlueprintGeneratorLocalUtils::IsInvalidPinTypeFailure(EnsureErrorMessage))
			{
				PinTypeDiagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
					TEXT("invalid_pin_type"),
					Declaration.Name,
					Declaration.Name,
					EnsureErrorMessage));
			}

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
		// v2.9 — 跳过虚拟入口/结果节点（导出不生成它们，但 AI 可能手动写入；导入时从图表中自动发现）
		if (ParsedNode.Id == TEXT("__function_entry__") || ParsedNode.Id == TEXT("__function_result__")
			|| ParsedNode.SourceType.Equals(TEXT("K2Node_FunctionEntry"), ESearchCase::IgnoreCase)
			|| ParsedNode.SourceType.Equals(TEXT("K2Node_FunctionResult"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		// v2.3 — Comment 节点特殊处理（UEdGraphNode_Comment 不是 UK2Node）
		if (ParsedNode.NodeType == EParsedBlueprintNodeType::Comment)
		{
			UEdGraphNode_Comment* CommentNode = NewObject<UEdGraphNode_Comment>(TargetGraph);
			TargetGraph->AddNode(CommentNode, true, false);
			CommentNode->CreateNewGuid();
			CommentNode->NodePosX = static_cast<int32>(ParsedNode.X);
			CommentNode->NodePosY = static_cast<int32>(ParsedNode.Y);
			CommentNode->NodeComment = ParsedNode.CommentReference.CommentText;
			CommentNode->FontSize = ParsedNode.CommentReference.FontSize;
			CommentNode->NodeWidth = static_cast<int32>(ParsedNode.CommentReference.Width);
			CommentNode->NodeHeight = static_cast<int32>(ParsedNode.CommentReference.Height);
			if (!ParsedNode.CommentReference.CommentColor.IsEmpty())
			{
				FLinearColor Color;
				if (Color.InitFromString(ParsedNode.CommentReference.CommentColor))
				{
					CommentNode->CommentColor = Color;
				}
			}
			++GeneratedNodeCount;
			continue;
		}

		UK2Node* SpawnedNode = nullptr;
		FString SpawnErrorMessage;

		IBlueprintNodeHandler* Handler = FBlueprintNodeHandlerRegistry::Get().FindHandler(ParsedNode.NodeType);
		if (Handler)
		{
			SpawnedNode = Handler->Spawn(TargetGraph, ParsedNode, SpawnErrorMessage);
		}
		else
		{
			SpawnErrorMessage = ParsedNode.SourceType.IsEmpty()
				? TEXT("未识别的节点类型，且缺少可用的函数/变量/宏描述。")
				: FString::Printf(TEXT("未识别的节点类型：%s"), *ParsedNode.SourceType);
		}

		if (SpawnedNode)
		{
			IdToSpawnedNode.Add(ParsedNode.Id, SpawnedNode);
			++GeneratedNodeCount;
			continue;
		}

		if (FTextToBlueprintGeneratorLocalUtils::IsInvalidPinTypeFailure(SpawnErrorMessage))
		{
			PinTypeDiagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
				TEXT("invalid_pin_type"),
				ParsedNode.Id,
				FTextToBlueprintGeneratorLocalUtils::FindDiagnosticPinName(ParsedNode, SpawnErrorMessage),
				SpawnErrorMessage));
		}

		TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
		UnresolvedItem->NodeData = ParsedNode;
		UnresolvedItem->DisplayText = ParsedNode.FunctionName.IsEmpty()
			? FString::Printf(TEXT("%s (%s)"), *ParsedNode.SourceType, *ParsedNode.Id)
			: FString::Printf(TEXT("%s (%s)"), *ParsedNode.FunctionName, *ParsedNode.Id);
		UnresolvedItem->Reason = SpawnErrorMessage.IsEmpty() ? TEXT("不支持的节点类型或配置不完整。") : SpawnErrorMessage;
		OutUnresolvedNodes.Add(UnresolvedItem);
	}

	// 将图中已有的 FunctionEntry / FunctionResult 注入 ID 映射，以便连线恢复
	for (UEdGraphNode* ExistingNode : TargetGraph->Nodes)
	{
		if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(ExistingNode))
		{
			IdToSpawnedNode.FindOrAdd(TEXT("__function_entry__"), Entry);
		}
		else if (UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(ExistingNode))
		{
			IdToSpawnedNode.FindOrAdd(TEXT("__function_result__"), ResultNode);
		}
	}

	// v2.9 — existing_node_refs：允许增量导入引用图中已有节点
	const TArray<TSharedPtr<FJsonValue>>* ExistingRefsArray = nullptr;
	if (GraphJsonObject->TryGetArrayField(TEXT("existing_node_refs"), ExistingRefsArray) && ExistingRefsArray)
	{
		for (const TSharedPtr<FJsonValue>& RefValue : *ExistingRefsArray)
		{
			const TSharedPtr<FJsonObject> RefObject = RefValue->AsObject();
			if (!RefObject.IsValid()) continue;

			FString RefId;
			RefObject->TryGetStringField(TEXT("id"), RefId);
			if (RefId.IsEmpty()) continue;

			FString MatchTitle;
			RefObject->TryGetStringField(TEXT("node_title"), MatchTitle);
			FString MatchGuid;
			RefObject->TryGetStringField(TEXT("node_guid"), MatchGuid);

			for (UEdGraphNode* RefCandidate : TargetGraph->Nodes)
			{
				UK2Node* K2Existing = Cast<UK2Node>(RefCandidate);
				if (!K2Existing) continue;
				if (IdToSpawnedNode.FindKey(K2Existing)) continue; // 已经被映射

				bool bMatched = false;
				if (!MatchGuid.IsEmpty())
				{
					bMatched = RefCandidate->NodeGuid.ToString(EGuidFormats::Digits) == MatchGuid;
				}
				else if (!MatchTitle.IsEmpty())
				{
					const FString Title = RefCandidate->GetNodeTitle(ENodeTitleType::ListView).ToString();
					bMatched = Title.Contains(MatchTitle);
				}

				if (bMatched)
				{
					IdToSpawnedNode.Add(RefId, K2Existing);
					break;
				}
			}
		}
	}

	// v2.9 — 先 Reconstruct 新生成的节点以确保引脚完整，再连线（避免连线后 Reconstruct 破坏连接）
	for (const auto& Pair : IdToSpawnedNode)
	{
		if (Pair.Value)
		{
			TargetGraph->GetSchema()->ReconstructNode(*Pair.Value);
		}
	}

	int32 AppliedDefaultValueCount = 0;
	for (const FParsedNode& ParsedNode : ParsedNodes)
	{
		UK2Node** SpawnedNodePtr = IdToSpawnedNode.Find(ParsedNode.Id);
		if (!SpawnedNodePtr || !*SpawnedNodePtr)
		{
			continue;
		}

		TArray<FBlueprintGeneratorDiagnostic> NodeDiagnostics = ApplyDefaultValues(*SpawnedNodePtr, ParsedNode.DefaultValues, ParsedNode.Id);
		AppliedDefaultValueCount += FMath::Max(0, ParsedNode.DefaultValues.Num() - NodeDiagnostics.Num());
		DefaultValueDiagnostics.Append(NodeDiagnostics);
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	int32 CreatedConnectionCount = 0;
	for (const FParsedLink& ParsedLink : ParsedLinks)
	{
		if (!Schema)
		{
			ConnectionDiagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
				TEXT("link_connection_rejected"),
				ParsedLink.FromId,
				ParsedLink.FromPin,
				TEXT("连线创建失败：K2 Schema 无效。")));
			continue;
		}

		UK2Node** FromNodePtr = IdToSpawnedNode.Find(ParsedLink.FromId);
		UK2Node** ToNodePtr = IdToSpawnedNode.Find(ParsedLink.ToId);
		if (!FromNodePtr || !*FromNodePtr)
		{
			ConnectionDiagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
				TEXT("link_node_not_found"),
				ParsedLink.FromId,
				ParsedLink.FromPin,
				FString::Printf(TEXT("连线来源节点未找到：%s。"), *ParsedLink.FromId)));
			continue;
		}
		if (!ToNodePtr || !*ToNodePtr)
		{
			ConnectionDiagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
				TEXT("link_node_not_found"),
				ParsedLink.ToId,
				ParsedLink.ToPin,
				FString::Printf(TEXT("连线目标节点未找到：%s。"), *ParsedLink.ToId)));
			continue;
		}

		UK2Node* FromNode = *FromNodePtr;
		UK2Node* ToNode = *ToNodePtr;
		UEdGraphPin* FromPin = FindPinByAlias(FromNode, ParsedLink.FromPin);
		UEdGraphPin* ToPin = FindPinByAlias(ToNode, ParsedLink.ToPin);
		if (!FromPin)
		{
			ConnectionDiagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
				TEXT("link_pin_not_found"),
				ParsedLink.FromId,
				ParsedLink.FromPin,
				FString::Printf(TEXT("连线来源引脚未找到：%s.%s。"), *ParsedLink.FromId, *ParsedLink.FromPin)));
			continue;
		}
		if (!ToPin)
		{
			ConnectionDiagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
				TEXT("link_pin_not_found"),
				ParsedLink.ToId,
				ParsedLink.ToPin,
				FString::Printf(TEXT("连线目标引脚未找到：%s.%s。"), *ParsedLink.ToId, *ParsedLink.ToPin)));
			continue;
		}

		const FPinConnectionResponse ConnectionResponse = Schema->CanCreateConnection(FromPin, ToPin);
		if (Schema->TryCreateConnection(FromPin, ToPin))
		{
			++CreatedConnectionCount;
		}
		else
		{
			ConnectionDiagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
				TEXT("link_connection_rejected"),
				ParsedLink.FromId,
				ParsedLink.FromPin,
				ConnectionResponse.Message.IsEmpty()
					? FString::Printf(TEXT("Schema 拒绝连线：%s.%s -> %s.%s。"),
						*ParsedLink.FromId, *ParsedLink.FromPin, *ParsedLink.ToId, *ParsedLink.ToPin)
					: ConnectionResponse.Message.ToString()));
		}
	}

	TargetGraph->NotifyGraphChanged();

	Result.bSucceed = GeneratedNodeCount > 0 || CreatedConnectionCount > 0;
	Result.GeneratedNodeCount = GeneratedNodeCount;
	Result.RequestedDefaultValueCount = RequestedDefaultValueCount;
	Result.AppliedDefaultValueCount = AppliedDefaultValueCount;
	Result.DefaultValueDiagnostics = MoveTemp(DefaultValueDiagnostics);
	Result.RequestedPinTypeCount = RequestedPinTypeCount;
	Result.ResolvedPinTypeCount = FMath::Max(0, RequestedPinTypeCount - PinTypeDiagnostics.Num());
	Result.PinTypeDiagnostics = MoveTemp(PinTypeDiagnostics);
	Result.RequestedConnectionCount = ParsedLinks.Num();
	Result.CreatedConnectionCount = CreatedConnectionCount;
	Result.ConnectionDiagnostics = MoveTemp(ConnectionDiagnostics);
	Result.UnresolvedNodeCount = OutUnresolvedNodes.Num();
	if (Result.bSucceed)
	{
		Result.Message = FString::Printf(TEXT("生成完成：成功 %d 个节点，建立 %d 条连线，未匹配 %d 个。"), Result.GeneratedNodeCount, CreatedConnectionCount, Result.UnresolvedNodeCount);
	}
	return Result;
}

FBlueprintGenerateResult TextToBlueprintGenerator::GenerateMultiGraphFromJson(
	UBlueprint* Blueprint, const FString& JsonString,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes)
{
	FBlueprintGenerateResult Result;
	Result.Message = TEXT("生成失败。");

	if (!Blueprint)
	{
		Result.Message = TEXT("蓝图对象无效。");
		return Result;
	}

	OutUnresolvedNodes.Empty();
	const FString TrimmedJsonString = JsonString.TrimStartAndEnd();
	if (TrimmedJsonString.IsEmpty() || !TrimmedJsonString.StartsWith(TEXT("{")))
	{
		Result.Message = TEXT("JSON 文本无效。");
		return Result;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(TrimmedJsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		Result.Message = FString::Printf(TEXT("JSON 解析失败：%s"), *Reader->GetErrorMessage());
		return Result;
	}

	// === 蓝图级操作 ===
	const TArray<TSharedPtr<FJsonValue>>* BlueprintOpsArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("blueprint_operations"), BlueprintOpsArray) && BlueprintOpsArray && BlueprintOpsArray->Num() > 0)
	{
		for (const TSharedPtr<FJsonValue>& OpValue : *BlueprintOpsArray)
		{
			const TSharedPtr<FJsonObject> OpObject = OpValue->AsObject();
			if (!OpObject.IsValid())
			{
				continue;
			}

			FString OpName;
			OpObject->TryGetStringField(TEXT("op"), OpName);
			if (OpName.IsEmpty())
			{
				continue;
			}

			IBlueprintOperationHandler* OpHandler = FBlueprintOperationHandlerRegistry::Get().FindHandler(OpName);
			if (!OpHandler)
			{
				TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
				UnresolvedItem->DisplayText = FString::Printf(TEXT("BlueprintOp: %s"), *OpName);
				UnresolvedItem->Reason = FString::Printf(TEXT("未识别的蓝图级操作：%s"), *OpName);
				OutUnresolvedNodes.Add(UnresolvedItem);
				continue;
			}

			FString OpError;
			if (!OpHandler->Execute(Blueprint, OpObject, OpError))
			{
				TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
				UnresolvedItem->DisplayText = FString::Printf(TEXT("BlueprintOp: %s"), *OpName);
				UnresolvedItem->Reason = OpError;
				OutUnresolvedNodes.Add(UnresolvedItem);
			}
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	// === 多图模式（graphs 数组） ===
	int32 TotalGenerated = 0;
	int32 TotalRequestedDefaultValues = 0;
	int32 TotalAppliedDefaultValues = 0;
	int32 TotalRequestedPinTypes = 0;
	int32 TotalResolvedPinTypes = 0;
	int32 TotalRequestedConnections = 0;
	int32 TotalCreatedConnections = 0;
	const TArray<TSharedPtr<FJsonValue>>* GraphsArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("graphs"), GraphsArray) && GraphsArray && GraphsArray->Num() > 0)
	{
		for (const TSharedPtr<FJsonValue>& GraphValue : *GraphsArray)
		{
			const TSharedPtr<FJsonObject> GraphObject = GraphValue->AsObject();
			if (!GraphObject.IsValid())
			{
				continue;
			}

			FString GraphName;
			GraphObject->TryGetStringField(TEXT("graph"), GraphName);
			if (GraphName.IsEmpty())
			{
				continue;
			}

			UEdGraph* TargetGraph = FindGraphByName(Blueprint, GraphName);
			if (!TargetGraph)
			{
				TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
				UnresolvedItem->DisplayText = FString::Printf(TEXT("Graph: %s"), *GraphName);
				UnresolvedItem->Reason = FString::Printf(TEXT("未找到名为 '%s' 的图表，请先通过 blueprint_operations 创建。"), *GraphName);
				OutUnresolvedNodes.Add(UnresolvedItem);
				continue;
			}

			FBlueprintGenerateResult GraphResult = GenerateNodesAndLinksForGraph(TargetGraph, GraphObject, OutUnresolvedNodes);
			TotalGenerated += GraphResult.GeneratedNodeCount;
			TotalRequestedDefaultValues += GraphResult.RequestedDefaultValueCount;
			TotalAppliedDefaultValues += GraphResult.AppliedDefaultValueCount;
			Result.DefaultValueDiagnostics.Append(GraphResult.DefaultValueDiagnostics);
			TotalRequestedPinTypes += GraphResult.RequestedPinTypeCount;
			TotalResolvedPinTypes += GraphResult.ResolvedPinTypeCount;
			Result.PinTypeDiagnostics.Append(GraphResult.PinTypeDiagnostics);
			TotalRequestedConnections += GraphResult.RequestedConnectionCount;
			TotalCreatedConnections += GraphResult.CreatedConnectionCount;
			Result.ConnectionDiagnostics.Append(GraphResult.ConnectionDiagnostics);
		}
	}
	else if (JsonObject->HasField(TEXT("nodes")))
	{
		// 单图回退：使用默认 EventGraph
		UEdGraph* DefaultGraph = FindGraphByName(Blueprint, TEXT("EventGraph"));
		if (!DefaultGraph)
		{
			Result.Message = TEXT("蓝图中未找到 EventGraph。");
			return Result;
		}

		FBlueprintGenerateResult GraphResult = GenerateNodesAndLinksForGraph(DefaultGraph, JsonObject, OutUnresolvedNodes);
		TotalGenerated = GraphResult.GeneratedNodeCount;
		TotalRequestedDefaultValues = GraphResult.RequestedDefaultValueCount;
		TotalAppliedDefaultValues = GraphResult.AppliedDefaultValueCount;
		Result.DefaultValueDiagnostics = MoveTemp(GraphResult.DefaultValueDiagnostics);
		TotalRequestedPinTypes = GraphResult.RequestedPinTypeCount;
		TotalResolvedPinTypes = GraphResult.ResolvedPinTypeCount;
		Result.PinTypeDiagnostics = MoveTemp(GraphResult.PinTypeDiagnostics);
		TotalRequestedConnections = GraphResult.RequestedConnectionCount;
		TotalCreatedConnections = GraphResult.CreatedConnectionCount;
		Result.ConnectionDiagnostics = MoveTemp(GraphResult.ConnectionDiagnostics);
	}

	Result.bSucceed = TotalGenerated > 0;
	Result.GeneratedNodeCount = TotalGenerated;
	Result.RequestedDefaultValueCount = TotalRequestedDefaultValues;
	Result.AppliedDefaultValueCount = TotalAppliedDefaultValues;
	Result.RequestedPinTypeCount = TotalRequestedPinTypes;
	Result.ResolvedPinTypeCount = TotalResolvedPinTypes;
	Result.RequestedConnectionCount = TotalRequestedConnections;
	Result.CreatedConnectionCount = TotalCreatedConnections;
	Result.UnresolvedNodeCount = OutUnresolvedNodes.Num();
	if (Result.bSucceed)
	{
		Result.Message = FString::Printf(TEXT("多图生成完成：成功 %d 个节点，未匹配 %d 个。"), Result.GeneratedNodeCount, Result.UnresolvedNodeCount);
	}
	else if (Result.UnresolvedNodeCount > 0)
	{
		Result.Message = FString::Printf(TEXT("未生成任何节点：共有 %d 个未匹配项。"), Result.UnresolvedNodeCount);
	}
	else
	{
		Result.Message = TEXT("未生成任何节点。");
	}
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
		OutErrorMessage = TEXT("宏节点生成失败：缺少 macro.name，标准宏请提供真实宏名，例如 ForLoop。");
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
	TargetGraph->GetSchema()->ReconstructNode(*MacroNode);
	ApplyDefaultValues(MacroNode, NodeData.DefaultValues, NodeData.Id);
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
	TargetGraph->GetSchema()->ReconstructNode(*CallFunctionNode);
	ApplyDefaultValues(CallFunctionNode, NodeData.DefaultValues, NodeData.Id);
	return CallFunctionNode;
}

TArray<FBlueprintGeneratorDiagnostic> TextToBlueprintGenerator::ApplyDefaultValues(
	UK2Node* TargetNode,
	const TMap<FString, FString>& DefaultValues,
	const FString& NodeId)
{
	TArray<FBlueprintGeneratorDiagnostic> Diagnostics;
	if (!TargetNode)
	{
		for (const auto& Pair : DefaultValues)
		{
			Diagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
				TEXT("default_pin_not_found"),
				NodeId,
				Pair.Key,
				FString::Printf(TEXT("默认值 '%s' 无法应用：目标节点无效。"), *Pair.Key)));
		}
		return Diagnostics;
	}

	for (const auto& Pair : DefaultValues)
	{
		UEdGraphPin* Pin = FindPinByAlias(TargetNode, Pair.Key);
		if (!Pin)
		{
			Diagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
				TEXT("default_pin_not_found"),
				NodeId,
				Pair.Key,
				FString::Printf(TEXT("默认值引脚未找到：%s。"), *Pair.Key)));
			continue;
		}

		FString DiagnosticCode;
		FString DiagnosticMessage;
		if (!ApplyPinDefaultValue(Pin, Pair.Value, DiagnosticCode, DiagnosticMessage))
		{
			Diagnostics.Add(FTextToBlueprintGeneratorLocalUtils::MakeGeneratorDiagnostic(
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
