// BlueprintHelper TaskSpec / ReadContext workbench services.

#include "Systems/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchServices.h"
#include "Systems/TaskSpecWorkbench/Utils/BlueprintHelperTaskSpecWorkbenchUtils.h"

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

FBlueprintHelperInputDocument FBlueprintHelperWorkbenchInputClassifier::Classify(const FString& SourceText)
{
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
		Document.StatusText = TEXT("Blueprint T3D detected. Export logicflow, logicmd, or logicjson to clipboard.");
		return Document;
	}

	TSharedPtr<FJsonObject> RootObject;
	FString Error;
	if (UBlueprintHelperTaskSpecWorkbenchUtils::TryDeserializeJsonObject(Trimmed, RootObject, Error))
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
	if (!UBlueprintHelperTaskSpecWorkbenchUtils::TryDeserializeJsonObject(RawJsonText, RawJsonRoot, Error))
	{
		Result.Message = FString::Printf(TEXT("Converted T3D JSON could not be parsed: %s"), *Error);
		return Result;
	}

	if (Request.Format == EBlueprintHelperReadContextExportFormat::LogicFlow)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		UBlueprintHelperTaskSpecWorkbenchUtils::BuildLogicFlowPayload(RawJsonRoot, Payload);
		Result.ExportText = UBlueprintHelperTaskSpecWorkbenchUtils::SerializeJsonObject(Payload);
		Result.bSucceeded = true;
		Result.Message = TEXT("logicflow copied to clipboard.");
		return Result;
	}

	if (Request.Format == EBlueprintHelperReadContextExportFormat::LogicJson)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		UBlueprintHelperTaskSpecWorkbenchUtils::BuildLogicJsonPayload(RawJsonRoot, Payload);
		Result.ExportText = UBlueprintHelperTaskSpecWorkbenchUtils::SerializeJsonObject(Payload);
		Result.bSucceeded = true;
		Result.Message = TEXT("logicjson copied to clipboard.");
		return Result;
	}

	Result.ExportText = UBlueprintHelperTaskSpecWorkbenchUtils::BuildLogicMdFromRawJson(RawJsonRoot);
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
	OutStatusText.Reset();
	TArray<FBlueprintHelperCallFunctionCardModel> Cards;

	TSharedPtr<FJsonObject> RootObject;
	FString Error;
	if (!UBlueprintHelperTaskSpecWorkbenchUtils::TryDeserializeJsonObject(TaskSpecText, RootObject, Error))
	{
		OutStatusText = FString::Printf(TEXT("TaskSpec parse failed: %s"), *Error);
		return Cards;
	}

	TArray<FCallStatementDescriptor> Calls;
	UBlueprintHelperTaskSpecWorkbenchUtils::CollectCallStatementsFromObject(RootObject, TEXT("$"), Calls);
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
		Card.StatusText = UBlueprintHelperTaskSpecWorkbenchUtils::ResolveStatusText(ResolveResult);

		TArray<FBlueprintHelperCallFunctionCandidateInfo> CandidateInfos = ResolveResult.CandidateFunctions;
		if (CandidateInfos.Num() == 0)
		{
			for (const FBlueprintHelperCallFunctionCandidate& Candidate : ResolveResult.Candidates)
			{
				CandidateInfos.Add(UBlueprintHelperTaskSpecWorkbenchUtils::MakeCandidateInfo(Candidate));
			}
		}
		if (CandidateInfos.Num() == 0 && ResolveResult.IsResolved())
		{
			CandidateInfos.Add(UBlueprintHelperTaskSpecWorkbenchUtils::MakeCandidateInfo(ResolveResult.Selected));
		}

		for (int32 Index = 0; Index < CandidateInfos.Num(); ++Index)
		{
			Card.Candidates.Add(UBlueprintHelperTaskSpecWorkbenchUtils::MakeRowFromCandidateInfo(CandidateInfos[Index], Index));
		}

		Cards.Add(MoveTemp(Card));
	}

	OutStatusText = FString::Printf(TEXT("CallFunction cards: %d"), Cards.Num());
	return Cards;
}

FBlueprintHelperTaskSpecPreviewModel FBlueprintHelperTaskSpecPreviewModelBuilder::BuildPreviewModel(
	const FBlueprintHelperInputDocument& InputDocument)
{
	FBlueprintHelperTaskSpecPreviewModel Model;

	if (InputDocument.InputType != EBlueprintHelperWorkbenchInputType::TaskSpec)
	{
		UBlueprintHelperTaskSpecWorkbenchUtils::AddPreviewBlock(
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
	if (!UBlueprintHelperTaskSpecWorkbenchUtils::TryDeserializeJsonObject(InputDocument.RawText, RootObject, Error))
	{
		UBlueprintHelperTaskSpecWorkbenchUtils::AddPreviewBlock(
			Model,
			EBlueprintHelperTaskSpecPreviewBlockKind::Diagnostic,
			TEXT("$"),
			TEXT("TaskSpec parse error"),
			Error);
		FBlueprintHelperTaskSpecPreviewLayoutCoordinator::ApplyLayout(Model);
		return Model;
	}

	UBlueprintHelperTaskSpecWorkbenchUtils::AddTopLevelNonGraphBlock(RootObject, TEXT("target"), Model);
	UBlueprintHelperTaskSpecWorkbenchUtils::AddTopLevelNonGraphBlock(RootObject, TEXT("scope_policy"), Model);
	UBlueprintHelperTaskSpecWorkbenchUtils::AddTopLevelNonGraphBlock(RootObject, TEXT("execution_policy"), Model);
	UBlueprintHelperTaskSpecWorkbenchUtils::AddTopLevelNonGraphBlock(RootObject, TEXT("validation"), Model);
	UBlueprintHelperTaskSpecWorkbenchUtils::AddTopLevelNonGraphBlock(RootObject, TEXT("components"), Model);
	UBlueprintHelperTaskSpecWorkbenchUtils::AddTopLevelNonGraphBlock(RootObject, TEXT("variables"), Model);
	UBlueprintHelperTaskSpecWorkbenchUtils::AddTopLevelNonGraphBlock(RootObject, TEXT("class_settings"), Model);
	UBlueprintHelperTaskSpecWorkbenchUtils::AddTopLevelNonGraphBlock(RootObject, TEXT("integration"), Model);

	const TSharedPtr<FJsonValue>* BehaviorValue = RootObject->Values.Find(TEXT("behavior"));
	if (BehaviorValue && BehaviorValue->IsValid())
	{
		const TSharedPtr<FJsonObject> BehaviorObject = (*BehaviorValue)->AsObject();
		FString Strategy = BehaviorObject.IsValid()
			? UBlueprintHelperTaskSpecWorkbenchUtils::ReadStringField(BehaviorObject, TEXT("graph_strategy"))
			: FString();
		UBlueprintHelperTaskSpecWorkbenchUtils::AddPreviewBlock(
			Model,
			EBlueprintHelperTaskSpecPreviewBlockKind::GraphLogic,
			TEXT("$.behavior"),
			TEXT("behavior"),
			Strategy);
		UBlueprintHelperTaskSpecWorkbenchUtils::CollectPreviewBlocksFromValue(*BehaviorValue, TEXT("$.behavior"), Model);
	}

	if (Model.Blocks.Num() == 0)
	{
		UBlueprintHelperTaskSpecWorkbenchUtils::AddPreviewBlock(
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
