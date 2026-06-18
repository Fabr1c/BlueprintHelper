// BlueprintHelper Review MaterialInstance presenter model.

#include "UI/Review/BlueprintHelperReviewMaterialInstancePresenterModel.h"

#include "Materials/MaterialInstanceConstant.h"
#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceParameterJsonUtils.h"
#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceResolver.h"
#include "UI/Review/BlueprintHelperReviewAssetPresenterTypes.h"

class FBlueprintHelperReviewMaterialInstancePresenterModelPrivate
{
public:
	static void AddUniqueTrimmedKey(TArray<FString>& OutKeys, const FString& InKey)
	{
		FString Key = InKey;
		Key.TrimStartAndEndInline();
		if (!Key.IsEmpty())
		{
			OutKeys.AddUnique(Key);
		}
	}

	static TSharedRef<FBlueprintHelperReviewDataAssetRowItem> MakeRow(
		const FString& Label,
		const FString& Value,
		const FString& SearchText,
		int32 Depth)
	{
		TSharedRef<FBlueprintHelperReviewDataAssetRowItem> Row = MakeShared<FBlueprintHelperReviewDataAssetRowItem>();
		Row->Label = Label;
		Row->Value = Value;
		Row->SearchText = SearchText;
		Row->Depth = Depth;
		return Row;
	}

	static void AddAlias(
		const TSharedRef<FBlueprintHelperReviewDataAssetRowItem>& Row,
		const FString& Alias)
	{
		if (!Alias.IsEmpty())
		{
			Row->SearchAliases.AddUnique(Alias);
		}
	}

	static FString MakeParameterValueSummary(const FBlueprintHelperMaterialInstanceParameterSchemaEntry& Parameter)
	{
		const FString Type = BlueprintHelperMaterialInstanceParameterTypeToString(Parameter.Type);
		const FString Source = BlueprintHelperMaterialInstanceParameterSourceToString(Parameter.Source);
		const FString OverrideState = FBlueprintHelperMaterialInstanceParameterJsonUtils::MakeOverrideState(
			Parameter.bHasOverride,
			Source);
		const FString EffectiveValue = Parameter.EffectiveValue.ToDebugString(Parameter.Type);
		const FString OverrideValue = Parameter.OverrideValue.ToDebugString(Parameter.Type);
		return FString::Printf(
			TEXT("type=%s source=%s override_state=%s effective_value=%s override_value=%s"),
			*Type,
			*Source,
			*OverrideState,
			*EffectiveValue,
			*OverrideValue);
	}
};

FString FBlueprintHelperReviewMaterialInstancePresenterModel::MakeParameterTargetKey(
	const FString& ParameterName,
	const FString& ParameterType)
{
	const FString TrimmedName = ParameterName.TrimStartAndEnd();
	const FString TrimmedType = ParameterType.TrimStartAndEnd();
	if (TrimmedName.IsEmpty() || TrimmedType.IsEmpty())
	{
		return FString();
	}
	return FString::Printf(
		TEXT("material_instance_parameter:%s:%s"),
		*TrimmedType,
		*TrimmedName);
}

FString FBlueprintHelperReviewMaterialInstancePresenterModel::MakeParameterDisplayLabel(
	const FString& ParameterName,
	const FString& ParameterType)
{
	const FString TrimmedName = ParameterName.TrimStartAndEnd();
	const FString TrimmedType = ParameterType.TrimStartAndEnd();
	if (TrimmedName.IsEmpty())
	{
		return FString();
	}
	if (TrimmedType.IsEmpty())
	{
		return TrimmedName;
	}
	return FString::Printf(TEXT("%s (%s)"), *TrimmedName, *TrimmedType);
}

