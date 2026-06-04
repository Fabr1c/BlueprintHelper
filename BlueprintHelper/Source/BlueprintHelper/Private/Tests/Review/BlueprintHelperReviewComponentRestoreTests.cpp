#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.h"
#include "Systems/Review/Utils/BlueprintHelperReviewStoreJsonUtils.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentFacts.h"

#include "Components/SceneComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

class FBlueprintHelperReviewComponentRestoreTestsLocalUtils
{
public:
	static FString MakeUniqueObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UBlueprint* MakeActorBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperReviewComponentRestore/%s"),
			*MakeUniqueObjectName(Prefix)));
		if (!Package)
		{
			return nullptr;
		}

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeUniqueObjectName(TEXT("BP_ComponentRestore")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperReviewComponentRestoreTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static USCS_Node* AddSceneNode(UBlueprint* Blueprint, const FString& ComponentName)
	{
		if (!Blueprint || !Blueprint->SimpleConstructionScript)
		{
			return nullptr;
		}

		USCS_Node* Node = Blueprint->SimpleConstructionScript->CreateNode(
			USceneComponent::StaticClass(),
			FName(*ComponentName));
		if (Node)
		{
			Blueprint->SimpleConstructionScript->AddNode(Node);
		}
		return Node;
	}

	static USCS_Node* FindNodeByName(UBlueprint* Blueprint, const FString& ComponentName)
	{
		if (!Blueprint || !Blueprint->SimpleConstructionScript)
		{
			return nullptr;
		}
		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->GetVariableName().ToString() == ComponentName)
			{
				return Node;
			}
		}
		return nullptr;
	}

	static FBoolProperty* FindWritableBoolProperty(UObject* Object, FString& OutPropertyName)
	{
		if (!Object)
		{
			return nullptr;
		}

		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			FBoolProperty* BoolProperty = CastField<FBoolProperty>(*It);
			if (BoolProperty && FBlueprintHelperEditablePropertyPolicy::AllowsWrite(BoolProperty))
			{
				OutPropertyName = BoolProperty->GetName();
				return BoolProperty;
			}
		}
		return nullptr;
	}

	static bool ReadBoolPropertyValue(UObject* Object, FBoolProperty* Property)
	{
		return Object && Property
			? Property->GetPropertyValue(Property->ContainerPtrToValuePtr<void>(Object))
			: false;
	}

	static void WriteBoolPropertyValue(UObject* Object, FBoolProperty* Property, bool bValue)
	{
		if (Object && Property)
		{
			Property->SetPropertyValue(Property->ContainerPtrToValuePtr<void>(Object), bValue);
		}
	}

	static TSharedRef<FJsonObject> MakeComponentSnapshot(
		USCS_Node* Node,
		const FString& PropertyPath,
		const FString& PropertyValue)
	{
		TSharedRef<FJsonObject> Snapshot = MakeShared<FJsonObject>();
		Snapshot->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.ComponentSnapshot.v1"));
		Snapshot->SetBoolField(TEXT("exists"), true);
		Snapshot->SetStringField(TEXT("component_name"), Node ? Node->GetVariableName().ToString() : FString());
		Snapshot->SetStringField(
			TEXT("component_class"),
			Node && Node->ComponentTemplate && Node->ComponentTemplate->GetClass()
				? Node->ComponentTemplate->GetClass()->GetPathName()
				: FString());
		Snapshot->SetStringField(
			TEXT("component_template_path"),
			Node && Node->ComponentTemplate ? Node->ComponentTemplate->GetPathName() : FString());

		TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("property_path"), PropertyPath);
		Entry->SetStringField(TEXT("name"), PropertyPath);
		Entry->SetStringField(TEXT("value"), PropertyValue);
		TArray<TSharedPtr<FJsonValue>> Entries;
		Entries.Add(MakeShared<FJsonValueObject>(Entry));
		Properties->SetArrayField(TEXT("properties"), Entries);
		Snapshot->SetObjectField(TEXT("properties"), Properties);
		return Snapshot;
	}

	static TSharedRef<FJsonObject> MakeStructuralComponentSnapshot(
		UBlueprint* Blueprint,
		USCS_Node* Node,
		const FString& ParentComponentName,
		const FString& SocketName)
	{
		const FBlueprintHelperComponentInfo Info =
			Blueprint && Node ? FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *Node) : FBlueprintHelperComponentInfo();
		TSharedRef<FJsonObject> Snapshot = MakeShared<FJsonObject>();
		Snapshot->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.ComponentSnapshot.v1"));
		Snapshot->SetBoolField(TEXT("exists"), true);
		Snapshot->SetStringField(TEXT("component_name"), Info.ComponentName);
		Snapshot->SetStringField(
			TEXT("component_class"),
			Node && Node->ComponentTemplate && Node->ComponentTemplate->GetClass()
				? Node->ComponentTemplate->GetClass()->GetPathName()
				: FString());
		Snapshot->SetStringField(TEXT("component_template_path"), Info.ComponentTemplatePath);
		Snapshot->SetStringField(TEXT("component_id"), Info.ComponentId);
		if (!ParentComponentName.IsEmpty())
		{
			Snapshot->SetStringField(TEXT("parent_component"), ParentComponentName);
		}
		if (!SocketName.IsEmpty())
		{
			Snapshot->SetStringField(TEXT("socket_name"), SocketName);
		}

		TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
		Properties->SetArrayField(TEXT("properties"), TArray<TSharedPtr<FJsonValue>>());
		Snapshot->SetObjectField(TEXT("properties"), Properties);
		return Snapshot;
	}

	static FBlueprintHelperReviewAtomicTarget MakeComponentTarget(
		UBlueprint* Blueprint,
		USCS_Node* Node,
		const FString& ComponentPathOverride = FString())
	{
		const FBlueprintHelperComponentInfo Info =
			Blueprint && Node ? FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *Node) : FBlueprintHelperComponentInfo();

		FBlueprintHelperReviewAtomicTarget Target;
		Target.AssetPath = Blueprint ? Blueprint->GetPathName() : FString();
		Target.Surface = EBlueprintHelperReviewSurface::Components;
		Target.TargetKind = TEXT("component");
		Target.TargetKey = FString::Printf(TEXT("component:%s"), *Info.ComponentName);
		Target.VisualGroupKey = Target.TargetKey;
		Target.DisplayLabel = Info.ComponentName;
		Target.ComponentPath = ComponentPathOverride.IsEmpty() ? Info.ComponentName : ComponentPathOverride;
		Target.ComponentTemplatePath = Info.ComponentTemplatePath;
		Target.ComponentId = Info.ComponentId;
		Target.ComponentOrigin = TEXT("owned_scs");
		Target.ReadbackFingerprintBefore = Info.ReadbackFingerprint;
		return Target;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewComponentSnapshotRestoreUsesTemplatePathTest,
	"BlueprintHelper.Review.Action.ComponentSnapshotRestoreUsesTemplatePath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewComponentSnapshotRestoreUsesTemplatePathTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewComponentRestoreTestsLocalUtils::MakeActorBlueprint(TEXT("TemplatePath"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	USCS_Node* WrongVisibleNode = FBlueprintHelperReviewComponentRestoreTestsLocalUtils::AddSceneNode(
		Blueprint,
		TEXT("VisibleWrongComponent"));
	USCS_Node* TargetNode = FBlueprintHelperReviewComponentRestoreTestsLocalUtils::AddSceneNode(
		Blueprint,
		TEXT("TemplatePathTargetComponent"));
	TestNotNull(TEXT("wrong visible node exists"), WrongVisibleNode);
	TestNotNull(TEXT("target node exists"), TargetNode);
	TestNotNull(TEXT("target template exists"), TargetNode ? TargetNode->ComponentTemplate.Get() : nullptr);
	if (!WrongVisibleNode || !TargetNode || !TargetNode->ComponentTemplate)
	{
		return false;
	}

	FString PropertyName;
	FBoolProperty* BoolProperty = FBlueprintHelperReviewComponentRestoreTestsLocalUtils::FindWritableBoolProperty(
		TargetNode->ComponentTemplate,
		PropertyName);
	TestNotNull(TEXT("target template has writable bool property"), BoolProperty);
	if (!BoolProperty)
	{
		return false;
	}

	FBlueprintHelperReviewComponentRestoreTestsLocalUtils::WriteBoolPropertyValue(
		TargetNode->ComponentTemplate,
		BoolProperty,
		false);
	FBlueprintHelperReviewAtomicTarget Target =
		FBlueprintHelperReviewComponentRestoreTestsLocalUtils::MakeComponentTarget(
			Blueprint,
			TargetNode,
			TEXT("VisibleWrongComponent"));
	TSharedRef<FJsonObject> Snapshot =
		FBlueprintHelperReviewComponentRestoreTestsLocalUtils::MakeComponentSnapshot(
			TargetNode,
			PropertyName,
			TEXT("true"));

	FString RestoreError;
	TestTrue(TEXT("restore succeeds using component_template_path despite component_path mismatch"),
		FBlueprintHelperReviewSnapshotRestoreService::RestoreComponentFromSnapshot(
			Target,
			Snapshot,
			RestoreError));
	if (!RestoreError.IsEmpty())
	{
		AddInfo(RestoreError);
	}
	TestTrue(TEXT("target template value restored"),
		FBlueprintHelperReviewComponentRestoreTestsLocalUtils::ReadBoolPropertyValue(
			TargetNode->ComponentTemplate,
			BoolProperty));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewComponentRejectRestoresDescendantsDeepestFirstTest,
	"BlueprintHelper.Review.Action.ComponentRejectRestoresDescendantsDeepestFirst",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewComponentRejectRestoresDescendantsDeepestFirstTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewComponentRestoreTestsLocalUtils::MakeActorBlueprint(TEXT("RestoreHierarchy"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		return false;
	}

	USCS_Node* ParentNode = FBlueprintHelperReviewComponentRestoreTestsLocalUtils::AddSceneNode(
		Blueprint,
		TEXT("RestoreParent"));
	USCS_Node* ChildNode = Blueprint->SimpleConstructionScript->CreateNode(
		USceneComponent::StaticClass(),
		FName(TEXT("RestoreChild")));
	TestNotNull(TEXT("parent node exists"), ParentNode);
	TestNotNull(TEXT("child node exists"), ChildNode);
	if (!ParentNode || !ChildNode)
	{
		return false;
	}
	ParentNode->AddChildNode(ChildNode);
	ChildNode->AttachToName = FName(TEXT("RestoreSocket"));

	FBlueprintHelperReviewAtomicTarget ParentTarget =
		FBlueprintHelperReviewComponentRestoreTestsLocalUtils::MakeComponentTarget(Blueprint, ParentNode);
	FBlueprintHelperReviewAtomicTarget ChildTarget =
		FBlueprintHelperReviewComponentRestoreTestsLocalUtils::MakeComponentTarget(Blueprint, ChildNode);
	TSharedRef<FJsonObject> ParentSnapshot =
		FBlueprintHelperReviewComponentRestoreTestsLocalUtils::MakeStructuralComponentSnapshot(
			Blueprint,
			ParentNode,
			TEXT(""),
			TEXT(""));
	TSharedRef<FJsonObject> ChildSnapshot =
		FBlueprintHelperReviewComponentRestoreTestsLocalUtils::MakeStructuralComponentSnapshot(
			Blueprint,
			ChildNode,
			TEXT("RestoreParent"),
			TEXT("RestoreSocket"));

	ParentNode->RemoveChildNode(ChildNode);
	Blueprint->SimpleConstructionScript->AddNode(ChildNode);
	Blueprint->SimpleConstructionScript->RemoveNode(ParentNode);
	TestNull(TEXT("parent removed before restore"),
		FBlueprintHelperReviewComponentRestoreTestsLocalUtils::FindNodeByName(Blueprint, TEXT("RestoreParent")));

	FString RestoreError;
	TestTrue(TEXT("parent component restore recreates removed parent"),
		FBlueprintHelperReviewSnapshotRestoreService::RestoreComponentFromSnapshot(
			ParentTarget,
			ParentSnapshot,
			RestoreError));
	if (!RestoreError.IsEmpty())
	{
		AddInfo(RestoreError);
	}
	RestoreError.Reset();
	TestTrue(TEXT("child component restore relinks to restored parent"),
		FBlueprintHelperReviewSnapshotRestoreService::RestoreComponentFromSnapshot(
			ChildTarget,
			ChildSnapshot,
			RestoreError));
	if (!RestoreError.IsEmpty())
	{
		AddInfo(RestoreError);
	}

	USCS_Node* RestoredParent = FBlueprintHelperReviewComponentRestoreTestsLocalUtils::FindNodeByName(
		Blueprint,
		TEXT("RestoreParent"));
	USCS_Node* RestoredChild = FBlueprintHelperReviewComponentRestoreTestsLocalUtils::FindNodeByName(
		Blueprint,
		TEXT("RestoreChild"));
	TestNotNull(TEXT("restored parent exists"), RestoredParent);
	TestNotNull(TEXT("restored child exists"), RestoredChild);
	if (RestoredParent && RestoredChild)
	{
		TestEqual(TEXT("child parent restored"),
			Blueprint->SimpleConstructionScript->FindParentNode(RestoredChild),
			RestoredParent);
		TestEqual(TEXT("child socket restored"),
			RestoredChild->AttachToName.ToString(),
			FString(TEXT("RestoreSocket")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewComponentSnapshotRestoreRejectsInvalidTemplateEvidenceTest,
	"BlueprintHelper.Review.Action.ComponentSnapshotRestoreRejectsInvalidTemplateEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewComponentSnapshotRestoreRejectsInvalidTemplateEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperReviewComponentRestoreTestsLocalUtils::MakeActorBlueprint(TEXT("RejectInvalid"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	USCS_Node* TargetNode = FBlueprintHelperReviewComponentRestoreTestsLocalUtils::AddSceneNode(
		Blueprint,
		TEXT("InvalidEvidenceComponent"));
	TestNotNull(TEXT("target node exists"), TargetNode);
	TestNotNull(TEXT("target template exists"), TargetNode ? TargetNode->ComponentTemplate.Get() : nullptr);
	if (!TargetNode || !TargetNode->ComponentTemplate)
	{
		return false;
	}

	FString PropertyName;
	FBoolProperty* BoolProperty = FBlueprintHelperReviewComponentRestoreTestsLocalUtils::FindWritableBoolProperty(
		TargetNode->ComponentTemplate,
		PropertyName);
	TestNotNull(TEXT("target template has writable bool property"), BoolProperty);
	if (!BoolProperty)
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target =
		FBlueprintHelperReviewComponentRestoreTestsLocalUtils::MakeComponentTarget(Blueprint, TargetNode);
	TSharedRef<FJsonObject> Snapshot =
		FBlueprintHelperReviewComponentRestoreTestsLocalUtils::MakeComponentSnapshot(
			TargetNode,
			PropertyName,
			TEXT("true"));

	FBlueprintHelperReviewAtomicTarget StalePathTarget = Target;
	StalePathTarget.ComponentTemplatePath += TEXT("_STALE");
	FString RestoreError;
	TestFalse(TEXT("stale template path is rejected"),
		FBlueprintHelperReviewSnapshotRestoreService::RestoreComponentFromSnapshot(
			StalePathTarget,
			Snapshot,
			RestoreError));
	TestTrue(TEXT("stale path error is reported"), RestoreError.Contains(TEXT("component_template_path")));

	FBlueprintHelperReviewAtomicTarget MismatchedIdTarget = Target;
	MismatchedIdTarget.ComponentId += TEXT("_MISMATCH");
	RestoreError.Reset();
	TestFalse(TEXT("component id mismatch is rejected"),
		FBlueprintHelperReviewSnapshotRestoreService::RestoreComponentFromSnapshot(
			MismatchedIdTarget,
			Snapshot,
			RestoreError));
	TestTrue(TEXT("component id mismatch error is reported"), RestoreError.Contains(TEXT("component_id")));

	TSharedRef<FJsonObject> InvalidPathSnapshot =
		FBlueprintHelperReviewComponentRestoreTestsLocalUtils::MakeComponentSnapshot(
			TargetNode,
			TEXT("DoesNotExist.Nested"),
			TEXT("true"));
	RestoreError.Reset();
	TestFalse(TEXT("invalid property path is rejected"),
		FBlueprintHelperReviewSnapshotRestoreService::RestoreComponentFromSnapshot(
			Target,
			InvalidPathSnapshot,
			RestoreError));
	TestTrue(TEXT("invalid property path error is reported"), RestoreError.Contains(TEXT("property")));

	TSharedRef<FJsonObject> InvalidValueSnapshot =
		FBlueprintHelperReviewComponentRestoreTestsLocalUtils::MakeComponentSnapshot(
			TargetNode,
			PropertyName,
			TEXT("not_a_bool_value"));
	RestoreError.Reset();
	TestFalse(TEXT("invalid property value is rejected"),
		FBlueprintHelperReviewSnapshotRestoreService::RestoreComponentFromSnapshot(
			Target,
			InvalidValueSnapshot,
			RestoreError));
	TestTrue(TEXT("invalid value error is reported"), RestoreError.Contains(TEXT("property")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewComponentMetadataRoundTripsJsonTest,
	"BlueprintHelper.Review.Action.ComponentMetadataRoundTripsJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewComponentMetadataRoundTripsJsonTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = TEXT("/Game/BlueprintHelperReview/BP_ComponentMetadata");
	Target.Surface = EBlueprintHelperReviewSurface::Components;
	Target.TargetKind = TEXT("component");
	Target.TargetKey = TEXT("component:MetadataTarget");
	Target.VisualGroupKey = Target.TargetKey;
	Target.DisplayLabel = TEXT("MetadataTarget");
	Target.ComponentPath = TEXT("MetadataTarget");
	Target.ComponentTemplatePath = TEXT("/Game/BlueprintHelperReview/BP_ComponentMetadata.BP_ComponentMetadata:SimpleConstructionScript_0.MetadataTarget");
	Target.ComponentId = TEXT("/Game/BlueprintHelperReview/BP_ComponentMetadata.BP_ComponentMetadata::SCS::MetadataTarget");
	Target.ComponentOrigin = TEXT("owned_scs");
	Target.ChangedPropertiesJson = TEXT("[{\"property_path\":\"bAutoActivate\",\"before_value\":\"true\",\"after_value\":\"false\"}]");
	Target.ReadbackFingerprintBefore = TEXT("before_fp");
	Target.ReadbackFingerprintAfter = TEXT("after_fp");

	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("change_component_metadata");
	Change.AssetPath = Target.AssetPath;
	Change.LocationKey = Target.VisualGroupKey;
	Change.LatestEvidenceId = TEXT("evidence_component_metadata");
	Change.SourceEvidenceIds.Add(Change.LatestEvidenceId);
	Change.DisplayLabel = Target.DisplayLabel;
	Change.AtomicTargets.Add(Target);

	FBlueprintHelperReviewRecord Record;
	Record.Schema = TEXT("BlueprintHelper.ReviewRecord.v2");
	Record.ReviewRecordId = TEXT("review_component_metadata");
	Record.ArchiveSessionId = TEXT("archive_component_metadata");
	Record.AssetPath = Target.AssetPath;
	Record.VisibleChanges.Add(Change);

	const TSharedRef<FJsonObject> Json = FBlueprintHelperReviewStoreJsonUtils::ReviewRecordToJson(Record);
	const TArray<TSharedPtr<FJsonValue>>* Changes = nullptr;
	TestTrue(TEXT("record json has visible_changes"), Json->TryGetArrayField(TEXT("visible_changes"), Changes));
	TestTrue(TEXT("record json has one change"), Changes && Changes->Num() == 1);
	if (!Changes || Changes->Num() != 1)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> ChangeJson = (*Changes)[0].IsValid() ? (*Changes)[0]->AsObject() : nullptr;
	const TArray<TSharedPtr<FJsonValue>>* AtomicTargets = nullptr;
	TestTrue(TEXT("change json has atomic targets"),
		ChangeJson.IsValid() && ChangeJson->TryGetArrayField(TEXT("atomic_targets"), AtomicTargets));
	TestTrue(TEXT("change json has one target"), AtomicTargets && AtomicTargets->Num() == 1);
	if (!AtomicTargets || AtomicTargets->Num() != 1)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> TargetJson = (*AtomicTargets)[0].IsValid()
		? (*AtomicTargets)[0]->AsObject()
		: nullptr;
	TestTrue(TEXT("target json exists"), TargetJson.IsValid());
	if (!TargetJson.IsValid())
	{
		return false;
	}

	FString ComponentTemplatePath;
	FString ComponentId;
	TestTrue(TEXT("component_template_path serialized"),
		TargetJson->TryGetStringField(TEXT("component_template_path"), ComponentTemplatePath));
	TestTrue(TEXT("component_id serialized"),
		TargetJson->TryGetStringField(TEXT("component_id"), ComponentId));
	TestEqual(TEXT("component_template_path value"), ComponentTemplatePath, Target.ComponentTemplatePath);
	TestEqual(TEXT("component_id value"), ComponentId, Target.ComponentId);

	const TArray<TSharedPtr<FJsonValue>>* ChangedProperties = nullptr;
	TestTrue(TEXT("changed_properties serialized as array"),
		TargetJson->TryGetArrayField(TEXT("changed_properties"), ChangedProperties));
	TestEqual(TEXT("one changed property serialized"), ChangedProperties ? ChangedProperties->Num() : 0, 1);

	FBlueprintHelperReviewRecord Loaded;
	TestTrue(TEXT("review record reads back"),
		FBlueprintHelperReviewStoreJsonUtils::ReadReviewRecordFromJson(Json, Loaded));
	TestEqual(TEXT("one loaded change"), Loaded.VisibleChanges.Num(), 1);
	TestEqual(TEXT("one loaded atomic target"),
		Loaded.VisibleChanges.Num() == 1 ? Loaded.VisibleChanges[0].AtomicTargets.Num() : 0,
		1);
	if (Loaded.VisibleChanges.Num() != 1 || Loaded.VisibleChanges[0].AtomicTargets.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& LoadedTarget = Loaded.VisibleChanges[0].AtomicTargets[0];
	TestEqual(TEXT("loaded component_template_path"), LoadedTarget.ComponentTemplatePath, Target.ComponentTemplatePath);
	TestEqual(TEXT("loaded component_id"), LoadedTarget.ComponentId, Target.ComponentId);
	TestEqual(TEXT("loaded component_origin"), LoadedTarget.ComponentOrigin, Target.ComponentOrigin);
	TestTrue(TEXT("loaded changed_properties keeps property path"),
		LoadedTarget.ChangedPropertiesJson.Contains(TEXT("bAutoActivate")));
	TestEqual(TEXT("loaded readback fingerprint before"),
		LoadedTarget.ReadbackFingerprintBefore,
		Target.ReadbackFingerprintBefore);
	TestEqual(TEXT("loaded readback fingerprint after"),
		LoadedTarget.ReadbackFingerprintAfter,
		Target.ReadbackFingerprintAfter);
	return true;
}

#endif
