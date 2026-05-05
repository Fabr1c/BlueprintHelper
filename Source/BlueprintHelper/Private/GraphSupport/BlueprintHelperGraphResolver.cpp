// BlueprintHelper Service Layer — 图表定位器实现

#include "GraphSupport/BlueprintHelperGraphResolver.h"
#include "BlueprintEditor.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "TextToBlueprintGenerator.h"

UEdGraph* FBlueprintHelperGraphResolver::ResolveGraph(const FBlueprintHelperGraphTarget& Target, FBlueprintHelperDiagnosticSet& OutDiag) const
{
	UBlueprint* Blueprint = ResolveBlueprint(Target, OutDiag);
	if (!Blueprint)
	{
		return nullptr;
	}

	// 图表名为空时走降级路径
	if (Target.GraphName.IsEmpty())
	{
		// 显式蓝图路径 → 默认查找 EventGraph
		if (!Target.BlueprintPath.IsEmpty())
		{
			UEdGraph* EventGraph = TextToBlueprintGenerator::FindGraphByName(Blueprint, TEXT("EventGraph"));
			if (EventGraph)
			{
				return EventGraph;
			}
			OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error, TEXT("蓝图中未找到 EventGraph。"));
			return nullptr;
		}

		// 无路径无图表名 → 当前焦点图表
		UEdGraph* FocusedGraph = GetFocusedGraph();
		if (FocusedGraph)
		{
			return FocusedGraph;
		}
		OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error, TEXT("未找到当前焦点蓝图图表。"));
		return nullptr;
	}

	// 显式图表名查找
	return FindGraph(Blueprint, Target.GraphName, OutDiag);
}

UBlueprint* FBlueprintHelperGraphResolver::ResolveBlueprint(const FBlueprintHelperGraphTarget& Target, FBlueprintHelperDiagnosticSet& OutDiag) const
{
	if (Target.BlueprintPath.IsEmpty())
	{
		UBlueprint* Focused = GetFocusedBlueprint();
		if (!Focused)
		{
			OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error, TEXT("未指定蓝图路径且未找到当前焦点蓝图。"));
		}
		return Focused;
	}

	return LoadBlueprintByPath(Target.BlueprintPath, OutDiag);
}

UEdGraph* FBlueprintHelperGraphResolver::GetFocusedGraph() const
{
	if (!GEditor)
	{
		return nullptr;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		return nullptr;
	}

	const TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
	for (UObject* Asset : EditedAssets)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
		if (!Blueprint)
		{
			continue;
		}

		IAssetEditorInstance* EditorInstance = AssetEditorSubsystem->FindEditorForAsset(Blueprint, false);
		if (!EditorInstance || EditorInstance->GetEditorName() != TEXT("BlueprintEditor"))
		{
			continue;
		}

		FBlueprintEditor* BlueprintEditor = static_cast<FBlueprintEditor*>(EditorInstance);
		if (BlueprintEditor)
		{
			if (UEdGraph* FocusedGraph = BlueprintEditor->GetFocusedGraph())
			{
				return FocusedGraph;
			}
		}
	}

	return nullptr;
}

UBlueprint* FBlueprintHelperGraphResolver::GetFocusedBlueprint() const
{
	if (!GEditor)
	{
		return nullptr;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		return nullptr;
	}

	const TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
	for (UObject* Asset : EditedAssets)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
		if (!Blueprint)
		{
			continue;
		}

		IAssetEditorInstance* EditorInstance = AssetEditorSubsystem->FindEditorForAsset(Blueprint, false);
		if (!EditorInstance || EditorInstance->GetEditorName() != TEXT("BlueprintEditor"))
		{
			continue;
		}

		return Blueprint;
	}

	return nullptr;
}

UBlueprint* FBlueprintHelperGraphResolver::LoadBlueprintByPath(const FString& AssetPath, FBlueprintHelperDiagnosticSet& OutDiag) const
{
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error,
			FString::Printf(TEXT("无法加载蓝图资产：%s"), *AssetPath));
		return nullptr;
	}

	// 确保编辑器已打开（蓝图操作需要编辑器上下文）
	EnsureBlueprintEditorOpen(Blueprint, OutDiag);
	return Blueprint;
}

FBlueprintEditor* FBlueprintHelperGraphResolver::EnsureBlueprintEditorOpen(UBlueprint* Blueprint, FBlueprintHelperDiagnosticSet& OutDiag) const
{
	if (!GEditor || !Blueprint)
	{
		return nullptr;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error, TEXT("无法获取 AssetEditorSubsystem。"));
		return nullptr;
	}

	// 先检查是否已打开
	IAssetEditorInstance* ExistingEditor = AssetEditorSubsystem->FindEditorForAsset(Blueprint, false);
	if (!ExistingEditor)
	{
		AssetEditorSubsystem->OpenEditorForAsset(Blueprint);
		ExistingEditor = AssetEditorSubsystem->FindEditorForAsset(Blueprint, false);
	}

	if (!ExistingEditor || ExistingEditor->GetEditorName() != TEXT("BlueprintEditor"))
	{
		OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Warning, TEXT("蓝图编辑器未打开或不是 BlueprintEditor 类型。"));
		return nullptr;
	}

	return static_cast<FBlueprintEditor*>(ExistingEditor);
}

UEdGraph* FBlueprintHelperGraphResolver::FindGraph(UBlueprint* Blueprint, const FString& GraphName, FBlueprintHelperDiagnosticSet& OutDiag) const
{
	UEdGraph* Graph = TextToBlueprintGenerator::FindGraphByName(Blueprint, GraphName);
	if (!Graph)
	{
		TArray<FString> AvailableGraphs;
		if (Blueprint)
		{
			for (UEdGraph* Candidate : Blueprint->UbergraphPages)
			{
				if (Candidate)
				{
					AvailableGraphs.Add(Candidate->GetName());
				}
			}
			for (UEdGraph* Candidate : Blueprint->FunctionGraphs)
			{
				if (Candidate)
				{
					AvailableGraphs.Add(Candidate->GetName());
				}
			}
			for (UEdGraph* Candidate : Blueprint->MacroGraphs)
			{
				if (Candidate)
				{
					AvailableGraphs.Add(Candidate->GetName());
				}
			}
		}

		OutDiag.Add(EBlueprintHelperDiagnosticSeverity::Error,
			FString::Printf(TEXT("蓝图 %s 中未找到图表 '%s'。可用图表：%s"),
				Blueprint ? *Blueprint->GetName() : TEXT("<null>"),
				*GraphName,
				*FString::Join(AvailableGraphs, TEXT(", "))),
			TEXT(""),
			TEXT("graph_not_found"),
			TEXT("target_graph"));
	}
	return Graph;
}
