#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldActionReadback.h"

#include "EdGraph/EdGraphNode.h"
#include "K2Node.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionClusterUtils.h"

void FBlueprintHelperFieldActionReadback::AppendFlatFacts(TMap<FString, FString>& OutFacts) const
{
	UGraphWriteActionClusterUtils::AddFactIfPresent(OutFacts, TEXT("field.capability_id"), CapabilityId);
	UGraphWriteActionClusterUtils::AddFactIfPresent(OutFacts, TEXT("field.node_guid"), NodeGuid);
	UGraphWriteActionClusterUtils::AddFactIfPresent(OutFacts, TEXT("field.node_class"), NodeClassPath);
	UGraphWriteActionClusterUtils::AddFactIfPresent(OutFacts, TEXT("field.node_title"), NodeTitle);
	UGraphWriteActionClusterUtils::AddFactIfPresent(OutFacts, TEXT("field.expected_node_family"), ExpectedNodeFamily);
	for (const TPair<FString, FString>& FactPair : Facts)
	{
		UGraphWriteActionClusterUtils::AddFactIfPresent(OutFacts, FactPair.Key, FactPair.Value);
	}
}

FBlueprintHelperFieldActionReadback FBlueprintHelperFieldActionReadbackCollector::CollectFromNode(
	const FString& CapabilityId,
	UEdGraphNode* Node)
{
	FBlueprintHelperFieldActionReadback Readback;
	Readback.CapabilityId = CapabilityId;
	if (const FBlueprintHelperFieldCapabilitySpec* Spec = FBlueprintHelperFieldCapabilityRegistry::FindById(CapabilityId))
	{
		Readback.ExpectedNodeFamily = Spec->ExpectedNodeFamily;
	}
	if (!Node)
	{
		return Readback;
	}

	Readback.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
	Readback.NodeClassPath = Node->GetClass() ? Node->GetClass()->GetPathName() : FString();
	Readback.NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
	if (const UK2Node* K2Node = Cast<UK2Node>(Node))
	{
		Readback.Facts.Add(TEXT("field.k2_node_guid"), K2Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
	}
	return Readback;
}

void FBlueprintHelperFieldActionReadbackCollector::CollectCompileDiagnostics(
	UBlueprint* Blueprint,
	const FString& CapabilityId,
	TArray<FBlueprintHelperFieldCompileDiagnosticReadback>& OutDiagnostics)
{
	OutDiagnostics.Reset();
	if (!Blueprint)
	{
		FBlueprintHelperFieldCompileDiagnosticReadback Diagnostic;
		Diagnostic.CapabilityId = CapabilityId;
		Diagnostic.Code = TEXT("field_compile_blueprint_missing");
		Diagnostic.Message = TEXT("Blueprint is null while collecting field compile diagnostics.");
		OutDiagnostics.Add(MoveTemp(Diagnostic));
	}
}
