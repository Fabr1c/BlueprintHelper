// BlueprintHelper dependency analysis type helpers.

#include "Shared/BlueprintHelperDependencyAnalysisTypes.h"

TArray<TSharedPtr<FJsonValue>> FBlueprintHelperDependencyAnalysisJson::StringArray(const TArray<FString>& Items)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FString& Item : Items)
	{
		Values.Add(MakeShared<FJsonValueString>(Item));
	}
	return Values;
}

TSharedRef<FJsonObject> FBlueprintHelperReferenceSampleSummary::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!GraphName.IsEmpty())
	{
		Json->SetStringField(TEXT("graph_name"), GraphName);
	}
	Json->SetStringField(TEXT("reference_kind"), ReferenceKind);
	return Json;
}

void FBlueprintHelperReferenceAssetSummary::AddReference(
	const FString& ReferenceKind,
	const FString& GraphName,
	const FString& InSafety,
	bool bIncludeSample,
	int32 MaxSamples)
{
	++MatchCount;
	ReferenceKinds.AddUnique(ReferenceKind.IsEmpty() ? TEXT("unknown") : ReferenceKind);
	EscalateSafety(InSafety);
	if (bIncludeSample && Samples.Num() < MaxSamples)
	{
		FBlueprintHelperReferenceSampleSummary Sample;
		Sample.GraphName = GraphName;
		Sample.ReferenceKind = ReferenceKind.IsEmpty() ? TEXT("unknown") : ReferenceKind;
		Samples.Add(Sample);
	}
}

TSharedRef<FJsonObject> FBlueprintHelperReferenceAssetSummary::ToJson(bool bIncludeSamples) const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("asset_path"), AssetPath);
	Json->SetStringField(TEXT("asset_type"), AssetType);
	Json->SetNumberField(TEXT("match_count"), MatchCount);
	Json->SetArrayField(TEXT("reference_kinds"), FBlueprintHelperDependencyAnalysisJson::StringArray(ReferenceKinds));
	Json->SetStringField(TEXT("safety"), Safety);
	if (bIncludeSamples)
	{
		TArray<TSharedPtr<FJsonValue>> SampleValues;
		for (const FBlueprintHelperReferenceSampleSummary& Sample : Samples)
		{
			SampleValues.Add(MakeShared<FJsonValueObject>(Sample.ToJson()));
		}
		Json->SetArrayField(TEXT("samples"), SampleValues);
	}
	return Json;
}

void FBlueprintHelperReferenceAssetSummary::EscalateSafety(const FString& InSafety)
{
	if (InSafety == TEXT("blocking") || Safety.IsEmpty())
	{
		Safety = TEXT("blocking");
	}
	else if (InSafety == TEXT("warning") && Safety != TEXT("blocking"))
	{
		Safety = TEXT("warning");
	}
	else if (Safety.IsEmpty())
	{
		Safety = TEXT("info");
	}
}

TSharedRef<FJsonObject> FBlueprintHelperReferenceIndexStatus::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("unindexed_count"), UnindexedCount);
	Json->SetNumberField(TEXT("out_of_date_count"), OutOfDateCount);
	Json->SetNumberField(TEXT("failed_count"), FailedCount);
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperReferenceContextSummary::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("asset_count"), AssetCount);
	Json->SetNumberField(TEXT("reference_count"), ReferenceCount);
	Json->SetNumberField(TEXT("blocking_count"), BlockingCount);
	Json->SetNumberField(TEXT("warning_count"), WarningCount);
	Json->SetBoolField(TEXT("partial"), bPartial);
	Json->SetBoolField(TEXT("truncated"), bTruncated);
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperReferenceContextAgentHints::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("can_edit_safely"), bCanEditSafely);
	Json->SetBoolField(TEXT("requires_preview"), bRequiresPreview);
	Json->SetStringField(TEXT("recommended_task_strategy"), RecommendedTaskStrategy);
	Json->SetArrayField(TEXT("blockers"), FBlueprintHelperDependencyAnalysisJson::StringArray(Blockers));
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperReferenceContextPack::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), FBlueprintHelperReferenceContextProtocol::Schema);
	Json->SetStringField(TEXT("context_id"), ContextId);
	Json->SetObjectField(TEXT("summary"), Summary.ToJson());
	Json->SetObjectField(TEXT("index_status"), IndexStatus.ToJson());

	TArray<TSharedPtr<FJsonValue>> DependencyValues;
	for (const FBlueprintHelperReferenceAssetSummary& Dependency : Dependencies)
	{
		DependencyValues.Add(MakeShared<FJsonValueObject>(Dependency.ToJson(false)));
	}
	Json->SetArrayField(TEXT("dependencies"), DependencyValues);

	const bool bIncludeSamples = Referencers.ContainsByPredicate(
		[](const FBlueprintHelperReferenceAssetSummary& Referencer)
		{
			return Referencer.Samples.Num() > 0;
		});
	TArray<TSharedPtr<FJsonValue>> ReferencerValues;
	for (const FBlueprintHelperReferenceAssetSummary& Referencer : Referencers)
	{
		ReferencerValues.Add(MakeShared<FJsonValueObject>(Referencer.ToJson(bIncludeSamples)));
	}
	Json->SetArrayField(TEXT("referencers"), ReferencerValues);
	Json->SetObjectField(TEXT("agent_hints"), AgentHints.ToJson());
	Json->SetArrayField(TEXT("unsupported_checks"), FBlueprintHelperDependencyAnalysisJson::StringArray(UnsupportedChecks));
	return Json;
}
