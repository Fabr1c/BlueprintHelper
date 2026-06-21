#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassDefaultSetterMutationService.h"

#include "Dom/JsonValue.h"
#include "UObject/Class.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

bool FBlueprintHelperClassDefaultSetterMutationService::ConvertJsonValueToImportText(
	const TSharedPtr<FJsonValue>& Value,
	FString& OutText,
	FString& OutActualType,
	FString& OutError)
{
	if (!Value.IsValid() || Value->Type == EJson::Null)
	{
		OutActualType = TEXT("null");
		OutError = TEXT("setter value cannot be null.");
		return false;
	}

	switch (Value->Type)
	{
	case EJson::String:
		OutText = Value->AsString();
		OutActualType = TEXT("string");
		return true;
	case EJson::Boolean:
		OutText = Value->AsBool() ? TEXT("true") : TEXT("false");
		OutActualType = TEXT("bool");
		return true;
	case EJson::Number:
		OutText = LexToString(Value->AsNumber());
		OutActualType = TEXT("number");
		return true;
	default:
		OutActualType = TEXT("object_or_array");
		OutError = TEXT("setter-aware class default values support string, bool, and number in this version.");
		return false;
	}
}

FProperty* FBlueprintHelperClassDefaultSetterMutationService::FindSetterInput(UFunction* Function)
{
	FProperty* InputProperty = nullptr;
	int32 InputCount = 0;
	if (!Function)
	{
		return nullptr;
	}
	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		FProperty* Param = *It;
		if (!Param->HasAnyPropertyFlags(CPF_Parm) || Param->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			continue;
		}
		if (Param->HasAnyPropertyFlags(CPF_OutParm | CPF_ReferenceParm))
		{
			return nullptr;
		}
		++InputCount;
		InputProperty = Param;
	}
	return InputCount == 1 ? InputProperty : nullptr;
}

FProperty* FBlueprintHelperClassDefaultSetterMutationService::FindGetterReturn(UFunction* Function)
{
	FProperty* ReturnProperty = nullptr;
	int32 InputCount = 0;
	int32 ReturnCount = 0;
	if (!Function)
	{
		return nullptr;
	}
	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		FProperty* Param = *It;
		if (!Param->HasAnyPropertyFlags(CPF_Parm))
		{
			continue;
		}
		if (Param->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			++ReturnCount;
			ReturnProperty = Param;
			continue;
		}
		++InputCount;
	}
	return InputCount == 0 && ReturnCount == 1 ? ReturnProperty : nullptr;
}

FString FBlueprintHelperClassDefaultSetterMutationService::SerializePropertyValue(
	const FProperty* Property,
	const void* ValuePtr,
	const UObject* OwnerObject)
{
	if (!Property || !ValuePtr)
	{
		return FString();
	}

	if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
	{
		const UObject* ObjectValue = ObjectProperty->GetObjectPropertyValue(ValuePtr);
		return ObjectValue ? ObjectValue->GetPathName() : FString(TEXT("None"));
	}

	FString Exported;
	Property->ExportTextItem_Direct(Exported, ValuePtr, nullptr, const_cast<UObject*>(OwnerObject), PPF_None);
	return Exported;
}

bool FBlueprintHelperClassDefaultSetterMutationService::BuildSetterParamBuffer(
	UFunction* SetterFunction,
	const TSharedPtr<FJsonValue>& Value,
	TArray<uint8>& OutBuffer,
	FString& OutExpectedValue,
	FString& OutErrorCode,
	FString& OutErrorMessage)
{
	FProperty* SetterInput = FindSetterInput(SetterFunction);
	if (!SetterFunction || !SetterInput)
	{
		OutErrorCode = TEXT("class_default_setter_signature_unsupported");
		OutErrorMessage = TEXT("Setter function does not have exactly one supported input parameter.");
		return false;
	}

	FString ImportText;
	FString ActualType;
	FString ConvertError;
	if (!ConvertJsonValueToImportText(Value, ImportText, ActualType, ConvertError))
	{
		OutErrorCode = TEXT("class_default_setter_value_type_mismatch");
		OutErrorMessage = ConvertError;
		return false;
	}

	OutBuffer.SetNumZeroed(SetterFunction->ParmsSize);
	SetterFunction->InitializeStruct(OutBuffer.GetData());
	void* ParamValuePtr = SetterInput->ContainerPtrToValuePtr<void>(OutBuffer.GetData());
	const TCHAR* ImportEnd = SetterInput->ImportText_Direct(*ImportText, ParamValuePtr, nullptr, PPF_None);
	if (!ImportEnd)
	{
		SetterFunction->DestroyStruct(OutBuffer.GetData());
		OutBuffer.Reset();
		OutErrorCode = TEXT("class_default_setter_value_type_mismatch");
		OutErrorMessage = FString::Printf(TEXT("Value cannot be imported into setter parameter type %s."), *SetterInput->GetCPPType());
		return false;
	}

	OutExpectedValue = SerializePropertyValue(SetterInput, ParamValuePtr, nullptr);
	return true;
}

