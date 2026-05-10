// BlueprintHelper Service Layer �?Internal Dependency Analysis 内部类型定义
// 不导出独�?Agent-facing MCP 工具簇，仅供 Cleanup/Replace/Remove 等调用方内部使用

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class FBlueprintHelperReferenceContextProtocol
{
public:
	static constexpr const TCHAR* Schema = TEXT("BlueprintHelper.ReferenceContextPack.v1");
};

class FBlueprintHelperDependencyAnalysisJson
{
public:
	static TArray<TSharedPtr<FJsonValue>> StringArray(const TArray<FString>& Items)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FString& Item : Items)
		{
			Values.Add(MakeShared<FJsonValueString>(Item));
		}
		return Values;
	}
};

// ─── 内部目标描述 ───

struct FBlueprintHelperDependencyAnalysisTarget
{
	FString AssetPath;
	FString TargetType; // asset | function | event | custom_event | block | widget | data_table_row | interface
	FString TargetName, BlockId, GraphName, RowName, WidgetName, InterfacePath;
};

// ─── 内部 Options ───

struct FBlueprintHelperDependencyAnalysisOptions
{
	bool bIncludeHardReferences = true;
	bool bIncludeSoftReferences = true;
	bool bAnalyzeBlueprintCalls = true;
	bool bAnalyzeWidgetBindings = true;
	bool bAnalyzeDataTableRows = true;
	bool bScanCppSource = false;
	bool bAnalyzeRuntimeStringLookup = false;
	bool bAnalyzeDynamicSoftReferences = false;
	int32 MaxResultCount = 100;
};

// ─── 内部 ref 摘要 ───

struct FBlueprintHelperAssetRefSummary
{
	FString AssetPath, AssetType;
	FString ReferenceKind = TEXT("package");
	FString Source = TEXT("asset_registry");
	FString EvidencePath;
	FString Confidence = TEXT("high");

	TSharedRef<FJsonObject> ToDependencyJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("asset_path"), AssetPath);
		Json->SetStringField(TEXT("asset_type"), AssetType);
		Json->SetStringField(TEXT("reference_kind"), ReferenceKind);
		Json->SetStringField(TEXT("source"), Source);
		Json->SetStringField(TEXT("confidence"), Confidence);
		return Json;
	}

	TSharedRef<FJsonObject> ToReferencerJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("asset_path"), AssetPath);
		Json->SetStringField(TEXT("asset_type"), AssetType);
		Json->SetStringField(TEXT("reference_kind"), ReferenceKind);
		Json->SetStringField(TEXT("evidence_path"), EvidencePath.IsEmpty() ? AssetPath : EvidencePath);
		Json->SetStringField(TEXT("confidence"), Confidence);
		return Json;
	}
};

struct FBlueprintHelperDependentRefSummary
{
	FString AssetPath;
	FString DependentType; // asset_reference | blueprint_call | interface_call | widget_binding | data_table_row_reference | soft_reference | unknown
	FString AssetType;
	FString GraphName;
	FString MemberName;
	FString WidgetName;
	FString RowName;
	FString Impact;
	FString Safety;
	FString Evidence;
	FString SuggestedAction;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("dependent_type"), DependentType);
		if (!AssetPath.IsEmpty()) Json->SetStringField(TEXT("asset_path"), AssetPath);
		if (!AssetType.IsEmpty()) Json->SetStringField(TEXT("asset_type"), AssetType);
		if (!GraphName.IsEmpty()) Json->SetStringField(TEXT("graph_name"), GraphName);
		if (!MemberName.IsEmpty()) Json->SetStringField(TEXT("member_name"), MemberName);
		if (!WidgetName.IsEmpty()) Json->SetStringField(TEXT("widget_name"), WidgetName);
		if (!RowName.IsEmpty()) Json->SetStringField(TEXT("row_name"), RowName);
		if (!Impact.IsEmpty()) Json->SetStringField(TEXT("impact"), Impact);
		if (!Safety.IsEmpty()) Json->SetStringField(TEXT("safety"), Safety);
		if (!Evidence.IsEmpty()) Json->SetStringField(TEXT("evidence"), Evidence);
		if (!SuggestedAction.IsEmpty()) Json->SetStringField(TEXT("suggested_action"), SuggestedAction);
		return Json;
	}
};

// ─── Dependencies (target �?external assets) ───

struct FBlueprintHelperAssetDependencySummary
{
	int32 DependencyCount = 0;
	TArray<FBlueprintHelperAssetRefSummary> Dependencies;
	bool bPartial = false;
	TArray<FString> UnsupportedChecks;
};

