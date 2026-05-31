#include "Systems/ToolClusters/GraphWrite/Policy/BlueprintHelperGraphWriteDomainPolicy.h"

bool FBlueprintHelperGraphWriteDomainPolicy::ValidateOwnedRequest(
	const FBlueprintHelperGraphWriteDomainPolicyRequest& Request,
	FString& OutError)
{
	if (Request.Domain != EBlueprintHelperGraphWriteTargetDomain::BlueprintHelperOwned)
	{
		OutError = TEXT("Owned GraphWrite requests must use the BlueprintHelperOwned target domain.");
		return false;
	}
	if (Request.Strategy != TEXT("owned_graph_edit"))
	{
		OutError = TEXT("Owned GraphWrite requests must use write.strategy=owned_graph_edit.");
		return false;
	}
	if (Request.OwnershipScope != TEXT("blueprinthelper_owned"))
	{
		OutError = TEXT("Owned GraphWrite requests must use constraints.ownership_scope=blueprinthelper_owned.");
		return false;
	}
	if (Request.bAllowModifyUserNodes)
	{
		OutError = TEXT("Owned GraphWrite requests must not set constraints.allow_modify_user_nodes=true.");
		return false;
	}
	return true;
}

bool FBlueprintHelperGraphWriteDomainPolicy::ValidateExternalRequest(
	const FBlueprintHelperGraphWriteDomainPolicyRequest& Request,
	FString& OutError)
{
	if (Request.Domain != EBlueprintHelperGraphWriteTargetDomain::ExternalUserAuthored)
	{
		OutError = TEXT("External GraphWrite requests must use the ExternalUserAuthored target domain.");
		return false;
	}
	if (Request.Strategy != TEXT("external_graph_edit"))
	{
		OutError = TEXT("External GraphWrite requests must use write.strategy=external_graph_edit.");
		return false;
	}
	if (Request.OwnershipScope != TEXT("external_user_authored"))
	{
		OutError = TEXT("External GraphWrite requests must use constraints.ownership_scope=external_user_authored.");
		return false;
	}
	if (Request.bAllowModifyUserNodes)
	{
		OutError = TEXT("External GraphWrite requests must use explicit external mutation policy instead of allow_modify_user_nodes=true.");
		return false;
	}
	if (Request.AllowedExternalMutations.Num() == 0)
	{
		OutError = TEXT("External GraphWrite requests require a non-empty explicit mutation allowlist.");
		return false;
	}
	return true;
}
