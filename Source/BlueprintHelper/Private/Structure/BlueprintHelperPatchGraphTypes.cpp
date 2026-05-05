// BlueprintHelper Service Layer — PatchGraphTypes ToJson 实现

#include "Structure/BlueprintHelperPatchGraphTypes.h"
#include "Structure/BlueprintHelperReplaceGraphTypes.h"

TSharedRef<FJsonObject> FBlueprintHelperPatchDryRunResult::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("result"), Result);
	Json->SetBoolField(TEXT("can_execute"), bCanExecute);
	if (BlockedBy.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FString& Item : BlockedBy) { Arr.Add(MakeShared<FJsonValueString>(Item)); }
		Json->SetArrayField(TEXT("blocked_by"), Arr);
	}
	if (Conflicts.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const auto& C : Conflicts) { Arr.Add(MakeShared<FJsonValueObject>(C.ToJson())); }
		Json->SetArrayField(TEXT("conflicts"), Arr);
	}
	if (Errors.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const auto& E : Errors) { Arr.Add(MakeShared<FJsonValueObject>(E.ToJson())); }
		Json->SetArrayField(TEXT("errors"), Arr);
	}
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperPatchDryRunData::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), Schema);
	Json->SetObjectField(TEXT("dry_run"), DryRun.ToJson());
	return Json;
}
