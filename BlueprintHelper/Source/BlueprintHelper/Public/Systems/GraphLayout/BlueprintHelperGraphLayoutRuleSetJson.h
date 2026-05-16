#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
class BLUEPRINTHELPER_API FRuleSetJson
{
public:
	static FValidationResult Validate(const TSharedPtr<FJsonObject>& Json);
	static bool Import(const TSharedPtr<FJsonObject>& Json, FRuleSet& OutRuleSet, FValidationResult& OutValidation);
	static bool ImportString(const FString& JsonText, FRuleSet& OutRuleSet, FValidationResult& OutValidation);
	static TSharedRef<FJsonObject> Export(const FRuleSet& RuleSet);
	static FString ExportString(const FRuleSet& RuleSet);
};
}
