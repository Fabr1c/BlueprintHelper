// BlueprintHelper call_function resolver anonymous namespace utility helpers implementation.

#include "Systems/ToolClusters/GraphWrite/FunctionResolution/Utils/GraphWriteFunctionResolutionUtils.h"

#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/Utils/BlueprintHelperCallFunctionResolverUtils.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

FString UGraphWriteFunctionResolutionUtils::DescribePinTypeForDiagnostics(const FBlueprintHelperCallFunctionPinType& PinType)
{
	TArray<FString> Parts;
	if (!PinType.Category.IsEmpty())
	{
		Parts.Add(FString::Printf(TEXT("category=%s"), *PinType.Category));
	}
	if (!PinType.SubCategory.IsEmpty())
	{
		Parts.Add(FString::Printf(TEXT("subcategory=%s"), *PinType.SubCategory));
	}
	if (!PinType.ObjectPath.IsEmpty())
	{
		Parts.Add(FString::Printf(TEXT("object=%s"), *PinType.ObjectPath));
	}
	if (!PinType.ContainerType.IsEmpty())
	{
		Parts.Add(FString::Printf(TEXT("container=%s"), *PinType.ContainerType));
	}
	if (PinType.bIsReference)
	{
		Parts.Add(TEXT("ref"));
	}
	if (PinType.bIsConst)
	{
		Parts.Add(TEXT("const"));
	}
	return Parts.Num() > 0 ? FString::Join(Parts, TEXT(",")) : FString(TEXT("unknown"));
}

UClass* UGraphWriteFunctionResolutionUtils::ResolveClassFromPinType(const FBlueprintHelperCallFunctionPinType& PinType)
{
	if (!PinType.ObjectPath.IsEmpty())
	{
		if (UClass* Class = FBlueprintHelperCallFunctionResolverUtils::ResolveClassByTypeName(PinType.ObjectPath))
		{
			return Class;
		}
	}
	if (!PinType.SubCategory.IsEmpty())
	{
		if (UClass* Class = FBlueprintHelperCallFunctionResolverUtils::ResolveClassByTypeName(PinType.SubCategory))
		{
			return Class;
		}
	}
	return nullptr;
}

UClass* UGraphWriteFunctionResolutionUtils::ResolveNodeClassByPath(const FString& NodeClassPath)
{
	const FString CleanPath = NodeClassPath.TrimStartAndEnd();
	if (CleanPath.IsEmpty())
	{
		return nullptr;
	}
	if (CleanPath.Equals(UK2Node_CallFunction::StaticClass()->GetPathName(), ESearchCase::IgnoreCase))
	{
		return UK2Node_CallFunction::StaticClass();
	}
	if (CleanPath.Equals(UK2Node_CallArrayFunction::StaticClass()->GetPathName(), ESearchCase::IgnoreCase))
	{
		return UK2Node_CallArrayFunction::StaticClass();
	}
	if (CleanPath.Equals(UK2Node_CommutativeAssociativeBinaryOperator::StaticClass()->GetPathName(), ESearchCase::IgnoreCase))
	{
		return UK2Node_CommutativeAssociativeBinaryOperator::StaticClass();
	}
	return FindObject<UClass>(nullptr, *CleanPath);
}

TSubclassOf<UK2Node_CallFunction> UGraphWriteFunctionResolutionUtils::InferNodeClassForFunction(const UFunction* Function)
{
	if (!Function)
	{
		return UK2Node_CallFunction::StaticClass();
	}
	if (Function->HasMetaData(TEXT("ArrayParm")))
	{
		return UK2Node_CallArrayFunction::StaticClass();
	}
	if (Function->HasMetaData(TEXT("CommutativeAssociativeBinaryOperator")))
	{
		return UK2Node_CommutativeAssociativeBinaryOperator::StaticClass();
	}
	return UK2Node_CallFunction::StaticClass();
}

