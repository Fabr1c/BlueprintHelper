#include "GraphWrite/BlueprintTextConverter.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_RemoveDelegate.h"
#include "K2Node_ClearDelegate.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeMap.h"
#include "K2Node_MakeSet.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Self.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_FormatText.h"
#include "K2Node_GetArrayItem.h"
#include "K2Node_Timeline.h"
#include "K2Node_Knot.h"
#include "K2Node_Literal.h"
#include "K2Node_GetEnumeratorName.h"
#include "K2Node_GetEnumeratorNameAsString.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_EnhancedInputAction.h"
#include "InputAction.h"
#include "K2Node_PromotableOperator.h"
#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "K2Node_SwitchInteger.h"
#include "K2Node_SwitchString.h"
#include "K2Node_SwitchName.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_Select.h"
#include "EdGraphNode_Comment.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Internationalization/Regex.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	/**
	 * 清洗 T3D 导出的默认值文本，便于内部统一复用。
	 */
	FString NormalizeExportTextValue(const FString& InValue)
	{
		FString Result = InValue.TrimStartAndEnd();
		Result.RemoveFromStart(TEXT("\""));
		Result.RemoveFromEnd(TEXT("\""));
		Result.RemoveFromStart(TEXT("("));
		Result.RemoveFromEnd(TEXT(")"));
		return Result.TrimStartAndEnd();
	}

	/**
	 * 从导出文本中提取对象路径，兼容 Class'/Game/...' 这类格式。
	 */
	FString ExtractObjectPathFromExportValue(const FString& InValue)
	{
		FString Result = NormalizeExportTextValue(InValue);

		const int32 QuoteIndex = Result.Find(TEXT("'"), ESearchCase::IgnoreCase, ESearchDir::FromStart);
		const int32 LastQuoteIndex = Result.Find(TEXT("'"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (QuoteIndex != INDEX_NONE && LastQuoteIndex != INDEX_NONE && LastQuoteIndex > QuoteIndex)
		{
			Result = Result.Mid(QuoteIndex + 1, LastQuoteIndex - QuoteIndex - 1);
		}

		Result.ReplaceInline(TEXT("\""), TEXT(""));
		return Result.TrimStartAndEnd();
	}

	/**
	 * 归一化节点类型名称，统一转成 K2Node_xxx 形式。
	 */
	FString NormalizeNodeTypeName(const FString& InValue)
	{
		FString Result = NormalizeExportTextValue(InValue);
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
	 * 判断节点类型是否匹配指定短名。
	 */
	bool IsNodeTypeMatch(const FString& InNodeType, const TCHAR* ExpectedType)
	{
		return NormalizeNodeTypeName(InNodeType).Equals(ExpectedType, ESearchCase::IgnoreCase);
	}

	/**
	 * 解析正则的第一个捕获结果。
	 */
	FString MatchFirstGroup(const FString& SourceText, const FString& Pattern)
	{
		const FRegexPattern RegexPattern(Pattern);
		FRegexMatcher RegexMatcher(RegexPattern, SourceText);
		if (RegexMatcher.FindNext())
		{
			return RegexMatcher.GetCaptureGroup(1);
		}

		return TEXT("");
	}

	/**
	 * 查找变量节点的代表引脚，用于导出局部变量类型。
	 */
	const FMinimalPin* FindRepresentativeVariablePin(const FMinimalNode& Node)
	{
		auto IsCandidatePin = [](const FMinimalPin& Pin)
		{
			return !Pin.PinName.IsEmpty()
				&& Pin.PinCategory != TEXT("exec")
				&& !Pin.PinName.Equals(TEXT("self"), ESearchCase::IgnoreCase);
		};

		if (IsNodeTypeMatch(Node.NodeType, TEXT("K2Node_VariableGet")))
		{
			for (const FMinimalPin& Pin : Node.Outputs)
			{
				if (IsCandidatePin(Pin))
				{
					return &Pin;
				}
			}
		}

		if (IsNodeTypeMatch(Node.NodeType, TEXT("K2Node_VariableSet")))
		{
			for (const FMinimalPin& Pin : Node.Inputs)
			{
				if (IsCandidatePin(Pin))
				{
					return &Pin;
				}
			}
		}

		for (const FMinimalPin& Pin : Node.Inputs)
		{
			if (IsCandidatePin(Pin))
			{
				return &Pin;
			}
		}

		for (const FMinimalPin& Pin : Node.Outputs)
		{
			if (IsCandidatePin(Pin))
			{
				return &Pin;
			}
		}

		return nullptr;
	}

	/**
	 * 写入轻量引脚类型到 JSON。
	 */
	void WritePinTypeToJson(const FMinimalPin& Pin, const TSharedRef<FJsonObject>& PinTypeObject)
	{
		PinTypeObject->SetStringField(TEXT("category"), Pin.PinCategory);

		if (!Pin.PinSubCategory.IsEmpty())
		{
			PinTypeObject->SetStringField(TEXT("sub_category"), Pin.PinSubCategory);
		}

		if (!Pin.PinSubCategoryObjectPath.IsEmpty())
		{
			PinTypeObject->SetStringField(TEXT("object_path"), Pin.PinSubCategoryObjectPath);
		}

		if (!Pin.ContainerType.IsEmpty())
		{
			PinTypeObject->SetStringField(TEXT("container"), Pin.ContainerType);
		}

		if (Pin.bIsReference)
		{
			PinTypeObject->SetBoolField(TEXT("is_reference"), true);
		}

		if (Pin.bIsConst)
		{
			PinTypeObject->SetBoolField(TEXT("is_const"), true);
		}
	}

	bool IsExecPin(const UEdGraphPin* Pin)
	{
		return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
	}

	bool HasReadablePinCategory(const UEdGraphPin* Pin)
	{
		return Pin && !Pin->PinType.PinCategory.IsNone();
	}

	FString GetLinkKind(const UEdGraphPin* SourcePin, const UEdGraphPin* TargetPin)
	{
		if (IsExecPin(SourcePin) || IsExecPin(TargetPin))
		{
			return TEXT("exec");
		}

		if (HasReadablePinCategory(SourcePin) && HasReadablePinCategory(TargetPin))
		{
			return TEXT("data");
		}

		return TEXT("unknown");
	}

	FString GetPinCategoryString(const UEdGraphPin* Pin)
	{
		return HasReadablePinCategory(Pin) ? Pin->PinType.PinCategory.ToString() : TEXT("unknown");
	}

	FString GetPinDirectionString(const UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return TEXT("unknown");
		}

		switch (Pin->Direction)
		{
		case EGPD_Input:
			return TEXT("input");
		case EGPD_Output:
			return TEXT("output");
		default:
			return TEXT("unknown");
		}
	}
}

FString FBlueprintToTextConverter::ConvertClipboardToMinimalText()
{
	return ConvertClipboardToJson();
}

FString FBlueprintToTextConverter::ConvertClipboardToJson()
{
	FString ClipboardText;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardText);
	return ConvertTextToJson(ClipboardText);
}

