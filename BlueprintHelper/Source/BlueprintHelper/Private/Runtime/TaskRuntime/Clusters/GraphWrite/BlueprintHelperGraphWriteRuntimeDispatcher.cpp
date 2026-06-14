#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteRuntimeDispatcher.h"

#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperExternalUserGraphMutationAdapter.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskPlanLoweringUtils.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperMaterialGraphMutationAdapter.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperOwnedGraphMutationAdapter.h"

bool FBlueprintHelperGraphWriteRuntimeDispatcher::TryLower(
	const FBlueprintHelperGraphWriteLoweringRequest& Request,
	FBlueprintHelperGraphWriteLoweringResult& OutResult,
	FBlueprintHelperToolError& OutError)
{
	OutResult = FBlueprintHelperGraphWriteLoweringResult();
	OutError = FBlueprintHelperToolError();

	const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
	if (!Request.StepObject.IsValid() ||
		!Request.StepObject->TryGetObjectField(TEXT("write"), WriteObjectPtr) ||
		!WriteObjectPtr ||
		!WriteObjectPtr->IsValid())
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("invalid_taskplan_step_write"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("graph_write TaskPlan step requires write object."),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("write")));
		return false;
	}

	FString Strategy;
	if (!(*WriteObjectPtr)->TryGetStringField(TEXT("strategy"), Strategy))
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("unsupported_graph_write_strategy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("GraphWrite Task Runtime supports owned_graph_edit and external_graph_edit."),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("write.strategy")));
		return false;
	}

	FString GraphDomain;
	(*WriteObjectPtr)->TryGetStringField(TEXT("graph_domain"), GraphDomain);
	if (GraphDomain == TEXT("material_graph"))
	{
		return FBlueprintHelperMaterialGraphMutationAdapter::TryLower(Request, OutResult, OutError);
	}

	if (Strategy == TEXT("owned_graph_edit"))
	{
		return FBlueprintHelperOwnedGraphMutationAdapter::TryLower(Request, OutResult, OutError);
	}
	if (Strategy == TEXT("external_graph_edit"))
	{
		return FBlueprintHelperExternalUserGraphMutationAdapter::TryLower(Request, OutResult, OutError);
	}

	OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
		TEXT("unsupported_graph_write_strategy"),
		EBlueprintHelperToolStage::ParseInput,
		TEXT("GraphWrite Task Runtime supports owned_graph_edit and external_graph_edit."),
		BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("write.strategy")));
	return false;
}
