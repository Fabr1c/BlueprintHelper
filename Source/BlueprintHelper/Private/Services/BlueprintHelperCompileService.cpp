// BlueprintHelper Service Layer — 编译服务实现

#include "Services/BlueprintHelperCompileService.h"
#include "Services/BlueprintHelperGraphResolver.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"

FBlueprintHelperCompileService::FBlueprintHelperCompileService(const FBlueprintHelperGraphResolver& InResolver)
	: Resolver(InResolver)
{
}

FBlueprintHelperCompileResult FBlueprintHelperCompileService::Compile(const FBlueprintHelperGraphTarget& Target) const
{
	FBlueprintHelperCompileResult Result;

	UBlueprint* Blueprint = Resolver.ResolveBlueprint(Target, Result.Diagnostics);
	if (!Blueprint)
	{
		return Result;
	}

	// 触发编译
	FCompilerResultsLog LogResults;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &LogResults);

	// 收集状态
	Result.BlueprintStatus = static_cast<int32>(Blueprint->Status);
	Result.bSuccess = (Blueprint->Status != BS_Error);

	// 从编译日志提取消息
	for (const TSharedRef<FTokenizedMessage>& Msg : LogResults.Messages)
	{
		EBlueprintHelperDiagnosticSeverity Sev;
		switch (Msg->GetSeverity())
		{
		case EMessageSeverity::Error:
			Sev = EBlueprintHelperDiagnosticSeverity::Error;
			break;
		case EMessageSeverity::Warning:
		case EMessageSeverity::PerformanceWarning:
			Sev = EBlueprintHelperDiagnosticSeverity::Warning;
			break;
		default:
			Sev = EBlueprintHelperDiagnosticSeverity::Info;
			break;
		}
		Result.Diagnostics.Add(Sev, Msg->ToText().ToString());
	}

	return Result;
}

FBlueprintHelperCompileResult FBlueprintHelperCompileService::GetStatus(const FBlueprintHelperGraphTarget& Target) const
{
	FBlueprintHelperCompileResult Result;

	UBlueprint* Blueprint = Resolver.ResolveBlueprint(Target, Result.Diagnostics);
	if (!Blueprint)
	{
		return Result;
	}

	Result.BlueprintStatus = static_cast<int32>(Blueprint->Status);
	Result.bSuccess = (Blueprint->Status != BS_Error);

	if (Blueprint->Status == BS_Error)
	{
		Result.Diagnostics.Add(EBlueprintHelperDiagnosticSeverity::Error,
			FString::Printf(TEXT("蓝图 %s 当前编译状态为 Error。"), *Blueprint->GetName()));
	}

	return Result;
}