FString FBlueprintToTextConverter::ConvertTextToJson(const FString& T3DText)
{
	if (!IsBlueprintT3DText(T3DText))
	{
		return TEXT("");
	}

	TMap<FString, FMinimalNode> ParsedNodes;
	ParseT3DToNodes(T3DText, ParsedNodes);
	return FormatNodesToJson(ParsedNodes);
}

bool FBlueprintToTextConverter::IsBlueprintT3DText(const FString& SourceText)
{
	return SourceText.Contains(TEXT("Begin Object Class=/Script/BlueprintGraph"));
}

void FBlueprintToTextConverter::ParseVariableReferenceLine(const FString& Line, FMinimalNode& Node)
{
	Node.DisplayName = NormalizeExportValue(MatchFirstGroup(Line, TEXT("MemberName=\"([^\"]+)\"")));
	Node.VariableScopeGraphName = NormalizeExportValue(MatchFirstGroup(Line, TEXT("MemberScope=\"([^\"]+)\"")));
	Node.VariableOwnerClassPath = ExtractObjectPathFromExportValue(MatchFirstGroup(Line, TEXT("MemberParent=([^,)]+)")));

	const FString SelfContextString = MatchFirstGroup(Line, TEXT("bSelfContext=(true|false|True|False)"));
	if (!SelfContextString.IsEmpty())
	{
		Node.bSelfContext = SelfContextString.Equals(TEXT("true"), ESearchCase::IgnoreCase);
	}

	Node.VariableScopeType = Node.VariableScopeGraphName.IsEmpty() ? TEXT("member") : TEXT("local");
}

