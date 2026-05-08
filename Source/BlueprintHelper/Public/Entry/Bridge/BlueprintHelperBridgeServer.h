// BlueprintHelper Bridge Layer — TCP Bridge Server

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"

class FBlueprintHelperBridgeRouter;
class FSocket;

DECLARE_LOG_CATEGORY_EXTERN(LogBlueprintHelperBridge, Log, All);

/**
 * TCP 监听服务器，运行在独立 IO 线程。
 * 接收外部客户端的 JSON 请求，通过 GameThread 投递给 Router 执行，
 * 然后将响应写回客户端。
 *
 * 消息帧格式：[4 字节大端 uint32 body 长度][UTF-8 JSON body]
 */
class BLUEPRINTHELPER_API FBlueprintHelperBridgeServer : public FRunnable
{
public:
	FBlueprintHelperBridgeServer(FBlueprintHelperBridgeRouter& InRouter, int32 InPort = 54321);
	virtual ~FBlueprintHelperBridgeServer() override;

	/** 启动监听线程。成功返回 true。 */
	bool Start();

	/** 停止监听并等待线程结束。 */
	void Shutdown();

	/** 获取实际绑定端口。 */
	int32 GetPort() const { return Port; }

	// FRunnable
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	/** 处理单个客户端连接（循环读请求 → 处理 → 写响应）。 */
	void HandleClient(FSocket* ClientSocket);

	/** 读取一条完整消息（4 字节长度头 + body）。 */
	bool ReadMessage(FSocket* Socket, FString& OutJson) const;

	/** 写入一条完整消息（4 字节长度头 + body）。 */
	bool WriteMessage(FSocket* Socket, const FString& Json) const;

	FBlueprintHelperBridgeRouter& Router;
	int32 Port;
	FSocket* ListenerSocket = nullptr;
	TUniquePtr<FRunnableThread> Thread;
	TAtomic<bool> bStopping{false};
};
