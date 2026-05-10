// BlueprintHelper Service Layer 。CleanupBlockTypes ToJson 实现

#include "Shared/CleanupOwnership/BlueprintHelperCleanupBlockTypes.h"

TSharedRef<FJsonObject> FBlueprintHelperCleanupDryRunResult::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("result"), Result);
	J->SetBoolField(TEXT("can_execute"), bCanExecute);
	if (BlockedBy.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> A;
		for (const auto& I : BlockedBy) A.Add(MakeShared<FJsonValueString>(I));
		J->SetArrayField(TEXT("blocked_by"), A);
	}
	if (Conflicts.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> A;
		for (const auto& C : Conflicts) A.Add(MakeShared<FJsonValueObject>(C.ToJson()));
		J->SetArrayField(TEXT("conflicts"), A);
	}
	if (Errors.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> A;
		for (const auto& E : Errors) A.Add(MakeShared<FJsonValueObject>(E.ToJson()));
		J->SetArrayField(TEXT("errors"), A);
	}
	return J;
}

TSharedRef<FJsonObject> FBlueprintHelperCleanupDryRunData::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("schema"), Schema);
	J->SetObjectField(TEXT("dry_run"), DryRun.ToJson());
	return J;
}
