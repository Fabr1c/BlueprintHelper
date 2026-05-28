#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionEvidenceUtils.h"

const FBlueprintHelperContainerActionSpec* FBlueprintHelperContainerActionVocabulary::Find(
	const FString& ContainerKind,
	const FString& ContainerOperation)
{
	const FString Kind = UGraphWriteActionEvidenceUtils::NormalizeContainerActionToken(ContainerKind);
	const FString Operation = UGraphWriteActionEvidenceUtils::NormalizeContainerActionToken(ContainerOperation);
	for (const FBlueprintHelperContainerActionSpec& Spec : UGraphWriteActionEvidenceUtils::GetContainerActionSpecs())
	{
		if (UGraphWriteActionEvidenceUtils::NormalizeContainerActionToken(Spec.ContainerKind) == Kind && UGraphWriteActionEvidenceUtils::NormalizeContainerActionToken(Spec.ContainerOperation) == Operation)
		{
			return &Spec;
		}
	}
	return nullptr;
}

TArray<FBlueprintHelperContainerActionSpec> FBlueprintHelperContainerActionVocabulary::All()
{
	return UGraphWriteActionEvidenceUtils::GetContainerActionSpecs();
}
