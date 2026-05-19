// Read cache policy for BlueprintHelper pure-data boundaries.

#pragma once

#include "CoreMinimal.h"

enum class EBlueprintHelperReadCacheDataKind : uint8
{
	CliSchemaMetadata,
	ReadCapabilityMatrix,
	RuntimeProfileStaticInfo,
	FormatterRegistryMetadata,
	AssetGraphSnapshot,
	UObjectPointer,
	BlueprintObjectPointer,
	GraphObjectPointer,
	WidgetTreeObjectPointer,
	PropertyReflectionResult
};

class BLUEPRINTHELPER_API FBlueprintHelperReadCachePolicy
{
public:
	static bool IsPersistentCacheAllowed(EBlueprintHelperReadCacheDataKind Kind);
	static bool IsRequestLocalOnly(EBlueprintHelperReadCacheDataKind Kind);
	static FString Describe(EBlueprintHelperReadCacheDataKind Kind);
};
