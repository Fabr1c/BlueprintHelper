// BlueprintHelper Service Layer - generic UObject property reflection service.

#include "Services/DataAssetObjectProperty/BlueprintHelperPropertyReflectionService.h"

#include "Dom/JsonValue.h"
#include "GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Structure/BlueprintHelperServiceTypes.h"
#include "UObject/UnrealType.h"

namespace
{
	FBlueprintHelperToolError MakeObjectPropertyToolError(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field = TEXT(""))
	{
		FBlueprintHelperToolError Error;
		Error.Code = Code;
		Error.Stage = Stage;
		Error.Message = Message;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		Error.Field = Field;
		return Error;
	}

	TSharedRef<FJsonObject> MakeObjectPropertyTarget(
		const FString& AssetPath,
		const FString& PropertyPath)
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Target->SetStringField(TEXT("target_type"), TEXT("property"));
		if (!PropertyPath.IsEmpty())
		{
			Target->SetStringField(TEXT("property_path"), PropertyPath);
		}
		return Target;
	}

	FBlueprintHelperToolResultBase MakeObjectPropertyToolResult(
		const FString& Operation,
		const FString& AssetPath,
		const FString& PropertyPath,
		const FBlueprintHelperSetPropertiesResult& MutationResult)
	{
		const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
		FBlueprintHelperToolResultBase Result;
		if (MutationResult.bSuccess)
		{
			if (MutationResult.bDryRun)
			{
				Result = FBlueprintHelperToolResultBuilder::DryRun(Operation, TraceId);
			}
			else if (MutationResult.PropertyResult.ChangedCount > 0)
			{
				Result = FBlueprintHelperToolResultBuilder::Applied(Operation, TraceId);
			}
			else
			{
				Result = FBlueprintHelperToolResultBuilder::NoOp(Operation, TraceId);
			}
		}
		else
		{
			Result = FBlueprintHelperToolResultBuilder::Failure(
				Operation,
				TraceId,
				MakeObjectPropertyToolError(
					TEXT("object_property_operation_failed"),
					EBlueprintHelperToolStage::Execute,
					MutationResult.ErrorMessage));
		}

		Result.CustomTargetJson = MakeObjectPropertyTarget(AssetPath, PropertyPath);
		Result.bModified = MutationResult.bSuccess && !MutationResult.bDryRun &&
			MutationResult.PropertyResult.ChangedCount > 0;

		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("ObjectPropertyMutation.v1"));
		Data->SetBoolField(TEXT("dry_run"), MutationResult.bDryRun);
		Data->SetObjectField(TEXT("property_result"), MutationResult.PropertyResult.ToJson());
		Result.Data = Data;
		return Result;
	}
}

UObject* FBlueprintHelperPropertyReflectionService::ResolveAsset(
	const FString& AssetPath,
	FString& OutError) const
{
	if (AssetPath.IsEmpty())
	{
		OutError = TEXT("asset_path cannot be empty.");
		return nullptr;
	}

	UObject* Obj = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
	if (!Obj)
	{
		OutError = FString::Printf(TEXT("Cannot load asset/object: %s"), *AssetPath);
		return nullptr;
	}
	return Obj;
}

FString FBlueprintHelperPropertyReflectionService::BuildFlagsSummary(uint64 PropertyFlags)
{
	return FBlueprintHelperEditablePropertyPolicy::BuildFlagsSummary(PropertyFlags);
}

