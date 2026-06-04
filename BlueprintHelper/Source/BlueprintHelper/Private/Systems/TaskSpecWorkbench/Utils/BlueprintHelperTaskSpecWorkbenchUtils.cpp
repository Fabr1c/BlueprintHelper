// BlueprintHelper Utils -- TaskSpec/ReadContext workbench 工具函数库实现

#include "Systems/TaskSpecWorkbench/Utils/BlueprintHelperTaskSpecWorkbenchUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintTextConverter.h"

FString UBlueprintHelperTaskSpecWorkbenchUtils::SanitizeIdSegment(const FString& Value)
{
	FString Result = Value;
	Result.ReplaceInline(TEXT("$"), TEXT("root"));
	Result.ReplaceInline(TEXT("."), TEXT("_"));
	Result.ReplaceInline(TEXT("["), TEXT("_"));
	Result.ReplaceInline(TEXT("]"), TEXT(""));
	Result.ReplaceInline(TEXT(" "), TEXT("_"));
	Result.ReplaceInline(TEXT("|"), TEXT("_"));
	Result.ReplaceInline(TEXT(":"), TEXT("_"));
	Result.ReplaceInline(TEXT("/"), TEXT("_"));
	return Result.IsEmpty() ? TEXT("item") : Result;
}

bool UBlueprintHelperTaskSpecWorkbenchUtils::TryDeserializeJsonObject(
	const FString& SourceText,
	TSharedPtr<FJsonObject>& OutObject,
	FString& OutError)
{
	OutObject.Reset();
	OutError.Reset();

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SourceText);
	if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
	{
		OutError = Reader->GetErrorMessage();
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Input is not a valid JSON object.");
		}
		return false;
	}
	return true;
}

FString UBlueprintHelperTaskSpecWorkbenchUtils::SerializeJsonObject(const TSharedPtr<FJsonObject>& Object)
{
	if (!Object.IsValid())
	{
		return TEXT("{}");
	}

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
	return Output;
}

FString UBlueprintHelperTaskSpecWorkbenchUtils::JsonValueToDisplayString(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		return TEXT("");
	}

	switch (Value->Type)
	{
	case EJson::String:
		return Value->AsString();
	case EJson::Number:
		return LexToString(Value->AsNumber());
	case EJson::Boolean:
		return Value->AsBool() ? TEXT("true") : TEXT("false");
	case EJson::Object:
		return SerializeJsonObject(Value->AsObject());
	case EJson::Array:
		return FString::Printf(TEXT("array[%d]"), Value->AsArray().Num());
	default:
		return TEXT("");
	}
}

FString UBlueprintHelperTaskSpecWorkbenchUtils::ReadStringField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName)
{
	if (!Object.IsValid())
	{
		return FString();
	}

	FString Value;
	Object->TryGetStringField(FieldName, Value);
	return Value;
}

void UBlueprintHelperTaskSpecWorkbenchUtils::ReadStringArrayField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	TArray<FString>& OutValues)
{
	const TArray<TSharedPtr<FJsonValue>>* ArrayValues = nullptr;
	if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, ArrayValues) || !ArrayValues)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *ArrayValues)
	{
		if (Value.IsValid() && Value->Type == EJson::String)
		{
			OutValues.Add(Value->AsString());
		}
	}
}

bool UBlueprintHelperTaskSpecWorkbenchUtils::IsCallStatementKind(const FString& Kind)
{
	return Kind.Equals(TEXT("call"), ESearchCase::IgnoreCase);
}

bool UBlueprintHelperTaskSpecWorkbenchUtils::IsGraphStatementKind(const FString& Kind)
{
	return IsCallStatementKind(Kind)
		|| Kind.Equals(TEXT("set"), ESearchCase::IgnoreCase)
		|| Kind.Equals(TEXT("set_property"), ESearchCase::IgnoreCase)
		|| Kind.Equals(TEXT("branch"), ESearchCase::IgnoreCase)
		|| Kind.Equals(TEXT("let"), ESearchCase::IgnoreCase)
		|| Kind.Equals(TEXT("return"), ESearchCase::IgnoreCase)
		|| Kind.Equals(TEXT("op"), ESearchCase::IgnoreCase)
		|| Kind.Equals(TEXT("construct"), ESearchCase::IgnoreCase)
		|| Kind.Equals(TEXT("deconstruct"), ESearchCase::IgnoreCase)
		|| Kind.Equals(TEXT("select"), ESearchCase::IgnoreCase)
		|| Kind.Equals(TEXT("get"), ESearchCase::IgnoreCase)
		|| Kind.Equals(TEXT("get_property"), ESearchCase::IgnoreCase);
}

