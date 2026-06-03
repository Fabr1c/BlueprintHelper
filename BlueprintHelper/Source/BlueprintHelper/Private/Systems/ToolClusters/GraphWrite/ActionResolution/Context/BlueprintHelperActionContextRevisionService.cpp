#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextRevisionService.h"

#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/Crc.h"

class FBlueprintHelperActionContextRevisionServiceLocalUtils
{
public:
	static int32 HashCanonical(const FString& Canonical)
	{
		return static_cast<int32>(FCrc::StrCrc32(*Canonical));
	}

	static FString ContainerTypeToString(const EPinContainerType ContainerType)
	{
		switch (ContainerType)
		{
		case EPinContainerType::Array:
			return TEXT("array");
		case EPinContainerType::Set:
			return TEXT("set");
		case EPinContainerType::Map:
			return TEXT("map");
		case EPinContainerType::None:
		default:
			return TEXT("none");
		}
	}

	static FString PinTypeToString(const FEdGraphPinType& PinType)
	{
		TArray<FString> Parts;
		Parts.Add(FString::Printf(TEXT("category=%s"), *PinType.PinCategory.ToString()));
		Parts.Add(FString::Printf(TEXT("subcategory=%s"), *PinType.PinSubCategory.ToString()));
		Parts.Add(FString::Printf(TEXT("container=%s"), *ContainerTypeToString(PinType.ContainerType)));
		Parts.Add(FString::Printf(TEXT("object=%s"), PinType.PinSubCategoryObject.IsValid() ? *PinType.PinSubCategoryObject->GetPathName() : TEXT("")));
		Parts.Add(FString::Printf(TEXT("ref=%d"), PinType.bIsReference ? 1 : 0));
		Parts.Add(FString::Printf(TEXT("const=%d"), PinType.bIsConst ? 1 : 0));
		return FString::Join(Parts, TEXT("|"));
	}

	static void AddGraphFacts(TArray<FString>& Parts, const TCHAR* Prefix, const TArray<UEdGraph*>& Graphs)
	{
		for (const UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}

			Parts.Add(FString::Printf(
				TEXT("%s|name=%s|schema=%s"),
				Prefix,
				*Graph->GetName(),
				Graph->Schema ? *Graph->Schema->GetPathName() : TEXT("")));
		}
	}

	static FString BuildBlueprintCanonical(UBlueprint* Blueprint)
	{
		TArray<FString> Parts;
		if (!Blueprint)
		{
			return TEXT("blueprint=null");
		}

		Parts.Add(FString::Printf(TEXT("asset=%s"), *Blueprint->GetPathName()));
		Parts.Add(FString::Printf(TEXT("generated_class=%s"), Blueprint->GeneratedClass ? *Blueprint->GeneratedClass->GetPathName() : TEXT("")));
		Parts.Add(FString::Printf(TEXT("parent_class=%s"), Blueprint->ParentClass ? *Blueprint->ParentClass->GetPathName() : TEXT("")));

		for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
		{
			Parts.Add(FString::Printf(
				TEXT("var|name=%s|guid=%s|type=%s|flags=%llu"),
				*Variable.VarName.ToString(),
				*Variable.VarGuid.ToString(EGuidFormats::Digits),
				*PinTypeToString(Variable.VarType),
				static_cast<uint64>(Variable.PropertyFlags)));
		}

		AddGraphFacts(Parts, TEXT("ubergraph"), Blueprint->UbergraphPages);
		AddGraphFacts(Parts, TEXT("function_graph"), Blueprint->FunctionGraphs);
		AddGraphFacts(Parts, TEXT("macro_graph"), Blueprint->MacroGraphs);
		AddGraphFacts(Parts, TEXT("delegate_graph"), Blueprint->DelegateSignatureGraphs);

		if (Blueprint->SimpleConstructionScript)
		{
			for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
			{
				if (!Node)
				{
					continue;
				}

				Parts.Add(FString::Printf(
					TEXT("component|name=%s|class=%s|guid=%s"),
					*Node->GetVariableName().ToString(),
					Node->ComponentClass ? *Node->ComponentClass->GetPathName() : TEXT(""),
					*Node->VariableGuid.ToString(EGuidFormats::Digits)));
			}
		}

		for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
		{
			Parts.Add(FString::Printf(
				TEXT("interface|class=%s"),
				Interface.Interface ? *Interface.Interface->GetPathName() : TEXT("")));
		}

		Parts.Sort();
		return FString::Join(Parts, TEXT("\n"));
	}

	static FString BuildGraphCanonical(UEdGraph* Graph)
	{
		TArray<FString> Parts;
		TSet<FString> LinkFacts;
		if (!Graph)
		{
			return TEXT("graph=null");
		}

		Parts.Add(FString::Printf(
			TEXT("graph=%s|schema=%s"),
			*Graph->GetPathName(),
			Graph->Schema ? *Graph->Schema->GetPathName() : TEXT("")));

		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			const FString NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
			Parts.Add(FString::Printf(
				TEXT("node|guid=%s|class=%s|title=%s"),
				*NodeGuid,
				*Node->GetClass()->GetPathName(),
				*Node->GetNodeTitle(ENodeTitleType::ListView).ToString()));

			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin)
				{
					continue;
				}

				Parts.Add(FString::Printf(
					TEXT("pin|node=%s|name=%s|dir=%d|type=%s"),
					*NodeGuid,
					*Pin->PinName.ToString(),
					static_cast<int32>(Pin->Direction),
					*PinTypeToString(Pin->PinType)));

				for (const UEdGraphPin* LinkedTo : Pin->LinkedTo)
				{
					const UEdGraphNode* LinkedNode = LinkedTo ? LinkedTo->GetOwningNode() : nullptr;
					if (!LinkedTo || !LinkedNode)
					{
						continue;
					}

					const FString ThisEndpoint = FString::Printf(TEXT("%s.%s"), *NodeGuid, *Pin->PinName.ToString());
					const FString OtherEndpoint = FString::Printf(
						TEXT("%s.%s"),
						*LinkedNode->NodeGuid.ToString(EGuidFormats::Digits),
						*LinkedTo->PinName.ToString());
					LinkFacts.Add(ThisEndpoint < OtherEndpoint
						? FString::Printf(TEXT("link|%s|%s"), *ThisEndpoint, *OtherEndpoint)
						: FString::Printf(TEXT("link|%s|%s"), *OtherEndpoint, *ThisEndpoint));
				}
			}
		}

		for (const FString& LinkFact : LinkFacts)
		{
			Parts.Add(LinkFact);
		}
		Parts.Sort();
		return FString::Join(Parts, TEXT("\n"));
	}
};