bool FBlueprintHelperPropertyReflectionService::ResolvePropertyPath(
	UObject* RootObject,
	const FString& PropertyPath,
	FProperty*& OutProperty,
	void*& OutValuePtr,
	FString& OutExpectedType,
	FString& OutErrorCode,
	FString& OutErrorMessage)
{
	OutProperty = nullptr;
	OutValuePtr = nullptr;

	if (!RootObject)
	{
		OutErrorCode = TEXT("asset_not_found");
		OutErrorMessage = TEXT("target object is null.");
		return false;
	}

	TArray<FString> Segments;
	PropertyPath.ParseIntoArray(Segments, TEXT("."), true);
	if (Segments.Num() == 0)
	{
		OutErrorCode = TEXT("property_not_found");
		OutErrorMessage = TEXT("property_path cannot be empty.");
		return false;
	}

	UStruct* CurrentStruct = RootObject->GetClass();
	void* CurrentContainer = RootObject;

	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		const bool bLast = Index == Segments.Num() - 1;
		FProperty* Property = CurrentStruct
			? CurrentStruct->FindPropertyByName(FName(*Segments[Index]))
			: nullptr;
		if (!Property)
		{
			OutErrorCode = TEXT("property_not_found");
			OutErrorMessage = FString::Printf(TEXT("Property path segment not found: %s"), *Segments[Index]);
			return false;
		}

		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(CurrentContainer);
		if (bLast)
		{
			OutProperty = Property;
			OutValuePtr = ValuePtr;
			OutExpectedType = Property->GetCPPType();
			return true;
		}

		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			CurrentStruct = StructProperty->Struct;
			CurrentContainer = ValuePtr;
			continue;
		}

		if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			UObject* NestedObject = ObjectProperty->GetObjectPropertyValue(ValuePtr);
			if (!NestedObject)
			{
				OutErrorCode = TEXT("object_reference_not_found");
				OutErrorMessage = FString::Printf(TEXT("Object reference path segment is null: %s"), *Segments[Index]);
				return false;
			}

			CurrentStruct = NestedObject->GetClass();
			CurrentContainer = NestedObject;
			continue;
		}

		OutErrorCode = TEXT("struct_field_invalid");
		OutErrorMessage = FString::Printf(
			TEXT("Property path segment is not a struct/object and cannot be traversed: %s"),
			*Segments[Index]);
		return false;
	}

	return false;
}

bool FBlueprintHelperPropertyReflectionService::JsonValueToImportText(
	const TSharedPtr<FJsonValue>& Value,
	FString& OutText,
	FString& OutSummary,
	FString& OutActualType,
	FString& OutError)
{
	OutText.Empty();
	OutSummary.Empty();
	OutActualType.Empty();
	OutError.Empty();

	if (!Value.IsValid() || Value->Type == EJson::Null || Value->Type == EJson::None)
	{
		OutActualType = TEXT("null");
		OutError = TEXT("value cannot be null.");
		return false;
	}

	switch (Value->Type)
	{
	case EJson::String:
		OutText = Value->AsString();
		OutSummary = OutText.Left(128);
		OutActualType = TEXT("string");
		return true;
	case EJson::Boolean:
		OutText = Value->AsBool() ? TEXT("true") : TEXT("false");
		OutSummary = OutText;
		OutActualType = TEXT("bool");
		return true;
	case EJson::Number:
		OutText = FString::SanitizeFloat(Value->AsNumber());
		OutSummary = OutText;
		OutActualType = TEXT("number");
		return true;
	default:
		OutActualType = TEXT("object_or_array");
		OutError = TEXT("object_property value currently supports string, bool, and number; use leaf property paths for struct values.");
		return false;
	}
}

FBlueprintHelperObjectPropertiesResult FBlueprintHelperPropertyReflectionService::GetObjectProperties(
	const FString& AssetPath) const
{
	FBlueprintHelperObjectPropertiesResult Result;

	UObject* Obj = ResolveAsset(AssetPath, Result.ErrorMessage);
	if (!Obj)
	{
		return Result;
	}

	Result.ClassName = Obj->GetClass()->GetName();
	Result.AssetPath = AssetPath;

	for (TFieldIterator<FProperty> It(Obj->GetClass()); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop || !(Prop->PropertyFlags & (CPF_Edit | CPF_BlueprintVisible)))
		{
			continue;
		}

		FBlueprintHelperObjectPropertyInfo Info;
		Info.Name = Prop->GetName();
		Info.TypeName = Prop->GetCPPType();

		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Obj);
		Prop->ExportTextItem_Direct(Info.Value, ValuePtr, nullptr, Obj, PPF_None);

		if (Prop->HasMetaData(TEXT("Category")))
		{
			Info.Category = Prop->GetMetaData(TEXT("Category"));
		}

		Info.Flags = BuildFlagsSummary(Prop->PropertyFlags);
		Result.Properties.Add(MoveTemp(Info));
	}

	Result.bSuccess = true;
	return Result;
}

