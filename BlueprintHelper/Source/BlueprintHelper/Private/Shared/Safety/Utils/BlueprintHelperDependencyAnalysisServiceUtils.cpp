// BlueprintHelper dependency analysis service utilities.

#include "Shared/Safety/Utils/BlueprintHelperDependencyAnalysisServiceUtils.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ClearDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_RemoveDelegate.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"

FString FBlueprintHelperDependencyAnalysisServiceUtils::NormalizeLegacyScope(
	const FString& Scope)
{
	const FString LowerScope =
		Scope.IsEmpty() ? TEXT("safety_context") : Scope.ToLower();
	if (LowerScope == TEXT("dependencies") || LowerScope == TEXT("referencers") ||
		LowerScope == TEXT("external_dependents") || LowerScope == TEXT("all"))
	{
		return LowerScope;
	}
	return TEXT("safety_context");
}

FString FBlueprintHelperDependencyAnalysisServiceUtils::NormalizeSearchScope(
	const FString& SearchScope)
{
	const FString LowerSearchScope =
		SearchScope.IsEmpty() ? TEXT("project") : SearchScope.ToLower();
	return LowerSearchScope == TEXT("asset") ? TEXT("asset") : TEXT("project");
}

FString
FBlueprintHelperDependencyAnalysisServiceUtils::NormalizeResolutionPolicy(
	const FString& ResolutionPolicy)
{
	const FString LowerPolicy = ResolutionPolicy.IsEmpty()
									? TEXT("ue_then_name")
									: ResolutionPolicy.ToLower();
	if (LowerPolicy == TEXT("ue_only") || LowerPolicy == TEXT("name_only"))
	{
		return LowerPolicy;
	}
	return TEXT("ue_then_name");
}

FString FBlueprintHelperDependencyAnalysisServiceUtils::NormalizeDetail(
	const FString& Detail)
{
	const FString LowerDetail =
		Detail.IsEmpty() ? TEXT("samples") : Detail.ToLower();
	if (LowerDetail == TEXT("summary") || LowerDetail == TEXT("full"))
	{
		return LowerDetail;
	}
	return TEXT("samples");
}

FString FBlueprintHelperDependencyAnalysisServiceUtils::NormalizeTargetType(
	const FString& TargetType)
{
	const FString LowerType =
		TargetType.IsEmpty() ? TEXT("asset") : TargetType.ToLower();
	if (LowerType == TEXT("blueprint"))
	{
		return TEXT("asset");
	}
	return LowerType;
}

bool FBlueprintHelperDependencyAnalysisServiceUtils::ShouldReadDependencies(
	const FString& Scope)
{
	return Scope == TEXT("safety_context") || Scope == TEXT("dependencies") ||
		   Scope == TEXT("all");
}

bool FBlueprintHelperDependencyAnalysisServiceUtils::ShouldReadReferencers(
	const FString& Scope)
{
	return Scope == TEXT("safety_context") || Scope == TEXT("referencers") ||
		   Scope == TEXT("all");
}

bool FBlueprintHelperDependencyAnalysisServiceUtils::IsMemberTarget(
	const FString& TargetType)
{
	return TargetType == TEXT("function") ||
		   TargetType == TEXT("member_variable") ||
		   TargetType == TEXT("local_variable") || TargetType == TEXT("event") ||
		   TargetType == TEXT("custom_event") ||
		   TargetType == TEXT("event_dispatcher");
}

int32 FBlueprintHelperDependencyAnalysisServiceUtils::ClampMaxResults(
	int32 MaxResults)
{
	if (MaxResults <= 0)
	{
		return DefaultMaxResults;
	}
	return FMath::Clamp(MaxResults, 1, MaxAllowedResults);
}

bool FBlueprintHelperDependencyAnalysisServiceUtils::ShouldIncludeSamples(
	const FString& Detail)
{
	return Detail != TEXT("summary");
}

FString
FBlueprintHelperDependencyAnalysisServiceUtils::PackageNameFromAssetPath(
	const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return FString();
	}
	if (FPackageName::IsValidLongPackageName(AssetPath))
	{
		return AssetPath;
	}
	if (FPackageName::IsValidObjectPath(AssetPath))
	{
		return FPackageName::ObjectPathToPackageName(AssetPath);
	}
	return AssetPath.Contains(TEXT("."))
			   ? FPackageName::ObjectPathToPackageName(AssetPath)
			   : AssetPath;
}

