#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/BlueprintSignature/BlueprintHelperSignatureTestFixtures.h"

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"

FString FBlueprintHelperSignatureTestFixtures::MakeSignatureServiceTestObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

UBlueprint* FBlueprintHelperSignatureTestFixtures::MakeSignatureServiceActorBlueprint(const FString& Prefix)
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperSignature/%s"),
		*MakeSignatureServiceTestObjectName(Prefix)));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeSignatureServiceTestObjectName(TEXT("BP_SignatureService")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperSignatureServiceTests"));
	if (Blueprint)
	{
		// Keep service unit tests isolated from unrelated dirty project Blueprints queued for skeleton compile.
		Blueprint->Status = BS_BeingCreated;
	}
	Package->SetDirtyFlag(false);
	return Blueprint;
}

#endif
