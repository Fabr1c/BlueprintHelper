// BlueprintHelper Service Layer 。Blueprint Component 服务实现

#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentClassResolver.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentFacts.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"

#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/UnrealType.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/BlueprintGeneratedClass.h"

// ═══════════════════════════════════════════════════════════
// ToJson
// ═══════════════════════════════════════════════════════════

class FBlueprintHelperComponentServiceLocalUtils
{
public:
	struct FBlueprintHelperComponentOperationState
	{
		bool bOk = false;
		FString Operation;
		FString Status;
		bool bModified = false;

		FString AssetPath;
		FString TargetType;
		FString ComponentName;
		FString ComponentClass;
		FString PropertyPath;

		FBlueprintHelperComponentInfo Component;
		FBlueprintHelperComponentInfo BeforeComponent;
		FBlueprintHelperComponentInfo AfterComponent;
		FBlueprintHelperComponentAttachmentInfo Attachment;
		FBlueprintHelperComponentNameCollisionInfo NameCollision;
		FBlueprintHelperComponentPropertyResult PropertyResult;

		TArray<FBlueprintHelperComponentInfo> Components;
		TArray<FString> DeletedComponentIds;
		TArray<FString> MovedComponentIds;
		int32 ComponentCount = 0;
		int32 RootComponentCount = 0;
		FString BeforeParent;
		FString AfterParent;
		FString BeforeRoot;
		FString AfterRoot;
		FString DeletePolicy;
		FString TransformPolicy;

		bool bDryRun = false;
		int32 WouldChangeCount = 0;
		int32 WouldCreateCount = 0;
		int32 WouldUpdateCount = 0;
		int32 WouldRemoveCount = 0;
		int32 WouldNoOpCount = 0;

		bool bShouldCompile = false;
		bool bShouldSave = false;

		FString ErrorCode;
		FString ErrorStage;
		FString ErrorMessage;
		bool bRetryable = false;
		FString RollbackResult = TEXT("not_needed");
		TOptional<FBlueprintHelperToolSuggestedRoute> SuggestedRoute;
		TOptional<FBlueprintHelperToolBlockedBoundary> BlockedBoundary;
	};

	static EBlueprintHelperToolStage ComponentErrorStageFromString(const FString& Stage)
	{
		if (Stage == TEXT("resolve_blueprint") ||
			Stage == TEXT("resolve_component") ||
			Stage == TEXT("resolve_component_class") ||
			Stage == TEXT("resolve_parent_component"))
		{
			return EBlueprintHelperToolStage::ResolveTarget;
		}

		if (Stage == TEXT("preflight") || Stage == TEXT("name_collision"))
		{
			return EBlueprintHelperToolStage::Preflight;
		}

		return EBlueprintHelperToolStage::Execute;
	}

	static EBlueprintHelperRollbackResult ComponentRollbackFromString(const FString& RollbackResult)
	{
		return RollbackResult == TEXT("rolled_back")
			? EBlueprintHelperRollbackResult::RolledBack
			: EBlueprintHelperRollbackResult::NotNeeded;
	}

	static FBlueprintHelperToolError MakeComponentError(const FBlueprintHelperComponentOperationState& State)
	{
		FBlueprintHelperToolError Error;
		Error.Code = State.ErrorCode;
		Error.Stage = ComponentErrorStageFromString(State.ErrorStage);
		Error.Message = State.ErrorMessage;
		Error.bRetryable = State.bRetryable;
		Error.RollbackResult = ComponentRollbackFromString(State.RollbackResult);
		Error.SuggestedRoute = State.SuggestedRoute;
		Error.BlockedBoundary = State.BlockedBoundary;
		return Error;
	}

