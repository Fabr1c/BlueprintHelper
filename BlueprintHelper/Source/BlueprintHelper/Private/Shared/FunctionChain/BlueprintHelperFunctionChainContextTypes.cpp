#include "Shared/FunctionChain/BlueprintHelperFunctionChainContextTypes.h"

#include "Dom/JsonValue.h"

TSharedRef<FJsonObject> FBlueprintHelperFunctionChainLogicRef::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("order"), Order);
	Json->SetNumberField(TEXT("depth"), Depth);
	Json->SetNumberField(TEXT("parent_order"), ParentOrder);
	Json->SetStringField(TEXT("asset_path"), AssetPath);
	Json->SetStringField(TEXT("target_type"), TargetType);
	Json->SetStringField(TEXT("target_name"), TargetName);
	if (!GraphName.IsEmpty())
	{
		Json->SetStringField(TEXT("graph_name"), GraphName);
	}
	Json->SetStringField(TEXT("call_kind"), CallKind);
	Json->SetStringField(TEXT("reason"), Reason);
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperFunctionChainIssue::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("code"), Code);
	Json->SetStringField(TEXT("message"), Message);
	if (!AssetPath.IsEmpty())
	{
		Json->SetStringField(TEXT("asset_path"), AssetPath);
	}
	if (!GraphName.IsEmpty())
	{
		Json->SetStringField(TEXT("graph_name"), GraphName);
	}
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperFunctionChainSummary::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("visited_nodes"), VisitedNodes);
	Json->SetNumberField(TEXT("returned_custom_refs"), ReturnedCustomRefs);
	Json->SetNumberField(TEXT("filtered_engine_or_trusted_plugin_calls"), FilteredEngineOrTrustedPluginCalls);
	Json->SetNumberField(TEXT("filtered_native_pure_calls"), FilteredNativePureCalls);
	Json->SetNumberField(TEXT("project_native_terminal_calls"), ProjectNativeTerminalCalls);
	Json->SetNumberField(TEXT("unresolved_calls"), UnresolvedCalls);
	Json->SetNumberField(TEXT("ambiguous_calls"), AmbiguousCalls);
	Json->SetNumberField(TEXT("cycle_count"), CycleCount);
	Json->SetBoolField(TEXT("truncated"), bTruncated);
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperFunctionChainContextPack::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), FBlueprintHelperFunctionChainContextProtocol::Schema);

	TArray<TSharedPtr<FJsonValue>> RefValues;
	for (const FBlueprintHelperFunctionChainLogicRef& Ref : CustomLogicRefs)
	{
		RefValues.Add(MakeShared<FJsonValueObject>(Ref.ToJson()));
	}
	Json->SetArrayField(TEXT("custom_logic_refs"), RefValues);
	Json->SetObjectField(TEXT("summary"), Summary.ToJson());

	TArray<TSharedPtr<FJsonValue>> UnresolvedValues;
	for (const FBlueprintHelperFunctionChainIssue& Issue : Unresolved)
	{
		UnresolvedValues.Add(MakeShared<FJsonValueObject>(Issue.ToJson()));
	}
	Json->SetArrayField(TEXT("unresolved"), UnresolvedValues);

	TArray<TSharedPtr<FJsonValue>> AmbiguousValues;
	for (const FBlueprintHelperFunctionChainIssue& Issue : Ambiguous)
	{
		AmbiguousValues.Add(MakeShared<FJsonValueObject>(Issue.ToJson()));
	}
	Json->SetArrayField(TEXT("ambiguous"), AmbiguousValues);

	return Json;
}
