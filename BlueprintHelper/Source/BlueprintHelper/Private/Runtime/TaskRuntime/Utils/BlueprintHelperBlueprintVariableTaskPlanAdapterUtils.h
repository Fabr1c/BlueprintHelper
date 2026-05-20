#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FJsonObject;

class FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils
{
public:
	static FBlueprintHelperToolError MakeVariableTaskPlanError(
		const FString& Code,
		const FString& Message,
		const FString& Field);

	static FString BuildStepFieldPath(const FString& Suffix);
	static FString BuildOpFieldPath(int32 OpIndex, const FString& Suffix);

	static bool TryValidateMemberVariableOp(
		const FString& OpName,
		const TSharedPtr<FJsonObject>& OpObject,
		int32 OpIndex,
		FBlueprintHelperToolError& OutError);

	static bool TryValidateMemberDefaultOp(
		const FString& OpName,
		const TSharedPtr<FJsonObject>& OpObject,
		int32 OpIndex,
		FBlueprintHelperToolError& OutError);

	static bool TryValidateLocalVariableOp(
		const FString& OpName,
		const TSharedPtr<FJsonObject>& OpObject,
		const FString& FunctionName,
		int32 OpIndex,
		FBlueprintHelperToolError& OutError);

	static void CopyObjectFieldsExceptOp(
		const TSharedPtr<FJsonObject>& Source,
		const TSharedRef<FJsonObject>& Destination);

	static TSharedRef<FJsonObject> CopyOpForBatch(
		const TSharedPtr<FJsonObject>& Source,
		const FString& LocalFunctionName);

private:
	static bool TryReadRequiredString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		const FString& FieldPath,
		FString& OutValue,
		FBlueprintHelperToolError& OutError);

	static bool TryReadRequiredObject(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		const FString& FieldPath,
		FBlueprintHelperToolError& OutError);

	static bool TryReadRequiredArray(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		const FString& FieldPath,
		FBlueprintHelperToolError& OutError);

	static bool TryRequireValueField(
		const TSharedPtr<FJsonObject>& Object,
		const FString& FieldPath,
		FBlueprintHelperToolError& OutError);
};
