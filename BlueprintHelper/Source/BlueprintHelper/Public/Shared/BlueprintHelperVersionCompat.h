#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
#define BLUEPRINTHELPER_UE_HAS_FPACKAGE_METADATA 1
#else
#define BLUEPRINTHELPER_UE_HAS_FPACKAGE_METADATA 0
#endif

#if BLUEPRINTHELPER_UE_HAS_FPACKAGE_METADATA
using FBlueprintHelperPackageMetaData = FMetaData;
#else
using FBlueprintHelperPackageMetaData = UMetaData;
#endif

namespace FBlueprintHelperVersionCompat
{
	FORCEINLINE FBlueprintHelperPackageMetaData& GetPackageMetaData(UPackage* Package)
	{
#if BLUEPRINTHELPER_UE_HAS_FPACKAGE_METADATA
		return Package->GetMetaData();
#else
		return *Package->GetMetaData();
#endif
	}

	template<typename ArrayType>
	FORCEINLINE auto PopNoShrink(ArrayType& Array) -> decltype(Array.Pop(false))
	{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
		return Array.Pop(EAllowShrinking::No);
#else
		return Array.Pop(false);
#endif
	}
}
