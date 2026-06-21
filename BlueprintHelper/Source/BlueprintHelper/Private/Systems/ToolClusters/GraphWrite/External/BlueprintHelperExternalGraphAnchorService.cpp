#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorService.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorFingerprintService.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace BlueprintHelperExternalGraphAnchorService
{
	static bool IsBlueprintHelperOwned(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return false;
		}

		if (UPackage* Package = Node->GetOutermost())
		{
			FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
			return MetaData.GetValue(Node, TEXT("BlueprintHelperOwned")) == TEXT("true");
		}

		return false;
	}

	static FString DirectionToString(const EEdGraphPinDirection Direction)
	{
		switch (Direction)
		{
		case EGPD_Input:
			return TEXT("input");
		case EGPD_Output:
			return TEXT("output");
		default:
			return TEXT("unknown");
		}
	}
}

bool FBlueprintHelperExternalGraphAnchorService::BuildNodeAnchor(
	const FString& AssetPath,
	const FString& GraphName,
	const UEdGraphNode* Node,
	FBlueprintHelperExternalGraphAnchor& OutAnchor,
	FString& OutError) const
{
	if (!Node)
	{
		OutError = TEXT("external_anchor_node_not_found");
		return false;
	}
	if (BlueprintHelperExternalGraphAnchorService::IsBlueprintHelperOwned(Node))
	{
		OutError = TEXT("external_anchor_owned_node_not_supported");
		return false;
	}

	const FBlueprintHelperExternalGraphAnchorFingerprintService FingerprintService;
	OutAnchor = FBlueprintHelperExternalGraphAnchor();
	OutAnchor.AssetPath = AssetPath;
	OutAnchor.GraphName = GraphName;
	OutAnchor.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
	OutAnchor.NodeClass = Node->GetClass() ? Node->GetClass()->GetPathName() : TEXT("");
	OutAnchor.SemanticRole = EBlueprintHelperExternalGraphAnchorRole::Node;
	OutAnchor.Fingerprint = FingerprintService.BuildNodeFingerprint(Node);
	return true;
}

bool FBlueprintHelperExternalGraphAnchorService::BuildBodyEntryAnchor(
	const FString& AssetPath,
	const FString& GraphName,
	const UEdGraphNode* Node,
	FBlueprintHelperExternalGraphAnchor& OutAnchor,
	FString& OutError) const
{
	if (!Node)
	{
		OutError = TEXT("external_anchor_node_not_found");
		return false;
	}

	const FBlueprintHelperExternalGraphAnchorFingerprintService FingerprintService;
	OutAnchor = FBlueprintHelperExternalGraphAnchor();
	OutAnchor.AssetPath = AssetPath;
	OutAnchor.GraphName = GraphName;
	OutAnchor.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
	OutAnchor.NodeClass = Node->GetClass() ? Node->GetClass()->GetPathName() : TEXT("");
	OutAnchor.SemanticRole = EBlueprintHelperExternalGraphAnchorRole::BodyEntry;
	OutAnchor.Fingerprint = FingerprintService.BuildNodeFingerprint(Node);
	return true;
}

bool FBlueprintHelperExternalGraphAnchorService::BuildExecBoundaryAnchor(
	const FString& AssetPath,
	const FString& GraphName,
	const UEdGraphPin* SourcePin,
	FBlueprintHelperExternalGraphAnchor& OutAnchor,
	FString& OutError) const
{
	const UEdGraphNode* SourceNode = SourcePin ? SourcePin->GetOwningNode() : nullptr;
	if (!SourcePin || !SourceNode)
	{
		OutError = TEXT("external_anchor_pin_not_found");
		return false;
	}
	if (SourcePin->Direction != EGPD_Output || SourcePin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
	{
		OutError = TEXT("external_anchor_pin_not_found");
		return false;
	}
	if (BlueprintHelperExternalGraphAnchorService::IsBlueprintHelperOwned(SourceNode))
	{
		OutError = TEXT("external_anchor_owned_node_not_supported");
		return false;
	}

	const FBlueprintHelperExternalGraphAnchorFingerprintService FingerprintService;
	OutAnchor = FBlueprintHelperExternalGraphAnchor();
	OutAnchor.AssetPath = AssetPath;
	OutAnchor.GraphName = GraphName;
	OutAnchor.NodeGuid = SourceNode->NodeGuid.ToString(EGuidFormats::Digits);
	OutAnchor.NodeClass = SourceNode->GetClass() ? SourceNode->GetClass()->GetPathName() : TEXT("");
	OutAnchor.PinName = SourcePin->PinName.ToString();
	OutAnchor.PinDirection = BlueprintHelperExternalGraphAnchorService::DirectionToString(SourcePin->Direction);
	OutAnchor.SemanticRole = EBlueprintHelperExternalGraphAnchorRole::ExecBoundary;
	OutAnchor.Fingerprint = FingerprintService.BuildExecBoundaryFingerprint(SourcePin);
	return true;
}
