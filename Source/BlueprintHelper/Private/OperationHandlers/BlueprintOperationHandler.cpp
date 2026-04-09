#include "OperationHandlers/BlueprintOperationHandler.h"

FBlueprintOperationHandlerRegistry& FBlueprintOperationHandlerRegistry::Get()
{
	static FBlueprintOperationHandlerRegistry Instance;
	return Instance;
}

void FBlueprintOperationHandlerRegistry::Register(TSharedRef<IBlueprintOperationHandler> Handler)
{
	Handlers.Insert(Handler, 0);
}

IBlueprintOperationHandler* FBlueprintOperationHandlerRegistry::FindHandler(const FString& OpName) const
{
	for (const TSharedRef<IBlueprintOperationHandler>& Handler : Handlers)
	{
		if (Handler->CanHandle(OpName))
		{
			return &Handler.Get();
		}
	}
	return nullptr;
}

void FBlueprintOperationHandlerRegistry::Reset()
{
	Handlers.Empty();
}
