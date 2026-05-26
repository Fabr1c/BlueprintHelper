#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"

namespace
{
static FString Normalize(const FString& Value)
{
	return Value.TrimStartAndEnd().ToLower();
}

static FBlueprintHelperContainerActionRoleBinding BindInputRole(const TCHAR* RoleName, const TCHAR* FunctionPinName)
{
	FBlueprintHelperContainerActionRoleBinding Binding;
	Binding.RoleName = RoleName;
	Binding.FunctionPinName = FunctionPinName;
	Binding.bProjectToCallableRequest = true;
	return Binding;
}

static FBlueprintHelperContainerActionRoleBinding BindOutputRole(const TCHAR* RoleName, const TCHAR* FunctionPinName)
{
	FBlueprintHelperContainerActionRoleBinding Binding;
	Binding.RoleName = RoleName;
	Binding.FunctionPinName = FunctionPinName;
	Binding.bProjectToCallableRequest = false;
	return Binding;
}

static FBlueprintHelperContainerActionWildcardPolicy MakeWildcardPolicy(TArray<FString> TypedRoles)
{
	FBlueprintHelperContainerActionWildcardPolicy Policy;
	Policy.TypedRoles = MoveTemp(TypedRoles);
	return Policy;
}

static FBlueprintHelperContainerActionSpec MakeSpec(
	const TCHAR* OperationId,
	const TCHAR* ContainerKind,
	const TCHAR* ContainerOperation,
	const TCHAR* StableUFunctionPath,
	TArray<FString> RequiredRoles,
	TArray<FBlueprintHelperContainerActionRoleBinding> RoleBindings,
	const EBlueprintHelperContainerActionResultKind ResultKind,
	FBlueprintHelperContainerActionWildcardPolicy WildcardPolicy,
	TArray<FString> ReadbackPinRoles,
	const bool bMutatesTarget,
	const bool bReturnsValue)
{
	FBlueprintHelperContainerActionSpec Spec;
	Spec.OperationId = OperationId;
	Spec.ContainerKind = ContainerKind;
	Spec.ContainerOperation = ContainerOperation;
	Spec.StableUFunctionPath = StableUFunctionPath;
	Spec.FunctionQuery = StableUFunctionPath;
	Spec.RequiredRoles = MoveTemp(RequiredRoles);
	Spec.RoleBindings = MoveTemp(RoleBindings);
	Spec.ResultKind = ResultKind;
	Spec.WildcardPolicy = MoveTemp(WildcardPolicy);
	Spec.ReadbackPinRoles = MoveTemp(ReadbackPinRoles);
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
		Result.Reserve(58);

		Result.Add(MakeSpec(TEXT("container.array.get"), TEXT("array"), TEXT("get"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Get"),
			{ TEXT("target"), TEXT("index") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("index"), TEXT("Index")), BindOutputRole(TEXT("result"), TEXT("Item")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("index"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.array.set"), TEXT("array"), TEXT("set"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Set"),
			{ TEXT("target"), TEXT("index"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("index"), TEXT("Index")), BindInputRole(TEXT("item"), TEXT("Item")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("index"), TEXT("item") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.add"), TEXT("array"), TEXT("add"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Add"),
			{ TEXT("target"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("item"), TEXT("NewItem")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("item"), TEXT("result") },
			true, true));
		Result.Add(MakeSpec(TEXT("container.array.add_unique"), TEXT("array"), TEXT("add_unique"), TEXT("/Script/Engine.KismetArrayLibrary:Array_AddUnique"),
			{ TEXT("target"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("item"), TEXT("NewItem")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("item"), TEXT("result") },
			true, true));
		Result.Add(MakeSpec(TEXT("container.array.append"), TEXT("array"), TEXT("append"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Append"),
			{ TEXT("target"), TEXT("items") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("items"), TEXT("SourceArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target"), TEXT("items") }),
			{ TEXT("target"), TEXT("items") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.insert"), TEXT("array"), TEXT("insert"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Insert"),
			{ TEXT("target"), TEXT("index"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("item"), TEXT("NewItem")), BindInputRole(TEXT("index"), TEXT("Index")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("index"), TEXT("item") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.remove_item"), TEXT("array"), TEXT("remove_item"), TEXT("/Script/Engine.KismetArrayLibrary:Array_RemoveItem"),
			{ TEXT("target"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("item"), TEXT("Item")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("item"), TEXT("result") },
			true, true));
		Result.Add(MakeSpec(TEXT("container.array.remove_index"), TEXT("array"), TEXT("remove_index"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Remove"),
			{ TEXT("target"), TEXT("index") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("index"), TEXT("IndexToRemove")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("index") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.clear"), TEXT("array"), TEXT("clear"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Clear"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.contains"), TEXT("array"), TEXT("contains"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Contains"),
			{ TEXT("target"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("item"), TEXT("ItemToFind")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("item"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.array.find"), TEXT("array"), TEXT("find"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Find"),
			{ TEXT("target"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("item"), TEXT("ItemToFind")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("item"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.array.length"), TEXT("array"), TEXT("length"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Length"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.array.shuffle"), TEXT("array"), TEXT("shuffle"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Shuffle"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.shuffle_from_stream"), TEXT("array"), TEXT("shuffle_from_stream"), TEXT("/Script/Engine.KismetArrayLibrary:Array_ShuffleFromStream"),
			{ TEXT("target"), TEXT("random_stream") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("random_stream"), TEXT("RandomStream")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("random_stream") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.identical"), TEXT("array"), TEXT("identical"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Identical"),
			{ TEXT("target"), TEXT("items") },
			{ BindInputRole(TEXT("target"), TEXT("ArrayA")), BindInputRole(TEXT("items"), TEXT("ArrayB")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("items") }),
			{ TEXT("target"), TEXT("items"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.array.resize"), TEXT("array"), TEXT("resize"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Resize"),
			{ TEXT("target"), TEXT("size") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("size"), TEXT("Size")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("size") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.reverse"), TEXT("array"), TEXT("reverse"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Reverse"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.is_empty"), TEXT("array"), TEXT("is_empty"), TEXT("/Script/Engine.KismetArrayLibrary:Array_IsEmpty"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.array.is_not_empty"), TEXT("array"), TEXT("is_not_empty"), TEXT("/Script/Engine.KismetArrayLibrary:Array_IsNotEmpty"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.array.last_index"), TEXT("array"), TEXT("last_index"), TEXT("/Script/Engine.KismetArrayLibrary:Array_LastIndex"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.array.swap"), TEXT("array"), TEXT("swap"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Swap"),
			{ TEXT("target"), TEXT("first_index"), TEXT("second_index") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("first_index"), TEXT("FirstIndex")), BindInputRole(TEXT("second_index"), TEXT("SecondIndex")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("first_index"), TEXT("second_index") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.filter_array"), TEXT("array"), TEXT("filter_array"), TEXT("/Script/Engine.KismetArrayLibrary:FilterArray"),
			{ TEXT("target"), TEXT("filter_class") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("filter_class"), TEXT("FilterClass")), BindOutputRole(TEXT("result"), TEXT("FilteredArray")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("filter_class"), TEXT("result") },
			false, false));
		Result.Add(MakeSpec(TEXT("container.array.is_valid_index"), TEXT("array"), TEXT("is_valid_index"), TEXT("/Script/Engine.KismetArrayLibrary:Array_IsValidIndex"),
			{ TEXT("target"), TEXT("index") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("index"), TEXT("IndexToTest")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("index"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.array.random"), TEXT("array"), TEXT("random"), TEXT("/Script/Engine.KismetArrayLibrary:Array_Random"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindOutputRole(TEXT("result"), TEXT("OutItem")), BindOutputRole(TEXT("index"), TEXT("OutIndex")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result"), TEXT("index") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.array.random_from_stream"), TEXT("array"), TEXT("random_from_stream"), TEXT("/Script/Engine.KismetArrayLibrary:Array_RandomFromStream"),
			{ TEXT("target"), TEXT("random_stream") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")), BindInputRole(TEXT("random_stream"), TEXT("RandomStream")), BindOutputRole(TEXT("result"), TEXT("OutItem")), BindOutputRole(TEXT("index"), TEXT("OutIndex")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("random_stream"), TEXT("result"), TEXT("index") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.array.sort_string"), TEXT("array"), TEXT("sort_string"), TEXT("/Script/Engine.KismetArrayLibrary:SortStringArray"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.sort_name"), TEXT("array"), TEXT("sort_name"), TEXT("/Script/Engine.KismetArrayLibrary:SortNameArray"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.sort_byte"), TEXT("array"), TEXT("sort_byte"), TEXT("/Script/Engine.KismetArrayLibrary:SortByteArray"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.sort_int"), TEXT("array"), TEXT("sort_int"), TEXT("/Script/Engine.KismetArrayLibrary:SortIntArray"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.sort_int64"), TEXT("array"), TEXT("sort_int64"), TEXT("/Script/Engine.KismetArrayLibrary:SortInt64Array"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.array.sort_float"), TEXT("array"), TEXT("sort_float"), TEXT("/Script/Engine.KismetArrayLibrary:SortFloatArray"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetArray")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));

		Result.Add(MakeSpec(TEXT("container.map.add"), TEXT("map"), TEXT("add"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Add"),
			{ TEXT("target"), TEXT("key"), TEXT("value") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindInputRole(TEXT("key"), TEXT("Key")), BindInputRole(TEXT("value"), TEXT("Value")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target"), TEXT("key"), TEXT("value") }),
			{ TEXT("target"), TEXT("key"), TEXT("value") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.map.remove"), TEXT("map"), TEXT("remove"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Remove"),
			{ TEXT("target"), TEXT("key") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindInputRole(TEXT("key"), TEXT("Key")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("key") }),
			{ TEXT("target"), TEXT("key"), TEXT("result") },
			true, true));
		Result.Add(MakeSpec(TEXT("container.map.find"), TEXT("map"), TEXT("find"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Find"),
			{ TEXT("target"), TEXT("key") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindInputRole(TEXT("key"), TEXT("Key")), BindOutputRole(TEXT("result"), TEXT("Value")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target"), TEXT("key") }),
			{ TEXT("target"), TEXT("key"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.map.contains"), TEXT("map"), TEXT("contains"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Contains"),
			{ TEXT("target"), TEXT("key") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindInputRole(TEXT("key"), TEXT("Key")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("key") }),
			{ TEXT("target"), TEXT("key"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.map.keys"), TEXT("map"), TEXT("keys"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Keys"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindOutputRole(TEXT("result"), TEXT("Keys")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.map.values"), TEXT("map"), TEXT("values"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Values"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindOutputRole(TEXT("result"), TEXT("Values")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.map.clear"), TEXT("map"), TEXT("clear"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Clear"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.map.length"), TEXT("map"), TEXT("length"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_Length"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.map.is_empty"), TEXT("map"), TEXT("is_empty"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_IsEmpty"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.map.is_not_empty"), TEXT("map"), TEXT("is_not_empty"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_IsNotEmpty"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.map.get_key_value_by_index"), TEXT("map"), TEXT("get_key_value_by_index"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_GetKeyValueByIndex"),
			{ TEXT("target"), TEXT("index") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindInputRole(TEXT("index"), TEXT("Index")), BindOutputRole(TEXT("key"), TEXT("Key")), BindOutputRole(TEXT("value"), TEXT("Value")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("index"), TEXT("key"), TEXT("value") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.map.get_last_index"), TEXT("map"), TEXT("get_last_index"), TEXT("/Script/Engine.BlueprintMapLibrary:Map_GetLastIndex"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetMap")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));

		Result.Add(MakeSpec(TEXT("container.set.add"), TEXT("set"), TEXT("add"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Add"),
			{ TEXT("target"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindInputRole(TEXT("item"), TEXT("NewItem")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("item") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.set.remove"), TEXT("set"), TEXT("remove"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Remove"),
			{ TEXT("target"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindInputRole(TEXT("item"), TEXT("Item")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("item"), TEXT("result") },
			true, true));
		Result.Add(MakeSpec(TEXT("container.set.contains"), TEXT("set"), TEXT("contains"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Contains"),
			{ TEXT("target"), TEXT("item") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindInputRole(TEXT("item"), TEXT("ItemToFind")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target"), TEXT("item") }),
			{ TEXT("target"), TEXT("item"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.set.clear"), TEXT("set"), TEXT("clear"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Clear"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.set.length"), TEXT("set"), TEXT("length"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Length"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.set.to_array"), TEXT("set"), TEXT("to_array"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_ToArray"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("A")), BindOutputRole(TEXT("result"), TEXT("Result")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.set.add_items"), TEXT("set"), TEXT("add_items"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_AddItems"),
			{ TEXT("target"), TEXT("items") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindInputRole(TEXT("items"), TEXT("NewItems")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target"), TEXT("items") }),
			{ TEXT("target"), TEXT("items") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.set.remove_items"), TEXT("set"), TEXT("remove_items"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_RemoveItems"),
			{ TEXT("target"), TEXT("items") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindInputRole(TEXT("items"), TEXT("Items")) },
			EBlueprintHelperContainerActionResultKind::None,
			MakeWildcardPolicy({ TEXT("target"), TEXT("items") }),
			{ TEXT("target"), TEXT("items") },
			true, false));
		Result.Add(MakeSpec(TEXT("container.set.is_empty"), TEXT("set"), TEXT("is_empty"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_IsEmpty"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.set.is_not_empty"), TEXT("set"), TEXT("is_not_empty"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_IsNotEmpty"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.set.intersection"), TEXT("set"), TEXT("intersection"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Intersection"),
			{ TEXT("target"), TEXT("other"), TEXT("result") },
			{ BindInputRole(TEXT("target"), TEXT("A")), BindInputRole(TEXT("other"), TEXT("B")), BindOutputRole(TEXT("result"), TEXT("Result")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target"), TEXT("other") }),
			{ TEXT("target"), TEXT("other"), TEXT("result") },
			false, false));
		Result.Add(MakeSpec(TEXT("container.set.union"), TEXT("set"), TEXT("union"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Union"),
			{ TEXT("target"), TEXT("other"), TEXT("result") },
			{ BindInputRole(TEXT("target"), TEXT("A")), BindInputRole(TEXT("other"), TEXT("B")), BindOutputRole(TEXT("result"), TEXT("Result")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target"), TEXT("other") }),
			{ TEXT("target"), TEXT("other"), TEXT("result") },
			false, false));
		Result.Add(MakeSpec(TEXT("container.set.difference"), TEXT("set"), TEXT("difference"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_Difference"),
			{ TEXT("target"), TEXT("other"), TEXT("result") },
			{ BindInputRole(TEXT("target"), TEXT("A")), BindInputRole(TEXT("other"), TEXT("B")), BindOutputRole(TEXT("result"), TEXT("Result")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target"), TEXT("other") }),
			{ TEXT("target"), TEXT("other"), TEXT("result") },
			false, false));
		Result.Add(MakeSpec(TEXT("container.set.get_item_by_index"), TEXT("set"), TEXT("get_item_by_index"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_GetItemByIndex"),
			{ TEXT("target"), TEXT("index") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindInputRole(TEXT("index"), TEXT("Index")), BindOutputRole(TEXT("result"), TEXT("Item")) },
			EBlueprintHelperContainerActionResultKind::OutputPins,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("index"), TEXT("result") },
			false, true));
		Result.Add(MakeSpec(TEXT("container.set.get_last_index"), TEXT("set"), TEXT("get_last_index"), TEXT("/Script/Engine.BlueprintSetLibrary:Set_GetLastIndex"),
			{ TEXT("target") },
			{ BindInputRole(TEXT("target"), TEXT("TargetSet")), BindOutputRole(TEXT("result"), TEXT("ReturnValue")) },
			EBlueprintHelperContainerActionResultKind::ReturnValue,
			MakeWildcardPolicy({ TEXT("target") }),
			{ TEXT("target"), TEXT("result") },
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