void FBlueprintToTextConverter::ParseMacroReferenceLine(const FString& Line, FMinimalNode& Node)
{
	FString MacroGraphValue = MatchFirstGroup(Line, TEXT("MacroGraph=([^,)]+)"));
	if (MacroGraphValue.IsEmpty())
	{
		MacroGraphValue = MatchFirstGroup(Line, TEXT("MacroGraphReference=\\(([^)]+)\\)"));
	}

	MacroGraphValue = NormalizeExportValue(MacroGraphValue);
	if (MacroGraphValue.IsEmpty())
	{
		return;
	}

	Node.MacroAssetPath = ExtractObjectPathFromExportValue(MacroGraphValue);

	const FString SourcePath = Node.MacroAssetPath.IsEmpty() ? MacroGraphValue : Node.MacroAssetPath;
	int32 MacroNameIndex = SourcePath.Find(TEXT(":"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (MacroNameIndex == INDEX_NONE)
	{
		MacroNameIndex = SourcePath.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	}

	Node.MacroName = MacroNameIndex != INDEX_NONE ? SourcePath.Mid(MacroNameIndex + 1) : SourcePath;
	Node.MacroName = NormalizeExportValue(Node.MacroName);
	if (MacroNameIndex != INDEX_NONE)
	{
		Node.MacroAssetPath = SourcePath.Left(MacroNameIndex);
	}
	Node.MacroAssetPath = NormalizeExportValue(Node.MacroAssetPath);
	if (!Node.MacroName.IsEmpty())
	{
		Node.DisplayName = Node.MacroName;
	}
}

void FBlueprintToTextConverter::ParseT3DToNodes(const FString& T3DText, TMap<FString, FMinimalNode>& OutNodes)
{
	TArray<FString> Lines;
	T3DText.ParseIntoArrayLines(Lines);

	FMinimalNode* CurrentNode = nullptr;
	int32 BlueprintObjectDepth = 0;

	for (const FString& RawLine : Lines)
	{
		const FString Line = RawLine.TrimStartAndEnd();
		if (Line.StartsWith(TEXT("Begin Object")))
		{
			if (Line.Contains(TEXT("/Script/BlueprintGraph")))
			{
				++BlueprintObjectDepth;
				if (BlueprintObjectDepth == 1)
				{
					FString ClassPath;
					FString NodeName;
					FParse::Value(*Line, TEXT("Class="), ClassPath);
					FParse::Value(*Line, TEXT("Name="), NodeName);
					NodeName = NormalizeExportValue(NodeName);

					const FString CleanType = NormalizeNodeTypeName(ClassPath);

					FMinimalNode NewNode;
					NewNode.InternalName = NodeName;
					NewNode.NodeType = CleanType;
					OutNodes.Add(NodeName, NewNode);
					CurrentNode = &OutNodes[NodeName];
				}
			}

			continue;
		}

		if (Line.StartsWith(TEXT("End Object")))
		{
			if (BlueprintObjectDepth > 0)
			{
				if (BlueprintObjectDepth == 1 && CurrentNode)
				{
					if (CurrentNode->DisplayName.IsEmpty())
					{
						if (!CurrentNode->MacroName.IsEmpty())
						{
							CurrentNode->DisplayName = CurrentNode->MacroName;
						}
						else
						{
							CurrentNode->DisplayName = CurrentNode->NodeType.Replace(TEXT("K2Node_"), TEXT(""));
						}
					}

					CurrentNode = nullptr;
				}

				--BlueprintObjectDepth;
			}

			continue;
		}

		if (!CurrentNode || BlueprintObjectDepth != 1)
		{
			continue;
		}

		if (Line.Contains(TEXT("NodePosX=")))
		{
			FParse::Value(*Line, TEXT("NodePosX="), CurrentNode->NodePosX);
		}

		if (Line.Contains(TEXT("NodePosY=")))
		{
			FParse::Value(*Line, TEXT("NodePosY="), CurrentNode->NodePosY);
		}

		if ((IsNodeTypeMatch(CurrentNode->NodeType, TEXT("K2Node_VariableGet")) || IsNodeTypeMatch(CurrentNode->NodeType, TEXT("K2Node_VariableSet"))) && Line.Contains(TEXT("VariableReference=")))
		{
			ParseVariableReferenceLine(Line, *CurrentNode);
			continue;
		}

		if (IsNodeTypeMatch(CurrentNode->NodeType, TEXT("K2Node_MacroInstance")) && (Line.Contains(TEXT("MacroGraphReference=")) || Line.Contains(TEXT("MacroGraph="))))
		{
			ParseMacroReferenceLine(Line, *CurrentNode);
			continue;
		}

		if (Line.Contains(TEXT("MemberName=")) && CurrentNode->DisplayName.IsEmpty())
		{
			FString MemberName;
			if (FParse::Value(*Line, TEXT("MemberName="), MemberName))
			{
				CurrentNode->DisplayName = NormalizeExportValue(MemberName);
			}
		}

		if (Line.StartsWith(TEXT("CustomProperties Pin")))
		{
			FMinimalPin NewPin;
			FParse::Value(*Line, TEXT("PinId="), NewPin.PinId);
			FParse::Value(*Line, TEXT("PinName="), NewPin.PinName);
			FParse::Value(*Line, TEXT("PinType.PinCategory="), NewPin.PinCategory);
			FParse::Value(*Line, TEXT("PinType.PinSubCategory="), NewPin.PinSubCategory);
			FParse::Value(*Line, TEXT("PinType.PinSubCategoryObject="), NewPin.PinSubCategoryObjectPath);
			FParse::Value(*Line, TEXT("DefaultValue="), NewPin.DefaultValue);

			if (NewPin.DefaultValue.IsEmpty())
			{
				FParse::Value(*Line, TEXT("AutogeneratedDefaultValue="), NewPin.DefaultValue);
			}

			const FString ContainerMatch = MatchFirstGroup(Line, TEXT("PinType.ContainerType=([^,)]+)"));
			const FString ReferenceMatch = MatchFirstGroup(Line, TEXT("PinType.bIsReference=(true|false|True|False)"));
			const FString ConstMatch = MatchFirstGroup(Line, TEXT("PinType.bIsConst=(true|false|True|False)"));

			NewPin.PinName = NormalizeExportValue(NewPin.PinName);
			NewPin.PinCategory = NormalizeExportValue(NewPin.PinCategory);
			NewPin.PinSubCategory = NormalizeExportValue(NewPin.PinSubCategory);
			NewPin.PinSubCategoryObjectPath = NormalizeExportValue(NewPin.PinSubCategoryObjectPath);
			NewPin.ContainerType = NormalizeExportValue(ContainerMatch);
			NewPin.DefaultValue = NormalizeExportValue(NewPin.DefaultValue);
			NewPin.bIsReference = ReferenceMatch.Equals(TEXT("true"), ESearchCase::IgnoreCase);
			NewPin.bIsConst = ConstMatch.Equals(TEXT("true"), ESearchCase::IgnoreCase);

			if (Line.Contains(TEXT("Direction=\"EGPD_Output\"")))
			{
				NewPin.bIsOutput = true;
			}

			const int32 LinkedToIndex = Line.Find(TEXT("LinkedTo=("));
			if (LinkedToIndex != INDEX_NONE)
			{
				const int32 StartIndex = LinkedToIndex + 10;
				const int32 EndIndex = Line.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, StartIndex);
				if (EndIndex != INDEX_NONE)
				{
					const FString LinksString = Line.Mid(StartIndex, EndIndex - StartIndex);
					LinksString.ParseIntoArray(NewPin.RawLinkedTo, TEXT(","), true);
				}
			}

			if (NewPin.bIsOutput)
			{
				CurrentNode->Outputs.Add(NewPin);
			}
			else
			{
				CurrentNode->Inputs.Add(NewPin);
			}
		}
	}
}

FString FBlueprintToTextConverter::FormatNodesToJson(const TMap<FString, FMinimalNode>& Nodes)
{
	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("version"), TEXT("2.2"));
	RootObject->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.JsonToBlueprint"));

	TMap<FString, TPair<FString, FString>> PinIdToNodePin;
	for (const auto& Pair : Nodes)
	{
		const FMinimalNode& Node = Pair.Value;
		auto CachePins = [&PinIdToNodePin, &Node](const TArray<FMinimalPin>& Pins)
		{
			for (const FMinimalPin& Pin : Pins)
			{
				PinIdToNodePin.Add(Pin.PinId, TPair<FString, FString>(Node.InternalName, Pin.PinName));
			}
		};

		CachePins(Node.Inputs);
		CachePins(Node.Outputs);
	}

	TMap<FString, TSharedPtr<FJsonObject>> LocalDeclarationMap;
	TArray<TSharedPtr<FJsonValue>> NodeValues;
	for (const auto& Pair : Nodes)
	{
		const FMinimalNode& Node = Pair.Value;
		TSharedRef<FJsonObject> NodeObject = MakeShared<FJsonObject>();
		NodeObject->SetStringField(TEXT("id"), Node.InternalName);
		NodeObject->SetStringField(TEXT("type"), Node.NodeType);
		NodeObject->SetStringField(TEXT("name"), Node.DisplayName);
		NodeObject->SetNumberField(TEXT("x"), Node.NodePosX);
		NodeObject->SetNumberField(TEXT("y"), Node.NodePosY);

		TSharedRef<FJsonObject> InputsObject = MakeShared<FJsonObject>();
		for (const FMinimalPin& Pin : Node.Inputs)
		{
			if (Pin.PinName.IsEmpty() || Pin.DefaultValue.IsEmpty())
			{
				continue;
			}

			InputsObject->SetStringField(Pin.PinName, Pin.DefaultValue);
		}
		NodeObject->SetObjectField(TEXT("inputs"), InputsObject);

		if (IsNodeTypeMatch(Node.NodeType, TEXT("K2Node_VariableGet")) || IsNodeTypeMatch(Node.NodeType, TEXT("K2Node_VariableSet")))
		{
			TSharedRef<FJsonObject> VariableObject = MakeShared<FJsonObject>();
			VariableObject->SetStringField(TEXT("scope"), Node.VariableScopeType.IsEmpty() ? TEXT("member") : Node.VariableScopeType);
			VariableObject->SetStringField(TEXT("name"), Node.DisplayName);
			VariableObject->SetBoolField(TEXT("self_context"), Node.bSelfContext);

			if (!Node.VariableOwnerClassPath.IsEmpty())
			{
				VariableObject->SetStringField(TEXT("owner_class_path"), Node.VariableOwnerClassPath);
			}

			if (!Node.VariableScopeGraphName.IsEmpty())
			{
				VariableObject->SetStringField(TEXT("scope_graph_name"), Node.VariableScopeGraphName);
			}

			const FMinimalPin* RepresentativePin = FindRepresentativeVariablePin(Node);
			if (RepresentativePin)
			{
				TSharedRef<FJsonObject> PinTypeObject = MakeShared<FJsonObject>();
				WritePinTypeToJson(*RepresentativePin, PinTypeObject);
				VariableObject->SetObjectField(TEXT("pin_type"), PinTypeObject);

				if (!RepresentativePin->DefaultValue.IsEmpty())
				{
					VariableObject->SetStringField(TEXT("default_value"), RepresentativePin->DefaultValue);
				}
			}

			if (Node.VariableScopeType.Equals(TEXT("local"), ESearchCase::IgnoreCase))
			{
				VariableObject->SetBoolField(TEXT("ensure_exists"), true);

				if (!LocalDeclarationMap.Contains(Node.DisplayName))
				{
					TSharedRef<FJsonObject> LocalVariableObject = MakeShared<FJsonObject>();
					LocalVariableObject->SetStringField(TEXT("name"), Node.DisplayName);
					LocalVariableObject->SetBoolField(TEXT("ensure_exists"), true);

					if (RepresentativePin)
					{
						TSharedRef<FJsonObject> PinTypeObject = MakeShared<FJsonObject>();
						WritePinTypeToJson(*RepresentativePin, PinTypeObject);
						LocalVariableObject->SetObjectField(TEXT("pin_type"), PinTypeObject);

						if (!RepresentativePin->DefaultValue.IsEmpty())
						{
							LocalVariableObject->SetStringField(TEXT("default_value"), RepresentativePin->DefaultValue);
						}
					}

					LocalDeclarationMap.Add(Node.DisplayName, LocalVariableObject);
				}
			}

			NodeObject->SetObjectField(TEXT("variable"), VariableObject);
		}
		else if (IsNodeTypeMatch(Node.NodeType, TEXT("K2Node_MacroInstance")))
		{
			TSharedRef<FJsonObject> MacroObject = MakeShared<FJsonObject>();
			MacroObject->SetStringField(TEXT("name"), Node.MacroName.IsEmpty() ? Node.DisplayName : Node.MacroName);
			MacroObject->SetStringField(TEXT("library"), Node.MacroAssetPath.Contains(TEXT("StandardMacros")) ? TEXT("standard") : TEXT("asset_path"));
			if (!Node.MacroAssetPath.IsEmpty())
			{
				MacroObject->SetStringField(TEXT("asset_path"), Node.MacroAssetPath);
			}
			NodeObject->SetObjectField(TEXT("macro"), MacroObject);
		}
		else
		{
			NodeObject->SetStringField(TEXT("function_name"), Node.DisplayName);
		}

		NodeValues.Add(MakeShared<FJsonValueObject>(NodeObject));
	}

	TSharedRef<FJsonObject> DeclarationsObject = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> LocalDeclarationValues;
	for (const auto& Pair : LocalDeclarationMap)
	{
		LocalDeclarationValues.Add(MakeShared<FJsonValueObject>(Pair.Value.ToSharedRef()));
	}
	DeclarationsObject->SetArrayField(TEXT("local_variables"), LocalDeclarationValues);
	RootObject->SetObjectField(TEXT("declarations"), DeclarationsObject);
	RootObject->SetArrayField(TEXT("nodes"), NodeValues);

	TArray<TSharedPtr<FJsonValue>> LinkValues;
	TSet<FString> LinkDeduplication;
	for (const auto& Pair : Nodes)
	{
		const FMinimalNode& Node = Pair.Value;
		for (const FMinimalPin& Pin : Node.Outputs)
		{
			for (const FString& RawLink : Pin.RawLinkedTo)
			{
				TArray<FString> LinkParts;
				RawLink.TrimStartAndEnd().ParseIntoArrayWS(LinkParts);
				if (LinkParts.Num() != 2)
				{
					continue;
				}

				const FString& TargetPinId = LinkParts[1];
				if (!PinIdToNodePin.Contains(TargetPinId))
				{
					continue;
				}

				const TPair<FString, FString>& TargetPin = PinIdToNodePin[TargetPinId];
				const FString LinkKey = FString::Printf(TEXT("%s.%s->%s.%s"), *Node.InternalName, *Pin.PinName, *TargetPin.Key, *TargetPin.Value);
				if (LinkDeduplication.Contains(LinkKey))
				{
					continue;
				}

				LinkDeduplication.Add(LinkKey);

				TSharedRef<FJsonObject> LinkObject = MakeShared<FJsonObject>();
				LinkObject->SetStringField(TEXT("from_id"), Node.InternalName);
				LinkObject->SetStringField(TEXT("from_pin"), Pin.PinName);
				LinkObject->SetStringField(TEXT("to_id"), TargetPin.Key);
				LinkObject->SetStringField(TEXT("to_pin"), TargetPin.Value);
				LinkValues.Add(MakeShared<FJsonValueObject>(LinkObject));
			}
		}
	}

	RootObject->SetArrayField(TEXT("links"), LinkValues);

	FString OutputString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject, Writer);
	return OutputString;
}

