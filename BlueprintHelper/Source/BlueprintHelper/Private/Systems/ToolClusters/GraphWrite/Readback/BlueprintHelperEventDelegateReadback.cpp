#include "Systems/ToolClusters/GraphWrite/Readback/BlueprintHelperEventDelegateReadback.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_CreateDelegate.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"
#include "Systems/ToolClusters/GraphWrite/Readback/Utils/GraphWriteReadbackUtils.h"

FBlueprintHelperEventDelegateReadbackFacts FBlueprintHelperEventDelegateReadback::Collect(
	const FBlueprintHelperNodeFragment& Fragment,
	const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence)
{
	FBlueprintHelperEventDelegateReadbackFacts Result;
	TMap<FString, FString>& Facts = Result.Facts;

	UGraphWriteReadbackUtils::AddPrimaryNodeFacts(Fragment, Facts);
	UGraphWriteReadbackUtils::AddIfPresent(Facts, TEXT("spawner_or_factory_kind"), Evidence.DelegateOperation.Equals(TEXT("assign"), ESearchCase::IgnoreCase)
		? TEXT("ue_delegate_manual_assign_factory")
		: TEXT("ue_delegate_node_spawner"));
	UGraphWriteReadbackUtils::AddIfPresent(Facts, TEXT("delegate_owner_class_path"), Evidence.DelegateOwnerClassPath);
	UGraphWriteReadbackUtils::AddIfPresent(Facts, TEXT("delegate_property_path"), Evidence.DelegatePropertyPath);
	UGraphWriteReadbackUtils::AddIfPresent(Facts, TEXT("delegate_signature_function_path"), Evidence.DelegateSignatureFunctionPath);
	UGraphWriteReadbackUtils::AddIfPresent(Facts, TEXT("binding_object_kind"), Evidence.BindingObjectKind);
	UGraphWriteReadbackUtils::AddIfPresent(Facts, TEXT("binding_object_evidence_id"), Evidence.BindingObjectEvidenceId);
	UGraphWriteReadbackUtils::AddIfPresent(Facts, TEXT("handler_function_path"), Evidence.HandlerFunctionPath);
	UGraphWriteReadbackUtils::AddIfPresent(Facts, TEXT("statement_id"), Fragment.SourceStatementId);
	if (!Facts.FindRef(TEXT("statement_id")).IsEmpty() && !Facts.FindRef(TEXT("node_guid")).IsEmpty())
	{
		Facts.Add(
			TEXT("compile_diagnostic_correlation_key"),
			FString::Printf(TEXT("%s:%s"), *Facts.FindRef(TEXT("statement_id")), *Facts.FindRef(TEXT("node_guid"))));
	}

	for (UEdGraphNode* Node : Fragment.Nodes)
	{
		if (const UK2Node_CreateDelegate* CreateDelegate = Cast<UK2Node_CreateDelegate>(Node))
		{
			UGraphWriteReadbackUtils::AddIfPresent(Facts, TEXT("create_delegate.handler_name"), CreateDelegate->GetFunctionName().ToString());
		}
	}

	UGraphWriteReadbackUtils::AddPinFacts(Fragment, Facts);
	return Result;
}
