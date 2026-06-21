#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassDefaultPropertyMutationResolver.h"

#include "Shared/BlueprintHelperServiceTypes.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

FString FBlueprintHelperClassDefaultPropertyMutationResolver::ResolveMetadataName(
	const FProperty* Property,
	const TCHAR* PrimaryKey,
	const TCHAR* SecondaryKey)
{
	if (!Property)
	{
		return FString();
	}

	const FString PrimaryValue = Property->GetMetaData(PrimaryKey);
	if (!PrimaryValue.IsEmpty())
	{
		return PrimaryValue;
	}
	return Property->GetMetaData(SecondaryKey);
}

FString FBlueprintHelperClassDefaultPropertyMutationResolver::BuildOwnerObjectClass(const UObject* OwnerObject)
{
	return OwnerObject && OwnerObject->GetClass()
		? OwnerObject->GetClass()->GetPathName()
		: FString();
}

bool FBlueprintHelperClassDefaultPropertyMutationResolver::Resolve(
	UObject* RootObject,
	const FString& PropertyPath,
	FBlueprintHelperClassDefaultResolvedMutationTarget& OutTarget,
	FString& OutErrorCode,
	FString& OutErrorMessage) const
{
	OutTarget = FBlueprintHelperClassDefaultResolvedMutationTarget();
	OutErrorCode.Reset();
	OutErrorMessage.Reset();

	if (!RootObject)
	{
		OutErrorCode = TEXT("class_default_property_not_found");
		OutErrorMessage = TEXT("Root class default object is null.");
		return false;
	}

	TArray<FString> Segments;
	PropertyPath.ParseIntoArray(Segments, TEXT("."), true);
	if (Segments.Num() == 0)
	{
		OutErrorCode = TEXT("class_default_property_not_found");
		OutErrorMessage = TEXT("property_path is required.");
		return false;
	}

	UObject* CurrentObject = RootObject;
	UStruct* CurrentStruct = RootObject->GetClass();
	void* CurrentContainer = RootObject;
	TArray<FString> OwnerSegments;

	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		const bool bLast = Index == Segments.Num() - 1;
		FProperty* Property = CurrentStruct
			? CurrentStruct->FindPropertyByName(FName(*Segments[Index]))
			: nullptr;
		if (!Property)
		{
			OutErrorCode = TEXT("class_default_property_not_found");
			OutErrorMessage = FString::Printf(TEXT("Class default property segment was not found: %s"), *Segments[Index]);
			return false;
		}

		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(CurrentContainer);
		if (bLast)
		{
			OutTarget.RootObject = RootObject;
			OutTarget.OwnerObject = CurrentObject ? CurrentObject : RootObject;
			OutTarget.LeafProperty = Property;
			OutTarget.LeafValuePtr = ValuePtr;
			OutTarget.OwnerObjectPath = FString::Join(OwnerSegments, TEXT("."));
			OutTarget.OwnerObjectClass = BuildOwnerObjectClass(OutTarget.OwnerObject);
			OutTarget.PropertyPath = PropertyPath;
			OutTarget.LeafPropertyName = Property->GetName();
			OutTarget.ExpectedType = Property->GetCPPType();
			OutTarget.PropertyFlags = FBlueprintHelperEditablePropertyPolicy::BuildFlagsSummary(Property->PropertyFlags);
			OutTarget.SetterFunctionName = ResolveMetadataName(Property, TEXT("Setter"), TEXT("BlueprintSetter"));
			OutTarget.GetterFunctionName = ResolveMetadataName(Property, TEXT("Getter"), TEXT("BlueprintGetter"));
			if (OutTarget.OwnerObject && OutTarget.OwnerObject->GetClass())
			{
				if (!OutTarget.SetterFunctionName.IsEmpty())
				{
					OutTarget.SetterFunction = OutTarget.OwnerObject->GetClass()->FindFunctionByName(
						FName(*OutTarget.SetterFunctionName));
				}
				if (!OutTarget.GetterFunctionName.IsEmpty())
				{
					OutTarget.GetterFunction = OutTarget.OwnerObject->GetClass()->FindFunctionByName(
						FName(*OutTarget.GetterFunctionName));
				}
			}
			return true;
		}

		if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			OwnerSegments.Add(Segments[Index]);
			CurrentStruct = StructProp->Struct;
			CurrentContainer = ValuePtr;
			continue;
		}

		if (FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Property))
		{
			UObject* NestedObject = ObjectProp->GetObjectPropertyValue(ValuePtr);
			if (!NestedObject)
			{
				OutErrorCode = TEXT("object_reference_not_found");
				OutErrorMessage = FString::Printf(TEXT("Object reference segment is null: %s"), *Segments[Index]);
				return false;
			}
			OwnerSegments.Add(Segments[Index]);
			CurrentObject = NestedObject;
			CurrentStruct = NestedObject->GetClass();
			CurrentContainer = NestedObject;
			continue;
		}

		OutErrorCode = TEXT("struct_field_invalid");
		OutErrorMessage = FString::Printf(TEXT("Property segment is not a struct or object: %s"), *Segments[Index]);
		return false;
	}

	OutErrorCode = TEXT("class_default_property_not_found");
	OutErrorMessage = TEXT("Class default property could not be resolved.");
	return false;
}
