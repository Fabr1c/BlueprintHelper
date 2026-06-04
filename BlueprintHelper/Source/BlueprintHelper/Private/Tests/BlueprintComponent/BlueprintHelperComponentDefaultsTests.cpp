#if WITH_DEV_AUTOMATION_TESTS

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeTypes.h"
#include "Runtime/TaskRuntime/Clusters/Component/BlueprintHelperComponentTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintComponent/BlueprintHelperComponentTaskPlanAdapter.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"

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

class FBlueprintHelperComponentDefaultsTestsLocalUtils
{
public:
	static FString MakeUniqueObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UBlueprint* MakeActorBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperComponentDefaults/%s"),
			*MakeUniqueObjectName(Prefix)));
		if (!Package)
		{
			return nullptr;
		}

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeUniqueObjectName(TEXT("BP_ComponentDefaults")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperComponentDefaultsTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
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
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentDefaultsRoundTripsTemplateValuesTest,
	"BlueprintHelper.Component.Defaults.RoundTripsTemplateValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentDefaultsRoundTripsTemplateValuesTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperComponentDefaultsTestsLocalUtils::MakeActorBlueprint(TEXT("RoundTrip"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);

	const FString ComponentName = TEXT("DefaultsRoundTripComponent");
	FBlueprintHelperAddComponentRequest AddRequest;
	AddRequest.AssetPath = Blueprint->GetPathName();
	AddRequest.ComponentName = ComponentName;
	AddRequest.ComponentClass = TEXT("SceneComponent");
	const FBlueprintHelperToolResultBase AddResult = ComponentService.AddComponent(AddRequest);
	TestTrue(TEXT("component add succeeds"), AddResult.bOk);
	TestEqual(TEXT("component add applies"), AddResult.Status, EBlueprintHelperToolStatus::Applied);

	USCS_Node* Node = FBlueprintHelperComponentDefaultsTestsLocalUtils::FindNodeByName(Blueprint, ComponentName);
	TestNotNull(TEXT("component node exists"), Node);
	TestNotNull(TEXT("component template exists"), Node ? Node->ComponentTemplate.Get() : nullptr);
	if (!Node || !Node->ComponentTemplate)
	{
		return false;
	}

	FString PropertyName;
	FBoolProperty* BoolProperty = FBlueprintHelperComponentDefaultsTestsLocalUtils::FindWritableBoolProperty(
		Node->ComponentTemplate,
		PropertyName);
	TestNotNull(TEXT("component template has writable bool property"), BoolProperty);
	if (!BoolProperty)
	{
		return false;
	}

	FBlueprintHelperComponentDefaultsTestsLocalUtils::WriteBoolPropertyValue(
		Node->ComponentTemplate,
		BoolProperty,
		true);
	TestTrue(TEXT("template bool starts true"),
		FBlueprintHelperComponentDefaultsTestsLocalUtils::ReadBoolPropertyValue(Node->ComponentTemplate, BoolProperty));

	FBlueprintHelperSetComponentPropertiesRequest SetRequest;
	SetRequest.AssetPath = Blueprint->GetPathName();
	SetRequest.ComponentName = ComponentName;
	SetRequest.Mode = EBlueprintHelperComponentPropertyMode::Batch;

	FBlueprintHelperComponentPropertySetting Setting;
	Setting.PropertyPath = PropertyName;
	Setting.Value = MakeShared<FJsonValueBoolean>(false);
	SetRequest.Settings.Add(MoveTemp(Setting));

	const FBlueprintHelperToolResultBase SetResult = ComponentService.SetComponentProperties(SetRequest);
	TestTrue(TEXT("set component defaults succeeds"), SetResult.bOk);
	TestEqual(TEXT("set component defaults applies"), SetResult.Status, EBlueprintHelperToolStatus::Applied);
	TestFalse(TEXT("template bool was written"),
		FBlueprintHelperComponentDefaultsTestsLocalUtils::ReadBoolPropertyValue(Node->ComponentTemplate, BoolProperty));
	TestTrue(TEXT("result data exists"), SetResult.Data.IsValid());
	if (!SetResult.Data.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* ComponentObject = nullptr;
	TestTrue(TEXT("component identity exists in result"),
		SetResult.Data->TryGetObjectField(TEXT("component"), ComponentObject) && ComponentObject && ComponentObject->IsValid());
	FString ComponentTemplatePath;
	FString ComponentId;
	if (ComponentObject && ComponentObject->IsValid())
	{
		TestTrue(TEXT("component_template_path is emitted"),
			(*ComponentObject)->TryGetStringField(TEXT("component_template_path"), ComponentTemplatePath));
		TestTrue(TEXT("component_id is emitted"),
			(*ComponentObject)->TryGetStringField(TEXT("component_id"), ComponentId));
	}
	TestEqual(TEXT("component_template_path targets template object"),
		ComponentTemplatePath,
		Node->ComponentTemplate->GetPathName());
	TestFalse(TEXT("component_id is non-empty"), ComponentId.IsEmpty());

	const TSharedPtr<FJsonObject>* PropertyResult = nullptr;
	TestTrue(TEXT("property_result exists"),
		SetResult.Data->TryGetObjectField(TEXT("property_result"), PropertyResult) && PropertyResult && PropertyResult->IsValid());
	if (!PropertyResult || !PropertyResult->IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ChangedProperties = nullptr;
	TestTrue(TEXT("changed_properties array is emitted"),
		(*PropertyResult)->TryGetArrayField(TEXT("changed_properties"), ChangedProperties));
	TestEqual(TEXT("one changed property recorded"), ChangedProperties ? ChangedProperties->Num() : 0, 1);
	if (!ChangedProperties || ChangedProperties->Num() != 1)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> ChangedProperty = (*ChangedProperties)[0].IsValid()
		? (*ChangedProperties)[0]->AsObject()
		: nullptr;
	TestTrue(TEXT("changed property object exists"), ChangedProperty.IsValid());
	if (!ChangedProperty.IsValid())
	{
		return false;
	}

	FString ChangedPath;
	FString BeforeValue;
	FString AfterValue;
	TestTrue(TEXT("changed property has property_path"),
		ChangedProperty->TryGetStringField(TEXT("property_path"), ChangedPath));
	TestTrue(TEXT("changed property has before_value"),
		ChangedProperty->TryGetStringField(TEXT("before_value"), BeforeValue));
	TestTrue(TEXT("changed property has after_value"),
		ChangedProperty->TryGetStringField(TEXT("after_value"), AfterValue));
	TestEqual(TEXT("changed property path is normalized"), ChangedPath, PropertyName);
	TestEqual(TEXT("before value recorded"), BeforeValue.ToLower(), FString(TEXT("true")));
	TestEqual(TEXT("after value recorded"), AfterValue.ToLower(), FString(TEXT("false")));

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	LoweredStep.Capability = FBlueprintHelperComponentTaskPlanAdapter::CapabilityBlueprintComponent;
	LoweredStep.AdapterOperation = FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationSetComponentProperties;
	LoweredStep.Payload = MakeShared<FJsonObject>();
	LoweredStep.Payload->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
	LoweredStep.Payload->SetStringField(TEXT("component_name"), ComponentName);

	FBlueprintHelperWriteReviewEvidence Evidence;
	TestTrue(TEXT("component runtime evidence is built"),
		FBlueprintHelperComponentTaskRuntimeCluster::BuildReviewEvidence(
			LoweredStep,
			SetResult,
			TEXT("archive_defaults_roundtrip"),
			TEXT("task_defaults_roundtrip"),
			0,
			Evidence));
	TestEqual(TEXT("one atomic review target"), Evidence.AtomicTargets.Num(), 1);
	if (Evidence.AtomicTargets.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& Target = Evidence.AtomicTargets[0];
	TestEqual(TEXT("review target stores component template path"), Target.ComponentTemplatePath, ComponentTemplatePath);
	TestEqual(TEXT("review target stores component id"), Target.ComponentId, ComponentId);
	TestTrue(TEXT("review target stores changed properties"), Target.ChangedPropertiesJson.Contains(PropertyName));
	TestFalse(TEXT("review target stores before snapshot"), Target.BeforeSnapshotJson.IsEmpty());
	TestFalse(TEXT("review target stores after snapshot"), Target.AfterSnapshotJson.IsEmpty());
	TestTrue(TEXT("before snapshot carries property path"), Target.BeforeSnapshotJson.Contains(PropertyName));
	TestTrue(TEXT("after snapshot carries property path"), Target.AfterSnapshotJson.Contains(PropertyName));
	return true;
}

#endif
