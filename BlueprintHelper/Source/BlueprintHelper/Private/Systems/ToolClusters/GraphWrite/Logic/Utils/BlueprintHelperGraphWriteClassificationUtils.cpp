// BlueprintHelper GraphWrite classification utilities implementation.

#include "Systems/ToolClusters/GraphWrite/Logic/Utils/BlueprintHelperGraphWriteClassificationUtils.h"

#include "Dom/JsonObject.h"

struct FBlueprintHelperGraphWriteTextClassificationRule
{
	const TCHAR* Result;
	TArray<const TCHAR*> Tokens;
	TArray<const TCHAR*> RequiredFields;
};

struct FBlueprintHelperGraphWriteKindRule
{
	EBlueprintHelperLogicNodeKind Kind;
	TArray<const TCHAR*> ClassTokens;
	TArray<const TCHAR*> MemberTokens;
};

FString FBlueprintHelperGraphWriteClassificationUtils::NormalizeToken(const FString& InValue)
{
	FString Result = InValue;
	Result.TrimStartAndEndInline();
	Result.ReplaceInline(TEXT("\""), TEXT(""));
	Result.ReplaceInline(TEXT("'"), TEXT(""));
	Result.ReplaceInline(TEXT(" "), TEXT(""));
	Result.ReplaceInline(TEXT("_"), TEXT(""));
	Result.ReplaceInline(TEXT("-"), TEXT(""));
	return Result.ToLower();
}

