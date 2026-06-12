// BlueprintHelper Core Utils -- GraphWrite 域通用工具函数库
// 集中存放从各文件匿名 namespace 提取的静态工具函数

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeCallableFragmentService.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuildRequest.h"
#include "Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationIntent.h"
#include "GraphWriteCoreUtils.generated.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UK2Node_ExecutionSequence;
class FBlueprintHelperActionContextScope;

UCLASS()
class BLUEPRINTHELPER_API UGraphWriteCoreUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ===== From BlueprintHelperReplaceEntryResolver.cpp =====
	/** 比较两个字符串是否匹配（忽略大小写和首尾空格） */
	static bool NameMatches(const FString& Candidate, const FString& Expected);

	/** 判断 EntryName 是否匹配任意一个 Candidate */
	static bool EntryNameMatchesAny(const TArray<FString>& Candidates, const FString& EntryName);

	// ===== From BlueprintHelperMergeBlueprintGraphService.cpp =====
	/** 构建 MergeCallableFragmentRequest */
	static FBlueprintHelperMergeCallableFragmentRequest MakeMergeCallableRequest(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FString& Query,
		const FString& FragmentId,
		const FString& SearchMode = FString(),
		const FString& AmbiguityPolicy = FString(),
		const TArray<FString>& CategoryPriority = {});

	/** 格式化 MergeCallable 失败信息 */
	static FString FormatMergeCallableFailure(
		const FBlueprintHelperMergeCallableFragmentResult& Result,
		const FString& FallbackQuery);

	// ===== From BlueprintHelperMergeCallableFragmentService.cpp =====
	/** 构建 CallActionContextScope */
	static bool BuildCallActionContextScope(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FBlueprintHelperGraphFragmentBuildRequest& BuildRequest,
		FBlueprintHelperActionContextScope& OutScope,
		FString& OutError);

	// ===== From BlueprintHelperGraphWriteMutationCoordinator.cpp =====
	/** 判断 Pin 是否为 Exec 引脚 */
	static bool IsExecPin(const UEdGraphPin* Pin);

	/** 查找 Node 上第一个指定方向的 Exec 引脚 */
	static UEdGraphPin* FindFirstExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction);

	/** 收集 Node 上所有 Exec 输出引脚 */
	static void CollectExecOutputPins(UEdGraphNode* Node, TArray<UEdGraphPin*>& OutPins);

	/** 尝试连接两个引脚 */
	static bool TrySchemaConnect(UEdGraph* Graph, UEdGraphPin* FromPin, UEdGraphPin* ToPin, FString& OutError, bool& bOutChanged);

	/** 尝试断开两个引脚的连接 */
	static bool TryBreakLink(UEdGraphPin* FromPin, UEdGraphPin* ToPin, FString& OutError, bool& bOutChanged);

	/** 生成 Sequence 节点用于 BranchFork */
	static UK2Node_ExecutionSequence* SpawnSequenceNode(UEdGraph* TargetGraph, const FString& IntentId, FString& OutError);

	/** 确保 Sequence 节点有足够的输出引脚 */
	static bool EnsureSequenceOutputCount(UK2Node_ExecutionSequence* SequenceNode, int32 DesiredCount, TArray<UEdGraphPin*>& OutPins, FString& OutError);

	/** 设置 Pin 的默认值 */
	static bool ApplySetPinDefault(UEdGraph* Graph, UEdGraphPin* Pin, const FString& NewValue, FString& OutError, bool& bOutChanged);

	/** 替换 Pin 连接 */
	static bool ApplyReplaceLink(UEdGraph* Graph, UEdGraphPin* FromPin, UEdGraphPin* OldToPin, UEdGraphPin* NewToPin, FString& OutError, bool& bOutChanged);

	static bool ApplyReplaceLinkSource(UEdGraph* Graph, UEdGraphPin* OldFromPin, UEdGraphPin* ToPin, UEdGraphPin* NewFromPin, FString& OutError, bool& bOutChanged);

	/** Append 语义体 */
	static bool ApplyAppendSemanticBody(UEdGraph* Graph, const FBlueprintHelperGraphWriteMutationIntent& Intent, FString& OutError, bool& bOutChanged);

	/** Insert 语义体 */
	static bool ApplyInsertSemanticBody(UEdGraph* Graph, const FBlueprintHelperGraphWriteMutationIntent& Intent, FString& OutError, bool& bOutChanged);

	/** BranchFork 语义体 */
	static bool ApplyBranchForkSemanticBody(UEdGraph* Graph, const FBlueprintHelperGraphWriteMutationIntent& Intent, FString& OutError, bool& bOutChanged);
};
