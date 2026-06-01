#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
enum class EGraphLayoutPreviewNodeFactory : uint8
{
	CustomEvent,
	CallFunction,
	IfThenElse,
	ExecutionSequence,
	MakeArray,
	Self,
	Comment,
	GenericK2
};

struct FGraphLayoutPreviewNodeSpec
{
	FString NodeId;
	FString Title;
	EGraphLayoutPreviewNodeFactory Factory = EGraphLayoutPreviewNodeFactory::GenericK2;
	ENodeRole Role = ENodeRole::Unknown;
	FVector2D Size = FVector2D(220.0f, 96.0f);
};

struct FGraphLayoutPreviewLinkSpec
{
	FString FromNodeId;
	FString FromPinName;
	FString ToNodeId;
	FString ToPinName;
	bool bExec = false;
};

struct FGraphLayoutPreviewSample
{
	ESemanticScene Scene = ESemanticScene::LinearExecChain;
	FGraphSnapshot Snapshot;
	TArray<FGraphLayoutPreviewNodeSpec> Nodes;
	TArray<FGraphLayoutPreviewLinkSpec> Links;
};

struct FGraphLayoutPreviewBuildResult
{
	uint64 JobId = 0;
	bool bSuccess = false;
	FString Error;
	FGraphLayoutPreviewSample Sample;
	FLayoutPlan LayoutPlan;
};
}
