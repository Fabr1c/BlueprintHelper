#include "Shared/BlueprintClassSettings/BlueprintHelperClassDefaultMutationTypes.h"

const TCHAR* ToString(EBlueprintHelperClassDefaultMutationStrategy Strategy)
{
	switch (Strategy)
	{
	case EBlueprintHelperClassDefaultMutationStrategy::DirectProperty:
		return TEXT("direct_property");
	case EBlueprintHelperClassDefaultMutationStrategy::SetterAwareProperty:
		return TEXT("setter_aware_property");
	case EBlueprintHelperClassDefaultMutationStrategy::Blocked:
	default:
		return TEXT("blocked");
	}
}

EBlueprintHelperClassDefaultMutationStrategy BlueprintHelperClassDefaultMutationStrategyFromString(
	const FString& Strategy)
{
	if (Strategy.IsEmpty() || Strategy.Equals(TEXT("direct_property"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperClassDefaultMutationStrategy::DirectProperty;
	}
	if (Strategy.Equals(TEXT("setter_aware_property"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperClassDefaultMutationStrategy::SetterAwareProperty;
	}
	return EBlueprintHelperClassDefaultMutationStrategy::Blocked;
}

TSharedRef<FJsonObject> FBlueprintHelperClassDefaultSetterMutationEvidence::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), Schema);
	if (!AssetPath.IsEmpty()) Json->SetStringField(TEXT("asset_path"), AssetPath);
	if (!OwnerRoot.IsEmpty()) Json->SetStringField(TEXT("owner_root"), OwnerRoot);
	if (!OwnerObjectPath.IsEmpty()) Json->SetStringField(TEXT("owner_object_path"), OwnerObjectPath);
	if (!OwnerObjectClass.IsEmpty()) Json->SetStringField(TEXT("owner_object_class"), OwnerObjectClass);
	if (!PropertyPath.IsEmpty()) Json->SetStringField(TEXT("property_path"), PropertyPath);
	if (!LeafPropertyName.IsEmpty()) Json->SetStringField(TEXT("leaf_property_name"), LeafPropertyName);
	if (!MutationStrategy.IsEmpty()) Json->SetStringField(TEXT("mutation_strategy"), MutationStrategy);
	if (!SetterFunction.IsEmpty()) Json->SetStringField(TEXT("setter_function"), SetterFunction);
	if (!GetterFunction.IsEmpty()) Json->SetStringField(TEXT("getter_function"), GetterFunction);
	if (!ExpectedType.IsEmpty()) Json->SetStringField(TEXT("expected_type"), ExpectedType);
	if (!InputValue.IsEmpty()) Json->SetStringField(TEXT("input_value"), InputValue);
	if (!BeforeValue.IsEmpty()) Json->SetStringField(TEXT("before_value"), BeforeValue);
	if (!AfterValue.IsEmpty()) Json->SetStringField(TEXT("after_value"), AfterValue);
	if (!PropertyFlags.IsEmpty()) Json->SetStringField(TEXT("property_flags"), PropertyFlags);
	if (Diagnostics.Num() > 0)
	{
		Json->SetArrayField(TEXT("diagnostics"), BlueprintHelperDiagnosticItemsToJsonArray(Diagnostics));
	}
	return Json;
}

bool FBlueprintHelperClassDefaultSetterMutationEvidence::FromJson(
	const TSharedPtr<FJsonObject>& Json,
	FBlueprintHelperClassDefaultSetterMutationEvidence& OutEvidence)
{
	if (!Json.IsValid())
	{
		return false;
	}

	Json->TryGetStringField(TEXT("schema"), OutEvidence.Schema);
	Json->TryGetStringField(TEXT("asset_path"), OutEvidence.AssetPath);
	Json->TryGetStringField(TEXT("owner_root"), OutEvidence.OwnerRoot);
	Json->TryGetStringField(TEXT("owner_object_path"), OutEvidence.OwnerObjectPath);
	Json->TryGetStringField(TEXT("owner_object_class"), OutEvidence.OwnerObjectClass);
	Json->TryGetStringField(TEXT("property_path"), OutEvidence.PropertyPath);
	Json->TryGetStringField(TEXT("leaf_property_name"), OutEvidence.LeafPropertyName);
	Json->TryGetStringField(TEXT("mutation_strategy"), OutEvidence.MutationStrategy);
	Json->TryGetStringField(TEXT("setter_function"), OutEvidence.SetterFunction);
	Json->TryGetStringField(TEXT("getter_function"), OutEvidence.GetterFunction);
	Json->TryGetStringField(TEXT("expected_type"), OutEvidence.ExpectedType);
	Json->TryGetStringField(TEXT("input_value"), OutEvidence.InputValue);
	Json->TryGetStringField(TEXT("before_value"), OutEvidence.BeforeValue);
	Json->TryGetStringField(TEXT("after_value"), OutEvidence.AfterValue);
	Json->TryGetStringField(TEXT("property_flags"), OutEvidence.PropertyFlags);
	BlueprintHelperReadDiagnosticArrayField(Json, TEXT("diagnostics"), OutEvidence.Diagnostics);
	return OutEvidence.Schema.Equals(TEXT("BlueprintHelper.ClassDefaultSetterMutationEvidence.v1"));
}