TSubclassOf<UK2Node_CallFunction> UGraphWriteFunctionResolutionUtils::ResolveCandidateNodeClass(
	const UFunction* Function,
	const UBlueprintNodeSpawner* NodeSpawner)
{
	if (NodeSpawner && NodeSpawner->NodeClass)
	{
		return TSubclassOf<UK2Node_CallFunction>(*NodeSpawner->NodeClass);
	}
	return InferNodeClassForFunction(Function);
}

void UGraphWriteFunctionResolutionUtils::GetPermittedNodeClasses(
	const FBlueprintHelperCallFunctionResolveRequest& Request,
	TArray<UClass*>& OutNodeClasses)
{
	if (Request.CandidatePolicy.PermittedNodeClassPaths.Num() == 0)
	{
		OutNodeClasses.Add(UK2Node_CallFunction::StaticClass());
		return;
	}

	for (const FString& NodeClassPath : Request.CandidatePolicy.PermittedNodeClassPaths)
	{
		if (UClass* NodeClass = ResolveNodeClassByPath(NodeClassPath))
		{
			OutNodeClasses.AddUnique(NodeClass);
		}
	}
}

bool UGraphWriteFunctionResolutionUtils::IsStableCallableIdPermitted(
	const FString& StableId,
	const FBlueprintHelperCallFunctionResolveRequest& Request)
{
	if (Request.CandidatePolicy.RequiredStableCallableIds.Num() == 0)
	{
		return true;
	}
	return Request.CandidatePolicy.RequiredStableCallableIds.ContainsByPredicate(
		[&StableId](const FString& RequiredStableId)
		{
			return StableId.Equals(RequiredStableId.TrimStartAndEnd(), ESearchCase::IgnoreCase);
		});
}

UFunction* UGraphWriteFunctionResolutionUtils::ResolveStableCallableFunction(const FString& StableCallableId)
{
	FString OwnerPath;
	FString FunctionName;
	if (!FBlueprintHelperCallFunctionResolver::TryParseQualifiedQuery(StableCallableId, OwnerPath, FunctionName))
	{
		return nullptr;
	}

	UClass* OwnerClass = FBlueprintHelperCallFunctionResolverUtils::ResolveClassByTypeName(OwnerPath);
	if (!OwnerClass)
	{
		return nullptr;
	}

	return OwnerClass->FindFunctionByName(FName(*FunctionName));
}

