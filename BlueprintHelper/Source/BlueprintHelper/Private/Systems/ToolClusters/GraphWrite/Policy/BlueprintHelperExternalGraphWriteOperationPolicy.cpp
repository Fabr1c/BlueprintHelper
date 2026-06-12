#include "Systems/ToolClusters/GraphWrite/Policy/BlueprintHelperExternalGraphWriteOperationPolicy.h"

const FString& FBlueprintHelperExternalGraphWriteOperationPolicy::ExternalGraphEditStrategy()
{
	static const FString Value(TEXT("external_graph_edit"));
	return Value;
}

const FString& FBlueprintHelperExternalGraphWriteOperationPolicy::MergeFlowAdapterOperation()
{
	static const FString Value(TEXT("merge_external_flow"));
	return Value;
}

const FString& FBlueprintHelperExternalGraphWriteOperationPolicy::PropertyPatchAdapterOperation()
{
	static const FString Value(TEXT("patch_external_graph"));
	return Value;
}

const FString& FBlueprintHelperExternalGraphWriteOperationPolicy::LinkPatchAdapterOperation()
{
	static const FString Value(TEXT("patch_external_links"));
	return Value;
}

const FString& FBlueprintHelperExternalGraphWriteOperationPolicy::BodyReplaceAdapterOperation()
{
	static const FString Value(TEXT("replace_external_body"));
	return Value;
}

bool FBlueprintHelperExternalGraphWriteOperationPolicy::IsExternalGraphEditStrategy(const FString& Strategy)
{
	return Strategy == ExternalGraphEditStrategy();
}

bool FBlueprintHelperExternalGraphWriteOperationPolicy::IsExternalPropertyPatchAdapterOperation(
	const FString& AdapterOperation)
{
	return AdapterOperation == PropertyPatchAdapterOperation();
}

bool FBlueprintHelperExternalGraphWriteOperationPolicy::IsExternalLinkPatchAdapterOperation(
	const FString& AdapterOperation)
{
	return AdapterOperation == LinkPatchAdapterOperation();
}

bool FBlueprintHelperExternalGraphWriteOperationPolicy::TryClassifyAdapterOperation(
	const FString& AdapterOperation,
	EBlueprintHelperExternalGraphWriteAdapterOperationKind& OutKind)
{
	OutKind = EBlueprintHelperExternalGraphWriteAdapterOperationKind::Unknown;
	if (AdapterOperation == MergeFlowAdapterOperation())
	{
		OutKind = EBlueprintHelperExternalGraphWriteAdapterOperationKind::MergeFlow;
		return true;
	}
	if (AdapterOperation == PropertyPatchAdapterOperation())
	{
		OutKind = EBlueprintHelperExternalGraphWriteAdapterOperationKind::PropertyPatch;
		return true;
	}
	if (AdapterOperation == LinkPatchAdapterOperation())
	{
		OutKind = EBlueprintHelperExternalGraphWriteAdapterOperationKind::LinkPatch;
		return true;
	}
	if (AdapterOperation == BodyReplaceAdapterOperation())
	{
		OutKind = EBlueprintHelperExternalGraphWriteAdapterOperationKind::BodyReplace;
		return true;
	}
	return false;
}

bool FBlueprintHelperExternalGraphWriteOperationPolicy::TryResolveTaskOpAdapterOperation(
	const FString& TaskOp,
	FString& OutAdapterOperation,
	EBlueprintHelperExternalGraphWriteAdapterOperationKind& OutKind)
{
	OutAdapterOperation.Reset();
	OutKind = EBlueprintHelperExternalGraphWriteAdapterOperationKind::Unknown;

	if (TaskOp == TEXT("insert_external_flow"))
	{
		OutAdapterOperation = MergeFlowAdapterOperation();
		OutKind = EBlueprintHelperExternalGraphWriteAdapterOperationKind::MergeFlow;
		return true;
	}
	if (TaskOp == TEXT("set_external_pin_default") ||
		TaskOp == TEXT("set_external_node_comment") ||
		TaskOp == TEXT("set_external_node_property"))
	{
		OutAdapterOperation = PropertyPatchAdapterOperation();
		OutKind = EBlueprintHelperExternalGraphWriteAdapterOperationKind::PropertyPatch;
		return true;
	}
	if (TaskOp == TEXT("connect_external_pins") ||
		TaskOp == TEXT("disconnect_external_link") ||
		TaskOp == TEXT("replace_external_link"))
	{
		OutAdapterOperation = LinkPatchAdapterOperation();
		OutKind = EBlueprintHelperExternalGraphWriteAdapterOperationKind::LinkPatch;
		return true;
	}
	if (TaskOp == TEXT("replace_external_body"))
	{
		OutAdapterOperation = BodyReplaceAdapterOperation();
		OutKind = EBlueprintHelperExternalGraphWriteAdapterOperationKind::BodyReplace;
		return true;
	}
	return false;
}

bool FBlueprintHelperExternalGraphWriteOperationPolicy::TryBuildExpectedMutationAllowlist(
	const FString& AdapterOperation,
	TArray<FString>& OutExpectedMutations)
{
	OutExpectedMutations.Reset();

	EBlueprintHelperExternalGraphWriteAdapterOperationKind Kind =
		EBlueprintHelperExternalGraphWriteAdapterOperationKind::Unknown;
	if (!TryClassifyAdapterOperation(AdapterOperation, Kind))
	{
		return false;
	}

	if (Kind == EBlueprintHelperExternalGraphWriteAdapterOperationKind::MergeFlow)
	{
		OutExpectedMutations.Add(TEXT("exec_boundary_link"));
	}
	else if (Kind == EBlueprintHelperExternalGraphWriteAdapterOperationKind::PropertyPatch)
	{
		OutExpectedMutations.Add(TEXT("pin_default"));
		OutExpectedMutations.Add(TEXT("node_comment"));
		OutExpectedMutations.Add(TEXT("node_property"));
	}
	else if (Kind == EBlueprintHelperExternalGraphWriteAdapterOperationKind::LinkPatch)
	{
		OutExpectedMutations.Add(TEXT("link_connect"));
		OutExpectedMutations.Add(TEXT("link_disconnect"));
		OutExpectedMutations.Add(TEXT("link_replace"));
	}
	else if (Kind == EBlueprintHelperExternalGraphWriteAdapterOperationKind::BodyReplace)
	{
		OutExpectedMutations.Add(TEXT("body_replace"));
	}

	return OutExpectedMutations.Num() > 0;
}