FString FBlueprintToTextConverter::NormalizeExportValue(const FString& InValue)
{
	return NormalizeExportTextValue(InValue);
}

// ============================================================================
// v2.1 — 蓝图完整导出
// ============================================================================

TSharedPtr<FJsonObject> FBlueprintToTextConverter::PinTypeToJson(const FEdGraphPinType& PinType)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	Obj->SetStringField(TEXT("category"), PinType.PinCategory.ToString());

	if (!PinType.PinSubCategory.IsNone())
	{
		Obj->SetStringField(TEXT("sub_category"), PinType.PinSubCategory.ToString());
	}

	if (PinType.PinSubCategoryObject.IsValid())
	{
		Obj->SetStringField(TEXT("object_path"), PinType.PinSubCategoryObject->GetPathName());
	}

	if (PinType.ContainerType == EPinContainerType::Array)
	{
		Obj->SetStringField(TEXT("container_type"), TEXT("array"));
	}
	else if (PinType.ContainerType == EPinContainerType::Set)
	{
		Obj->SetStringField(TEXT("container_type"), TEXT("set"));
	}
	else if (PinType.ContainerType == EPinContainerType::Map)
	{
		Obj->SetStringField(TEXT("container_type"), TEXT("map"));
	}

	if (PinType.bIsReference)
	{
		Obj->SetBoolField(TEXT("is_reference"), true);
	}

	if (PinType.bIsConst)
	{
		Obj->SetBoolField(TEXT("is_const"), true);
	}

	return Obj;
}

