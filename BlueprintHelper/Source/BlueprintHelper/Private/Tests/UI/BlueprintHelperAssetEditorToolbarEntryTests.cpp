#include "Entry/BlueprintHelper.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Editor.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "WidgetBlueprint.h"

class FBlueprintHelperAssetEditorToolbarEntryTestUtils
{
public:
	static constexpr const TCHAR* ToolbarEntryName = TEXT("OpenBlueprintHelperAssetEditorToolbar");

	static bool HasDefaultToolbarEntry()
	{
		if (UToolMenus* ToolMenus = UToolMenus::Get())
		{
			if (UToolMenu* DefaultToolbar = ToolMenus->FindMenu(TEXT("AssetEditor.DefaultToolBar")))
			{
				return DefaultToolbar->ContainsEntry(ToolbarEntryName);
			}
		}

		return false;
	}

	static UPackage* MakePackage(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelper/Automation/ToolbarEntry/%s_%s"),
			*Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		Package->SetDirtyFlag(false);
		return Package;
	}
};

class FBlueprintHelperVerifyAssetEditorToolbarEntryCommand : public IAutomationLatentCommand
{
public:
	FBlueprintHelperVerifyAssetEditorToolbarEntryCommand(
		FAutomationTestBase* InTest,
		UObject* InAsset,
		const FName InExpectedEditorName,
		const FString& InLabel)
		: Test(InTest)
		, Asset(InAsset)
		, ExpectedEditorName(InExpectedEditorName)
		, Label(InLabel)
		, StartTime(FPlatformTime::Seconds())
	{
	}

	virtual bool Update() override
	{
		UObject* AssetObject = Asset.Get();
		if (!AssetObject)
		{
			Test->AddError(FString::Printf(TEXT("%s asset was garbage collected before editor verification."), *Label));
			return true;
		}

		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor
			? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()
			: nullptr;
		if (!AssetEditorSubsystem)
		{
			Test->AddError(TEXT("Unable to resolve AssetEditorSubsystem."));
			return true;
		}

		if (!bOpenRequested)
		{
			AssetEditorSubsystem->OpenEditorForAsset(AssetObject);
			bOpenRequested = true;
		}

		IAssetEditorInstance* EditorInstance = AssetEditorSubsystem->FindEditorForAsset(AssetObject, false);
		if (EditorInstance)
		{
			Test->TestEqual(
				FString::Printf(TEXT("%s editor name"), *Label),
				EditorInstance->GetEditorName(),
				ExpectedEditorName);
			Test->TestTrue(
				FString::Printf(TEXT("%s can inherit BlueprintHelper asset editor toolbar entry"), *Label),
				FBlueprintHelperAssetEditorToolbarEntryTestUtils::HasDefaultToolbarEntry());
			EditorInstance->CloseWindow(EAssetEditorCloseReason::AssetEditorHostClosed);
			return true;
		}

		if (FPlatformTime::Seconds() - StartTime > 10.0)
		{
			Test->AddError(FString::Printf(TEXT("%s editor was not opened within the timeout."), *Label));
			return true;
		}

		return false;
	}

private:
	FAutomationTestBase* Test = nullptr;
	TStrongObjectPtr<UObject> Asset;
	FName ExpectedEditorName;
	FString Label;
	double StartTime = 0.0;
	bool bOpenRequested = false;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetEditorToolbarEntryTest,
	"BlueprintHelper.Entry.AssetEditorToolbar.EntryRegisteredOnDefaultToolbar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetEditorToolbarEntryTest::RunTest(const FString& Parameters)
{
	FModuleManager::LoadModuleChecked<FBlueprintHelperModule>(TEXT("BlueprintHelper"));

	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!TestNotNull(TEXT("ToolMenus"), ToolMenus))
	{
		return false;
	}