FBlueprintHelperSetPropertyResult FBlueprintHelperPropertyReflectionService::SetObjectProperty(
	const FString& AssetPath,
	const FString& PropertyName,
	const FString& Value,
	bool bDryRun) const
{
	FBlueprintHelperSetPropertyResult Result;
	Result.PropertyName = PropertyName;
	Result.PropertyPath = PropertyName;
	Result.bDryRun = bDryRun;

	FString LoadError;
	UObject* Obj = ResolveAsset(AssetPath, LoadError);
	if (!Obj)
	{
		Result.ErrorMessage = LoadError;
		return Result;
	}

	FProperty* Prop = nullptr;
	void* ValuePtr = nullptr;
	FString ExpectedType;
	FString ErrorCode;
	FString ErrorMessage;
	if (!ResolvePropertyPath(Obj, PropertyName, Prop, ValuePtr, ExpectedType, ErrorCode, ErrorMessage))
	{
		Result.ErrorMessage = ErrorMessage;
		return Result;
	}

	if (!FBlueprintHelperEditablePropertyPolicy::AllowsWrite(Prop))
	{
		Result.ErrorMessage = FString::Printf(
			TEXT("Property %s is not safely writable in the editor. Flags: %s"),
			*PropertyName,
			*BuildFlagsSummary(Prop->PropertyFlags));
		return Result;
	}

	Prop->ExportTextItem_Direct(Result.OldValue, ValuePtr, nullptr, Obj, PPF_None);

	void* TempValue = FMemory_Alloca(Prop->GetSize());
	Prop->InitializeValue(TempValue);
	const TCHAR* PreflightImportResult = Prop->ImportText_Direct(*Value, TempValue, Obj, PPF_None);
	if (!PreflightImportResult)
	{
		Prop->DestroyValue(TempValue);
		Result.ErrorMessage = FString::Printf(
			TEXT("Cannot import object property %s value \"%s\"."),
			*PropertyName,
			*Value);
		return Result;
	}
	Prop->ExportTextItem_Direct(Result.NewValue, TempValue, nullptr, Obj, PPF_None);
	Prop->DestroyValue(TempValue);

	if (bDryRun)
	{
		Result.bSuccess = true;
		return Result;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Set Object Property")), Obj);

	const TCHAR* ImportResult = Prop->ImportText_Direct(*Value, ValuePtr, Obj, PPF_None);
	if (!ImportResult)
	{
		Prop->ImportText_Direct(*Result.OldValue, ValuePtr, Obj, PPF_None);
		Mutation.Rollback();
		Result.ErrorMessage = FString::Printf(
			TEXT("Cannot import object property %s value \"%s\"."),
			*PropertyName,
			*Value);
		return Result;
	}

	Obj->PostEditChange();
	Mutation.Commit();

	Prop->ExportTextItem_Direct(Result.NewValue, ValuePtr, nullptr, Obj, PPF_None);

	Result.bSuccess = true;
	return Result;
}

FBlueprintHelperSetPropertiesResult FBlueprintHelperPropertyReflectionService::SetObjectProperties(
	const FString& AssetPath,
	const TArray<FBlueprintHelperObjectPropertyTextSetting>& Settings,
	bool bDryRun) const
{
	FBlueprintHelperSetPropertiesResult Result;
	Result.AssetPath = AssetPath;
	Result.bDryRun = bDryRun;
	Result.PropertyResult.Mode = Settings.Num() == 1 ? TEXT("single") : TEXT("batch");
	Result.PropertyResult.RequestedCount = Settings.Num();

	if (Settings.Num() == 0)
	{
		Result.ErrorMessage = TEXT("set_object_properties requires at least one setting.");
		return Result;
	}

	FString LoadError;
	UObject* Obj = ResolveAsset(AssetPath, LoadError);
	if (!Obj)
	{
		Result.ErrorMessage = LoadError;
		return Result;
	}

	struct FResolvedObjectPropertySetting
	{
		FBlueprintHelperObjectPropertyTextSetting Setting;
		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		FString OldValue;
		FString NewValue;
	};

	TArray<FResolvedObjectPropertySetting> ResolvedSettings;
	for (const FBlueprintHelperObjectPropertyTextSetting& Setting : Settings)
	{
		FBlueprintHelperInvalidObjectPropertySetting Invalid;
		Invalid.PropertyPath = Setting.PropertyPath;

		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		FString ExpectedType;
		FString ResolveErrorCode;
		FString ResolveErrorMessage;
		if (!ResolvePropertyPath(Obj, Setting.PropertyPath, Property, ValuePtr, ExpectedType, ResolveErrorCode, ResolveErrorMessage))
		{
			Invalid.Code = ResolveErrorCode;
			Invalid.ExpectedType = ExpectedType;
			Invalid.ValueSummary = ResolveErrorMessage.Left(128);
			Result.PropertyResult.InvalidSettings.Add(MoveTemp(Invalid));
			continue;
		}

		Invalid.ExpectedType = ExpectedType;
		if (!FBlueprintHelperEditablePropertyPolicy::AllowsWrite(Property))
		{
			Invalid.Code = TEXT("property_not_writable");
			Invalid.ValueSummary = BuildFlagsSummary(Property->PropertyFlags);
			Result.PropertyResult.InvalidSettings.Add(MoveTemp(Invalid));
			continue;
		}

		FString NewValue;
		void* TempValue = FMemory_Alloca(Property->GetSize());
		Property->InitializeValue(TempValue);
		const TCHAR* ImportResult = Property->ImportText_Direct(*Setting.Value, TempValue, Obj, PPF_None);
		if (!ImportResult)
		{
			Property->DestroyValue(TempValue);
			Invalid.Code = TEXT("type_mismatch");
			Invalid.ValueSummary = Setting.Value.Left(128);
			Result.PropertyResult.InvalidSettings.Add(MoveTemp(Invalid));
			continue;
		}

		Property->ExportTextItem_Direct(NewValue, TempValue, nullptr, Obj, PPF_None);
		Property->DestroyValue(TempValue);

		FResolvedObjectPropertySetting Resolved;
		Resolved.Setting = Setting;
		Resolved.Property = Property;
		Resolved.ValuePtr = ValuePtr;
		Resolved.NewValue = MoveTemp(NewValue);
		Property->ExportTextItem_Direct(Resolved.OldValue, ValuePtr, nullptr, Obj, PPF_None);
		ResolvedSettings.Add(MoveTemp(Resolved));
	}

	if (Result.PropertyResult.InvalidSettings.Num() > 0)
	{
		Result.ErrorMessage = TEXT("One or more object property settings are invalid.");
		return Result;
	}

	if (bDryRun)
	{
		Result.bSuccess = true;
		for (const FResolvedObjectPropertySetting& Resolved : ResolvedSettings)
		{
			if (Resolved.OldValue == Resolved.NewValue)
			{
				++Result.PropertyResult.NoOpCount;
			}
			else
			{
				++Result.PropertyResult.ChangedCount;
			}
		}
		return Result;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Set Object Properties")), Obj);

	for (const FResolvedObjectPropertySetting& Resolved : ResolvedSettings)
	{
		const TCHAR* ImportResult = Resolved.Property->ImportText_Direct(
			*Resolved.Setting.Value,
			Resolved.ValuePtr,
			Obj,
			PPF_None);
		if (!ImportResult)
		{
			Mutation.Rollback();
			Result.ErrorMessage = FString::Printf(
				TEXT("Object property write failed: %s"),
				*Resolved.Setting.PropertyPath);
			Result.PropertyResult.AppliedCount = 0;
			Result.PropertyResult.ChangedCount = 0;
			Result.PropertyResult.NoOpCount = 0;
			return Result;
		}

		FString AfterValue;
		Resolved.Property->ExportTextItem_Direct(AfterValue, Resolved.ValuePtr, nullptr, Obj, PPF_None);
		++Result.PropertyResult.AppliedCount;
		if (Resolved.OldValue == AfterValue)
		{
			++Result.PropertyResult.NoOpCount;
		}
		else
		{
			++Result.PropertyResult.ChangedCount;
		}
	}

	if (Result.PropertyResult.ChangedCount == 0)
	{
		Mutation.Rollback();
		Result.bSuccess = true;
		return Result;
	}

	Obj->PostEditChange();
	Mutation.Commit();

	Result.bSuccess = true;
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperPropertyReflectionService::SetObjectProperty(
	const FBlueprintHelperSetObjectPropertiesRequest& Request) const
{
	if (Request.Settings.Num() != 1)
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("set_object_property"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			MakeObjectPropertyToolError(
				TEXT("invalid_object_property_settings"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("set_object_property requires exactly one setting."),
				TEXT("settings")));
	}

	FBlueprintHelperToolResultBase Result = SetObjectProperties(Request);
	Result.Operation = TEXT("set_object_property");
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperPropertyReflectionService::SetObjectProperties(
	const FBlueprintHelperSetObjectPropertiesRequest& Request) const
{
	TArray<FBlueprintHelperObjectPropertyTextSetting> TextSettings;
	FBlueprintHelperSetPropertiesResult ConversionFailure;
	ConversionFailure.AssetPath = Request.AssetPath;
	ConversionFailure.bDryRun = Request.bDryRun;
	ConversionFailure.PropertyResult.Mode = Request.Settings.Num() == 1 ? TEXT("single") : TEXT("batch");
	ConversionFailure.PropertyResult.RequestedCount = Request.Settings.Num();

	for (const FBlueprintHelperObjectPropertySetting& Setting : Request.Settings)
	{
		FString ImportText;
		FString Summary;
		FString ActualType;
		FString ConvertError;
		if (!JsonValueToImportText(Setting.Value, ImportText, Summary, ActualType, ConvertError))
		{
			FBlueprintHelperInvalidObjectPropertySetting Invalid;
			Invalid.PropertyPath = Setting.PropertyPath;
			Invalid.Code = TEXT("type_mismatch");
			Invalid.ActualType = ActualType;
			Invalid.ValueSummary = ConvertError.Left(128);
			ConversionFailure.PropertyResult.InvalidSettings.Add(MoveTemp(Invalid));
			continue;
		}

		FBlueprintHelperObjectPropertyTextSetting TextSetting;
		TextSetting.PropertyPath = Setting.PropertyPath;
		TextSetting.Value = MoveTemp(ImportText);
		TextSettings.Add(MoveTemp(TextSetting));
	}

	if (ConversionFailure.PropertyResult.InvalidSettings.Num() > 0)
	{
		ConversionFailure.ErrorMessage = TEXT("One or more object property values are invalid.");
		return MakeObjectPropertyToolResult(
			TEXT("set_object_properties"),
			Request.AssetPath,
			Request.Settings.Num() == 1 ? Request.Settings[0].PropertyPath : FString(),
			ConversionFailure);
	}

	FBlueprintHelperSetPropertiesResult MutationResult = SetObjectProperties(
		Request.AssetPath,
		TextSettings,
		Request.bDryRun);
	return MakeObjectPropertyToolResult(
		TEXT("set_object_properties"),
		Request.AssetPath,
		Request.Settings.Num() == 1 ? Request.Settings[0].PropertyPath : FString(),
		MutationResult);
}
