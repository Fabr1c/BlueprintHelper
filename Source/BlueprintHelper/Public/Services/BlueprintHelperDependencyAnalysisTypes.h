// BlueprintHelper Service Layer — Internal Dependency Analysis 内部类型定义
// 不导出独立 Agent-facing MCP 工具簇，仅供 Cleanup/Replace/Remove 等调用方内部使用

#pragma once

#include "CoreMinimal.h"

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
};

struct FBlueprintHelperDependentRefSummary
{
	FString AssetPath;
	FString DependentType; // asset_reference | blueprint_call | interface_call | widget_binding | data_table_row_reference | soft_reference | unknown
};

// ─── Dependencies (target → external assets) ───

struct FBlueprintHelperAssetDependencySummary
{
	int32 DependencyCount = 0;
	TArray<FBlueprintHelperAssetRefSummary> Dependencies;
	bool bPartial = false;
	TArray<FString> UnsupportedChecks;
};

// ─── Referencers (external assets → target) ───

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
