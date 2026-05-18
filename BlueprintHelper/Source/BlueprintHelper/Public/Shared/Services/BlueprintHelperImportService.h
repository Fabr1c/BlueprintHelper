// BlueprintHelper Service Layer — 导入服务

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperServiceTypes.h"

class FBlueprintHelperGraphResolver;
class FBlueprintHelperValidationService;
class FBlueprintHelperCompileService;

/**
 * 导入服务，封装「校验 → 定位图表 → 事务开始 → 生成节点 → 事务结束」完整流程。
 * 失败时可通过 Ctrl+Z 整体 Undo。
 */
class BLUEPRINTHELPER_API FBlueprintHelperImportService
{
public:
	FBlueprintHelperImportService(const FBlueprintHelperGraphResolver& InResolver,
	                              const FBlueprintHelperValidationService& InValidator);

	/** 设置 CompileService 引用（用于 bAutoCompile）。 */
	void SetCompileService(const FBlueprintHelperCompileService* InCompileService);

	/** 将 JSON 导入到目标蓝图图表。 */
	FBlueprintHelperImportResult Import(const FBlueprintHelperImportRequest& Request) const;

private:
	/** 判断 JSON 是否需要走多图路径（有 graphs 数组或 blueprint_operations）。 */
	bool NeedsMultiGraphPath(const FString& JsonText) const;

	/** Serialize the object-first RawJson request to importable text. */
	FString ResolveImportJsonText(const FBlueprintHelperImportRequest& Request, FBlueprintHelperImportResult& Result) const;

	const FBlueprintHelperGraphResolver& Resolver;
	const FBlueprintHelperValidationService& Validator;
	const FBlueprintHelperCompileService* CompileService = nullptr;
};
