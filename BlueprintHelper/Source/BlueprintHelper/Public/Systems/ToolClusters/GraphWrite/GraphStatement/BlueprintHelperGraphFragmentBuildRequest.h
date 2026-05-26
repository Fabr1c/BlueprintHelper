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
	FString CapabilityId;
	TMap<FString, FString> CapabilityFacts;
	FString FieldOperation;
	FString FieldScope;
	FString FunctionOperation;
	FString TransformOperation;
	FString ScheduleOperation;
	FString CreateOperation;
	FString ContainerKind;
	FString ContainerOperation;
	FString ClassPath;
	FString AssetPath;
	FString GraphLatentAllowed;
	FString ElementType;
	FString KeyType;
	FString ValueType;
	FString PinType;
	FString KeyPinType;
	FString ValuePinType;
	FString TypeName;
	FString ExpectedReturnType;
	FVector2D Location = FVector2D::ZeroVector;
	TMap<FString, FString> DefaultValues;
	TMap<FString, FString> ArgumentTypes;
	TMap<FString, FBlueprintHelperCallFunctionPinType> ArgumentPinTypes;
	TMap<FString, FString> ContextEvidence;
	FString ResolvedStableId;
	FString SearchMode;
	FString AmbiguityPolicy;
	TArray<FString> CategoryPriority;
	FString TargetObjectType;
	FBlueprintHelperCallFunctionPinType TargetObjectPinType;
	FBlueprintHelperCallFunctionPinType ExpectedReturnPinType;
	bool bIsExpression = false;

	static FBlueprintHelperGraphFragmentBuildRequest FromStatement(const FBlueprintHelperGraphStatementIR& Statement);
	static FBlueprintHelperGraphFragmentBuildRequest FromExpression(const FBlueprintHelperGraphExpressionIR& Expression);
};