FString FBlueprintToTextConverter::IdentifyNodeType(UEdGraphNode* Node)
{
	if (!Node)
	{
		return TEXT("Unknown");
	}

	// 具体类型检测（顺序重要：子类在前）
	if (Cast<UK2Node_AssignDelegate>(Node)) return TEXT("K2Node_AssignDelegate");
	if (Cast<UK2Node_CallDelegate>(Node)) return TEXT("K2Node_CallDelegate");
	if (Cast<UK2Node_AddDelegate>(Node)) return TEXT("K2Node_AddDelegate");
	if (Cast<UK2Node_RemoveDelegate>(Node)) return TEXT("K2Node_RemoveDelegate");
	if (Cast<UK2Node_ClearDelegate>(Node)) return TEXT("K2Node_ClearDelegate");
	if (Cast<UK2Node_CreateDelegate>(Node)) return TEXT("K2Node_CreateDelegate");
	if (Cast<UK2Node_CustomEvent>(Node)) return TEXT("K2Node_CustomEvent");
	if (Cast<UK2Node_ComponentBoundEvent>(Node)) return TEXT("K2Node_ComponentBoundEvent");
	if (Cast<UK2Node_Event>(Node)) return TEXT("K2Node_Event");
	if (Cast<UK2Node_IfThenElse>(Node)) return TEXT("K2Node_IfThenElse");
	if (Cast<UK2Node_ExecutionSequence>(Node)) return TEXT("K2Node_ExecutionSequence");
	if (Cast<UK2Node_MacroInstance>(Node)) return TEXT("K2Node_MacroInstance");
	if (Cast<UK2Node_VariableGet>(Node)) return TEXT("K2Node_VariableGet");
	if (Cast<UK2Node_VariableSet>(Node)) return TEXT("K2Node_VariableSet");
	if (Cast<UK2Node_MakeArray>(Node)) return TEXT("K2Node_MakeArray");
	if (Cast<UK2Node_MakeMap>(Node)) return TEXT("K2Node_MakeMap");
	if (Cast<UK2Node_MakeSet>(Node)) return TEXT("K2Node_MakeSet");
	if (Cast<UK2Node_MakeStruct>(Node)) return TEXT("K2Node_MakeStruct");
	if (Cast<UK2Node_BreakStruct>(Node)) return TEXT("K2Node_BreakStruct");
	if (Cast<UK2Node_Self>(Node)) return TEXT("K2Node_Self");
	if (Cast<UK2Node_DynamicCast>(Node)) return TEXT("K2Node_DynamicCast");
	if (Cast<UK2Node_SpawnActorFromClass>(Node)) return TEXT("K2Node_SpawnActorFromClass");
	if (Cast<UK2Node_FormatText>(Node)) return TEXT("K2Node_FormatText");
	if (Cast<UK2Node_GetArrayItem>(Node)) return TEXT("K2Node_GetArrayItem");
	if (Cast<UK2Node_Timeline>(Node)) return TEXT("K2Node_Timeline");
	if (Cast<UK2Node_Knot>(Node)) return TEXT("K2Node_Knot");
	if (Cast<UK2Node_Literal>(Node)) return TEXT("K2Node_Literal");
	if (Cast<UK2Node_GetEnumeratorNameAsString>(Node)) return TEXT("K2Node_GetEnumeratorNameAsString");
	if (Cast<UK2Node_GetEnumeratorName>(Node)) return TEXT("K2Node_GetEnumeratorName");
	if (Cast<UK2Node_FunctionEntry>(Node)) return TEXT("K2Node_FunctionEntry");
	if (Cast<UK2Node_FunctionResult>(Node)) return TEXT("K2Node_FunctionResult");
	// v2.9 — 子类在前：PromotableOperator 和 CommutativeAssociativeBinaryOperator 均继承自 CallFunction
	if (Cast<UK2Node_EnhancedInputAction>(Node)) return TEXT("K2Node_EnhancedInputAction");
	if (Cast<UK2Node_PromotableOperator>(Node)) return TEXT("K2Node_PromotableOperator");
	if (Cast<UK2Node_CommutativeAssociativeBinaryOperator>(Node)) return TEXT("K2Node_CommutativeAssociativeBinaryOperator");
	if (Cast<UK2Node_SwitchEnum>(Node)) return TEXT("K2Node_SwitchEnum");
	if (Cast<UK2Node_SwitchInteger>(Node)) return TEXT("K2Node_SwitchInteger");
	if (Cast<UK2Node_SwitchString>(Node)) return TEXT("K2Node_SwitchString");
	if (Cast<UK2Node_SwitchName>(Node)) return TEXT("K2Node_SwitchName");
	if (Cast<UK2Node_Select>(Node)) return TEXT("K2Node_Select");
	if (Cast<UK2Node_CallFunction>(Node)) return TEXT("K2Node_CallFunction");
	if (Cast<UEdGraphNode_Comment>(Node)) return TEXT("EdGraphNode_Comment");

	return Node->GetClass()->GetName();
}

