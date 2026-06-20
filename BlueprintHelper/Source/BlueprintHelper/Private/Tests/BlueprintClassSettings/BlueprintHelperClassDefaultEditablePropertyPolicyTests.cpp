#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Shared/BlueprintClassSettings/BlueprintHelperClassSettingsTypes.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "UObject/Package.h"

class FBlueprintHelperClassDefaultEditablePropertyPolicyTestUtils
{
public:
	static FString MakeUniqueName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UPackage* MakePackage(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperTests/ClassDefaultPolicy/%s"),
			*MakeUniqueName(Prefix)));
		Package->SetDirtyFlag(false);
		return Package;
	}

	static UBlueprint* MakeBlueprint(UClass* ParentClass, const FString& Prefix)
	{
		UPackage* Package = MakePackage(Prefix);
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			Package,
			*MakeUniqueName(TEXT("BP_ClassDefaultPolicy")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperClassDefaultPolicyTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperPropertyPolicyClassDefaultAllowsEditBlueprintReadOnlyTest,
	"BlueprintHelper.PropertyPolicy.ClassDefaultAllowsEditBlueprintReadOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperPropertyPolicyClassDefaultAllowsEditBlueprintReadOnlyTest::RunTest(const FString& Parameters)
{
	const FProperty* Property = AGameModeBase::StaticClass()->FindPropertyByName(
		GET_MEMBER_NAME_CHECKED(AGameModeBase, PlayerControllerClass));
	TestNotNull(TEXT("PlayerControllerClass property exists"), Property);
	if (!Property)
	{
		return false;
	}

	TestTrue(TEXT("PlayerControllerClass is editor editable"), Property->HasAnyPropertyFlags(CPF_Edit));
	TestTrue(TEXT("PlayerControllerClass is BlueprintReadOnly"), Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly));
	TestFalse(TEXT("PlayerControllerClass is not template edit disabled"), Property->HasAnyPropertyFlags(CPF_DisableEditOnTemplate));
	TestTrue(TEXT("class-default policy allows editor-editable BlueprintReadOnly property"), FBlueprintHelperEditablePropertyPolicy::AllowsClassDefaultWrite(Property));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperPropertyPolicyDefaultStillRejectsBlueprintReadOnlyTest,
	"BlueprintHelper.PropertyPolicy.DefaultPolicyStillRejectsBlueprintReadOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperPropertyPolicyDefaultStillRejectsBlueprintReadOnlyTest::RunTest(const FString& Parameters)
{
	const FProperty* Property = AGameModeBase::StaticClass()->FindPropertyByName(
		GET_MEMBER_NAME_CHECKED(AGameModeBase, PlayerControllerClass));
	TestNotNull(TEXT("PlayerControllerClass property exists"), Property);
	if (!Property)
	{
		return false;
	}

	TestFalse(TEXT("legacy default policy still rejects BlueprintReadOnly"), FBlueprintHelperEditablePropertyPolicy::AllowsWrite(Property));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperClassSettingsGameModePlayerControllerDryRunTest,
	"BlueprintHelper.Safety.ClassSettings.GameModePlayerControllerClassDryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperClassSettingsGameModePlayerControllerDryRunTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperClassDefaultEditablePropertyPolicyTestUtils::MakeBlueprint(
		AGameModeBase::StaticClass(),
		TEXT("GameModePlayerControllerDryRun"));
	TestNotNull(TEXT("GameMode Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperClassDefaultPropertySetting Setting;
	Setting.PropertyPath = TEXT("PlayerControllerClass");
	Setting.Value = MakeShared<FJsonValueString>(APlayerController::StaticClass()->GetPathName());

	FBlueprintHelperGraphResolver Resolver;
	const FBlueprintHelperClassSettingsService Service(Resolver);
	const FBlueprintHelperToolResultBase Result = Service.SetClassDefaultProperties(
		Blueprint->GetPathName(),
		{ Setting },
		true);

	TestTrue(TEXT("PlayerControllerClass dry-run succeeds"), Result.bOk);
	TestEqual(TEXT("dry-run status"), Result.Status, EBlueprintHelperToolStatus::DryRun);
	TestFalse(TEXT("dry-run does not mark modified"), Result.bModified);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperClassSettingsReadGameModePlayerControllerDefaultTest,
	"BlueprintHelper.ReadContext.ClassSettings.ReadGameModePlayerControllerDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperClassSettingsReadGameModePlayerControllerDefaultTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperClassDefaultEditablePropertyPolicyTestUtils::MakeBlueprint(
		AGameModeBase::StaticClass(),
		TEXT("ReadGameModePlayerControllerDefault"));
	TestNotNull(TEXT("GameMode Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	const FBlueprintHelperClassSettingsService Service(Resolver);
	const FBlueprintHelperToolResultBase Result = Service.ReadClassDefaultProperty(
		Blueprint->GetPathName(),
		TEXT("PlayerControllerClass"));

	TestTrue(TEXT("class default read succeeds"), Result.bOk);
	TestEqual(TEXT("read operation"), Result.Operation, FString(TEXT("read_blueprint_class_default_property")));
	TestTrue(TEXT("result has data"), Result.Data.IsValid());
	TestTrue(TEXT("result has target"), Result.Target.IsSet());
	if (!Result.Data.IsValid() || !Result.Target.IsSet())
	{
		return false;
	}

	FString Schema;
	TestTrue(TEXT("data carries schema"), Result.Data->TryGetStringField(TEXT("schema"), Schema));
	TestEqual(TEXT("class default read schema"), Schema, FString(TEXT("BlueprintClassDefaultPropertyContext.v1")));
	TestEqual(TEXT("target type is blueprint property"), Result.Target->TargetType, EBlueprintHelperTargetType::Property);
	TestEqual(TEXT("target property path"), Result.Target->PropertyPath, FString(TEXT("PlayerControllerClass")));

	FString OwnerRoot;
	TestTrue(TEXT("data carries owner_root"), Result.Data->TryGetStringField(TEXT("owner_root"), OwnerRoot));
	TestEqual(TEXT("owner_root is CDO"), OwnerRoot, FString(TEXT("blueprint_cdo")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperClassSettingsReadMissingClassDefaultReturnsStructuredErrorTest,
	"BlueprintHelper.ReadContext.ClassSettings.ReadMissingClassDefaultReturnsStructuredError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperClassSettingsReadMissingClassDefaultReturnsStructuredErrorTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperClassDefaultEditablePropertyPolicyTestUtils::MakeBlueprint(
		AGameModeBase::StaticClass(),
		TEXT("ReadMissingClassDefault"));
	TestNotNull(TEXT("GameMode Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	const FBlueprintHelperClassSettingsService Service(Resolver);
	const FBlueprintHelperToolResultBase Result = Service.ReadClassDefaultProperty(
		Blueprint->GetPathName(),
		TEXT("DoesNotExist"));

	TestFalse(TEXT("missing class default read fails"), Result.bOk);
	TestTrue(TEXT("error exists"), Result.Error.IsSet());
	if (!Result.Error.IsSet())
	{
		return false;
	}
	TestEqual(TEXT("error code"), Result.Error->Code, FString(TEXT("class_default_property_not_found")));
	TestEqual(TEXT("error category"), Result.Error->Category, FString(TEXT("parameter_error")));
	TestEqual(TEXT("safe next action"), Result.Error->SafeNextAction, FString(TEXT("correct_property_path_then_retry")));
	return true;
}

#endif
