// BlueprintHelper TaskSpec / ReadContext workbench services.

#pragma once

#include "CoreMinimal.h"
#include "UI/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchData.h"

class UEdGraph;

struct BLUEPRINTHELPER_API FBlueprintHelperReadContextExportRequest
{
	FString SourceText;
	EBlueprintHelperReadContextExportFormat Format =
		EBlueprintHelperReadContextExportFormat::LogicMd;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReadContextExportResult
{
	bool bSucceeded = false;
	FString ExportText;
	FString Message;
};

class BLUEPRINTHELPER_API FBlueprintHelperWorkbenchInputClassifier
{
public:
	static FBlueprintHelperInputDocument Classify(const FString& SourceText);
};

class BLUEPRINTHELPER_API FBlueprintHelperReadContextExportService
{
public:
	static FBlueprintHelperReadContextExportResult Export(const FBlueprintHelperReadContextExportRequest& Request);
};

class BLUEPRINTHELPER_API FBlueprintHelperTaskSpecCallFunctionCandidateCoordinator
{
public:
	static TArray<FBlueprintHelperCallFunctionCardModel> BuildCandidateCards(
		const FString& TaskSpecText,
		UEdGraph* ContextGraph,
		FString& OutStatusText);
};

class BLUEPRINTHELPER_API FBlueprintHelperTaskSpecPreviewModelBuilder
{
public:
	static FBlueprintHelperTaskSpecPreviewModel BuildPreviewModel(
		const FBlueprintHelperInputDocument& InputDocument);
};

class BLUEPRINTHELPER_API FBlueprintHelperTaskSpecPreviewLayoutCoordinator
{
public:
	static void ApplyLayout(FBlueprintHelperTaskSpecPreviewModel& Model);
};
