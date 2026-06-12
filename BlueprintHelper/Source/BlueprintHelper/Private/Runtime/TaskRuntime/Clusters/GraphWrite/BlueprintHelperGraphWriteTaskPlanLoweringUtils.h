// BlueprintHelper GraphWrite task-plan lowering helpers.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

namespace BlueprintHelperGraphWriteLowering
{
	FBlueprintHelperToolError MakeToolError(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field);

	FString BuildStepFieldPath(const FString& Suffix);
	FString BuildOpFieldPath(int32 OpIndex, const FString& Suffix);
	FString ToIdSegment(const FString& Value);

	TSharedPtr<FJsonObject> AsJsonObjectIfObject(const TSharedPtr<FJsonValue>& Value);
	TSharedRef<FJsonObject> CopyJsonObject(const TSharedPtr<FJsonObject>& Source);
	void CopyObjectFields(
		const TSharedPtr<FJsonObject>& Source,
		const TSharedRef<FJsonObject>& Destination);

	bool TryReadStepTarget(
		const TSharedPtr<FJsonObject>& StepObject,
		TSharedPtr<FJsonObject>& OutTargetObject,
		FString& OutAssetPath,
		FString& OutGraphName,
		FBlueprintHelperToolError& OutError);

	bool TryReadWriteOps(
		const TSharedPtr<FJsonObject>& StepObject,
		const TArray<TSharedPtr<FJsonValue>>*& OutOpsArray,
		FBlueprintHelperToolError& OutError);

	bool TryReadRequiredObject(
		const TSharedPtr<FJsonObject>& Source,
		const FString& FieldName,
		const FString& FieldPath,
		TSharedPtr<FJsonObject>& OutObject,
		FBlueprintHelperToolError& OutError);

	TSharedRef<FJsonObject> BuildTargetPayload(
		const TSharedPtr<FJsonObject>& TargetObject,
		const FString& AssetPath,
		const FString& GraphName);

	TArray<FString> ReadStepDependsOn(const TSharedPtr<FJsonObject>& StepObject);
	bool TryReadExecutionPolicyBool(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TCHAR* FieldName,
		bool& OutValue);
}
