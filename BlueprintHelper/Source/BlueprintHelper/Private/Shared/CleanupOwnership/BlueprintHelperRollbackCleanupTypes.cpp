// BlueprintHelper Service Layer 。RollbackCleanupTypes ToJson

#include "Shared/CleanupOwnership/BlueprintHelperRollbackCleanupTypes.h"

TSharedRef<FJsonObject> FBlueprintHelperRollbackDryRunResult::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("result"), Result);
	J->SetBoolField(TEXT("can_execute"), bCanExecute);
	J->SetObjectField(TEXT("rollback_summary"), RollbackSummary.ToJson());
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

TSharedRef<FJsonObject> FBlueprintHelperRollbackCleanupDryRunData::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("schema"), Schema);
	J->SetObjectField(TEXT("dry_run"), DryRun.ToJson());
	return J;
}
