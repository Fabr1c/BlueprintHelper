#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuildRequest.h"

FBlueprintHelperGraphFragmentBuildRequest FBlueprintHelperGraphFragmentBuildRequest::FromStatement(
	const FBlueprintHelperGraphStatementIR& Statement)
{
	FBlueprintHelperGraphFragmentBuildRequest Request;
	Request.FragmentId = !Statement.StatementId.IsEmpty() ? Statement.StatementId : Statement.Path;
	Request.SourceStatementId = Request.FragmentId;
	Request.ActionContextStatementId = Request.FragmentId;
	Request.Query = !Statement.Target.IsEmpty() ? Statement.Target : Statement.Name;
	Request.Target = Statement.Target;
	Request.PropertyPath = Statement.Property;
	Request.CapabilityId = Statement.CapabilityId;
	Request.CapabilityFacts = Statement.CapabilityFacts;
	Request.FieldOperation = Statement.FieldOperation;
	Request.FieldScope = Statement.FieldScope;
	Request.FunctionOperation = Statement.FunctionOperation;
	Request.TransformOperation = Statement.TransformOperation;
	Request.ScheduleOperation = Statement.ScheduleOperation;
	Request.CreateOperation = Statement.CreateOperation;
	Request.ContainerKind = Statement.ContainerKind;
	Request.ContainerOperation = Statement.ContainerOperation;
	Request.ClassPath = Statement.ClassPath;
	Request.AssetPath = Statement.AssetPath;
	Request.GraphLatentAllowed = Statement.GraphLatentAllowed;
	Request.ElementType = Statement.ElementType;
	Request.KeyType = Statement.KeyType;
	Request.ValueType = Statement.ValueType;
	Request.PinType = Statement.PinType;
	Request.KeyPinType = Statement.KeyPinType;
	Request.ValuePinType = Statement.ValuePinType;
	Request.TypeName = Statement.Value.IsValid() ? Statement.Value->Type : Statement.ResolvedTarget.Type;
	Request.SearchMode = Statement.SearchMode;
	Request.AmbiguityPolicy = Statement.AmbiguityPolicy;
	Request.CategoryPriority = Statement.CategoryPriority;
	Request.ContextEvidence = Statement.ContextEvidence;
	Request.ResolvedStableId = Statement.ResolvedCallFunctionStableId;
	if (Statement.ResolvedTarget.Kind == EBlueprintHelperGraphTargetKind::ComponentMemberFunction)
	{
		Request.TargetObjectType = Statement.ResolvedTarget.Type;
	}
	if (Statement.TargetObject.IsValid())
	{
		Request.Target = !Statement.TargetObject->ResolvedTarget.Member.IsEmpty()
			? Statement.TargetObject->ResolvedTarget.Member
			: (!Statement.TargetObject->Target.IsEmpty() ? Statement.TargetObject->Target : Statement.TargetObject->Name);
		Request.TargetObjectType = Statement.TargetObject->Type;
	}
	return Request;
}

FBlueprintHelperGraphFragmentBuildRequest FBlueprintHelperGraphFragmentBuildRequest::FromExpression(
	const FBlueprintHelperGraphExpressionIR& Expression)
{
	FBlueprintHelperGraphFragmentBuildRequest Request;
	Request.bIsExpression = true;
	Request.FragmentId = Expression.ExpressionId;
	Request.SourceStatementId = Expression.ExpressionId;
	Request.ActionContextStatementId = Expression.ExpressionId;
	Request.Query = Expression.Target;
	Request.Target = Expression.Target;
	Request.PropertyPath = Expression.ResolvedTarget.PropertyPath;
	Request.CapabilityId = Expression.CapabilityId;
	Request.CapabilityFacts = Expression.CapabilityFacts;
	Request.FieldOperation = Expression.FieldOperation;
	Request.FieldScope = Expression.FieldScope;
	Request.FunctionOperation = Expression.FunctionOperation;
	Request.TransformOperation = Expression.TransformOperation;
	Request.ScheduleOperation = Expression.ScheduleOperation;
	Request.CreateOperation = Expression.CreateOperation;
	Request.ContainerKind = Expression.ContainerKind;
	Request.ContainerOperation = Expression.ContainerOperation;
	Request.ClassPath = Expression.ClassPath;
	Request.AssetPath = Expression.AssetPath;
	Request.GraphLatentAllowed = Expression.GraphLatentAllowed;
	Request.ElementType = Expression.ElementType;
	Request.KeyType = Expression.KeyType;
	Request.ValueType = Expression.ValueType;
	Request.PinType = Expression.PinType;
	Request.KeyPinType = Expression.KeyPinType;
	Request.ValuePinType = Expression.ValuePinType;
	Request.TypeName = Expression.Type;
	Request.ExpectedReturnType = Expression.Type;
	Request.SearchMode = Expression.SearchMode;
	Request.AmbiguityPolicy = Expression.AmbiguityPolicy;
	Request.CategoryPriority = Expression.CategoryPriority;
	Request.ContextEvidence = Expression.ContextEvidence;
	if (Expression.TargetObject.IsValid())
	{
		Request.Target = Expression.TargetObject->Target;
		Request.TargetObjectType = Expression.TargetObject->Type;
	}
	return Request;
}