bool UGraphWriteFunctionResolutionUtils::IsNodeClassPermitted(
	const UClass* NodeClass,
	const FBlueprintHelperCallFunctionResolveRequest& Request)
{
	TArray<UClass*> PermittedNodeClasses;
	GetPermittedNodeClasses(Request, PermittedNodeClasses);
	if (!NodeClass)
	{
		return false;
	}

	for (const UClass* PermittedNodeClass : PermittedNodeClasses)
	{
		if (PermittedNodeClass && NodeClass->GetPathName().Equals(PermittedNodeClass->GetPathName(), ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

bool UGraphWriteFunctionResolutionUtils::IsContainerPinCompatibleWithProperty(const FBlueprintHelperCallFunctionPinType& PinType, const FProperty* Property)
{
	const FString ContainerToken = FBlueprintHelperCallFunctionResolverUtils::NormalizeTypeToken(PinType.ContainerType);
	const FString CategoryToken = FBlueprintHelperCallFunctionResolverUtils::NormalizeTypeToken(PinType.Category);
	if (ContainerToken.IsEmpty() && CategoryToken != TEXT("tarray") && CategoryToken != TEXT("array")
		&& CategoryToken != TEXT("tset") && CategoryToken != TEXT("set")
		&& CategoryToken != TEXT("tmap") && CategoryToken != TEXT("map"))
	{
		return true;
	}

	if (ContainerToken == TEXT("array") || CategoryToken == TEXT("tarray") || CategoryToken == TEXT("array"))
	{
		return CastField<FArrayProperty>(Property) != nullptr;
	}
	if (ContainerToken == TEXT("set") || CategoryToken == TEXT("tset") || CategoryToken == TEXT("set"))
	{
		return CastField<FSetProperty>(Property) != nullptr;
	}
	if (ContainerToken == TEXT("map") || CategoryToken == TEXT("tmap") || CategoryToken == TEXT("map"))
	{
		return CastField<FMapProperty>(Property) != nullptr;
	}
	return true;
}

UFunction* UGraphWriteFunctionResolutionUtils::CanonicalizeActionDatabaseFunction(const UFunction* Function)
{
	if (!Function)
	{
		return nullptr;
	}

	const UClass* OwnerClass = Function->GetOwnerClass();
	const UBlueprint* OwnerBlueprint = OwnerClass
		? Cast<UBlueprint>(OwnerClass->ClassGeneratedBy)
		: nullptr;
	if (!OwnerBlueprint
		|| OwnerBlueprint->SkeletonGeneratedClass.Get() != OwnerClass
		|| !OwnerBlueprint->GeneratedClass)
	{
		return const_cast<UFunction*>(Function);
	}

	if (UFunction* GeneratedFunction = OwnerBlueprint->GeneratedClass->FindFunctionByName(Function->GetFName()))
	{
		return GeneratedFunction;
	}
	return const_cast<UFunction*>(Function);
}

FString UGraphWriteFunctionResolutionUtils::StripLeadingBoolPrefixForCompare(const FString& Name)
{
	if (Name.Len() <= 1 || !Name.StartsWith(TEXT("b")))
	{
		return Name;
	}

	const TCHAR NextChar = Name[1];
	if (!FChar::IsUpper(NextChar))
	{
		return Name;
	}

	return Name.RightChop(1);
}

bool UGraphWriteFunctionResolutionUtils::IsTargetObjectSemanticPortName(const FString& Name)
{
	return Name.Equals(TEXT("target_object"), ESearchCase::IgnoreCase);
}

void UGraphWriteFunctionResolutionUtils::RemoveTargetObjectSemanticPortFromContext(FBlueprintHelperK2CallContext& Context)
{
	Context.ArgumentNames.RemoveAll([](const FString& ArgumentName)
	{
		return IsTargetObjectSemanticPortName(ArgumentName);
	});

	TArray<FString> ArgumentTypeKeys;
	Context.ArgumentTypes.GetKeys(ArgumentTypeKeys);
	for (const FString& Key : ArgumentTypeKeys)
	{
		if (!IsTargetObjectSemanticPortName(Key))
		{
			continue;
		}
		if (Context.TargetObjectType.IsEmpty())
		{
			Context.TargetObjectType = Context.ArgumentTypes.FindRef(Key);
		}
		Context.ArgumentTypes.Remove(Key);
	}

	TArray<FString> ArgumentPinTypeKeys;
	Context.ArgumentPinTypes.GetKeys(ArgumentPinTypeKeys);
	for (const FString& Key : ArgumentPinTypeKeys)
	{
		if (!IsTargetObjectSemanticPortName(Key))
		{
			continue;
		}
		if (!Context.TargetObjectPinType.IsValid())
		{
			Context.TargetObjectPinType = Context.ArgumentPinTypes.FindRef(Key);
		}
		Context.ArgumentPinTypes.Remove(Key);
	}
}

void UGraphWriteFunctionResolutionUtils::AddRequiredStableCallableCandidates(
	const FBlueprintHelperCallFunctionResolveRequest& Request,
	TMap<FString, FBlueprintHelperCallFunctionCandidate>& InOutCandidates)
{
	for (const FString& RequiredStableId : Request.CandidatePolicy.RequiredStableCallableIds)
	{
		if (UFunction* Function = ResolveStableCallableFunction(RequiredStableId.TrimStartAndEnd()))
		{
			FBlueprintHelperCallFunctionResolverUtils::AddCandidateForFunction(Function, Request, InOutCandidates);
		}
	}
}
