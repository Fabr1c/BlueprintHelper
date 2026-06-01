// BlueprintHelper Service Layer — 通用编辑器命令服务

#pragma once

#include "CoreMinimal.h"

class UPackage;

// ─── 通用命令结果 ───

struct BLUEPRINTHELPER_API FBlueprintHelperCommandResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	FString Message;
};

// ─── Create Blueprint 结果 ───

struct BLUEPRINTHELPER_API FBlueprintHelperCreateBlueprintResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	FString AssetPath;
	FString BlueprintName;
	FString ParentClassName;
};

/**
 * 通用编辑器命令服务。
 * 提供 Undo/Redo、PIE 控制、蓝图创建、控制台命令等操作。
 */
class BLUEPRINTHELPER_API FBlueprintHelperEditorCommandService
{
public:
	/** 撤销上一步操作。 */
	FBlueprintHelperCommandResult Undo() const;

	/** 重做上一步撤销。 */
	FBlueprintHelperCommandResult Redo() const;

	/** 启动 Play In Editor (PIE) 会话。 */
	FBlueprintHelperCommandResult PlayInEditor() const;

	/** 停止当前 PIE 会话。 */
	FBlueprintHelperCommandResult StopPIE() const;

	/**
	 * 创建新的 Blueprint 资产。
	 * @param AssetPath  资产路径（如 /Game/Blueprints/BP_MyActor）
	 * @param ParentClass 父类名称（如 Actor、Pawn、Character，默认 Actor）
	 */
	FBlueprintHelperCreateBlueprintResult CreateBlueprint(
		const FString& AssetPath,
		const FString& ParentClassName = TEXT("Actor")) const;

	/**
	 * 执行编辑器控制台命令。
	 * @param Command 控制台命令文本
	 * @return 命令输出文本
	 */
	FBlueprintHelperCommandResult ExecConsoleCommand(const FString& Command) const;

	/**
	 * 保存所有脏资源并请求关闭编辑器。
	 * 退出在下一帧生效，调用方可以在退出前收到成功响应。
	 * @param bSaveAll 是否在退出前保存所有脏包（默认 true）
	 */
	FBlueprintHelperCommandResult CloseEditor(bool bSaveAll = true) const;

	/** Clear dirty flags for packages that should be discarded during an explicit no-save editor shutdown. */
	static int32 DiscardDirtyPackages(TConstArrayView<UPackage*> Packages);

	/** Collect all current dirty packages and mark them clean before a no-save editor shutdown. */
	static int32 DiscardAllDirtyPackagesForClose();

private:
	/** 根据类名查找 UClass。 */
	static UClass* ResolveParentClass(const FString& ClassName, FString& OutError);
};
