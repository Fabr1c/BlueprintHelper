#include "BlueprintTextConverter.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Internationalization/Regex.h"
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
	RootObject->SetStringField(TEXT("version"), TEXT("2.0"));
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

