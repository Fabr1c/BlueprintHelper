// BlueprintHelper Service Layer - External node property descriptor registry.

#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalNodePropertyDescriptorRegistry.h"

class FBlueprintHelperExternalNodePropertyDescriptorRegistryLocalUtils
{
public:
	static FBlueprintHelperExternalNodePropertyDescriptor MakeDescriptor(
		const TCHAR* DescriptorId,
		const TCHAR* FieldKind,
		const TCHAR* DisplayName,
		bool bWritable,
		const TCHAR* DisabledReason = TEXT(""))
	{
		FBlueprintHelperExternalNodePropertyDescriptor Descriptor;
		Descriptor.DescriptorId = DescriptorId;
		Descriptor.FieldKind = FieldKind;
		Descriptor.DisplayName = DisplayName;
		Descriptor.bWritable = bWritable;
		Descriptor.DisabledReason = DisabledReason;
		return Descriptor;
	}
};

const TArray<FBlueprintHelperExternalNodePropertyDescriptor>&
FBlueprintHelperExternalNodePropertyDescriptorRegistry::GetDescriptors()
{
	static const TArray<FBlueprintHelperExternalNodePropertyDescriptor> Descriptors = {
		FBlueprintHelperExternalNodePropertyDescriptorRegistryLocalUtils::MakeDescriptor(
			TEXT("k2.node.comment"),
			TEXT("node_comment"),
			TEXT("K2 node comment"),
			true),
		FBlueprintHelperExternalNodePropertyDescriptorRegistryLocalUtils::MakeDescriptor(
			TEXT("k2.call.function_target"),
			TEXT("call_function_target"),
			TEXT("K2 call function target"),
			false,
			TEXT("call function target mutation is reserved for a future descriptor adapter.")),
		FBlueprintHelperExternalNodePropertyDescriptorRegistryLocalUtils::MakeDescriptor(
			TEXT("k2.field.member_reference"),
			TEXT("field_member_reference"),
			TEXT("K2 field member reference"),
			false,
			TEXT("field member reference mutation is reserved for a future descriptor adapter.")),
	};
	return Descriptors;
}

const FBlueprintHelperExternalNodePropertyDescriptor*
FBlueprintHelperExternalNodePropertyDescriptorRegistry::Find(const FString& DescriptorId)
{
	if (DescriptorId.IsEmpty())
	{
		return nullptr;
	}

	for (const FBlueprintHelperExternalNodePropertyDescriptor& Descriptor : GetDescriptors())
	{
		if (Descriptor.DescriptorId.Equals(DescriptorId, ESearchCase::IgnoreCase))
		{
			return &Descriptor;
		}
	}
	return nullptr;
}

FString FBlueprintHelperExternalNodePropertyDescriptorRegistry::ResolveFieldKind(
	const FString& DescriptorIdOrFieldKind)
{
	if (const FBlueprintHelperExternalNodePropertyDescriptor* Descriptor = Find(DescriptorIdOrFieldKind))
	{
		return Descriptor->FieldKind;
	}
	return DescriptorIdOrFieldKind;
}
