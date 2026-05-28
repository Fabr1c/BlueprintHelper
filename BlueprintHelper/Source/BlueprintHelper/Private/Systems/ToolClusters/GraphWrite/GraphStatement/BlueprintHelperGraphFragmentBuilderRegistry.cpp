#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuildRequest.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementTypeUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/GraphWriteGraphStatementUtils.h"

bool FBlueprintHelperGraphFragmentBuilderRegistry::TryBuildStatement(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions,
	const TMap<FString, FBlueprintHelperCallFunctionPinType>* SemanticArgumentPinTypes)
{
	const FString StatementId = UGraphWriteGraphStatementUtils::GetStatementId(Statement);
	const FString StatementContextId = UGraphWriteGraphStatementUtils::GetStatementContextId(Statement);
	if (Statement.Kind == EBlueprintHelperGraphStatementKind::Call)
	{
		FBlueprintHelperGraphFragmentBuildRequest Request = FBlueprintHelperGraphFragmentBuildRequest::FromStatement(Statement);
		Request.FragmentId = StatementId;
		Request.SourceStatementId = StatementId;
		Request.ActionContextStatementId = StatementContextId;
		Request.Query = !Statement.Target.IsEmpty() ? Statement.Target : Statement.Name;
		Request.ResolvedStableId = Statement.ResolvedCallFunctionStableId;
		UGraphWriteGraphStatementUtils::FillLiteralArgsAsDefaultsAndTypes(Statement.Args, Request.DefaultValues, Request.ArgumentTypes);
		if (SemanticArgumentPinTypes)
		{
			Request.ArgumentPinTypes.Append(*SemanticArgumentPinTypes);
		}
		return FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment(
			TargetGraph,
			Request,
			OutFragment,
			OutError,
			OutCandidateFunctions,
			ActionContextScope);
	}

	if (Statement.Kind == EBlueprintHelperGraphStatementKind::Create)
	{
		FBlueprintHelperGraphFragmentBuildRequest Request = FBlueprintHelperGraphFragmentBuildRequest::FromStatement(Statement);
		Request.FragmentId = StatementId;
		Request.SourceStatementId = StatementId;
		Request.ActionContextStatementId = StatementContextId;
		Request.Query = Statement.FunctionOperation.Equals(TEXT("create_function"), ESearchCase::IgnoreCase)
			? (!Statement.Target.IsEmpty() ? Statement.Target : Statement.Name)
			: Statement.CreateOperation;
		Request.Target = !Statement.ClassPath.IsEmpty() ? Statement.ClassPath : Statement.Target;
		Request.TypeName = Statement.ClassPath;
		UGraphWriteGraphStatementUtils::FillLiteralArgsAsDefaultsAndTypes(Statement.Args, Request.DefaultValues, Request.ArgumentTypes);
		if (SemanticArgumentPinTypes)
		{
			Request.ArgumentPinTypes.Append(*SemanticArgumentPinTypes);
		}
		return FBlueprintHelperGraphStatementBuilder::BuildCreateFragment(
			TargetGraph,
			Request,
			OutFragment,
			OutError,
			ActionContextScope);
	}

	if (Statement.Kind == EBlueprintHelperGraphStatementKind::Control)
	{
		FBlueprintHelperGraphFragmentBuildRequest Request = FBlueprintHelperGraphFragmentBuildRequest::FromStatement(Statement);
		Request.FragmentId = StatementId;
		Request.SourceStatementId = StatementId;
		Request.ActionContextStatementId = StatementContextId;
		Request.Query = !Statement.ControlOperation.IsEmpty()
			? Statement.ControlOperation
			: Statement.ContextEvidence.FindRef(TEXT("generic.control.operation"));
		return FBlueprintHelperGraphStatementBuilder::BuildActionProviderFragment(
			TargetGraph,
			Request,
			EBlueprintHelperActionSemanticKind::Control,
			OutFragment,
			OutError,
			ActionContextScope);
	}

	if (Statement.Kind == EBlueprintHelperGraphStatementKind::Convert
		|| Statement.Kind == EBlueprintHelperGraphStatementKind::Schedule)
	{
		FBlueprintHelperGraphFragmentBuildRequest Request = FBlueprintHelperGraphFragmentBuildRequest::FromStatement(Statement);
		Request.FragmentId = StatementId;
		Request.SourceStatementId = StatementId;
		Request.ActionContextStatementId = StatementContextId;
		Request.Query = !Statement.TransformOperation.IsEmpty()
			? Statement.TransformOperation
			: (!Statement.ScheduleOperation.IsEmpty() ? Statement.ScheduleOperation : Statement.PatternName);
		Request.Target = !Statement.ClassPath.IsEmpty() ? Statement.ClassPath : Statement.Target;
		Request.TypeName = !Statement.ClassPath.IsEmpty() ? Statement.ClassPath : Statement.ResolvedTarget.Type;
		UGraphWriteGraphStatementUtils::FillLiteralArgsAsDefaultsAndTypes(Statement.Args, Request.DefaultValues, Request.ArgumentTypes);
		if (SemanticArgumentPinTypes)
		{
			Request.ArgumentPinTypes.Append(*SemanticArgumentPinTypes);
		}
		return FBlueprintHelperGraphStatementBuilder::BuildActionProviderFragment(
			TargetGraph,
			Request,
			Statement.Kind == EBlueprintHelperGraphStatementKind::Convert
				? EBlueprintHelperActionSemanticKind::Convert
				: EBlueprintHelperActionSemanticKind::Schedule,
			OutFragment,
			OutError,
			ActionContextScope);
	}

	if (Statement.Kind == EBlueprintHelperGraphStatementKind::ContainerAction)
	{
		FBlueprintHelperGraphFragmentBuildRequest Request = FBlueprintHelperGraphFragmentBuildRequest::FromStatement(Statement);
		Request.FragmentId = StatementId;
		Request.SourceStatementId = StatementId;
		Request.ActionContextStatementId = StatementContextId;
		Request.Query = Statement.ContainerKind + TEXT(".") + Statement.ContainerOperation;
		Request.Target = Statement.TargetObject.IsValid()
			? (!Statement.TargetObject->Target.IsEmpty() ? Statement.TargetObject->Target : Statement.TargetObject->Name)
			: Statement.Target;
		Request.TypeName = !Statement.ValueType.IsEmpty()
			? Statement.ValueType
			: (!Statement.ElementType.IsEmpty() ? Statement.ElementType : Statement.ResolvedTarget.Type);
		UGraphWriteGraphStatementUtils::FillLiteralArgsAsDefaultsAndTypes(Statement.Args, Request.DefaultValues, Request.ArgumentTypes);
		if (SemanticArgumentPinTypes)
		{
			Request.ArgumentPinTypes.Append(*SemanticArgumentPinTypes);
		}
		return FBlueprintHelperGraphStatementBuilder::BuildActionProviderFragment(
			TargetGraph,
			Request,
			EBlueprintHelperActionSemanticKind::ContainerAction,
			OutFragment,
			OutError,
			ActionContextScope);
	}

	if (Statement.Kind == EBlueprintHelperGraphStatementKind::Field
		&& !Statement.CapabilityId.TrimStartAndEnd().IsEmpty())
	{
		const FString FieldName = !Statement.ResolvedTarget.Member.IsEmpty()
			? Statement.ResolvedTarget.Member
			: (!Statement.Property.IsEmpty() ? Statement.Property : (!Statement.Target.IsEmpty() ? Statement.Target : Statement.Name));
		FBlueprintHelperGraphFragmentBuildRequest Request = FBlueprintHelperGraphFragmentBuildRequest::FromStatement(Statement);
		Request.FragmentId = StatementId;
		Request.SourceStatementId = StatementId;
		Request.ActionContextStatementId = StatementContextId;
		Request.Query = FieldName;
		Request.Target = !Statement.ResolvedTarget.Raw.IsEmpty() ? Statement.ResolvedTarget.Raw : FieldName;
		Request.PropertyPath = !Statement.ResolvedTarget.PropertyPath.IsEmpty() ? Statement.ResolvedTarget.PropertyPath : Statement.Property;
		if (Statement.Value.IsValid() && Statement.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			Request.DefaultValues.Add(FieldName, Statement.Value->LiteralValue);
			Request.DefaultValues.Add(TEXT("value"), Statement.Value->LiteralValue);
		}
		return FBlueprintHelperGraphStatementBuilder::BuildFieldCapabilityFragment(
			TargetGraph,
			Request,
			OutFragment,
			OutError,
			ActionContextScope);
	}

	if (Statement.Kind == EBlueprintHelperGraphStatementKind::Field
		&& Statement.FieldOperation.Equals(TEXT("set"), ESearchCase::IgnoreCase)
		&& Statement.FieldScope.Equals(TEXT("variable"), ESearchCase::IgnoreCase))
	{
		const FString VariableName = !Statement.ResolvedTarget.Member.IsEmpty()
			? Statement.ResolvedTarget.Member
			: Statement.Target;
		FBlueprintHelperGraphFragmentBuildRequest Request = FBlueprintHelperGraphFragmentBuildRequest::FromStatement(Statement);
		Request.FragmentId = StatementId;
		Request.SourceStatementId = StatementId;
		Request.ActionContextStatementId = StatementContextId;
		Request.Target = VariableName;
		if (Statement.Value.IsValid() && Statement.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			Request.DefaultValues.Add(VariableName, Statement.Value->LiteralValue);
			Request.DefaultValues.Add(TEXT("value"), Statement.Value->LiteralValue);
		}
		return FBlueprintHelperGraphStatementBuilder::BuildVariableSetFragment(
			TargetGraph,
			Request,
			OutFragment,
			OutError,
			ActionContextScope);
	}

	if (Statement.Kind == EBlueprintHelperGraphStatementKind::Field
		&& Statement.FieldOperation.Equals(TEXT("set"), ESearchCase::IgnoreCase)
		&& Statement.FieldScope.Equals(TEXT("property_path"), ESearchCase::IgnoreCase))
	{
		const FString PropertyTarget = !Statement.ResolvedTarget.Member.IsEmpty()
			? Statement.ResolvedTarget.Member
			: (!Statement.Property.IsEmpty() ? Statement.Property : Statement.Target);
		FBlueprintHelperGraphFragmentBuildRequest Request = FBlueprintHelperGraphFragmentBuildRequest::FromStatement(Statement);
		Request.FragmentId = StatementId;
		Request.SourceStatementId = StatementId;
		Request.ActionContextStatementId = StatementContextId;
		Request.Target = PropertyTarget;
		if (Statement.Value.IsValid() && Statement.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			Request.DefaultValues.Add(PropertyTarget, Statement.Value->LiteralValue);
			Request.DefaultValues.Add(TEXT("value"), Statement.Value->LiteralValue);
		}
		return FBlueprintHelperGraphStatementBuilder::BuildSetPropertyFragment(
			TargetGraph,
			Request,
			OutFragment,
			OutError,
			ActionContextScope);
	}

	if (Statement.Kind == EBlueprintHelperGraphStatementKind::Branch
		|| Statement.Kind == EBlueprintHelperGraphStatementKind::Sequence
		|| Statement.Kind == EBlueprintHelperGraphStatementKind::Return)
	{
		return FBlueprintHelperControlFragmentBuilder::BuildStatement(
			TargetGraph,
			ActionContextScope,
			Statement,
			OutFragment,
			OutError);
	}

	if (Statement.Kind == EBlueprintHelperGraphStatementKind::ComponentBoundEvent
		|| Statement.Kind == EBlueprintHelperGraphStatementKind::Delegate)
	{
		return FBlueprintHelperEventDelegateFragmentBuilder::BuildStatement(
			TargetGraph,
			ActionContextScope,
			Statement,
			OutFragment,
			OutError);
	}

	OutError = FString::Printf(TEXT("Semantic statement kind is not node-backed: %s."), *Statement.PatternName);
	return false;
}

bool FBlueprintHelperGraphFragmentBuilderRegistry::TryBuildExpression(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FBlueprintHelperGraphExpressionIR& Expression,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions)
{
	return FBlueprintHelperGraphStatementBuilder::BuildExpressionFragment(
		TargetGraph,
		Expression,
		OutFragment,
		OutError,
		OutCandidateFunctions,
		ActionContextScope);
}
