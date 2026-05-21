#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

class BLUEPRINTHELPER_API FBlueprintHelperActionContextDemandCollector
{
public:
	static TArray<FBlueprintHelperActionContextDemand> CollectFromSemanticIR(
		const FBlueprintHelperGraphSemanticIR& SemanticIR);

	static TArray<FBlueprintHelperActionContextDemand> CollectFromStatements(
		const TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>& Statements);

private:
	static void CollectFromStatementArray(
		const TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>& Statements,
		TArray<FBlueprintHelperActionContextDemand>& OutDemands);

	static void AppendDemandForStatement(
		const FBlueprintHelperGraphStatementIR& Statement,
		TArray<FBlueprintHelperActionContextDemand>& OutDemands);

	static void AppendDemandForExpression(
		const FBlueprintHelperGraphExpressionIR& Expression,
		const FString& OwnerStatementId,
		TArray<FBlueprintHelperActionContextDemand>& OutDemands);

	static FBlueprintHelperActionContextDemand BuildDemand(
		const FString& StableId,
		const FString& SourcePath,
		EBlueprintHelperActionSemanticKind SemanticKind,
		const FString& Query,
		const FString& TargetPath,
		const FString& PropertyPath,
		const FString& TypeName,
		const FString& SearchMode,
		const FString& AmbiguityPolicy,
		const TArray<FString>& CategoryPriority,
		const TArray<FString>& ArgumentNames);

	static void ApplyDemandKinds(FBlueprintHelperActionContextDemand& Demand);
	static EBlueprintHelperActionSemanticKind ToActionSemanticKind(EBlueprintHelperGraphStatementKind Kind);
	static EBlueprintHelperActionSemanticKind ToActionSemanticKind(EBlueprintHelperGraphExpressionKind Kind);
};