bool FBlueprintHelperClassDefaultSetterMutationService::InvokeGetter(
	UObject* OwnerObject,
	UFunction* GetterFunction,
	FString& OutValue,
	FString& OutErrorCode,
	FString& OutErrorMessage)
{
	FProperty* GetterReturn = FindGetterReturn(GetterFunction);
	if (!OwnerObject || !GetterFunction || !GetterReturn)
	{
		OutErrorCode = TEXT("class_default_setter_getter_required");
		OutErrorMessage = TEXT("Getter function is missing or has an unsupported signature.");
		return false;
	}

	TArray<uint8> Buffer;
	Buffer.SetNumZeroed(GetterFunction->ParmsSize);
	GetterFunction->InitializeStruct(Buffer.GetData());
	OwnerObject->ProcessEvent(GetterFunction, Buffer.GetData());
	const void* ReturnValuePtr = GetterReturn->ContainerPtrToValuePtr<void>(Buffer.GetData());
	OutValue = SerializePropertyValue(GetterReturn, ReturnValuePtr, OwnerObject);
	GetterFunction->DestroyStruct(Buffer.GetData());
	return true;
}

FBlueprintHelperClassDefaultSetterMutationResult FBlueprintHelperClassDefaultSetterMutationService::Apply(
	const FBlueprintHelperClassDefaultSetterMutationRequest& Request) const
{
	FBlueprintHelperClassDefaultSetterMutationResult Result;
	Result.Evidence.AssetPath = Request.AssetPath;
	Result.Evidence.OwnerRoot = Request.Target.OwnerRoot;
	Result.Evidence.OwnerObjectPath = Request.Target.OwnerObjectPath;
	Result.Evidence.OwnerObjectClass = Request.Target.OwnerObjectClass;
	Result.Evidence.PropertyPath = Request.Target.PropertyPath;
	Result.Evidence.LeafPropertyName = Request.Target.LeafPropertyName;
	Result.Evidence.SetterFunction = Request.Target.SetterFunctionName;
	Result.Evidence.GetterFunction = Request.Target.GetterFunctionName;
	Result.Evidence.ExpectedType = Request.Target.ExpectedType;
	Result.Evidence.PropertyFlags = Request.Target.PropertyFlags;

	if (!Request.Target.OwnerObject || !Request.Target.SetterFunction || !Request.Target.GetterFunction)
	{
		Result.ErrorCode = TEXT("class_default_setter_not_found");
		Result.ErrorMessage = TEXT("Setter-aware mutation target is incomplete.");
		return Result;
	}

	FString BeforeValue;
	if (!InvokeGetter(
		Request.Target.OwnerObject,
		Request.Target.GetterFunction,
		BeforeValue,
		Result.ErrorCode,
		Result.ErrorMessage))
	{
		return Result;
	}
	Result.Evidence.BeforeValue = BeforeValue;

	TArray<uint8> SetterParams;
	FString ExpectedValue;
	if (!BuildSetterParamBuffer(
		Request.Target.SetterFunction,
		Request.Value,
		SetterParams,
		ExpectedValue,
		Result.ErrorCode,
		Result.ErrorMessage))
	{
		return Result;
	}
	Result.Evidence.InputValue = ExpectedValue;
	Result.bWouldChange = BeforeValue != ExpectedValue;

	if (Request.bDryRun)
	{
		Request.Target.SetterFunction->DestroyStruct(SetterParams.GetData());
		Result.bOk = true;
		Result.bModified = false;
		return Result;
	}

	if (Request.RootObject)
	{
		Request.RootObject->Modify();
	}
	Request.Target.OwnerObject->Modify();
	Request.Target.OwnerObject->ProcessEvent(Request.Target.SetterFunction, SetterParams.GetData());
	Request.Target.SetterFunction->DestroyStruct(SetterParams.GetData());

	FString AfterValue;
	if (!InvokeGetter(
		Request.Target.OwnerObject,
		Request.Target.GetterFunction,
		AfterValue,
		Result.ErrorCode,
		Result.ErrorMessage))
	{
		return Result;
	}

	Result.Evidence.AfterValue = AfterValue;
	if (AfterValue != ExpectedValue)
	{
		Result.ErrorCode = TEXT("class_default_setter_readback_mismatch");
		Result.ErrorMessage = FString::Printf(
			TEXT("Setter readback mismatch: expected %s but got %s."),
			*ExpectedValue,
			*AfterValue);
		return Result;
	}

	Result.bOk = true;
	Result.bModified = BeforeValue != AfterValue;
	return Result;
}
