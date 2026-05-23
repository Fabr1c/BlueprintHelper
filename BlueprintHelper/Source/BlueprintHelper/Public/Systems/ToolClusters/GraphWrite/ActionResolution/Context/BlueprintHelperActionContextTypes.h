#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

enum class EBlueprintHelperActionContextDemandKind : uint8
{
	Graph,
	TypedPins,
	Target,
	SearchPolicy,
	Binding,
	SpawnerEvidence
};

enum class EBlueprintHelperActionContextSourceThread : uint8
{
	GameThreadSnapshot,
	WorkerInference
};

struct FBlueprintHelperActionContextRevisionToken
{
	FString AssetPath;
	FString GraphName;
	FString TaskRunId;
	FString PlanHash;
	int32 BlueprintRevision = 0;
	int32 GraphRevision = 0;

	bool IsCompatibleWith(const FBlueprintHelperActionContextRevisionToken& Other) const
	{
		return AssetPath == Other.AssetPath
			&& GraphName == Other.GraphName
			&& TaskRunId == Other.TaskRunId
			&& PlanHash == Other.PlanHash
			&& BlueprintRevision == Other.BlueprintRevision
			&& GraphRevision == Other.GraphRevision;
	}
};

struct FBlueprintHelperActionContextDemand
{
	FString StatementId;
	FString SourcePath;
	EBlueprintHelperSpawnerClusterKind ClusterKind = EBlueprintHelperSpawnerClusterKind::Unknown;
	EBlueprintHelperActionSemanticKind SemanticKind = EBlueprintHelperActionSemanticKind::Unknown;
	EBlueprintHelperActionSemanticFamily SemanticFamily = EBlueprintHelperActionSemanticFamily::Unknown;
	EBlueprintHelperTypeOperation TypeOperation = EBlueprintHelperTypeOperation::None;
	TSet<EBlueprintHelperActionContextDemandKind> RequiredKinds;
	FString Query;
	FString TargetPath;
	FString PropertyPath;
	FString FieldOperation;
	FString FieldScope;
	FString TypeName;
	FString StructPath;
	FString TypeStructureId;
	FString SearchMode;
	FString AmbiguityPolicy;
	TArray<FString> CategoryPriority;
	TMap<FString, FString> DefaultValues;
	TMap<FString, FString> ArgumentTypes;
	TMap<FString, FBlueprintHelperCallFunctionPinType> ArgumentPinTypes;
	FString TargetObjectType;
	FBlueprintHelperCallFunctionPinType TargetObjectPinType;
	FString ExpectedReturnType;
	FBlueprintHelperCallFunctionPinType ExpectedReturnPinType;
	FString ComponentPath;
	FString BindingObjectPath;
	FString DelegateName;
	FString DelegateOperation;
	FString DelegateSignature;
	FString HandlerName;
	FString UnbindMode;
	FString TargetGraphName;
	TArray<FString> SelectedObjectPaths;
	TArray<FString> ArgumentNames;
	TArray<FString> SourceSymbolIds;
	TArray<FString> ConsumerSymbolIds;
};

struct FBlueprintHelperActionContextGraphSnapshot
{
	FString AssetPath;
	FString BlueprintClassPath;
	FString GraphName;
	FString GraphType;
	FString SchemaClassPath;
	FString FunctionName;
	bool bImpureAllowed = false;
	bool bLatentAllowed = false;
};

struct FBlueprintHelperActionContextFieldSnapshot
{
	FString Name;
	FString OwnerClassPath;
	FString FieldPath;
	FString PinCategory;
	FString PinSubCategory;
	FString PinSubCategoryObjectPath;
	bool bReadable = true;
	bool bWritable = true;
	bool bComponent = false;
	bool bMulticastDelegate = false;
	bool bBlueprintAssignable = false;
	bool bBlueprintCallable = false;
	FString DelegateSignatureFunctionPath;
};

struct FBlueprintHelperActionContextSnapshot
{
	FBlueprintHelperActionContextRevisionToken Revision;
	FBlueprintHelperActionContextGraphSnapshot Graph;
	TArray<FBlueprintHelperActionContextFieldSnapshot> Fields;
	TMap<FString, FBlueprintHelperCallFunctionPinType> SymbolPinTypes;
};

struct FBlueprintHelperResolvedActionContext
{
	FString StatementId;
	EBlueprintHelperSpawnerClusterKind ClusterKind = EBlueprintHelperSpawnerClusterKind::Unknown;
	FBlueprintHelperActionSemanticConstraints Semantic;
	FString GraphName;
	EBlueprintHelperActionContextSourceThread SourceThread = EBlueprintHelperActionContextSourceThread::WorkerInference;
	TMap<FString, FString> Evidence;
};

struct FBlueprintHelperResolvedActionContextBundle
{
	FBlueprintHelperActionContextRevisionToken Revision;
	TArray<FBlueprintHelperResolvedActionContext> Contexts;

	const FBlueprintHelperResolvedActionContext* FindByStatementId(const FString& StatementId) const
	{
		return Contexts.FindByPredicate(
			[&StatementId](const FBlueprintHelperResolvedActionContext& Context)
			{
				return Context.StatementId == StatementId;
			});
	}

	FBlueprintHelperResolvedActionContext* FindMutableByStatementId(const FString& StatementId)
	{
		return Contexts.FindByPredicate(
			[&StatementId](const FBlueprintHelperResolvedActionContext& Context)
			{
				return Context.StatementId == StatementId;
			});
	}

	void AddOrMerge(FBlueprintHelperResolvedActionContext&& Context)
	{
		if (FBlueprintHelperResolvedActionContext* Existing = FindMutableByStatementId(Context.StatementId))
		{
			if (Existing->ClusterKind == EBlueprintHelperSpawnerClusterKind::Unknown)
			{
				Existing->ClusterKind = Context.ClusterKind;
			}

			if (Existing->Semantic.Kind == EBlueprintHelperActionSemanticKind::Unknown)
			{
				Existing->Semantic = MoveTemp(Context.Semantic);
			}
			else
			{
				if (Existing->Semantic.SemanticFamily == EBlueprintHelperActionSemanticFamily::Unknown)
				{
					Existing->Semantic.SemanticFamily = Context.Semantic.SemanticFamily;
				}
				if (Existing->Semantic.TypeOperation == EBlueprintHelperTypeOperation::None)
				{
					Existing->Semantic.TypeOperation = Context.Semantic.TypeOperation;
				}
				if (Existing->Semantic.StructPath.IsEmpty())
				{
					Existing->Semantic.StructPath = MoveTemp(Context.Semantic.StructPath);
				}
				if (Existing->Semantic.TypeStructureId.IsEmpty())
				{
					Existing->Semantic.TypeStructureId = MoveTemp(Context.Semantic.TypeStructureId);
				}
			}

			if (Existing->GraphName.IsEmpty())
			{
				Existing->GraphName = MoveTemp(Context.GraphName);
			}

			for (TPair<FString, FString>& EvidencePair : Context.Evidence)
			{
				Existing->Evidence.FindOrAdd(EvidencePair.Key, EvidencePair.Value);
			}
			return;
		}

		Contexts.Add(MoveTemp(Context));
	}

	void MergeFrom(const FBlueprintHelperResolvedActionContextBundle& Other)
	{
		for (const FBlueprintHelperResolvedActionContext& Context : Other.Contexts)
		{
			FBlueprintHelperResolvedActionContext Copy = Context;
			AddOrMerge(MoveTemp(Copy));
		}
	}
};