TSharedRef<FJsonObject> FBlueprintHelperActionContextRevisionDebugFacts::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("blueprint_canonical"), BlueprintCanonical);
	Json->SetStringField(TEXT("graph_canonical"), GraphCanonical);
	return Json;
}

FBlueprintHelperActionContextRevisionToken FBlueprintHelperActionContextRevisionService::BuildRevisionToken(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FString& TaskRunId,
	const FString& PlanHash)
{
	FBlueprintHelperActionContextRevisionToken Revision;
	Revision.AssetPath = Blueprint ? Blueprint->GetPathName() : FString();
	Revision.GraphName = Graph ? Graph->GetName() : FString();
	Revision.TaskRunId = TaskRunId;
	Revision.PlanHash = PlanHash;
	Revision.BlueprintRevision = BuildBlueprintRevision(Blueprint);
	Revision.GraphRevision = BuildGraphRevision(Graph);
	return Revision;
}

int32 FBlueprintHelperActionContextRevisionService::BuildBlueprintRevision(UBlueprint* Blueprint)
{
	return FBlueprintHelperActionContextRevisionServiceLocalUtils::HashCanonical(
		FBlueprintHelperActionContextRevisionServiceLocalUtils::BuildBlueprintCanonical(Blueprint));
}

int32 FBlueprintHelperActionContextRevisionService::BuildGraphRevision(UEdGraph* Graph)
{
	return FBlueprintHelperActionContextRevisionServiceLocalUtils::HashCanonical(
		FBlueprintHelperActionContextRevisionServiceLocalUtils::BuildGraphCanonical(Graph));
}

FBlueprintHelperActionContextRevisionDebugFacts FBlueprintHelperActionContextRevisionService::BuildDebugFacts(
	UBlueprint* Blueprint,
	UEdGraph* Graph)
{
	FBlueprintHelperActionContextRevisionDebugFacts Facts;
	Facts.BlueprintCanonical = FBlueprintHelperActionContextRevisionServiceLocalUtils::BuildBlueprintCanonical(Blueprint);
	Facts.GraphCanonical = FBlueprintHelperActionContextRevisionServiceLocalUtils::BuildGraphCanonical(Graph);
	return Facts;
}
