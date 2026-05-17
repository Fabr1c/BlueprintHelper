#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/Class.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
#define BLUEPRINTHELPER_UE_HAS_FPACKAGE_METADATA 1
#define BLUEPRINTHELPER_UE_HAS_EALLOW_SHRINKING 1
#define BLUEPRINTHELPER_UE_HAS_CONST_SCRIPTSTRUCT_IMPORTTEXT 1
#define BLUEPRINTHELPER_UE_HAS_AUTOMATION_EXPECTED_ERROR_PLAIN 1
#else
#define BLUEPRINTHELPER_UE_HAS_FPACKAGE_METADATA 0
#define BLUEPRINTHELPER_UE_HAS_EALLOW_SHRINKING 0
#define BLUEPRINTHELPER_UE_HAS_CONST_SCRIPTSTRUCT_IMPORTTEXT 0
#define BLUEPRINTHELPER_UE_HAS_AUTOMATION_EXPECTED_ERROR_PLAIN 0
#endif

#if BLUEPRINTHELPER_UE_HAS_FPACKAGE_METADATA
using FBlueprintHelperPackageMetaData = FMetaData;
#else
using FBlueprintHelperPackageMetaData = UMetaData;
#endif

class FBlueprintHelperVersionCompat
{
public:
	static FORCEINLINE FBlueprintHelperPackageMetaData& GetPackageMetaData(UPackage* Package)
	{
#if BLUEPRINTHELPER_UE_HAS_FPACKAGE_METADATA
		return Package->GetMetaData();
#else
		return *Package->GetMetaData();
#endif
	}

	template<typename ArrayType>
	static FORCEINLINE auto PopNoShrink(ArrayType& Array) -> decltype(Array.Pop(false))
	{
#if BLUEPRINTHELPER_UE_HAS_EALLOW_SHRINKING
		return Array.Pop(EAllowShrinking::No);
#else
		return Array.Pop(false);
#endif
	}

	static FORCEINLINE void LeftInlineNoShrink(FString& String, const int32 Count)
	{
#if BLUEPRINTHELPER_UE_HAS_EALLOW_SHRINKING
		String.LeftInline(Count, EAllowShrinking::No);
#else
		String.LeftInline(Count, false);
#endif
	}

	template<typename ArrayType>
	static FORCEINLINE void RemoveAtSwapNoShrink(ArrayType& Array, const int32 Index, const int32 Count = 1)
	{
#if BLUEPRINTHELPER_UE_HAS_EALLOW_SHRINKING
		Array.RemoveAtSwap(Index, Count, EAllowShrinking::No);
#else
		Array.RemoveAtSwap(Index, Count, false);
#endif
	}

	static FORCEINLINE const TCHAR* ImportScriptStructText(
		const UScriptStruct* Struct,
		const TCHAR* Buffer,
		void* Data,
		UObject* OwnerObject,
		const int32 PortFlags,
		FOutputDevice* ErrorText,
		const FString& StructName)
	{
#if BLUEPRINTHELPER_UE_HAS_CONST_SCRIPTSTRUCT_IMPORTTEXT
		return Struct->ImportText(Buffer, Data, OwnerObject, PortFlags, ErrorText, StructName);
#else
		return const_cast<UScriptStruct*>(Struct)->ImportText(Buffer, Data, OwnerObject, PortFlags, ErrorText, StructName);
#endif
	}

	template<typename TestType, typename ExpectedErrorFlagsType>
	static FORCEINLINE void AddExpectedErrorPlainCompat(
		TestType& Test,
		const TCHAR* ExpectedPattern,
		const ExpectedErrorFlagsType MatchType,
		const int32 Occurrences)
	{
#if BLUEPRINTHELPER_UE_HAS_AUTOMATION_EXPECTED_ERROR_PLAIN
		Test.AddExpectedErrorPlain(ExpectedPattern, MatchType, Occurrences);
#else
		Test.AddExpectedError(ExpectedPattern, MatchType, Occurrences);
#endif
	}
};