FString UBlueprintHelperTaskSpecWorkbenchUtils::ResolveCallQuery(const TSharedPtr<FJsonObject>& Object, const FString& Kind)
{
	FString Query;
	Object->TryGetStringField(TEXT("target"), Query);
	return Query.TrimStartAndEnd();
}

FString UBlueprintHelperTaskSpecWorkbenchUtils::ResolveStatementTitle(const TSharedPtr<FJsonObject>& Object, const FString& Kind)
{
	if (IsCallStatementKind(Kind))
	{
		return FString::Printf(TEXT("CallFunction: %s"), *ResolveCallQuery(Object, Kind));
	}

	FString Target;
	Object->TryGetStringField(TEXT("target"), Target);
	if (Target.IsEmpty())
	{
		Object->TryGetStringField(TEXT("name"), Target);
	}
	if (Target.IsEmpty())
	{
		Object->TryGetStringField(TEXT("op"), Target);
	}
	return Target.IsEmpty()
		? FString::Printf(TEXT("Graph: %s"), *Kind)
		: FString::Printf(TEXT("%s: %s"), *Kind, *Target);
}

FString UBlueprintHelperTaskSpecWorkbenchUtils::ResolveStatementDetail(const TSharedPtr<FJsonObject>& Object)
{
	const TSharedPtr<FJsonObject>* ArgsObject = nullptr;
	if (Object.IsValid() && Object->TryGetObjectField(TEXT("args"), ArgsObject) && ArgsObject && ArgsObject->IsValid())
	{
		TArray<FString> Args;
		FBlueprintHelperVersionCompat::GetJsonObjectKeys(*ArgsObject, Args);
		return FString::Printf(TEXT("args: %s"), *FString::Join(Args, TEXT(", ")));
	}
	return FString();
}

FString UBlueprintHelperTaskSpecWorkbenchUtils::InferExpressionType(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		return FString();
	}

	if (Value->Type != EJson::Object)
	{
		switch (Value->Type)
		{
		case EJson::String:
			return TEXT("string");
		case EJson::Number:
			return TEXT("float");
		case EJson::Boolean:
			return TEXT("bool");
		default:
			return FString();
		}
	}

	const TSharedPtr<FJsonObject> Object = Value->AsObject();
	if (!Object.IsValid())
	{
		return FString();
	}

	FString Type;
	Object->TryGetStringField(TEXT("type"), Type);
	if (Type.IsEmpty())
	{
		Object->TryGetStringField(TEXT("value_type"), Type);
	}
	return Type;
}

void UBlueprintHelperTaskSpecWorkbenchUtils::ReadCallMatchOptions(
	const TSharedPtr<FJsonObject>& Object,
	FCallStatementDescriptor& Descriptor)
{
	Object->TryGetStringField(TEXT("search_mode"), Descriptor.SearchMode);
	Object->TryGetStringField(TEXT("ambiguity"), Descriptor.AmbiguityPolicy);
	if (Descriptor.AmbiguityPolicy.IsEmpty())
	{
		Object->TryGetStringField(TEXT("ambiguity_policy"), Descriptor.AmbiguityPolicy);
	}
	ReadStringArrayField(Object, TEXT("category_priority"), Descriptor.CategoryPriority);

	const TSharedPtr<FJsonObject>* MatchObject = nullptr;
	if (Object->TryGetObjectField(TEXT("match"), MatchObject) && MatchObject && MatchObject->IsValid())
	{
		if (Descriptor.SearchMode.IsEmpty())
		{
			(*MatchObject)->TryGetStringField(TEXT("mode"), Descriptor.SearchMode);
		}
		if (Descriptor.AmbiguityPolicy.IsEmpty())
		{
			(*MatchObject)->TryGetStringField(TEXT("ambiguity"), Descriptor.AmbiguityPolicy);
		}
		ReadStringArrayField(*MatchObject, TEXT("category_priority"), Descriptor.CategoryPriority);
	}
}

