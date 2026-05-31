#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/AssetDiscovery/BlueprintHelperAssetDiscoveryService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

namespace BlueprintHelperAssetDiscoveryServiceTests
{
	static constexpr const TCHAR* CodexSmokePath = TEXT("/Game/BlueprintHelper/CodexSmoke");
	static constexpr const TCHAR* GeneralityPath = TEXT("/Game/BlueprintHelper/Generality");
	static constexpr const TCHAR* BlueprintClassPath = TEXT("/Script/Engine.Blueprint");
	static constexpr const TCHAR* RedirectorClassPath = TEXT("/Script/CoreUObject.ObjectRedirector");

	static bool IsObjectAssetPath(const FString& AssetPath)
	{
		return AssetPath.StartsWith(TEXT("/")) && AssetPath.Contains(TEXT(".")) &&
			!AssetPath.Contains(TEXT("'")) && !AssetPath.Contains(TEXT(" "));
	}

	static bool AssertCompactFindAssetsResult(
		FAutomationTestBase& Test,
		const FBlueprintHelperFindAssetsResult& Result,
		const int32 ExpectedLimit)
	{
		Test.TestTrue(TEXT("FindAssets succeeds"), Result.bSuccess);
		Test.TestEqual(TEXT("FindAssets.v1 schema is preserved"), Result.Data.Schema, FString(TEXT("FindAssets.v1")));
		Test.TestEqual(TEXT("page limit is reported"), Result.Data.Page.Limit, ExpectedLimit);

		const TSharedRef<FJsonObject> Json = Result.Data.ToJson();
		Test.TestEqual(TEXT("result json stays compact"), Json->Values.Num(), 3);
		Test.TestTrue(TEXT("result json has schema"), Json->HasField(TEXT("schema")));
		Test.TestTrue(TEXT("result json has assets"), Json->HasField(TEXT("assets")));
		Test.TestTrue(TEXT("result json has page"), Json->HasField(TEXT("page")));
		Test.TestFalse(TEXT("result json omits total_count"), Json->HasField(TEXT("total_count")));

		const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
		Test.TestTrue(TEXT("assets json array exists"), Json->TryGetArrayField(TEXT("assets"), Assets));
		if (!Assets)
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& AssetValue : *Assets)
		{
			const TSharedPtr<FJsonObject> AssetJson = AssetValue.IsValid() ? AssetValue->AsObject() : nullptr;
			Test.TestTrue(TEXT("asset item is a json object"), AssetJson.IsValid());
			if (!AssetJson.IsValid())
			{
				return false;
			}

			Test.TestEqual(TEXT("asset item json stays compact"), AssetJson->Values.Num(), 3);
			Test.TestTrue(TEXT("asset item has asset_path"), AssetJson->HasField(TEXT("asset_path")));
			Test.TestTrue(TEXT("asset item has asset_type"), AssetJson->HasField(TEXT("asset_type")));
			Test.TestTrue(TEXT("asset item has asset_class"), AssetJson->HasField(TEXT("asset_class")));
			Test.TestFalse(TEXT("asset item omits asset_name"), AssetJson->HasField(TEXT("asset_name")));
		}

