// BlueprintHelper Bridge Layer 闁?TCP Bridge Server 閻庡湱鍋熼獮?

#include "Entry/Bridge/BlueprintHelperBridgeServer.h"
#include "Entry/Bridge/BlueprintHelperBridgeRouter.h"
#include "Entry/Bridge/BlueprintHelperBridgeProtocol.h"
#include "Entry/Bridge/Utils/BlueprintHelperBridgeTransportTimingUtils.h"
#include "Entry/Bridge/Utils/BlueprintHelperBridgeUtils.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "Common/TcpSocketBuilder.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

DEFINE_LOG_CATEGORY(LogBlueprintHelperBridge);

FBlueprintHelperBridgeServer::FBlueprintHelperBridgeServer(
	FBlueprintHelperBridgeRouter& InRouter,
	const FBlueprintHelperBridgeRuntimeConfig& InConfig,
	const FBlueprintHelperDebugEntryService* InDebugEntryService)
	: Router(InRouter)
	, DebugEntryService(InDebugEntryService)
	, Config(InConfig)
	, Port(InConfig.Port)
{
}

FBlueprintHelperBridgeServer::FBlueprintHelperBridgeServer(
	FBlueprintHelperBridgeRouter& InRouter,
	int32 InPort,
	const FBlueprintHelperDebugEntryService* InDebugEntryService)
	: FBlueprintHelperBridgeServer(InRouter, UBlueprintHelperBridgeUtils::BridgeConfigWithPort(InPort), InDebugEntryService)
{
}

FBlueprintHelperBridgeServer::~FBlueprintHelperBridgeServer()
{
	Shutdown();
}

bool FBlueprintHelperBridgeServer::Start()
{
	if (Thread.IsValid())
	{
		UE_LOG(LogBlueprintHelperBridge, Warning, TEXT("Bridge server is already running."));
		return false;
	}

	// 闁告帗绋戠紓鎾绘儎閹存繃鍎?socket
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		UE_LOG(LogBlueprintHelperBridge, Error, TEXT("Failed to acquire SocketSubsystem."));
		return false;
	}

	ListenerSocket = FTcpSocketBuilder(TEXT("BlueprintHelperBridge"))
		.AsReusable()
		.BoundToAddress(FIPv4Address(127, 0, 0, 1))
		.BoundToPort(Port)
		.Listening(Config.MaxPendingConnections)
		.WithSendBufferSize(Config.SocketBufferBytes)
		.WithReceiveBufferSize(Config.SocketBufferBytes)
		.Build();

	if (!ListenerSocket)
	{
		UE_LOG(
			LogBlueprintHelperBridge,
			Error,
			TEXT("Failed to create listener socket on 127.0.0.1:%d. On Windows, check excluded TCP ranges with: netsh interface ipv4 show excludedportrange protocol=tcp"),
			Port);
		return false;
	}

	bStopping = false;
	Thread = TUniquePtr<FRunnableThread>(
		FRunnableThread::Create(this, TEXT("BlueprintHelperBridgeThread"), 0, TPri_Normal));

	UE_LOG(LogBlueprintHelperBridge, Log, TEXT("Bridge server started on 127.0.0.1:%d."), Port);
	return true;
}

void FBlueprintHelperBridgeServer::Shutdown()
{
	bStopping = true;

	if (ListenerSocket)
	{
		ListenerSocket->Close();
	}

	if (Thread.IsValid())
	{
		Thread->WaitForCompletion();
		Thread.Reset();
	}

	if (ListenerSocket)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket);
		ListenerSocket = nullptr;
	}

	UE_LOG(LogBlueprintHelperBridge, Log, TEXT("Bridge server stopped."));
}

