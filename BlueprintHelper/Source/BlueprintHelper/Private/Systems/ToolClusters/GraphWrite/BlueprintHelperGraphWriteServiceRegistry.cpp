// BlueprintHelper GraphWrite operation registry.

#include "Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteServiceRegistry.h"

#include "Dom/JsonObject.h"

namespace BlueprintHelperGraphWriteServiceRegistry
{
	static const TSet<FString>& KnownOperations()
	{
		static const TSet<FString> Operations = {
			TEXT("append_blueprint_graph"),
			TEXT("replace_blueprint_graph"),
			TEXT("patch_blueprint_graph"),
			TEXT("merge_blueprint_graph"),
			TEXT("merge_external_flow"),
			TEXT("patch_external_graph"),
			TEXT("replace_external_body"),
		};
		return Operations;
	}
}

bool FBlueprintHelperGraphWriteServiceRegistry::IsKnownOperation(const FString& Operation)
{
	return BlueprintHelperGraphWriteServiceRegistry::KnownOperations().Contains(NormalizeOperation(Operation));
}

void FBlueprintHelperGraphWriteServiceRegistry::RegisterHandler(
	const FString& Operation,
	FExecuteHandler Handler)
{
	const FString Key = NormalizeOperation(Operation);
	if (Key.IsEmpty() || !Handler)
	{
		return;
	}
	Handlers.Add(Key, MoveTemp(Handler));
}

bool FBlueprintHelperGraphWriteServiceRegistry::HasHandler(const FString& Operation) const
{
	return Handlers.Contains(NormalizeOperation(Operation));
}

FBlueprintHelperToolResultBase FBlueprintHelperGraphWriteServiceRegistry::Execute(
	const FString& Operation,
	const TSharedRef<FJsonObject>& Payload) const
{
	const FString Key = NormalizeOperation(Operation);
	if (const FExecuteHandler* Handler = Handlers.Find(Key))
	{
		return (*Handler)(Payload);
	}

	return FBlueprintHelperToolResultBuilder::Failure(
		Operation.IsEmpty() ? TEXT("graph_write") : Operation,
		FBlueprintHelperToolResultBuilder::GenerateTraceId(),
		MakeUnsupportedOperationError(Operation, IsKnownOperation(Operation)));
}

FString FBlueprintHelperGraphWriteServiceRegistry::NormalizeOperation(const FString& Operation)
{
	FString Key = Operation;
	Key.TrimStartAndEndInline();
	return Key.ToLower();
}

FBlueprintHelperToolError FBlueprintHelperGraphWriteServiceRegistry::MakeUnsupportedOperationError(
	const FString& Operation,
	bool bKnownOperation)
{
	FBlueprintHelperToolError Error;
	Error.Code = bKnownOperation
		? TEXT("graph_write_operation_unregistered")
		: TEXT("unsupported_graph_write_operation");
	Error.Stage = EBlueprintHelperToolStage::ParseInput;
	Error.Message = bKnownOperation
		? FString::Printf(TEXT("GraphWrite operation '%s' is known but has no registered handler."), *Operation)
		: FString::Printf(TEXT("Unsupported GraphWrite operation '%s'."), *Operation);
	Error.Field = TEXT("command");
	Error.bRetryable = false;
	Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
	return Error;
}