	UToolMenu* DefaultToolbar = ToolMenus->FindMenu(TEXT("AssetEditor.DefaultToolBar"));
	if (!TestNotNull(TEXT("AssetEditor.DefaultToolBar"), DefaultToolbar))
	{
		return false;
	}

	TestTrue(
		TEXT("BlueprintHelper entry is registered on the default Asset Editor toolbar"),
		DefaultToolbar->ContainsEntry(FBlueprintHelperAssetEditorToolbarEntryTestUtils::ToolbarEntryName));

	if (UToolMenu* BlueprintToolbar = ToolMenus->FindMenu(TEXT("AssetEditor.BlueprintEditor.ToolBar")))
	{
		TestFalse(
			TEXT("BlueprintHelper entry is no longer registered only on the Blueprint Editor toolbar"),
			BlueprintToolbar->ContainsEntry(TEXT("OpenBlueprintHelperToolbar")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetEditorToolbarEntryE2ETest,
	"BlueprintHelper.Entry.AssetEditorToolbar.EntryAvailableForCommonAssetEditors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetEditorToolbarEntryE2ETest::RunTest(const FString& Parameters)
{
	FModuleManager::LoadModuleChecked<FBlueprintHelperModule>(TEXT("BlueprintHelper"));

	if (!TestTrue(
		TEXT("BlueprintHelper entry is registered on the default Asset Editor toolbar before opening asset editors"),
		FBlueprintHelperAssetEditorToolbarEntryTestUtils::HasDefaultToolbarEntry()))
	{
		return false;
	}

	UPackage* WidgetPackage = FBlueprintHelperAssetEditorToolbarEntryTestUtils::MakePackage(TEXT("WBP"));
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
		UUserWidget::StaticClass(),
		WidgetPackage,
		TEXT("WBP_BlueprintHelperToolbarEntry"),
		BPTYPE_Normal,
		UWidgetBlueprint::StaticClass(),
		UWidgetBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperToolbarEntryTest")));
	TestNotNull(TEXT("Widget Blueprint test asset"), WidgetBlueprint);

	UPackage* DataTablePackage = FBlueprintHelperAssetEditorToolbarEntryTestUtils::MakePackage(TEXT("DT"));
	UDataTable* DataTable = NewObject<UDataTable>(
		DataTablePackage,
		TEXT("DT_BlueprintHelperToolbarEntry"),
		RF_Public | RF_Standalone | RF_Transactional);
	DataTable->RowStruct = FTableRowBase::StaticStruct();
	TestNotNull(TEXT("DataTable test asset"), DataTable);

	UPackage* DataAssetPackage = FBlueprintHelperAssetEditorToolbarEntryTestUtils::MakePackage(TEXT("DA"));
	UPrimaryDataAsset* DataAsset = NewObject<UPrimaryDataAsset>(
		DataAssetPackage,
		TEXT("DA_BlueprintHelperToolbarEntry"),
		RF_Public | RF_Standalone | RF_Transactional);
	TestNotNull(TEXT("DataAsset test asset"), DataAsset);

	WidgetPackage->SetDirtyFlag(false);
	DataTablePackage->SetDirtyFlag(false);
	DataAssetPackage->SetDirtyFlag(false);

	ADD_LATENT_AUTOMATION_COMMAND(FBlueprintHelperVerifyAssetEditorToolbarEntryCommand(
		this,
		WidgetBlueprint,
		TEXT("WidgetBlueprintEditor"),
		TEXT("Widget Blueprint")));
	ADD_LATENT_AUTOMATION_COMMAND(FBlueprintHelperVerifyAssetEditorToolbarEntryCommand(
		this,
		DataTable,
		TEXT("DataTableEditor"),
		TEXT("DataTable")));
	ADD_LATENT_AUTOMATION_COMMAND(FBlueprintHelperVerifyAssetEditorToolbarEntryCommand(
		this,
		DataAsset,
		TEXT("GenericAssetEditor"),
		TEXT("DataAsset")));

	return true;
}