FString
FBlueprintHelperDependencyAnalysisServiceUtils::ObjectPathFromPackageName(
	const FString& PackageName)
{
	if (PackageName.IsEmpty())
	{
		return FString();
	}
	return PackageName + TEXT(".") +
		   FPackageName::GetLongPackageAssetName(PackageName);
}

FString
FBlueprintHelperDependencyAnalysisServiceUtils::AssetPathFromDataOrPackage(
	const FAssetData& AssetData, const FName& PackageName)
{
	if (AssetData.IsValid())
	{
		return AssetData.GetObjectPathString();
	}
	return PackageName.ToString();
}

FString FBlueprintHelperDependencyAnalysisServiceUtils::AssetTypeFromData(
	const FAssetData& AssetData)
{
	if (!AssetData.IsValid())
	{
		return TEXT("unknown");
	}
	const FName AssetClassName = AssetData.AssetClassPath.GetAssetName();
	return AssetClassName.IsNone() ? AssetData.AssetClassPath.ToString()
								   : AssetClassName.ToString();
}

bool FBlueprintHelperDependencyAnalysisServiceUtils::TryFindAssetData(
	IAssetRegistry& Registry, const FName PackageName,
	FAssetData& OutAssetData)
{
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPackageName(PackageName, Assets, false);
	if (Assets.Num() > 0)
	{
		OutAssetData = Assets[0];
		return true;
	}
	return false;
}

bool FBlueprintHelperDependencyAnalysisServiceUtils::TryResolveTargetAsset(
	IAssetRegistry& Registry, const FString& AssetPath, FName& OutPackageName,
	FAssetData& OutAssetData, FString& OutErrorCode, FString& OutErrorMessage)
{
	if (AssetPath.IsEmpty())
	{
		OutErrorCode = TEXT("invalid_request");
		OutErrorMessage = TEXT("asset_path is required.");
		return false;
	}

	const FString PackageName = PackageNameFromAssetPath(AssetPath);
	OutPackageName = FName(*PackageName);
	if (TryFindAssetData(Registry, OutPackageName, OutAssetData))
	{
		return true;
	}

	UObject* LoadedAsset =
		StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
	if (!LoadedAsset && FPackageName::IsValidLongPackageName(PackageName))
	{
		const FString ObjectPath = ObjectPathFromPackageName(PackageName);
		LoadedAsset =
			StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
	}
	if (LoadedAsset)
	{
		const FString LoadedPackageName = LoadedAsset->GetOutermost()->GetName();
		OutPackageName = FName(*LoadedPackageName);
		TryFindAssetData(Registry, OutPackageName, OutAssetData);
		return true;
	}

	OutErrorCode = TEXT("asset_not_found");
	OutErrorMessage =
		FString::Printf(TEXT("Target asset was not found: %s"), *AssetPath);
	return false;
}

UBlueprint*
FBlueprintHelperDependencyAnalysisServiceUtils::LoadBlueprintFromAssetData(
	const FAssetData& AssetData)
{
	return AssetData.IsValid() ? Cast<UBlueprint>(AssetData.GetAsset()) : nullptr;
}

UBlueprint*
FBlueprintHelperDependencyAnalysisServiceUtils::LoadBlueprintFromPath(
	const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return nullptr;
	}
	if (UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath))
	{
		return Blueprint;
	}
	const FString PackageName = PackageNameFromAssetPath(AssetPath);
	if (FPackageName::IsValidLongPackageName(PackageName))
	{
		const FString ObjectPath = ObjectPathFromPackageName(PackageName);
		return LoadObject<UBlueprint>(nullptr, *ObjectPath);
	}
	return nullptr;
}

UClass* FBlueprintHelperDependencyAnalysisServiceUtils::BlueprintReferenceClass(
	UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return nullptr;
	}
	return Blueprint->SkeletonGeneratedClass ? Blueprint->SkeletonGeneratedClass
											 : Blueprint->GeneratedClass;
}

