#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/Review/BlueprintHelperClassDefaultSetterRestoreAdapter.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassDefaultPropertyMutationPolicy.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassDefaultPropertyMutationResolver.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "UObject/Package.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

class FBlueprintHelperClassDefaultSetterMutationTestUtils
{
public:
	static FString MakeUniqueName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UBlueprint* MakeCharacterBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperTests/ClassDefaultSetter/%s"),
			*MakeUniqueName(Prefix)));
		Package->SetDirtyFlag(false);
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ACharacter::StaticClass(),
			Package,
			*MakeUniqueName(TEXT("BP_SetterAwareCharacter")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperClassDefaultSetterMutationTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UObject* GetClassDefaultObject(UBlueprint* Blueprint)
	{
		UBlueprintGeneratedClass* GeneratedClass = Blueprint ? Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass) : nullptr;
		return GeneratedClass ? GeneratedClass->GetDefaultObject() : nullptr;
	}

	static FString MannyMeshPath()
	{
		return TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
	}

	static FBlueprintHelperClassDefaultPropertySetting MakeSetterSetting(const FString& MeshPath)
	{
		FBlueprintHelperClassDefaultPropertySetting Setting;
		Setting.PropertyPath = TEXT("Mesh.SkeletalMeshAsset");
		Setting.Value = MakeShared<FJsonValueString>(MeshPath);
		Setting.MutationStrategy = TEXT("setter_aware_property");
		return Setting;
	}

	static FString MannyMeshExportText()
	{
		return TEXT("/Script/Engine.SkeletalMesh'/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple'");
	}

	static FString MakeReviewTargetSnapshotJson(
		const FString& AssetPath,
		const FString& PropertyPath,
		const FString& Value)
	{
		TSharedRef<FJsonObject> Snapshot = MakeShared<FJsonObject>();
		Snapshot->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.ReviewTargetSnapshot.v2"));
		Snapshot->SetStringField(TEXT("asset_path"), AssetPath);
		Snapshot->SetStringField(TEXT("target_kind"), TEXT("class_default_setter_property"));
		Snapshot->SetStringField(TEXT("target_key"), TEXT("class_default_setter_property:Mesh_SkeletalMeshAsset"));
		Snapshot->SetStringField(TEXT("target_name"), PropertyPath);
		Snapshot->SetBoolField(TEXT("asset_exists"), true);
		Snapshot->SetStringField(TEXT("surface"), TEXT("details"));
		Snapshot->SetBoolField(TEXT("exists"), true);
		Snapshot->SetStringField(TEXT("property_path"), PropertyPath);
		Snapshot->SetStringField(TEXT("property_class"), TEXT("ObjectProperty"));
		Snapshot->SetStringField(TEXT("expected_type"), TEXT("TObjectPtr<USkeletalMesh>"));
		Snapshot->SetStringField(TEXT("value"), Value);

		FString Json;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Snapshot, Writer);
		return Json;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperClassDefaultSetterMutation_ResolvesSkeletalMeshAssetSetter,
	"BlueprintHelper.ClassSettings.SetterAware.ResolvesSkeletalMeshAssetSetter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperClassDefaultSetterMutation_ResolvesSkeletalMeshAssetSetter::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperClassDefaultSetterMutationTestUtils::MakeCharacterBlueprint(TEXT("ResolveSetter"));
	UObject* CDO = FBlueprintHelperClassDefaultSetterMutationTestUtils::GetClassDefaultObject(Blueprint);
	TestNotNull(TEXT("Character Blueprint CDO exists"), CDO);
	if (!CDO)
	{
		return false;
	}

	FBlueprintHelperClassDefaultResolvedMutationTarget Target;
	FString ErrorCode;
	FString ErrorMessage;
	const FBlueprintHelperClassDefaultPropertyMutationResolver Resolver;
	TestTrue(TEXT("Mesh.SkeletalMeshAsset resolves"), Resolver.Resolve(CDO, TEXT("Mesh.SkeletalMeshAsset"), Target, ErrorCode, ErrorMessage));
	TestEqual(TEXT("owner object path"), Target.OwnerObjectPath, FString(TEXT("Mesh")));
	TestEqual(TEXT("leaf property"), Target.LeafPropertyName, FString(TEXT("SkeletalMeshAsset")));
	TestEqual(TEXT("setter"), Target.SetterFunctionName, FString(TEXT("SetSkeletalMeshAsset")));
	TestEqual(TEXT("getter"), Target.GetterFunctionName, FString(TEXT("GetSkeletalMeshAsset")));
	TestNotNull(TEXT("setter function"), Target.SetterFunction);
	TestNotNull(TEXT("getter function"), Target.GetterFunction);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperClassDefaultSetterMutation_DirectPolicyStillRejectsTransient,
	"BlueprintHelper.ClassSettings.SetterAware.DirectPolicyStillRejectsTransient",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperClassDefaultSetterMutation_DirectPolicyStillRejectsTransient::RunTest(const FString& Parameters)
{
	const FProperty* Property = USkeletalMeshComponent::StaticClass()->FindPropertyByName(TEXT("SkeletalMeshAsset"));
	TestNotNull(TEXT("SkeletalMeshAsset property exists"), Property);
	if (!Property)
	{
		return false;
	}

	TestTrue(TEXT("SkeletalMeshAsset is transient"), Property->HasAnyPropertyFlags(CPF_Transient));
	TestFalse(TEXT("direct writer rejects transient"), FBlueprintHelperEditablePropertyPolicy::AllowsClassDefaultWrite(Property));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperClassDefaultSetterMutation_DirectMisuseSuggestsSetterRoute,
	"BlueprintHelper.ClassSettings.SetterAware.DirectMisuseSuggestsSetterRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperClassDefaultSetterMutation_DirectMisuseSuggestsSetterRoute::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperClassDefaultSetterMutationTestUtils::MakeCharacterBlueprint(TEXT("DirectMisuse"));
	TestNotNull(TEXT("Character Blueprint exists"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperClassDefaultPropertySetting Setting;
	Setting.PropertyPath = TEXT("Mesh.SkeletalMeshAsset");
	Setting.Value = MakeShared<FJsonValueString>(FBlueprintHelperClassDefaultSetterMutationTestUtils::MannyMeshPath());

	FBlueprintHelperGraphResolver GraphResolver;
	const FBlueprintHelperClassSettingsService Service(GraphResolver);
	const FBlueprintHelperToolResultBase Result = Service.SetClassDefaultProperties(Blueprint->GetPathName(), { Setting }, true);
	TestFalse(TEXT("direct misuse fails"), Result.bOk);
	TestTrue(TEXT("error exists"), Result.Error.IsSet());
	if (!Result.Error.IsSet())
	{
		return false;
	}
	TestEqual(TEXT("error code"), Result.Error->Code, FString(TEXT("class_default_property_setter_required")));
	TestEqual(TEXT("safe next action"), Result.Error->SafeNextAction, FString(TEXT("use_suggested_route_and_rerun_preview")));
	TestTrue(TEXT("suggested route exists"), Result.Error->SuggestedRoute.IsSet());
	if (Result.Error->SuggestedRoute.IsSet())
	{
		TestEqual(TEXT("route id"), Result.Error->SuggestedRoute->RouteId, FString(TEXT("blueprint_class_settings.class_default_setter")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperClassDefaultSetterMutation_PreviewDoesNotInvokeSetter,
	"BlueprintHelper.ClassSettings.SetterAware.PreviewDoesNotInvokeSetter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperClassDefaultSetterMutation_PreviewDoesNotInvokeSetter::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperClassDefaultSetterMutationTestUtils::MakeCharacterBlueprint(TEXT("Preview"));
	TestNotNull(TEXT("Character Blueprint exists"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver GraphResolver;
	const FBlueprintHelperClassSettingsService Service(GraphResolver);
	const FBlueprintHelperToolResultBase Result = Service.SetClassDefaultProperties(
		Blueprint->GetPathName(),
		{ FBlueprintHelperClassDefaultSetterMutationTestUtils::MakeSetterSetting(
			FBlueprintHelperClassDefaultSetterMutationTestUtils::MannyMeshPath()) },
		true);

	TestTrue(TEXT("preview ok"), Result.bOk);
	TestEqual(TEXT("preview status"), Result.Status, EBlueprintHelperToolStatus::DryRun);
	TestFalse(TEXT("preview not modified"), Result.bModified);
	TestTrue(TEXT("result has data"), Result.Data.IsValid());
	if (!Result.Data.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* DefaultResult = nullptr;
	TestTrue(TEXT("default result present"), Result.Data->TryGetObjectField(TEXT("default_property_result"), DefaultResult));
	const TArray<TSharedPtr<FJsonValue>>* EvidenceArray = nullptr;
	TestTrue(TEXT("setter evidence present"), DefaultResult && (*DefaultResult)->TryGetArrayField(TEXT("setter_mutation_evidence"), EvidenceArray));
	TestTrue(TEXT("one setter evidence row"), EvidenceArray && EvidenceArray->Num() == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperClassDefaultSetterMutation_ExecuteInvokesSetterAndReadbackMatches,
	"BlueprintHelper.ClassSettings.SetterAware.ExecuteInvokesSetterAndReadbackMatches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperClassDefaultSetterMutation_ExecuteInvokesSetterAndReadbackMatches::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperClassDefaultSetterMutationTestUtils::MakeCharacterBlueprint(TEXT("Execute"));
	TestNotNull(TEXT("Character Blueprint exists"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	const FString MeshPath = FBlueprintHelperClassDefaultSetterMutationTestUtils::MannyMeshPath();
	FBlueprintHelperGraphResolver GraphResolver;
	const FBlueprintHelperClassSettingsService Service(GraphResolver);
	const FBlueprintHelperToolResultBase Result = Service.SetClassDefaultProperties(
		Blueprint->GetPathName(),
		{ FBlueprintHelperClassDefaultSetterMutationTestUtils::MakeSetterSetting(MeshPath) },
		false);

	TestTrue(TEXT("execute ok"), Result.bOk);
	TestTrue(TEXT("execute marks modified"), Result.bModified);
	TestTrue(TEXT("result has data"), Result.Data.IsValid());
	if (!Result.Data.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* DefaultResult = nullptr;
	TestTrue(TEXT("default result present"), Result.Data->TryGetObjectField(TEXT("default_property_result"), DefaultResult));
	const TArray<TSharedPtr<FJsonValue>>* EvidenceArray = nullptr;
	TestTrue(TEXT("setter evidence present"), DefaultResult && (*DefaultResult)->TryGetArrayField(TEXT("setter_mutation_evidence"), EvidenceArray));
	TestTrue(TEXT("one setter evidence row"), EvidenceArray && EvidenceArray->Num() == 1);
	if (!EvidenceArray || EvidenceArray->Num() == 0)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Evidence = (*EvidenceArray)[0]->AsObject();
	FString MutationStrategy;
	FString AfterValue;
	TestTrue(TEXT("strategy present"), Evidence.IsValid() && Evidence->TryGetStringField(TEXT("mutation_strategy"), MutationStrategy));
	TestEqual(TEXT("strategy"), MutationStrategy, FString(TEXT("setter_aware_property")));
	TestTrue(TEXT("after present"), Evidence->TryGetStringField(TEXT("after_value"), AfterValue));
	TestEqual(TEXT("after value"), AfterValue, MeshPath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperClassDefaultSetterMutation_RestoreAdapterAcceptsReviewSnapshot,
	"BlueprintHelper.ClassSettings.SetterAware.RestoreAdapterAcceptsReviewSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperClassDefaultSetterMutation_RestoreAdapterAcceptsReviewSnapshot::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperClassDefaultSetterMutationTestUtils::MakeCharacterBlueprint(TEXT("RestoreSnapshot"));
	TestNotNull(TEXT("Character Blueprint exists"), Blueprint);
	ACharacter* CDO = Cast<ACharacter>(FBlueprintHelperClassDefaultSetterMutationTestUtils::GetClassDefaultObject(Blueprint));
	TestNotNull(TEXT("Character CDO exists"), CDO);
	TestNotNull(TEXT("Mesh component exists"), CDO ? CDO->GetMesh() : nullptr);
	if (!Blueprint || !CDO || !CDO->GetMesh())
	{
		return false;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Blueprint->GetPathName();
	Target.Surface = EBlueprintHelperReviewSurface::Details;
	Target.TargetKind = TEXT("class_default_setter_property");
	Target.TargetSubKind = TEXT("setter_aware_property");
	Target.TargetKey = TEXT("class_default_setter_property:Mesh_SkeletalMeshAsset");
	Target.PropertyPath = TEXT("Mesh.SkeletalMeshAsset");
	Target.ComponentPath = TEXT("Mesh");
	Target.BeforeSnapshotJson =
		FBlueprintHelperClassDefaultSetterMutationTestUtils::MakeReviewTargetSnapshotJson(
			Blueprint->GetPathName(),
			TEXT("Mesh.SkeletalMeshAsset"),
			FBlueprintHelperClassDefaultSetterMutationTestUtils::MannyMeshExportText());

	FBlueprintHelperReviewVisibleChange Change;
	Change.AtomicTargets.Add(Target);

	const FBlueprintHelperClassDefaultSetterRestoreAdapter Adapter;
	const FBlueprintHelperReviewRestoreResult RestoreResult = Adapter.RestoreBeforeSnapshot(Change);
	TestTrue(TEXT("restore succeeds"), RestoreResult.bSucceeded);
	TestEqual(TEXT("restore status"), RestoreResult.NewStatus, EBlueprintHelperReviewChangeStatus::Rejected);

	USkeletalMesh* Mesh = CDO->GetMesh()->GetSkeletalMeshAsset();
	TestNotNull(TEXT("mesh restored"), Mesh);
	if (Mesh)
	{
		TestEqual(TEXT("mesh path"), Mesh->GetPathName(), FBlueprintHelperClassDefaultSetterMutationTestUtils::MannyMeshPath());
	}
	return true;
}

#endif
