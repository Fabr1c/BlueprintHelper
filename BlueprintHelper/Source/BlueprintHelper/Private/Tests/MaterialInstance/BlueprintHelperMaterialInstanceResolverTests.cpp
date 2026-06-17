#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceResolver.h"

#include "Engine/Texture2D.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

class FBlueprintHelperMaterialInstanceResolverTestUtils
{
public:
	static UPackage* MakePackage(const FString& Prefix)
	{
		const FString AssetName = FString::Printf(
			TEXT("%s_%s"),
			*Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		return CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperMaterialInstanceResolverTests/%s"),
			*AssetName));
	}

	static UMaterial* MakeParentMaterial(
		UPackage*& OutPackage,
		UTexture2D*& OutTexture)
	{
		OutPackage = MakePackage(TEXT("M_BH_MI_Parent"));
		if (!OutPackage)
		{
			return nullptr;
		}

		UMaterial* Material = NewObject<UMaterial>(
			OutPackage,
			UMaterial::StaticClass(),
			*OutPackage->GetName().RightChop(OutPackage->GetName().Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) + 1),
			RF_Public | RF_Standalone | RF_Transactional);
		if (!Material)
		{
			return nullptr;
		}

		UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material,
				UMaterialExpressionScalarParameter::StaticClass()));
		UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material,
				UMaterialExpressionVectorParameter::StaticClass()));
		UMaterialExpressionTextureSampleParameter2D* Texture = Cast<UMaterialExpressionTextureSampleParameter2D>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material,
				UMaterialExpressionTextureSampleParameter2D::StaticClass()));
		UMaterialExpressionStaticSwitchParameter* StaticSwitch = Cast<UMaterialExpressionStaticSwitchParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material,
				UMaterialExpressionStaticSwitchParameter::StaticClass()));

		if (!Scalar || !Vector || !Texture || !StaticSwitch)
		{
			return nullptr;
		}

		OutTexture = NewObject<UTexture2D>(
			OutPackage,
			UTexture2D::StaticClass(),
			TEXT("T_BH_MI_Texture"),
			RF_Public | RF_Standalone | RF_Transactional);
		if (!OutTexture)
		{
			return nullptr;
		}

		Scalar->ParameterName = TEXT("BH_Scalar");
		Scalar->DefaultValue = 1.25f;
		Vector->ParameterName = TEXT("BH_Vector");
		Vector->DefaultValue = FLinearColor(0.25f, 0.5f, 0.75f, 1.0f);
		Texture->ParameterName = TEXT("BH_Texture");
		Texture->Texture = OutTexture;
		Texture->AutoSetSampleType();
		StaticSwitch->ParameterName = TEXT("BH_StaticSwitch");
		StaticSwitch->DefaultValue = 1;

		Material->UpdateCachedExpressionData();
		Material->PostEditChange();
		OutPackage->SetDirtyFlag(false);
		return Material;
	}

	static UMaterialInstanceConstant* MakeMaterialInstance(
		UPackage*& OutPackage,
		UMaterialInterface* Parent,
		const FString& Prefix = TEXT("MI_BH_Resolver"))
	{
		OutPackage = MakePackage(Prefix);
		if (!OutPackage)
		{
			return nullptr;
		}

		const FString AssetName = OutPackage->GetName().RightChop(
			OutPackage->GetName().Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) + 1);
		UMaterialInstanceConstant* Instance = NewObject<UMaterialInstanceConstant>(
			OutPackage,
			UMaterialInstanceConstant::StaticClass(),
			*AssetName,
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
		OutPackage->SetDirtyFlag(false);
		return Instance;
	}

	static bool FindType(
		const TArray<FBlueprintHelperMaterialInstanceParameterSchemaEntry>& Schema,
		EBlueprintHelperMaterialInstanceParameterType Type,
		const FName Name)
	{
		for (const FBlueprintHelperMaterialInstanceParameterSchemaEntry& Entry : Schema)
		{
			if (Entry.Type == Type && Entry.ParameterInfo.Name.IsEqual(Name))
			{
				return true;
			}
		}
		return false;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialInstanceResolverRejectsMissingAssetTest,
	"BlueprintHelper.MaterialInstance.Resolver.MissingAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialInstanceResolverRejectsMissingAssetTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperMaterialInstanceAssetResolveResult Result =
		FBlueprintHelperMaterialInstanceResolver::ResolveAsset(
			TEXT("/Game/BlueprintHelperMaterialInstanceResolverTests/Missing_MI"));
	TestFalse(TEXT("missing asset is not resolved"), Result.bSuccess);
	TestEqual(
		TEXT("missing asset error code"),
		Result.ErrorCode,
		FString(TEXT("material_instance_asset_not_found")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialInstanceResolverRejectsNonMaterialInstanceTest,
	"BlueprintHelper.MaterialInstance.Resolver.NonMaterialInstance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialInstanceResolverRejectsNonMaterialInstanceTest::RunTest(const FString& Parameters)
{
	UPackage* Package = nullptr;
	UTexture2D* Texture = nullptr;
	UMaterial* Material = FBlueprintHelperMaterialInstanceResolverTestUtils::MakeParentMaterial(Package, Texture);
	TestNotNull(TEXT("material fixture is created"), Material);
	if (!Material)
	{
		return false;
	}

	const FBlueprintHelperMaterialInstanceAssetResolveResult Result =
		FBlueprintHelperMaterialInstanceResolver::ResolveAsset(Material->GetPathName());
	TestFalse(TEXT("non MIC asset is rejected"), Result.bSuccess);
	TestEqual(
		TEXT("non MIC error code"),
		Result.ErrorCode,
		FString(TEXT("material_instance_asset_type_mismatch")));
	Package->SetDirtyFlag(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialInstanceResolverReportsMissingParentTest,
	"BlueprintHelper.MaterialInstance.Resolver.MissingParent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialInstanceResolverReportsMissingParentTest::RunTest(const FString& Parameters)
{
	UPackage* InstancePackage = nullptr;
	UMaterialInstanceConstant* Instance =
		FBlueprintHelperMaterialInstanceResolverTestUtils::MakeMaterialInstance(
			InstancePackage,
			nullptr,
			TEXT("MI_BH_MissingParent"));
	TestNotNull(TEXT("material instance fixture is created"), Instance);
	if (!Instance)
	{
		return false;
	}

	const FBlueprintHelperMaterialInstanceAssetResolveResult AssetResult =
		FBlueprintHelperMaterialInstanceResolver::ResolveAsset(Instance->GetPathName());
	TestTrue(TEXT("asset itself resolves"), AssetResult.bSuccess);
	TestFalse(TEXT("parent is absent"), AssetResult.bHasParent);

	TArray<FBlueprintHelperMaterialInstanceParameterSchemaEntry> Schema;
	FString ErrorCode;
	FString ErrorMessage;
	TestFalse(
		TEXT("schema collection fails without parent"),
		FBlueprintHelperMaterialInstanceResolver::CollectParameterSchema(
			Instance,
			Schema,
			ErrorCode,
			ErrorMessage));
	TestEqual(
		TEXT("missing parent error code"),
		ErrorCode,
		FString(TEXT("material_instance_missing_parent")));
	InstancePackage->SetDirtyFlag(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialInstanceResolverDetectsParameterTypesTest,
	"BlueprintHelper.MaterialInstance.Resolver.ParameterTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialInstanceResolverDetectsParameterTypesTest::RunTest(const FString& Parameters)
{
	UPackage* MaterialPackage = nullptr;
	UTexture2D* Texture = nullptr;
	UMaterial* Material = FBlueprintHelperMaterialInstanceResolverTestUtils::MakeParentMaterial(
		MaterialPackage,
		Texture);
	TestNotNull(TEXT("parent material fixture is created"), Material);
	if (!Material)
	{
		return false;
	}

	UPackage* InstancePackage = nullptr;
	UMaterialInstanceConstant* Instance =
		FBlueprintHelperMaterialInstanceResolverTestUtils::MakeMaterialInstance(
			InstancePackage,
			Material);
	TestNotNull(TEXT("material instance fixture is created"), Instance);
	if (!Instance)
	{
		return false;
	}

	Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("BH_Scalar")), 2.5f);
	UMaterialEditingLibrary::UpdateMaterialInstance(Instance);

	TArray<FBlueprintHelperMaterialInstanceParameterSchemaEntry> Schema;
	FString ErrorCode;
	FString ErrorMessage;
	TestTrue(
		TEXT("schema collection succeeds"),
		FBlueprintHelperMaterialInstanceResolver::CollectParameterSchema(
			Instance,
			Schema,
			ErrorCode,
			ErrorMessage));
	if (!ErrorMessage.IsEmpty())
	{
		AddError(FString::Printf(TEXT("resolver error: %s"), *ErrorMessage));
	}

	TestTrue(
		TEXT("scalar parameter detected"),
		FBlueprintHelperMaterialInstanceResolverTestUtils::FindType(
			Schema,
			EBlueprintHelperMaterialInstanceParameterType::Scalar,
			TEXT("BH_Scalar")));
	TestTrue(
		TEXT("vector parameter detected"),
		FBlueprintHelperMaterialInstanceResolverTestUtils::FindType(
			Schema,
			EBlueprintHelperMaterialInstanceParameterType::Vector,
			TEXT("BH_Vector")));
	TestTrue(
		TEXT("texture parameter detected"),
		FBlueprintHelperMaterialInstanceResolverTestUtils::FindType(
			Schema,
			EBlueprintHelperMaterialInstanceParameterType::Texture,
			TEXT("BH_Texture")));
	TestTrue(
		TEXT("static switch parameter detected"),
		FBlueprintHelperMaterialInstanceResolverTestUtils::FindType(
			Schema,
			EBlueprintHelperMaterialInstanceParameterType::StaticSwitch,
			TEXT("BH_StaticSwitch")));

	const FBlueprintHelperMaterialInstanceParameterResolveResult ScalarResult =
		FBlueprintHelperMaterialInstanceResolver::ResolveParameter(
			Instance,
			TEXT("BH_Scalar"));
	TestTrue(TEXT("scalar resolves by inferred type"), ScalarResult.bSuccess);
	TestTrue(TEXT("scalar override state detected"), ScalarResult.Parameter.bHasOverride);
	TestEqual(
		TEXT("scalar source is override"),
		BlueprintHelperMaterialInstanceParameterSourceToString(ScalarResult.Parameter.Source),
		FString(TEXT("override")));
	TestEqual(TEXT("scalar effective value is override"), ScalarResult.Parameter.EffectiveValue.Scalar, 2.5f);

	const FBlueprintHelperMaterialInstanceParameterResolveResult TextureResult =
		FBlueprintHelperMaterialInstanceResolver::ResolveParameter(
			Instance,
			TEXT("BH_Texture"),
			EBlueprintHelperMaterialInstanceParameterType::Texture);
	TestTrue(TEXT("texture resolves explicitly"), TextureResult.bSuccess);
	TestEqual(
		TEXT("texture source is inherited"),
		BlueprintHelperMaterialInstanceParameterSourceToString(TextureResult.Parameter.Source),
		FString(TEXT("inherited")));
	TestEqual(
		TEXT("texture effective path matches fixture"),
		TextureResult.Parameter.EffectiveValue.TexturePath,
		Texture ? Texture->GetPathName() : FString());

	MaterialPackage->SetDirtyFlag(false);
	InstancePackage->SetDirtyFlag(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialInstanceResolverReportsMissingParameterTest,
	"BlueprintHelper.MaterialInstance.Resolver.MissingParameter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialInstanceResolverReportsMissingParameterTest::RunTest(const FString& Parameters)
{
	UPackage* MaterialPackage = nullptr;
	UTexture2D* Texture = nullptr;
	UMaterial* Material = FBlueprintHelperMaterialInstanceResolverTestUtils::MakeParentMaterial(
		MaterialPackage,
		Texture);
	TestNotNull(TEXT("parent material fixture is created"), Material);
	if (!Material)
	{
		return false;
	}

	UPackage* InstancePackage = nullptr;
	UMaterialInstanceConstant* Instance =
		FBlueprintHelperMaterialInstanceResolverTestUtils::MakeMaterialInstance(
			InstancePackage,
			Material,
			TEXT("MI_BH_MissingParameter"));
	TestNotNull(TEXT("material instance fixture is created"), Instance);
	if (!Instance)
	{
		return false;
	}

	const FBlueprintHelperMaterialInstanceParameterResolveResult Result =
		FBlueprintHelperMaterialInstanceResolver::ResolveParameter(
			Instance,
			TEXT("BH_NotThere"));
	TestFalse(TEXT("missing parameter is rejected"), Result.bSuccess);
	TestEqual(
		TEXT("missing parameter error code"),
		Result.ErrorCode,
		FString(TEXT("material_instance_parameter_not_found")));

	MaterialPackage->SetDirtyFlag(false);
	InstancePackage->SetDirtyFlag(false);
	return true;
}

#endif
