// BlueprintHelper Service Layer - external dependents analysis service.

#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalDependentsAnalysisService.h"

#include "EdGraph/EdGraph.h"

namespace BlueprintHelperExternalDependentsAnalysis
{
	static TArray<TSharedPtr<FJsonValue>> LinkArrayToJson(
		const TArray<FBlueprintHelperExternalBodyLink>& Links)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		for (const FBlueprintHelperExternalBodyLink& Link : Links)
		{
			Array.Add(MakeShared<FJsonValueObject>(Link.ToJson()));
		}
		return Array;
	}
}

TSharedRef<FJsonObject> FBlueprintHelperExternalDependentsAnalysis::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("supported"), bSupported);
	Json->SetArrayField(
		TEXT("unsupported_dependents"),
		BlueprintHelperExternalDependentsAnalysis::LinkArrayToJson(UnsupportedDependents));
	return Json;
}

bool FBlueprintHelperExternalDependentsAnalysisService::Analyze(
	UEdGraph* Graph,
	const FBlueprintHelperExternalBodySnapshot& Snapshot,
	FBlueprintHelperExternalDependentsAnalysis& OutAnalysis,
	FString& OutError) const
{
	OutAnalysis = FBlueprintHelperExternalDependentsAnalysis();
	OutError.Reset();
	if (!Graph)
	{
		OutError = TEXT("target_graph_not_found");
		return false;
	}

	OutAnalysis.UnsupportedDependents = Snapshot.BodyToExternalLinks;
	OutAnalysis.bSupported = OutAnalysis.UnsupportedDependents.Num() == 0;
	return true;
}