void FBlueprintToTextConverter::ExportGraphNodesAndLinks(
	UEdGraph* Graph,
	TArray<TSharedPtr<FJsonValue>>& OutNodes,
	TArray<TSharedPtr<FJsonValue>>& OutLinks)
{
	if (!Graph)
	{
		return;
	}

	TMap<UEdGraphNode*, FString> NodeToIdMap;
	TSet<FString> LinkDeduplication;

	// 为每个节点生成 ID 并导出
	for (int32 NodeIndex = 0; NodeIndex < Graph->Nodes.Num(); ++NodeIndex)
	{
		UEdGraphNode* Node = Graph->Nodes[NodeIndex];
		if (!Node)
		{
			continue;
		}

		// FunctionEntry / FunctionResult 不导出为节点，但加入 NodeToIdMap 以保留连线
		if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
		{
			NodeToIdMap.Add(Node, TEXT("__function_entry__"));
			continue;
		}
		if (UK2Node_FunctionResult* Result = Cast<UK2Node_FunctionResult>(Node))
		{
			NodeToIdMap.Add(Node, TEXT("__function_result__"));
			continue;
		}

		const FString NodeId = FString::Printf(TEXT("Node_%d"), NodeIndex);
		NodeToIdMap.Add(Node, NodeId);

		TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
		NodeObj->SetStringField(TEXT("id"), NodeId);
		NodeObj->SetStringField(TEXT("type"), IdentifyNodeType(Node));
		NodeObj->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::Digits));
		NodeObj->SetNumberField(TEXT("x"), Node->NodePosX);
		NodeObj->SetNumberField(TEXT("y"), Node->NodePosY);

		if (UPackage* Package = Node->GetOutermost())
		{
			FMetaData& MetaData = Package->GetMetaData();
			const FString Owned = MetaData.GetValue(Node, TEXT("BlueprintHelperOwned"));
			const FString BlockId = MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId"));
			if (!Owned.IsEmpty() || !BlockId.IsEmpty())
			{
				TSharedRef<FJsonObject> MetadataObj = MakeShared<FJsonObject>();
				if (!Owned.IsEmpty())
				{
					MetadataObj->SetStringField(TEXT("BlueprintHelperOwned"), Owned);
				}
				if (!BlockId.IsEmpty())
				{
					MetadataObj->SetStringField(TEXT("BlueprintHelperBlockId"), BlockId);
				}
				NodeObj->SetObjectField(TEXT("metadata"), MetadataObj);
			}
		}

		// 节点名称
		const FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		if (!NodeTitle.IsEmpty())
		{
			NodeObj->SetStringField(TEXT("name"), NodeTitle);
		}

		// 类型特定字段
		if (UK2Node_CallFunction* CallFunc = Cast<UK2Node_CallFunction>(Node))
		{
			if (UFunction* Func = CallFunc->GetTargetFunction())
			{
				NodeObj->SetStringField(TEXT("function_name"), Func->GetName());
			}
		}
		else if (UK2Node_VariableGet* VarGet = Cast<UK2Node_VariableGet>(Node))
		{
			TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
			VarObj->SetStringField(TEXT("scope"), VarGet->VariableReference.IsLocalScope() ? TEXT("local") : TEXT("member"));
			VarObj->SetStringField(TEXT("name"), VarGet->GetVarName().ToString());
			VarObj->SetBoolField(TEXT("self_context"), VarGet->VariableReference.IsSelfContext());
			NodeObj->SetObjectField(TEXT("variable"), VarObj);
		}
		else if (UK2Node_VariableSet* VarSet = Cast<UK2Node_VariableSet>(Node))
		{
			TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
			VarObj->SetStringField(TEXT("scope"), VarSet->VariableReference.IsLocalScope() ? TEXT("local") : TEXT("member"));
			VarObj->SetStringField(TEXT("name"), VarSet->GetVarName().ToString());
			VarObj->SetBoolField(TEXT("self_context"), VarSet->VariableReference.IsSelfContext());
			NodeObj->SetObjectField(TEXT("variable"), VarObj);
		}
		else if (UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))
		{
			TSharedPtr<FJsonObject> MacroObj = MakeShared<FJsonObject>();
			if (UEdGraph* MacroGraph = MacroNode->GetMacroGraph())
			{
				MacroObj->SetStringField(TEXT("name"), MacroGraph->GetName());
			}
			MacroObj->SetStringField(TEXT("library"), TEXT("standard"));
			NodeObj->SetObjectField(TEXT("macro"), MacroObj);
		}
		else if (UK2Node_CustomEvent* CustomEvt = Cast<UK2Node_CustomEvent>(Node))
		{
			TSharedPtr<FJsonObject> EventObj = MakeShared<FJsonObject>();
			EventObj->SetStringField(TEXT("event_name"), CustomEvt->CustomFunctionName.ToString());
			NodeObj->SetObjectField(TEXT("event"), EventObj);
		}
		else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
		{
			TSharedPtr<FJsonObject> EventObj = MakeShared<FJsonObject>();
			EventObj->SetStringField(TEXT("event_name"), EventNode->GetFunctionName().ToString());
			NodeObj->SetObjectField(TEXT("event"), EventObj);
		}
		else if (UK2Node_BaseMCDelegate* DelegateNode = Cast<UK2Node_BaseMCDelegate>(Node))
		{
			TSharedPtr<FJsonObject> DelegateObj = MakeShared<FJsonObject>();
			DelegateObj->SetStringField(TEXT("delegate_property_name"), DelegateNode->DelegateReference.GetMemberName().ToString());
			NodeObj->SetObjectField(TEXT("delegate"), DelegateObj);
		}
		// v2.3 — 组件绑定事件（注意：需在 Event 之前检测，但此处已由 IdentifyNodeType 区分）
		else if (UK2Node_ComponentBoundEvent* CompEvent = Cast<UK2Node_ComponentBoundEvent>(Node))
		{
			TSharedPtr<FJsonObject> CompEventObj = MakeShared<FJsonObject>();
			CompEventObj->SetStringField(TEXT("delegate_property"), CompEvent->DelegatePropertyName.ToString());
			CompEventObj->SetStringField(TEXT("component_property"), CompEvent->GetComponentPropertyName().ToString());
			if (CompEvent->DelegateOwnerClass)
			{
				CompEventObj->SetStringField(TEXT("delegate_owner_class"), CompEvent->DelegateOwnerClass->GetPathName());
			}
			NodeObj->SetObjectField(TEXT("component_event"), CompEventObj);
		}
		// v2.3 — Literal（对象引用常量）
		else if (UK2Node_Literal* LiteralNode = Cast<UK2Node_Literal>(Node))
		{
			TSharedPtr<FJsonObject> LiteralObj = MakeShared<FJsonObject>();
			if (LiteralNode->GetObjectRef())
			{
				LiteralObj->SetStringField(TEXT("object_path"), LiteralNode->GetObjectRef()->GetPathName());
			}
			NodeObj->SetObjectField(TEXT("literal"), LiteralObj);
		}
		// v2.3 — Comment 节点
		else if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(Node))
		{
			TSharedPtr<FJsonObject> CommentObj = MakeShared<FJsonObject>();
			CommentObj->SetStringField(TEXT("text"), CommentNode->NodeComment);
			CommentObj->SetNumberField(TEXT("width"), CommentNode->NodeWidth);
			CommentObj->SetNumberField(TEXT("height"), CommentNode->NodeHeight);
			CommentObj->SetNumberField(TEXT("font_size"), CommentNode->FontSize);
			CommentObj->SetStringField(TEXT("color"), CommentNode->CommentColor.ToString());
			NodeObj->SetObjectField(TEXT("comment"), CommentObj);
		}
		// v2.9 — Enhanced Input Action
		if (UK2Node_EnhancedInputAction* InputActionNode = Cast<UK2Node_EnhancedInputAction>(Node))
		{
			const UInputAction* IA = InputActionNode->InputAction;
			if (IA)
			{
				NodeObj->SetStringField(TEXT("input_action_path"), IA->GetPathName());
			}
		}
		// v2.9 — PromotableOperator（继承自 CallFunction，type 已被识别，但仍导出 function_name）
		// v2.9 — Switch 系列
		if (UK2Node_SwitchEnum* SwitchEnum = Cast<UK2Node_SwitchEnum>(Node))
		{
			TSharedPtr<FJsonObject> SwitchObj = MakeShared<FJsonObject>();
			SwitchObj->SetBoolField(TEXT("has_default"), SwitchEnum->bHasDefaultPin);
			if (SwitchEnum->GetEnum())
			{
				SwitchObj->SetStringField(TEXT("enum_path"), SwitchEnum->GetEnum()->GetPathName());
			}
			NodeObj->SetObjectField(TEXT("switch"), SwitchObj);
		}
		else if (UK2Node_SwitchInteger* SwitchInt = Cast<UK2Node_SwitchInteger>(Node))
		{
			TSharedPtr<FJsonObject> SwitchObj = MakeShared<FJsonObject>();
			SwitchObj->SetBoolField(TEXT("has_default"), SwitchInt->bHasDefaultPin);
			SwitchObj->SetNumberField(TEXT("start_index"), SwitchInt->StartIndex);
			NodeObj->SetObjectField(TEXT("switch"), SwitchObj);
		}
		else if (UK2Node_SwitchString* SwitchStr = Cast<UK2Node_SwitchString>(Node))
		{
			TSharedPtr<FJsonObject> SwitchObj = MakeShared<FJsonObject>();
			SwitchObj->SetBoolField(TEXT("has_default"), SwitchStr->bHasDefaultPin);
			TArray<TSharedPtr<FJsonValue>> CaseArray;
			for (const FName& PinName : SwitchStr->PinNames)
			{
				CaseArray.Add(MakeShared<FJsonValueString>(PinName.ToString()));
			}
			SwitchObj->SetArrayField(TEXT("case_values"), CaseArray);
			NodeObj->SetObjectField(TEXT("switch"), SwitchObj);
		}
		else if (UK2Node_SwitchName* SwitchNameNode = Cast<UK2Node_SwitchName>(Node))
		{
			TSharedPtr<FJsonObject> SwitchObj = MakeShared<FJsonObject>();
			SwitchObj->SetBoolField(TEXT("has_default"), SwitchNameNode->bHasDefaultPin);
			TArray<TSharedPtr<FJsonValue>> CaseArray;
			for (const FName& PinName : SwitchNameNode->PinNames)
			{
				CaseArray.Add(MakeShared<FJsonValueString>(PinName.ToString()));
			}
			SwitchObj->SetArrayField(TEXT("case_values"), CaseArray);
			NodeObj->SetObjectField(TEXT("switch"), SwitchObj);
		}
		// v2.9 — Select
		if (UK2Node_Select* SelectNode = Cast<UK2Node_Select>(Node))
		{
			TSharedPtr<FJsonObject> SelectObj = MakeShared<FJsonObject>();
			TArray<UEdGraphPin*> OptionPins;
			SelectNode->GetOptionPins(OptionPins);
			SelectObj->SetNumberField(TEXT("num_options"), OptionPins.Num());
			if (SelectNode->GetEnum())
			{
				SelectObj->SetStringField(TEXT("enum_path"), SelectNode->GetEnum()->GetPathName());
			}
			NodeObj->SetObjectField(TEXT("select"), SelectObj);
		}

		// 输入引脚默认值
		TSharedPtr<FJsonObject> InputsObj = MakeShared<FJsonObject>();
		bool bHasInputDefaults = false;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input)
			{
				continue;
			}
			if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				continue;
			}
			if (Pin->LinkedTo.Num() > 0)
			{
				continue;
			}
			if (!Pin->DefaultValue.IsEmpty())
			{
				InputsObj->SetStringField(Pin->PinName.ToString(), Pin->DefaultValue);
				bHasInputDefaults = true;
			}
		}
		if (bHasInputDefaults)
		{
			NodeObj->SetObjectField(TEXT("inputs"), InputsObj);
		}

		OutNodes.Add(MakeShared<FJsonValueObject>(NodeObj));
	}

	// 导出连线
	for (const auto& Pair : NodeToIdMap)
	{
		UEdGraphNode* SourceNode = Pair.Key;
		const FString& SourceId = Pair.Value;

		for (UEdGraphPin* Pin : SourceNode->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output)
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!LinkedPin || !LinkedPin->GetOwningNode())
				{
					continue;
				}

				const FString* TargetId = NodeToIdMap.Find(LinkedPin->GetOwningNode());
				if (!TargetId)
				{
					continue;
				}

				const FString LinkKey = FString::Printf(TEXT("%s.%s->%s.%s"),
					*SourceId, *Pin->PinName.ToString(), **TargetId, *LinkedPin->PinName.ToString());
				if (LinkDeduplication.Contains(LinkKey))
				{
					continue;
				}
				LinkDeduplication.Add(LinkKey);

				TSharedRef<FJsonObject> LinkObj = MakeShared<FJsonObject>();
				LinkObj->SetStringField(TEXT("from_id"), SourceId);
				LinkObj->SetStringField(TEXT("from_pin"), Pin->PinName.ToString());
				LinkObj->SetStringField(TEXT("to_id"), *TargetId);
				LinkObj->SetStringField(TEXT("to_pin"), LinkedPin->PinName.ToString());
				LinkObj->SetStringField(TEXT("kind"), GetLinkKind(Pin, LinkedPin));
				LinkObj->SetStringField(TEXT("from_pin_type"), GetPinCategoryString(Pin));
				LinkObj->SetStringField(TEXT("to_pin_type"), GetPinCategoryString(LinkedPin));
				LinkObj->SetStringField(TEXT("from_direction"), GetPinDirectionString(Pin));
				LinkObj->SetStringField(TEXT("to_direction"), GetPinDirectionString(LinkedPin));
				OutLinks.Add(MakeShared<FJsonValueObject>(LinkObj));
			}
		}
	}
}