void FBlueprintHelperReviewMaterialInstancePresenterModel::AppendStableMatchKeys(
	const FString& ParameterName,
	const FString& ParameterType,
	TArray<FString>& OutKeys,
	const FString& TargetKey,
	const FString& PropertyPath,
	const FString& DisplayLabel)
{
	const FString CanonicalTargetKey = MakeParameterTargetKey(ParameterName, ParameterType);
	const FString CanonicalDisplayLabel = MakeParameterDisplayLabel(ParameterName, ParameterType);
	FBlueprintHelperReviewMaterialInstancePresenterModelPrivate::AddUniqueTrimmedKey(OutKeys, CanonicalTargetKey);
	FBlueprintHelperReviewMaterialInstancePresenterModelPrivate::AddUniqueTrimmedKey(OutKeys, TargetKey);
	FBlueprintHelperReviewMaterialInstancePresenterModelPrivate::AddUniqueTrimmedKey(
		OutKeys,
		PropertyPath.IsEmpty() ? ParameterName : PropertyPath);
	FBlueprintHelperReviewMaterialInstancePresenterModelPrivate::AddUniqueTrimmedKey(
		OutKeys,
		DisplayLabel.IsEmpty() ? CanonicalDisplayLabel : DisplayLabel);
	FBlueprintHelperReviewMaterialInstancePresenterModelPrivate::AddUniqueTrimmedKey(OutKeys, ParameterName);
	FBlueprintHelperReviewMaterialInstancePresenterModelPrivate::AddUniqueTrimmedKey(OutKeys, ParameterType);
}

void FBlueprintHelperReviewMaterialInstancePresenterModel::AppendRows(
	UMaterialInstanceConstant* MaterialInstance,
	TArray<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>& OutRows)
{
	if (!MaterialInstance)
	{
		return;
	}

	TArray<FBlueprintHelperMaterialInstanceParameterSchemaEntry> Schema;
	FString ErrorCode;
	FString ErrorMessage;
	if (!FBlueprintHelperMaterialInstanceResolver::CollectParameterSchema(
		MaterialInstance,
		Schema,
		ErrorCode,
		ErrorMessage))
	{
		if (!ErrorCode.IsEmpty() || !ErrorMessage.IsEmpty())
		{
			const FString Summary = FString::Printf(
				TEXT("error_code=%s error_message=%s"),
				*ErrorCode,
				*ErrorMessage);
			OutRows.Add(FBlueprintHelperReviewMaterialInstancePresenterModelPrivate::MakeRow(
				TEXT("Material Instance Parameters"),
				Summary,
				Summary,
				1));
		}
		return;
	}

	for (const FBlueprintHelperMaterialInstanceParameterSchemaEntry& Parameter : Schema)
	{
		const FString ParameterName = Parameter.ParameterInfo.Name.ToString();
		const FString ValueSummary =
			FBlueprintHelperReviewMaterialInstancePresenterModelPrivate::MakeParameterValueSummary(Parameter);
		const FString TypeName = BlueprintHelperMaterialInstanceParameterTypeToString(Parameter.Type);
		const FString CanonicalTargetKey = MakeParameterTargetKey(ParameterName, TypeName);
		const FString DisplayLabel = MakeParameterDisplayLabel(ParameterName, TypeName);
		TSharedRef<FBlueprintHelperReviewDataAssetRowItem> Row =
			FBlueprintHelperReviewMaterialInstancePresenterModelPrivate::MakeRow(
				ParameterName,
				ValueSummary,
				CanonicalTargetKey,
				1);
		AppendStableMatchKeys(
			ParameterName,
			TypeName,
			Row->SearchAliases,
			CanonicalTargetKey,
			ParameterName,
			DisplayLabel);
		FBlueprintHelperReviewMaterialInstancePresenterModelPrivate::AddAlias(Row, ParameterName);
		FBlueprintHelperReviewMaterialInstancePresenterModelPrivate::AddAlias(Row, TypeName);
		FBlueprintHelperReviewMaterialInstancePresenterModelPrivate::AddAlias(
			Row,
			BlueprintHelperMaterialInstanceParameterSourceToString(Parameter.Source));
		OutRows.Add(Row);
	}
}