uint32 FBlueprintHelperBridgeServer::Run()
{
	const FTimespan AcceptWaitTime = FTimespan::FromMilliseconds(Config.AcceptWaitMs);

	while (!bStopping)
	{
		bool bHasPendingConnection = false;
		if (!ListenerSocket->WaitForPendingConnection(bHasPendingConnection, AcceptWaitTime))
		{
			continue;
		}
		if (!bHasPendingConnection)
		{
			continue;
		}

		TSharedRef<FInternetAddr> RemoteAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		FSocket* ClientSocket = ListenerSocket->Accept(*RemoteAddr, TEXT("BlueprintHelperBridgeClient"));
		if (!ClientSocket)
		{
			continue;
		}

	UE_LOG(LogBlueprintHelperBridge, Verbose, TEXT("Bridge client connected."));
		HandleClient(ClientSocket);

		ClientSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
	UE_LOG(LogBlueprintHelperBridge, Verbose, TEXT("Bridge client disconnected."));
	}

	return 0;
}

void FBlueprintHelperBridgeServer::Stop()
{
	bStopping = true;
}

void FBlueprintHelperBridgeServer::HandleClient(FSocket* ClientSocket)
{
	const double IdleTimeoutSeconds = FMath::Max(0.01, Config.IdleTimeoutSeconds);
	double LastActivityTime = FPlatformTime::Seconds();

	while (!bStopping)
	{
		uint32 PendingDataSize = 0;
		if (!ClientSocket->HasPendingData(PendingDataSize) || PendingDataSize == 0)
		{
			const ESocketConnectionState State = ClientSocket->GetConnectionState();
			if (State != SCS_Connected)
			{
				break;
			}

			if ((FPlatformTime::Seconds() - LastActivityTime) >= IdleTimeoutSeconds)
			{
				UE_LOG(LogBlueprintHelperBridge, Verbose, TEXT("Bridge client idle timeout; closing connection."));
				break;
			}

			FPlatformProcess::Sleep(0.01f);
			continue;
		}

		LastActivityTime = FPlatformTime::Seconds();

		const double ReceiveStageStart = FPlatformTime::Seconds();
		FString RequestJson;
		if (!ReadMessage(ClientSocket, RequestJson))
		{
			RecordBridgeFailureBestEffort(
				TEXT("bridge_transport_failure"),
				TEXT("bridge"),
				TEXT("transport_read_failed"),
				TEXT("Bridge request frame could not be read."));
			break;
		}
		const double ReceiveStageEnd = FPlatformTime::Seconds();

		UE_LOG(LogBlueprintHelperBridge, Verbose, TEXT("闁衡偓鐠哄搫鐓傞悹鍥敱閻? %s"), *RequestJson.Left(200));

		// 闁硅埖娲熼埀顒佸笒閸?GameThread 妤犵偛澧庨悺鎴濐嚗閸涱垳娉㈤柡?
		const TOptional<FBlueprintHelperBridgeRequest> Req =
			FBlueprintHelperBridgeProtocol::ParseRequest(RequestJson);
		const FBlueprintHelperBridgeRoutePlan RoutePlan = Req.IsSet()
			? FBlueprintHelperBridgeRoutePlanner::BuildPlan(Req.GetValue().Command)
			: FBlueprintHelperBridgeRoutePlan();
		FBlueprintHelperBridgeTransportTimingUtils::FTimingTrace TransportTiming =
			FBlueprintHelperBridgeTransportTimingUtils::StartTrace(Req, ReceiveStageStart);
		FBlueprintHelperBridgeTransportTimingUtils::AddStage(
			TransportTiming,
			TEXT("bridge.receive"),
			ReceiveStageStart,
			ReceiveStageEnd);

		FString ResponseJson;
		if (Req.IsSet() && Req.GetValue().Command == TEXT("ping"))
		{
			FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Success(Req.GetValue().RequestId);
			Resp.Result = MakeShared<FJsonObject>();
			Resp.Result->SetStringField(TEXT("schema"), TEXT("BridgePing.v1"));
			Resp.Result->SetBoolField(TEXT("reachable"), true);
			ResponseJson = FBlueprintHelperBridgeTransportTimingUtils::SerializeResponseWithTiming(Resp, TransportTiming);
		}
		else if (Req.IsSet() && Req.GetValue().Command == TEXT("client_disconnect"))
		{
			FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Success(Req.GetValue().RequestId);
			Resp.Result = MakeShared<FJsonObject>();
			Resp.Result->SetStringField(TEXT("schema"), TEXT("BridgeClientDisconnect.v1"));
			Resp.Result->SetBoolField(TEXT("closing"), true);
			ResponseJson = FBlueprintHelperBridgeTransportTimingUtils::SerializeResponseWithTiming(Resp, TransportTiming);
		}
		else
		{
			TPromise<FString> Promise;
			TFuture<FString> Future = Promise.GetFuture();

			const double GameThreadEnqueueStageStart =
				FBlueprintHelperBridgeTransportTimingUtils::StartStage(TransportTiming);
			AsyncTask(
				ENamedThreads::GameThread,
				[this, Req, RoutePlan, &Promise, &TransportTiming, GameThreadEnqueueStageStart]()
			{
				const double GameThreadStart = FPlatformTime::Seconds();
				FBlueprintHelperBridgeTransportTimingUtils::AddStage(
					TransportTiming,
					TEXT("bridge.game_thread_enqueue_wait"),
					GameThreadEnqueueStageStart,
					GameThreadStart);

				FBlueprintHelperBridgeResponse Resp;
				const double RouteStageStart =
					FBlueprintHelperBridgeTransportTimingUtils::StartStage(TransportTiming);
				if (Req.IsSet())
				{
					Resp = Router.HandleRequestWithPlan(Req.GetValue(), RoutePlan);
				}
				else
				{
					if (DebugEntryService)
					{
						FBlueprintHelperDebugEntryEventInput DebugInput;
						DebugInput.SourceLayer = TEXT("bridge");
						DebugInput.Source = TEXT("malformed_bridge_request");
						DebugInput.Operation = TEXT("bridge");
						DebugInput.Stage = TEXT("parse_input");
						DebugInput.Error.Code = TEXT("json_parse_failed");
						DebugInput.Error.Message = TEXT("Bridge request JSON parse failed.");
						DebugInput.RecommendedNext = TEXT("send_valid_bridge_request");
						DebugEntryService->RecordEventBestEffort(DebugInput);
					}
					Resp = FBlueprintHelperBridgeResponse::Error(
						TEXT(""), EBlueprintHelperBridgeError::InvalidRequest,
						TEXT("Bridge request JSON parse failed."));
				}

				FBlueprintHelperBridgeTransportTimingUtils::AddStage(
					TransportTiming,
					TEXT("bridge.route_execute"),
					RouteStageStart,
					FPlatformTime::Seconds());
				Promise.SetValue(FBlueprintHelperBridgeTransportTimingUtils::SerializeResponseWithTiming(
					Resp,
					TransportTiming));
			});

			ResponseJson = Future.Get();
		}

		UE_LOG(LogBlueprintHelperBridge, Verbose, TEXT("闁告瑦鍨块埀顑跨閹奸攱鎯? %s"), *ResponseJson.Left(200));

		if (!WriteMessage(ClientSocket, ResponseJson))
		{
			RecordBridgeFailureBestEffort(
				TEXT("bridge_transport_failure"),
				TEXT("bridge"),
				TEXT("transport_write_failed"),
				TEXT("Bridge response frame could not be written."),
				Req.IsSet() ? Req.GetValue().RequestId : FString());
			break;
		}
		if (Req.IsSet() && Req.GetValue().bCloseAfterResponse)
		{
			break;
		}
		LastActivityTime = FPlatformTime::Seconds();
	}
}

