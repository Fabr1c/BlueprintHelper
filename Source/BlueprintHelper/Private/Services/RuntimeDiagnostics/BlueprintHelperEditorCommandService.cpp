// BlueprintHelper Service Layer 。通用编辑器命令服务实。

#include "Services/RuntimeDiagnostics/BlueprintHelperEditorCommandService.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "FileHelpers.h"
#include "Misc/StringFormatArg.h"

// ═══════════════════════════════════════════════════════════
// Undo / Redo
// ═══════════════════════════════════════════════════════════

FBlueprintHelperCommandResult FBlueprintHelperEditorCommandService::Undo() const
{
	FBlueprintHelperCommandResult Result;

	if (!GEditor)
	{
		Result.ErrorMessage = TEXT("GEditor 不可用。");
		return Result;
	}

	if (GEditor->IsTransactionActive())
	{
		Result.ErrorMessage = TEXT("当前有活跃事务，无法撤销。");
		return Result;
	}

	const bool bUndone = GEditor->UndoTransaction();
	if (!bUndone)
	{
		Result.ErrorMessage = TEXT("没有可撤销的操作。");
		return Result;
	}

	Result.bSuccess = true;
	Result.Message = TEXT("撤销成功。");
	return Result;
}

FBlueprintHelperCommandResult FBlueprintHelperEditorCommandService::Redo() const
{
	FBlueprintHelperCommandResult Result;

	if (!GEditor)
	{
		Result.ErrorMessage = TEXT("GEditor 不可用。");
		return Result;
	}

	if (GEditor->IsTransactionActive())
	{
		Result.ErrorMessage = TEXT("当前有活跃事务，无法重做。");
		return Result;
	}

	const bool bRedone = GEditor->RedoTransaction();
	if (!bRedone)
	{
		Result.ErrorMessage = TEXT("没有可重做的操作。");
		return Result;
	}

	Result.bSuccess = true;
	Result.Message = TEXT("重做成功。");
	return Result;
}

// ═══════════════════════════════════════════════════════════
// PIE 控制
// ═══════════════════════════════════════════════════════════

FBlueprintHelperCommandResult FBlueprintHelperEditorCommandService::PlayInEditor() const
{
	FBlueprintHelperCommandResult Result;

	if (!GEditor)
	{
		Result.ErrorMessage = TEXT("GEditor 不可用。");
		return Result;
	}

	if (GEditor->IsPlaySessionInProgress())
	{
		Result.ErrorMessage = TEXT("PIE 会话已在运行中。");
		return Result;
	}

	FRequestPlaySessionParams Params;
	GEditor->RequestPlaySession(Params);

	Result.bSuccess = true;
	Result.Message = TEXT("PIE 会话启动请求已发送（将在下一帧启动）。");
	return Result;
}

FBlueprintHelperCommandResult FBlueprintHelperEditorCommandService::StopPIE() const
{
	FBlueprintHelperCommandResult Result;

	if (!GEditor)
	{
		Result.ErrorMessage = TEXT("GEditor 不可用。");
		return Result;
	}

	if (!GEditor->IsPlayingSessionInEditor())
	{
		Result.ErrorMessage = TEXT("当前没有运行中的 PIE 会话。");
		return Result;
	}

	GEditor->RequestEndPlayMap();

	Result.bSuccess = true;
	Result.Message = TEXT("PIE 停止请求已发送。");
	return Result;
}

// ═══════════════════════════════════════════════════════════
// Create Blueprint
// ═══════════════════════════════════════════════════════════

UClass* FBlueprintHelperEditorCommandService::ResolveParentClass(
	const FString& ClassName, FString& OutError)
{
	if (ClassName.IsEmpty())
	{
		OutError = TEXT("父类名不能为空。");
		return nullptr;
	}

	// 尝试直接查找（常见类名如 Actor、Pawn、Character）
	UClass* Found = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::ExactClass, ELogVerbosity::NoLogging);
	if (Found) return Found;

	// 尝试。A 前缀（AActor 等）
	Found = FindFirstObject<UClass>(*(TEXT("A") + ClassName), EFindFirstObjectOptions::ExactClass, ELogVerbosity::NoLogging);
	if (Found) return Found;

	// 尝试。U 前缀
	Found = FindFirstObject<UClass>(*(TEXT("U") + ClassName), EFindFirstObjectOptions::ExactClass, ELogVerbosity::NoLogging);
	if (Found) return Found;

	// 遍历查找
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->GetName() == ClassName || It->GetName() == (TEXT("A") + ClassName) || It->GetName() == (TEXT("U") + ClassName))
		{
			return *It;
		}
	}

	OutError = FString::Printf(TEXT("未找到父类: %s"), *ClassName);
	return nullptr;
}

