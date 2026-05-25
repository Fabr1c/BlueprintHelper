#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"

namespace
{
static FString Normalize(const FString& Value)
{
	return Value.TrimStartAndEnd().ToLower();
}

static FBlueprintHelperContainerActionRoleBinding BindRole(const TCHAR* RoleName, const TCHAR* FunctionPinName)
{
	FBlueprintHelperContainerActionRoleBinding Binding;
	Binding.RoleName = RoleName;
	Binding.FunctionPinName = FunctionPinName;
	return Binding;
}

static FBlueprintHelperContainerActionSpec MakeSpec(
	const TCHAR* OperationId,
	const TCHAR* ContainerKind,
	const TCHAR* ContainerOperation,
	const TCHAR* FunctionQuery,
	TArray<FString> RequiredRoles,
	TArray<FBlueprintHelperContainerActionRoleBinding> RoleBindings,
	const bool bMutatesTarget,
	const bool bReturnsValue)
{
	FBlueprintHelperContainerActionSpec Spec;
	Spec.OperationId = OperationId;
	Spec.ContainerKind = ContainerKind;
	Spec.ContainerOperation = ContainerOperation;
	Spec.FunctionQuery = FunctionQuery;
	Spec.RequiredRoles = MoveTemp(RequiredRoles);
	Spec.RoleBindings = MoveTemp(RoleBindings);
	Spec.bMutatesTarget = bMutatesTarget;
	Spec.bReturnsValue = bReturnsValue;
	Spec.bPureQuery = bReturnsValue && !bMutatesTarget;
	return Spec;
}

static const TArray<FBlueprintHelperContainerActionSpec>& Specs()
{
	static const TArray<FBlueprintHelperContainerActionSpec> Items = []()
	{
		TArray<FBlueprintHelperContainerActionSpec> Result;
		Result.Reserve(26);

		Result.Add(MakeSpec(TEXT("container.array.get"), TEXT("array"), TEXT("get"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Get"),
			{ TEXT("target"), TEXT("index") },
			{ BindRole(TEXT("target"), TEXT("TargetArray")), BindRole(TEXT("index"), TEXT("Index")) },
			false, true));
		Result.Add(MakeSpec(TEXT("container.array.set"), TEXT("array"), TEXT("set"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Set"),
			{ TEXT("target"), TEXT("index"), TEXT("item") },
			{ BindRole(TEXT("target"), TEXT("TargetArray")), BindRole(TEXT("index"), TEXT("Index")), BindRole(TEXT("item"), TEXT("Item")) },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.add"), TEXT("array"), TEXT("add"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Add"),
			{ TEXT("target"), TEXT("item") },
			{ BindRole(TEXT("target"), TEXT("TargetArray")), BindRole(TEXT("item"), TEXT("NewItem")) },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.add_unique"), TEXT("array"), TEXT("add_unique"), TEXT("/Script/Engine.KismetArrayLibrary:Array_AddUnique"),
			{ TEXT("target"), TEXT("item") },
			{ BindRole(TEXT("target"), TEXT("TargetArray")), BindRole(TEXT("item"), TEXT("NewItem")) },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.append"), TEXT("array"), TEXT("append"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Append"),
			{ TEXT("target"), TEXT("items") },
			{ BindRole(TEXT("target"), TEXT("TargetArray")), BindRole(TEXT("items"), TEXT("SourceArray")) },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.insert"), TEXT("array"), TEXT("insert"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Insert"),
			{ TEXT("target"), TEXT("index"), TEXT("item") },
			{ BindRole(TEXT("target"), TEXT("TargetArray")), BindRole(TEXT("index"), TEXT("Index")), BindRole(TEXT("item"), TEXT("NewItem")) },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.remove_item"), TEXT("array"), TEXT("remove_item"), TEXT("/Script/Engine.KismetArrayLibrary:Array_RemoveItem"),
			{ TEXT("target"), TEXT("item") },
			{ BindRole(TEXT("target"), TEXT("TargetArray")), BindRole(TEXT("item"), TEXT("Item")) },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.remove_index"), TEXT("array"), TEXT("remove_index"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Remove"),
			{ TEXT("target"), TEXT("index") },
			{ BindRole(TEXT("target"), TEXT("TargetArray")), BindRole(TEXT("index"), TEXT("IndexToRemove")) },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.clear"), TEXT("array"), TEXT("clear"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Clear"),
			{ TEXT("target") },
			{ BindRole(TEXT("target"), TEXT("TargetArray")) },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.contains"), TEXT("array"), TEXT("contains"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Contains"),
			{ TEXT("target"), TEXT("item") },
			{ BindRole(TEXT("target"), TEXT("TargetArray")), BindRole(TEXT("item"), TEXT("ItemToFind")) },
			false, true));
		Result.Add(MakeSpec(TEXT("container.array.find"), TEXT("array"), TEXT("find"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Find"),
			{ TEXT("target"), TEXT("item") },
			{ BindRole(TEXT("target"), TEXT("TargetArray")), BindRole(TEXT("item"), TEXT("ItemToFind")) },
			false, true));
		Result.Add(MakeSpec(TEXT("container.array.length"), TEXT("array"), TEXT("length"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Length"),
			{ TEXT("target") },
			{ BindRole(TEXT("target"), TEXT("TargetArray")) },
			false, true));

		Result.Add(MakeSpec(TEXT("container.map.add"), TEXT("map"), TEXT("add"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Add"),
			{ TEXT("target"), TEXT("key"), TEXT("value") },
			{ BindRole(TEXT("target"), TEXT("TargetMap")), BindRole(TEXT("key"), TEXT("Key")), BindRole(TEXT("value"), TEXT("Value")) },
			true, false));
		Result.Add(MakeSpec(TEXT("container.map.remove"), TEXT("map"), TEXT("remove"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Remove"),
			{ TEXT("target"), TEXT("key") },
			{ BindRole(TEXT("target"), TEXT("TargetMap")), BindRole(TEXT("key"), TEXT("Key")) },
			true, false));
		Result.Add(MakeSpec(TEXT("container.map.find"), TEXT("map"), TEXT("find"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Find"),
			{ TEXT("target"), TEXT("key") },
			{ BindRole(TEXT("target"), TEXT("TargetMap")), BindRole(TEXT("key"), TEXT("Key")) },
			false, true));
		Result.Add(MakeSpec(TEXT("container.map.contains"), TEXT("map"), TEXT("contains"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Contains"),
			{ TEXT("target"), TEXT("key") },
			{ BindRole(TEXT("target"), TEXT("TargetMap")), BindRole(TEXT("key"), TEXT("Key")) },
			false, true));
		Result.Add(MakeSpec(TEXT("container.map.keys"), TEXT("map"), TEXT("keys"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Keys"),
			{ TEXT("target") },
			{ BindRole(TEXT("target"), TEXT("TargetMap")) },
			false, true));
		Result.Add(MakeSpec(TEXT("container.map.values"), TEXT("map"), TEXT("values"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Values"),
			{ TEXT("target") },
			{ BindRole(TEXT("target"), TEXT("TargetMap")) },
			false, true));
		Result.Add(MakeSpec(TEXT("container.map.clear"), TEXT("map"), TEXT("clear"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Clear"),
			{ TEXT("target") },
			{ BindRole(TEXT("target"), TEXT("TargetMap")) },
			true, false));
		Result.Add(MakeSpec(TEXT("container.map.length"), TEXT("map"), TEXT("length"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Length"),
			{ TEXT("target") },
			{ BindRole(TEXT("target"), TEXT("TargetMap")) },
			false, true));

		Result.Add(MakeSpec(TEXT("container.set.add"), TEXT("set"), TEXT("add"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Add"),
			{ TEXT("target"), TEXT("item") },
			{ BindRole(TEXT("target"), TEXT("TargetSet")), BindRole(TEXT("item"), TEXT("NewItem")) },
			true, false));
		Result.Add(MakeSpec(TEXT("container.set.remove"), TEXT("set"), TEXT("remove"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Remove"),
			{ TEXT("target"), TEXT("item") },
			{ BindRole(TEXT("target"), TEXT("TargetSet")), BindRole(TEXT("item"), TEXT("Item")) },
			true, false));
		Result.Add(MakeSpec(TEXT("container.set.contains"), TEXT("set"), TEXT("contains"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Contains"),
			{ TEXT("target"), TEXT("item") },
			{ BindRole(TEXT("target"), TEXT("TargetSet")), BindRole(TEXT("item"), TEXT("ItemToFind")) },
			false, true));
		Result.Add(MakeSpec(TEXT("container.set.clear"), TEXT("set"), TEXT("clear"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Clear"),
			{ TEXT("target") },
			{ BindRole(TEXT("target"), TEXT("TargetSet")) },
			true, false));
		Result.Add(MakeSpec(TEXT("container.set.length"), TEXT("set"), TEXT("length"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Length"),
			{ TEXT("target") },
			{ BindRole(TEXT("target"), TEXT("TargetSet")) },
			false, true));
		Result.Add(MakeSpec(TEXT("container.set.to_array"), TEXT("set"), TEXT("to_array"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_ToArray"),
			{ TEXT("target") },
			{ BindRole(TEXT("target"), TEXT("A")) },
			false, true));

		return Result;
	}();
	return Items;
}
} // namespace

const FBlueprintHelperContainerActionSpec* FBlueprintHelperContainerActionVocabulary::Find(
	const FString& ContainerKind,
	const FString& ContainerOperation)
{
	const FString Kind = Normalize(ContainerKind);
	const FString Operation = Normalize(ContainerOperation);
	for (const FBlueprintHelperContainerActionSpec& Spec : Specs())
	{
		if (Normalize(Spec.ContainerKind) == Kind && Normalize(Spec.ContainerOperation) == Operation)
		{
			return &Spec;
		}
	}
	return nullptr;
}

TArray<FBlueprintHelperContainerActionSpec> FBlueprintHelperContainerActionVocabulary::All()
{
	return Specs();
}