		return Result.bSuccess;
	}

	static bool AssertAllResultsUseObjectAssetPaths(
		FAutomationTestBase& Test,
		const FBlueprintHelperFindAssetsResult& Result)
	{
		for (const FBlueprintHelperAssetListItem& Asset : Result.Data.Assets)
		{
			Test.TestTrue(
				FString::Printf(TEXT("asset_path is a UE object asset path: %s"), *Asset.AssetPath),
				IsObjectAssetPath(Asset.AssetPath));
			Test.TestFalse(TEXT("asset_type metadata is compact"), Asset.AssetType.IsEmpty());
			Test.TestFalse(TEXT("asset_class metadata is compact"), Asset.AssetClass.IsEmpty());
		}

		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetDiscoveryFindAssetsDefaultScopeIsGameTest,
	"BlueprintHelper.AssetDiscovery.FindAssets.DefaultScopeIsGame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetDiscoveryFindAssetsDefaultScopeIsGameTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperFindAssetsRequest Request;
	Request.Query = TEXT("GraphLocalValueProducer_20260527_001");
	Request.Limit = 20;

	const FBlueprintHelperAssetDiscoveryService Service;
	const FBlueprintHelperFindAssetsResult Result = Service.FindAssets(Request);

	BlueprintHelperAssetDiscoveryServiceTests::AssertCompactFindAssetsResult(*this, Result, 20);
	TestTrue(TEXT("default /Game scope finds the project fixture"), Result.Data.Assets.Num() > 0);
	for (const FBlueprintHelperAssetListItem& Asset : Result.Data.Assets)
	{
		TestTrue(TEXT("default scope only returns /Game assets"), Asset.AssetPath.StartsWith(TEXT("/Game/")));
		TestFalse(TEXT("default scope excludes /Engine"), Asset.AssetPath.StartsWith(TEXT("/Engine/")));
	}

	return BlueprintHelperAssetDiscoveryServiceTests::AssertAllResultsUseObjectAssetPaths(*this, Result) &&
		Result.bSuccess && Result.Data.Assets.Num() > 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetDiscoveryFindAssetsQueryFiltersByNameTest,
	"BlueprintHelper.AssetDiscovery.FindAssets.QueryFiltersByName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetDiscoveryFindAssetsQueryFiltersByNameTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperFindAssetsRequest Request;
	Request.PathPrefixes.Add(BlueprintHelperAssetDiscoveryServiceTests::CodexSmokePath);
	Request.Query = TEXT("TimerLatentDoor");
	Request.Limit = 10;

	const FBlueprintHelperAssetDiscoveryService Service;
	const FBlueprintHelperFindAssetsResult Result = Service.FindAssets(Request);

	BlueprintHelperAssetDiscoveryServiceTests::AssertCompactFindAssetsResult(*this, Result, 10);
	TestTrue(TEXT("query finds the named fixture"), Result.Data.Assets.Num() > 0);
	for (const FBlueprintHelperAssetListItem& Asset : Result.Data.Assets)
	{
		TestTrue(TEXT("query filters by asset name"), Asset.AssetPath.Contains(TEXT("TimerLatentDoor")));
		TestFalse(TEXT("query excludes unrelated fixture names"), Asset.AssetPath.Contains(TEXT("GraphLocalValueProducer")));
	}

	return BlueprintHelperAssetDiscoveryServiceTests::AssertAllResultsUseObjectAssetPaths(*this, Result) &&
		Result.bSuccess && Result.Data.Assets.Num() > 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetDiscoveryFindAssetsQueryFiltersByPathFragmentTest,
	"BlueprintHelper.AssetDiscovery.FindAssets.QueryFiltersByPathFragment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetDiscoveryFindAssetsQueryFiltersByPathFragmentTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperFindAssetsRequest Request;
	Request.PathPrefixes.Add(TEXT("/Game/BlueprintHelper"));
	Request.Query = TEXT("/CodexSmoke/");
	Request.Limit = 20;

	const FBlueprintHelperAssetDiscoveryService Service;
	const FBlueprintHelperFindAssetsResult Result = Service.FindAssets(Request);

	BlueprintHelperAssetDiscoveryServiceTests::AssertCompactFindAssetsResult(*this, Result, 20);
	TestTrue(TEXT("query finds fixtures by path fragment"), Result.Data.Assets.Num() > 0);
	for (const FBlueprintHelperAssetListItem& Asset : Result.Data.Assets)
	{
		TestTrue(TEXT("query filters by object path fragment"), Asset.AssetPath.Contains(TEXT("/CodexSmoke/")));
		TestFalse(TEXT("query excludes sibling package paths"), Asset.AssetPath.Contains(TEXT("/Generality/")));
	}

	return BlueprintHelperAssetDiscoveryServiceTests::AssertAllResultsUseObjectAssetPaths(*this, Result) &&
		Result.bSuccess && Result.Data.Assets.Num() > 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetDiscoveryFindAssetsSemanticTypeMapsToClassPathTest,
	"BlueprintHelper.AssetDiscovery.FindAssets.SemanticTypeMapsToClassPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetDiscoveryFindAssetsSemanticTypeMapsToClassPathTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperFindAssetsRequest Request;
	Request.PathPrefixes.Add(BlueprintHelperAssetDiscoveryServiceTests::CodexSmokePath);
	Request.AssetTypes.Add(TEXT("blueprint"));
	Request.Query = TEXT("GraphLocalValueProducer_20260527_001");
	Request.Limit = 10;

	const FBlueprintHelperAssetDiscoveryService Service;
	const FBlueprintHelperFindAssetsResult Result = Service.FindAssets(Request);

	BlueprintHelperAssetDiscoveryServiceTests::AssertCompactFindAssetsResult(*this, Result, 10);
	TestTrue(TEXT("semantic blueprint query finds the fixture"), Result.Data.Assets.Num() > 0);
	for (const FBlueprintHelperAssetListItem& Asset : Result.Data.Assets)
	{
		TestEqual(TEXT("semantic asset type is reported"), Asset.AssetType, FString(TEXT("blueprint")));
		TestEqual(TEXT("semantic type maps to exact class path"), Asset.AssetClass, FString(BlueprintHelperAssetDiscoveryServiceTests::BlueprintClassPath));
	}

	return BlueprintHelperAssetDiscoveryServiceTests::AssertAllResultsUseObjectAssetPaths(*this, Result) &&
		Result.bSuccess && Result.Data.Assets.Num() > 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetDiscoveryFindAssetsFullClassPathFiltersExactlyTest,
	"BlueprintHelper.AssetDiscovery.FindAssets.FullClassPathFiltersExactly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetDiscoveryFindAssetsFullClassPathFiltersExactlyTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperFindAssetsRequest Request;
	Request.PathPrefixes.Add(BlueprintHelperAssetDiscoveryServiceTests::CodexSmokePath);
	Request.AssetClasses.Add(BlueprintHelperAssetDiscoveryServiceTests::BlueprintClassPath);
	Request.Limit = 20;

	const FBlueprintHelperAssetDiscoveryService Service;
	const FBlueprintHelperFindAssetsResult Result = Service.FindAssets(Request);

	BlueprintHelperAssetDiscoveryServiceTests::AssertCompactFindAssetsResult(*this, Result, 20);
	TestTrue(TEXT("full class path query finds blueprint fixtures"), Result.Data.Assets.Num() > 0);
	for (const FBlueprintHelperAssetListItem& Asset : Result.Data.Assets)
	{
		TestEqual(TEXT("full class path filters exactly"), Asset.AssetClass, FString(BlueprintHelperAssetDiscoveryServiceTests::BlueprintClassPath));
	}

	return BlueprintHelperAssetDiscoveryServiceTests::AssertAllResultsUseObjectAssetPaths(*this, Result) &&
		Result.bSuccess && Result.Data.Assets.Num() > 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetDiscoveryFindAssetsReturnsLimitAndHasMoreTest,
	"BlueprintHelper.AssetDiscovery.FindAssets.ReturnsLimitAndHasMore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetDiscoveryFindAssetsReturnsLimitAndHasMoreTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperFindAssetsRequest Request;
	Request.PathPrefixes.Add(BlueprintHelperAssetDiscoveryServiceTests::GeneralityPath);
	Request.AssetTypes.Add(TEXT("blueprint"));
	Request.Query = TEXT("GraphWriteGenerality_GraphWrite80_RealCaseE2E_20260527_001_container_");
	Request.Limit = 1;

	const FBlueprintHelperAssetDiscoveryService Service;
	const FBlueprintHelperFindAssetsResult Result = Service.FindAssets(Request);

	BlueprintHelperAssetDiscoveryServiceTests::AssertCompactFindAssetsResult(*this, Result, 1);
	TestEqual(TEXT("result count is capped at limit"), Result.Data.Assets.Num(), 1);
	TestTrue(TEXT("page reports has_more when enumeration exceeded limit"), Result.Data.Page.bHasMore);

	return BlueprintHelperAssetDiscoveryServiceTests::AssertAllResultsUseObjectAssetPaths(*this, Result) &&
		Result.bSuccess && Result.Data.Assets.Num() == 1 && Result.Data.Page.bHasMore;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetDiscoveryFindAssetsExcludesRedirectorsByDefaultTest,
	"BlueprintHelper.AssetDiscovery.FindAssets.ExcludesRedirectorsByDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetDiscoveryFindAssetsExcludesRedirectorsByDefaultTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperFindAssetsRequest Request;
	Request.AssetClasses.Add(BlueprintHelperAssetDiscoveryServiceTests::RedirectorClassPath);
	Request.Limit = 100;

	const FBlueprintHelperAssetDiscoveryService Service;
	const FBlueprintHelperFindAssetsResult Result = Service.FindAssets(Request);

	BlueprintHelperAssetDiscoveryServiceTests::AssertCompactFindAssetsResult(*this, Result, 100);
	TestEqual(TEXT("redirectors are excluded by default"), Result.Data.Assets.Num(), 0);
	for (const FBlueprintHelperAssetListItem& Asset : Result.Data.Assets)
	{
		TestNotEqual(TEXT("redirector class is not returned"), Asset.AssetClass, FString(BlueprintHelperAssetDiscoveryServiceTests::RedirectorClassPath));
	}

	return Result.bSuccess && Result.Data.Assets.Num() == 0;
}

#endif
