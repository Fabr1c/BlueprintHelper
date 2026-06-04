// BlueprintHelper GraphWrite AutoSearch policy parser.

#include "Systems/ToolClusters/GraphWrite/AutoSearch/BlueprintHelperGraphWriteAutoSearchPolicyResolver.h"

#include "Dom/JsonObject.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionSettingsResolver.h"

class FBlueprintHelperGraphWriteAutoSearchPolicyResolverLocalUtils
{
public:
	static FString MakeInvalidPolicyError(const FString& Detail)
	{
		return FString::Printf(TEXT("invalid_graphwrite_autosearch_policy: %s"), *Detail);
	}

	static bool ReadBoundedInt(
		const TSharedPtr<FJsonObject>& PolicyObject,
		const TCHAR* FieldName,
		int32 DefaultValue,
		int32 MinValue,
		int32 MaxValue,
		int32& OutValue,
		FString& OutError)
	{
		OutValue = DefaultValue;
		if (!PolicyObject.IsValid() || !PolicyObject->HasField(FieldName))
		{
			return true;
		}

		double RawValue = 0.0;
		if (!PolicyObject->TryGetNumberField(FieldName, RawValue))
		{
			OutError = MakeInvalidPolicyError(FString::Printf(TEXT("%s must be an integer."), FieldName));
			return false;
		}

		const int32 IntValue = static_cast<int32>(RawValue);
		if (!FMath::IsNearlyEqual(RawValue, static_cast<double>(IntValue)))
		{
			OutError = MakeInvalidPolicyError(FString::Printf(TEXT("%s must be an integer."), FieldName));
			return false;
		}

		if (IntValue < MinValue || IntValue > MaxValue)
		{
			OutError = MakeInvalidPolicyError(FString::Printf(
				TEXT("%s must be between %d and %d."),
				FieldName,
				MinValue,
				MaxValue));
			return false;
		}

		OutValue = IntValue;
		return true;
	}

	static bool ReadDetailLevel(
		const TSharedPtr<FJsonObject>& PolicyObject,
		FString& OutDetailLevel,
		FString& OutError)
	{
		OutDetailLevel = TEXT("short");
		if (!PolicyObject.IsValid() || !PolicyObject->HasField(TEXT("detail_level")))
		{
			return true;
		}

		FString DetailLevel;
		if (!PolicyObject->TryGetStringField(TEXT("detail_level"), DetailLevel))
		{
			OutError = MakeInvalidPolicyError(TEXT("detail_level must be short or diagnostic."));
			return false;
		}

		DetailLevel = DetailLevel.TrimStartAndEnd().ToLower();
		if (DetailLevel != TEXT("short") && DetailLevel != TEXT("diagnostic"))
		{
			OutError = MakeInvalidPolicyError(TEXT("detail_level must be short or diagnostic."));
			return false;
		}

		OutDetailLevel = DetailLevel;
		return true;
	}
};

bool FBlueprintHelperGraphWriteAutoSearchPolicyResolver::TryParseFromWriteObject(
	const TSharedPtr<FJsonObject>& WriteObject,
	FBlueprintHelperGraphWriteAutoSearchPolicy& OutPolicy,
	FString& OutError)
{
	OutPolicy = FBlueprintHelperGraphWriteAutoSearchPolicy();
	OutError.Reset();

	const FBlueprintHelperActionResolutionSettings Settings =
		FBlueprintHelperActionResolutionSettingsResolver::Load();
	OutPolicy.MaxCandidatesPerStatement = Settings.AutoSearchMaxCandidatesPerStatement;
	OutPolicy.MaxAutoSearchStatements = Settings.AutoSearchMaxStatements;
	OutPolicy.MaxTotalSearchMs = Settings.AutoSearchMaxTotalMs;
	OutPolicy.DetailLevel = Settings.AutoSearchDetailLevel;

	if (!WriteObject.IsValid())
	{
		return true;
	}

	const TSharedPtr<FJsonObject>* PolicyObjectPtr = nullptr;
	if (!WriteObject->TryGetObjectField(TEXT("auto_search_policy"), PolicyObjectPtr) ||
		!PolicyObjectPtr ||
		!PolicyObjectPtr->IsValid())
	{
		return true;
	}

	const TSharedPtr<FJsonObject> PolicyObject = *PolicyObjectPtr;
	FString Mode;
	if (!PolicyObject->HasField(TEXT("mode")))
	{
		return true;
	}
	if (!PolicyObject->TryGetStringField(TEXT("mode"), Mode))
	{
		OutError = FBlueprintHelperGraphWriteAutoSearchPolicyResolverLocalUtils::MakeInvalidPolicyError(
			TEXT("mode must be off or on_preview_resolution_failure."));
		return false;
	}

	Mode = Mode.TrimStartAndEnd().ToLower();
	if (Mode.IsEmpty() || Mode == TEXT("off"))
	{
		return true;
	}

	if (Mode != TEXT("on_preview_resolution_failure"))
	{
		OutError = FBlueprintHelperGraphWriteAutoSearchPolicyResolverLocalUtils::MakeInvalidPolicyError(
			TEXT("mode must be off or on_preview_resolution_failure."));
		return false;
	}

	OutPolicy.bEnablePreviewRecovery = true;
	if (!FBlueprintHelperGraphWriteAutoSearchPolicyResolverLocalUtils::ReadBoundedInt(
		PolicyObject,
		TEXT("max_candidates_per_statement"),
		Settings.AutoSearchMaxCandidatesPerStatement,
		1,
		10,
		OutPolicy.MaxCandidatesPerStatement,
		OutError))
	{
		return false;
	}

	if (!FBlueprintHelperGraphWriteAutoSearchPolicyResolverLocalUtils::ReadBoundedInt(
		PolicyObject,
		TEXT("max_auto_search_statements"),
		Settings.AutoSearchMaxStatements,
		1,
		64,
		OutPolicy.MaxAutoSearchStatements,
		OutError))
	{
		return false;
	}

	if (!FBlueprintHelperGraphWriteAutoSearchPolicyResolverLocalUtils::ReadBoundedInt(
		PolicyObject,
		TEXT("max_total_auto_search_ms"),
		Settings.AutoSearchMaxTotalMs,
		1,
		1000,
		OutPolicy.MaxTotalSearchMs,
		OutError))
	{
		return false;
	}

	return FBlueprintHelperGraphWriteAutoSearchPolicyResolverLocalUtils::ReadDetailLevel(
		PolicyObject,
		OutPolicy.DetailLevel,
		OutError);
}
