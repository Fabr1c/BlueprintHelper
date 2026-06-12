// BlueprintHelper Service Layer - External node property descriptor registry.

#pragma once

#include "CoreMinimal.h"

struct BLUEPRINTHELPER_API FBlueprintHelperExternalNodePropertyDescriptor
{
	FString DescriptorId;
	FString FieldKind;
	FString DisplayName;
	bool bWritable = false;
	FString DisabledReason;

	bool IsWritable() const
	{
		return bWritable;
	}
};

class BLUEPRINTHELPER_API FBlueprintHelperExternalNodePropertyDescriptorRegistry
{
public:
	static const FBlueprintHelperExternalNodePropertyDescriptor* Find(const FString& DescriptorId);
	static const TArray<FBlueprintHelperExternalNodePropertyDescriptor>& GetDescriptors();
	static FString ResolveFieldKind(const FString& DescriptorIdOrFieldKind);
};