void FBlueprintHelperBridgeServer::RecordBridgeFailureBestEffort(
	const FString& Source,
	const FString& Stage,
	const FString& Code,
	const FString& Message,
	const FString& RequestId) const
{
	if (!DebugEntryService)
	{
		return;
	}

	const FBlueprintHelperDebugEntryService* DebugService = DebugEntryService;
	AsyncTask(ENamedThreads::GameThread, [DebugService, Source, Stage, Code, Message, RequestId]()
	{
		FBlueprintHelperDebugEntryEventInput DebugInput;
		DebugInput.SourceLayer = TEXT("bridge");
		DebugInput.Source = Source;
		DebugInput.Operation = TEXT("bridge");
		DebugInput.Stage = Stage;
		DebugInput.TraceId = RequestId;
		DebugInput.Error.Code = Code;
		DebugInput.Error.Message = Message;
		DebugInput.RecommendedNext = TEXT("retry_bridge_request");
		DebugService->RecordEventBestEffort(DebugInput);
	});
}

bool FBlueprintHelperBridgeServer::ReadMessage(FSocket* Socket, FString& OutJson) const
{
	// 閻?4 閻庢稒顨夋俊顓㈡⒐閸喖顔婂鎯版彧缁辨瑦寰勮椤忣剟鏁?
	uint8 LengthBytes[4];
	int32 BytesRead = 0;
	int32 TotalRead = 0;

	while (TotalRead < 4)
	{
		if (!Socket->Recv(LengthBytes + TotalRead, 4 - TotalRead, BytesRead))
		{
			return false;
		}
		if (BytesRead <= 0)
		{
			return false;
		}
		TotalRead += BytesRead;
	}

	const uint32 BodyLength =
		(static_cast<uint32>(LengthBytes[0]) << 24) |
		(static_cast<uint32>(LengthBytes[1]) << 16) |
		(static_cast<uint32>(LengthBytes[2]) << 8) |
		static_cast<uint32>(LengthBytes[3]);

	// 閻庣懓顦崣蹇涙⒔閹邦剙鐓戦柨娑欑濞撹埖寰?16MB
	const uint32 MaxFrameBytes = static_cast<uint32>(FMath::Max(1, Config.MaxFrameBytes));
	if (BodyLength == 0 || BodyLength > MaxFrameBytes)
	{
		UE_LOG(LogBlueprintHelperBridge, Warning, TEXT("婵炴垵鐗婃导鍛存⒐閸喖顔婄€殿喖鍊搁悥? %u"), BodyLength);
		RecordBridgeFailureBestEffort(
			TEXT("bridge_transport_failure"),
			TEXT("bridge"),
			TEXT("invalid_frame_length"),
			FString::Printf(TEXT("Bridge frame length is invalid: %u."), BodyLength));
		return false;
	}

	// 閻?body
	TArray<uint8> BodyBytes;
	BodyBytes.SetNumUninitialized(BodyLength);
	TotalRead = 0;

	while (static_cast<uint32>(TotalRead) < BodyLength)
	{
		if (!Socket->Recv(BodyBytes.GetData() + TotalRead, BodyLength - TotalRead, BytesRead))
		{
			return false;
		}
		if (BytesRead <= 0)
		{
			return false;
		}
		TotalRead += BytesRead;
	}

	// UTF-8 闁?FString
	FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(BodyBytes.GetData()), BodyLength);
	OutJson = FString(Converter.Length(), Converter.Get());
	return true;
}