UClass* FBlueprintHelperDependencyAnalysisServiceUtils::ResolveClassPath(
	const FString& ClassPath)
{
	if (ClassPath.IsEmpty())
	{
		return nullptr;
	}
	if (UClass* Class = LoadObject<UClass>(nullptr, *ClassPath))
	{
		return Class->GetAuthoritativeClass();
	}
	if (UBlueprint* Blueprint = LoadBlueprintFromPath(ClassPath))
	{
		return BlueprintReferenceClass(Blueprint);
	}
	return nullptr;
}

UClass* FBlueprintHelperDependencyAnalysisServiceUtils::ResolveTargetClass(
	const FBlueprintHelperDependencyAnalysisTarget& Target)
{
	if (UClass* DeclaringClass = ResolveClassPath(Target.DeclaringClassPath))
	{
		return DeclaringClass;
	}
	if (UBlueprint* Blueprint = LoadBlueprintFromPath(Target.AssetPath))
	{
		return BlueprintReferenceClass(Blueprint);
	}
	return nullptr;
}

bool FBlueprintHelperDependencyAnalysisServiceUtils::ClassesMatch(
	UClass* CandidateClass, UClass* TargetClass)
{
	if (!CandidateClass || !TargetClass)
	{
		return false;
	}
	UClass* Candidate = CandidateClass->GetAuthoritativeClass();
	UClass* Target = TargetClass->GetAuthoritativeClass();
	if (!Candidate || !Target)
	{
		return false;
	}
	return Candidate == Target || Candidate->IsChildOf(Target) ||
		   Target->IsChildOf(Candidate) ||
		   (Candidate->ClassGeneratedBy &&
			Candidate->ClassGeneratedBy == Target->ClassGeneratedBy);
}

bool FBlueprintHelperDependencyAnalysisServiceUtils::MatchesResolution(
	const FString& ResolutionPolicy, UClass* CandidateClass,
	UClass* TargetClass, bool& bOutUsedFallback)
{
	bOutUsedFallback = false;
	if (ResolutionPolicy == TEXT("name_only"))
	{
		bOutUsedFallback = true;
		return true;
	}
	if (TargetClass)
	{
		return ClassesMatch(CandidateClass, TargetClass);
	}
	if (ResolutionPolicy == TEXT("ue_only"))
	{
		return false;
	}
	bOutUsedFallback = true;
	return true;
}

void FBlueprintHelperDependencyAnalysisServiceUtils::AddUnsupportedCheck(
	TArray<FString>& UnsupportedChecks, const FString& Check)
{
	UnsupportedChecks.AddUnique(Check);
}

FBlueprintHelperReferenceAssetSummary&
FBlueprintHelperDependencyAnalysisServiceUtils::FindOrAddSummary(
	TMap<FString, FBlueprintHelperReferenceAssetSummary>& Summaries,
	const FString& AssetPath, const FString& AssetType)
{
	FBlueprintHelperReferenceAssetSummary& Summary =
		Summaries.FindOrAdd(AssetPath);
	if (Summary.AssetPath.IsEmpty())
	{
		Summary.AssetPath = AssetPath;
		Summary.AssetType = AssetType.IsEmpty() ? TEXT("unknown") : AssetType;
	}
	return Summary;
}

void FBlueprintHelperDependencyAnalysisServiceUtils::AddAggregateReference(
	TMap<FString, FBlueprintHelperReferenceAssetSummary>& Summaries,
	const FString& AssetPath, const FString& AssetType,
	const FString& ReferenceKind, const FString& GraphName,
	const FString& Safety, bool bIncludeSamples)
{
	FBlueprintHelperReferenceAssetSummary& Summary =
		FindOrAddSummary(Summaries, AssetPath, AssetType);
	Summary.AddReference(ReferenceKind, GraphName, Safety, bIncludeSamples,
						 MaxSamplesPerAsset);
}

FBlueprintHelperReferenceAssetSummary
FBlueprintHelperDependencyAnalysisServiceUtils::MakePackageSummary(
	IAssetRegistry& Registry, const FName PackageName,
	const FString& ReferenceKind, const FString& Safety)
{
	FAssetData AssetData;
	TryFindAssetData(Registry, PackageName, AssetData);

	FBlueprintHelperReferenceAssetSummary Summary;
	Summary.AssetPath = AssetPathFromDataOrPackage(AssetData, PackageName);
	Summary.AssetType = AssetTypeFromData(AssetData);
	Summary.AddReference(ReferenceKind, FString(), Safety, false, 0);
	return Summary;
}