FString FBlueprintHelperGraphWriteClassificationUtils::NormalizeNodeTypeName(const FString& InValue)
{
	FString Result = InValue;
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

static bool BlueprintHelperGraphWriteContainsAnyToken(
	const FString& NormalizedText,
	const TArray<const TCHAR*>& Tokens)
{
	for (const TCHAR* Token : Tokens)
	{
		if (Token && NormalizedText.Contains(Token, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

static bool BlueprintHelperGraphWriteHasAnyField(
	const TSharedPtr<FJsonObject>& NodeObject,
	const TArray<const TCHAR*>& Fields)
{
	for (const TCHAR* Field : Fields)
	{
		if (Field && NodeObject.IsValid() && NodeObject->HasField(Field))
		{
			return true;
		}
	}
	return false;
}

FString FBlueprintHelperGraphWriteClassificationUtils::ClassifyLogicNode(
	const TSharedPtr<FJsonObject>& NodeObject,
	const FString& RawType)
{
	const FString TypeKey = NormalizeToken(NormalizeNodeTypeName(RawType));
	static const FBlueprintHelperGraphWriteTextClassificationRule Rules[] =
	{
		{ TEXT("comment"), { TEXT("comment") }, { TEXT("comment") } },
		{ TEXT("branch"), { TEXT("ifthenelse"), TEXT("branch") }, {} },
		{ TEXT("switch"), { TEXT("switch") }, {} },
		{ TEXT("sequence"), { TEXT("executionsequence"), TEXT("sequence") }, {} },
		{ TEXT("loop"), { TEXT("loop"), TEXT("foreach"), TEXT("while") }, {} },
		{ TEXT("broadcast"), { TEXT("calldelegate"), TEXT("broadcast") }, {} },
		{ TEXT("bind_delegate"), { TEXT("adddelegate"), TEXT("assigndelegate"), TEXT("createdelegate"), TEXT("binddelegate") }, {} },
		{ TEXT("unbind_delegate"), { TEXT("removedelegate"), TEXT("cleardelegate"), TEXT("unbinddelegate") }, {} },
		{ TEXT("timeline"), { TEXT("timeline") }, { TEXT("timeline") } },
		{ TEXT("cast"), { TEXT("dynamiccast"), TEXT("cast") }, { TEXT("cast") } },
		{ TEXT("reroute"), { TEXT("knot"), TEXT("reroute") }, {} },
		{ TEXT("event"), { TEXT("customevent"), TEXT("componentboundevent"), TEXT("enhancedinputaction"), TEXT("k2nodeevent"), TEXT("event") }, { TEXT("event"), TEXT("component_event"), TEXT("input_action_path") } },
		{ TEXT("set"), { TEXT("variableset") }, {} },
		{ TEXT("get"), { TEXT("variableget"), TEXT("self"), TEXT("literal"), TEXT("getenumerator"), TEXT("getarrayitem") }, { TEXT("variable") } },
		{ TEXT("call"), { TEXT("callfunction"), TEXT("macroinstance"), TEXT("promotableoperator"), TEXT("commutativeassociativebinaryoperator"), TEXT("spawnactor"), TEXT("formattext"), TEXT("select") }, { TEXT("function_name"), TEXT("macro") } }
	};

	for (const FBlueprintHelperGraphWriteTextClassificationRule& Rule : Rules)
	{
		if (BlueprintHelperGraphWriteContainsAnyToken(TypeKey, Rule.Tokens)
			|| BlueprintHelperGraphWriteHasAnyField(NodeObject, Rule.RequiredFields))
		{
			if (FString(Rule.Result).Equals(TEXT("get"), ESearchCase::IgnoreCase)
				&& TypeKey.Contains(TEXT("variableset")))
			{
				return TEXT("set");
			}
			return Rule.Result;
		}
	}
	return TEXT("unknown");
}

FString FBlueprintHelperGraphWriteClassificationUtils::NormalizeExplicitLinkKind(const FString& RawKind)
{
	const FString KindKey = NormalizeToken(RawKind);
	static const FBlueprintHelperGraphWriteTextClassificationRule Rules[] =
	{
		{ TEXT("exec"), { TEXT("exec"), TEXT("execution"), TEXT("flow"), TEXT("control") }, {} },
		{ TEXT("data"), { TEXT("data"), TEXT("value"), TEXT("dependency"), TEXT("property") }, {} }
	};

	for (const FBlueprintHelperGraphWriteTextClassificationRule& Rule : Rules)
	{
		if (BlueprintHelperGraphWriteContainsAnyToken(KindKey, Rule.Tokens))
		{
			return Rule.Result;
		}
	}
	return TEXT("unknown");
}

bool FBlueprintHelperGraphWriteClassificationUtils::IsExecPinName(const FString& PinName)
{
	const FString Key = NormalizeToken(PinName);
	static const TCHAR* ExecPinNames[] =
	{
		TEXT("exec"),
		TEXT("execute"),
		TEXT("then"),
		TEXT("completed"),
		TEXT("complete"),
		TEXT("finished"),
		TEXT("true"),
		TEXT("false"),
		TEXT("loopbody"),
		TEXT("body"),
		TEXT("castsucceeded"),
		TEXT("castfailed"),
		TEXT("valid"),
		TEXT("notvalid"),
		TEXT("isvalid"),
		TEXT("isnotvalid")
	};

	for (const TCHAR* Name : ExecPinNames)
	{
		if (Key.Equals(Name) || (FString(Name).Equals(TEXT("then")) && Key.StartsWith(TEXT("then"))))
		{
			return true;
		}
	}
	return false;
}

EBlueprintHelperLogicLinkType FBlueprintHelperGraphWriteClassificationUtils::IdentifyGraphLinkType(
	const FString& ExplicitKind,
	const FString& PinType,
	const FString& FromPin,
	const FString& ToPin)
{
	const FString NormalizedKind = NormalizeExplicitLinkKind(ExplicitKind);
	if (NormalizedKind.Equals(TEXT("data"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperLogicLinkType::Data;
	}
	if (NormalizedKind.Equals(TEXT("exec"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperLogicLinkType::Exec;
	}

	if (!PinType.IsEmpty())
	{
		return NormalizeToken(PinType).Equals(TEXT("exec"))
			? EBlueprintHelperLogicLinkType::Exec
			: EBlueprintHelperLogicLinkType::Data;
	}

	return IsExecPinName(FromPin) || IsExecPinName(ToPin)
		? EBlueprintHelperLogicLinkType::Exec
		: EBlueprintHelperLogicLinkType::Data;
}

EBlueprintHelperLogicNodeKind FBlueprintHelperGraphWriteClassificationUtils::IdentifyNodeKind(
	const FString& ClassName,
	const FString& MemberName)
{
	static const FBlueprintHelperGraphWriteKindRule Rules[] =
	{
		{ EBlueprintHelperLogicNodeKind::FunctionEntry, { TEXT("K2Node_FunctionEntry") }, {} },
		{ EBlueprintHelperLogicNodeKind::CustomEvent, { TEXT("K2Node_CustomEvent") }, { TEXT("CustomEvent") } },
		{ EBlueprintHelperLogicNodeKind::Event, { TEXT("K2Node_Event") }, {} },
		{ EBlueprintHelperLogicNodeKind::CallFunction, { TEXT("K2Node_CallFunction") }, {} },
		{ EBlueprintHelperLogicNodeKind::Branch, { TEXT("K2Node_IfThenElse") }, {} },
		{ EBlueprintHelperLogicNodeKind::Sequence, { TEXT("K2Node_ExecutionSequence") }, {} },
		{ EBlueprintHelperLogicNodeKind::VariableGet, { TEXT("K2Node_VariableGet") }, {} },
		{ EBlueprintHelperLogicNodeKind::VariableSet, { TEXT("K2Node_VariableSet") }, {} },
		{ EBlueprintHelperLogicNodeKind::Macro, { TEXT("K2Node_MacroInstance") }, {} },
		{ EBlueprintHelperLogicNodeKind::DelegateBind, { TEXT("K2Node_CreateDelegate") }, {} },
		{ EBlueprintHelperLogicNodeKind::Timeline, { TEXT("Timeline") }, {} }
	};

	for (const FBlueprintHelperGraphWriteKindRule& Rule : Rules)
	{
		if (BlueprintHelperGraphWriteContainsAnyToken(ClassName, Rule.ClassTokens)
			|| BlueprintHelperGraphWriteContainsAnyToken(MemberName, Rule.MemberTokens))
		{
			return Rule.Kind;
		}
	}
	return EBlueprintHelperLogicNodeKind::Unknown;
}
