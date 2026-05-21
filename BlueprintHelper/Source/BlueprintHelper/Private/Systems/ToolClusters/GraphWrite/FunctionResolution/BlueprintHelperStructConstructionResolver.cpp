#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperStructConstructionResolver.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_MakeStruct.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphNodeFactory.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

namespace
{
static FString NormalizeNativeFunctionPath(const FString& RawPath)
{
	FString Path = RawPath.TrimStartAndEnd();
	if (Path.IsEmpty() || Path.Contains(TEXT(":")))
	{
		return Path;
	}

	int32 LastDotIndex = INDEX_NONE;
	if (Path.FindLastChar(TEXT('.'), LastDotIndex) && LastDotIndex > 0 && LastDotIndex < Path.Len() - 1)
	{
		Path = Path.Left(LastDotIndex) + TEXT(":") + Path.Mid(LastDotIndex + 1);
	}
	return Path;
}

static FString SemanticTypeFromProperty(const FProperty* Property)
{
	if (!Property)
	{
		return FString();
	}
	if (Property->IsA<FBoolProperty>())
	{
		return TEXT("bool");
	}
	if (Property->IsA<FIntProperty>() || Property->IsA<FUInt32Property>() || Property->IsA<FByteProperty>())
	{
		return TEXT("int");
	}
	if (Property->IsA<FInt64Property>())
	{
		return TEXT("int64");
	}
	if (Property->IsA<FFloatProperty>())
	{
		return TEXT("float");
	}
	if (Property->IsA<FDoubleProperty>())
	{
		return TEXT("double");
	}
	if (Property->IsA<FStrProperty>())
	{
		return TEXT("string");
	}
	if (Property->IsA<FNameProperty>())
	{
		return TEXT("name");
	}
	if (Property->IsA<FTextProperty>())
	{
		return TEXT("text");
	}
	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		return StructProperty->Struct ? StructProperty->Struct->GetPathName() : FString();
	}
	if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
	{
		return ObjectProperty->PropertyClass ? ObjectProperty->PropertyClass->GetPathName() : FString();
	}
	if (const FClassProperty* ClassProperty = CastField<FClassProperty>(Property))
	{
		return ClassProperty->MetaClass ? ClassProperty->MetaClass->GetPathName() : FString(TEXT("class"));
	}
	return FString();
}

static void FillArgumentConstraintsFromStruct(
	const UScriptStruct* TargetStruct,
	const TMap<FString, FString>& Defaults,
	FBlueprintHelperCallFunctionResolveRequest& Request)
{
	if (!TargetStruct)
	{
		return;
	}

	for (const TPair<FString, FString>& Pair : Defaults)
	{
		Request.ArgumentNames.AddUnique(Pair.Key);
		const FProperty* Property = FindFProperty<FProperty>(TargetStruct, *Pair.Key);
		if (!Property)
		{
			for (TFieldIterator<FProperty> It(TargetStruct); It; ++It)
			{
				if (It->GetName().Equals(Pair.Key, ESearchCase::IgnoreCase)
					|| It->GetDisplayNameText().ToString().Equals(Pair.Key, ESearchCase::IgnoreCase))
				{
					Property = *It;
					break;
				}
			}
		}

		const FString SemanticType = SemanticTypeFromProperty(Property);
		if (!SemanticType.IsEmpty())
		{
			Request.ArgumentTypes.Add(Pair.Key, SemanticType);
		}
	}
}

static void PopulateContext(FBlueprintHelperCallFunctionResolveRequest& Request, UEdGraph* TargetGraph)
{
	Request.Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	Request.Graph = TargetGraph;
	Request.Context.Blueprint = Request.Blueprint;
	Request.Context.Graph = TargetGraph;
	Request.Context.Schema = TargetGraph ? TargetGraph->GetSchema() : nullptr;
	Request.Context.SelfClass = Request.Blueprint
		? (Request.Blueprint->GeneratedClass ? Request.Blueprint->GeneratedClass.Get() : Request.Blueprint->SkeletonGeneratedClass.Get())
		: nullptr;
	Request.Context.GraphKind = TargetGraph && TargetGraph->GetClass() ? TargetGraph->GetClass()->GetName() : FString();
	Request.Context.ArgumentNames = Request.ArgumentNames;
	Request.Context.ArgumentTypes = Request.ArgumentTypes;
	Request.Context.ArgumentPinTypes = Request.ArgumentPinTypes;
	Request.Context.ExpectedReturnType = Request.ExpectedReturnType;
	Request.Context.ExpectedReturnPinType = Request.ExpectedReturnPinType;
}

