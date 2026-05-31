#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateBindingObjectResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

namespace
{
static bool ReadPluginSourceFile(const FString& RelativePath, FString& OutText)
{
	const FString FullPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectPluginsDir() / TEXT("BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/") / RelativePath);
	return FFileHelper::LoadFileToString(OutText, *FullPath);
}

static UEdGraphPin* MakeResolverTestPin()
{
	UEdGraphNode* Node = NewObject<UEdGraphNode>(GetTransientPackage());
	FEdGraphPinType PinType;
	PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
	return Node ? Node->CreatePin(EGPD_Output, PinType, FName(TEXT("Value"))) : nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateFragmentBuilderBoundaryTest,
	"BlueprintHelper.GraphWrite.EventDelegate.FragmentBuilder.Boundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateFragmentBuilderBoundaryTest::RunTest(const FString& Parameters)
{
	FString BuilderText;
	FString StatementUtilsText;
	FString ResolverHeaderText;
	FString ResolverText;

	TestTrue(
		TEXT("read event delegate fragment builder"),
		ReadPluginSourceFile(TEXT("GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.cpp"), BuilderText));
	TestTrue(
		TEXT("read graph statement utils"),
		ReadPluginSourceFile(TEXT("GraphStatement/Utils/GraphWriteGraphStatementUtils.cpp"), StatementUtilsText));
	TestTrue(
		TEXT("read binding object resolver header"),
		ReadPluginSourceFile(TEXT("GraphStatement/BlueprintHelperEventDelegateBindingObjectResolver.h"), ResolverHeaderText));
	TestTrue(
		TEXT("read binding object resolver source"),
		ReadPluginSourceFile(TEXT("GraphStatement/BlueprintHelperEventDelegateBindingObjectResolver.cpp"), ResolverText));

	TestTrue(TEXT("builder routes projected binding object through statement utils"),
		BuilderText.Contains(TEXT("ConnectProjectedBindingObjectToPrimaryTarget")));
	TestTrue(TEXT("statement utils consumes binding object resolver"),
		StatementUtilsText.Contains(TEXT("FBlueprintHelperEventDelegateBindingObjectResolver::Resolve")));
	TestFalse(TEXT("builder does not allocate component getter"), BuilderText.Contains(TEXT("NewObject<UK2Node_VariableGet>")));
	TestFalse(TEXT("statement utils does not allocate component getter"), StatementUtilsText.Contains(TEXT("NewObject<UK2Node_VariableGet>")));
	TestFalse(TEXT("builder does not include component getter class"), BuilderText.Contains(TEXT("K2Node_VariableGet.h")));
	TestFalse(TEXT("statement utils does not include component getter class"), StatementUtilsText.Contains(TEXT("K2Node_VariableGet.h")));
	TestTrue(TEXT("resolver supports self"), ResolverText.Contains(TEXT("self")));
	TestTrue(TEXT("resolver supports component_ref"), ResolverText.Contains(TEXT("component_ref")));
	TestTrue(TEXT("resolver supports field_get_ref"), ResolverText.Contains(TEXT("field_get_ref")));
	TestTrue(TEXT("resolver supports linked_pin_ref"), ResolverText.Contains(TEXT("linked_pin_ref")));
	TestTrue(TEXT("resolver supports function_return_ref"), ResolverText.Contains(TEXT("function_return_ref")));
	TestTrue(TEXT("resolver DTO is present"), ResolverHeaderText.Contains(TEXT("FBlueprintHelperEventDelegateBindingObjectResolution")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateBindingObjectResolverTest,
	"BlueprintHelper.GraphWrite.EventDelegate.FragmentBuilder.BindingObjectResolver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateBindingObjectResolverTest::RunTest(const FString& Parameters)
{
	UEdGraphPin* ProjectedPin = MakeResolverTestPin();
	TestNotNull(TEXT("projected pin"), ProjectedPin);
	if (!ProjectedPin)
	{
		return false;
	}

	FBlueprintHelperNodeFragment Fragment;
	Fragment.SourceStatementId = TEXT("stmt_delegate_call");
	Fragment.PinBindings.Add(
		TEXT("field_get_ref:DoorMesh"),
		FBlueprintHelperFragmentPinRef{ TEXT("binding_object"), TEXT("Value"), TEXT("object"), ProjectedPin });
	Fragment.PinBindings.Add(
		TEXT("linked_pin_ref:source_node:Target"),
		FBlueprintHelperFragmentPinRef{ TEXT("binding_object"), TEXT("Value"), TEXT("object"), ProjectedPin });
	Fragment.PinBindings.Add(
		TEXT("function_return_ref:stmt_delegate_call:return"),
		FBlueprintHelperFragmentPinRef{ TEXT("binding_object"), TEXT("Value"), TEXT("object"), ProjectedPin });

	FBlueprintHelperEventDelegateUseSiteEvidence Evidence;
	Evidence.BindingObjectKind = TEXT("field_get_ref");
	Evidence.BindingObjectEvidenceId = TEXT("field_get_ref:DoorMesh");
	FBlueprintHelperEventDelegateBindingObjectResolution Resolution =
		FBlueprintHelperEventDelegateBindingObjectResolver::Resolve(Evidence, Fragment);
	TestTrue(TEXT("field_get_ref resolves projected pin"), Resolution.bResolved);
	TestEqual(TEXT("field_get_ref pin"), Resolution.ObjectPin, ProjectedPin);

	Evidence.BindingObjectKind = TEXT("linked_pin_ref");
	Evidence.BindingObjectEvidenceId = TEXT("linked_pin_ref:source_node:Target");
	Evidence.BindingObjectNodeGuid.Reset();
	Evidence.BindingObjectPinName.Reset();
	Resolution = FBlueprintHelperEventDelegateBindingObjectResolver::Resolve(Evidence, Fragment);
	TestFalse(TEXT("linked_pin_ref requires stable anchor"), Resolution.bResolved);
	TestEqual(TEXT("linked_pin_ref anchor error"), Resolution.ErrorCode, FString(TEXT("binding_object_linked_pin_anchor_missing")));

	Evidence.BindingObjectNodeGuid = TEXT("0123456789abcdef0123456789abcdef");
	Evidence.BindingObjectPinName = TEXT("Target");
	Resolution = FBlueprintHelperEventDelegateBindingObjectResolver::Resolve(Evidence, Fragment);
	TestTrue(TEXT("linked_pin_ref resolves with stable anchor"), Resolution.bResolved);
	TestEqual(TEXT("linked_pin_ref pin"), Resolution.ObjectPin, ProjectedPin);

	Evidence.BindingObjectKind = TEXT("function_return_ref");
	Evidence.BindingObjectEvidenceId = TEXT("function_return_ref:stmt_delegate_call:return");
	Evidence.BindingObjectProducerStatementId = TEXT("other_statement");
	Resolution = FBlueprintHelperEventDelegateBindingObjectResolver::Resolve(Evidence, Fragment);
	TestFalse(TEXT("cross-statement function_return_ref rejected"), Resolution.bResolved);
	TestEqual(TEXT("cross-statement error"), Resolution.ErrorCode, FString(TEXT("binding_object_cross_statement_unsupported")));

	Evidence.BindingObjectProducerStatementId = TEXT("stmt_delegate_call");
	Resolution = FBlueprintHelperEventDelegateBindingObjectResolver::Resolve(Evidence, Fragment);
	TestTrue(TEXT("same-statement function_return_ref resolves"), Resolution.bResolved);
	TestEqual(TEXT("same-statement function_return_ref pin"), Resolution.ObjectPin, ProjectedPin);
	return true;
}

#endif