void FBlueprintHelperDependencyAnalysisServiceUtils::AppendPackageRefs(
	IAssetRegistry& Registry, const TArray<FName>& PackageNames,
	const FString& ReferenceKind, const FString& Safety, int32 MaxResults,
	TArray<FBlueprintHelperReferenceAssetSummary>& OutSamples,
	bool& bOutTruncated)
{
	for (const FName PackageName : PackageNames)
	{
		if (OutSamples.Num() >= MaxResults)
		{
			bOutTruncated = true;
			continue;
		}

		OutSamples.Add(
			MakePackageSummary(Registry, PackageName, ReferenceKind, Safety));
	}
}

void FBlueprintHelperDependencyAnalysisServiceUtils::GatherBlueprintCandidates(
	IAssetRegistry& Registry, const FString& SearchScope,
	const FName TargetPackageName, const FAssetData& TargetAssetData,
	TArray<FAssetData>& OutCandidates)
{
	OutCandidates.Reset();
	if (SearchScope == TEXT("asset"))
	{
		if (TargetAssetData.IsValid())
		{
			OutCandidates.Add(TargetAssetData);
		}
		return;
	}

	Registry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(),
							  OutCandidates, true);
	OutCandidates.Sort([](const FAssetData& A, const FAssetData& B)
					   { return A.GetObjectPathString() < B.GetObjectPathString(); });

	if (TargetAssetData.IsValid() &&
		!OutCandidates.ContainsByPredicate(
			[TargetPackageName](const FAssetData& Candidate)
			{
				return Candidate.PackageName == TargetPackageName;
			}))
	{
		OutCandidates.Add(TargetAssetData);
	}
}

FString FBlueprintHelperDependencyAnalysisServiceUtils::DelegateReferenceKind(
	const UK2Node_BaseMCDelegate* DelegateNode)
{
	if (!DelegateNode)
	{
		return TEXT("unknown");
	}
	if (DelegateNode->IsA<UK2Node_AssignDelegate>())
	{
		return TEXT("dispatcher_assign");
	}
	if (DelegateNode->IsA<UK2Node_AddDelegate>() ||
		DelegateNode->IsA<UK2Node_RemoveDelegate>())
	{
		return DelegateNode->IsA<UK2Node_RemoveDelegate>()
				   ? TEXT("dispatcher_clear")
				   : TEXT("dispatcher_bind");
	}
	if (DelegateNode->IsA<UK2Node_ClearDelegate>())
	{
		return TEXT("dispatcher_clear");
	}
	if (DelegateNode->IsA<UK2Node_CallDelegate>())
	{
		return TEXT("dispatcher_broadcast");
	}
	return TEXT("unknown");
}

FString FBlueprintHelperDependencyAnalysisServiceUtils::EventReferenceKind(
	const FString& TargetType)
{
	return TargetType == TEXT("custom_event") ? TEXT("custom_event_reference")
											  : TEXT("event_reference");
}

FString FBlueprintHelperDependencyAnalysisServiceUtils::VariableReferenceKind(
	const FString& TargetType, const UK2Node_Variable* VariableNode)
{
	const bool bLocal = TargetType == TEXT("local_variable");
	if (VariableNode && VariableNode->IsA<UK2Node_VariableSet>())
	{
		return bLocal ? TEXT("local_variable_set") : TEXT("variable_set");
	}
	return bLocal ? TEXT("local_variable_get") : TEXT("variable_get");
}

bool FBlueprintHelperDependencyAnalysisServiceUtils::MatchesVariableScope(
	const FBlueprintHelperDependencyAnalysisTarget& Target,
	const UK2Node_Variable* VariableNode, const UEdGraph* Graph)
{
	if (!VariableNode)
	{
		return false;
	}
	if (NormalizeTargetType(Target.TargetType) != TEXT("local_variable"))
	{
		return !VariableNode->VariableReference.IsLocalScope();
	}

	if (!VariableNode->VariableReference.IsLocalScope())
	{
		return false;
	}
	const FString ScopeName =
		VariableNode->VariableReference.GetMemberScopeName();
	return ScopeName == Target.GraphName ||
		   (Graph && Graph->GetName() == Target.GraphName);
}