static FBlueprintHelperCallFunctionResolveRequest MakeBaseRequest(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	UScriptStruct* TargetStruct)
{
	FBlueprintHelperCallFunctionResolveRequest Request;
	Request.SearchMode = NodeData.SearchMode.IsEmpty() ? FString(TEXT("ue_search")) : NodeData.SearchMode;
	Request.AmbiguityPolicy = TEXT("pick_best");
	Request.CategoryPriority = NodeData.CategoryPriority;
	Request.ExpectedReturnType = TargetStruct ? TargetStruct->GetPathName() : NodeData.StructReference.StructPath;
	Request.ExpectedReturnPinType = NodeData.ExpectedReturnPinType;
	Request.ArgumentTypes = NodeData.ArgumentTypes;
	Request.ArgumentPinTypes = NodeData.ArgumentPinTypes;
	NodeData.ArgumentTypes.GetKeys(Request.ArgumentNames);
	FillArgumentConstraintsFromStruct(TargetStruct, NodeData.DefaultValues, Request);
	PopulateContext(Request, TargetGraph);
	return Request;
}

static UK2Node* SpawnResolved(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	const FBlueprintHelperCallFunctionResolveResult& ResolveResult,
	FString& OutError)
{
	if (!ResolveResult.IsResolved())
	{
		OutError = ResolveResult.Message;
		return nullptr;
	}

	UK2Node* Node = FBlueprintHelperCallFunctionResolver::SpawnResolvedNode(
		TargetGraph,
		ResolveResult.Selected,
		FVector2D(NodeData.X, NodeData.Y),
		OutError);
	if (!Node)
	{
		return nullptr;
	}

	FBlueprintGraphWriteFacade::ApplyDefaultValues(Node, NodeData.DefaultValues, NodeData.Id);
	return Node;
}

static void AddUniqueQuery(TArray<FString>& Queries, const FString& Query)
{
	if (!Query.IsEmpty() && !Queries.Contains(Query))
	{
		Queries.Add(Query);
	}
}
}

UScriptStruct* FBlueprintHelperStructConstructionResolver::ResolveStructType(const FString& TypeName)
{
	const FString Query = TypeName.TrimStartAndEnd();
	if (Query.IsEmpty())
	{
		return nullptr;
	}

	if (UScriptStruct* DirectStruct = FindObject<UScriptStruct>(nullptr, *Query))
	{
		return DirectStruct;
	}
	if (UScriptStruct* LoadedStruct = LoadObject<UScriptStruct>(nullptr, *Query))
	{
		return LoadedStruct;
	}

	FString NormalizedQuery = Query;
	NormalizedQuery.TrimStartAndEndInline();
	NormalizedQuery.ToLowerInline();
	NormalizedQuery.ReplaceInline(TEXT(" "), TEXT(""));
	if (NormalizedQuery.StartsWith(TEXT("struct ")))
	{
		NormalizedQuery.RightChopInline(7);
	}
	if (NormalizedQuery.StartsWith(TEXT("f")))
	{
		NormalizedQuery.RightChopInline(1);
	}

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Candidate = *It;
		if (!Candidate)
		{
			continue;
		}

		FString CandidateName = Candidate->GetName();
		CandidateName.ToLowerInline();
		FString CandidateCppName = Candidate->GetPrefixCPP() + Candidate->GetName();
		CandidateCppName.ToLowerInline();
		FString CandidatePath = Candidate->GetPathName();
		CandidatePath.ToLowerInline();
		if (CandidateName == NormalizedQuery
			|| CandidateCppName == NormalizedQuery
			|| CandidatePath == NormalizedQuery
			|| CandidatePath.EndsWith(TEXT(".") + NormalizedQuery))
		{
			return Candidate;
		}
	}

	return nullptr;
}

void FBlueprintHelperStructConstructionResolver::CollectStructFieldTypes(
	const UScriptStruct* TargetStruct,
	TMap<FString, FString>& OutFieldTypes)
{
	OutFieldTypes.Reset();
	if (!TargetStruct)
	{
		return;
	}

	for (TFieldIterator<FProperty> It(TargetStruct); It; ++It)
	{
		const FProperty* Property = *It;
		if (!Property)
		{
			continue;
		}

		OutFieldTypes.Add(Property->GetName(), SemanticTypeFromProperty(Property));
		const FString DisplayName = Property->GetDisplayNameText().ToString();
		if (!DisplayName.IsEmpty() && !OutFieldTypes.Contains(DisplayName))
		{
			OutFieldTypes.Add(DisplayName, SemanticTypeFromProperty(Property));
		}
	}
}