void UBlueprintHelperTaskSpecWorkbenchUtils::ReadCallArguments(
	const TSharedPtr<FJsonObject>& Object,
	FCallStatementDescriptor& Descriptor)
{
	const TSharedPtr<FJsonObject>* ArgsObject = nullptr;
	if (!Object->TryGetObjectField(TEXT("args"), ArgsObject) || !ArgsObject || !ArgsObject->IsValid())
	{
		return;
	}

	for (const auto& Pair : (*ArgsObject)->Values)
	{
		const FString Key = FBlueprintHelperVersionCompat::JsonKeyToString(Pair.Key);
		Descriptor.ArgumentNames.Add(Key);
		const FString Type = InferExpressionType(Pair.Value);
		if (!Type.IsEmpty())
		{
			Descriptor.ArgumentTypes.Add(Key, Type);
		}
	}
}

void UBlueprintHelperTaskSpecWorkbenchUtils::CollectCallStatementsFromObject(
	const TSharedPtr<FJsonObject>& Object,
	const FString& Path,
	TArray<FCallStatementDescriptor>& OutCalls)
{
	if (!Object.IsValid())
	{
		return;
	}

	const FString Kind = ReadStringField(Object, TEXT("kind"));
	if (IsCallStatementKind(Kind))
	{
		FCallStatementDescriptor Descriptor;
		Descriptor.Path = Path;
		Descriptor.Query = ResolveCallQuery(Object, Kind);
		Descriptor.CardId = SanitizeIdSegment(Path) + TEXT("|") + SanitizeIdSegment(Descriptor.Query);
		ReadCallMatchOptions(Object, Descriptor);
		ReadCallArguments(Object, Descriptor);
		if (!Descriptor.Query.IsEmpty())
		{
			OutCalls.Add(MoveTemp(Descriptor));
		}
	}

	for (const auto& Pair : Object->Values)
	{
		const FString Key = FBlueprintHelperVersionCompat::JsonKeyToString(Pair.Key);
		CollectCallStatementsFromValue(Pair.Value, Path + TEXT(".") + Key, OutCalls);
	}
}

void UBlueprintHelperTaskSpecWorkbenchUtils::CollectCallStatementsFromValue(
	const TSharedPtr<FJsonValue>& Value,
	const FString& Path,
	TArray<FCallStatementDescriptor>& OutCalls)
{
	if (!Value.IsValid())
	{
		return;
	}

	if (Value->Type == EJson::Object)
	{
		CollectCallStatementsFromObject(Value->AsObject(), Path, OutCalls);
		return;
	}

	if (Value->Type == EJson::Array)
	{
		const TArray<TSharedPtr<FJsonValue>>& Array = Value->AsArray();
		for (int32 Index = 0; Index < Array.Num(); ++Index)
		{
			CollectCallStatementsFromValue(
				Array[Index],
				FString::Printf(TEXT("%s[%d]"), *Path, Index),
				OutCalls);
		}
	}
}

FBlueprintHelperCallFunctionCandidateRowModel UBlueprintHelperTaskSpecWorkbenchUtils::MakeRowFromCandidateInfo(
	const FBlueprintHelperCallFunctionCandidateInfo& Info,
	int32 Index)
{
	FBlueprintHelperCallFunctionCandidateRowModel Row;
	Row.CandidateId = Info.StableId.IsEmpty()
		? FString::Printf(TEXT("candidate_%d"), Index)
		: Info.StableId;
	Row.StableId = Info.StableId;
	Row.DisplayName = Info.DisplayName.IsEmpty() ? Info.NativeFunctionName : Info.DisplayName;
	Row.NativeFunctionName = Info.NativeFunctionName;
	Row.OwnerClassPath = Info.OwnerClassPath;
	Row.Category = Info.Category;
	Row.MatchReason = Info.MatchReason;
	Row.MismatchReason = Info.MismatchReason;
	Row.Score = Info.Score;
	return Row;
}

FBlueprintHelperCallFunctionCandidateInfo UBlueprintHelperTaskSpecWorkbenchUtils::MakeCandidateInfo(
	const FBlueprintHelperCallFunctionCandidate& Candidate)
{
	FBlueprintHelperCallFunctionCandidateInfo Info;
	Info.StableId = Candidate.StableId;
	Info.DisplayName = Candidate.DisplayName;
	Info.NativeFunctionName = Candidate.NativeFunctionName;
	Info.OwnerClassPath = Candidate.OwnerClassPath;
	Info.Category = Candidate.Category;
	Info.MatchReason = Candidate.MatchReason;
	Info.MismatchReason = Candidate.MismatchReason;
	Info.Score = Candidate.Score;
	Info.ReturnType = Candidate.ReturnType;
	Info.InputPins = Candidate.InputPins;
	Info.InputPinTypes = Candidate.InputPinTypes;
	Info.bGraphCompatible = Candidate.bGraphCompatible;
	Info.bFromActionDatabase = Candidate.bFromActionDatabase;
	Info.bBlueprintCallable = Candidate.bBlueprintCallable;
	Info.bBlueprintPure = Candidate.bBlueprintPure;
	return Info;
}

