#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassDefaultPropertyMutationPolicy.h"

#include "Shared/BlueprintHelperServiceTypes.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

class FBlueprintHelperClassDefaultPropertyMutationPolicyLocal
{
public:
	static FProperty* FindSingleSetterInput(UFunction* Function)
	{
		FProperty* InputProperty = nullptr;
		int32 InputCount = 0;
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

	static FProperty* FindSingleGetterReturn(UFunction* Function)
	{
		FProperty* ReturnProperty = nullptr;
		int32 InputCount = 0;
		int32 ReturnCount = 0;
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

	static bool ArePropertiesCompatible(const FProperty* Left, const FProperty* Right)
	{
		return Left && Right && Left->SameType(Right);
	}
};

bool FBlueprintHelperClassDefaultPropertyMutationPolicy::IsValidMutationStrategy(const FString& RequestedStrategy)
{
	return RequestedStrategy.IsEmpty() ||
		RequestedStrategy.Equals(TEXT("direct_property"), ESearchCase::IgnoreCase) ||
		RequestedStrategy.Equals(TEXT("setter_aware_property"), ESearchCase::IgnoreCase);
}

FBlueprintHelperToolSuggestedRoute FBlueprintHelperClassDefaultPropertyMutationPolicy::MakeSetterAwareSuggestedRoute(
	const FString& PropertyPath)
{
	FBlueprintHelperToolSuggestedRoute Route;
	Route.RouteId = TEXT("blueprint_class_settings.class_default_setter");
	Route.Family = TEXT("blueprint_class_settings");
	Route.ClusterId = TEXT("class_settings");
	Route.OperationId = TEXT("set_class_default_via_setter");
	Route.TemplateId = TEXT("blueprint_class_settings.class_settings.set_class_default_via_setter");
	Route.TaskType = TEXT("edit_blueprint_class_settings");
	Route.Reason = TEXT("Direct class-default write is blocked; use the reflected setter/getter mutation route.");
	Route.AppliesWhen = TEXT("direct_write.writable=false and setter_aware_write.supported=true");
	Route.PropertyPathHint = PropertyPath;
	if (!PropertyPath.IsEmpty())
	{
		Route.PropertyPathHints.Add(PropertyPath);
	}
	return Route;
}

bool FBlueprintHelperClassDefaultPropertyMutationPolicy::IsSetterGetterPairSupported(
	const FBlueprintHelperClassDefaultResolvedMutationTarget& Target,
	FString& OutCode,
	FString& OutMessage)
{
	if (Target.SetterFunctionName.IsEmpty() || !Target.SetterFunction)
	{
		OutCode = TEXT("class_default_setter_not_found");
		OutMessage = TEXT("Class default property declares no reflected setter function.");
		return false;
	}
	if (Target.GetterFunctionName.IsEmpty() || !Target.GetterFunction)
	{
		OutCode = TEXT("class_default_setter_getter_required");
		OutMessage = TEXT("Setter-aware class default mutation requires a reflected getter.");
		return false;
	}
	if (Target.SetterFunction->HasMetaData(TEXT("Latent")) || Target.SetterFunction->HasMetaData(TEXT("WorldContext")))
	{
		OutCode = TEXT("class_default_setter_signature_unsupported");
		OutMessage = TEXT("Setter-aware class default mutation does not support latent or world-context setters.");
		return false;
	}
	if (Target.GetterFunction->HasMetaData(TEXT("Latent")) || Target.GetterFunction->HasMetaData(TEXT("WorldContext")))
	{
		OutCode = TEXT("class_default_setter_signature_unsupported");
		OutMessage = TEXT("Setter-aware class default mutation does not support latent or world-context getters.");
		return false;
	}

	FProperty* SetterInput = FBlueprintHelperClassDefaultPropertyMutationPolicyLocal::FindSingleSetterInput(
		Target.SetterFunction);
	if (!SetterInput)
	{
		OutCode = TEXT("class_default_setter_signature_unsupported");
		OutMessage = TEXT("Setter-aware class default mutation requires exactly one non-ref input parameter.");
		return false;
	}

	FProperty* GetterReturn = FBlueprintHelperClassDefaultPropertyMutationPolicyLocal::FindSingleGetterReturn(
		Target.GetterFunction);
	if (!GetterReturn)
	{
		OutCode = TEXT("class_default_setter_signature_unsupported");
		OutMessage = TEXT("Setter-aware class default mutation requires a no-input getter with one return value.");
		return false;
	}

	if (!FBlueprintHelperClassDefaultPropertyMutationPolicyLocal::ArePropertiesCompatible(SetterInput, GetterReturn) ||
		!FBlueprintHelperClassDefaultPropertyMutationPolicyLocal::ArePropertiesCompatible(SetterInput, Target.LeafProperty))
	{
		OutCode = TEXT("class_default_setter_signature_unsupported");
		OutMessage = TEXT("Setter, getter, and leaf property types must match.");
		return false;
	}

	return true;
}

FBlueprintHelperClassDefaultMutationPolicyDecision FBlueprintHelperClassDefaultPropertyMutationPolicy::Decide(
	const FBlueprintHelperClassDefaultResolvedMutationTarget& Target,
	const FString& RequestedStrategy) const
{
	FBlueprintHelperClassDefaultMutationPolicyDecision Decision;
	if (!IsValidMutationStrategy(RequestedStrategy))
	{
		Decision.Code = TEXT("class_default_mutation_strategy_invalid");
		Decision.Message = TEXT("class default mutation_strategy must be direct_property or setter_aware_property.");
		Decision.SafeNextAction = TEXT("correct_mutation_strategy_then_retry");
		return Decision;
	}

	const bool bDirectRequested = RequestedStrategy.IsEmpty() ||
		RequestedStrategy.Equals(TEXT("direct_property"), ESearchCase::IgnoreCase);
	if (bDirectRequested && FBlueprintHelperEditablePropertyPolicy::AllowsClassDefaultWrite(Target.LeafProperty))
	{
		Decision.Strategy = EBlueprintHelperClassDefaultMutationStrategy::DirectProperty;
		return Decision;
	}

	FString SetterCode;
	FString SetterMessage;
	const bool bSetterSupported = IsSetterGetterPairSupported(Target, SetterCode, SetterMessage);
	if (RequestedStrategy.Equals(TEXT("setter_aware_property"), ESearchCase::IgnoreCase))
	{
		if (bSetterSupported)
		{
			Decision.Strategy = EBlueprintHelperClassDefaultMutationStrategy::SetterAwareProperty;
			return Decision;
		}
		Decision.Code = SetterCode;
		Decision.Message = SetterMessage;
		Decision.SafeNextAction = TEXT("inspect_setter_getter_signature_then_retry");
		return Decision;
	}

	if (bSetterSupported)
	{
		Decision.Code = TEXT("class_default_property_setter_required");
		Decision.Message = TEXT("Direct class-default write is blocked; this property supports setter-aware mutation.");
		Decision.SafeNextAction = TEXT("use_suggested_route_and_rerun_preview");
		Decision.SuggestedRoute = MakeSetterAwareSuggestedRoute(Target.PropertyPath);
		return Decision;
	}

	const bool bHasSetterMetadata = !Target.SetterFunctionName.IsEmpty() || !Target.GetterFunctionName.IsEmpty();
	Decision.Code = bHasSetterMetadata && !SetterCode.IsEmpty()
		? SetterCode
		: TEXT("class_default_property_not_writable");
	Decision.Message = bHasSetterMetadata && !SetterMessage.IsEmpty()
		? SetterMessage
		: TEXT("Class default property is not writable.");
	Decision.SafeNextAction = TEXT("inspect_blueprint_class_settings_then_retry");
	return Decision;
}
