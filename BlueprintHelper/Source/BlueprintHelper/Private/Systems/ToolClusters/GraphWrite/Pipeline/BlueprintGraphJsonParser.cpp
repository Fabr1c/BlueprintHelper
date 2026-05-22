#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.h"

#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphExistingNodeMapper.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLinker.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLocalVariableService.h"
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

EParsedBlueprintNodeType FBlueprintGraphJsonParser::ResolveNodeType(const TSharedPtr<FJsonObject>& NodeObject)
{
	if (!NodeObject.IsValid())
	{
		return EParsedBlueprintNodeType::Unknown;
	}

	FString NodeTypeString;
	NodeObject->TryGetStringField(TEXT("type"), NodeTypeString);
	if (NodeTypeString.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("kind"), NodeTypeString);
	}
	const FString NormalizedNodeType = FBlueprintGraphNodeUtility::NormalizeNodeTypeName(NodeTypeString);

	if (NormalizedNodeType.Equals(TEXT("K2Node_CallFunction"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("CallFunction"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("Call"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::CallFunction;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_VariableGet"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("VariableGet"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("Get"), ESearchCase::IgnoreCase))
	{
		return EParsedBlueprintNodeType::VariableGet;
	}

	if (NormalizedNodeType.Equals(TEXT("K2Node_VariableSet"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("VariableSet"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("Set"), ESearchCase::IgnoreCase))
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
		|| NormalizedNodeType.Equals(TEXT("CustomEvent"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("Custom_Event"), ESearchCase::IgnoreCase))
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
		|| NormalizedNodeType.Equals(TEXT("MakeStruct"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("Make_Struct"), ESearchCase::IgnoreCase))
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
		|| NormalizedNodeType.Equals(TEXT("PromotableOperator"), ESearchCase::IgnoreCase)
		|| NormalizedNodeType.Equals(TEXT("Compare"), ESearchCase::IgnoreCase))
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

FString FBlueprintGraphJsonParser::ConvertJsonValueToString(const TSharedPtr<FJsonValue>& JsonValue)
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
			CombinedValue += FBlueprintGraphJsonParser::ConvertJsonValueToString(ArrayValues[ValueIndex]);
		}
		return CombinedValue;
	}

	default:
		return TEXT("");
	}
}

FString FBlueprintGraphJsonParser::ResolveNodeFunctionName(const TSharedPtr<FJsonObject>& NodeObject)
{
	if (!NodeObject.IsValid())
	{
		return TEXT("");
	}

	FString FunctionName;
	if (NodeObject->TryGetStringField(TEXT("function"), FunctionName) && !FunctionName.IsEmpty())
	{
		return FunctionName;
	}

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

FParsedPinType FBlueprintGraphJsonParser::ResolvePinType(const TSharedPtr<FJsonObject>& PinTypeObject)
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

FParsedVariableReference FBlueprintGraphJsonParser::ResolveVariableReference(const TSharedPtr<FJsonObject>& NodeObject)
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
			Result.PinType = FBlueprintGraphJsonParser::ResolvePinType(*PinTypeObject);
		}
	}

	if (Result.VariableName.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("var"), Result.VariableName);
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

FParsedMacroReference FBlueprintGraphJsonParser::ResolveMacroReference(const TSharedPtr<FJsonObject>& NodeObject)
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

	if (Result.MacroName.IsEmpty() || FBlueprintGraphNodeUtility::IsPlaceholderMacroName(Result.MacroName))
	{
		NodeObject->TryGetStringField(TEXT("function_name"), Result.MacroName);
	}

	if (FBlueprintGraphNodeUtility::IsPlaceholderMacroName(Result.MacroName))
	{
		Result.MacroName.Reset();
	}

	if (Result.LibraryType.IsEmpty())
	{
		Result.LibraryType = TEXT("standard");
	}

	return Result;
}

FParsedEventReference FBlueprintGraphJsonParser::ResolveEventReference(const TSharedPtr<FJsonObject>& NodeObject)
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
					Param.PinType = FBlueprintGraphJsonParser::ResolvePinType(*PinTypeObject);
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

FParsedDelegateReference FBlueprintGraphJsonParser::ResolveDelegateReference(const TSharedPtr<FJsonObject>& NodeObject)
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

FParsedContainerReference FBlueprintGraphJsonParser::ResolveContainerReference(const TSharedPtr<FJsonObject>& NodeObject)
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
			Result.ElementType = FBlueprintGraphJsonParser::ResolvePinType(*ElementTypeObject);
		}

		const TSharedPtr<FJsonObject>* KeyTypeObject = nullptr;
		if ((*ContainerObject)->TryGetObjectField(TEXT("key_type"), KeyTypeObject) && KeyTypeObject && KeyTypeObject->IsValid())
		{
			Result.KeyType = FBlueprintGraphJsonParser::ResolvePinType(*KeyTypeObject);
		}

		const TSharedPtr<FJsonObject>* ValueTypeObject = nullptr;
		if ((*ContainerObject)->TryGetObjectField(TEXT("value_type"), ValueTypeObject) && ValueTypeObject && ValueTypeObject->IsValid())
		{
			Result.ValueType = FBlueprintGraphJsonParser::ResolvePinType(*ValueTypeObject);
		}
	}

	return Result;
}

FParsedStructReference FBlueprintGraphJsonParser::ResolveStructReference(const TSharedPtr<FJsonObject>& NodeObject)
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

	if (Result.StructPath.IsEmpty())
	{
		NodeObject->TryGetStringField(TEXT("type"), Result.StructPath);
	}

	return Result;
}

FParsedCastReference FBlueprintGraphJsonParser::ResolveCastReference(const TSharedPtr<FJsonObject>& NodeObject)
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

FParsedSpawnReference FBlueprintGraphJsonParser::ResolveSpawnReference(const TSharedPtr<FJsonObject>& NodeObject)
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

FParsedFormatTextReference FBlueprintGraphJsonParser::ResolveFormatTextReference(const TSharedPtr<FJsonObject>& NodeObject)
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

FParsedLiteralReference FBlueprintGraphJsonParser::ResolveLiteralReference(const TSharedPtr<FJsonObject>& NodeObject)
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

FParsedComponentBoundEventReference FBlueprintGraphJsonParser::ResolveComponentBoundEventReference(const TSharedPtr<FJsonObject>& NodeObject)
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

FParsedCommentReference FBlueprintGraphJsonParser::ResolveCommentReference(const TSharedPtr<FJsonObject>& NodeObject)
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

FParsedEnhancedInputActionReference FBlueprintGraphJsonParser::ResolveEnhancedInputActionReference(const TSharedPtr<FJsonObject>& NodeObject)
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

FParsedSwitchReference FBlueprintGraphJsonParser::ResolveSwitchReference(const TSharedPtr<FJsonObject>& NodeObject)
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

FParsedSelectReference FBlueprintGraphJsonParser::ResolveSelectReference(const TSharedPtr<FJsonObject>& NodeObject)
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

void FBlueprintGraphJsonParser::ResolveLocalVariableDeclarations(const TSharedPtr<FJsonObject>& JsonObject, TArray<FParsedLocalVariableDeclaration>& OutDeclarations)
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
			Declaration.PinType = FBlueprintGraphJsonParser::ResolvePinType(*PinTypeObject);
		}

		if (!Declaration.Name.IsEmpty())
		{
			OutDeclarations.Add(Declaration);
		}
	}
}