FBlueprintHelperCreateBlueprintResult FBlueprintHelperEditorCommandService::CreateBlueprint(
	const FString& AssetPath,
	const FString& ParentClassName) const
{
	FBlueprintHelperCreateBlueprintResult Result;

	if (AssetPath.IsEmpty())
	{
		Result.ErrorMessage = TEXT("asset_path 不能为空。");
		return Result;
	}

	// 解析父类
	FString ClassError;
	UClass* ParentClass = ResolveParentClass(ParentClassName, ClassError);
	if (!ParentClass)
	{
		Result.ErrorMessage = ClassError;
		return Result;
	}

	// 检查能否基于该类创建蓝图
	if (!FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass))
	{
		Result.ErrorMessage = FString::Printf(
			TEXT("无法基于 %s 创建蓝图。"), *ParentClass->GetName());
		return Result;
	}

	// 从路径中提取包名和资产名
	const FString PackagePath = AssetPath;
	const FString AssetName = FPackageName::GetShortName(AssetPath);

	// 检查是否已存在
	if (StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath))
	{
		Result.ErrorMessage = FString::Printf(TEXT("资产已存在: %s"), *AssetPath);
		return Result;
	}

	// 创建包
	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		Result.ErrorMessage = FString::Printf(TEXT("无法创建包: %s"), *PackagePath);
		return Result;
	}

	// 创建蓝图
	UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(
		ParentClass, Package, FName(*AssetName), BPTYPE_Normal);
	if (!NewBP)
	{
		Result.ErrorMessage = FString::Printf(TEXT("创建蓝图失败: %s"), *AssetPath);
		return Result;
	}

	// 注册。AssetRegistry
	FAssetRegistryModule::AssetCreated(NewBP);
	Package->MarkPackageDirty();

	Result.bSuccess = true;
	Result.AssetPath = NewBP->GetPathName();
	Result.BlueprintName = AssetName;
	Result.ParentClassName = ParentClass->GetName();
	return Result;
}

// ═══════════════════════════════════════════════════════════
// Console Command
// ═══════════════════════════════════════════════════════════

FBlueprintHelperCommandResult FBlueprintHelperEditorCommandService::ExecConsoleCommand(
	const FString& Command) const
{
	FBlueprintHelperCommandResult Result;

	if (Command.IsEmpty())
	{
		Result.ErrorMessage = TEXT("command 不能为空。");
		return Result;
	}

	if (!GEditor)
	{
		Result.ErrorMessage = TEXT("GEditor 不可用。");
		return Result;
	}

	// 捕获命令输出
	FStringOutputDevice OutputDevice;
	UWorld* World = GEditor->GetEditorWorldContext().World();
	GEditor->Exec(World, *Command, OutputDevice);

	Result.bSuccess = true;
	Result.Message = OutputDevice.IsEmpty() ? TEXT("命令已执行（无输出）。") : *OutputDevice;
	return Result;
}

// ═══════════════════════════════════════════════════════════
// Close Editor
// ═══════════════════════════════════════════════════════════

FBlueprintHelperCommandResult FBlueprintHelperEditorCommandService::CloseEditor(bool bSaveAll) const
{
	FBlueprintHelperCommandResult Result;

	if (!GEditor)
	{
		Result.ErrorMessage = TEXT("GEditor 不可用。");
		return Result;
	}

	if (bSaveAll)
	{
		const bool bSaved = FEditorFileUtils::SaveDirtyPackages(
			/*bPromptUserToSave=*/ false,
			/*bSaveMapPackages=*/ true,
			/*bSaveContentPackages=*/ true);
		if (!bSaved)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BlueprintHelper] CloseEditor: 部分包保存失败，仍将继续关闭。"));
		}
	}

	// 延迟到下一帧退出，确保本次 Bridge 响应能发送回去
	GEngine->DeferredCommands.Add(TEXT("QUIT_EDITOR"));

	Result.bSuccess = true;
	Result.Message = bSaveAll
		? TEXT("已保存所有脏资源，编辑器将在下一帧关闭。")
		: TEXT("编辑器将在下一帧关闭（未保存）。");
	return Result;
}
