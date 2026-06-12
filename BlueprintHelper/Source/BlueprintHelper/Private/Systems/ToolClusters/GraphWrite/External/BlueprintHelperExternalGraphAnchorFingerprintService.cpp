#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorFingerprintService.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Misc/Crc.h"
#include "Misc/SecureHash.h"

namespace BlueprintHelperExternalGraphAnchorFingerprint
{
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

	static FString NodeGuidString(const UEdGraphNode* Node)
	{
		return Node ? Node->NodeGuid.ToString(EGuidFormats::Digits) : TEXT("");
	}

	static FString NodeClassPath(const UEdGraphNode* Node)
	{
		return Node && Node->GetClass() ? Node->GetClass()->GetPathName() : TEXT("");
	}

	static FString PinCategoryString(const UEdGraphPin* Pin)
	{
		return Pin ? Pin->PinType.PinCategory.ToString() : TEXT("");
	}

	static FString PinSubCategoryString(const UEdGraphPin* Pin)
	{
		return Pin ? Pin->PinType.PinSubCategory.ToString() : TEXT("");
	}

	static FString PinSubCategoryObjectPath(const UEdGraphPin* Pin)
	{
		return Pin && Pin->PinType.PinSubCategoryObject.IsValid()
			? Pin->PinType.PinSubCategoryObject->GetPathName()
			: TEXT("");
	}

	static FString PinStableText(const UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return TEXT("");
		}

		return FString::Printf(
			TEXT("pin:%s|%s|%s|%s|%s"),
			*Pin->PinName.ToString(),
			*DirectionToString(Pin->Direction),
			*PinCategoryString(Pin),
			*PinSubCategoryString(Pin),
			*PinSubCategoryObjectPath(Pin));
	}

	static FString HashStableText(const FString& StableText)
	{
		return FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*StableText));
	}

	static FString HashStableTextSha1Short(const FString& StableText)
	{
		const FTCHARToUTF8 Utf8Text(*StableText);
		const FSHAHash Hash = FSHA1::HashBuffer(Utf8Text.Get(), Utf8Text.Length());
		return Hash.ToString().Left(10).ToLower();
	}
}

FString FBlueprintHelperExternalGraphAnchorFingerprintService::BuildNodeFingerprint(const UEdGraphNode* Node) const
{
	if (!Node)
	{
		return TEXT("");
	}

	TArray<FString> PinTexts;
	for (const UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin)
		{
			PinTexts.Add(BlueprintHelperExternalGraphAnchorFingerprint::PinStableText(Pin));
		}
	}
	PinTexts.Sort();

	FString StableText = FString::Printf(
		TEXT("external-node|guid=%s|class=%s"),
		*BlueprintHelperExternalGraphAnchorFingerprint::NodeGuidString(Node),
		*BlueprintHelperExternalGraphAnchorFingerprint::NodeClassPath(Node));
	for (const FString& PinText : PinTexts)
	{
		StableText += TEXT("|");
		StableText += PinText;
	}

	return BlueprintHelperExternalGraphAnchorFingerprint::HashStableText(StableText);
}

FString FBlueprintHelperExternalGraphAnchorFingerprintService::BuildPinFingerprint(const UEdGraphPin* Pin) const
{
	const UEdGraphNode* Node = Pin ? Pin->GetOwningNode() : nullptr;
	if (!Pin || !Node)
	{
		return TEXT("");
	}

	const FString StableText = FString::Printf(
		TEXT("external-pin|node_guid=%s|node_class=%s|%s"),
		*BlueprintHelperExternalGraphAnchorFingerprint::NodeGuidString(Node),
		*BlueprintHelperExternalGraphAnchorFingerprint::NodeClassPath(Node),
		*BlueprintHelperExternalGraphAnchorFingerprint::PinStableText(Pin));
	return BlueprintHelperExternalGraphAnchorFingerprint::HashStableText(StableText);
}

