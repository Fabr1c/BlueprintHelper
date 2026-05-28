#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldPathResolution.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenWrappers.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionResolverUtils.h"

EBlueprintHelperFieldPathRole FBlueprintHelperFieldPathResolution::RoleFromScope(const FString& FieldScope)
{
	const FString Scope = UGraphWriteActionResolverUtils::CleanLower(FieldScope);
	if (Scope == TEXT("variable"))
	{
		return EBlueprintHelperFieldPathRole::Variable;
	}
	if (Scope == TEXT("property_path"))
	{
		return EBlueprintHelperFieldPathRole::PropertyPath;
	}
	if (Scope == TEXT("component_ref"))
	{
		return EBlueprintHelperFieldPathRole::ComponentRef;
	}
	if (Scope == TEXT("field_access"))
	{
		return EBlueprintHelperFieldPathRole::FieldAccess;
	}
	return EBlueprintHelperFieldPathRole::Unknown;
}

FString FBlueprintHelperFieldPathResolution::RoleToScope(EBlueprintHelperFieldPathRole Role)
{
	switch (Role)
	{
	case EBlueprintHelperFieldPathRole::Variable:
		return TEXT("variable");
	case EBlueprintHelperFieldPathRole::PropertyPath:
		return TEXT("property_path");
	case EBlueprintHelperFieldPathRole::ComponentRef:
		return TEXT("component_ref");
	case EBlueprintHelperFieldPathRole::FieldAccess:
		return TEXT("field_access");
	default:
		return TEXT("unknown");
	}
}

FBlueprintHelperResolvedFieldPath FBlueprintHelperFieldPathResolution::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const TMap<FString, FString>& Evidence)
{
	FBlueprintHelperResolvedFieldPath Result;
	Result.Role = RoleFromScope(Request.Semantic.FieldScope);
	Result.OwnerClassPath = UGraphWriteActionResolverUtils::ResolveOwnerEvidence(Request, Evidence);
	Result.OwnerPinType = Request.Semantic.TargetObjectPinType;
	Result.LeafPinType = Request.Semantic.ExpectedReturnPinType;

	switch (Result.Role)
	{
	case EBlueprintHelperFieldPathRole::Variable:
		Result.FullPath = FirstNonEmpty(Request.Semantic.TargetPath, Request.Semantic.Query);
		if (Result.FullPath.IsEmpty())
		{
			UGraphWriteActionResolverUtils::SetInvalid(Result, TEXT("needs_more_semantic_context"), TEXT("Field variable resolution requires target or query."));
			return Result;
		}
		Result.RootName = Result.FullPath;
		Result.LeafName = Result.FullPath;
		Result.Segments.Add(Result.FullPath);
		Result.bIsValid = true;
		return Result;

	case EBlueprintHelperFieldPathRole::PropertyPath:
		Result.FullPath = UGraphWriteActionResolverUtils::ComposePropertyFullPath(Request, Evidence);
		if (Result.FullPath.IsEmpty())
		{
			UGraphWriteActionResolverUtils::SetInvalid(Result, TEXT("missing_required_evidence"), TEXT("Property path field resolution requires property_path evidence."));
			return Result;
		}
		UGraphWriteActionResolverUtils::PopulateSegments(Result);
		Result.bRequiresFragmentDecomposition = Result.Segments.Num() > 2;
		Result.bIsValid = true;
		return Result;

	case EBlueprintHelperFieldPathRole::ComponentRef:
		Result.FullPath = FirstNonEmpty(
			UGraphWriteActionResolverUtils::EvidenceValue(Evidence, TEXT("component_property_name")),
			UGraphWriteActionResolverUtils::EvidenceValue(Evidence, TEXT("component_path")),
			Request.Semantic.TargetPath,
			Request.Semantic.Query);
		if (Result.FullPath.IsEmpty())
		{
			UGraphWriteActionResolverUtils::SetInvalid(Result, TEXT("needs_more_semantic_context"), TEXT("Component field resolution requires component_property_name or component target evidence."));
			return Result;
		}
		Result.RootName = Result.FullPath;
		Result.LeafName = Result.FullPath;
		Result.Segments.Add(Result.FullPath);
		Result.OwnerClassPath = FirstNonEmpty(
			UGraphWriteActionResolverUtils::EvidenceValue(Evidence, TEXT("component_binding_owner_class_path")),
			Result.OwnerClassPath);
		Result.bIsValid = true;
		return Result;

	case EBlueprintHelperFieldPathRole::FieldAccess:
		Result.OwnerClassPath = UGraphWriteActionResolverUtils::ResolveOwnerEvidence(Request, Evidence);
		if (Result.OwnerClassPath.IsEmpty() && !Result.OwnerPinType.IsValid())
		{
			UGraphWriteActionResolverUtils::SetInvalid(
				Result,
				TEXT("needs_more_semantic_context"),
				TEXT("Field access resolution requires owner class evidence or a target object pin type."));
			return Result;
		}
		Result.FullPath = FirstNonEmpty(
			Request.Semantic.PropertyPath,
			UGraphWriteActionResolverUtils::EvidenceValue(Evidence, TEXT("property_path")),
			Request.Semantic.TargetPath,
			Request.Semantic.Query);
		if (Result.FullPath.IsEmpty())
		{
			UGraphWriteActionResolverUtils::SetInvalid(Result, TEXT("needs_more_semantic_context"), TEXT("Field access resolution requires property_path, target, or query evidence."));
			return Result;
		}
		UGraphWriteActionResolverUtils::PopulateSegments(Result);
		if (Result.OwnerClassPath.IsEmpty())
		{
			Result.OwnerClassPath = UGraphWriteActionResolverUtils::DescribePinTypeEvidence(Result.OwnerPinType);
		}
		Result.bIsValid = true;
		return Result;

	case EBlueprintHelperFieldPathRole::Unknown:
	default:
		UGraphWriteActionResolverUtils::SetInvalid(Result, TEXT("needs_more_semantic_context"), TEXT("Unsupported field_scope for field path resolution."));
		return Result;
	}
}
