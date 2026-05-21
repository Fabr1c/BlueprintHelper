// BlueprintHelper TaskSpec / ReadContext workbench services.

#include "Systems/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchServices.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintTextConverter.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

namespace BlueprintHelperTaskSpecWorkbenchServices
{
struct FCallStatementDescriptor
{
	FString CardId;
	FString Path;
	FString Query;
	FString SearchMode;
	FString AmbiguityPolicy;
	TArray<FString> CategoryPriority;
	TArray<FString> ArgumentNames;
	TMap<FString, FString> ArgumentTypes;
};

static FString SanitizeIdSegment(const FString& Value)
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

static bool TryDeserializeJsonObject(
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

static FString SerializeJsonObject(const TSharedPtr<FJsonObject>& Object)
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

static FString JsonValueToDisplayString(const TSharedPtr<FJsonValue>& Value)
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

static FString ReadStringField(
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

static void ReadStringArrayField(
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

static bool IsCallStatementKind(const FString& Kind)
{
	return Kind.Equals(TEXT("call"), ESearchCase::IgnoreCase);
}

static bool IsGraphStatementKind(const FString& Kind)
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

static FString ResolveCallQuery(const TSharedPtr<FJsonObject>& Object, const FString& Kind)
{
	FString Query;
	Object->TryGetStringField(TEXT("target"), Query);
	return Query.TrimStartAndEnd();
}

static FString ResolveStatementTitle(const TSharedPtr<FJsonObject>& Object, const FString& Kind)
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

static FString ResolveStatementDetail(const TSharedPtr<FJsonObject>& Object)
{
	const TSharedPtr<FJsonObject>* ArgsObject = nullptr;
	if (Object.IsValid() && Object->TryGetObjectField(TEXT("args"), ArgsObject) && ArgsObject && ArgsObject->IsValid())
	{
		TArray<FString> Args;
		(*ArgsObject)->Values.GetKeys(Args);
		return FString::Printf(TEXT("args: %s"), *FString::Join(Args, TEXT(", ")));
	}
	return FString();
}

static FString InferExpressionType(const TSharedPtr<FJsonValue>& Value)
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

static void ReadCallMatchOptions(
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

static void ReadCallArguments(
	const TSharedPtr<FJsonObject>& Object,
	FCallStatementDescriptor& Descriptor)
{
	const TSharedPtr<FJsonObject>* ArgsObject = nullptr;
	if (!Object->TryGetObjectField(TEXT("args"), ArgsObject) || !ArgsObject || !ArgsObject->IsValid())
	{
		return;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ArgsObject)->Values)
	{
		Descriptor.ArgumentNames.Add(Pair.Key);
		const FString Type = InferExpressionType(Pair.Value);
		if (!Type.IsEmpty())
		{
			Descriptor.ArgumentTypes.Add(Pair.Key, Type);
		}
	}
}

static void CollectCallStatementsFromValue(
	const TSharedPtr<FJsonValue>& Value,
	const FString& Path,
	TArray<FCallStatementDescriptor>& OutCalls);

static void CollectCallStatementsFromObject(
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

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
	{
		CollectCallStatementsFromValue(Pair.Value, Path + TEXT(".") + Pair.Key, OutCalls);
	}
}

static void CollectCallStatementsFromValue(
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

static FBlueprintHelperCallFunctionCandidateRowModel MakeRowFromCandidateInfo(
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

static FBlueprintHelperCallFunctionCandidateInfo MakeCandidateInfo(
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

static FString ResolveStatusText(const FBlueprintHelperCallFunctionResolveResult& Result)
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

static void AddPreviewBlock(
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

static void CollectPreviewBlocksFromValue(
	const TSharedPtr<FJsonValue>& Value,
	const FString& Path,
	FBlueprintHelperTaskSpecPreviewModel& Model);

static void CollectPreviewBlocksFromObject(
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

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
	{
		CollectPreviewBlocksFromValue(Pair.Value, Path + TEXT(".") + Pair.Key, Model);
	}
}

static void CollectPreviewBlocksFromValue(
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

static void AddTopLevelNonGraphBlock(
	const TSharedPtr<FJsonObject>& RootObject,
	const TCHAR* FieldName,
	FBlueprintHelperTaskSpecPreviewModel& Model)
{
	if (!RootObject.IsValid())
	{
		return;
	}

	const TSharedPtr<FJsonValue>* FieldValue = RootObject->Values.Find(FieldName);
	if (!FieldValue || !FieldValue->IsValid())
	{
		return;
	}

	AddPreviewBlock(
		Model,
		EBlueprintHelperTaskSpecPreviewBlockKind::NonGraphLogic,
		FString::Printf(TEXT("$.%s"), FieldName),
		FieldName,
		JsonValueToDisplayString(*FieldValue));
}

static void BuildLogicJsonPayload(
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
	OutPayload->SetStringField(TEXT("format"), TEXT("logicjson"));
	OutPayload->SetStringField(TEXT("source"), TEXT("t3d_clipboard"));
	OutPayload->SetObjectField(TEXT("logic"), LogicObject);
	OutPayload->SetObjectField(TEXT("stats"), StatsObject);
}

static FString BuildLogicMdFromRawJson(const TSharedPtr<FJsonObject>& RawJsonRoot)
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
}

FBlueprintHelperInputDocument FBlueprintHelperWorkbenchInputClassifier::Classify(const FString& SourceText)
{
	using namespace BlueprintHelperTaskSpecWorkbenchServices;

	FBlueprintHelperInputDocument Document;
	Document.RawText = SourceText;
	const FString Trimmed = SourceText.TrimStartAndEnd();

	if (Trimmed.IsEmpty())
	{
		Document.StatusText = TEXT("Paste TaskSpec JSON or Blueprint T3D text.");
		return Document;
	}

	if (FBlueprintToTextConverter::IsBlueprintT3DText(Trimmed))
	{
		Document.InputType = EBlueprintHelperWorkbenchInputType::T3D;
		Document.bRecognized = true;
		Document.bParseSucceeded = true;
		Document.StatusText = TEXT("Blueprint T3D detected. Export logicmd or logicjson to clipboard.");
		return Document;
	}

	TSharedPtr<FJsonObject> RootObject;
	FString Error;
	if (TryDeserializeJsonObject(Trimmed, RootObject, Error))
	{
		FString Schema;
		RootObject->TryGetStringField(TEXT("schema"), Schema);
		if (Schema.Equals(TEXT("BlueprintHelper.TaskSpec.v1"), ESearchCase::IgnoreCase))
		{
			Document.InputType = EBlueprintHelperWorkbenchInputType::TaskSpec;
			Document.bRecognized = true;
			Document.bParseSucceeded = true;
			Document.StatusText = TEXT("TaskSpec detected.");
			return Document;
		}

		Document.bParseSucceeded = true;
		Document.StatusText = FString::Printf(TEXT("JSON detected, but schema is not BlueprintHelper.TaskSpec.v1: %s"), *Schema);
		return Document;
	}

	Document.StatusText = FString::Printf(TEXT("Input is neither TaskSpec JSON nor Blueprint T3D: %s"), *Error);
	return Document;
}

FBlueprintHelperReadContextExportResult FBlueprintHelperReadContextExportService::Export(
	const FBlueprintHelperReadContextExportRequest& Request)
{
	using namespace BlueprintHelperTaskSpecWorkbenchServices;

	FBlueprintHelperReadContextExportResult Result;
	if (!FBlueprintToTextConverter::IsBlueprintT3DText(Request.SourceText))
	{
		Result.Message = TEXT("ReadContext export requires Blueprint T3D input.");
		return Result;
	}

	const FString RawJsonText = FBlueprintToTextConverter::ConvertTextToJson(Request.SourceText);
	if (RawJsonText.IsEmpty())
	{
		Result.Message = TEXT("T3D conversion failed.");
		return Result;
	}

	TSharedPtr<FJsonObject> RawJsonRoot;
	FString Error;
	if (!TryDeserializeJsonObject(RawJsonText, RawJsonRoot, Error))
	{
		Result.Message = FString::Printf(TEXT("Converted T3D JSON could not be parsed: %s"), *Error);
		return Result;
	}

	if (Request.Format == EBlueprintHelperReadContextExportFormat::LogicJson)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		BuildLogicJsonPayload(RawJsonRoot, Payload);
		Result.ExportText = SerializeJsonObject(Payload);
		Result.bSucceeded = true;
		Result.Message = TEXT("logicjson copied to clipboard.");
		return Result;
	}

	Result.ExportText = BuildLogicMdFromRawJson(RawJsonRoot);
	Result.bSucceeded = true;
	Result.Message = TEXT("logicmd copied to clipboard.");
	return Result;
}

TArray<FBlueprintHelperCallFunctionCardModel>
FBlueprintHelperTaskSpecCallFunctionCandidateCoordinator::BuildCandidateCards(
	const FString& TaskSpecText,
	UEdGraph* ContextGraph,
	FString& OutStatusText)
{
	using namespace BlueprintHelperTaskSpecWorkbenchServices;

	OutStatusText.Reset();
	TArray<FBlueprintHelperCallFunctionCardModel> Cards;

	TSharedPtr<FJsonObject> RootObject;
	FString Error;
	if (!TryDeserializeJsonObject(TaskSpecText, RootObject, Error))
	{
		OutStatusText = FString::Printf(TEXT("TaskSpec parse failed: %s"), *Error);
		return Cards;
	}

	TArray<FCallStatementDescriptor> Calls;
	CollectCallStatementsFromObject(RootObject, TEXT("$"), Calls);
	if (Calls.Num() == 0)
	{
		OutStatusText = TEXT("No CallFunction statements found in TaskSpec.");
		return Cards;
	}

	UBlueprint* Blueprint = ContextGraph ? FBlueprintEditorUtils::FindBlueprintForGraph(ContextGraph) : nullptr;

	for (const FCallStatementDescriptor& Call : Calls)
	{
		FBlueprintHelperCallFunctionResolveRequest ResolveRequest;
		ResolveRequest.Blueprint = Blueprint;
		ResolveRequest.Graph = ContextGraph;
		ResolveRequest.Query = Call.Query;
		ResolveRequest.SearchMode = Call.SearchMode;
		ResolveRequest.AmbiguityPolicy = Call.AmbiguityPolicy.IsEmpty()
			? TEXT("return_candidates")
			: Call.AmbiguityPolicy;
		ResolveRequest.CategoryPriority = Call.CategoryPriority;
		ResolveRequest.ArgumentNames = Call.ArgumentNames;
		ResolveRequest.ArgumentTypes = Call.ArgumentTypes;
		ResolveRequest.MaxCandidates = 8;
		ResolveRequest.Context.Blueprint = Blueprint;
		ResolveRequest.Context.Graph = ContextGraph;
		ResolveRequest.Context.Schema = ContextGraph ? ContextGraph->GetSchema() : nullptr;
		ResolveRequest.Context.SelfClass = Blueprint
			? (Blueprint->GeneratedClass ? Blueprint->GeneratedClass.Get() : Blueprint->SkeletonGeneratedClass.Get())
			: nullptr;
		ResolveRequest.Context.ArgumentNames = Call.ArgumentNames;
		ResolveRequest.Context.ArgumentTypes = Call.ArgumentTypes;

		const FBlueprintHelperCallFunctionResolveResult ResolveResult =
			FBlueprintHelperCallFunctionResolver::Resolve(ResolveRequest);

		FBlueprintHelperCallFunctionCardModel Card;
		Card.CardId = Call.CardId;
		Card.SourcePath = Call.Path;
		Card.Query = Call.Query;
		Card.StatusText = ResolveStatusText(ResolveResult);

		TArray<FBlueprintHelperCallFunctionCandidateInfo> CandidateInfos = ResolveResult.CandidateFunctions;
		if (CandidateInfos.Num() == 0)
		{
			for (const FBlueprintHelperCallFunctionCandidate& Candidate : ResolveResult.Candidates)
			{
				CandidateInfos.Add(MakeCandidateInfo(Candidate));
			}
		}
		if (CandidateInfos.Num() == 0 && ResolveResult.IsResolved())
		{
			CandidateInfos.Add(MakeCandidateInfo(ResolveResult.Selected));
		}

		for (int32 Index = 0; Index < CandidateInfos.Num(); ++Index)
		{
			Card.Candidates.Add(MakeRowFromCandidateInfo(CandidateInfos[Index], Index));
		}

		Cards.Add(MoveTemp(Card));
	}

	OutStatusText = FString::Printf(TEXT("CallFunction cards: %d"), Cards.Num());
	return Cards;
}

FBlueprintHelperTaskSpecPreviewModel FBlueprintHelperTaskSpecPreviewModelBuilder::BuildPreviewModel(
	const FBlueprintHelperInputDocument& InputDocument)
{
	using namespace BlueprintHelperTaskSpecWorkbenchServices;

	FBlueprintHelperTaskSpecPreviewModel Model;

	if (InputDocument.InputType != EBlueprintHelperWorkbenchInputType::TaskSpec)
	{
		AddPreviewBlock(
			Model,
			EBlueprintHelperTaskSpecPreviewBlockKind::Diagnostic,
			TEXT("$"),
			TEXT("TaskSpec preview unavailable"),
			InputDocument.StatusText);
		FBlueprintHelperTaskSpecPreviewLayoutCoordinator::ApplyLayout(Model);
		return Model;
	}

	TSharedPtr<FJsonObject> RootObject;
	FString Error;
	if (!TryDeserializeJsonObject(InputDocument.RawText, RootObject, Error))
	{
		AddPreviewBlock(
			Model,
			EBlueprintHelperTaskSpecPreviewBlockKind::Diagnostic,
			TEXT("$"),
			TEXT("TaskSpec parse error"),
			Error);
		FBlueprintHelperTaskSpecPreviewLayoutCoordinator::ApplyLayout(Model);
		return Model;
	}

	AddTopLevelNonGraphBlock(RootObject, TEXT("target"), Model);
	AddTopLevelNonGraphBlock(RootObject, TEXT("scope_policy"), Model);
	AddTopLevelNonGraphBlock(RootObject, TEXT("execution_policy"), Model);
	AddTopLevelNonGraphBlock(RootObject, TEXT("validation"), Model);
	AddTopLevelNonGraphBlock(RootObject, TEXT("components"), Model);
	AddTopLevelNonGraphBlock(RootObject, TEXT("variables"), Model);
	AddTopLevelNonGraphBlock(RootObject, TEXT("class_settings"), Model);
	AddTopLevelNonGraphBlock(RootObject, TEXT("integration"), Model);

	const TSharedPtr<FJsonValue>* BehaviorValue = RootObject->Values.Find(TEXT("behavior"));
	if (BehaviorValue && BehaviorValue->IsValid())
	{
		const TSharedPtr<FJsonObject> BehaviorObject = (*BehaviorValue)->AsObject();
		FString Strategy = BehaviorObject.IsValid()
			? ReadStringField(BehaviorObject, TEXT("graph_strategy"))
			: FString();
		AddPreviewBlock(
			Model,
			EBlueprintHelperTaskSpecPreviewBlockKind::GraphLogic,
			TEXT("$.behavior"),
			TEXT("behavior"),
			Strategy);
		CollectPreviewBlocksFromValue(*BehaviorValue, TEXT("$.behavior"), Model);
	}

	if (Model.Blocks.Num() == 0)
	{
		AddPreviewBlock(
			Model,
			EBlueprintHelperTaskSpecPreviewBlockKind::Diagnostic,
			TEXT("$"),
			TEXT("Empty TaskSpec preview"),
			TEXT("No recognized preview fields."));
	}

	FBlueprintHelperTaskSpecPreviewLayoutCoordinator::ApplyLayout(Model);
	return Model;
}

void FBlueprintHelperTaskSpecPreviewLayoutCoordinator::ApplyLayout(
	FBlueprintHelperTaskSpecPreviewModel& Model)
{
	int32 NonGraphRow = 0;
	int32 GraphIndex = 0;
	for (FBlueprintHelperTaskSpecPreviewBlock& Block : Model.Blocks)
	{
		if (Block.Kind == EBlueprintHelperTaskSpecPreviewBlockKind::NonGraphLogic
			|| Block.Kind == EBlueprintHelperTaskSpecPreviewBlockKind::Diagnostic)
		{
			Block.Column = 0;
			Block.Row = NonGraphRow++;
			continue;
		}

		Block.Column = 1 + (GraphIndex % 3);
		Block.Row = GraphIndex / 3;
		++GraphIndex;
	}

	Model.Revision++;
	Model.StatusText = FString::Printf(TEXT("preview_blocks=%d graph_blocks=%d"), Model.Blocks.Num(), GraphIndex);
}