FName FBlueprintHelperDependencyAnalysisServiceUtils::ResolveBindingFunctionName(
	const FDelegateEditorBinding& Binding,
	const UWidgetBlueprint* WidgetBlueprint)
{
	if (Binding.MemberGuid.IsValid() && WidgetBlueprint &&
		WidgetBlueprint->SkeletonGeneratedClass)
	{
		const FName ResolvedName =
			UBlueprint::GetFieldNameFromClassByGuid<UFunction>(
				WidgetBlueprint->SkeletonGeneratedClass, Binding.MemberGuid);
		if (!ResolvedName.IsNone())
		{
			return ResolvedName;
		}
	}
	return Binding.FunctionName;
}

FName FBlueprintHelperDependencyAnalysisServiceUtils::ResolveBindingSourcePropertyName(
	const FDelegateEditorBinding& Binding,
	const UWidgetBlueprint* WidgetBlueprint)
{
	if (Binding.MemberGuid.IsValid() && WidgetBlueprint &&
		WidgetBlueprint->SkeletonGeneratedClass)
	{
		const FName ResolvedName =
			UBlueprint::GetFieldNameFromClassByGuid<FProperty>(
				WidgetBlueprint->SkeletonGeneratedClass, Binding.MemberGuid);
		if (!ResolvedName.IsNone())
		{
			return ResolvedName;
		}
	}
	return Binding.SourceProperty;
}

bool FBlueprintHelperDependencyAnalysisServiceUtils::BindingSourcePathContainsMember(
	const FDelegateEditorBinding& Binding,
	const FName TargetName)
{
	for (const FEditorPropertyPathSegment& Segment :
		 Binding.SourcePath.Segments)
	{
		if (Segment.GetMemberName() == TargetName)
		{
			return true;
		}
	}
	return false;
}

bool FBlueprintHelperDependencyAnalysisServiceUtils::BindingMatchesMemberVariable(
	const FDelegateEditorBinding& Binding,
	const UWidgetBlueprint* WidgetBlueprint,
	const FName TargetName)
{
	if (FName(*Binding.ObjectName) == TargetName)
	{
		return true;
	}
	if (Binding.Kind != EBindingKind::Property)
	{
		return false;
	}
	return ResolveBindingSourcePropertyName(Binding, WidgetBlueprint) ==
			   TargetName ||
		   Binding.SourceProperty == TargetName ||
		   BindingSourcePathContainsMember(Binding, TargetName);
}

void FBlueprintHelperDependencyAnalysisServiceUtils::ScanWidgetBlueprintBindingsForMemberReferences(
	const FBlueprintHelperDependencyAnalysisTarget& Target,
	const FString& TargetType, UWidgetBlueprint* WidgetBlueprint,
	const FString& AssetPath, const FString& AssetType,
	bool bIncludeSamples,
	TMap<FString, FBlueprintHelperReferenceAssetSummary>& OutReferencers)
{
	if (!WidgetBlueprint)
	{
		return;
	}

	const FName TargetName(*Target.TargetName);
	for (const FDelegateEditorBinding& Binding : WidgetBlueprint->Bindings)
	{
		if (TargetType == TEXT("member_variable") &&
			BindingMatchesMemberVariable(Binding, WidgetBlueprint, TargetName))
		{
			AddAggregateReference(OutReferencers, AssetPath, AssetType,
								  FName(*Binding.ObjectName) == TargetName
									  ? TEXT("widget_binding_target")
									  : TEXT("widget_property_binding"),
								  Binding.ObjectName, TEXT("blocking"),
								  bIncludeSamples);
			continue;
		}

		if ((TargetType == TEXT("function") || TargetType == TEXT("event") ||
			 TargetType == TEXT("custom_event")) &&
			Binding.Kind == EBindingKind::Function &&
			(ResolveBindingFunctionName(Binding, WidgetBlueprint) == TargetName ||
			 Binding.FunctionName == TargetName))
		{
			AddAggregateReference(OutReferencers, AssetPath, AssetType,
								  TEXT("widget_property_binding_function"),
								  Binding.ObjectName, TEXT("blocking"),
								  bIncludeSamples);
		}
	}
}

