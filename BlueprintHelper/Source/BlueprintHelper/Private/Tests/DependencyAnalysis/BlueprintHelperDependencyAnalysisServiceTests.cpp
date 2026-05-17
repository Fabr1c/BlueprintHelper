#if WITH_DEV_AUTOMATION_TESTS

#include "EdGraph/EdGraph.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "WidgetBlueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Shared/Safety/BlueprintHelperDependencyAnalysisService.h"
#include "UObject/Package.h"

class FBlueprintHelperDependencyAnalysisServiceTestsLocalUtils
{
public:
	static UBlueprint* MakeActorBlueprint(const FString& Prefix)
	{
		const FString UniqueName = FString::Printf(
			TEXT("%s_%s"),
			*Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Game/BlueprintHelperDependency/%s"), *UniqueName));
		Package->SetDirtyFlag(false);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*FString::Printf(TEXT("BP_%s"), *UniqueName),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperDependencyAnalysisServiceTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UWidgetBlueprint* MakeWidgetBlueprint(const FString& Prefix)
	{
		const FString UniqueName = FString::Printf(
			TEXT("%s_%s"),
			*Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Game/BlueprintHelperDependency/%s"), *UniqueName));
		Package->SetDirtyFlag(false);

		UWidgetBlueprint* Blueprint = NewObject<UWidgetBlueprint>(
			Package,
			*FString::Printf(TEXT("WBP_%s"), *UniqueName),
			RF_Public | RF_Standalone | RF_Transactional);
		if (Blueprint)
		{
			Blueprint->ParentClass = UUserWidget::StaticClass();
		}
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UK2Node_CallFunction* AddActorDestroyCall(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
		CallNode->FunctionReference.SetExternalMember(
			GET_FUNCTION_NAME_CHECKED(AActor, K2_DestroyActor),
			AActor::StaticClass());
		Graph->AddNode(CallNode, true, false);
		return CallNode;
	}

	static FDelegateEditorBinding MakeWidgetPropertyBinding(
		const FString& WidgetName,
		const FString& PropertyName,
		const FString& SourceProperty)
	{
		FDelegateEditorBinding Binding;
		Binding.ObjectName = WidgetName;
		Binding.PropertyName = FName(*PropertyName);
		Binding.SourceProperty = FName(*SourceProperty);
		Binding.Kind = EBindingKind::Property;
		return Binding;
	}

	static FDelegateEditorBinding MakeWidgetFunctionBinding(
		const FString& WidgetName,
		const FString& PropertyName,
		const FString& FunctionName)
	{
		FDelegateEditorBinding Binding;
		Binding.ObjectName = WidgetName;
		Binding.PropertyName = FName(*PropertyName);
		Binding.FunctionName = FName(*FunctionName);
		Binding.Kind = EBindingKind::Function;
		return Binding;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDependencyAnalysisServiceFindsFunctionCallTest,
	"BlueprintHelper.DependencyAnalysis.Service.FindsFunctionCallReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDependencyAnalysisServiceFindsFunctionCallTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperDependencyAnalysisServiceTestsLocalUtils::MakeActorBlueprint(TEXT("FunctionCallReference"));
	TestNotNull(TEXT("test Blueprint exists"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0)
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	TestNotNull(TEXT("test graph exists"), Graph);
	TestNotNull(TEXT("test function call exists"),
		FBlueprintHelperDependencyAnalysisServiceTestsLocalUtils::AddActorDestroyCall(Graph));

	FBlueprintHelperDependencyAnalysisTarget Target;
	Target.AssetPath = Blueprint->GetPathName();
	Target.TargetType = TEXT("function");
	Target.TargetName = TEXT("K2_DestroyActor");
	Target.DeclaringClassPath = TEXT("/Script/Engine.Actor");

	FBlueprintHelperDependencyAnalysisOptions Options;
	Options.SearchScope = TEXT("asset");
	Options.ResolutionPolicy = TEXT("ue_only");
	Options.Detail = TEXT("samples");
	Options.MaxResultCount = 10;

	FBlueprintHelperDependencyAnalysisService Service;
	FBlueprintHelperReferenceContextPack Context;
	FString ErrorCode;
	FString ErrorMessage;
	const bool bResult = Service.TryBuildReferenceContext(Target, Options, Context, ErrorCode, ErrorMessage);
	TestTrue(TEXT("reference context builds"), bResult);
	TestEqual(TEXT("reference context has no error code"), ErrorCode, FString());
	TestEqual(TEXT("reference context returns one referencer asset"), Context.Referencers.Num(), 1);
	TestEqual(TEXT("reference context counts function call"), Context.Summary.ReferenceCount, 1);
	if (Context.Referencers.Num() > 0)
	{
		TestTrue(TEXT("referencer is blocking"), Context.Referencers[0].Safety == TEXT("blocking"));
		TestTrue(TEXT("referencer records function_call kind"), Context.Referencers[0].ReferenceKinds.Contains(TEXT("function_call")));
		TestTrue(TEXT("referencer records compact sample"), Context.Referencers[0].Samples.Num() > 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDependencyAnalysisServiceFindsWidgetPropertyBindingTest,
	"BlueprintHelper.DependencyAnalysis.Service.FindsWidgetPropertyBindingReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDependencyAnalysisServiceFindsWidgetPropertyBindingTest::RunTest(const FString& Parameters)
{
	UWidgetBlueprint* Blueprint = FBlueprintHelperDependencyAnalysisServiceTestsLocalUtils::MakeWidgetBlueprint(TEXT("WidgetPropertyBinding"));
	TestNotNull(TEXT("test WidgetBlueprint exists"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	Blueprint->Bindings.Add(FBlueprintHelperDependencyAnalysisServiceTestsLocalUtils::MakeWidgetPropertyBinding(
		TEXT("HealthLabel"),
		TEXT("Text"),
		TEXT("HealthText")));

	FBlueprintHelperDependencyAnalysisTarget Target;
	Target.AssetPath = Blueprint->GetPathName();
	Target.TargetType = TEXT("member_variable");
	Target.TargetName = TEXT("HealthText");

	FBlueprintHelperDependencyAnalysisOptions Options;
	Options.SearchScope = TEXT("asset");
	Options.ResolutionPolicy = TEXT("name_only");
	Options.Detail = TEXT("samples");

	FBlueprintHelperDependencyAnalysisService Service;
	FBlueprintHelperReferenceContextPack Context;
	FString ErrorCode;
	FString ErrorMessage;
	const bool bResult = Service.TryBuildReferenceContext(Target, Options, Context, ErrorCode, ErrorMessage);

	TestTrue(TEXT("reference context builds"), bResult);
	TestEqual(TEXT("widget binding returns one referencer asset"), Context.Referencers.Num(), 1);
	TestEqual(TEXT("widget binding counts one reference"), Context.Summary.ReferenceCount, 1);
	TestFalse(TEXT("widget binding scan is no longer unsupported"),
		Context.UnsupportedChecks.Contains(TEXT("widget_property_binding_scan")));
	if (Context.Referencers.Num() > 0)
	{
		TestTrue(TEXT("referencer records widget_property_binding kind"),
			Context.Referencers[0].ReferenceKinds.Contains(TEXT("widget_property_binding")));
		TestTrue(TEXT("referencer is blocking"), Context.Referencers[0].Safety == TEXT("blocking"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDependencyAnalysisServiceFindsWidgetFunctionBindingTest,
	"BlueprintHelper.DependencyAnalysis.Service.FindsWidgetFunctionBindingReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDependencyAnalysisServiceFindsWidgetFunctionBindingTest::RunTest(const FString& Parameters)
{
	UWidgetBlueprint* Blueprint = FBlueprintHelperDependencyAnalysisServiceTestsLocalUtils::MakeWidgetBlueprint(TEXT("WidgetFunctionBinding"));
	TestNotNull(TEXT("test WidgetBlueprint exists"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	Blueprint->Bindings.Add(FBlueprintHelperDependencyAnalysisServiceTestsLocalUtils::MakeWidgetFunctionBinding(
		TEXT("HealthLabel"),
		TEXT("Text"),
		TEXT("GetHealthText")));

	FBlueprintHelperDependencyAnalysisTarget Target;
	Target.AssetPath = Blueprint->GetPathName();
	Target.TargetType = TEXT("function");
	Target.TargetName = TEXT("GetHealthText");

	FBlueprintHelperDependencyAnalysisOptions Options;
	Options.SearchScope = TEXT("asset");
	Options.ResolutionPolicy = TEXT("name_only");
	Options.Detail = TEXT("samples");

	FBlueprintHelperDependencyAnalysisService Service;
	FBlueprintHelperReferenceContextPack Context;
	FString ErrorCode;
	FString ErrorMessage;
	const bool bResult = Service.TryBuildReferenceContext(Target, Options, Context, ErrorCode, ErrorMessage);

	TestTrue(TEXT("reference context builds"), bResult);
	TestEqual(TEXT("widget function binding returns one referencer asset"), Context.Referencers.Num(), 1);
	TestEqual(TEXT("widget function binding counts one reference"), Context.Summary.ReferenceCount, 1);
	if (Context.Referencers.Num() > 0)
	{
		TestTrue(TEXT("referencer records widget_property_binding_function kind"),
			Context.Referencers[0].ReferenceKinds.Contains(TEXT("widget_property_binding_function")));
		TestTrue(TEXT("referencer is blocking"), Context.Referencers[0].Safety == TEXT("blocking"));
	}
	return true;
}

#endif