UK2Node* FBlueprintHelperStructConstructionResolver::SpawnMakeStructNode(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	UScriptStruct* TargetStruct,
	FString& OutError)
{
	if (!TargetGraph || !TargetStruct)
	{
		OutError = TEXT("struct construction resolve failed: graph or struct is invalid.");
		return nullptr;
	}

	if (UK2Node* NativeNode = SpawnNativeMakeFunctionNode(TargetGraph, NodeData, TargetStruct, OutError))
	{
		return NativeNode;
	}

	FString NativeError = OutError;
	if (UK2Node* SearchNode = SpawnSearchResolvedMakeFunctionNode(TargetGraph, NodeData, TargetStruct, OutError))
	{
		return SearchNode;
	}

	FString SearchError = OutError;
	if (UK2Node* DirectNode = SpawnDirectMakeStructNode(TargetGraph, NodeData, TargetStruct, OutError))
	{
		return DirectNode;
	}

	if (!NativeError.IsEmpty())
	{
		OutError = FString::Printf(TEXT("%s; %s; %s"), *NativeError, *SearchError, *OutError);
	}
	return nullptr;
}

UK2Node* FBlueprintHelperStructConstructionResolver::SpawnBreakStructNode(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	UScriptStruct* TargetStruct,
	FString& OutError)
{
	if (!TargetGraph || !TargetStruct)
	{
		OutError = TEXT("struct deconstruction resolve failed: graph or struct is invalid.");
		return nullptr;
	}

	UK2Node_BreakStruct* Node = FBlueprintHelperGraphNodeFactory::SpawnK2Node<UK2Node_BreakStruct>(
		TargetGraph,
		FVector2D(NodeData.X, NodeData.Y),
		[TargetStruct](UK2Node_BreakStruct* BreakNode)
		{
			BreakNode->StructType = TargetStruct;
		});
	if (!Node)
	{
		OutError = FString::Printf(TEXT("struct deconstruction spawn failed: %s."), *TargetStruct->GetPathName());
		return nullptr;
	}

	return Node;
}

UK2Node* FBlueprintHelperStructConstructionResolver::SpawnDirectMakeStructNode(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	UScriptStruct* TargetStruct,
	FString& OutError)
{
	UK2Node_MakeStruct* Node = FBlueprintHelperGraphNodeFactory::SpawnK2Node<UK2Node_MakeStruct>(
		TargetGraph,
		FVector2D(NodeData.X, NodeData.Y),
		[TargetStruct](UK2Node_MakeStruct* MakeNode)
		{
			MakeNode->StructType = TargetStruct;
		});
	if (!Node)
	{
		OutError = FString::Printf(TEXT("struct construction direct make spawn failed: %s."), *TargetStruct->GetPathName());
		return nullptr;
	}

	FBlueprintGraphWriteFacade::ApplyDefaultValues(Node, NodeData.DefaultValues, NodeData.Id);
	return Node;
}

UK2Node* FBlueprintHelperStructConstructionResolver::SpawnNativeMakeFunctionNode(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	UScriptStruct* TargetStruct,
	FString& OutError)
{
	const FString NativeMake = TargetStruct ? TargetStruct->GetMetaData(TEXT("HasNativeMake")) : FString();
	if (NativeMake.IsEmpty())
	{
		OutError = TEXT("struct construction native make skipped: struct has no HasNativeMake metadata.");
		return nullptr;
	}

	FBlueprintHelperCallFunctionResolveRequest Request = MakeBaseRequest(TargetGraph, NodeData, TargetStruct);
	Request.Query = NormalizeNativeFunctionPath(NativeMake);
	Request.SearchMode = TEXT("exact");

	const FBlueprintHelperCallFunctionResolveResult ResolveResult = FBlueprintHelperCallFunctionResolver::Resolve(Request);
	return SpawnResolved(TargetGraph, NodeData, ResolveResult, OutError);
}

UK2Node* FBlueprintHelperStructConstructionResolver::SpawnSearchResolvedMakeFunctionNode(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	UScriptStruct* TargetStruct,
	FString& OutError)
{
	TArray<FString> Queries;
	const FString DisplayName = TargetStruct ? TargetStruct->GetDisplayNameText().ToString() : FString();
	const FString StructName = TargetStruct ? TargetStruct->GetName() : FString();
	AddUniqueQuery(Queries, FString::Printf(TEXT("Make %s"), *DisplayName));
	AddUniqueQuery(Queries, FString::Printf(TEXT("Make %s"), *StructName));
	AddUniqueQuery(Queries, DisplayName);
	AddUniqueQuery(Queries, StructName);

	for (const FString& Query : Queries)
	{
		FBlueprintHelperCallFunctionResolveRequest Request = MakeBaseRequest(TargetGraph, NodeData, TargetStruct);
		Request.Query = Query;
		const FBlueprintHelperCallFunctionResolveResult ResolveResult = FBlueprintHelperCallFunctionResolver::Resolve(Request);
		if (UK2Node* Node = SpawnResolved(TargetGraph, NodeData, ResolveResult, OutError))
		{
			return Node;
		}
	}

	OutError = FString::Printf(
		TEXT("struct construction resolve failed: no callable make function returned %s."),
		TargetStruct ? *TargetStruct->GetPathName() : TEXT("<null>"));
	return nullptr;
}