void FBlueprintHelperDependencyAnalysisServiceUtils::ScanBlueprintForMemberReferences(
	const FBlueprintHelperDependencyAnalysisTarget& Target,
	const FString& TargetType, const FString& ResolutionPolicy,
	UClass* TargetClass, UBlueprint* Blueprint, const FString& AssetPath,
	const FString& AssetType, bool bIncludeSamples,
	bool bAnalyzeWidgetBindings,
	TMap<FString, FBlueprintHelperReferenceAssetSummary>& OutReferencers,
	TArray<FString>& UnsupportedChecks, bool& bOutUsedFallback)
{
	if (!Blueprint)
	{
		return;
	}
	const FName TargetName(*Target.TargetName);
	UClass* BlueprintClass = BlueprintReferenceClass(Blueprint);
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);

	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			if ((TargetType == TEXT("function") ||
				 TargetType == TEXT("custom_event") || TargetType == TEXT("event")) &&
				Node->IsA<UK2Node_CallFunction>())
			{
				const UK2Node_CallFunction* CallNode =
					CastChecked<UK2Node_CallFunction>(Node);
				if (CallNode->FunctionReference.GetMemberName() != TargetName)
				{
					continue;
				}

				bool bUsedFallback = false;
				UClass* CandidateClass =
					CallNode->FunctionReference.GetMemberParentClass(BlueprintClass);
				if (!MatchesResolution(ResolutionPolicy, CandidateClass, TargetClass,
									   bUsedFallback))
				{
					continue;
				}

				bOutUsedFallback = bOutUsedFallback || bUsedFallback;
				AddAggregateReference(
					OutReferencers, AssetPath, AssetType,
					TargetType == TEXT("function") ? TEXT("function_call")
												   : EventReferenceKind(TargetType),
					Graph->GetName(),
					bUsedFallback ? TEXT("warning") : TEXT("blocking"),
					bIncludeSamples);
				continue;
			}

			if ((TargetType == TEXT("event") || TargetType == TEXT("custom_event")) &&
				Node->IsA<UK2Node_Event>())
			{
				const UK2Node_Event* EventNode = CastChecked<UK2Node_Event>(Node);
				const FName EventName =
					EventNode->IsA<UK2Node_CustomEvent>()
						? CastChecked<UK2Node_CustomEvent>(EventNode)
							  ->CustomFunctionName
						: EventNode->GetFunctionName();
				if (EventName != TargetName)
				{
					continue;
				}

				bool bUsedFallback = false;
				UClass* CandidateClass =
					EventNode->EventReference.GetMemberParentClass(BlueprintClass);
				if (!MatchesResolution(ResolutionPolicy, CandidateClass, TargetClass,
									   bUsedFallback))
				{
					continue;
				}

				bOutUsedFallback = bOutUsedFallback || bUsedFallback;
				AddAggregateReference(OutReferencers, AssetPath, AssetType,
									  TargetType == TEXT("custom_event")
										  ? TEXT("custom_event_declaration")
										  : TEXT("event_declaration"),
									  Graph->GetName(),
									  bUsedFallback ? TEXT("warning") : TEXT("info"),
									  bIncludeSamples);
				continue;
			}

			if ((TargetType == TEXT("member_variable") ||
				 TargetType == TEXT("local_variable")) &&
				Node->IsA<UK2Node_Variable>())
			{
				const UK2Node_Variable* VariableNode =
					CastChecked<UK2Node_Variable>(Node);
				if (VariableNode->GetVarName() != TargetName ||
					!MatchesVariableScope(Target, VariableNode, Graph))
				{
					continue;
				}

				bool bUsedFallback = false;
				UClass* CandidateClass =
					VariableNode->VariableReference.GetMemberParentClass(
						BlueprintClass);
				if (!MatchesResolution(ResolutionPolicy, CandidateClass, TargetClass,
									   bUsedFallback))
				{
					continue;
				}

				bOutUsedFallback = bOutUsedFallback || bUsedFallback;
				AddAggregateReference(
					OutReferencers, AssetPath, AssetType,
					VariableReferenceKind(TargetType, VariableNode), Graph->GetName(),
					bUsedFallback ? TEXT("warning") : TEXT("blocking"),
					bIncludeSamples);
				continue;
			}

			if (TargetType == TEXT("event_dispatcher") &&
				Node->IsA<UK2Node_BaseMCDelegate>())
			{
				const UK2Node_BaseMCDelegate* DelegateNode =
					CastChecked<UK2Node_BaseMCDelegate>(Node);
				if (DelegateNode->GetPropertyName() != TargetName)
				{
					continue;
				}

				bool bUsedFallback = false;
				FProperty* DelegateProperty = DelegateNode->GetProperty();
				UClass* CandidateClass =
					DelegateProperty ? DelegateProperty->GetOwnerClass() : nullptr;
				if (!MatchesResolution(ResolutionPolicy, CandidateClass, TargetClass,
									   bUsedFallback))
				{
					continue;
				}

				bOutUsedFallback = bOutUsedFallback || bUsedFallback;
				AddAggregateReference(
					OutReferencers, AssetPath, AssetType,
					DelegateReferenceKind(DelegateNode), Graph->GetName(),
					bUsedFallback ? TEXT("warning") : TEXT("blocking"),
					bIncludeSamples);
			}
		}
	}

	if (UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Blueprint))
	{
		if (bAnalyzeWidgetBindings)
		{
			ScanWidgetBlueprintBindingsForMemberReferences(
				Target, TargetType, WidgetBlueprint, AssetPath, AssetType,
				bIncludeSamples, OutReferencers);
		}
		else if (TargetType == TEXT("member_variable") ||
				 TargetType == TEXT("function"))
		{
			AddUnsupportedCheck(UnsupportedChecks,
								TEXT("widget_property_binding_scan_disabled"));
		}
	}
}

