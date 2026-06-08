#include "Generated/BlueprintHelperReadContextRouteManifest.generated.h"

#include "Containers/Set.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

class FBlueprintHelperReadContextRouteManifestTestUtils
{
public:
	static bool IsEmpty(const TCHAR* Value)
	{
		return Value == nullptr || FCString::Strlen(Value) == 0;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReadContextGeneratedRouteMirrorTest,
	"BlueprintHelper.ReadContext.RouteManifest.GeneratedMirror",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReadContextGeneratedRouteMirrorTest::RunTest(const FString&)
{
	TestTrue(TEXT("generated ReadContext route manifest has rows"), GBlueprintHelperReadContextRouteCount > 0);

	TSet<FString> RouteIds;
	int32 ActiveRoutes = 0;
	for (const FBlueprintHelperGeneratedReadContextRouteDescriptor& Route : GBlueprintHelperReadContextRoutes)
	{
		TestFalse(TEXT("route id is populated"), FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.RouteId));
		TestFalse(TEXT("domain is populated"), FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.Domain));
		TestFalse(TEXT("read cluster is populated"), FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.ReadCluster));
		TestFalse(TEXT("target kind is populated"), FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.TargetKind));
		TestFalse(TEXT("view template is populated"), FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.ViewTemplate));
		TestFalse(TEXT("status is populated"), FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.Status));

		const FString RouteId(Route.RouteId);
		TestFalse(FString::Printf(TEXT("route id is unique: %s"), *RouteId), RouteIds.Contains(RouteId));
		RouteIds.Add(RouteId);

		const FString Status(Route.Status);
		if (Status == TEXT("active"))
		{
			++ActiveRoutes;
			TestFalse(
				FString::Printf(TEXT("active route has command: %s"), *RouteId),
				FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.Command));
			TestFalse(
				FString::Printf(TEXT("active route has cluster: %s"), *RouteId),
				FBlueprintHelperReadContextRouteManifestTestUtils::IsEmpty(Route.Cluster));
		}
	}

	TestTrue(TEXT("generated ReadContext route mirror exposes active routes"), ActiveRoutes > 0);
	return true;
}

#endif
