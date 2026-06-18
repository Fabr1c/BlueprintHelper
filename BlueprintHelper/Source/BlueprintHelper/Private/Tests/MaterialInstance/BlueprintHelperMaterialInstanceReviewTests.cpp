#if WITH_DEV_AUTOMATION_TESTS

#include "Runtime/TaskRuntime/Clusters/MaterialInstance/BlueprintHelperMaterialInstanceReviewEvidenceBuilder.h"
#include "Runtime/TaskRuntime/Clusters/MaterialInstance/BlueprintHelperMaterialInstanceTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Review/BlueprintHelperTaskRuntimeReviewEvidenceBuilderRegistry.h"
#include "Systems/Review/BlueprintHelperReviewAdapterRegistry.h"
#include "Systems/Review/BlueprintHelperReviewMaterialInstanceEvidenceAdapter.h"
#include "Systems/Review/BlueprintHelperReviewMaterialInstanceRestoreAdapter.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceReadContextProjection.h"
#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceResolver.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "UI/Review/BlueprintHelperReviewDebugProjectionRegistry.h"
#include "UI/Review/BlueprintHelperReviewSurfaceProjectionRegistry.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"

class FBlueprintHelperMaterialInstanceReviewTestUtils
{
public:
	static FString MakeAssetName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static FString MakePackagePath(const FString& Prefix)
	{
		return FString::Printf(
			TEXT("/Game/BlueprintHelperMaterialInstanceReviewTests/%s"),
			*MakeAssetName(Prefix));
	}

	static FString GetLeafName(const FString& PackagePath)
	{
		int32 SlashIndex = INDEX_NONE;
		if (PackagePath.FindLastChar(TEXT('/'), SlashIndex))
		{
			return PackagePath.Mid(SlashIndex + 1);
		}
		return PackagePath;
	}

