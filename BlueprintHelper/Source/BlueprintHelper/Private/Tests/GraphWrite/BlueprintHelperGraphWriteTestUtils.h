// GraphWrite 测试工具类 — 提取匿名命名空间中的共享函数，避免 Unity Build 模式下重复定义

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class FBlueprintHelperActionContextScope;
struct FBlueprintHelperGraphStatementIR;

class FBlueprintHelperGraphWriteTestUtils
{
public:
	/** 为指定 Statement 构建 ActionContextScope，支持调用方自定义 Revision 标识和断言标签 */
	static bool BuildActionContextScopeForStatement(
		FAutomationTestBase& Test,
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FBlueprintHelperGraphStatementIR& Statement,
		const TCHAR* DemandsTestLabel,
		const TCHAR* RevisionTestId,
		const TCHAR* RevisionTaskId,
		FBlueprintHelperActionContextScope& OutScope,
		FString& OutError);

	/** 在 Fragment 中查找符合类型的唯一节点，自动断言数量为 1 */
	template <typename TNode>
	static TNode* FindSingleFragmentNode(
		FAutomationTestBase& Test,
		const FBlueprintHelperNodeFragment& Fragment,
		const TCHAR* Label)
	{
		TNode* Result = nullptr;
		int32 MatchCount = 0;
		for (UEdGraphNode* Node : Fragment.Nodes)
		{
			if (TNode* TypedNode = Cast<TNode>(Node))
			{
				Result = TypedNode;
				++MatchCount;
			}
		}

		Test.TestEqual(FString::Printf(TEXT("%s count"), Label), MatchCount, 1);
		return MatchCount == 1 ? Result : nullptr;
	}

	/** 统计 Fragment 中指定类的节点数量 */
	static int32 CountFragmentNodesOfClass(
		const FBlueprintHelperNodeFragment& Fragment,
		const UClass* NodeClass);
};