void FBlueprintHelperDependencyAnalysisServiceUtils::EmitAggregatedSummaries(
	const TMap<FString, FBlueprintHelperReferenceAssetSummary>& Summaries,
	int32 MaxResults,
	TArray<FBlueprintHelperReferenceAssetSummary>& OutSummaries,
	bool& bOutTruncated)
{
	TArray<FString> Keys;
	Summaries.GetKeys(Keys);
	Keys.Sort();
	for (const FString& Key : Keys)
	{
		if (OutSummaries.Num() >= MaxResults)
		{
			bOutTruncated = true;
			continue;
		}
		if (const FBlueprintHelperReferenceAssetSummary* Summary =
				Summaries.Find(Key))
		{
			OutSummaries.Add(*Summary);
		}
	}
}

void FBlueprintHelperDependencyAnalysisServiceUtils::FillSummaryAndHints(
	FBlueprintHelperReferenceContextPack& Context)
{
	Context.Summary.AssetCount = Context.Referencers.Num();
	Context.Summary.ReferenceCount = 0;
	Context.Summary.BlockingCount = 0;
	Context.Summary.WarningCount = 0;
	for (const FBlueprintHelperReferenceAssetSummary& Referencer :
		 Context.Referencers)
	{
		Context.Summary.ReferenceCount += Referencer.MatchCount;
		if (Referencer.Safety == TEXT("blocking"))
		{
			++Context.Summary.BlockingCount;
		}
		else if (Referencer.Safety == TEXT("warning"))
		{
			++Context.Summary.WarningCount;
		}
	}

	Context.AgentHints.bCanEditSafely = Context.Summary.BlockingCount == 0 &&
										!Context.Summary.bPartial &&
										!Context.Summary.bTruncated;
	Context.AgentHints.bRequiresPreview = true;
	Context.AgentHints.RecommendedTaskStrategy = TEXT("preview_before_write");
	if (Context.Summary.BlockingCount > 0)
	{
		Context.AgentHints.Blockers.Add(
			FString::Printf(TEXT("%d assets have blocking references."),
							Context.Summary.BlockingCount));
	}
	if (Context.Summary.bPartial)
	{
		Context.AgentHints.Blockers.Add(TEXT("reference context is partial"));
	}
	if (Context.Summary.bTruncated)
	{
		Context.AgentHints.Blockers.Add(TEXT("reference context is truncated"));
	}
}