FString FBlueprintHelperExternalGraphAnchorFingerprintService::BuildCompactNodeFingerprint(const UEdGraphNode* Node) const
{
	const FString ExpandedFingerprint = BuildNodeFingerprint(Node);
	return ExpandedFingerprint.Len() <= 10 ? ExpandedFingerprint : ExpandedFingerprint.Left(10);
}

FString FBlueprintHelperExternalGraphAnchorFingerprintService::BuildCompactPinFingerprint(const UEdGraphPin* Pin) const
{
	const UEdGraphNode* Node = Pin ? Pin->GetOwningNode() : nullptr;
	if (!Pin || !Node)
	{
		return TEXT("");
	}

	const FString StableText = FString::Printf(
		TEXT("%s|%s|%s"),
		*BlueprintHelperExternalGraphAnchorFingerprint::NodeGuidString(Node),
		*Pin->PinName.ToString(),
		*BuildNodeFingerprint(Node));
	return BlueprintHelperExternalGraphAnchorFingerprint::HashStableTextSha1Short(StableText);
}

FString FBlueprintHelperExternalGraphAnchorFingerprintService::BuildExecBoundaryFingerprint(const UEdGraphPin* SourcePin) const
{
	const UEdGraphNode* SourceNode = SourcePin ? SourcePin->GetOwningNode() : nullptr;
	if (!SourcePin || !SourceNode)
	{
		return TEXT("");
	}

	TArray<FString> LinkedEndpoints;
	for (const UEdGraphPin* LinkedPin : SourcePin->LinkedTo)
	{
		const UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
		if (!LinkedPin || !LinkedNode)
		{
			continue;
		}

		LinkedEndpoints.Add(FString::Printf(
			TEXT("%s.%s"),
			*BlueprintHelperExternalGraphAnchorFingerprint::NodeGuidString(LinkedNode),
			*LinkedPin->PinName.ToString()));
	}
	LinkedEndpoints.Sort();

	FString StableText = FString::Printf(
		TEXT("external-exec-boundary|node_guid=%s|node_class=%s|%s"),
		*BlueprintHelperExternalGraphAnchorFingerprint::NodeGuidString(SourceNode),
		*BlueprintHelperExternalGraphAnchorFingerprint::NodeClassPath(SourceNode),
		*BlueprintHelperExternalGraphAnchorFingerprint::PinStableText(SourcePin));
	for (const FString& Endpoint : LinkedEndpoints)
	{
		StableText += TEXT("|link=");
		StableText += Endpoint;
	}

	return BlueprintHelperExternalGraphAnchorFingerprint::HashStableText(StableText);
}

FString FBlueprintHelperExternalGraphAnchorFingerprintService::BuildLinkFingerprint(
	const UEdGraphPin* SourcePin,
	const UEdGraphPin* TargetPin,
	const FString& LinkKind) const
{
	const UEdGraphNode* SourceNode = SourcePin ? SourcePin->GetOwningNode() : nullptr;
	const UEdGraphNode* TargetNode = TargetPin ? TargetPin->GetOwningNode() : nullptr;
	if (!SourcePin || !TargetPin || !SourceNode || !TargetNode)
	{
		return TEXT("");
	}

	const FString StableText = FString::Printf(
		TEXT("%s|%s|%s|%s|%s|%s|%s"),
		*LinkKind.ToLower(),
		*BlueprintHelperExternalGraphAnchorFingerprint::NodeGuidString(SourceNode),
		*SourcePin->PinName.ToString(),
		*BuildNodeFingerprint(SourceNode),
		*BlueprintHelperExternalGraphAnchorFingerprint::NodeGuidString(TargetNode),
		*TargetPin->PinName.ToString(),
		*BuildNodeFingerprint(TargetNode));
	return BlueprintHelperExternalGraphAnchorFingerprint::HashStableTextSha1Short(StableText);
}
