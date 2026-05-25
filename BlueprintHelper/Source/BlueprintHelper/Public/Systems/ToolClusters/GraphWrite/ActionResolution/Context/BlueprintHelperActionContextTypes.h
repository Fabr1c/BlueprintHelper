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
	FString FunctionOperation;
	FString TransformOperation;
	FString ScheduleOperation;
	FString CreateOperation;
	FString ContainerKind;
	FString ContainerOperation;
	FString ClassPath;
	FString AssetPath;
	FString GraphLatentAllowed;
	FString ElementType;
	FString KeyType;
	FString ValueType;
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
	FBlueprintHelperCallFunctionPinType ContainerElementPinType;
	FBlueprintHelperCallFunctionPinType ContainerKeyPinType;
	FBlueprintHelperCallFunctionPinType ContainerValuePinType;
	FString ComponentPath;
	FString BindingObjectPath;
	FString DelegateName;
	FString DelegateOperation;
	FString DelegateSignature;
	FString HandlerName;
	FString HandlerFunctionPath;
	FString HandlerSourceCluster;
	FString SignatureEvidenceId;
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
	FString PinContainerType;
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
				if (Existing->Semantic.FunctionOperation.IsEmpty())
				{
					Existing->Semantic.FunctionOperation = MoveTemp(Context.Semantic.FunctionOperation);
				}
				if (Existing->Semantic.TransformOperation.IsEmpty())
				{
					Existing->Semantic.TransformOperation = MoveTemp(Context.Semantic.TransformOperation);
				}
				if (Existing->Semantic.ScheduleOperation.IsEmpty())
				{
					Existing->Semantic.ScheduleOperation = MoveTemp(Context.Semantic.ScheduleOperation);
				}
				if (Existing->Semantic.CreateOperation.IsEmpty())
				{
					Existing->Semantic.CreateOperation = MoveTemp(Context.Semantic.CreateOperation);
				}
				if (Existing->Semantic.ContainerKind.IsEmpty())
				{
					Existing->Semantic.ContainerKind = MoveTemp(Context.Semantic.ContainerKind);
				}
				if (Existing->Semantic.ContainerOperation.IsEmpty())
				{
					Existing->Semantic.ContainerOperation = MoveTemp(Context.Semantic.ContainerOperation);
				}
				if (Existing->Semantic.ClassPath.IsEmpty())
				{
					Existing->Semantic.ClassPath = MoveTemp(Context.Semantic.ClassPath);
				}
				if (Existing->Semantic.AssetPath.IsEmpty())
				{
					Existing->Semantic.AssetPath = MoveTemp(Context.Semantic.AssetPath);
				}
				if (Existing->Semantic.ElementType.IsEmpty())
				{
					Existing->Semantic.ElementType = MoveTemp(Context.Semantic.ElementType);
				}
				if (Existing->Semantic.KeyType.IsEmpty())
				{
					Existing->Semantic.KeyType = MoveTemp(Context.Semantic.KeyType);
				}
				if (Existing->Semantic.ValueType.IsEmpty())
				{
					Existing->Semantic.ValueType = MoveTemp(Context.Semantic.ValueType);
				}
				if (!Existing->Semantic.ContainerElementPinType.IsValid())
				{
					Existing->Semantic.ContainerElementPinType = MoveTemp(Context.Semantic.ContainerElementPinType);
				}
				if (!Existing->Semantic.ContainerKeyPinType.IsValid())
				{
					Existing->Semantic.ContainerKeyPinType = MoveTemp(Context.Semantic.ContainerKeyPinType);
				}
				if (!Existing->Semantic.ContainerValuePinType.IsValid())
				{
					Existing->Semantic.ContainerValuePinType = MoveTemp(Context.Semantic.ContainerValuePinType);
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
