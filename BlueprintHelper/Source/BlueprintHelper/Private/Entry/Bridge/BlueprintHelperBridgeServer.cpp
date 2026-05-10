// BlueprintHelper Bridge Layer — TCP Bridge Server 实现

#include "Entry/Bridge/BlueprintHelperBridgeServer.h"
#include "Entry/Bridge/BlueprintHelperBridgeRouter.h"
#include "Entry/Bridge/BlueprintHelperBridgeProtocol.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Async/Async.h"
#include "Common/TcpSocketBuilder.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

DEFINE_LOG_CATEGORY(LogBlueprintHelperBridge);

FBlueprintHelperBridgeServer::FBlueprintHelperBridgeServer(
	FBlueprintHelperBridgeRouter& InRouter,
	int32 InPort,
	const FBlueprintHelperDebugEntryService* InDebugEntryService)
	: Router(InRouter)
	, DebugEntryService(InDebugEntryService)
	, Port(InPort)
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
		UE_LOG(LogBlueprintHelperBridge, Warning, TEXT("Bridge Server 已在运行中。"));
		return false;
	}

	// 创建监听 socket
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		UE_LOG(LogBlueprintHelperBridge, Error, TEXT("无法获取 SocketSubsystem。"));
		return false;
	}

	ListenerSocket = FTcpSocketBuilder(TEXT("BlueprintHelperBridge"))
		.AsReusable()
		.BoundToAddress(FIPv4Address(127, 0, 0, 1))
		.BoundToPort(Port)
		.Listening(8)
		.WithSendBufferSize(256 * 1024)
		.WithReceiveBufferSize(256 * 1024)
		.Build();

	if (!ListenerSocket)
	{
		UE_LOG(LogBlueprintHelperBridge, Error, TEXT("无法创建监听 Socket, 端口 %d。"), Port);
		return false;
	}

	bStopping = false;
	Thread = TUniquePtr<FRunnableThread>(
		FRunnableThread::Create(this, TEXT("BlueprintHelperBridgeThread"), 0, TPri_Normal));

	UE_LOG(LogBlueprintHelperBridge, Log, TEXT("Bridge Server 已启动，监听 127.0.0.1:%d。"), Port);
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

	UE_LOG(LogBlueprintHelperBridge, Log, TEXT("Bridge Server 已停止。"));
}

uint32 FBlueprintHelperBridgeServer::Run()
{
	while (!bStopping)
	{
		bool bHasPendingConnection = false;
		if (!ListenerSocket->HasPendingConnection(bHasPendingConnection) || !bHasPendingConnection)
		{
			FPlatformProcess::Sleep(0.05f);
			continue;
		}

		TSharedRef<FInternetAddr> RemoteAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		FSocket* ClientSocket = ListenerSocket->Accept(*RemoteAddr, TEXT("BlueprintHelperBridgeClient"));
		if (!ClientSocket)
		{
			continue;
		}

		UE_LOG(LogBlueprintHelperBridge, Log, TEXT("Bridge 客户端已连接。"));
		HandleClient(ClientSocket);

		ClientSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
		UE_LOG(LogBlueprintHelperBridge, Log, TEXT("Bridge 客户端已断开。"));
	}

	return 0;
}

void FBlueprintHelperBridgeServer::Stop()
{
	bStopping = true;
}

void FBlueprintHelperBridgeServer::HandleClient(FSocket* ClientSocket)
{
	while (!bStopping)
	{
		// 检查是否有数据可读
		uint32 PendingDataSize = 0;
		if (!ClientSocket->HasPendingData(PendingDataSize))
		{
			// 连接可能已关闭
			break;
		}

		if (PendingDataSize == 0)
		{
			// 检查连接是否还活着
			ESocketConnectionState State = ClientSocket->GetConnectionState();
			if (State != SCS_Connected)
			{
				break;
			}
			FPlatformProcess::Sleep(0.01f);
			continue;
		}

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

		UE_LOG(LogBlueprintHelperBridge, Verbose, TEXT("收到请求: %s"), *RequestJson.Left(200));

		// 投递到 GameThread 并等待结果
		const TOptional<FBlueprintHelperBridgeRequest> Req =
			FBlueprintHelperBridgeProtocol::ParseRequest(RequestJson);
		const FBlueprintHelperBridgeRoutePlan RoutePlan = Req.IsSet()
			? FBlueprintHelperBridgeRoutePlanner::BuildPlan(Req.GetValue().Command)
			: FBlueprintHelperBridgeRoutePlan();

		TPromise<FString> Promise;
		TFuture<FString> Future = Promise.GetFuture();

		AsyncTask(ENamedThreads::GameThread, [this, Req, RoutePlan, &Promise]()
		{
			FBlueprintHelperBridgeResponse Resp;
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
					TEXT("请求 JSON 解析失败。"));
			}

			Promise.SetValue(FBlueprintHelperBridgeProtocol::SerializeResponse(Resp));
		});

		const FString ResponseJson = Future.Get();

		UE_LOG(LogBlueprintHelperBridge, Verbose, TEXT("发送响应: %s"), *ResponseJson.Left(200));

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
	// 读 4 字节长度头（大端）
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

	// 安全限制：最大 16MB
	if (BodyLength == 0 || BodyLength > 16 * 1024 * 1024)
	{
		UE_LOG(LogBlueprintHelperBridge, Warning, TEXT("消息长度异常: %u"), BodyLength);
		RecordBridgeFailureBestEffort(
			TEXT("bridge_transport_failure"),
			TEXT("bridge"),
			TEXT("invalid_frame_length"),
			FString::Printf(TEXT("Bridge frame length is invalid: %u."), BodyLength));
		return false;
	}

	// 读 body
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

	// UTF-8 → FString
	FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(BodyBytes.GetData()), BodyLength);
	OutJson = FString(Converter.Length(), Converter.Get());
	return true;
}

bool FBlueprintHelperBridgeServer::WriteMessage(FSocket* Socket, const FString& Json) const
{
	// FString → UTF-8
	FTCHARToUTF8 Converter(*Json);
	const uint32 BodyLength = static_cast<uint32>(Converter.Length());

	// 写 4 字节长度头（大端）
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

	// 写 body
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