FString UBlueprintHelperTaskSpecWorkbenchUtils::ResolveStatusText(const FBlueprintHelperCallFunctionResolveResult& Result)
{
	if (!Result.Message.IsEmpty())
	{
		return Result.Message;
	}

	switch (Result.Status)
	{
	case EBlueprintHelperCallFunctionResolveStatus::Resolved:
		return TEXT("resolved");
	case EBlueprintHelperCallFunctionResolveStatus::Ambiguous:
		return TEXT("ambiguous_function_call");
	case EBlueprintHelperCallFunctionResolveStatus::Blocked:
		return TEXT("blocked");
	default:
		return TEXT("function_call_not_found");
	}
}

void UBlueprintHelperTaskSpecWorkbenchUtils::AddPreviewBlock(
	FBlueprintHelperTaskSpecPreviewModel& Model,
	EBlueprintHelperTaskSpecPreviewBlockKind Kind,
	const FString& SourcePath,
	const FString& Title,
	const FString& Detail)
{
	FBlueprintHelperTaskSpecPreviewBlock Block;
	Block.BlockId = SanitizeIdSegment(SourcePath) + TEXT("_") + LexToString(Model.Blocks.Num());
	Block.SourcePath = SourcePath;
	Block.Title = Title.IsEmpty() ? SourcePath : Title;
	Block.Detail = Detail;
	Block.Kind = Kind;
	Model.Blocks.Add(MoveTemp(Block));
}

void UBlueprintHelperTaskSpecWorkbenchUtils::CollectPreviewBlocksFromObject(
	const TSharedPtr<FJsonObject>& Object,
	const FString& Path,
	FBlueprintHelperTaskSpecPreviewModel& Model)
{
	if (!Object.IsValid())
	{
		return;
	}

	const FString Kind = ReadStringField(Object, TEXT("kind"));
	if (IsGraphStatementKind(Kind))
	{
		AddPreviewBlock(
			Model,
			EBlueprintHelperTaskSpecPreviewBlockKind::GraphLogic,
			Path,
			ResolveStatementTitle(Object, Kind),
			ResolveStatementDetail(Object));
	}

	for (const auto& Pair : Object->Values)
	{
		const FString Key = FBlueprintHelperVersionCompat::JsonKeyToString(Pair.Key);
		CollectPreviewBlocksFromValue(Pair.Value, Path + TEXT(".") + Key, Model);
	}
}

void UBlueprintHelperTaskSpecWorkbenchUtils::CollectPreviewBlocksFromValue(
	const TSharedPtr<FJsonValue>& Value,
	const FString& Path,
	FBlueprintHelperTaskSpecPreviewModel& Model)
{
	if (!Value.IsValid())
	{
		return;
	}

	if (Value->Type == EJson::Object)
	{
		CollectPreviewBlocksFromObject(Value->AsObject(), Path, Model);
		return;
	}

	if (Value->Type == EJson::Array)
	{
		const TArray<TSharedPtr<FJsonValue>>& Array = Value->AsArray();
		for (int32 Index = 0; Index < Array.Num(); ++Index)
		{
			CollectPreviewBlocksFromValue(
				Array[Index],
				FString::Printf(TEXT("%s[%d]"), *Path, Index),
				Model);
		}
	}
}

void UBlueprintHelperTaskSpecWorkbenchUtils::AddTopLevelNonGraphBlock(
	const TSharedPtr<FJsonObject>& RootObject,
	const TCHAR* FieldName,
	FBlueprintHelperTaskSpecPreviewModel& Model)
{
	if (!RootObject.IsValid())
	{
		return;
	}

	const TSharedPtr<FJsonValue> FieldValue = FBlueprintHelperVersionCompat::FindJsonValue(RootObject, FieldName);
	if (!FieldValue.IsValid())
	{
		return;
	}

	AddPreviewBlock(
		Model,
		EBlueprintHelperTaskSpecPreviewBlockKind::NonGraphLogic,
		FString::Printf(TEXT("$.%s"), FieldName),
		FieldName,
		JsonValueToDisplayString(FieldValue));
}

