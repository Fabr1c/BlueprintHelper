#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_SetFieldsInStruct.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Math/Vector.h"
#include "Misc/AutomationTest.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Package.h"

namespace
{
static FString MakeFieldFragmentBuilderTestObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeFieldFragmentBuilderTestBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperFieldFragmentBuilder/%s"),
		*MakeFieldFragmentBuilderTestObjectName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeFieldFragmentBuilderTestObjectName(TEXT("BP_FieldFragmentBuilder")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperFieldFragmentBuilderTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldFragmentBuilderSelectsVariableNodeFamiliesTest,
	"BlueprintHelper.GraphWrite.FieldFragmentBuilder.SelectsVariableNodeFamilies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldFragmentBuilderSelectsVariableNodeFamiliesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("member get family"), FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(TEXT("field.member_get")), FString(TEXT("variable_get")));
	TestEqual(TEXT("member set family"), FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(TEXT("field.member_set")), FString(TEXT("variable_set")));
	TestTrue(TEXT("set fragment needs exec pins"), FBlueprintHelperFieldFragmentBuilder::DoesCapabilityProduceExecPins(TEXT("field.member_set")));
	TestFalse(TEXT("get fragment has no exec pins"), FBlueprintHelperFieldFragmentBuilder::DoesCapabilityProduceExecPins(TEXT("field.member_get")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphStatementBuilderRoutesFieldCapabilityFamiliesTest,
	"BlueprintHelper.GraphWrite.FieldFragmentBuilder.Routing.RoutesCapabilityFamilies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphStatementBuilderRoutesFieldCapabilityFamiliesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("component ref family"), FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(TEXT("field.component_ref_get")), FString(TEXT("component_variable_get")));
	TestEqual(TEXT("object pin family"), FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(TEXT("field.object_pin_member_get")), FString(TEXT("variable_get_target")));
	TestEqual(TEXT("struct get family"), FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(TEXT("field.struct_member_get")), FString(TEXT("break_struct")));
	TestEqual(TEXT("struct set family"), FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(TEXT("field.struct_member_set")), FString(TEXT("set_fields_in_struct")));
	TestEqual(TEXT("nested family"), FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(TEXT("field.nested_property_path")), FString(TEXT("property_path_fragment")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldFragmentBuilderBuildsStructAndNestedFragmentsTest,
	"BlueprintHelper.GraphWrite.FieldFragmentBuilder.PropertyPath.BuildsStructAndNestedFragments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldFragmentBuilderBuildsStructAndNestedFragmentsTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeFieldFragmentBuilderTestBlueprint();
	UEdGraph* Graph = Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
	TestNotNull(TEXT("test blueprint"), Blueprint);
	TestNotNull(TEXT("test graph"), Graph);
	if (!Graph)
	{
		return false;
	}

	FBlueprintHelperFieldFragmentPlan Plan;
	Plan.CapabilityId = TEXT("field.struct_member_get");
	Plan.CapabilityFacts.Add(TEXT("field.struct_type"), TBaseStructure<FVector>::Get()->GetPathName());
	Plan.CapabilityFacts.Add(TEXT("field.property_path"), TEXT("X"));

	FBlueprintHelperNodeFragment ReadFragment;
	FString Error;
	TestTrue(TEXT("struct read builds"), FBlueprintHelperFieldFragmentBuilder::BuildStructReadFragment(Graph, Plan, ReadFragment, Error));
	TestTrue(TEXT("struct read fragment valid"), ReadFragment.IsValid());
	TestNotNull(TEXT("struct read uses break struct"), Cast<UK2Node_BreakStruct>(ReadFragment.PrimaryNode));
	TestEqual(TEXT("struct read node family"), ReadFragment.OwnershipTags.FindRef(TEXT("field.expected_node_family")), FString(TEXT("break_struct")));
	TestEqual(TEXT("struct read segment count"), ReadFragment.OwnershipTags.FindRef(TEXT("field.property_path.segment_count")), FString(TEXT("1")));
	TestTrue(TEXT("struct read exposes leaf pin"), ReadFragment.DataOutputs.Contains(TEXT("X")));

	Plan.CapabilityId = TEXT("field.struct_member_set");
	FBlueprintHelperNodeFragment WriteFragment;
	Error.Reset();
	TestTrue(TEXT("struct write builds"), FBlueprintHelperFieldFragmentBuilder::BuildStructWriteFragment(Graph, Plan, WriteFragment, Error));
	TestTrue(TEXT("struct write fragment valid"), WriteFragment.IsValid());
	TestNotNull(TEXT("struct write uses set fields in struct"), Cast<UK2Node_SetFieldsInStruct>(WriteFragment.PrimaryNode));
	TestEqual(TEXT("struct write node family"), WriteFragment.OwnershipTags.FindRef(TEXT("field.expected_node_family")), FString(TEXT("set_fields_in_struct")));

	Plan.CapabilityId = TEXT("field.nested_property_path");
	Plan.CapabilityFacts.FindOrAdd(TEXT("field.struct_type")) = TBaseStructure<FInterpCurvePointVector>::Get()->GetPathName();
	Plan.CapabilityFacts.FindOrAdd(TEXT("field.property_path")) = TEXT("OutVal.X");
	FBlueprintHelperNodeFragment NestedFragment;
	Error.Reset();
	const bool bNestedPathBuilt = FBlueprintHelperFieldFragmentBuilder::BuildNestedPropertyPathFragment(Graph, Plan, NestedFragment, Error);
	if (!bNestedPathBuilt)
	{
		AddError(FString::Printf(TEXT("nested path build failed: %s"), *Error));
	}
	TestTrue(TEXT("nested path builds"), bNestedPathBuilt);
	TestTrue(TEXT("nested path fragment valid"), NestedFragment.IsValid());
	TestNotNull(TEXT("nested path starts with break struct"), Cast<UK2Node_BreakStruct>(NestedFragment.PrimaryNode));
	TestEqual(TEXT("nested path node family"), NestedFragment.OwnershipTags.FindRef(TEXT("field.expected_node_family")), FString(TEXT("property_path_fragment")));
	TestEqual(TEXT("nested path segment count"), NestedFragment.OwnershipTags.FindRef(TEXT("field.property_path.segment_count")), FString(TEXT("2")));
	TestEqual(TEXT("nested path break node count"), NestedFragment.OwnershipTags.FindRef(TEXT("field.path.break_node_count")), FString(TEXT("2")));
	TestEqual(TEXT("nested path link count"), NestedFragment.OwnershipTags.FindRef(TEXT("field.path.link_count")), FString(TEXT("1")));
	TestEqual(TEXT("nested path internal links"), NestedFragment.InternalLinks.Num(), 1);
	TestTrue(TEXT("nested path exposes leaf pin"), NestedFragment.DataOutputs.Contains(TEXT("field.path.leaf")));

	return true;
}

#endif