TSharedPtr<FJsonObject> FBlueprintToTextConverter::ConvertGraphToJsonObject(UEdGraph* TargetGraph)
{
	if (!TargetGraph)
	{
		return MakeShared<FJsonObject>();
	}

	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("version"), TEXT("2.2"));
	RootObject->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.JsonToBlueprint"));

	TArray<TSharedPtr<FJsonValue>> NodesArray;
	TArray<TSharedPtr<FJsonValue>> LinksArray;
	ExportGraphNodesAndLinks(TargetGraph, NodesArray, LinksArray);

	RootObject->SetArrayField(TEXT("nodes"), NodesArray);
	RootObject->SetArrayField(TEXT("links"), LinksArray);

	return RootObject;
}

FString FBlueprintToTextConverter::ConvertGraphToJson(UEdGraph* TargetGraph)
{
	return SerializeJsonObject(ConvertGraphToJsonObject(TargetGraph));
}

TSharedPtr<FJsonObject> FBlueprintToTextConverter::ExportBlueprintToJsonObject(UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return MakeShared<FJsonObject>();
	}

	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("version"), TEXT("2.2"));
	RootObject->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.JsonToBlueprint"));

	// === blueprint_operations：成员变量 ===
	TArray<TSharedPtr<FJsonValue>> OperationsArray;

	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarType.PinCategory == UEdGraphSchema_K2::PC_MCDelegate)
		{
			// 事件分发器
			TSharedPtr<FJsonObject> OpObj = MakeShared<FJsonObject>();
			OpObj->SetStringField(TEXT("op"), TEXT("add_event_dispatcher"));
			OpObj->SetStringField(TEXT("name"), Var.VarName.ToString());

			// 导出签名参数
			UEdGraph* SigGraph = nullptr;
			for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
			{
				if (Graph && Graph->GetFName() == Var.VarName)
				{
					SigGraph = Graph;
					break;
				}
			}

			if (SigGraph)
			{
				TArray<TSharedPtr<FJsonValue>> ParamsArray;
				for (UEdGraphNode* Node : SigGraph->Nodes)
				{
					UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node);
					if (!Entry)
					{
						continue;
					}
					for (const TSharedPtr<FUserPinInfo>& PinInfo : Entry->UserDefinedPins)
					{
						TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
						ParamObj->SetStringField(TEXT("name"), PinInfo->PinName.ToString());
						ParamObj->SetObjectField(TEXT("pin_type"), PinTypeToJson(PinInfo->PinType));
						ParamsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
					}
					break;
				}
				if (ParamsArray.Num() > 0)
				{
					OpObj->SetArrayField(TEXT("params"), ParamsArray);
				}
			}

			OperationsArray.Add(MakeShared<FJsonValueObject>(OpObj));
		}
		else
		{
			// 普通成员变量
			TSharedPtr<FJsonObject> OpObj = MakeShared<FJsonObject>();
			OpObj->SetStringField(TEXT("op"), TEXT("add_member_variable"));
			OpObj->SetStringField(TEXT("name"), Var.VarName.ToString());
			OpObj->SetObjectField(TEXT("pin_type"), PinTypeToJson(Var.VarType));

			if (!Var.DefaultValue.IsEmpty())
			{
				OpObj->SetStringField(TEXT("default_value"), Var.DefaultValue);
			}

			if (!Var.Category.IsEmpty())
			{
				OpObj->SetStringField(TEXT("category"), Var.Category.ToString());
			}

			OperationsArray.Add(MakeShared<FJsonValueObject>(OpObj));
		}
	}

	// 函数图签名
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		// 跳过默认的 ConstructionScript
		const FString GraphName = Graph->GetName();
		if (GraphName.Equals(TEXT("UserConstructionScript"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		TSharedPtr<FJsonObject> OpObj = MakeShared<FJsonObject>();
		OpObj->SetStringField(TEXT("op"), TEXT("add_function_graph"));
		OpObj->SetStringField(TEXT("name"), GraphName);

		// 查找 Entry 和 Result 节点提取签名
		TArray<TSharedPtr<FJsonValue>> InputsArray;
		TArray<TSharedPtr<FJsonValue>> OutputsArray;
		bool bIsPure = false;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
			{
				bIsPure = (Entry->GetExtraFlags() & FUNC_BlueprintPure) != 0;
				for (const TSharedPtr<FUserPinInfo>& PinInfo : Entry->UserDefinedPins)
				{
					TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
					ParamObj->SetStringField(TEXT("name"), PinInfo->PinName.ToString());
					ParamObj->SetObjectField(TEXT("pin_type"), PinTypeToJson(PinInfo->PinType));
					InputsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
				}
			}
			else if (UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(Node))
			{
				for (const TSharedPtr<FUserPinInfo>& PinInfo : ResultNode->UserDefinedPins)
				{
					TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
					ParamObj->SetStringField(TEXT("name"), PinInfo->PinName.ToString());
					ParamObj->SetObjectField(TEXT("pin_type"), PinTypeToJson(PinInfo->PinType));
					OutputsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
				}
			}
		}

		if (InputsArray.Num() > 0)
		{
			OpObj->SetArrayField(TEXT("inputs"), InputsArray);
		}
		if (OutputsArray.Num() > 0)
		{
			OpObj->SetArrayField(TEXT("outputs"), OutputsArray);
		}
		if (bIsPure)
		{
			OpObj->SetBoolField(TEXT("is_pure"), true);
		}

		OperationsArray.Add(MakeShared<FJsonValueObject>(OpObj));
	}

	// 宏图签名
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		TSharedPtr<FJsonObject> OpObj = MakeShared<FJsonObject>();
		OpObj->SetStringField(TEXT("op"), TEXT("add_macro_graph"));
		OpObj->SetStringField(TEXT("name"), Graph->GetName());
		OperationsArray.Add(MakeShared<FJsonValueObject>(OpObj));
	}

	if (OperationsArray.Num() > 0)
	{
		RootObject->SetArrayField(TEXT("blueprint_operations"), OperationsArray);
	}

	// === graphs 数组 ===
	TArray<TSharedPtr<FJsonValue>> GraphsArray;

	// EventGraph 页面
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph)
		{
			continue;
		}

		TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
		GraphObj->SetStringField(TEXT("graph"), Graph->GetName());

		TArray<TSharedPtr<FJsonValue>> NodesArr;
		TArray<TSharedPtr<FJsonValue>> LinksArr;
		ExportGraphNodesAndLinks(Graph, NodesArr, LinksArr);

		GraphObj->SetArrayField(TEXT("nodes"), NodesArr);
		GraphObj->SetArrayField(TEXT("links"), LinksArr);
		GraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
	}

	// 函数图内容
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		const FString GraphName = Graph->GetName();
		if (GraphName.Equals(TEXT("UserConstructionScript"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
		GraphObj->SetStringField(TEXT("graph"), GraphName);

		TArray<TSharedPtr<FJsonValue>> NodesArr;
		TArray<TSharedPtr<FJsonValue>> LinksArr;
		ExportGraphNodesAndLinks(Graph, NodesArr, LinksArr);

		GraphObj->SetArrayField(TEXT("nodes"), NodesArr);
		GraphObj->SetArrayField(TEXT("links"), LinksArr);
		GraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
	}

	// 宏图内容
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
		GraphObj->SetStringField(TEXT("graph"), Graph->GetName());

		TArray<TSharedPtr<FJsonValue>> NodesArr;
		TArray<TSharedPtr<FJsonValue>> LinksArr;
		ExportGraphNodesAndLinks(Graph, NodesArr, LinksArr);

		GraphObj->SetArrayField(TEXT("nodes"), NodesArr);
		GraphObj->SetArrayField(TEXT("links"), LinksArr);
		GraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
	}

	RootObject->SetArrayField(TEXT("graphs"), GraphsArray);

	return RootObject;
}

FString FBlueprintToTextConverter::ExportBlueprintToJson(UBlueprint* Blueprint)
{
	return SerializeJsonObject(ExportBlueprintToJsonObject(Blueprint));
}

FString FBlueprintToTextConverter::SerializeJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (!JsonObject.IsValid())
	{
		return TEXT("{}");
	}
	FString OutputString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	return OutputString;
}
