# BlueprintHelper Phase 2 — Bridge 层实施计划

> Phase 1（Service 层 headless 化）已完成并编译通过。本文件描述 Phase 2 的具体实施。

---

## 零、Phase 1 完成状态

| Service | 状态 | 文件 |
|---------|------|------|
| ServiceTypes (DTO) | ✅ | `Public/Services/BlueprintHelperServiceTypes.h` |
| GraphResolver | ✅ | `Public/Services/ + Private/Services/` |
| ValidationService | ✅ | 同上 |
| ExportService | ✅ | 同上 |
| ImportService | ✅ | 同上（含 FScopedTransaction） |
| CompileService | ✅ | 同上（含 FCompilerResultsLog） |
| Module 集成 | ✅ | 5 个 TUniquePtr + getter |
| Widget 改用 Service | ✅ | GraphResolver 替代直调 |

---

## 一、Phase 2 目标

让外部本地客户端（未来的 MCP Server 进程）能通过 TCP 连接驱动 UE 执行蓝图操作，
无需打开 Slate UI。

**MVP 命令集**：
1. `get_rule_markdown` — 获取 JSON 规则文档
2. `get_editor_context` — 获取当前编辑器上下文
3. `validate_json` — JSON 预校验
4. `export_to_json` — 导出蓝图/图表为 JSON
5. `import_json` — 导入 JSON 到蓝图
6. `compile_blueprint` — 编译蓝图

---

## 二、传输层选择

### TCP on localhost（MVP）

选择 TCP 而非 Named Pipe 的理由：
- UE `Sockets` 模块完善，跨平台
- 调试方便（telnet / curl / 任何 TCP 客户端可直接连接测试）
- 外部 MCP Server（Node.js / Python）连接简单
- 不影响后续切换 WebSocket

**协议**：
- 绑定 `127.0.0.1:54321`（端口可配置）
- 每个连接读完整请求 → 处理 → 返回响应 → 保持连接
- 消息格式：`4 字节大端长度头 + UTF-8 JSON body`
- 一个连接支持多次 request/response（持久连接）

**线程模型**：
- Listener 运行在独立 `FRunnable` 线程
- 收到请求后通过 `AsyncTask(ENamedThreads::GameThread, ...)` 投递到主线程
- 主线程执行完毕后通过 `TPromise/TFuture` 或回调返回结果到 IO 线程
- IO 线程将响应写回 socket

---

## 三、任务总表

| 序号 | 任务 | 新增/修改 | 依赖 |
|------|------|----------|------|
| T1 | Bridge 协议类型定义 | 新增 | 无 |
| T2 | ContextService（编辑器上下文查询） | 新增 | T1 |
| T3 | BridgeProtocol（序列化/反序列化） | 新增 | T1 |
| T4 | BridgeRouter（命令路由） | 新增 | T1, T3, Phase1 Services |
| T5 | BridgeServer（TCP 监听 + IO 线程） | 新增 | T4 |
| T6 | Module 集成 | 修改 | T2, T5 |
| T7 | 编译验证 | — | T6 |

---

## 四、T1：Bridge 协议类型

**新增文件**：`Public/Bridge/BlueprintHelperBridgeTypes.h`

```cpp
// 错误码
enum class EBlueprintHelperBridgeError : uint8
{
    None,
    InvalidRequest,
    UnknownCommand,
    EditorNotReady,
    AssetNotFound,
    GraphNotFound,
    JsonParseFailed,
    ExecutionFailed,
    InternalError
};

// Bridge 请求
struct FBlueprintHelperBridgeRequest
{
    FString RequestId;     // 幂等/追踪 ID
    FString Command;       // 命令名
    TSharedPtr<FJsonObject> Payload;  // 业务参数
};

// Bridge 响应
struct FBlueprintHelperBridgeResponse
{
    FString RequestId;
    bool bSuccess = false;
    EBlueprintHelperBridgeError ErrorCode = EBlueprintHelperBridgeError::None;
    FString Message;
    TSharedPtr<FJsonObject> Result;   // 业务结果
};
```

---

## 五、T2：ContextService

**新增文件**：
- `Public/Services/BlueprintHelperContextService.h`
- `Private/Services/BlueprintHelperContextService.cpp`