	static TSharedRef<FJsonObject> MakeComponentTargetJson(const FBlueprintHelperComponentOperationState& State)
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), State.AssetPath);
		Target->SetStringField(TEXT("target_type"), State.TargetType);
		if (!State.ComponentName.IsEmpty()) Target->SetStringField(TEXT("component_name"), State.ComponentName);
		if (!State.ComponentClass.IsEmpty()) Target->SetStringField(TEXT("component_class"), State.ComponentClass);
		if (!State.PropertyPath.IsEmpty()) Target->SetStringField(TEXT("property_path"), State.PropertyPath);
		return Target;
	}

	static TSharedRef<FJsonObject> MakeComponentDryRunJson(const FBlueprintHelperComponentOperationState& State)
	{
		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		DryRun->SetStringField(TEXT("preview_kind"), TEXT("service"));
		DryRun->SetBoolField(TEXT("can_execute"), State.bOk);
		DryRun->SetStringField(TEXT("result"), State.bOk ? TEXT("passed") : TEXT("blocked"));
		DryRun->SetNumberField(TEXT("would_change_count"), State.WouldChangeCount);
		DryRun->SetNumberField(TEXT("would_create_count"), State.WouldCreateCount);
		DryRun->SetNumberField(TEXT("would_update_count"), State.WouldUpdateCount);
		DryRun->SetNumberField(TEXT("would_remove_count"), State.WouldRemoveCount);
		DryRun->SetNumberField(TEXT("would_no_op_count"), State.WouldNoOpCount);
		return DryRun;
	}

	static TSharedRef<FJsonObject> MakeComponentDataJson(const FBlueprintHelperComponentOperationState& State)
	{
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("BlueprintComponent.v1"));

		if (State.Operation == TEXT("read_components"))
		{
			TArray<TSharedPtr<FJsonValue>> Comps;
			for (const auto& Info : State.Components)
			{
				Comps.Add(MakeShared<FJsonValueObject>(Info.ToJson(false)));
			}
			Data->SetArrayField(TEXT("components"), Comps);
			TSharedRef<FJsonObject> Stats = MakeShared<FJsonObject>();
			Stats->SetNumberField(TEXT("components"), State.ComponentCount);
			Stats->SetNumberField(TEXT("root_components"), State.RootComponentCount);
			Data->SetObjectField(TEXT("stats"), Stats);
		}
		else if (State.Operation == TEXT("add_component"))
		{
			Data->SetObjectField(TEXT("component"), State.Component.ToJson(true));
			Data->SetObjectField(TEXT("attachment"), State.Attachment.ToJson());
			Data->SetObjectField(TEXT("name_collision"), State.NameCollision.ToJson());
		}
		else if (State.Operation == TEXT("set_component_property") || State.Operation == TEXT("set_component_properties"))
		{
			Data->SetObjectField(TEXT("component"), State.Component.ToJson(false));
			Data->SetObjectField(TEXT("property_result"), State.PropertyResult.ToJson());
		}
		else if (State.Operation == TEXT("remove_component"))
		{
			Data->SetObjectField(TEXT("component"), State.Component.ToJson(false));
		}
		else if (State.Operation == TEXT("rename_component") ||
			State.Operation == TEXT("reparent_component") ||
			State.Operation == TEXT("attach_component") ||
			State.Operation == TEXT("detach_component") ||
			State.Operation == TEXT("set_root_component"))
		{
			Data->SetObjectField(TEXT("component"), State.Component.ToJson(false));
		}

		if (State.BeforeComponent.bHasReadbackFacts)
		{
			Data->SetObjectField(TEXT("before_component"), State.BeforeComponent.ToJson(false));
		}
		if (State.AfterComponent.bHasReadbackFacts)
		{
			Data->SetObjectField(TEXT("after_component"), State.AfterComponent.ToJson(false));
		}
		if (!State.BeforeParent.IsEmpty() || !State.AfterParent.IsEmpty())
		{
			TSharedRef<FJsonObject> HierarchyChange = MakeShared<FJsonObject>();
			if (State.BeforeParent.IsEmpty()) HierarchyChange->SetField(TEXT("before_parent"), MakeShared<FJsonValueNull>());
			else HierarchyChange->SetStringField(TEXT("before_parent"), State.BeforeParent);
			if (State.AfterParent.IsEmpty()) HierarchyChange->SetField(TEXT("after_parent"), MakeShared<FJsonValueNull>());
			else HierarchyChange->SetStringField(TEXT("after_parent"), State.AfterParent);
			if (!State.TransformPolicy.IsEmpty())
			{
				HierarchyChange->SetStringField(TEXT("transform_policy"), State.TransformPolicy);
			}
			Data->SetObjectField(TEXT("hierarchy_change"), HierarchyChange);
		}
		if (!State.BeforeRoot.IsEmpty() || !State.AfterRoot.IsEmpty())
		{
			TSharedRef<FJsonObject> RootChange = MakeShared<FJsonObject>();
			if (State.BeforeRoot.IsEmpty()) RootChange->SetField(TEXT("before_root"), MakeShared<FJsonValueNull>());
			else RootChange->SetStringField(TEXT("before_root"), State.BeforeRoot);
			if (State.AfterRoot.IsEmpty()) RootChange->SetField(TEXT("after_root"), MakeShared<FJsonValueNull>());
			else RootChange->SetStringField(TEXT("after_root"), State.AfterRoot);
			Data->SetObjectField(TEXT("root_change"), RootChange);
		}
		if (!State.DeletePolicy.IsEmpty())
		{
			Data->SetStringField(TEXT("delete_policy"), State.DeletePolicy);
		}
		if (State.DeletedComponentIds.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Deleted;
			for (const FString& ComponentId : State.DeletedComponentIds)
			{
				Deleted.Add(MakeShared<FJsonValueString>(ComponentId));
			}
			Data->SetArrayField(TEXT("deleted_component_ids"), Deleted);
		}
		if (State.MovedComponentIds.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Moved;
			for (const FString& ComponentId : State.MovedComponentIds)
			{
				Moved.Add(MakeShared<FJsonValueString>(ComponentId));
			}
			Data->SetArrayField(TEXT("moved_component_ids"), Moved);
		}

		if (State.bDryRun)
		{
			Data->SetObjectField(TEXT("dry_run"), MakeComponentDryRunJson(State));
		}
		if (State.SuggestedRoute.IsSet())
		{
			Data->SetObjectField(TEXT("suggested_route"), State.SuggestedRoute->ToJson());
		}
		if (State.BlockedBoundary.IsSet())
		{
			Data->SetObjectField(TEXT("blocked_boundary"), State.BlockedBoundary->ToJson());
		}

		return Data;
	}

	static FBlueprintHelperValidationSummary MakeComponentValidation(const FBlueprintHelperComponentOperationState& State)
	{
		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = State.bShouldCompile;
		Validation.bShouldSave = State.bShouldSave;
		return Validation;
	}

	static FBlueprintHelperToolResultBase BuildComponentToolResult(const FBlueprintHelperComponentOperationState& State)
	{
		const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();

		FBlueprintHelperToolResultBase Result = !State.bOk
			? FBlueprintHelperToolResultBuilder::Failure(State.Operation, TraceId, MakeComponentError(State))
			: State.Status == TEXT("applied")
				? FBlueprintHelperToolResultBuilder::Applied(State.Operation, TraceId)
				: State.Status == TEXT("no_op")
					? FBlueprintHelperToolResultBuilder::NoOp(State.Operation, TraceId)
					: State.Status == TEXT("dry_run")
						? FBlueprintHelperToolResultBuilder::DryRun(State.Operation, TraceId)
						: FBlueprintHelperToolResultBuilder::Completed(State.Operation, TraceId);

		Result.bModified = State.bModified;
		Result.CustomTargetJson = MakeComponentTargetJson(State);
		Result.Data = MakeComponentDataJson(State);
		if (State.Operation != TEXT("read_components") && State.bOk)
		{
			Result.Validation = MakeComponentValidation(State);
		}
		return Result;
	}

	static FString NormalizeComponentPropertyPath(const FString& PropertyPath)
	{
		TArray<FString> Segments;
		PropertyPath.ParseIntoArray(Segments, TEXT("."), true);
		for (FString& Segment : Segments)
		{
			Segment.TrimStartAndEndInline();
		}
		return FString::Join(Segments, TEXT("."));
	}

	static void SetFailure(
		FBlueprintHelperComponentOperationState& State,
		const FString& Code,
		const FString& Stage,
		const FString& Message)
	{
		State.bOk = false;
		State.Status = TEXT("failed");
		State.ErrorCode = Code;
		State.ErrorStage = Stage;
		State.ErrorMessage = Message;
	}

	static FString ComponentOriginFromFacts(const FBlueprintHelperComponentInfo& Info)
	{
		if (Info.bIsOwnedSCS) return TEXT("owned_scs");
		if (Info.bIsNative) return TEXT("native");
		if (Info.bIsInherited) return TEXT("inherited");
		return TEXT("unknown");
	}

	static void AddOwnedScsBlockedBoundaryHint(
		FBlueprintHelperComponentOperationState& State,
		const FBlueprintHelperComponentInfo& Info,
		const FString& BlockedOperation)
	{
		FBlueprintHelperToolBlockedBoundary Boundary;
		Boundary.BoundaryId = TEXT("component_tree_owned_scs_only");
		Boundary.Origin = ComponentOriginFromFacts(Info);
		Boundary.BlockedOperation = BlockedOperation;
		State.BlockedBoundary = MoveTemp(Boundary);
	}

	static FBlueprintHelperToolSuggestedRoute MakeClassDefaultSuggestedRoute(
		const FString& ComponentName,
		const TArray<FBlueprintHelperComponentPropertySetting>& Settings)
	{
		FBlueprintHelperToolSuggestedRoute Route;
		Route.RouteId = TEXT("blueprint_class_settings.class_default");
		Route.Family = TEXT("blueprint_class_settings");
		Route.WriteMode = TEXT("class_settings.edit");
		Route.ClusterId = TEXT("class_settings");
		Route.OperationId = TEXT("set_class_default");
		Route.TemplateId = TEXT("blueprint_class_settings_class_default");
		Route.TaskType = TEXT("edit_blueprint_class_settings");
		Route.Reason = TEXT("native_component_default_property");
		Route.AppliesWhen = TEXT("intent_is_default_property_write");

		for (const FBlueprintHelperComponentPropertySetting& Setting : Settings)
		{
			const FString NormalizedPropertyPath = NormalizeComponentPropertyPath(Setting.PropertyPath);
			if (NormalizedPropertyPath.IsEmpty())
			{
				continue;
			}
			Route.PropertyPathHints.Add(ComponentName.IsEmpty()
				? NormalizedPropertyPath
				: ComponentName + TEXT(".") + NormalizedPropertyPath);
		}
		if (Route.PropertyPathHints.Num() > 0)
		{
			Route.PropertyPathHint = Route.PropertyPathHints[0];
		}
		return Route;
	}

	static bool EnsureOwnedScsMutationAllowed(
		const FBlueprintHelperComponentInfo& Info,
		const FString& CapabilityField,
		FBlueprintHelperComponentOperationState& State)
	{
		if (!Info.bIsOwnedSCS)
		{
			SetFailure(
				State,
				TEXT("component_not_owned_scs"),
				TEXT("preflight"),
				FString::Printf(
					TEXT("component mutation requires owned SCS component: %s origin=%s"),
					*Info.ComponentName,
					*ComponentOriginFromFacts(Info)));
			State.Component = Info;
			AddOwnedScsBlockedBoundaryHint(State, Info, State.Operation);
			return false;
		}

		const bool bAllowed = CapabilityField == TEXT("can_delete")
			? Info.bCanDelete
			: CapabilityField == TEXT("can_rename")
				? Info.bCanRename
				: CapabilityField == TEXT("can_reparent")
					? Info.bCanReparent
					: Info.bIsOwnedSCS;
		if (bAllowed)
		{
			return true;
		}

		SetFailure(
			State,
			TEXT("component_mutation_blocked"),
			TEXT("preflight"),
			FString::Printf(
				TEXT("component mutation is blocked by readback facts: %s capability=%s"),
				*Info.ComponentName,
				*CapabilityField));
		State.Component = Info;
		return false;
	}

	static bool FindReadbackComponentByName(
		const UBlueprint& Blueprint,
		const FString& ComponentName,
		FBlueprintHelperComponentInfo& OutInfo)
	{
		for (const FBlueprintHelperComponentInfo& Info : FBlueprintHelperComponentFacts::BuildReadbackFacts(Blueprint))
		{
			if (Info.ComponentName == ComponentName)
			{
				OutInfo = Info;
				return true;
			}
		}
		return false;
	}

	static bool FailIfReadbackOnlyComponentExists(
		const UBlueprint& Blueprint,
		const FString& ComponentName,
		const FString& CapabilityField,
		FBlueprintHelperComponentOperationState& State)
	{
		FBlueprintHelperComponentInfo Info;
		if (!FindReadbackComponentByName(Blueprint, ComponentName, Info))
		{
			return false;
		}

		EnsureOwnedScsMutationAllowed(Info, CapabilityField, State);
		return true;
	}

	static FString FindPrimaryRootName(const UBlueprint& Blueprint)
	{
		if (Blueprint.SimpleConstructionScript)
		{
			USCS_Node* SceneRootNode = nullptr;
			Blueprint.SimpleConstructionScript->GetSceneRootComponentTemplate(true, &SceneRootNode);
			if (SceneRootNode)
			{
				return SceneRootNode->GetVariableName().ToString();
			}
		}
		for (const FBlueprintHelperComponentInfo& Info : FBlueprintHelperComponentFacts::BuildReadbackFacts(Blueprint))
		{
			if (Info.bIsRoot)
			{
				return Info.ComponentName;
			}
		}
		return TEXT("");
	}

	static void ApplyTransformPolicy(
		USCS_Node* Node,
		EBlueprintHelperComponentTransformPolicy TransformPolicy,
		EBlueprintHelperAttachRule AttachRule)
	{
		if (!Node)
		{
			return;
		}
		USceneComponent* SceneComponent = Cast<USceneComponent>(Node->ComponentTemplate);
		if (!SceneComponent)
		{
			return;
		}
		if (TransformPolicy == EBlueprintHelperComponentTransformPolicy::ResetRelative ||
			AttachRule == EBlueprintHelperAttachRule::SnapToTarget)
		{
			SceneComponent->SetRelativeLocation(FVector::ZeroVector);
			SceneComponent->SetRelativeRotation(FRotator::ZeroRotator);
			SceneComponent->SetRelativeScale3D(FVector::OneVector);
		}
	}

	static bool ValidateSceneHierarchyPair(
		USCS_Node* ChildNode,
		USCS_Node* ParentNode,
		FBlueprintHelperComponentOperationState& State)
	{
		if (!ChildNode || !ChildNode->ComponentTemplate || !ChildNode->ComponentTemplate->IsA<USceneComponent>())
		{
			SetFailure(
				State,
				TEXT("component_not_scene"),
				TEXT("preflight"),
				FString::Printf(TEXT("component is not a scene component: %s"), *State.ComponentName));
			return false;
		}
		if (!ParentNode || !ParentNode->ComponentTemplate || !ParentNode->ComponentTemplate->IsA<USceneComponent>())
		{
			SetFailure(
				State,
				TEXT("component_parent_not_scene"),
				TEXT("preflight"),
				FString::Printf(TEXT("parent component is not a scene component: %s"), *State.Attachment.ParentComponent));
			return false;
		}
		return true;
	}

	static bool WouldCreateCycle(USCS_Node* ChildNode, USCS_Node* NewParentNode)
	{
		return ChildNode && NewParentNode &&
			(NewParentNode == ChildNode || NewParentNode->IsChildOf(ChildNode));
	}

	static void DetachNodeForMove(UBlueprint& Blueprint, USCS_Node& Node)
	{
		if (Blueprint.SimpleConstructionScript)
		{
			Blueprint.SimpleConstructionScript->RemoveNode(&Node, false);
		}
		Node.Modify();
		Node.AttachToName = NAME_None;
		Node.ParentComponentOrVariableName = NAME_None;
		Node.ParentComponentOwnerClassName = NAME_None;
		Node.bIsParentComponentNative = false;
	}

	static void AttachNodeToParent(
		USCS_Node& Node,
		USCS_Node& ParentNode,
		const FString& SocketName,
		EBlueprintHelperAttachRule AttachRule,
		EBlueprintHelperComponentTransformPolicy TransformPolicy)
	{
		ParentNode.Modify();
		Node.Modify();
		ParentNode.AddChildNode(&Node);
		Node.ParentComponentOrVariableName = ParentNode.GetVariableName();
		Node.ParentComponentOwnerClassName = NAME_None;
		Node.bIsParentComponentNative = false;
		Node.AttachToName = SocketName.IsEmpty() ? NAME_None : FName(*SocketName);
		ApplyTransformPolicy(&Node, TransformPolicy, AttachRule);
	}

	static void MoveChildrenToParent(
		UBlueprint& Blueprint,
		USCS_Node& SourceNode,
		USCS_Node* DestinationParent,
		TArray<FString>& OutMovedComponentIds)
	{
		if (!Blueprint.SimpleConstructionScript)
		{
			return;
		}

		const TArray<USCS_Node*> Children = SourceNode.GetChildNodes();
		for (USCS_Node* Child : Children)
		{
			if (!Child)
			{
				continue;
			}
			OutMovedComponentIds.Add(FBlueprintHelperComponentFacts::BuildReadbackFact(Blueprint, *Child).ComponentId);
			Child->Modify();
		}

		if (DestinationParent)
		{
			DestinationParent->Modify();
			DestinationParent->MoveChildNodes(&SourceNode);
			for (USCS_Node* Child : Children)
			{
				if (!Child)
				{
					continue;
				}
				Child->ParentComponentOrVariableName = DestinationParent->GetVariableName();
				Child->ParentComponentOwnerClassName = NAME_None;
				Child->bIsParentComponentNative = false;
			}
			return;
		}

		for (USCS_Node* Child : Children)
		{
			if (!Child)
			{
				continue;
			}
			SourceNode.RemoveChildNode(Child);
			Child->ParentComponentOrVariableName = NAME_None;
			Child->ParentComponentOwnerClassName = NAME_None;
			Child->bIsParentComponentNative = false;
			Child->AttachToName = NAME_None;
			Blueprint.SimpleConstructionScript->AddNode(Child);
		}
	}

	static void PromoteNodeToRoot(
		UBlueprint& Blueprint,
		USCS_Node& Node,
		EBlueprintHelperComponentTransformPolicy TransformPolicy)
	{
		DetachNodeForMove(Blueprint, Node);
		Node.Modify();
		Node.AttachToName = NAME_None;
		if (Blueprint.SimpleConstructionScript)
		{
			Blueprint.SimpleConstructionScript->AddNode(&Node);
		}
		ApplyTransformPolicy(&Node, TransformPolicy, EBlueprintHelperAttachRule::KeepRelative);
	}

	static void CollectDescendantsDeepestFirst(USCS_Node& Node, TArray<USCS_Node*>& OutNodes)
	{
		const TArray<USCS_Node*> Children = Node.GetChildNodes();
		for (USCS_Node* Child : Children)
		{
			if (Child)
			{
				CollectDescendantsDeepestFirst(*Child, OutNodes);
				OutNodes.Add(Child);
			}
		}
	}

	static bool ValidateNewComponentName(
		UBlueprint& Blueprint,
		USCS_Node& Node,
		const FString& NewComponentName,
		FBlueprintHelperComponentOperationState& State)
	{
		if (NewComponentName.IsEmpty() || FName(*NewComponentName).IsNone())
		{
			SetFailure(State, TEXT("invalid_component_name"), TEXT("preflight"), TEXT("new component name cannot be empty."));
			return false;
		}
		FKismetNameValidator Validator(&Blueprint, Node.GetVariableName());
		if (Validator.IsValid(NewComponentName) != EValidatorResult::Ok)
		{
			SetFailure(
				State,
				TEXT("invalid_component_name"),
				TEXT("preflight"),
				FString::Printf(TEXT("new component name is not valid: %s"), *NewComponentName));
			return false;
		}
		if (USCS_Node* ExistingNode = Blueprint.SimpleConstructionScript
			? Blueprint.SimpleConstructionScript->FindSCSNode(FName(*NewComponentName))
			: nullptr)
		{
			if (ExistingNode != &Node)
			{
				SetFailure(
					State,
					TEXT("component_name_conflict"),
					TEXT("preflight"),
					FString::Printf(TEXT("component name already exists: %s"), *NewComponentName));
				return false;
			}
		}
		return true;
	}

};

