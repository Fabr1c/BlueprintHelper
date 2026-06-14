// BlueprintHelper MaterialGraph task-runtime mutation adapter.

#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperMaterialGraphMutationAdapter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskPlanLoweringUtils.h"
#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphFacade.h"

bool FBlueprintHelperMaterialGraphMutationAdapter::TryLower(
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
			TEXT("material graph_write TaskPlan step requires write object."),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("write")));
		return false;
	}

	FString GraphDomain;
	(*WriteObjectPtr)->TryGetStringField(TEXT("graph_domain"), GraphDomain);
	if (GraphDomain != TEXT("material_graph"))
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("material_graph_domain_missing"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("MaterialGraph runtime adapter requires write.graph_domain=material_graph."),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("write.graph_domain")));
		return false;
	}

	FBlueprintHelperMaterialGraphFacade Facade;
	FBlueprintHelperMaterialGraphPreflightInput PreflightInput;
	PreflightInput.StepObject = Request.StepObject;
	PreflightInput.bDryRun = Request.bDryRun;
	const FBlueprintHelperMaterialGraphPreflightResult PreflightResult = Facade.Preflight(PreflightInput);
	if (!PreflightResult.bValid)
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			PreflightResult.ErrorCode.IsEmpty() ? TEXT("material_graph_schema_invalid") : PreflightResult.ErrorCode,
			EBlueprintHelperToolStage::Preflight,
			PreflightResult.ErrorMessage.IsEmpty() ? TEXT("MaterialGraph preflight failed.") : PreflightResult.ErrorMessage,
			PreflightResult.FieldPath.IsEmpty()
				? BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("write.ops"))
				: BlueprintHelperGraphWriteLowering::BuildStepFieldPath(PreflightResult.FieldPath));
		return false;
	}

	const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
	if (!Request.StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
		!TargetObjectPtr || !TargetObjectPtr->IsValid())
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("invalid_taskplan_step_target"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("MaterialGraph TaskPlan step requires target object."),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("target")));
		return false;
	}

	FString AssetPath;
	(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), AssetPath);
	if (AssetPath.IsEmpty())
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("invalid_taskplan_step_target"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("MaterialGraph TaskPlan target requires asset_path."),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("target.asset_path")));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
	if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OpsArray) || !OpsArray)
	{
		OutError = BlueprintHelperGraphWriteLowering::MakeToolError(
			TEXT("material_graph_schema_invalid"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("MaterialGraph TaskPlan step requires write.ops."),
			BlueprintHelperGraphWriteLowering::BuildStepFieldPath(TEXT("write.ops")));
		return false;
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> TargetPayload = MakeShared<FJsonObject>();
	BlueprintHelperGraphWriteLowering::CopyObjectFields(*TargetObjectPtr, TargetPayload);
	TargetPayload->SetStringField(TEXT("asset_path"), AssetPath);
	TargetPayload->SetStringField(TEXT("target_type"), TEXT("material_graph"));
	Payload->SetObjectField(TEXT("target"), TargetPayload);
	Payload->SetArrayField(TEXT("ops"), *OpsArray);
	Payload->SetBoolField(TEXT("dry_run"), Request.bDryRun);

	FString MaterialStrategy;
	(*WriteObjectPtr)->TryGetStringField(TEXT("material_strategy"), MaterialStrategy);
	if (MaterialStrategy.IsEmpty())
	{
		MaterialStrategy = TEXT("append_new_owned_graph");
	}
	Payload->SetStringField(TEXT("material_strategy"), MaterialStrategy);

	FString StepId;
	if (!Request.StepObject->TryGetStringField(TEXT("step_id"), StepId) || StepId.IsEmpty())
	{
		StepId = TEXT("step_material_graph");
	}

	OutResult.LoweredStep.StepId = StepId;
	OutResult.LoweredStep.DependsOn = BlueprintHelperGraphWriteLowering::ReadStepDependsOn(Request.StepObject);
	OutResult.LoweredStep.Capability = TEXT("graph_write");
	OutResult.LoweredStep.RuntimeOperation = TEXT("graph_write");
	OutResult.LoweredStep.AdapterOperation = TEXT("material_graph_edit");
	OutResult.LoweredStep.Payload = Payload;
	OutResult.LoweredStep.bAdapterDryRunSupported = true;
	return true;
}