```cpp
struct FBlueprintHelperEditorContext
{
    FString ActiveBlueprintPath;
    FString ActiveGraphName;
    FString BlueprintDisplayName;
    int32 NodeCount = 0;
    bool bIsCompiled = false;
    int32 BlueprintStatus = 0;
};

class BLUEPRINTHELPER_API FBlueprintHelperContextService
{
public:
    explicit FBlueprintHelperContextService(const FBlueprintHelperGraphResolver& InResolver);
    FBlueprintHelperEditorContext GetContext() const;

private:
    const FBlueprintHelperGraphResolver& Resolver;
};
```

---

## 六、T3：BridgeProtocol（序列化）

**新增文件**：
- `Public/Bridge/BlueprintHelperBridgeProtocol.h`
- `Private/Bridge/BlueprintHelperBridgeProtocol.cpp`

提供：
- `ParseRequest(const FString& Json)` → `TOptional<FBlueprintHelperBridgeRequest>`
- `SerializeResponse(const FBlueprintHelperBridgeResponse&)` → `FString`
- Service DTO ↔ Bridge Payload 的辅助转换

---

## 七、T4：BridgeRouter（命令路由）

**新增文件**：
- `Public/Bridge/BlueprintHelperBridgeRouter.h`
- `Private/Bridge/BlueprintHelperBridgeRouter.cpp`

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperBridgeRouter
{
public:
    FBlueprintHelperBridgeRouter(
        const FBlueprintHelperImportService& Import,
        const FBlueprintHelperExportService& Export,
        const FBlueprintHelperCompileService& Compile,
        const FBlueprintHelperValidationService& Validation,
        const FBlueprintHelperContextService& Context);

    /** 路由并执行命令，返回响应。必须在 GameThread 调用。 */
    FBlueprintHelperBridgeResponse HandleRequest(
        const FBlueprintHelperBridgeRequest& Request) const;

private:
    FBlueprintHelperBridgeResponse HandleGetRuleMarkdown(const FBlueprintHelperBridgeRequest& Req) const;
    FBlueprintHelperBridgeResponse HandleGetEditorContext(const FBlueprintHelperBridgeRequest& Req) const;
    FBlueprintHelperBridgeResponse HandleValidateJson(const FBlueprintHelperBridgeRequest& Req) const;
    FBlueprintHelperBridgeResponse HandleExportToJson(const FBlueprintHelperBridgeRequest& Req) const;
    FBlueprintHelperBridgeResponse HandleImportJson(const FBlueprintHelperBridgeRequest& Req) const;
    FBlueprintHelperBridgeResponse HandleCompileBlueprint(const FBlueprintHelperBridgeRequest& Req) const;

    // Service 引用
    ...
};
```

**命令 → Service 映射**：

| 命令 | Service 方法 |
|------|-------------|
| `get_rule_markdown` | `FBlueprintHelperModule::GetJsonToBlueprintRuleMarkdown()` |
| `get_editor_context` | `ContextService.GetContext()` |
| `validate_json` | `ValidationService.Validate()` |
| `export_to_json` | `ExportService.Export()` |
| `import_json` | `ImportService.Import()` |
| `compile_blueprint` | `CompileService.Compile()` |

---

## 八、T5：BridgeServer（TCP 监听）

**新增文件**：
- `Public/Bridge/BlueprintHelperBridgeServer.h`
- `Private/Bridge/BlueprintHelperBridgeServer.cpp`

```cpp
class FBlueprintHelperBridgeServer : public FRunnable
{
public:
    FBlueprintHelperBridgeServer(FBlueprintHelperBridgeRouter& InRouter, int32 InPort = 54321);
    ~FBlueprintHelperBridgeServer();

    bool Start();
    void Stop();

    // FRunnable
    virtual uint32 Run() override;
    virtual void Stop() override;

private:
    void HandleClient(FSocket* ClientSocket);
    bool ReadMessage(FSocket* Socket, FString& OutJson);
    bool WriteMessage(FSocket* Socket, const FString& Json);

