#include "Systems/Debug/BlueprintHelperDiagnosticProjection.h"

FBlueprintHelperDiagnosticProjection FBlueprintHelperDiagnosticProjectionUtils::FromDiagnosticItem(
	const FBlueprintHelperDiagnosticItem& Item,
	const FString& Source,
	const FString& AssetPath,
	const FString& ScopeIdentity)
{
	FBlueprintHelperDiagnosticProjection Projection;
	Projection.Source = Source;
	Projection.Code = Item.Code;
	Projection.Message = Item.Message;
	Projection.Severity = BlueprintHelperDiagnosticSeverityToString(Item.Severity);
	Projection.AssetPath = AssetPath;
	Projection.GraphName = Item.GraphName;
	Projection.TargetKey = Item.TargetKey;
	Projection.ScopeIdentity = ScopeIdentity;
	Projection.Details = BlueprintHelperDiagnosticItemToJson(Item);
	return Projection;
}
