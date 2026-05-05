// BlueprintHelper Service Layer — JSON 结构校验服务

#pragma once

#include "CoreMinimal.h"
#include "Structure/BlueprintHelperServiceTypes.h"

/**
 * JSON 结构校验服务，在导入前进行格式与完整性检查。
 */
class BLUEPRINTHELPER_API FBlueprintHelperValidationService
{
public:
	/** 校验 JSON 文本的结构完整性。 */
	FBlueprintHelperValidationResult Validate(const FString& JsonText) const;

private:
	bool ValidateJsonParseable(const FString& JsonText, TSharedPtr<class FJsonObject>& OutRoot, FBlueprintHelperDiagnosticSet& OutDiag) const;
	bool ValidateVersion(const TSharedPtr<class FJsonObject>& Root, FString& OutVersion, FBlueprintHelperDiagnosticSet& OutDiag) const;
	bool ValidateNodeIds(const TSharedPtr<class FJsonObject>& Root, FBlueprintHelperDiagnosticSet& OutDiag) const;
	bool ValidateLinkReferences(const TSharedPtr<class FJsonObject>& Root, FBlueprintHelperDiagnosticSet& OutDiag) const;
};