void UBlueprintHelperTaskSpecWorkbenchUtils::BuildLogicJsonPayload(
	const TSharedPtr<FJsonObject>& RawJsonRoot,
	TSharedRef<FJsonObject> OutPayload)
{
	const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
	RawJsonRoot->TryGetArrayField(TEXT("nodes"), Nodes);
	RawJsonRoot->TryGetArrayField(TEXT("links"), Links);

	TSharedRef<FJsonObject> LogicObject = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> EntryObject = MakeShared<FJsonObject>();
	EntryObject->SetStringField(TEXT("name"), TEXT("T3DClipboard"));
	EntryObject->SetStringField(TEXT("source"), TEXT("clipboard_t3d"));
	LogicObject->SetObjectField(TEXT("entry"), EntryObject);
	LogicObject->SetArrayField(TEXT("nodes"), Nodes ? *Nodes : TArray<TSharedPtr<FJsonValue>>());
	LogicObject->SetArrayField(TEXT("links"), Links ? *Links : TArray<TSharedPtr<FJsonValue>>());

	TSharedRef<FJsonObject> StatsObject = MakeShared<FJsonObject>();
	StatsObject->SetNumberField(TEXT("node_count"), Nodes ? Nodes->Num() : 0);
	StatsObject->SetNumberField(TEXT("link_count"), Links ? Links->Num() : 0);

	OutPayload->SetStringField(TEXT("schema"), TEXT("ReadContextPack.v1"));
	OutPayload->SetStringField(TEXT("format"), TEXT("logic_json"));
	OutPayload->SetStringField(TEXT("source"), TEXT("t3d_clipboard"));
	OutPayload->SetObjectField(TEXT("logic"), LogicObject);
	OutPayload->SetObjectField(TEXT("stats"), StatsObject);
}

FString UBlueprintHelperTaskSpecWorkbenchUtils::BuildLogicMdFromRawJson(const TSharedPtr<FJsonObject>& RawJsonRoot)
{
	const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
	RawJsonRoot->TryGetArrayField(TEXT("nodes"), Nodes);
	RawJsonRoot->TryGetArrayField(TEXT("links"), Links);

	TArray<FString> Lines;
	Lines.Add(TEXT("# ReadContext LogicMD"));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("- source: t3d_clipboard"));
	Lines.Add(FString::Printf(TEXT("- nodes: %d"), Nodes ? Nodes->Num() : 0));
	Lines.Add(FString::Printf(TEXT("- links: %d"), Links ? Links->Num() : 0));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("## Nodes"));

	if (Nodes)
	{
		for (int32 Index = 0; Index < Nodes->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> NodeObject = (*Nodes)[Index].IsValid()
				? (*Nodes)[Index]->AsObject()
				: nullptr;
			if (!NodeObject.IsValid())
			{
				continue;
			}

			const FString Id = ReadStringField(NodeObject, TEXT("id"));
			const FString Type = ReadStringField(NodeObject, TEXT("type"));
			FString Name = ReadStringField(NodeObject, TEXT("name"));
			if (Name.IsEmpty())
			{
				Name = ReadStringField(NodeObject, TEXT("function_name"));
			}
			Lines.Add(FString::Printf(
				TEXT("- %s | %s | %s"),
				*Id,
				*Type,
				Name.IsEmpty() ? TEXT("(unnamed)") : *Name));
		}
	}

	Lines.Add(TEXT(""));
	Lines.Add(TEXT("## Links"));
	if (Links)
	{
		for (int32 Index = 0; Index < Links->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> LinkObject = (*Links)[Index].IsValid()
				? (*Links)[Index]->AsObject()
				: nullptr;
			if (!LinkObject.IsValid())
			{
				continue;
			}
			Lines.Add(FString::Printf(
				TEXT("- %s.%s -> %s.%s"),
				*ReadStringField(LinkObject, TEXT("from_id")),
				*ReadStringField(LinkObject, TEXT("from_pin")),
				*ReadStringField(LinkObject, TEXT("to_id")),
				*ReadStringField(LinkObject, TEXT("to_pin"))));
		}
	}

	return FString::Join(Lines, TEXT("\n"));
}