	static UMaterial* MakeParentMaterial(UPackage*& OutPackage, float ScalarDefault)
	{
		const FString PackagePath = MakePackagePath(TEXT("M_BH_MI_ReviewParent"));
		OutPackage = CreatePackage(*PackagePath);
		if (!OutPackage)
		{
			return nullptr;
		}

		UMaterial* Material = NewObject<UMaterial>(
			OutPackage,
			UMaterial::StaticClass(),
			*GetLeafName(PackagePath),
			RF_Public | RF_Standalone | RF_Transactional);
		if (!Material)
		{
			return nullptr;
		}

		UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material,
				UMaterialExpressionScalarParameter::StaticClass()));
		if (Scalar)
		{
			Scalar->ParameterName = TEXT("BH_ReviewScalar");
			Scalar->DefaultValue = ScalarDefault;
		}

		UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material,
				UMaterialExpressionVectorParameter::StaticClass()));
		if (Vector)
		{
			Vector->ParameterName = TEXT("BH_ReviewVector");
			Vector->DefaultValue = FLinearColor(0.1f, 0.2f, 0.3f, 1.0f);
		}

		UMaterialExpressionTextureSampleParameter2D* Texture = Cast<UMaterialExpressionTextureSampleParameter2D>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material,
				UMaterialExpressionTextureSampleParameter2D::StaticClass()));
		if (Texture)
		{
			Texture->ParameterName = TEXT("BH_ReviewTexture");
		}

		UMaterialExpressionStaticSwitchParameter* StaticSwitch = Cast<UMaterialExpressionStaticSwitchParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material,
				UMaterialExpressionStaticSwitchParameter::StaticClass()));
		if (StaticSwitch)
		{
			StaticSwitch->ParameterName = TEXT("BH_ReviewSwitch");
			StaticSwitch->DefaultValue = true;
		}

		Material->UpdateCachedExpressionData();
		Material->PostEditChange();
		OutPackage->SetDirtyFlag(false);
		return Material;
	}

	static UMaterialInstanceConstant* MakeInstance(
		const FString& PackagePath,
		UMaterialInterface* Parent)
	{
		UPackage* Package = CreatePackage(*PackagePath);
		if (!Package)
		{
			return nullptr;
		}

		UMaterialInstanceConstant* Instance = NewObject<UMaterialInstanceConstant>(
			Package,
			UMaterialInstanceConstant::StaticClass(),
			*GetLeafName(PackagePath),
			RF_Public | RF_Standalone | RF_Transactional);
		if (!Instance)
		{
			return nullptr;
		}
		if (Parent)
		{
			Instance->SetParentEditorOnly(Parent);
			UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
		}
		Package->SetDirtyFlag(false);
		return Instance;
	}

	static TSharedRef<FJsonObject> MakeParameterSnapshot(
		const FString& AssetPath,
		const FString& ParameterName,
		const FString& ParameterType,
		bool bHasOverride,
		double ScalarValue,
		const FString& Source)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("target_kind"), TEXT("material_instance_parameter"));
		Json->SetStringField(TEXT("asset_path"), AssetPath);
		Json->SetStringField(TEXT("parameter_name"), ParameterName);
		Json->SetStringField(TEXT("parameter_type"), ParameterType);
		Json->SetBoolField(TEXT("has_override"), bHasOverride);
		Json->SetStringField(TEXT("override_state"), bHasOverride ? TEXT("override") : TEXT("inherited"));
		Json->SetStringField(TEXT("source"), Source);
		Json->SetStringField(TEXT("effective_value"), FString::SanitizeFloat(ScalarValue));
		Json->SetStringField(TEXT("override_value"), bHasOverride ? FString::SanitizeFloat(ScalarValue) : TEXT("<unset>"));
		Json->SetNumberField(TEXT("scalar"), ScalarValue);
		return Json;
	}

	static TSharedRef<FJsonObject> MakeOp(const FString& Op)
	{
		TSharedRef<FJsonObject> OpObject = MakeShared<FJsonObject>();
		OpObject->SetStringField(TEXT("op"), Op);
		return OpObject;
	}

	static TSharedRef<FJsonObject> MakeScalarOp(const FString& Op, double Value)
	{
		TSharedRef<FJsonObject> OpObject = MakeOp(Op);
		OpObject->SetStringField(TEXT("parameter_name"), TEXT("BH_ReviewScalar"));
		OpObject->SetStringField(TEXT("parameter_type"), TEXT("scalar"));
		OpObject->SetNumberField(TEXT("value"), Value);
		return OpObject;
	}

	static TSharedPtr<FJsonObject> ParseJsonObject(const FString& JsonText)
	{
		TSharedPtr<FJsonObject> Json;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		FJsonSerializer::Deserialize(Reader, Json);
		return Json;
	}

	static FString ToJsonString(const TSharedRef<FJsonObject>& Json)
	{
		FString JsonText;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
		FJsonSerializer::Serialize(Json, Writer);
		return JsonText;
	}

	static int32 CountTargetsByKind(
		const FBlueprintHelperWriteReviewEvidence& Evidence,
		const FString& TargetKind)
	{
		int32 Count = 0;
		for (const FBlueprintHelperReviewAtomicTarget& Target : Evidence.AtomicTargets)
		{
			if (Target.TargetKind == TargetKind)
			{
				++Count;
			}
		}
		return Count;
	}

	static const FBlueprintHelperReviewAtomicTarget* FindTargetByKind(
		const FBlueprintHelperWriteReviewEvidence& Evidence,
		const FString& TargetKind)
	{
		for (const FBlueprintHelperReviewAtomicTarget& Target : Evidence.AtomicTargets)
		{
			if (Target.TargetKind == TargetKind)
			{
				return &Target;
			}
		}
		return nullptr;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialInstanceReviewEvidenceBuilderTest,
	"BlueprintHelper.MaterialInstance.Review.EvidenceBuilder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialInstanceReviewEvidenceBuilderTest::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/MI_BH_ReviewEvidence"));

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	LoweredStep.Capability = TEXT("material_instance");
	LoweredStep.RuntimeOperation = TEXT("material_instance");
	LoweredStep.AdapterOperation = TEXT("material_instance_edit");
	LoweredStep.Payload = Payload;

	TSharedRef<FJsonObject> CreateResult = MakeShared<FJsonObject>();
	CreateResult->SetStringField(TEXT("op"), TEXT("create_material_instance"));
	CreateResult->SetStringField(TEXT("status"), TEXT("applied"));
	CreateResult->SetStringField(TEXT("asset_path"), TEXT("/Game/MI_BH_ReviewEvidence"));
	CreateResult->SetStringField(TEXT("parent_material"), TEXT("/Game/M_BH_Parent"));
	CreateResult->SetStringField(TEXT("object_path"), TEXT("/Game/MI_BH_ReviewEvidence.MI_BH_ReviewEvidence"));

	TSharedRef<FJsonObject> ParentResult = MakeShared<FJsonObject>();
	ParentResult->SetStringField(TEXT("op"), TEXT("set_parent"));
	ParentResult->SetStringField(TEXT("status"), TEXT("applied"));
	ParentResult->SetStringField(TEXT("before_parent_material"), TEXT("/Game/M_BH_Parent_A.M_BH_Parent_A"));
	ParentResult->SetStringField(TEXT("after_parent_material"), TEXT("/Game/M_BH_Parent_B.M_BH_Parent_B"));

	TSharedRef<FJsonObject> SetResult = MakeShared<FJsonObject>();
	SetResult->SetStringField(TEXT("op"), TEXT("set_scalar_override"));
	SetResult->SetStringField(TEXT("status"), TEXT("applied"));
	SetResult->SetObjectField(
		TEXT("before"),
		FBlueprintHelperMaterialInstanceReviewTestUtils::MakeParameterSnapshot(
			TEXT("/Game/MI_BH_ReviewEvidence"),
			TEXT("BH_ReviewScalar"),
			TEXT("scalar"),
			false,
			1.0,
			TEXT("inherited")));
	SetResult->SetObjectField(
		TEXT("after"),
		FBlueprintHelperMaterialInstanceReviewTestUtils::MakeParameterSnapshot(
			TEXT("/Game/MI_BH_ReviewEvidence"),
			TEXT("BH_ReviewScalar"),
			TEXT("scalar"),
			true,
			3.5,
			TEXT("override")));

	TSharedRef<FJsonObject> ClearResult = MakeShared<FJsonObject>();
	ClearResult->SetStringField(TEXT("op"), TEXT("clear_override"));
	ClearResult->SetStringField(TEXT("status"), TEXT("applied"));
	ClearResult->SetObjectField(
		TEXT("before"),
		FBlueprintHelperMaterialInstanceReviewTestUtils::MakeParameterSnapshot(
			TEXT("/Game/MI_BH_ReviewEvidence"),
			TEXT("BH_ReviewScalar"),
			TEXT("scalar"),
			true,
			2.25,
			TEXT("override")));
	ClearResult->SetObjectField(
		TEXT("after"),
		FBlueprintHelperMaterialInstanceReviewTestUtils::MakeParameterSnapshot(
			TEXT("/Game/MI_BH_ReviewEvidence"),
			TEXT("BH_ReviewScalar"),
			TEXT("scalar"),
			false,
			1.0,
			TEXT("inherited")));

	TArray<TSharedPtr<FJsonValue>> Operations;
	Operations.Add(MakeShared<FJsonValueObject>(CreateResult));
	Operations.Add(MakeShared<FJsonValueObject>(ParentResult));
	Operations.Add(MakeShared<FJsonValueObject>(SetResult));
	Operations.Add(MakeShared<FJsonValueObject>(ClearResult));

	FBlueprintHelperToolResultBase StepResult;
	StepResult.bOk = true;
	StepResult.Status = EBlueprintHelperToolStatus::Applied;
	StepResult.Data = MakeShared<FJsonObject>();
	StepResult.Data->SetStringField(TEXT("asset_path"), TEXT("/Game/MI_BH_ReviewEvidence"));
	StepResult.Data->SetArrayField(TEXT("operations"), Operations);

	FBlueprintHelperWriteReviewEvidence Evidence;
	const bool bBuilt = FBlueprintHelperMaterialInstanceReviewEvidenceBuilder::BuildEvidence(
		LoweredStep,
		StepResult,
		TEXT("archive_mi_review"),
		TEXT("task_mi_review"),
		4,
		Evidence);

	TestTrue(TEXT("MaterialInstance evidence builds"), bBuilt);
	TestEqual(TEXT("asset creation target emitted"),
		FBlueprintHelperMaterialInstanceReviewTestUtils::CountTargetsByKind(Evidence, TEXT("asset_factory")),
		1);
	TestEqual(TEXT("parent target emitted"),
		FBlueprintHelperMaterialInstanceReviewTestUtils::CountTargetsByKind(Evidence, TEXT("material_instance")),
		1);
	TestEqual(TEXT("two parameter targets emitted"),
		FBlueprintHelperMaterialInstanceReviewTestUtils::CountTargetsByKind(Evidence, TEXT("material_instance_parameter")),
		2);
	TestTrue(
		TEXT("material_instance target kind supports snapshot restore"),
		FBlueprintHelperReviewTargetKindRegistry::SupportsSnapshotRestore(TEXT("material_instance")));
	TestTrue(
		TEXT("material_instance_parameter target kind supports snapshot restore"),
		FBlueprintHelperReviewTargetKindRegistry::SupportsSnapshotRestore(TEXT("material_instance_parameter")));

	FBlueprintHelperReviewEvidenceInput AdapterInput;
	AdapterInput.EvidenceId = TEXT("material_instance_adapter_evidence");
	AdapterInput.AssetPath = TEXT("/Game/MI_BH_ReviewEvidence");
	AdapterInput.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	FBlueprintHelperReviewAtomicTarget AdapterTarget;
	AdapterTarget.AssetPath = AdapterInput.AssetPath;
	AdapterTarget.Surface = EBlueprintHelperReviewSurface::Material;
	AdapterTarget.TargetKind = TEXT("material_instance_parameter");
	AdapterTarget.TargetKey = TEXT("material_instance_parameter:scalar:BH_ReviewScalar");
	AdapterTarget.DisplayLabel = TEXT("BH_ReviewScalar (scalar)");
	AdapterTarget.BeforeSnapshotJson = TEXT("{\"target_kind\":\"material_instance_parameter\",\"parameter_name\":\"BH_ReviewScalar\",\"parameter_type\":\"scalar\",\"effective_value\":\"1.0\"}");
	AdapterTarget.AfterSnapshotJson = TEXT("{\"target_kind\":\"material_instance_parameter\",\"parameter_name\":\"BH_ReviewScalar\",\"parameter_type\":\"scalar\",\"effective_value\":\"3.5\"}");
	AdapterInput.AtomicTargets.Add(AdapterTarget);
	const FBlueprintHelperReviewEvidenceBuildResult AdapterResult =
		FBlueprintHelperReviewMaterialInstanceEvidenceAdapter(TEXT("material_instance_parameter")).BuildEvidence(AdapterInput);
	TestTrue(TEXT("MaterialInstance evidence adapter builds"), AdapterResult.bSucceeded);
	TestEqual(TEXT("adapter keeps one target"), AdapterResult.Evidence.AtomicTargets.Num(), 1);
	if (AdapterResult.Evidence.AtomicTargets.Num() == 1)
	{
		TestEqual(
			TEXT("adapter fills missing MaterialInstance visual group from target key"),
			AdapterResult.Evidence.AtomicTargets[0].VisualGroupKey,
			AdapterTarget.TargetKey);
	}

	for (const FBlueprintHelperReviewAtomicTarget& Target : Evidence.AtomicTargets)
	{
		TestFalse(
			FString::Printf(TEXT("%s carries a visible change group"), *Target.TargetKey),
			Target.VisualGroupKey.IsEmpty());
		TestEqual(
			FString::Printf(TEXT("%s groups by target key"), *Target.TargetKey),
			Target.VisualGroupKey,
			Target.TargetKey);
	}

	const FBlueprintHelperReviewAtomicTarget* ParameterTarget =
		FBlueprintHelperMaterialInstanceReviewTestUtils::FindTargetByKind(Evidence, TEXT("material_instance_parameter"));
	TestNotNull(TEXT("parameter target exists"), ParameterTarget);
	if (ParameterTarget)
	{
		TestEqual(TEXT("parameter routes to material surface"), ParameterTarget->Surface, EBlueprintHelperReviewSurface::Material);
		TestTrue(TEXT("parameter target carries target identity"),
			ParameterTarget->TargetKey.Contains(TEXT("material_instance_parameter:scalar:BH_ReviewScalar")));
		TestTrue(TEXT("parameter target carries before snapshot"), !ParameterTarget->BeforeSnapshotJson.IsEmpty());
		TestTrue(TEXT("parameter target carries after snapshot"), !ParameterTarget->AfterSnapshotJson.IsEmpty());
		TestTrue(TEXT("changed properties include effective value"),
			ParameterTarget->ChangedPropertiesJson.Contains(TEXT("effective_value")));
		TestTrue(TEXT("changed properties include source"),
			ParameterTarget->ChangedPropertiesJson.Contains(TEXT("source")));
	}

	FBlueprintHelperReviewStoreService StoreService;
	const TArray<FBlueprintHelperReviewRecord> Records =
		StoreService.BuildReviewRecordsFromEvidence({ Evidence });
	TestEqual(TEXT("one MaterialInstance review record is built"), Records.Num(), 1);
	if (Records.Num() == 1)
	{
		const FBlueprintHelperReviewVisibleChange* AssetRoot = nullptr;
		for (const FBlueprintHelperReviewVisibleChange& Change : Records[0].VisibleChanges)
		{
			if (Change.AtomicTargets.ContainsByPredicate([](const FBlueprintHelperReviewAtomicTarget& Target)
				{
					return Target.TargetKind == TEXT("asset_factory");
				}))
			{
				AssetRoot = &Change;
				break;
			}
		}

		TestNotNull(TEXT("asset factory visible change exists"), AssetRoot);
		if (AssetRoot)
		{
			TestTrue(TEXT("asset factory visible change is lifecycle root"), AssetRoot->bIsAssetLifecycleRoot);
			TestTrue(TEXT("asset factory root rejects children"), AssetRoot->bRejectRemovesChildren);
			TestTrue(TEXT("asset factory root has no parent"), AssetRoot->ParentChangeId.IsEmpty());
			for (const FBlueprintHelperReviewVisibleChange& Change : Records[0].VisibleChanges)
			{
				if (Change.ChangeId == AssetRoot->ChangeId)
				{
					continue;
				}
				TestEqual(
					FString::Printf(TEXT("%s links to asset lifecycle root"), *Change.DisplayLabel),
					Change.ParentChangeId,
					AssetRoot->ChangeId);
			}
		}
	}
	return bBuilt && Evidence.AtomicTargets.Num() == 4;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialInstanceRestoreAdapterTest,
	"BlueprintHelper.MaterialInstance.Review.RestoreAdapter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialInstanceRestoreAdapterTest::RunTest(const FString& Parameters)
{
	UPackage* ParentPackageA = nullptr;
	UPackage* ParentPackageB = nullptr;
	UMaterial* ParentA =
		FBlueprintHelperMaterialInstanceReviewTestUtils::MakeParentMaterial(ParentPackageA, 1.25f);
	UMaterial* ParentB =
		FBlueprintHelperMaterialInstanceReviewTestUtils::MakeParentMaterial(ParentPackageB, 4.0f);
	TestNotNull(TEXT("parent A exists"), ParentA);
	TestNotNull(TEXT("parent B exists"), ParentB);
	if (!ParentA || !ParentB)
	{
		return false;
	}

	const FString InstancePath =
		FBlueprintHelperMaterialInstanceReviewTestUtils::MakePackagePath(TEXT("MI_BH_Restore"));
	UMaterialInstanceConstant* Instance =
		FBlueprintHelperMaterialInstanceReviewTestUtils::MakeInstance(InstancePath, ParentA);
	TestNotNull(TEXT("material instance exists"), Instance);
	if (!Instance)
	{
		return false;
	}

	Instance->SetParentEditorOnly(ParentB);
	UMaterialEditingLibrary::UpdateMaterialInstance(Instance);

	FBlueprintHelperReviewVisibleChange ParentChange;
	ParentChange.AssetPath = InstancePath;
	FBlueprintHelperReviewAtomicTarget ParentTarget;
	ParentTarget.AssetPath = InstancePath;
	ParentTarget.TargetKind = TEXT("material_instance");
	ParentTarget.TargetKey = TEXT("material_instance:parent");
	ParentTarget.BeforeParent = ParentA->GetPathName();
	ParentTarget.AfterParent = ParentB->GetPathName();
	TSharedRef<FJsonObject> ParentBeforeSnapshot = MakeShared<FJsonObject>();
	ParentBeforeSnapshot->SetStringField(TEXT("target_kind"), TEXT("material_instance"));
	ParentBeforeSnapshot->SetStringField(TEXT("asset_path"), InstancePath);
	ParentBeforeSnapshot->SetStringField(TEXT("parent_material"), ParentA->GetPathName());
	ParentBeforeSnapshot->SetStringField(TEXT("before_parent_material"), ParentA->GetPathName());
	ParentTarget.BeforeSnapshotJson =
		FBlueprintHelperMaterialInstanceReviewTestUtils::ToJsonString(ParentBeforeSnapshot);
	ParentChange.AtomicTargets.Add(ParentTarget);

	const FBlueprintHelperReviewMaterialInstanceRestoreAdapter ParentRestoreAdapter(TEXT("material_instance"));
	const FBlueprintHelperReviewRestoreResult ParentRestoreResult =
		ParentRestoreAdapter.RestoreBeforeSnapshot(ParentChange);
	TestTrue(TEXT("parent restore succeeds"), ParentRestoreResult.bSucceeded);
	TestEqual(TEXT("parent restored to before"), Instance->Parent.Get(), static_cast<UMaterialInterface*>(ParentA));

	Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("BH_ReviewScalar")), 9.0f);
	UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
	FBlueprintHelperReviewVisibleChange NewOverrideChange;
	NewOverrideChange.AssetPath = InstancePath;
	FBlueprintHelperReviewAtomicTarget NewOverrideTarget;
	NewOverrideTarget.AssetPath = InstancePath;
	NewOverrideTarget.TargetKind = TEXT("material_instance_parameter");
	NewOverrideTarget.TargetKey = TEXT("material_instance_parameter:scalar:BH_ReviewScalar");
	NewOverrideTarget.BeforeSnapshotJson =
		FBlueprintHelperMaterialInstanceReviewTestUtils::ToJsonString(
			FBlueprintHelperMaterialInstanceReviewTestUtils::MakeParameterSnapshot(
				InstancePath,
				TEXT("BH_ReviewScalar"),
				TEXT("scalar"),
				false,
				1.25,
				TEXT("inherited")));
	NewOverrideChange.AtomicTargets.Add(NewOverrideTarget);

	const FBlueprintHelperReviewMaterialInstanceRestoreAdapter ParameterRestoreAdapter(TEXT("material_instance_parameter"));
	const FBlueprintHelperReviewRestoreResult NewOverrideRestoreResult =
		ParameterRestoreAdapter.RestoreBeforeSnapshot(NewOverrideChange);
	TestTrue(TEXT("new override restore succeeds"), NewOverrideRestoreResult.bSucceeded);
	const FBlueprintHelperMaterialInstanceParameterResolveResult ClearedParameter =
		FBlueprintHelperMaterialInstanceResolver::ResolveParameter(
			Instance,
			TEXT("BH_ReviewScalar"),
			EBlueprintHelperMaterialInstanceParameterType::Scalar);
	TestTrue(TEXT("cleared parameter resolves"), ClearedParameter.bSuccess);
	TestFalse(TEXT("new override reject clears override"), ClearedParameter.Parameter.bHasOverride);

	Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("BH_ReviewScalar")), 7.0f);
	UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
	FBlueprintHelperReviewVisibleChange ClearOverrideChange;
	ClearOverrideChange.AssetPath = InstancePath;
	FBlueprintHelperReviewAtomicTarget ClearOverrideTarget;
	ClearOverrideTarget.AssetPath = InstancePath;
	ClearOverrideTarget.TargetKind = TEXT("material_instance_parameter");
	ClearOverrideTarget.TargetKey = TEXT("material_instance_parameter:scalar:BH_ReviewScalar");
	ClearOverrideTarget.BeforeSnapshotJson =
		FBlueprintHelperMaterialInstanceReviewTestUtils::ToJsonString(
			FBlueprintHelperMaterialInstanceReviewTestUtils::MakeParameterSnapshot(
				InstancePath,
				TEXT("BH_ReviewScalar"),
				TEXT("scalar"),
				true,
				2.25,
				TEXT("override")));
	ClearOverrideChange.AtomicTargets.Add(ClearOverrideTarget);

	Instance->ScalarParameterValues.Empty();
	UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
	const FBlueprintHelperReviewRestoreResult ClearRestoreResult =
		ParameterRestoreAdapter.RestoreBeforeSnapshot(ClearOverrideChange);
	TestTrue(TEXT("clear override restore succeeds"), ClearRestoreResult.bSucceeded);
	const FBlueprintHelperMaterialInstanceParameterResolveResult RestoredParameter =
		FBlueprintHelperMaterialInstanceResolver::ResolveParameter(
			Instance,
			TEXT("BH_ReviewScalar"),
			EBlueprintHelperMaterialInstanceParameterType::Scalar);
	TestTrue(TEXT("restored parameter resolves"), RestoredParameter.bSuccess);
	TestTrue(TEXT("clear override reject restores override"), RestoredParameter.Parameter.bHasOverride);
	TestEqual(TEXT("restored override value"), RestoredParameter.Parameter.OverrideValue.Scalar, 2.25f);

	ParentPackageA->SetDirtyFlag(false);
	ParentPackageB->SetDirtyFlag(false);
	if (Instance->GetPackage())
	{
		Instance->GetPackage()->SetDirtyFlag(false);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialInstanceSurfaceProjectionAdapterTest,
	"BlueprintHelper.MaterialInstance.Surface.ProjectionAdapter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialInstanceSurfaceProjectionAdapterTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("material_instance_surface_change");
	Change.AssetPath = TEXT("/Game/MI_BH_Surface");
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	Change.Status = EBlueprintHelperReviewChangeStatus::Pending;
	Change.DisplayLabel = TEXT("BH_ReviewScalar");

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Change.AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::Material;
	Target.TargetKind = TEXT("material_instance_parameter");
	Target.TargetKey = TEXT("material_instance_parameter:scalar:BH_ReviewScalar");
	Target.DisplayLabel = TEXT("BH_ReviewScalar");
	Target.Status = EBlueprintHelperReviewChangeStatus::Pending;
	TSharedRef<FJsonObject> Before =
		FBlueprintHelperMaterialInstanceReviewTestUtils::MakeParameterSnapshot(
			Change.AssetPath,
			TEXT("BH_ReviewScalar"),
			TEXT("scalar"),
			false,
			1.25,
			TEXT("inherited"));
	TSharedRef<FJsonObject> After =
		FBlueprintHelperMaterialInstanceReviewTestUtils::MakeParameterSnapshot(
			Change.AssetPath,
			TEXT("BH_ReviewScalar"),
			TEXT("scalar"),
			true,
			3.5,
			TEXT("override"));
	Target.BeforeSnapshotJson = FBlueprintHelperMaterialInstanceReviewTestUtils::ToJsonString(Before);
	Target.AfterSnapshotJson = FBlueprintHelperMaterialInstanceReviewTestUtils::ToJsonString(After);
	TSharedRef<FJsonObject> Changed = MakeShared<FJsonObject>();
	Changed->SetObjectField(TEXT("before"), Before);
	Changed->SetObjectField(TEXT("after"), After);
	Target.ChangedPropertiesJson = FBlueprintHelperMaterialInstanceReviewTestUtils::ToJsonString(Changed);
	Change.AtomicTargets.Add(Target);

	const TSharedRef<FBlueprintHelperReviewSurfaceProjectionRegistry> Registry =
		FBlueprintHelperReviewSurfaceProjectionRegistry::CreateDefault();
	const TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel> Models =
		Registry->ProjectVisibleChange(Change, TEXT("material"), TEXT("material"));

	TestEqual(TEXT("one MaterialInstance surface diff model"), Models.Num(), 1);
	if (Models.Num() == 1)
	{
		TestEqual(TEXT("parameter name"), Models[0].ParameterName, FString(TEXT("BH_ReviewScalar")));
		TestEqual(TEXT("parameter type"), Models[0].ParameterType, FString(TEXT("scalar")));
		TestEqual(TEXT("before value"), Models[0].BeforeValue, FString(TEXT("1.25")));
		TestEqual(TEXT("after value"), Models[0].AfterValue, FString(TEXT("3.5")));
		TestEqual(TEXT("effective value"), Models[0].EffectiveValue, FString(TEXT("3.5")));
		TestEqual(TEXT("source"), Models[0].Source, FString(TEXT("override")));
		TestEqual(TEXT("override state"), Models[0].OverrideState, FString(TEXT("override")));
		TestTrue(TEXT("material instance model can reject"), Models[0].bCanReject);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialInstanceDebugProjectionAdapterTest,
	"BlueprintHelper.MaterialInstance.Debug.ProjectionAdapter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialInstanceDebugProjectionAdapterTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FBlueprintHelperReviewDebugProjectionRegistry> Registry =
		FBlueprintHelperReviewDebugProjectionRegistry::CreateDefault();

	const FString EventTypes[] = {
		TEXT("material_instance.preview"),
		TEXT("material_instance.mutation_plan"),
		TEXT("material_instance.mutation_result"),
		TEXT("material_instance.readback"),
		TEXT("material_instance.evidence.created"),
		TEXT("material_instance.restore.result"),
		TEXT("material_instance.surface.projected"),
	};

	for (const FString& EventType : EventTypes)
	{
		TSharedRef<FJsonObject> EventJson = MakeShared<FJsonObject>();
		EventJson->SetStringField(TEXT("event_type"), EventType);
		EventJson->SetStringField(TEXT("review_event_id"), TEXT("material_instance_review_event"));
		EventJson->SetStringField(TEXT("asset_path"), TEXT("/Game/MI_BH_Debug"));
		EventJson->SetStringField(TEXT("target_key"), TEXT("material_instance_parameter:scalar:BH_ReviewScalar"));
		EventJson->SetStringField(TEXT("surface_kind"), TEXT("material"));
		EventJson->SetStringField(TEXT("result"), TEXT("ok"));

		const TArray<FBlueprintHelperReviewDebugEventModel> Events = Registry->ProjectEvent(EventJson);
		TestEqual(FString::Printf(TEXT("%s projects"), *EventType), Events.Num(), 1);
		if (Events.Num() == 1)
		{
			TestEqual(TEXT("event type"), Events[0].EventType, EventType);
			TestEqual(TEXT("surface kind"), Events[0].SurfaceKind, FString(TEXT("material")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialInstanceReadContextProjectionTest,
	"BlueprintHelper.MaterialInstance.ReadContext.Projection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialInstanceReadContextProjectionTest::RunTest(const FString& Parameters)
{
	UPackage* ParentPackage = nullptr;
	UMaterial* ParentMaterial =
		FBlueprintHelperMaterialInstanceReviewTestUtils::MakeParentMaterial(ParentPackage, 1.25f);
	TestNotNull(TEXT("parent material fixture exists"), ParentMaterial);
	if (!ParentMaterial)
	{
		return false;
	}

	const FString InstancePath =
		FBlueprintHelperMaterialInstanceReviewTestUtils::MakePackagePath(TEXT("MI_BH_ReadContext"));
	UMaterialInstanceConstant* Instance =
		FBlueprintHelperMaterialInstanceReviewTestUtils::MakeInstance(InstancePath, ParentMaterial);
	TestNotNull(TEXT("material instance fixture exists"), Instance);
	if (!Instance)
	{
		return false;
	}
	Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("BH_ReviewScalar")), 3.5f);
	UMaterialEditingLibrary::UpdateMaterialInstance(Instance);

	TSharedPtr<FJsonObject> ContextJson;
	FString Error;
	TestTrue(
		TEXT("material instance read context builds"),
		FBlueprintHelperMaterialInstanceReadContextProjection::BuildReadContextJson(
			InstancePath,
			ContextJson,
			Error));
	TestTrue(TEXT("read context json is valid"), ContextJson.IsValid());
	if (!ContextJson.IsValid())
	{
		return false;
	}

	TestEqual(TEXT("schema"),
		ContextJson->GetStringField(TEXT("schema")),
		FString(TEXT("BlueprintHelper.MaterialInstanceReadContext.v1")));
	TestEqual(TEXT("asset path"), ContextJson->GetStringField(TEXT("asset_path")), InstancePath);
	TestEqual(TEXT("parent material"), ContextJson->GetStringField(TEXT("parent_material")), ParentMaterial->GetPathName());
	TestTrue(TEXT("scalar parameters array exists"), ContextJson->HasTypedField<EJson::Array>(TEXT("scalar_parameters")));
	TestTrue(TEXT("vector parameters array exists"), ContextJson->HasTypedField<EJson::Array>(TEXT("vector_parameters")));
	TestTrue(TEXT("texture parameters array exists"), ContextJson->HasTypedField<EJson::Array>(TEXT("texture_parameters")));
	TestTrue(TEXT("static switch parameters array exists"), ContextJson->HasTypedField<EJson::Array>(TEXT("static_switch_parameters")));

	const TArray<TSharedPtr<FJsonValue>>* Scalars = nullptr;
	TestTrue(TEXT("scalar parameters readable"), ContextJson->TryGetArrayField(TEXT("scalar_parameters"), Scalars) && Scalars);
	bool bFoundScalar = false;
	if (Scalars)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Scalars)
		{
			const TSharedPtr<FJsonObject> Parameter = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Parameter.IsValid())
			{
				continue;
			}
			FString ParameterName;
			Parameter->TryGetStringField(TEXT("parameter_name"), ParameterName);
			if (ParameterName == TEXT("BH_ReviewScalar"))
			{
				bFoundScalar = true;
				TestEqual(TEXT("scalar parameter type"),
					Parameter->GetStringField(TEXT("parameter_type")),
					FString(TEXT("scalar")));
				TestEqual(TEXT("scalar override state"),
					Parameter->GetStringField(TEXT("override_state")),
					FString(TEXT("override")));
				TestEqual(TEXT("scalar source"),
					Parameter->GetStringField(TEXT("source")),
					FString(TEXT("override")));
				TestEqual(TEXT("scalar effective value"),
					Parameter->GetStringField(TEXT("effective_value")),
					FString(TEXT("3.5")));
				TestTrue(TEXT("scalar carries override value"), Parameter->HasField(TEXT("override_value")));
			}
		}
	}
	TestTrue(TEXT("scalar parameter is included"), bFoundScalar);

	ParentPackage->SetDirtyFlag(false);
	if (Instance->GetPackage())
	{
		Instance->GetPackage()->SetDirtyFlag(false);
	}
	return bFoundScalar;
}

#endif
