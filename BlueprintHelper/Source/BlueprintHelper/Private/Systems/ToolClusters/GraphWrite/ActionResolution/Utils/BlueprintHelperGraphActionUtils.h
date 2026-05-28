// ActionResolution 共享工厂函数

#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class FBlueprintHelperGraphActionUtils
{
public:
	/** 创建 InvalidRequest 结果 */
	static FBlueprintHelperActionResolutionResult MakeInvalidResult(
		const FString& ErrorCode,
		const FString& Message);

	/** 创建 UnsupportedIntent 结果 */
	static FBlueprintHelperActionResolutionResult MakeUnsupportedResult(
		const FString& ErrorCode,
		const FString& Message);

	/** 检查是否有 FunctionOperation evidence */
	static bool HasFunctionBackedOperationEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic);

	/** 查找第一个非空 Class evidence */
	static FString ResolveClassEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic);
};