bool FBlueprintHelperBridgeServer::WriteMessage(FSocket* Socket, const FString& Json) const
{
	// FString 闁?UTF-8
	FTCHARToUTF8 Converter(*Json);
	const uint32 BodyLength = static_cast<uint32>(Converter.Length());

	// 闁?4 閻庢稒顨夋俊顓㈡⒐閸喖顔婂鎯版彧缁辨瑦寰勮椤忣剟鏁?
	uint8 LengthBytes[4];
	LengthBytes[0] = static_cast<uint8>((BodyLength >> 24) & 0xFF);
	LengthBytes[1] = static_cast<uint8>((BodyLength >> 16) & 0xFF);
	LengthBytes[2] = static_cast<uint8>((BodyLength >> 8) & 0xFF);
	LengthBytes[3] = static_cast<uint8>(BodyLength & 0xFF);

	int32 BytesSent = 0;
	if (!Socket->Send(LengthBytes, 4, BytesSent) || BytesSent != 4)
	{
		return false;
	}

	// 闁?body
	int32 TotalSent = 0;
	while (static_cast<uint32>(TotalSent) < BodyLength)
	{
		if (!Socket->Send(
			reinterpret_cast<const uint8*>(Converter.Get()) + TotalSent,
			BodyLength - TotalSent, BytesSent))
		{
			return false;
		}
		if (BytesSent <= 0)
		{
			return false;
		}
		TotalSent += BytesSent;
	}

	return true;
}