// ═══════════════════════════════════════════════════════════
// 服务实现
// ═══════════════════════════════════════════════════════════

FBlueprintHelperComponentService::FBlueprintHelperComponentService(const FBlueprintHelperGraphResolver& InResolver)
	: Resolver(InResolver)
{
}

// ─── ResolveBlueprint ───

UBlueprint* FBlueprintHelperComponentService::ResolveBlueprint(const FString& AssetPath, FString& OutError) const
{
	FBlueprintHelperGraphTarget Target;
	Target.BlueprintPath = AssetPath;

	FBlueprintHelperDiagnosticSet Diag;
	UBlueprint* Blueprint = Resolver.ResolveBlueprint(Target, Diag, FBlueprintHelperResolvePolicy::Mutation());
	if (!Blueprint)
	{
		OutError = Diag.Items.Num() > 0
			? Diag.Items[0].Message
			: FString::Printf(TEXT("无法解析蓝图资产: %s"), *AssetPath);
	}
	return Blueprint;
}

// ─── FindComponentNodeByName ───

USCS_Node* FBlueprintHelperComponentService::FindComponentNodeByName(UBlueprint* Blueprint, const FString& ComponentName)
{
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		return nullptr;
	}

	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (!Node) continue;
		if (Node->GetVariableName().ToString() == ComponentName)
		{
			return Node;
		}
	}

	UClass* ParentClass = Blueprint->GeneratedClass
		? Blueprint->GeneratedClass->GetSuperClass()
		: Blueprint->ParentClass.Get();
	while (ParentClass)
	{
		if (const UBlueprintGeneratedClass* ParentBPGC = Cast<UBlueprintGeneratedClass>(ParentClass))
		{
			if (ParentBPGC->SimpleConstructionScript)
			{
				for (USCS_Node* Node : ParentBPGC->SimpleConstructionScript->GetAllNodes())
				{
					if (Node && Node->GetVariableName().ToString() == ComponentName)
					{
						return Node;
					}
				}
			}
		}
		ParentClass = ParentClass->GetSuperClass();
	}

	return nullptr;
}

// ─── GetShortComponentClassName ───

FString FBlueprintHelperComponentService::GetShortComponentClassName(const UClass* ComponentClass)
{
	if (!ComponentClass) return TEXT("");
	FString Name = ComponentClass->GetName();
	Name.RemoveFromEnd(TEXT("_C"));
	return Name;
}

// ─── ResolveComponentClass ───

UClass* FBlueprintHelperComponentService::ResolveComponentClass(const FString& ComponentClass, FString& OutError)
{
	FBlueprintHelperComponentClassResolveResult ResolveResult;
	if (!FBlueprintHelperComponentClassResolver::ResolveActorComponentClass(ComponentClass, ResolveResult))
	{
		OutError = ResolveResult.ErrorMessage;
		return nullptr;
	}
	return ResolveResult.ResolvedClass;

}

// ─── ReadComponents ───

FBlueprintHelperToolResultBase FBlueprintHelperComponentService::ReadComponents(
	const FBlueprintHelperReadComponentsRequest& Request) const
{
	FBlueprintHelperComponentServiceLocalUtils::FBlueprintHelperComponentOperationState Result;
	Result.Operation = TEXT("read_components");
	Result.Status = TEXT("completed");
	Result.AssetPath = Request.AssetPath;
	Result.TargetType = TEXT("blueprint");

	FString Error;
	UBlueprint* Blueprint = ResolveBlueprint(Request.AssetPath, Error);
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("component_not_found");
		Result.ErrorStage = TEXT("resolve_blueprint");
		Result.ErrorMessage = Error.IsEmpty() ? TEXT("蓝图没有 SimpleConstructionScript。") : Error;
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	Result.Components = FBlueprintHelperComponentFacts::BuildReadbackFacts(*Blueprint);
	Result.ComponentCount = Result.Components.Num();
	for (const FBlueprintHelperComponentInfo& Info : Result.Components)
	{
		if (Info.bIsRoot)
		{
			++Result.RootComponentCount;
		}
	}
	Result.bOk = true;
	return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
}

// ─── AddComponent ───

