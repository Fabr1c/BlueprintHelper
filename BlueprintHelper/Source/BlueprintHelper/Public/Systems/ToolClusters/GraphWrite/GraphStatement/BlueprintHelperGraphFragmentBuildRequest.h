#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

struct BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentBuildRequest
{
	FString FragmentId;
	FString SourceStatementId;
	FString ActionContextStatementId;
	FString Query;
	FString Target;
	FString PropertyPath;
	FString FieldOperation;
	FString FieldScope;
	FString CreateOperation;
	FString ClassPath;
	FString AssetPath;
	FString PinType;
	FString KeyPinType;
	FString ValuePinType;
	FString TypeName;
	FString ExpectedReturnType;
	FVector2D Location = FVector2D::ZeroVector;
	TMap<FString, FString> DefaultValues;
	TMap<FString, FString> ArgumentTypes;
	TMap<FString, FBlueprintHelperCallFunctionPinType> ArgumentPinTypes;
	FString ResolvedStableId;
	FString SearchMode;
	FString AmbiguityPolicy;
	TArray<FString> CategoryPriority;
	FString TargetObjectType;
	FBlueprintHelperCallFunctionPinType TargetObjectPinType;
	FBlueprintHelperCallFunctionPinType ExpectedReturnPinType;
	FBlueprintHelperGraphStatementIR Statement;
	FBlueprintHelperGraphExpressionIR Expression;
	bool bIsExpression = false;

	static FBlueprintHelperGraphFragmentBuildRequest FromStatement(const FBlueprintHelperGraphStatementIR& Statement);
	static FBlueprintHelperGraphFragmentBuildRequest FromExpression(const FBlueprintHelperGraphExpressionIR& Expression);
};
