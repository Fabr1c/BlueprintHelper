#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "RHIFeatureLevel.h"
#include "RHIShaderPlatform.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/Class.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6 && ENGINE_MINOR_VERSION <= 8
#define BLUEPRINTHELPER_UE_HAS_FPACKAGE_METADATA 1
#define BLUEPRINTHELPER_UE_HAS_EALLOW_SHRINKING 1
#define BLUEPRINTHELPER_UE_HAS_CONST_SCRIPTSTRUCT_IMPORTTEXT 1
#define BLUEPRINTHELPER_UE_HAS_AUTOMATION_EXPECTED_ERROR_PLAIN 1
#define BLUEPRINTHELPER_UE_HAS_COMPONENT_BOUND_EVENT_GETTER 1
#define BLUEPRINTHELPER_UE_HAS_INLINE_EDITABLE_TEXT_DIRECT_GETTEXT 1
#define BLUEPRINTHELPER_UE_HAS_DETAILS_VIEW_SCROLL_PROPERTY_BOOL 1
#define BLUEPRINTHELPER_UE_HAS_SWITCH_ENUM_SETTER 1
#else
#define BLUEPRINTHELPER_UE_HAS_FPACKAGE_METADATA 0
#define BLUEPRINTHELPER_UE_HAS_EALLOW_SHRINKING 0
#define BLUEPRINTHELPER_UE_HAS_CONST_SCRIPTSTRUCT_IMPORTTEXT 0
#define BLUEPRINTHELPER_UE_HAS_AUTOMATION_EXPECTED_ERROR_PLAIN 0
#define BLUEPRINTHELPER_UE_HAS_COMPONENT_BOUND_EVENT_GETTER 0
#define BLUEPRINTHELPER_UE_HAS_INLINE_EDITABLE_TEXT_DIRECT_GETTEXT 0
#define BLUEPRINTHELPER_UE_HAS_DETAILS_VIEW_SCROLL_PROPERTY_BOOL 0
#define BLUEPRINTHELPER_UE_HAS_SWITCH_ENUM_SETTER 0
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7 && ENGINE_MINOR_VERSION <= 8
#define BLUEPRINTHELPER_UE_HAS_STRING_OUTPUT_DEVICE 1
#else
#define BLUEPRINTHELPER_UE_HAS_STRING_OUTPUT_DEVICE 0
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5 && ENGINE_MINOR_VERSION <= 8
#define BLUEPRINTHELPER_UE_HAS_PIN_LINK_DIRTY_FLAG 1
#else
#define BLUEPRINTHELPER_UE_HAS_PIN_LINK_DIRTY_FLAG 0
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6 && ENGINE_MINOR_VERSION <= 8
#define BLUEPRINTHELPER_UE_HAS_SOURCE_CONTROL_BATCH_QUERY 1
#define BLUEPRINTHELPER_UE_HAS_SOURCE_CONTROL_BRANCH_STATE 1
#else
#define BLUEPRINTHELPER_UE_HAS_SOURCE_CONTROL_BATCH_QUERY 0
#define BLUEPRINTHELPER_UE_HAS_SOURCE_CONTROL_BRANCH_STATE 0
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5 && ENGINE_MINOR_VERSION <= 8
#define BLUEPRINTHELPER_UE_HAS_MATERIAL_EXPRESSION_COUNT_INPUTS 1
#else
#define BLUEPRINTHELPER_UE_HAS_MATERIAL_EXPRESSION_COUNT_INPUTS 0
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7 && ENGINE_MINOR_VERSION <= 8
#define BLUEPRINTHELPER_UE_HAS_SHADER_PLATFORM_MATERIAL_RESOURCE 1
#else
#define BLUEPRINTHELPER_UE_HAS_SHADER_PLATFORM_MATERIAL_RESOURCE 0
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6 && ENGINE_MINOR_VERSION <= 8
#define BLUEPRINTHELPER_UE_HAS_MATERIAL_VALUE_TYPE_ACCESSORS 1
#else
#define BLUEPRINTHELPER_UE_HAS_MATERIAL_VALUE_TYPE_ACCESSORS 0
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 3
#define BLUEPRINTHELPER_UE_HAS_MATERIAL_STRATA_VALUE_TYPE 1
#else
#define BLUEPRINTHELPER_UE_HAS_MATERIAL_STRATA_VALUE_TYPE 0
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4 && ENGINE_MINOR_VERSION <= 8
#define BLUEPRINTHELPER_UE_HAS_MATERIAL_SUBSTRATE_VALUE_TYPE 1
#else
#define BLUEPRINTHELPER_UE_HAS_MATERIAL_SUBSTRATE_VALUE_TYPE 0
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5 && ENGINE_MINOR_VERSION <= 8
#define BLUEPRINTHELPER_UE_HAS_MATERIAL_TEXTURE_COLLECTION_VALUE_TYPES 1
#else
#define BLUEPRINTHELPER_UE_HAS_MATERIAL_TEXTURE_COLLECTION_VALUE_TYPES 0
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6 && ENGINE_MINOR_VERSION <= 8
#define BLUEPRINTHELPER_UE_HAS_MATERIAL_TEXTURE_MATERIAL_CACHE_VALUE_TYPE 1
#else
#define BLUEPRINTHELPER_UE_HAS_MATERIAL_TEXTURE_MATERIAL_CACHE_VALUE_TYPE 0
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
	static FORCEINLINE decltype(auto) PopNoShrink(ArrayType& Array)
	{
#if BLUEPRINTHELPER_UE_HAS_EALLOW_SHRINKING
		return Array.Pop(EAllowShrinking::No);
#else
		return Array.Pop(false);
#endif
	}

	template<typename JsonKeyType>
	static FORCEINLINE FString JsonKeyToString(const JsonKeyType& Key)
	{
		return FString(*Key);
	}

	static FORCEINLINE TSharedPtr<FJsonValue> FindJsonValue(
		const FJsonObject* Object,
		const FString& FieldName)
	{
		if (!Object)
		{
			return TSharedPtr<FJsonValue>();
		}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4 && ENGINE_MINOR_VERSION <= 7
		return Object->TryGetField(FStringView(FieldName));
#else
		return Object->TryGetField(FieldName);
#endif
	}

	static FORCEINLINE TSharedPtr<FJsonValue> FindJsonValue(
		const TSharedPtr<FJsonObject>& Object,
		const FString& FieldName)
	{
		return FindJsonValue(Object.Get(), FieldName);
	}

	static FORCEINLINE TSharedPtr<FJsonValue> FindJsonValue(
		const TSharedRef<FJsonObject>& Object,
		const FString& FieldName)
	{
		return FindJsonValue(&Object.Get(), FieldName);
	}

	static FORCEINLINE void GetJsonObjectKeys(const TSharedPtr<FJsonObject>& Object, TArray<FString>& OutKeys)
	{
		OutKeys.Reset();
		if (!Object.IsValid())
		{
			return;
		}

		for (auto It = Object->Values.CreateConstIterator(); It; ++It)
		{
			OutKeys.Add(JsonKeyToString(It.Key()));
		}
	}

	static FORCEINLINE void LeftInlineNoShrink(FString& String, const int32 Count)
	{
#if BLUEPRINTHELPER_UE_HAS_EALLOW_SHRINKING
		String.LeftInline(Count, EAllowShrinking::No);
#else
		String.LeftInline(Count, false);
#endif
	}

	static FORCEINLINE void RightChopInlineNoShrink(FString& String, const int32 Count)
	{
#if BLUEPRINTHELPER_UE_HAS_EALLOW_SHRINKING
		String.RightChopInline(Count, EAllowShrinking::No);
#else
		String.RightChopInline(Count, false);
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

	template<typename PinType>
	static FORCEINLINE void MakePinLinkTo(PinType* FromPin, PinType* ToPin, const bool bAlwaysMarkDirty)
	{
#if BLUEPRINTHELPER_UE_HAS_PIN_LINK_DIRTY_FLAG
		FromPin->MakeLinkTo(ToPin, bAlwaysMarkDirty);
#else
		FromPin->MakeLinkTo(ToPin);
#endif
	}

	template<typename PinType>
	static FORCEINLINE void BreakPinLinkTo(PinType* FromPin, PinType* ToPin, const bool bAlwaysMarkDirty)
	{
#if BLUEPRINTHELPER_UE_HAS_PIN_LINK_DIRTY_FLAG
		FromPin->BreakLinkTo(ToPin, bAlwaysMarkDirty);
#else
		FromPin->BreakLinkTo(ToPin);
#endif
	}

	static FORCEINLINE int32 CountMaterialExpressionInputs(const UMaterialExpression* Expression)
	{
		if (!Expression)
		{
			return 0;
		}
#if BLUEPRINTHELPER_UE_HAS_MATERIAL_EXPRESSION_COUNT_INPUTS
		return Expression->CountInputs();
#else
		return const_cast<UMaterialExpression*>(Expression)->GetInputsView().Num();
#endif
	}

	static FORCEINLINE FMaterialResource* GetMaterialResource(UMaterial* Material)
	{
		if (!Material)
		{
			return nullptr;
		}
#if BLUEPRINTHELPER_UE_HAS_SHADER_PLATFORM_MATERIAL_RESOURCE
		return Material->GetMaterialResource(GMaxRHIShaderPlatform);
#else
		return Material->GetMaterialResource(GMaxRHIFeatureLevel);
#endif
	}

	static FORCEINLINE uint32 GetMaterialExpressionInputValueType(UMaterialExpression* Expression, const int32 InputIndex)
	{
		if (!Expression)
		{
			return 0;
		}
#if BLUEPRINTHELPER_UE_HAS_MATERIAL_VALUE_TYPE_ACCESSORS
		return static_cast<uint32>(Expression->GetInputValueType(InputIndex));
#else
		return Expression->GetInputType(InputIndex);
#endif
	}

	static FORCEINLINE uint32 GetMaterialExpressionOutputValueType(UMaterialExpression* Expression, const int32 OutputIndex)
	{
		if (!Expression)
		{
			return 0;
		}
#if BLUEPRINTHELPER_UE_HAS_MATERIAL_VALUE_TYPE_ACCESSORS
		return static_cast<uint32>(Expression->GetOutputValueType(OutputIndex));
#else
		return Expression->GetOutputType(OutputIndex);
#endif
	}
};