FBlueprintHelperToolResultBase FBlueprintHelperComponentService::AddComponent(
	const FBlueprintHelperAddComponentRequest& Request) const
{
	FBlueprintHelperComponentServiceLocalUtils::FBlueprintHelperComponentOperationState Result;
	Result.Operation = TEXT("add_component");
	Result.AssetPath = Request.AssetPath;
	Result.TargetType = TEXT("component");
	Result.ComponentName = Request.ComponentName;
	Result.bDryRun = Request.bDryRun;
	Result.ComponentClass = Request.ComponentClass;
	Result.NameCollision.Policy = Request.NameCollisionPolicy;
	Result.Attachment.ParentComponent = Request.ParentComponent;
	Result.Attachment.SocketName = Request.SocketName;
	Result.Attachment.AttachRule = Request.AttachRule;

	FString Error;
	UBlueprint* Blueprint = ResolveBlueprint(Request.AssetPath, Error);
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("component_not_found");
		Result.ErrorStage = TEXT("resolve_blueprint");
		Result.ErrorMessage = Error.IsEmpty() ? TEXT("蓝图没有 SimpleConstructionScript。") : Error;
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	FString ClassError;
	UClass* ComponentClass = ResolveComponentClass(Request.ComponentClass, ClassError);
	if (!ComponentClass)
	{
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("unsupported_component_class");
		Result.ErrorStage = TEXT("resolve_component_class");
		Result.ErrorMessage = ClassError;
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	// 名称冲突检查
	USCS_Node* Existing = FindComponentNodeByName(Blueprint, Request.ComponentName);
	if (Existing)
	{
		Result.NameCollision.bHandled = true;
		Result.NameCollision.ExistingComponentName = Request.ComponentName;
		Result.Component.ComponentName = Request.ComponentName;
		Result.Component.ComponentClass = Existing->ComponentTemplate
			? GetShortComponentClassName(Existing->ComponentTemplate->GetClass()) : TEXT("");
		Result.NameCollision.ExistingComponentClass = Existing->ComponentTemplate && Existing->ComponentTemplate->GetClass()
			? Existing->ComponentTemplate->GetClass()->GetPathName()
			: TEXT("");
		Result.NameCollision.ExpectedComponentClass = ComponentClass->GetPathName();
		Result.Component.bCreated = false;
		Result.Component.bAlreadyExisted = true;

		// 类型不匹配
		if (!Existing->ComponentTemplate || !Existing->ComponentTemplate->GetClass()->IsChildOf(ComponentClass))
		{
			Result.bOk = false;
			Result.Status = TEXT("failed");
			Result.ErrorCode = TEXT("component_existing_mismatch");
			Result.ErrorStage = TEXT("name_collision");
			Result.ErrorMessage = FString::Printf(TEXT("组件 %s 已存在，但类型不是 %s。"), *Request.ComponentName, *Request.ComponentClass);
			return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
		}

		const FBlueprintHelperComponentInfo ExistingInfo =
			FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *Existing);
		Result.Component = ExistingInfo;
		if (!ExistingInfo.bIsOwnedSCS)
		{
			FBlueprintHelperComponentServiceLocalUtils::EnsureOwnedScsMutationAllowed(ExistingInfo, TEXT("owned_scs"), Result);
			return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
		}

		if (Request.NameCollisionPolicy == EBlueprintHelperComponentNameCollisionPolicy::FailIfExists)
		{
			Result.bOk = false;
			Result.Status = TEXT("failed");
			Result.ErrorCode = TEXT("component_already_exists");
			Result.ErrorStage = TEXT("name_collision");
			Result.ErrorMessage = FString::Printf(TEXT("组件已存在: %s"), *Request.ComponentName);
			return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
		}

		Result.bOk = true;
		Result.Status = Request.bDryRun ? TEXT("dry_run") : TEXT("no_op");
		Result.bModified = false;
		Result.bShouldCompile = false;
		Result.bShouldSave = false;
		if (Request.bDryRun)
		{
			Result.WouldNoOpCount = 1;
		}
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	// 父组件查找
	FBlueprintHelperComponentInfo ExistingReadbackInfo;
	if (FBlueprintHelperComponentServiceLocalUtils::FindReadbackComponentByName(*Blueprint, Request.ComponentName, ExistingReadbackInfo))
	{
		Result.NameCollision.bHandled = true;
		Result.NameCollision.ExistingComponentName = Request.ComponentName;
		Result.NameCollision.ExistingComponentClass = ExistingReadbackInfo.ClassPath;
		Result.NameCollision.ExpectedComponentClass = ComponentClass->GetPathName();
		Result.Component = ExistingReadbackInfo;
		Result.Component.bCreated = false;
		Result.Component.bAlreadyExisted = true;

		const UClass* ExistingComponentClass = ExistingReadbackInfo.ClassPath.IsEmpty()
			? nullptr
			: LoadObject<UClass>(nullptr, *ExistingReadbackInfo.ClassPath);
		if (!ExistingComponentClass || !ExistingComponentClass->IsChildOf(ComponentClass))
		{
			Result.bOk = false;
			Result.Status = TEXT("failed");
			Result.ErrorCode = TEXT("component_existing_mismatch");
			Result.ErrorStage = TEXT("name_collision");
			Result.ErrorMessage = FString::Printf(TEXT("component %s exists with a different class."), *Request.ComponentName);
			return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
		}

		FBlueprintHelperComponentServiceLocalUtils::EnsureOwnedScsMutationAllowed(
			ExistingReadbackInfo,
			TEXT("owned_scs"),
			Result);
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	USCS_Node* ParentNode = nullptr;
	if (!Request.ParentComponent.IsEmpty())
	{
		ParentNode = FindComponentNodeByName(Blueprint, Request.ParentComponent);
		if (!ParentNode)
		{
			if (FBlueprintHelperComponentServiceLocalUtils::FailIfReadbackOnlyComponentExists(
				*Blueprint,
				Request.ParentComponent,
				TEXT("owned_scs"),
				Result))
			{
				return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
			}

			Result.bOk = false;
			Result.Status = TEXT("failed");
			Result.ErrorCode = TEXT("parent_component_not_found");
			Result.ErrorStage = TEXT("resolve_parent_component");
			Result.ErrorMessage = FString::Printf(TEXT("未找到父组件: %s"), *Request.ParentComponent);
			return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
		}
	}

	if (ParentNode)
	{
		if (!ComponentClass->IsChildOf(USceneComponent::StaticClass()))
		{
			Result.bOk = false;
			Result.Status = TEXT("failed");
			Result.ErrorCode = TEXT("component_not_scene");
			Result.ErrorStage = TEXT("preflight");
			Result.ErrorMessage = FString::Printf(TEXT("component class cannot attach to a scene parent: %s"), *ComponentClass->GetPathName());
			return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
		}
		if (!ParentNode->ComponentTemplate || !ParentNode->ComponentTemplate->IsA<USceneComponent>())
		{
			Result.bOk = false;
			Result.Status = TEXT("failed");
			Result.ErrorCode = TEXT("component_parent_not_scene");
			Result.ErrorStage = TEXT("preflight");
			Result.ErrorMessage = FString::Printf(TEXT("parent component is not a scene component: %s"), *Request.ParentComponent);
			return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
		}
	}

	if (Request.bDryRun)
	{
		Result.bOk = true;
		Result.Status = TEXT("dry_run");
		Result.bModified = false;
		Result.Component.ComponentName = Request.ComponentName;
		Result.Component.ComponentClass = GetShortComponentClassName(ComponentClass);
		Result.Component.bCreated = false;
		Result.bShouldCompile = false;
		Result.bShouldSave = false;
		Result.WouldChangeCount = 1;
		Result.WouldCreateCount = 1;
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	// 创建组件
	FBlueprintHelperScopedAssetMutation Mutation(FText::FromString(TEXT("BlueprintHelper Add Component")), Blueprint);
	Mutation.Modify(Blueprint->SimpleConstructionScript);

	USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(ComponentClass, FName(*Request.ComponentName));
	if (!NewNode || !NewNode->ComponentTemplate)
	{
		Mutation.Rollback();
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("execution_failed");
		Result.ErrorStage = TEXT("create_scs_node");
		Result.ErrorMessage = TEXT("创建 SCS 组件节点失败。");
		Result.RollbackResult = TEXT("rolled_back");
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	Mutation.Modify(NewNode);
	Mutation.Modify(NewNode->ComponentTemplate);

	if (ParentNode)
	{
		Mutation.Modify(ParentNode);
		ParentNode->AddChildNode(NewNode);
		NewNode->ParentComponentOrVariableName = ParentNode->GetVariableName();
	}
	else
	{
		Blueprint->SimpleConstructionScript->AddNode(NewNode);
	}

	if (!Request.SocketName.IsEmpty())
	{
		NewNode->AttachToName = FName(*Request.SocketName);
		if (USceneComponent* SceneComp = Cast<USceneComponent>(NewNode->ComponentTemplate))
		{
			if (Request.AttachRule == EBlueprintHelperAttachRule::SnapToTarget)
			{
				SceneComp->SetRelativeLocation(FVector::ZeroVector);
				SceneComp->SetRelativeRotation(FRotator::ZeroRotator);
			}
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Mutation.Commit();

	Result.bOk = true;
	Result.Status = TEXT("applied");
	Result.bModified = true;
	Result.Component.ComponentName = Request.ComponentName;
	Result.Component.ComponentClass = GetShortComponentClassName(ComponentClass);
	Result.Component.bCreated = true;
	Result.bShouldCompile = true;
	Result.bShouldSave = true;
	return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
}

// ─── ResolvePropertyPath ───

bool FBlueprintHelperComponentService::ResolvePropertyPath(
	UObject* RootObject, const FString& PropertyPath,
	FProperty*& OutProperty, void*& OutValuePtr,
	FString& OutExpectedType, FString& OutErrorCode, FString& OutErrorMessage)
{
	OutProperty = nullptr;
	OutValuePtr = nullptr;

	if (!RootObject)
	{
		OutErrorCode = TEXT("component_not_found");
		OutErrorMessage = TEXT("组件模板为空。");
		return false;
	}

	TArray<FString> Segments;
	PropertyPath.ParseIntoArray(Segments, TEXT("."), true);
	if (Segments.Num() == 0)
	{
		OutErrorCode = TEXT("property_not_found");
		OutErrorMessage = TEXT("property_path 不能为空。");
		return false;
	}

	UStruct* CurrentStruct = RootObject->GetClass();
	void* CurrentContainer = RootObject;

	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		const bool bLast = Index == Segments.Num() - 1;
		const FName SegmentName(*Segments[Index]);

		FProperty* Property = CurrentStruct ? CurrentStruct->FindPropertyByName(SegmentName) : nullptr;
		if (!Property)
		{
			OutErrorCode = TEXT("property_not_found");
			OutErrorMessage = FString::Printf(TEXT("未找到属性路径段: %s"), *Segments[Index]);
			return false;
		}

		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(CurrentContainer);

		if (bLast)
		{
			OutProperty = Property;
			OutValuePtr = ValuePtr;
			OutExpectedType = Property->GetCPPType();
			return true;
		}

		if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			CurrentStruct = StructProp->Struct;
			CurrentContainer = ValuePtr;
			continue;
		}

		if (FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Property))
		{
			UObject* NestedObject = ObjectProp->GetObjectPropertyValue(ValuePtr);
			if (!NestedObject)
			{
				OutErrorCode = TEXT("object_reference_not_found");
				OutErrorMessage = FString::Printf(TEXT("属性路径段 %s 的对象引用为空。"), *Segments[Index]);
				return false;
			}
			CurrentStruct = NestedObject->GetClass();
			CurrentContainer = NestedObject;
			continue;
		}

		OutErrorCode = TEXT("struct_field_invalid");
		OutErrorMessage = FString::Printf(TEXT("属性路径段 %s 不是 struct/object，不能继续解析。"), *Segments[Index]);
		return false;
	}

	return false;
}

// ─── JsonValueToImportText ───

bool FBlueprintHelperComponentService::JsonValueToImportText(
	const TSharedPtr<FJsonValue>& Value, FString& OutText, FString& OutSummary, FString& OutError)
{
	if (!Value.IsValid() || Value->Type == EJson::Null)
	{
		OutError = TEXT("value 不能为空。");
		return false;
	}

	switch (Value->Type)
	{
	case EJson::String:
		OutText = Value->AsString();
		OutSummary = OutText.Left(128);
		return true;
	case EJson::Boolean:
		OutText = Value->AsBool() ? TEXT("true") : TEXT("false");
		OutSummary = OutText;
		return true;
	case EJson::Number:
		OutText = LexToString(Value->AsNumber());
		OutSummary = OutText;
		return true;
	default:
		OutError = TEXT("第一版组件属性 value 只支持 string / bool / number。");
		return false;
	}
}

// ─── ValidatePropertySetting ───

bool FBlueprintHelperComponentService::ValidatePropertySetting(
	UObject* ComponentTemplate, const FBlueprintHelperComponentPropertySetting& Setting,
	FBlueprintHelperInvalidComponentPropertySetting& OutInvalid)
{
	const FString NormalizedPropertyPath =
		FBlueprintHelperComponentServiceLocalUtils::NormalizeComponentPropertyPath(Setting.PropertyPath);
	OutInvalid.PropertyPath = NormalizedPropertyPath;

	FString ExpectedType, ErrorCode, ErrorMessage;
	FProperty* Property = nullptr;
	void* ValuePtr = nullptr;

	if (!ResolvePropertyPath(ComponentTemplate, NormalizedPropertyPath, Property, ValuePtr, ExpectedType, ErrorCode, ErrorMessage))
	{
		OutInvalid.Code = ErrorCode;
		OutInvalid.ExpectedType = ExpectedType;
		OutInvalid.ValueSummary = ErrorMessage.Left(128);
		return false;
	}

	OutInvalid.ExpectedType = ExpectedType;

	if (!FBlueprintHelperEditablePropertyPolicy::AllowsWrite(Property))
	{
		OutInvalid.Code = TEXT("property_not_writable");
		return false;
	}

	FString ImportText, Summary, ConvertError;
	if (!JsonValueToImportText(Setting.Value, ImportText, Summary, ConvertError))
	{
		OutInvalid.Code = TEXT("type_mismatch");
		OutInvalid.ExpectedType = ExpectedType;
		OutInvalid.ValueSummary = ConvertError.Left(128);
		return false;
	}

	// 尝试通过 ImportText_Direct 验证
	void* TempValue = FMemory_Alloca(Property->GetSize());
	Property->InitializeValue(TempValue);
	const TCHAR* ImportEnd = Property->ImportText_Direct(*ImportText, TempValue, ComponentTemplate, PPF_None);
	Property->DestroyValue(TempValue);

	if (!ImportEnd)
	{
		OutInvalid.Code = TEXT("type_mismatch");
		OutInvalid.ExpectedType = ExpectedType;
		OutInvalid.ValueSummary = Summary;
		return false;
	}

	return true;
}

// ─── SetComponentProperties ───

FBlueprintHelperToolResultBase FBlueprintHelperComponentService::SetComponentProperties(
	const FBlueprintHelperSetComponentPropertiesRequest& Request) const
{
	FBlueprintHelperComponentServiceLocalUtils::FBlueprintHelperComponentOperationState Result;
	Result.Operation = TEXT("set_component_properties");
	Result.AssetPath = Request.AssetPath;
	Result.TargetType = TEXT("component");
	Result.ComponentName = Request.ComponentName;
	Result.PropertyResult.Mode = EBlueprintHelperComponentPropertyMode::Batch;
	Result.PropertyResult.RequestedCount = Request.Settings.Num();
	Result.bDryRun = Request.bDryRun;

	FString Error;
	UBlueprint* Blueprint = ResolveBlueprint(Request.AssetPath, Error);
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("component_not_found");
		Result.ErrorStage = TEXT("resolve_blueprint");
		Result.ErrorMessage = Error;
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	USCS_Node* Node = FindComponentNodeByName(Blueprint, Request.ComponentName);
	if (!Node || !Node->ComponentTemplate)
	{
		if (FBlueprintHelperComponentServiceLocalUtils::FailIfReadbackOnlyComponentExists(
			*Blueprint,
			Request.ComponentName,
			TEXT("owned_scs"),
			Result))
		{
			Result.SuggestedRoute = FBlueprintHelperComponentServiceLocalUtils::MakeClassDefaultSuggestedRoute(
				Request.ComponentName,
				Request.Settings);
			return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
		}

		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("component_not_found");
		Result.ErrorStage = TEXT("resolve_component");
		Result.ErrorMessage = FString::Printf(TEXT("未找到组件: %s"), *Request.ComponentName);
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	Result.Component = FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *Node);
	if (!FBlueprintHelperComponentServiceLocalUtils::EnsureOwnedScsMutationAllowed(Result.Component, TEXT("owned_scs"), Result))
	{
		Result.SuggestedRoute = FBlueprintHelperComponentServiceLocalUtils::MakeClassDefaultSuggestedRoute(
			Request.ComponentName,
			Request.Settings);
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	if (Request.bDryRun)
	{
		for (const auto& Setting : Request.Settings)
		{
			const FString NormalizedPropertyPath =
				FBlueprintHelperComponentServiceLocalUtils::NormalizeComponentPropertyPath(Setting.PropertyPath);
			FBlueprintHelperInvalidComponentPropertySetting Invalid;
			Invalid.PropertyPath = NormalizedPropertyPath;

			FProperty* Property = nullptr;
			void* ValuePtr = nullptr;
			FString ExpectedType, ErrorCode, ErrorMessage;

			if (!ResolvePropertyPath(Node->ComponentTemplate, NormalizedPropertyPath,
				Property, ValuePtr, ExpectedType, ErrorCode, ErrorMessage))
			{
				Invalid.Code = ErrorCode;
				Invalid.ExpectedType = ExpectedType;
				Invalid.ValueSummary = ErrorMessage.Left(128);
				Result.PropertyResult.InvalidSettings.Add(MoveTemp(Invalid));
				continue;
			}

			Invalid.ExpectedType = ExpectedType;
			if (!FBlueprintHelperEditablePropertyPolicy::AllowsWrite(Property))
			{
				Invalid.Code = TEXT("property_not_writable");
				Result.PropertyResult.InvalidSettings.Add(MoveTemp(Invalid));
				continue;
			}

			FString ImportText, Summary, ConvertError;
			if (!JsonValueToImportText(Setting.Value, ImportText, Summary, ConvertError))
			{
				Invalid.Code = TEXT("type_mismatch");
				Invalid.ValueSummary = ConvertError.Left(128);
				Result.PropertyResult.InvalidSettings.Add(MoveTemp(Invalid));
				continue;
			}

			bool bWouldChange = true;
			if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
			{
				if (Setting.Value.IsValid() && Setting.Value->Type == EJson::Boolean)
				{
					const bool bCurrent = BoolProperty->GetPropertyValue(ValuePtr);
					bWouldChange = bCurrent != Setting.Value->AsBool();
				}
			}
			else if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
			{
				if (Setting.Value.IsValid() && Setting.Value->Type == EJson::Number)
				{
					if (NumericProperty->IsFloatingPoint())
					{
						bWouldChange = NumericProperty->GetFloatingPointPropertyValue(ValuePtr) != Setting.Value->AsNumber();
					}
					else if (NumericProperty->IsInteger())
					{
						bWouldChange = NumericProperty->GetSignedIntPropertyValue(ValuePtr) != static_cast<int64>(Setting.Value->AsNumber());
					}
				}
			}
			else if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
			{
				if (Setting.Value.IsValid() && Setting.Value->Type == EJson::String)
				{
					bWouldChange = StringProperty->GetPropertyValue(ValuePtr) != Setting.Value->AsString();
				}
			}

			if (bWouldChange)
			{
				++Result.WouldChangeCount;
				++Result.WouldUpdateCount;
			}
			else
			{
				++Result.WouldNoOpCount;
			}
		}

		if (Result.PropertyResult.InvalidSettings.Num() > 0)
		{
			Result.bOk = false;
			Result.Status = TEXT("failed");
			Result.bModified = false;
			Result.ErrorCode = TEXT("invalid_component_property_settings");
			Result.ErrorStage = TEXT("preflight");
			Result.ErrorMessage = TEXT("One or more component property settings are invalid.");
			Result.PropertyResult.AppliedCount = 0;
			Result.PropertyResult.ChangedCount = 0;
			Result.PropertyResult.NoOpCount = 0;
			return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
		}

		Result.bOk = true;
		Result.Status = TEXT("dry_run");
		Result.bModified = false;
		Result.bShouldCompile = false;
		Result.bShouldSave = false;
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	// Preflight: 检查所有属性设置
	for (const auto& Setting : Request.Settings)
	{
		FBlueprintHelperInvalidComponentPropertySetting Invalid;
		if (!ValidatePropertySetting(Node->ComponentTemplate, Setting, Invalid))
		{
			Result.PropertyResult.InvalidSettings.Add(MoveTemp(Invalid));
		}
	}

	if (Result.PropertyResult.InvalidSettings.Num() > 0)
	{
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.bModified = false;
		Result.ErrorCode = TEXT("invalid_component_property_settings");
		Result.ErrorStage = TEXT("preflight");
		Result.ErrorMessage = TEXT("One or more component property settings are invalid.");
		Result.PropertyResult.AppliedCount = 0;
		Result.PropertyResult.ChangedCount = 0;
		Result.PropertyResult.NoOpCount = 0;
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	// 应用属性
	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Set Component Properties")), Blueprint);

	Mutation.Modify(Blueprint->SimpleConstructionScript);
	Mutation.Modify(Node);
	Mutation.Modify(Node->ComponentTemplate);

	for (const auto& Setting : Request.Settings)
	{
		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		FString ExpectedType, ErrorCode, ErrorMessage;
		const FString NormalizedPropertyPath =
			FBlueprintHelperComponentServiceLocalUtils::NormalizeComponentPropertyPath(Setting.PropertyPath);

		if (!ResolvePropertyPath(Node->ComponentTemplate, NormalizedPropertyPath,
			Property, ValuePtr, ExpectedType, ErrorCode, ErrorMessage))
		{
			Mutation.Rollback();
			Result.bOk = false;
			Result.Status = TEXT("failed");
			Result.ErrorCode = TEXT("invalid_component_property_settings");
			Result.ErrorStage = TEXT("apply");
			Result.ErrorMessage = ErrorMessage;
			Result.RollbackResult = TEXT("rolled_back");
			return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
		}

		FString Before;
		Property->ExportTextItem_Direct(Before, ValuePtr, nullptr, Node->ComponentTemplate, PPF_None);

		FString ImportText, Summary, ConvertError;
		JsonValueToImportText(Setting.Value, ImportText, Summary, ConvertError);

		const TCHAR* ImportEnd = Property->ImportText_Direct(*ImportText, ValuePtr, Node->ComponentTemplate, PPF_None);
		if (!ImportEnd)
		{
			Mutation.Rollback();
			Result.bOk = false;
			Result.Status = TEXT("failed");
			Result.ErrorCode = TEXT("invalid_component_property_settings");
			Result.ErrorStage = TEXT("apply");
			Result.ErrorMessage = FString::Printf(TEXT("属性写入失败: %s"), *Setting.PropertyPath);
			Result.RollbackResult = TEXT("rolled_back");
			return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
		}

		FString After;
		Property->ExportTextItem_Direct(After, ValuePtr, nullptr, Node->ComponentTemplate, PPF_None);

		++Result.PropertyResult.AppliedCount;
		if (Before == After) ++Result.PropertyResult.NoOpCount;
		else
		{
			++Result.PropertyResult.ChangedCount;
			FBlueprintHelperChangedComponentProperty Changed;
			Changed.PropertyPath = NormalizedPropertyPath;
			Changed.BeforeValue = Before;
			Changed.AfterValue = After;
			Changed.ExpectedType = ExpectedType;
			Changed.ValueSummary = Summary;
			Result.PropertyResult.ChangedProperties.Add(MoveTemp(Changed));
		}
	}

	if (Result.PropertyResult.ChangedCount == 0)
	{
		Mutation.Rollback();
		Result.bOk = true;
		Result.Status = TEXT("no_op");
		Result.bModified = false;
		Result.bShouldCompile = false;
		Result.bShouldSave = false;
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	Node->ComponentTemplate->PostEditChange();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	Mutation.Commit();

	Result.bOk = true;
	Result.Status = TEXT("applied");
	Result.bModified = true;
	Result.Component = FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *Node);
	Result.bShouldCompile = true;
	Result.bShouldSave = true;
	return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
}

// ─── SetComponentProperty ───

FBlueprintHelperToolResultBase FBlueprintHelperComponentService::SetComponentProperty(
	const FBlueprintHelperSetComponentPropertiesRequest& Request) const
{
	FBlueprintHelperToolResultBase Result = SetComponentProperties(Request);
	Result.Operation = TEXT("set_component_property");
	if (Request.Settings.Num() > 0)
	{
		if (Result.CustomTargetJson.IsValid())
		{
			Result.CustomTargetJson->SetStringField(TEXT("property_path"), Request.Settings[0].PropertyPath);
		}
		if (Result.Data.IsValid())
		{
			const TSharedPtr<FJsonObject>* PropertyResultObj = nullptr;
			if (Result.Data->TryGetObjectField(TEXT("property_result"), PropertyResultObj) &&
				PropertyResultObj &&
				PropertyResultObj->IsValid())
			{
				(*PropertyResultObj)->SetStringField(TEXT("mode"), TEXT("single"));
				(*PropertyResultObj)->SetNumberField(TEXT("requested_count"), 1);
			}
		}
	}
	return Result;
}

// ─── RenameComponent ───

FBlueprintHelperToolResultBase FBlueprintHelperComponentService::RenameComponent(
	const FBlueprintHelperRenameComponentRequest& Request) const
{
	FBlueprintHelperComponentServiceLocalUtils::FBlueprintHelperComponentOperationState Result;
	Result.Operation = TEXT("rename_component");
	Result.AssetPath = Request.AssetPath;
	Result.TargetType = TEXT("component");
	Result.ComponentName = Request.ComponentName;
	Result.bDryRun = Request.bDryRun;

	FString Error;
	UBlueprint* Blueprint = ResolveBlueprint(Request.AssetPath, Error);
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		FBlueprintHelperComponentServiceLocalUtils::SetFailure(
			Result,
			TEXT("component_not_found"),
			TEXT("resolve_blueprint"),
			Error);
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	USCS_Node* Node = FindComponentNodeByName(Blueprint, Request.ComponentName);
	if (!Node || !Node->ComponentTemplate)
	{
		if (FBlueprintHelperComponentServiceLocalUtils::FailIfReadbackOnlyComponentExists(
			*Blueprint,
			Request.ComponentName,
			TEXT("can_rename"),
			Result))
		{
			return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
		}

		FBlueprintHelperComponentServiceLocalUtils::SetFailure(
			Result,
			TEXT("component_not_found"),
			TEXT("resolve_component"),
			FString::Printf(TEXT("component not found: %s"), *Request.ComponentName));
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	Result.BeforeComponent = FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *Node);
	Result.Component = Result.BeforeComponent;
	if (!FBlueprintHelperComponentServiceLocalUtils::EnsureOwnedScsMutationAllowed(Result.BeforeComponent, TEXT("can_rename"), Result))
	{
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}
	if (!FBlueprintHelperComponentServiceLocalUtils::ValidateNewComponentName(*Blueprint, *Node, Request.NewComponentName, Result))
	{
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	if (Request.ComponentName == Request.NewComponentName)
	{
		Result.bOk = true;
		Result.Status = Request.bDryRun ? TEXT("dry_run") : TEXT("no_op");
		Result.bModified = false;
		Result.AfterComponent = Result.BeforeComponent;
		if (Request.bDryRun)
		{
			Result.WouldNoOpCount = 1;
		}
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	if (Request.bDryRun)
	{
		Result.bOk = true;
		Result.Status = TEXT("dry_run");
		Result.bModified = false;
		Result.WouldChangeCount = 1;
		Result.WouldUpdateCount = 1;
		Result.AfterComponent = Result.BeforeComponent;
		Result.AfterComponent.ComponentName = Request.NewComponentName;
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Rename Component")), Blueprint);
	Mutation.Modify(Blueprint->SimpleConstructionScript);
	Mutation.Modify(Node);
	Mutation.Modify(Node->ComponentTemplate);

	FBlueprintEditorUtils::RenameComponentMemberVariable(Blueprint, Node, FName(*Request.NewComponentName));
	Mutation.Commit();

	Result.bOk = true;
	Result.Status = TEXT("applied");
	Result.bModified = true;
	Result.ComponentName = Request.NewComponentName;
	Result.AfterComponent = FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *Node);
	Result.Component = Result.AfterComponent;
	Result.bShouldCompile = true;
	Result.bShouldSave = true;
	return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
}

// ─── RemoveComponent ───

FBlueprintHelperToolResultBase FBlueprintHelperComponentService::ReparentComponent(
	const FBlueprintHelperReparentComponentRequest& Request) const
{
	FBlueprintHelperComponentServiceLocalUtils::FBlueprintHelperComponentOperationState Result;
	Result.Operation = TEXT("reparent_component");
	Result.AssetPath = Request.AssetPath;
	Result.TargetType = TEXT("component");
	Result.ComponentName = Request.ComponentName;
	Result.Attachment.ParentComponent = Request.NewParentComponent;
	Result.Attachment.SocketName = Request.SocketName;
	Result.Attachment.AttachRule = Request.AttachRule;
	Result.TransformPolicy = ComponentTransformPolicyToString(Request.TransformPolicy);
	Result.bDryRun = Request.bDryRun;

	FString Error;
	UBlueprint* Blueprint = ResolveBlueprint(Request.AssetPath, Error);
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		FBlueprintHelperComponentServiceLocalUtils::SetFailure(Result, TEXT("component_not_found"), TEXT("resolve_blueprint"), Error);
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	USCS_Node* Node = FindComponentNodeByName(Blueprint, Request.ComponentName);
	if (!Node || !Node->ComponentTemplate)
	{
		if (FBlueprintHelperComponentServiceLocalUtils::FailIfReadbackOnlyComponentExists(
			*Blueprint,
			Request.ComponentName,
			TEXT("can_reparent"),
			Result))
		{
			return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
		}

		FBlueprintHelperComponentServiceLocalUtils::SetFailure(
			Result,
			TEXT("component_not_found"),
			TEXT("resolve_component"),
			FString::Printf(TEXT("component not found: %s"), *Request.ComponentName));
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	USCS_Node* ParentNode = FindComponentNodeByName(Blueprint, Request.NewParentComponent);
	if (!ParentNode || !ParentNode->ComponentTemplate)
	{
		if (FBlueprintHelperComponentServiceLocalUtils::FailIfReadbackOnlyComponentExists(
			*Blueprint,
			Request.NewParentComponent,
			TEXT("owned_scs"),
			Result))
		{
			return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
		}

		FBlueprintHelperComponentServiceLocalUtils::SetFailure(
			Result,
			TEXT("parent_component_not_found"),
			TEXT("resolve_parent_component"),
			FString::Printf(TEXT("parent component not found: %s"), *Request.NewParentComponent));
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	Result.BeforeRoot = FBlueprintHelperComponentServiceLocalUtils::FindPrimaryRootName(*Blueprint);
	Result.BeforeComponent = FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *Node);
	Result.Component = Result.BeforeComponent;
	Result.BeforeParent = Result.BeforeComponent.ParentComponent;

	if (!FBlueprintHelperComponentServiceLocalUtils::EnsureOwnedScsMutationAllowed(Result.BeforeComponent, TEXT("can_reparent"), Result))
	{
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}
	FBlueprintHelperComponentInfo ParentInfo = FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *ParentNode);
	if (!FBlueprintHelperComponentServiceLocalUtils::EnsureOwnedScsMutationAllowed(ParentInfo, TEXT("owned_scs"), Result))
	{
		Result.Component = Result.BeforeComponent;
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}
	if (!FBlueprintHelperComponentServiceLocalUtils::ValidateSceneHierarchyPair(Node, ParentNode, Result))
	{
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}
	if (FBlueprintHelperComponentServiceLocalUtils::WouldCreateCycle(Node, ParentNode))
	{
		FBlueprintHelperComponentServiceLocalUtils::SetFailure(
			Result,
			TEXT("component_reparent_cycle"),
			TEXT("preflight"),
			FString::Printf(TEXT("reparent would create a cycle: %s -> %s"), *Request.ComponentName, *Request.NewParentComponent));
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	const bool bSameParent = Result.BeforeParent == Request.NewParentComponent;
	const bool bSameSocket = Result.BeforeComponent.SocketName == Request.SocketName;
	if (bSameParent && bSameSocket)
	{
		Result.bOk = true;
		Result.Status = Request.bDryRun ? TEXT("dry_run") : TEXT("no_op");
		Result.AfterComponent = Result.BeforeComponent;
		Result.AfterParent = Result.BeforeParent;
		Result.AfterRoot = Result.BeforeRoot;
		if (Request.bDryRun)
		{
			Result.WouldNoOpCount = 1;
		}
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	if (Request.bDryRun)
	{
		Result.bOk = true;
		Result.Status = TEXT("dry_run");
		Result.WouldChangeCount = 1;
		Result.WouldUpdateCount = 1;
		Result.AfterComponent = Result.BeforeComponent;
		Result.AfterParent = Request.NewParentComponent;
		Result.AfterRoot = Result.BeforeRoot;
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	FBlueprintHelperScopedAssetMutation Mutation(FText::FromString(TEXT("BlueprintHelper Reparent Component")), Blueprint);
	Mutation.Modify(Blueprint->SimpleConstructionScript);
	Mutation.Modify(Node);
	Mutation.Modify(Node->ComponentTemplate);
	Mutation.Modify(ParentNode);
	if (USCS_Node* OldParentNode = Blueprint->SimpleConstructionScript->FindParentNode(Node))
	{
		Mutation.Modify(OldParentNode);
	}

	FBlueprintHelperComponentServiceLocalUtils::DetachNodeForMove(*Blueprint, *Node);
	FBlueprintHelperComponentServiceLocalUtils::AttachNodeToParent(*Node, *ParentNode, Request.SocketName, Request.AttachRule, Request.TransformPolicy);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Mutation.Commit();

	Result.bOk = true;
	Result.Status = TEXT("applied");
	Result.bModified = true;
	Result.AfterComponent = FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *Node);
	Result.Component = Result.AfterComponent;
	Result.AfterParent = Result.AfterComponent.ParentComponent;
	Result.AfterRoot = FBlueprintHelperComponentServiceLocalUtils::FindPrimaryRootName(*Blueprint);
	Result.bShouldCompile = true;
	Result.bShouldSave = true;
	return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
}

FBlueprintHelperToolResultBase FBlueprintHelperComponentService::AttachComponent(
	const FBlueprintHelperAttachComponentRequest& Request) const
{
	FBlueprintHelperReparentComponentRequest ReparentRequest;
	ReparentRequest.AssetPath = Request.AssetPath;
	ReparentRequest.ComponentName = Request.ComponentName;
	ReparentRequest.NewParentComponent = Request.ParentComponent;
	ReparentRequest.SocketName = Request.SocketName;
	ReparentRequest.AttachRule = Request.AttachRule;
	ReparentRequest.TransformPolicy = Request.TransformPolicy;
	ReparentRequest.bDryRun = Request.bDryRun;

	FBlueprintHelperToolResultBase Result = ReparentComponent(ReparentRequest);
	Result.Operation = TEXT("attach_component");
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperComponentService::DetachComponent(
	const FBlueprintHelperDetachComponentRequest& Request) const
{
	FBlueprintHelperComponentServiceLocalUtils::FBlueprintHelperComponentOperationState Result;
	Result.Operation = TEXT("detach_component");
	Result.AssetPath = Request.AssetPath;
	Result.TargetType = TEXT("component");
	Result.ComponentName = Request.ComponentName;
	Result.TransformPolicy = ComponentTransformPolicyToString(Request.TransformPolicy);
	Result.bDryRun = Request.bDryRun;

	FString Error;
	UBlueprint* Blueprint = ResolveBlueprint(Request.AssetPath, Error);
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		FBlueprintHelperComponentServiceLocalUtils::SetFailure(Result, TEXT("component_not_found"), TEXT("resolve_blueprint"), Error);
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	USCS_Node* Node = FindComponentNodeByName(Blueprint, Request.ComponentName);
	if (!Node || !Node->ComponentTemplate)
	{
		if (FBlueprintHelperComponentServiceLocalUtils::FailIfReadbackOnlyComponentExists(
			*Blueprint,
			Request.ComponentName,
			TEXT("can_reparent"),
			Result))
		{
			return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
		}

		FBlueprintHelperComponentServiceLocalUtils::SetFailure(
			Result,
			TEXT("component_not_found"),
			TEXT("resolve_component"),
			FString::Printf(TEXT("component not found: %s"), *Request.ComponentName));
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	Result.BeforeRoot = FBlueprintHelperComponentServiceLocalUtils::FindPrimaryRootName(*Blueprint);
	Result.BeforeComponent = FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *Node);
	Result.Component = Result.BeforeComponent;
	Result.BeforeParent = Result.BeforeComponent.ParentComponent;

	if (!FBlueprintHelperComponentServiceLocalUtils::EnsureOwnedScsMutationAllowed(Result.BeforeComponent, TEXT("can_reparent"), Result))
	{
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}
	if (!Node->ComponentTemplate->IsA<USceneComponent>())
	{
		FBlueprintHelperComponentServiceLocalUtils::SetFailure(
			Result,
			TEXT("component_not_scene"),
			TEXT("preflight"),
			FString::Printf(TEXT("component is not a scene component: %s"), *Request.ComponentName));
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	if (Result.BeforeParent.IsEmpty())
	{
		Result.bOk = true;
		Result.Status = Request.bDryRun ? TEXT("dry_run") : TEXT("no_op");
		Result.AfterComponent = Result.BeforeComponent;
		Result.AfterRoot = Result.BeforeRoot;
		if (Request.bDryRun)
		{
			Result.WouldNoOpCount = 1;
		}
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	if (Request.bDryRun)
	{
		Result.bOk = true;
		Result.Status = TEXT("dry_run");
		Result.WouldChangeCount = 1;
		Result.WouldUpdateCount = 1;
		Result.AfterComponent = Result.BeforeComponent;
		Result.AfterParent = TEXT("");
		Result.AfterRoot = Request.ComponentName;
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	FBlueprintHelperScopedAssetMutation Mutation(FText::FromString(TEXT("BlueprintHelper Detach Component")), Blueprint);
	Mutation.Modify(Blueprint->SimpleConstructionScript);
	Mutation.Modify(Node);
	Mutation.Modify(Node->ComponentTemplate);
	if (USCS_Node* OldParentNode = Blueprint->SimpleConstructionScript->FindParentNode(Node))
	{
		Mutation.Modify(OldParentNode);
	}

	FBlueprintHelperComponentServiceLocalUtils::PromoteNodeToRoot(*Blueprint, *Node, Request.TransformPolicy);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Mutation.Commit();

	Result.bOk = true;
	Result.Status = TEXT("applied");
	Result.bModified = true;
	Result.AfterComponent = FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *Node);
	Result.Component = Result.AfterComponent;
	Result.AfterParent = Result.AfterComponent.ParentComponent;
	Result.AfterRoot = FBlueprintHelperComponentServiceLocalUtils::FindPrimaryRootName(*Blueprint);
	Result.bShouldCompile = true;
	Result.bShouldSave = true;
	return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
}

FBlueprintHelperToolResultBase FBlueprintHelperComponentService::SetRootComponent(
	const FBlueprintHelperSetRootComponentRequest& Request) const
{
	FBlueprintHelperComponentServiceLocalUtils::FBlueprintHelperComponentOperationState Result;
	Result.Operation = TEXT("set_root_component");
	Result.AssetPath = Request.AssetPath;
	Result.TargetType = TEXT("component");
	Result.ComponentName = Request.ComponentName;
	Result.bDryRun = Request.bDryRun;

	FString Error;
	UBlueprint* Blueprint = ResolveBlueprint(Request.AssetPath, Error);
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		FBlueprintHelperComponentServiceLocalUtils::SetFailure(Result, TEXT("component_not_found"), TEXT("resolve_blueprint"), Error);
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	USCS_Node* Node = FindComponentNodeByName(Blueprint, Request.ComponentName);
	if (!Node || !Node->ComponentTemplate)
	{
		if (FBlueprintHelperComponentServiceLocalUtils::FailIfReadbackOnlyComponentExists(
			*Blueprint,
			Request.ComponentName,
			TEXT("can_reparent"),
			Result))
		{
			return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
		}

		FBlueprintHelperComponentServiceLocalUtils::SetFailure(
			Result,
			TEXT("component_not_found"),
			TEXT("resolve_component"),
			FString::Printf(TEXT("component not found: %s"), *Request.ComponentName));
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	Result.BeforeRoot = FBlueprintHelperComponentServiceLocalUtils::FindPrimaryRootName(*Blueprint);
	Result.BeforeComponent = FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *Node);
	Result.Component = Result.BeforeComponent;
	Result.BeforeParent = Result.BeforeComponent.ParentComponent;

	if (!Node->ComponentTemplate->IsA<USceneComponent>())
	{
		FBlueprintHelperComponentServiceLocalUtils::SetFailure(
			Result,
			TEXT("component_not_scene"),
			TEXT("preflight"),
			FString::Printf(TEXT("root component must be a scene component: %s"), *Request.ComponentName));
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}
	if (Result.BeforeRoot == Request.ComponentName)
	{
		Result.bOk = true;
		Result.Status = Request.bDryRun ? TEXT("dry_run") : TEXT("no_op");
		Result.AfterComponent = Result.BeforeComponent;
		Result.AfterRoot = Result.BeforeRoot;
		if (Request.bDryRun)
		{
			Result.WouldNoOpCount = 1;
		}
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}
	if (!FBlueprintHelperComponentServiceLocalUtils::EnsureOwnedScsMutationAllowed(Result.BeforeComponent, TEXT("can_reparent"), Result))
	{
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	if (Request.bDryRun)
	{
		Result.bOk = true;
		Result.Status = TEXT("dry_run");
		Result.WouldChangeCount = 1;
		Result.WouldUpdateCount = 1;
		Result.AfterComponent = Result.BeforeComponent;
		Result.AfterParent = TEXT("");
		Result.AfterRoot = Request.ComponentName;
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	USCS_Node* OldRootNode = FindComponentNodeByName(Blueprint, Result.BeforeRoot);
	FBlueprintHelperComponentInfo OldRootInfo;
	if (OldRootNode && OldRootNode->ComponentTemplate)
	{
		OldRootInfo = FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *OldRootNode);
	}

	FBlueprintHelperScopedAssetMutation Mutation(FText::FromString(TEXT("BlueprintHelper Set Root Component")), Blueprint);
	Mutation.Modify(Blueprint->SimpleConstructionScript);
	Mutation.Modify(Node);
	Mutation.Modify(Node->ComponentTemplate);
	if (OldRootNode)
	{
		Mutation.Modify(OldRootNode);
		if (OldRootNode->ComponentTemplate)
		{
			Mutation.Modify(OldRootNode->ComponentTemplate);
		}
	}

	FBlueprintHelperComponentServiceLocalUtils::PromoteNodeToRoot(
		*Blueprint,
		*Node,
		EBlueprintHelperComponentTransformPolicy::PreserveRelative);

	if (OldRootNode && OldRootNode != Node)
	{
		const bool bRemoveEmptyDefaultRoot =
			Request.OldRootPolicy == EBlueprintHelperComponentOldRootPolicy::RemoveDefaultSceneRootWhenEmpty &&
			OldRootInfo.bIsDefaultSceneRoot &&
			OldRootNode->GetChildNodes().Num() == 0;
		if (bRemoveEmptyDefaultRoot)
		{
			Result.DeletedComponentIds.Add(OldRootInfo.ComponentId);
			Blueprint->SimpleConstructionScript->RemoveNode(OldRootNode, false);
		}
		else
		{
			Result.MovedComponentIds.Add(OldRootInfo.ComponentId);
			FBlueprintHelperComponentServiceLocalUtils::DetachNodeForMove(*Blueprint, *OldRootNode);
			FBlueprintHelperComponentServiceLocalUtils::AttachNodeToParent(
				*OldRootNode,
				*Node,
				TEXT(""),
				EBlueprintHelperAttachRule::KeepRelative,
				EBlueprintHelperComponentTransformPolicy::PreserveRelative);
		}
	}

	Blueprint->SimpleConstructionScript->ValidateSceneRootNodes();
	Result.AfterRoot = FBlueprintHelperComponentServiceLocalUtils::FindPrimaryRootName(*Blueprint);
	const TArray<FBlueprintHelperComponentInfo> AfterFacts = FBlueprintHelperComponentFacts::BuildReadbackFacts(*Blueprint);
	int32 RootCount = 0;
	for (const FBlueprintHelperComponentInfo& Info : AfterFacts)
	{
		if (Info.bIsRoot)
		{
			++RootCount;
		}
	}
	if (RootCount != 1)
	{
		Mutation.Rollback();
		FBlueprintHelperComponentServiceLocalUtils::SetFailure(
			Result,
			TEXT("component_root_invalid"),
			TEXT("apply"),
			FString::Printf(TEXT("set_root_component produced %d root components."), RootCount));
		Result.RollbackResult = TEXT("rolled_back");
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Mutation.Commit();

	Result.bOk = true;
	Result.Status = TEXT("applied");
	Result.bModified = true;
	Result.AfterComponent = FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *Node);
	Result.Component = Result.AfterComponent;
	Result.AfterParent = Result.AfterComponent.ParentComponent;
	Result.bShouldCompile = true;
	Result.bShouldSave = true;
	return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
}

FBlueprintHelperToolResultBase FBlueprintHelperComponentService::RemoveComponent(
	const FBlueprintHelperRemoveComponentRequest& Request) const
{
	FBlueprintHelperComponentServiceLocalUtils::FBlueprintHelperComponentOperationState Result;
	Result.Operation = TEXT("remove_component");
	Result.AssetPath = Request.AssetPath;
	Result.TargetType = TEXT("component");
	Result.ComponentName = Request.ComponentName;
	Result.bDryRun = Request.bDryRun;
	Result.DeletePolicy = ComponentDeletePolicyToString(Request.DeletePolicy);

	FString Error;
	UBlueprint* Blueprint = ResolveBlueprint(Request.AssetPath, Error);
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("component_not_found");
		Result.ErrorStage = TEXT("resolve_blueprint");
		Result.ErrorMessage = Error;
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	USCS_Node* Node = FindComponentNodeByName(Blueprint, Request.ComponentName);
	if (!Node || !Node->ComponentTemplate)
	{
		if (FBlueprintHelperComponentServiceLocalUtils::FailIfReadbackOnlyComponentExists(
			*Blueprint,
			Request.ComponentName,
			TEXT("can_delete"),
			Result))
		{
			return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
		}

		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("component_not_found");
		Result.ErrorStage = TEXT("resolve_component");
		Result.ErrorMessage = FString::Printf(TEXT("未找到组件: %s"), *Request.ComponentName);
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	Result.BeforeRoot = FBlueprintHelperComponentServiceLocalUtils::FindPrimaryRootName(*Blueprint);
	Result.BeforeComponent = FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *Node);
	Result.Component = Result.BeforeComponent;
	Result.BeforeParent = Result.BeforeComponent.ParentComponent;

	if (!Result.BeforeComponent.bIsOwnedSCS)
	{
		FBlueprintHelperComponentServiceLocalUtils::EnsureOwnedScsMutationAllowed(Result.BeforeComponent, TEXT("owned_scs"), Result);
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}
	if (Result.BeforeComponent.bIsDefaultSceneRoot)
	{
		FBlueprintHelperComponentServiceLocalUtils::SetFailure(
			Result,
			TEXT("remove_component_blocked"),
			TEXT("preflight"),
			TEXT("remove_component cannot delete DefaultSceneRoot; use set_root_component root policies first."));
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	const TArray<USCS_Node*> Children = Node->GetChildNodes();
	USCS_Node* ParentNode = Blueprint->SimpleConstructionScript->FindParentNode(Node);
	if (Children.Num() > 0 && Request.DeletePolicy == EBlueprintHelperComponentDeletePolicy::BlockIfChildren)
	{
		FBlueprintHelperComponentServiceLocalUtils::SetFailure(
			Result,
			TEXT("remove_component_blocked"),
			TEXT("preflight"),
			FString::Printf(TEXT("component has children and delete_policy=block_if_children: %s"), *Request.ComponentName));
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}
	if (Children.Num() > 0 &&
		Request.DeletePolicy == EBlueprintHelperComponentDeletePolicy::ReattachChildrenToParent &&
		!ParentNode)
	{
		FBlueprintHelperComponentServiceLocalUtils::SetFailure(
			Result,
			TEXT("remove_component_blocked"),
			TEXT("preflight"),
			FString::Printf(TEXT("delete_policy=reattach_children_to_parent requires a parent component: %s"), *Request.ComponentName));
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	Result.DeletedComponentIds.Add(Result.BeforeComponent.ComponentId);
	if (Request.DeletePolicy == EBlueprintHelperComponentDeletePolicy::DeleteOwnedChildren)
	{
		TArray<USCS_Node*> Descendants;
		FBlueprintHelperComponentServiceLocalUtils::CollectDescendantsDeepestFirst(*Node, Descendants);
		for (USCS_Node* Descendant : Descendants)
		{
			if (Descendant)
			{
				Result.DeletedComponentIds.Add(FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *Descendant).ComponentId);
			}
		}
	}
	else if (Children.Num() > 0)
	{
		for (USCS_Node* Child : Children)
		{
			if (Child)
			{
				Result.MovedComponentIds.Add(FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *Child).ComponentId);
			}
		}
	}


	Result.Component = Result.BeforeComponent;

	if (Request.bDryRun)
	{
		Result.bOk = true;
		Result.Status = TEXT("dry_run");
		Result.bModified = false;
		Result.Component.bRemoved = false;
		Result.bShouldCompile = false;
		Result.bShouldSave = false;
		Result.WouldChangeCount = 1;
		Result.WouldRemoveCount = Result.DeletedComponentIds.Num();
		Result.WouldUpdateCount = Result.MovedComponentIds.Num();
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Remove Component")), Blueprint);

	Mutation.Modify(Blueprint->SimpleConstructionScript);
	Mutation.Modify(Node);
	if (ParentNode)
	{
		Mutation.Modify(ParentNode);
	}
	for (USCS_Node* Child : Children)
	{
		if (Child)
		{
			Mutation.Modify(Child);
		}
	}

	if (Request.DeletePolicy == EBlueprintHelperComponentDeletePolicy::DeleteOwnedChildren)
	{
		TArray<USCS_Node*> Descendants;
		FBlueprintHelperComponentServiceLocalUtils::CollectDescendantsDeepestFirst(*Node, Descendants);
		for (USCS_Node* Descendant : Descendants)
		{
			if (Descendant)
			{
				Mutation.Modify(Descendant);
				Blueprint->SimpleConstructionScript->RemoveNode(Descendant, false);
			}
		}
	}
	else if (Children.Num() > 0 &&
		(Request.DeletePolicy == EBlueprintHelperComponentDeletePolicy::PromoteChildren ||
			Request.DeletePolicy == EBlueprintHelperComponentDeletePolicy::ReattachChildrenToParent))
	{
		Result.MovedComponentIds.Reset();
		FBlueprintHelperComponentServiceLocalUtils::MoveChildrenToParent(
			*Blueprint,
			*Node,
			ParentNode,
			Result.MovedComponentIds);
	}

	Blueprint->SimpleConstructionScript->RemoveNode(Node, false);
	Blueprint->SimpleConstructionScript->ValidateSceneRootNodes();
	const bool bActuallyRemoved = FindComponentNodeByName(Blueprint, Request.ComponentName) == nullptr;
	if (!bActuallyRemoved)
	{
		Mutation.Rollback();
		Result.bOk = false;
		Result.Status = TEXT("failed");
		Result.ErrorCode = TEXT("execution_failed");
		Result.ErrorStage = TEXT("remove_scs_node");
		Result.ErrorMessage = TEXT("删除 SCS 组件节点失败。");
		Result.RollbackResult = TEXT("rolled_back");
		return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Mutation.Commit();

	Result.bOk = true;
	Result.Status = TEXT("applied");
	Result.bModified = true;
	Result.Component.bRemoved = true;
	Result.AfterRoot = FBlueprintHelperComponentServiceLocalUtils::FindPrimaryRootName(*Blueprint);
	Result.bShouldCompile = true;
	Result.bShouldSave = true;
	return FBlueprintHelperComponentServiceLocalUtils::BuildComponentToolResult(Result);
}
