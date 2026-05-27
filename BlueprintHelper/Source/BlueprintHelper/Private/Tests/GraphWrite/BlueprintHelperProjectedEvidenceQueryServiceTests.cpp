#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGraphWriteProjectedEvidenceQueryService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace
{
static FString MakeProjectedEvidenceQueryTestName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeProjectedEvidenceQueryTestBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperProjectedEvidenceQuery/%s"),
		*MakeProjectedEvidenceQueryTestName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeProjectedEvidenceQueryTestName(TEXT("BP_ProjectedEvidenceQuery")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperProjectedEvidenceQueryServiceTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* GetProjectedEvidenceQueryTestGraph(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}

static TSharedRef<FJsonObject> MakeRequest(
	const FString& RequestId,
	const FString& OperationId,
	const FString& Kind,
	const TArray<FString>& Queries,
	const FString& GraphLatentAllowed = FString())
{
	TSharedRef<FJsonObject> Request = MakeShared<FJsonObject>();
	Request->SetStringField(TEXT("request_id"), RequestId);
	Request->SetStringField(TEXT("operation_id"), OperationId);
	Request->SetStringField(TEXT("projection_kind"), Kind);
	TArray<TSharedPtr<FJsonValue>> QueryValues;
	for (const FString& Query : Queries)
	{
		QueryValues.Add(MakeShared<FJsonValueString>(Query));
	}
	Request->SetArrayField(TEXT("queries"), QueryValues);
	if (!GraphLatentAllowed.IsEmpty())
	{
		Request->SetStringField(TEXT("graph_latent_allowed"), GraphLatentAllowed);
	}
	return Request;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperProjectedEvidenceQueryServiceProjectsActionDatabaseEvidenceTest,
	"BlueprintHelper.GraphWrite.ProjectedEvidenceQuery.ProjectsActionDatabaseEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperProjectedEvidenceQueryServiceProjectsActionDatabaseEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeProjectedEvidenceQueryTestBlueprint();
	UEdGraph* Graph = GetProjectedEvidenceQueryTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
	Payload->SetStringField(TEXT("graph_name"), Graph->GetName());

	TArray<TSharedPtr<FJsonValue>> Requests;
	Requests.Add(MakeShared<FJsonValueObject>(MakeRequest(
		TEXT("asset"),
		TEXT("create.asset_action"),
		TEXT("asset_action"),
		{ TEXT("Make Array") })));
	Requests.Add(MakeShared<FJsonValueObject>(MakeRequest(
		TEXT("latent"),
		TEXT("schedule.latent_or_async_node"),
		TEXT("schedule"),
		{ TEXT("Async Load Primary Asset") },
		TEXT("true"))));
	Requests.Add(MakeShared<FJsonValueObject>(MakeRequest(
		TEXT("timer"),
		TEXT("schedule.timer_delegate_node"),
		TEXT("schedule"),
		{ TEXT("Set Timer by Event"), TEXT("Set Timer by Delegate"), TEXT("Set Timer") })));
	Payload->SetArrayField(TEXT("requests"), Requests);

	const FBlueprintHelperToolResultBase Result =
		FBlueprintHelperGraphWriteProjectedEvidenceQueryService::Project(Payload);

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("projection result ok"), Result.bOk);
	bPassed &= TestTrue(TEXT("projection data exists"), Result.Data.IsValid());
	if (!Result.Data.IsValid())
	{
		return false;
	}

	bool bAllResolved = false;
	Result.Data->TryGetBoolField(TEXT("all_resolved"), bAllResolved);
	bPassed &= TestTrue(TEXT("all projected evidence resolved"), bAllResolved);

	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	bPassed &= TestTrue(TEXT("items array exists"), Result.Data->TryGetArrayField(TEXT("items"), Items) && Items != nullptr);
	if (!Items)
	{
		return false;
	}
	bPassed &= TestEqual(TEXT("item count"), Items->Num(), 3);

	for (const TSharedPtr<FJsonValue>& ItemValue : *Items)
	{
		const TSharedPtr<FJsonObject>* Item = nullptr;
		if (!ItemValue.IsValid() || !ItemValue->TryGetObject(Item) || !Item || !Item->IsValid())
		{
			AddError(TEXT("projection item was not an object."));
			return false;
		}
		FString Status;
		(*Item)->TryGetStringField(TEXT("status"), Status);
		bPassed &= TestEqual(TEXT("item status"), Status, FString(TEXT("resolved")));
		const TSharedPtr<FJsonObject>* Evidence = nullptr;
		bPassed &= TestTrue(TEXT("item evidence exists"), (*Item)->TryGetObjectField(TEXT("evidence"), Evidence) && Evidence && Evidence->IsValid());
		if (!Evidence || !Evidence->IsValid())
		{
			return false;
		}

		FString Kind;
		(*Item)->TryGetStringField(TEXT("projection_kind"), Kind);
		if (Kind == TEXT("asset_action"))
		{
			bPassed &= TestTrue(TEXT("asset stable id"), (*Evidence)->GetStringField(TEXT("asset_action_stable_id")).StartsWith(TEXT("action_database:")));
			bPassed &= TestFalse(TEXT("asset signature"), (*Evidence)->GetStringField(TEXT("asset_action_spawner_signature")).IsEmpty());
		}
		else if (Kind == TEXT("schedule"))
		{
			bPassed &= TestTrue(TEXT("schedule stable id"), (*Evidence)->GetStringField(TEXT("schedule_action_stable_id")).StartsWith(TEXT("action_database:")));
			bPassed &= TestFalse(TEXT("schedule signature"), (*Evidence)->GetStringField(TEXT("schedule_spawner_signature")).IsEmpty());
		}
	}

	return bPassed;
}

#endif
