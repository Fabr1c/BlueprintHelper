#include "Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

class FBlueprintHelperEventKindCatalogEvidenceTestLocalUtils
{
public:
	static FString MakeTestObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UBlueprint* MakeBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperGraphWriteEvents/%s"),
			*MakeTestObjectName(Prefix)));
		Package->SetDirtyFlag(false);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeTestObjectName(TEXT("BP_EventKindCatalogEvidence")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperEventKindCatalogEvidenceTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static TSharedRef<FJsonObject> MakeStringLiteralExpression(const FString& Value)
	{
		TSharedRef<FJsonObject> Literal = MakeShared<FJsonObject>();
		Literal->SetStringField(TEXT("kind"), TEXT("literal"));
		Literal->SetStringField(TEXT("value_type"), TEXT("string"));
		Literal->SetStringField(TEXT("value"), Value);
		return Literal;
	}

	static TSharedRef<FJsonObject> MakeLogicSpec(
		const FString& EventKind,
		const FString& EventName,
		bool bIncludeSignatureEvidence)
	{
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("id"), EventName + TEXT("_entry"));
		Entry->SetStringField(TEXT("kind"), EventKind);
		Entry->SetStringField(TEXT("event_kind"), EventKind);
		Entry->SetStringField(TEXT("name"), EventName);
		if (bIncludeSignatureEvidence)
		{
			const FString SignatureEvidenceId = FString::Printf(TEXT("signature:%s:%s"), *EventKind, *EventName);
			Entry->SetStringField(TEXT("source_cluster"), TEXT("blueprint_signature"));
			Entry->SetStringField(TEXT("signature_evidence_id"), SignatureEvidenceId);

			TSharedRef<FJsonObject> CatalogEvidence = MakeShared<FJsonObject>();
			CatalogEvidence->SetStringField(TEXT("source"), TEXT("signature"));
			CatalogEvidence->SetStringField(TEXT("signature_evidence_id"), SignatureEvidenceId);
			Entry->SetObjectField(TEXT("catalog_evidence"), CatalogEvidence);
		}

		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("kind"), TEXT("call"));
		Statement->SetStringField(TEXT("target"), TEXT("PrintString"));

		TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
		Args->SetObjectField(TEXT("InString"), MakeStringLiteralExpression(TEXT("event kind catalog evidence")));
		Statement->SetObjectField(TEXT("args"), Args);

		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(Statement));

		TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
		LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v2"));
		LogicSpec->SetObjectField(TEXT("entry"), Entry);
		LogicSpec->SetArrayField(TEXT("statements"), Statements);
		return LogicSpec;
	}

	static TSharedRef<FJsonObject> MakeAppendDryRunPayload(
		const FString& AssetPath,
		const FString& GraphName,
		const TSharedRef<FJsonObject>& LogicSpec)
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Target->SetStringField(TEXT("graph"), GraphName);

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("target"), Target);
		Payload->SetBoolField(TEXT("dry_run"), true);
		Payload->SetBoolField(TEXT("allow_existing_graph"), true);
		Payload->SetStringField(TEXT("feature_name"), TEXT("EventKindCatalogEvidence"));
		Payload->SetObjectField(TEXT("logic_spec"), LogicSpec);
		return Payload;
	}

	static FBlueprintHelperToolResultBase ExecuteAppendDryRun(
		const FString& AssetPath,
		const FString& GraphName,
		const TSharedRef<FJsonObject>& LogicSpec)
	{
		FBlueprintHelperGraphResolver Resolver;
		FBlueprintHelperBlockIdService BlockIdService;
		FBlueprintHelperOwnershipService OwnershipService;
		const FBlueprintHelperAppendBlueprintGraphService AppendService(
			Resolver,
			BlockIdService,
			OwnershipService);
		return AppendService.Execute(MakeAppendDryRunPayload(AssetPath, GraphName, LogicSpec));
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCustomEventAllowsBusinessBndEvtPrefixTest,
	"BlueprintHelper.GraphWrite.Events.CustomEventAllowsBusinessBndEvtPrefix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCustomEventAllowsBusinessBndEvtPrefixTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint =
		FBlueprintHelperEventKindCatalogEvidenceTestLocalUtils::MakeBlueprint(TEXT("BusinessPrefix"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	const FBlueprintHelperToolResultBase Result =
		FBlueprintHelperEventKindCatalogEvidenceTestLocalUtils::ExecuteAppendDryRun(
			Blueprint->GetPathName(),
			Blueprint->UbergraphPages[0]->GetName(),
			FBlueprintHelperEventKindCatalogEvidenceTestLocalUtils::MakeLogicSpec(
				TEXT("custom_event"),
				TEXT("BndEvt__BusinessTelemetry"),
				true));

	TestTrue(TEXT("custom business BndEvt prefix is accepted"), Result.bOk);
	TestFalse(TEXT("custom business BndEvt prefix is not blocked by error"), Result.Error.IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOverrideEventRequiresCatalogEvidenceTest,
	"BlueprintHelper.GraphWrite.Events.OverrideEventRequiresCatalogEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOverrideEventRequiresCatalogEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint =
		FBlueprintHelperEventKindCatalogEvidenceTestLocalUtils::MakeBlueprint(TEXT("OverrideMissingEvidence"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	const FBlueprintHelperToolResultBase Result =
		FBlueprintHelperEventKindCatalogEvidenceTestLocalUtils::ExecuteAppendDryRun(
			Blueprint->GetPathName(),
			Blueprint->UbergraphPages[0]->GetName(),
			FBlueprintHelperEventKindCatalogEvidenceTestLocalUtils::MakeLogicSpec(
				TEXT("override_event"),
				TEXT("ReceiveBeginPlay"),
				false));

	TestFalse(TEXT("override event without catalog evidence is rejected"), Result.bOk);
	TestTrue(TEXT("override event rejection has error"), Result.Error.IsSet());
	if (Result.Error.IsSet())
	{
		TestEqual(TEXT("override event rejection code"), Result.Error->Code, FString(TEXT("event_catalog_evidence_required")));
	}
	return true;
}

#endif
