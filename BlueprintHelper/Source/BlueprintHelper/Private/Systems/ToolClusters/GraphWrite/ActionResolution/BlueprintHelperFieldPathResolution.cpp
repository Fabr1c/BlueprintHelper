#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldPathResolution.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenWrappers.h"

namespace
{
static FString CleanLower(const FString& Value)
{
	return Clean(Value).ToLower();
}

static FString EvidenceValue(const TMap<FString, FString>& Evidence, const TCHAR* Key)
{
	if (const FString* Value = Evidence.Find(Key))
	{
		return Clean(*Value);
	}
	return FString();
}

static void SetInvalid(FBlueprintHelperResolvedFieldPath& Result, const FString& Code, const FString& Message)
{
	Result.bIsValid = false;
	Result.ErrorCode = Code;
	Result.Message = Message;
}

static void PopulateSegments(FBlueprintHelperResolvedFieldPath& Result)
{
	Result.Segments.Reset();
	Result.FullPath.ParseIntoArray(Result.Segments, TEXT("."), true);
	for (FString& Segment : Result.Segments)
	{
		Segment = Clean(Segment);
	}
	Result.Segments.RemoveAll([](const FString& Segment)
	{
		return Segment.IsEmpty();
	});

	if (Result.Segments.Num() > 0)
	{
		Result.RootName = Result.Segments[0];
		Result.LeafName = Result.Segments.Last();
	}
	else
	{
		Result.RootName = Result.FullPath;
		Result.LeafName = Result.FullPath;
	}
}

static FString ResolveOwnerEvidence(
	const FBlueprintHelperActionResolutionRequest& Request,
	const TMap<FString, FString>& Evidence)
{
	return FirstNonEmpty(
		EvidenceValue(Evidence, TEXT("field_owner_class")),
		EvidenceValue(Evidence, TEXT("property_owner")),
		EvidenceValue(Evidence, TEXT("target_object_type")),
		Request.Semantic.TargetObjectType);
}

static FString DescribePinTypeEvidence(const FBlueprintHelperCallFunctionPinType& PinType)
{
	if (!PinType.IsValid())
	{
		return FString();
	}
	return FirstNonEmpty(PinType.Category, PinType.ObjectPath);
}

static FString ComposePropertyFullPath(
	const FBlueprintHelperActionResolutionRequest& Request,
	const TMap<FString, FString>& Evidence)
{
	const FString PropertyPath = FirstNonEmpty(
		Request.Semantic.PropertyPath,
		EvidenceValue(Evidence, TEXT("property_path")));
	const FString TargetPath = Clean(Request.Semantic.TargetPath);
	if (PropertyPath.IsEmpty())
	{
		return FString();
	}
	if (!TargetPath.IsEmpty()
		&& !TargetPath.Contains(TEXT("."))
		&& !PropertyPath.StartsWith(TargetPath + TEXT("."), ESearchCase::CaseSensitive))
	{
		return TargetPath + TEXT(".") + PropertyPath;
	}
	return PropertyPath;
}
}

EBlueprintHelperFieldPathRole FBlueprintHelperFieldPathResolution::RoleFromScope(const FString& FieldScope)
{
	const FString Scope = CleanLower(FieldScope);
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
	Result.OwnerClassPath = ResolveOwnerEvidence(Request, Evidence);
	Result.OwnerPinType = Request.Semantic.TargetObjectPinType;
	Result.LeafPinType = Request.Semantic.ExpectedReturnPinType;

	switch (Result.Role)
	{
	case EBlueprintHelperFieldPathRole::Variable:
		Result.FullPath = FirstNonEmpty(Request.Semantic.TargetPath, Request.Semantic.Query);
		if (Result.FullPath.IsEmpty())
		{
			SetInvalid(Result, TEXT("needs_more_semantic_context"), TEXT("Field variable resolution requires target or query."));
			return Result;
		}
		Result.RootName = Result.FullPath;
		Result.LeafName = Result.FullPath;
		Result.Segments.Add(Result.FullPath);
		Result.bIsValid = true;
		return Result;

	case EBlueprintHelperFieldPathRole::PropertyPath:
		Result.FullPath = ComposePropertyFullPath(Request, Evidence);
		if (Result.FullPath.IsEmpty())
		{
			SetInvalid(Result, TEXT("missing_required_evidence"), TEXT("Property path field resolution requires property_path evidence."));
			return Result;
		}
		PopulateSegments(Result);
		Result.bRequiresFragmentDecomposition = Result.Segments.Num() > 2;
		Result.bIsValid = true;
		return Result;

	case EBlueprintHelperFieldPathRole::ComponentRef:
		Result.FullPath = FirstNonEmpty(
			EvidenceValue(Evidence, TEXT("component_property_name")),
			EvidenceValue(Evidence, TEXT("component_path")),
			Request.Semantic.TargetPath,
			Request.Semantic.Query);
		if (Result.FullPath.IsEmpty())
		{
			SetInvalid(Result, TEXT("needs_more_semantic_context"), TEXT("Component field resolution requires component_property_name or component target evidence."));
			return Result;
		}
		Result.RootName = Result.FullPath;
		Result.LeafName = Result.FullPath;
		Result.Segments.Add(Result.FullPath);
		Result.OwnerClassPath = FirstNonEmpty(
			EvidenceValue(Evidence, TEXT("component_binding_owner_class_path")),
			Result.OwnerClassPath);
		Result.bIsValid = true;
		return Result;

	case EBlueprintHelperFieldPathRole::FieldAccess:
		Result.OwnerClassPath = ResolveOwnerEvidence(Request, Evidence);
		if (Result.OwnerClassPath.IsEmpty() && !Result.OwnerPinType.IsValid())
		{
			SetInvalid(
				Result,
				TEXT("needs_more_semantic_context"),
				TEXT("Field access resolution requires owner class evidence or a target object pin type."));
			return Result;
		}
		Result.FullPath = FirstNonEmpty(
			Request.Semantic.PropertyPath,
			EvidenceValue(Evidence, TEXT("property_path")),
			Request.Semantic.TargetPath,
			Request.Semantic.Query);
		if (Result.FullPath.IsEmpty())
		{
			SetInvalid(Result, TEXT("needs_more_semantic_context"), TEXT("Field access resolution requires property_path, target, or query evidence."));
			return Result;
		}
		PopulateSegments(Result);
		if (Result.OwnerClassPath.IsEmpty())
		{
			Result.OwnerClassPath = DescribePinTypeEvidence(Result.OwnerPinType);
		}
		Result.bIsValid = true;
		return Result;

	case EBlueprintHelperFieldPathRole::Unknown:
	default:
		SetInvalid(Result, TEXT("needs_more_semantic_context"), TEXT("Unsupported field_scope for field path resolution."));
		return Result;
	}
}