// ─── Referencers (external assets �?target) ───

struct FBlueprintHelperAssetReferencerSummary
{
	int32 ReferencerCount = 0;
	TArray<FBlueprintHelperAssetRefSummary> Referencers;
	bool bPartial = false;
	TArray<FString> UnsupportedChecks;
};

// ─── Logical External Dependents ───

struct FBlueprintHelperExternalDependentSummary
{
	bool bHasExternalDependents = false;
	int32 ExternalDependentCount = 0;
	TArray<FBlueprintHelperDependentRefSummary> Dependents;
	bool bPartial = false;
	TArray<FString> UnsupportedChecks;
};

struct FBlueprintHelperReferenceContextAnalysis
{
	FString Scope = TEXT("safety_context");
	bool bPartial = false;
	bool bTruncated = false;
	int32 MaxResults = 50;
	TArray<FString> UnsupportedChecks;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("scope"), Scope);
		Json->SetBoolField(TEXT("partial"), bPartial);
		Json->SetBoolField(TEXT("truncated"), bTruncated);
		Json->SetNumberField(TEXT("max_results"), MaxResults);
		Json->SetArrayField(TEXT("unsupported_checks"), FBlueprintHelperDependencyAnalysisJson::StringArray(UnsupportedChecks));
		return Json;
	}
};

struct FBlueprintHelperReferenceContextSummary
{
	int32 DependencyCount = 0;
	int32 ReferencerCount = 0;
	int32 ExternalDependentCount = 0;
	int32 BlockingDependentCount = 0;
	int32 WarningCount = 0;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("dependency_count"), DependencyCount);
		Json->SetNumberField(TEXT("referencer_count"), ReferencerCount);
		Json->SetNumberField(TEXT("external_dependent_count"), ExternalDependentCount);
		Json->SetNumberField(TEXT("blocking_dependent_count"), BlockingDependentCount);
		Json->SetNumberField(TEXT("warning_count"), WarningCount);
		return Json;
	}
};

struct FBlueprintHelperReferenceContextAgentHints
{
	bool bCanEditSafely = true;
	bool bRequiresPreview = true;
	FString RecommendedTaskStrategy = TEXT("preview_before_write");
	TArray<FString> Blockers;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("can_edit_safely"), bCanEditSafely);
		Json->SetBoolField(TEXT("requires_preview"), bRequiresPreview);
		Json->SetStringField(TEXT("recommended_task_strategy"), RecommendedTaskStrategy);
		Json->SetArrayField(TEXT("blockers"), FBlueprintHelperDependencyAnalysisJson::StringArray(Blockers));
		return Json;
	}
};

struct FBlueprintHelperReferenceContextPack
{
	FString ContextId;
	FBlueprintHelperReferenceContextAnalysis Analysis;
	FBlueprintHelperReferenceContextSummary Summary;
	TArray<FBlueprintHelperAssetRefSummary> Dependencies;
	TArray<FBlueprintHelperAssetRefSummary> Referencers;
	TArray<FBlueprintHelperDependentRefSummary> ExternalDependents;
	FBlueprintHelperReferenceContextAgentHints AgentHints;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), FBlueprintHelperReferenceContextProtocol::Schema);
		Json->SetStringField(TEXT("context_id"), ContextId);
		Json->SetObjectField(TEXT("analysis"), Analysis.ToJson());
		Json->SetObjectField(TEXT("summary"), Summary.ToJson());

		TArray<TSharedPtr<FJsonValue>> DependencyValues;
		for (const FBlueprintHelperAssetRefSummary& Dependency : Dependencies)
		{
			DependencyValues.Add(MakeShared<FJsonValueObject>(Dependency.ToDependencyJson()));
		}
		Json->SetArrayField(TEXT("dependencies"), DependencyValues);

		TArray<TSharedPtr<FJsonValue>> ReferencerValues;
		for (const FBlueprintHelperAssetRefSummary& Referencer : Referencers)
		{
			ReferencerValues.Add(MakeShared<FJsonValueObject>(Referencer.ToReferencerJson()));
		}
		Json->SetArrayField(TEXT("referencers"), ReferencerValues);

		TArray<TSharedPtr<FJsonValue>> ExternalDependentValues;
		for (const FBlueprintHelperDependentRefSummary& Dependent : ExternalDependents)
		{
			ExternalDependentValues.Add(MakeShared<FJsonValueObject>(Dependent.ToJson()));
		}
		Json->SetArrayField(TEXT("external_dependents"), ExternalDependentValues);
		Json->SetObjectField(TEXT("agent_hints"), AgentHints.ToJson());
		return Json;
	}
};
