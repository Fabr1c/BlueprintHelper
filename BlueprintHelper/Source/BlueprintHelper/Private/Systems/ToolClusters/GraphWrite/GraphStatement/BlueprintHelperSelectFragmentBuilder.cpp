#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSelectFragmentBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Select.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementTypeUtils.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

namespace
{
static void AddPinAlias(
	TMap<FString, FBlueprintHelperFragmentPinRef>& PinMap,
	const FString& Alias,
	const FBlueprintHelperFragmentPinRef& PinRef)
{
	if (Alias.IsEmpty() || !PinRef.Pin)
	{
		return;
	}

	PinMap.Add(Alias, PinRef);
	const FString LowerAlias = Alias.ToLower();
	if (!PinMap.Contains(LowerAlias))
	{
		PinMap.Add(LowerAlias, FBlueprintHelperFragmentPinRef{ PinRef.NodeId, LowerAlias, PinRef.Type, PinRef.Pin });
	}
}

static bool TryBuildSelectPinType(const FString& TypeName, FEdGraphPinType& OutPinType)
{
	FString Raw = TypeName.TrimStartAndEnd();
	const FBlueprintHelperCallFunctionPinType ParsedPinType =
		FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(Raw);
	FString Category = ParsedPinType.Category.TrimStartAndEnd();
	FString ObjectPath = ParsedPinType.ObjectPath.TrimStartAndEnd();
	if (ObjectPath.IsEmpty() && !ParsedPinType.SubCategory.TrimStartAndEnd().IsEmpty())
	{
		ObjectPath = ParsedPinType.SubCategory.TrimStartAndEnd();
	}

	const FString Prefixes[] = {
		TEXT("object"),
		TEXT("class"),
		TEXT("soft_object"),
		TEXT("softobject"),
		TEXT("soft_class"),
		TEXT("softclass"),
		TEXT("interface"),
		TEXT("struct"),
		TEXT("enum")
	};
	for (const FString& Prefix : Prefixes)
	{
		if (Raw.StartsWith(Prefix + TEXT(":"), ESearchCase::IgnoreCase))
		{
			Category = Prefix;
			ObjectPath = Raw.Mid(Prefix.Len() + 1).TrimStartAndEnd();
			break;
		}
	}

	const FString Normalized = Raw.ToLower();
	if (Normalized.IsEmpty())
	{
		return false;
	}

	if (Normalized == TEXT("bool") || Normalized == TEXT("boolean"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		return true;
	}
	if (Normalized == TEXT("int") || Normalized == TEXT("integer"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		return true;
	}
	if (Normalized == TEXT("int64") || Normalized == TEXT("long"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
		return true;
	}
	if (Normalized == TEXT("float"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
		return true;
	}
	if (Normalized == TEXT("double") || Normalized == TEXT("real") || Normalized == TEXT("number"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		return true;
	}
	if (Normalized == TEXT("string"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
		return true;
	}
	if (Normalized == TEXT("name"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
		return true;
	}
	if (Normalized == TEXT("text"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
		return true;
	}

	auto ResolveTypeObject = [](const FString& Path) -> UObject*
	{
		const FString CleanPath = Path.TrimStartAndEnd();
		if (CleanPath.IsEmpty())
		{
			return nullptr;
		}
		if (UObject* Existing = FindObject<UObject>(nullptr, *CleanPath))
		{
			return Existing;
		}
		return LoadObject<UObject>(nullptr, *CleanPath);
	};

	UObject* TypeObject = ResolveTypeObject(ObjectPath);
	if (!TypeObject && Raw.StartsWith(TEXT("/"), ESearchCase::CaseSensitive))
	{
		TypeObject = ResolveTypeObject(Raw);
		if (TypeObject)
		{
			if (TypeObject->IsA<UEnum>())
			{
				Category = TEXT("enum");
			}
			else if (TypeObject->IsA<UScriptStruct>())
			{
				Category = TEXT("struct");
			}
			else if (TypeObject->IsA<UClass>())
			{
				Category = TEXT("object");
			}
		}
	}

	const FString EffectiveCategory = Category.TrimStartAndEnd().ToLower();
	if (EffectiveCategory == TEXT("object"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
		OutPinType.PinSubCategoryObject = TypeObject ? TypeObject : UObject::StaticClass();
		return true;
	}
	if (EffectiveCategory == TEXT("class"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Class;
		OutPinType.PinSubCategoryObject = TypeObject ? TypeObject : UObject::StaticClass();
		return true;
	}
	if (EffectiveCategory == TEXT("soft_object") || EffectiveCategory == TEXT("softobject"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
		OutPinType.PinSubCategoryObject = TypeObject ? TypeObject : UObject::StaticClass();
		return true;
	}
	if (EffectiveCategory == TEXT("soft_class") || EffectiveCategory == TEXT("softclass"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
		OutPinType.PinSubCategoryObject = TypeObject ? TypeObject : UObject::StaticClass();
		return true;
	}
	if (EffectiveCategory == TEXT("interface"))
	{
		UClass* InterfaceClass = Cast<UClass>(TypeObject);
		if (!InterfaceClass)
		{
			return false;
		}
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Interface;
		OutPinType.PinSubCategoryObject = InterfaceClass;
		return true;
	}
	if (EffectiveCategory == TEXT("struct"))
	{
		UScriptStruct* StructType = Cast<UScriptStruct>(TypeObject);
		if (!StructType)
		{
			return false;
		}
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = StructType;
		return true;
	}
	if (EffectiveCategory == TEXT("enum"))
	{
		UEnum* EnumType = Cast<UEnum>(TypeObject);
		if (!EnumType)
		{
			return false;
		}
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
		OutPinType.PinSubCategoryObject = EnumType;
		return true;
	}
	return false;
}

static FString GetExpressionEvidenceValue(
	const FBlueprintHelperGraphExpressionIR& Expression,
	const TCHAR* Key)
{
	if (const FString* Value = Expression.ContextEvidence.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

static FString ResolveSelectResultTypeProof(const FBlueprintHelperGraphExpressionIR& Expression)
{
	const FString EvidenceProof = GetExpressionEvidenceValue(Expression, TEXT("generic.select.result_type_proof"));
	if (!EvidenceProof.IsEmpty())
	{
		return EvidenceProof;
	}
	return Expression.Type.TrimStartAndEnd();
}

static bool IsWildcardSelectTypeToken(const FString& TypeName)
{
	const FString Normalized = TypeName.TrimStartAndEnd().ToLower();
	return Normalized == TEXT("wildcard")
		|| Normalized == TEXT("wildcard_pin")
		|| Normalized == TEXT("wildcardpin")
		|| Normalized == TEXT("any")
		|| Normalized == TEXT("unknown");
}

static bool ValidateSelectResultTypeProof(
	const FBlueprintHelperGraphExpressionIR& Expression,
	FEdGraphPinType& OutResultPinType,
	FString& OutError)
{
	const FString ResultTypeProof = ResolveSelectResultTypeProof(Expression);
	if (ResultTypeProof.IsEmpty())
	{
		OutError = TEXT("select_result_type_unresolved: select expression requires generic.select.result_type_proof or a resolved result type.");
		return false;
	}
	if (IsWildcardSelectTypeToken(ResultTypeProof))
	{
		OutError = TEXT("wildcard_residual: select result type proof is still wildcard.");
		return false;
	}
	if (!TryBuildSelectPinType(ResultTypeProof, OutResultPinType))
	{
		OutError = FString::Printf(
			TEXT("select_result_type_unresolved: unsupported select result type proof '%s'."),
			*ResultTypeProof);
		return false;
	}
	return true;
}

static bool TryGetExpressionLiteral(
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
	FString& OutLiteral)
{
	if (!Expression.IsValid() || Expression->Kind != EBlueprintHelperGraphExpressionKind::Literal)
	{
		return false;
	}

	OutLiteral = Expression->LiteralValue;
	return !OutLiteral.IsEmpty();
}

static void ApplyIndexPinType(UK2Node_Select* SelectNode, const FBlueprintHelperGraphExpressionIR& Expression)
{
	if (!SelectNode || !Expression.Condition.IsValid())
	{
		return;
	}

	FEdGraphPinType PinType;
	if (!TryBuildSelectPinType(Expression.Condition->Type, PinType))
	{
		return;
	}

	UEdGraphPin* IndexPin = SelectNode->GetIndexPin();
	if (!IndexPin || !SelectNode->CanChangePinType(IndexPin))
	{
		return;
	}

	IndexPin->PinType = PinType;
	SelectNode->ChangePinType(IndexPin);
}

static void ApplyResultPinType(UK2Node_Select* SelectNode, const FEdGraphPinType& PinType)
{
	if (!SelectNode)
	{
		return;
	}

	UEdGraphPin* ReturnPin = SelectNode->GetReturnValuePin();
	if (!ReturnPin || !SelectNode->CanChangePinType(ReturnPin))
	{
		return;
	}

	ReturnPin->PinType = PinType;
	SelectNode->ChangePinType(ReturnPin);
}

static int32 GetDesiredOptionCount(const FBlueprintHelperGraphExpressionIR& Expression)
{
	if (Expression.ThenValue.IsValid() || Expression.ElseValue.IsValid())
	{
		return 2;
	}
	return FMath::Max(2, Expression.Options.Num());
}

static bool EnsureOptionPinCount(
	UK2Node_Select* SelectNode,
	const int32 DesiredOptionCount,
	FString& OutError)
{
	if (!SelectNode)
	{
		OutError = TEXT("select fragment build failed: select node is invalid.");
		return false;
	}

	TArray<UEdGraphPin*> OptionPins;
	SelectNode->GetOptionPins(OptionPins);
	while (OptionPins.Num() < DesiredOptionCount)
	{
		if (!SelectNode->CanAddPin())
		{
			OutError = FString::Printf(
				TEXT("select fragment build failed: cannot add option pin %d."),
				OptionPins.Num());
			return false;
		}
		SelectNode->AddInputPin();
		OptionPins.Reset();
		SelectNode->GetOptionPins(OptionPins);
	}

	return true;
}

static void CollectLiteralDefaults(
	UK2Node_Select* SelectNode,
	const FBlueprintHelperGraphExpressionIR& Expression,
	TMap<FString, FString>& InOutDefaults)
{
	if (!SelectNode)
	{
		return;
	}

	FString Literal;
	if (TryGetExpressionLiteral(Expression.Condition, Literal))
	{
		InOutDefaults.Add(TEXT("Index"), Literal);
		InOutDefaults.Add(TEXT("condition"), Literal);
	}

	TArray<UEdGraphPin*> OptionPins;
	SelectNode->GetOptionPins(OptionPins);
	if (Expression.ThenValue.IsValid() || Expression.ElseValue.IsValid())
	{
		if (OptionPins.IsValidIndex(0) && TryGetExpressionLiteral(Expression.ElseValue, Literal))
		{
			InOutDefaults.Add(OptionPins[0]->PinName.ToString(), Literal);
			InOutDefaults.Add(TEXT("else"), Literal);
		}
		if (OptionPins.IsValidIndex(1) && TryGetExpressionLiteral(Expression.ThenValue, Literal))
		{
			InOutDefaults.Add(OptionPins[1]->PinName.ToString(), Literal);
			InOutDefaults.Add(TEXT("then"), Literal);
		}
	}
	else
	{
		for (int32 OptionIndex = 0; OptionIndex < Expression.Options.Num() && OptionIndex < OptionPins.Num(); ++OptionIndex)
		{
			if (TryGetExpressionLiteral(Expression.Options[OptionIndex], Literal))
			{
				InOutDefaults.Add(OptionPins[OptionIndex]->PinName.ToString(), Literal);
				InOutDefaults.Add(FString::Printf(TEXT("option_%d"), OptionIndex), Literal);
			}
		}
	}
}

static void PopulateSelectPins(
	UK2Node_Select* SelectNode,
	FBlueprintHelperNodeFragment& OutFragment)
{
	if (!SelectNode)
	{
		return;
	}

	if (UEdGraphPin* IndexPin = SelectNode->GetIndexPin())
	{
		const FString Type = IndexPin->PinType.PinCategory.ToString();
		const FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), IndexPin->PinName.ToString(), Type, IndexPin };
		AddPinAlias(OutFragment.PinBindings, IndexPin->PinName.ToString(), PinRef);
		AddPinAlias(OutFragment.DataInputs, IndexPin->PinName.ToString(), PinRef);
		AddPinAlias(OutFragment.PinBindings, TEXT("condition"), PinRef);
		AddPinAlias(OutFragment.DataInputs, TEXT("condition"), PinRef);
		AddPinAlias(OutFragment.PinBindings, TEXT("index"), PinRef);
		AddPinAlias(OutFragment.DataInputs, TEXT("index"), PinRef);
	}

	TArray<UEdGraphPin*> OptionPins;
	SelectNode->GetOptionPins(OptionPins);
	for (int32 OptionIndex = 0; OptionIndex < OptionPins.Num(); ++OptionIndex)
	{
		UEdGraphPin* OptionPin = OptionPins[OptionIndex];
		if (!OptionPin)
		{
			continue;
		}

		const FString Type = OptionPin->PinType.PinCategory.ToString();
		const FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), OptionPin->PinName.ToString(), Type, OptionPin };
		AddPinAlias(OutFragment.PinBindings, OptionPin->PinName.ToString(), PinRef);
		AddPinAlias(OutFragment.DataInputs, OptionPin->PinName.ToString(), PinRef);
		AddPinAlias(OutFragment.PinBindings, FString::Printf(TEXT("option_%d"), OptionIndex), PinRef);
		AddPinAlias(OutFragment.DataInputs, FString::Printf(TEXT("option_%d"), OptionIndex), PinRef);
	}

	if (OptionPins.IsValidIndex(0))
	{
		const FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), TEXT("else"), OptionPins[0]->PinType.PinCategory.ToString(), OptionPins[0] };
		AddPinAlias(OutFragment.PinBindings, TEXT("else"), PinRef);
		AddPinAlias(OutFragment.DataInputs, TEXT("else"), PinRef);
	}
	if (OptionPins.IsValidIndex(1))
	{
		const FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), TEXT("then"), OptionPins[1]->PinType.PinCategory.ToString(), OptionPins[1] };
		AddPinAlias(OutFragment.PinBindings, TEXT("then"), PinRef);
		AddPinAlias(OutFragment.DataInputs, TEXT("then"), PinRef);
	}

	if (UEdGraphPin* ReturnPin = SelectNode->GetReturnValuePin())
	{
		const FString Type = ReturnPin->PinType.PinCategory.ToString();
		const FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), ReturnPin->PinName.ToString(), Type, ReturnPin };
		AddPinAlias(OutFragment.PinBindings, ReturnPin->PinName.ToString(), PinRef);
		AddPinAlias(OutFragment.DataOutputs, ReturnPin->PinName.ToString(), PinRef);
		AddPinAlias(OutFragment.PinBindings, TEXT("result"), PinRef);
		AddPinAlias(OutFragment.DataOutputs, TEXT("result"), PinRef);
		AddPinAlias(OutFragment.PinBindings, TEXT("value"), PinRef);
		AddPinAlias(OutFragment.DataOutputs, TEXT("value"), PinRef);
		AddPinAlias(OutFragment.PinBindings, TEXT("return"), PinRef);
		AddPinAlias(OutFragment.DataOutputs, TEXT("return"), PinRef);
	}
}
}

bool FBlueprintHelperSelectFragmentBuilder::Build(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphExpressionIR& Expression,
	const FBlueprintHelperActionResolutionResult& ActionResult,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	OutError.Reset();
	if (!TargetGraph)
	{
		OutError = TEXT("select fragment build failed: target graph is invalid.");
		return false;
	}

	const FString ExpressionId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
	FEdGraphPinType ResultPinType;
	if (!ValidateSelectResultTypeProof(Expression, ResultPinType, OutError))
	{
		return false;
	}
	if (!ActionResult.IsResolved())
	{
		OutError = ActionResult.Message.IsEmpty() ? TEXT("select fragment build failed: action provider did not resolve.") : ActionResult.Message;
		return false;
	}
	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = ExpressionId;
	SpawnOptions.NodeConfigurationHook = [&Expression, ResultPinType](UK2Node& SpawnedNode, const FBlueprintHelperActionNodeSpawnContext&, FString& HookError)
	{
		UK2Node_Select* SelectNode = Cast<UK2Node_Select>(&SpawnedNode);
		if (!SelectNode)
		{
			HookError = TEXT("select fragment build failed: spawned node is not UK2Node_Select.");
			return false;
		}

		ApplyIndexPinType(SelectNode, Expression);
		if (!EnsureOptionPinCount(SelectNode, GetDesiredOptionCount(Expression), HookError))
		{
			return false;
		}
		ApplyResultPinType(SelectNode, ResultPinType);
		return true;
	};
	SpawnOptions.DefaultValueProvider = [&Expression](UK2Node& SpawnedNode, const FBlueprintHelperActionNodeSpawnContext&, TMap<FString, FString>& InOutDefaults)
	{
		CollectLiteralDefaults(Cast<UK2Node_Select>(&SpawnedNode), Expression, InOutDefaults);
	};

	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		TargetGraph,
		ActionResult,
		FVector2D::ZeroVector,
		SpawnOptions,
		OutError);
	UK2Node_Select* SelectNode = Cast<UK2Node_Select>(SpawnedNode);
	if (!SelectNode)
	{
		OutError = TEXT("select fragment build failed: UK2Node_Select spawn failed.");
		return false;
	}

	OutFragment.FragmentId = ExpressionId;
	OutFragment.SourceStatementId = Expression.ExpressionId;
	OutFragment.PrimaryNode = SelectNode;
	OutFragment.Nodes.Add(SelectNode);
	PopulateSelectPins(SelectNode, OutFragment);
	OutFragment.OwnershipTags.Add(TEXT("expression_id"), Expression.ExpressionId);
	OutFragment.OwnershipTags.Add(TEXT("semantic_kind"), TEXT("select"));
	OutFragment.ReviewTargets.Add(Expression.ExpressionId);
	return true;
}