    FBlueprintHelperBridgeRouter& Router;
    int32 Port;
    FSocket* ListenerSocket = nullptr;
    TUniquePtr<FRunnableThread> Thread;
    TAtomic<bool> bStopping{false};
};
```

**消息帧格式**：
```
[4 bytes: big-endian uint32 body length][UTF-8 JSON body]
```

**GameThread 投递**：
```cpp
void HandleClient(FSocket* ClientSocket)
{
    FString RequestJson;
    if (!ReadMessage(ClientSocket, RequestJson)) return;

    // 投递到 GameThread 并等待结果
    TPromise<FBlueprintHelperBridgeResponse> Promise;
    TFuture<FBlueprintHelperBridgeResponse> Future = Promise.GetFuture();

    AsyncTask(ENamedThreads::GameThread, [&]()
    {
        auto Req = FBlueprintHelperBridgeProtocol::ParseRequest(RequestJson);
        auto Resp = Req ? Router.HandleRequest(*Req) : ErrorResponse(...);
        Promise.SetValue(MoveTemp(Resp));
    });

    auto Response = Future.Get(); // 阻塞等待 GameThread 完成
    WriteMessage(ClientSocket, FBlueprintHelperBridgeProtocol::SerializeResponse(Response));
}
```

---

## 九、T6：Module 集成

**修改**：`BlueprintHelper.h` + `BlueprintHelper.cpp`

新增成员：
```cpp
TUniquePtr<FBlueprintHelperContextService> ContextService;
TUniquePtr<FBlueprintHelperBridgeRouter> BridgeRouter;
TUniquePtr<FBlueprintHelperBridgeServer> BridgeServer;
```

`StartupModule()` 追加（在 Service 初始化之后）：
```cpp
ContextService = MakeUnique<FBlueprintHelperContextService>(*GraphResolver);
BridgeRouter = MakeUnique<FBlueprintHelperBridgeRouter>(
    *ImportService, *ExportService, *CompileService, *ValidationService, *ContextService);
BridgeServer = MakeUnique<FBlueprintHelperBridgeServer>(*BridgeRouter);
BridgeServer->Start();
```

`ShutdownModule()` 追加（在 Service 销毁之前）：
```cpp
if (BridgeServer) BridgeServer->Stop();
BridgeServer.Reset();
BridgeRouter.Reset();
ContextService.Reset();
```

**Build.cs 追加模块依赖**：
- `Sockets`
- `Networking`

---

## 十、新增文件清单

| 序号 | 路径 | 说明 |
|------|------|------|
| 1 | `Public/Bridge/BlueprintHelperBridgeTypes.h` | 协议 DTO + 错误码 |
| 2 | `Public/Services/BlueprintHelperContextService.h` | 编辑器上下文服务 |
| 3 | `Private/Services/BlueprintHelperContextService.cpp` | 实现 |
| 4 | `Public/Bridge/BlueprintHelperBridgeProtocol.h` | 序列化/反序列化 |
| 5 | `Private/Bridge/BlueprintHelperBridgeProtocol.cpp` | 实现 |
| 6 | `Public/Bridge/BlueprintHelperBridgeRouter.h` | 命令路由 |
| 7 | `Private/Bridge/BlueprintHelperBridgeRouter.cpp` | 实现 |
| 8 | `Public/Bridge/BlueprintHelperBridgeServer.h` | TCP 监听服务 |
| 9 | `Private/Bridge/BlueprintHelperBridgeServer.cpp` | 实现 |

## 修改文件清单

| 序号 | 路径 | 改动 |
|------|------|------|
| 1 | `Public/BlueprintHelper.h` | +3 TUniquePtr + forward declarations |
| 2 | `Private/BlueprintHelper.cpp` | StartupModule/ShutdownModule 追加 Bridge |
| 3 | `BlueprintHelper.Build.cs` | +Sockets, +Networking |

---

## 十一、验收标准

### 功能验收
- [ ] 启动编辑器后 `127.0.0.1:54321` 可连接
- [ ] 发送 `get_rule_markdown` 返回规则 Markdown
- [ ] 发送 `get_editor_context` 返回当前蓝图/图表信息
- [ ] 发送 `validate_json` 能检测 JSON 格式错误
- [ ] 发送 `export_to_json` 能导出当前图表
- [ ] 发送 `import_json` 能生成蓝图节点（GameThread 执行）
- [ ] 发送 `compile_blueprint` 能触发编译并返回结果
- [ ] 关闭编辑器时 Server 干净退出

### 安全验收
- [ ] 仅绑定 127.0.0.1，不接受外部连接
- [ ] 无效 JSON 不崩溃
- [ ] 未知命令返回明确错误

### Phase 2 → Phase 3 交接条件
- [ ] 外部进程（Python/Node）可完成完整闭环
- [ ] 所有响应 JSON 结构稳定，可作为 MCP tool result
