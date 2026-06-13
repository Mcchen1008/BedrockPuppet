#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace puppet {

// ============================================================
// WebSocket 操作码
// ============================================================
enum class WsOpcode : uint8_t {
    Continuation = 0x0,
    Text         = 0x1,
    Binary       = 0x2,
    Close        = 0x8,
    Ping         = 0x9,
    Pong         = 0xA
};

// ============================================================
// WebSocket 帧
// ============================================================
struct WsFrame {
    bool fin = true;
    WsOpcode opcode = WsOpcode::Text;
    bool masked = false;
    uint8_t maskKey[4] = {0};
    std::string payload;
};

// ============================================================
// WebSocket 服务器处理器
// ============================================================
class WebSocketServer {
public:
    using MessageCallback = std::function<void(int fd, const std::string& message)>;
    using CloseCallback = std::function<void(int fd)>;

    WebSocketServer() = default;
    ~WebSocketServer() = default;

    // 握手：处理 HTTP Upgrade 请求，返回 101 响应
    // 成功返回 true，失败返回 false
    bool handshake(int fd, const std::string& httpRequest);

    // 解析 WebSocket 帧
    // 返回 true 表示解析到一个完整消息
    bool parseFrame(int fd, const std::string& rawData,
                    std::string& outMessage, bool& outCloseFrame);

    // 构建 WebSocket 帧（用于发送）
    std::string buildFrame(const std::string& message, WsOpcode opcode = WsOpcode::Text);

    // 构建关闭帧
    std::string buildCloseFrame(uint16_t code = 1000, const std::string& reason = "");

    // 构建 Pong 帧
    std::string buildPongFrame(const std::string& pingPayload = "");

    // 检查是否是完整的 HTTP 升级请求
    static bool isUpgradeRequest(const std::string& data);

    // 从 HTTP 请求中提取路径
    static std::string extractPath(const std::string& request);

private:
    // SHA-1 计算（使用 OpenSSL）
    static std::string sha1(const std::string& input);

    // Base64 编码
    static std::string base64Encode(const std::string& input);

    // 生成 WebSocket Accept Key
    static std::string computeAcceptKey(const std::string& clientKey);

    // 从 HTTP 请求头中提取 Sec-WebSocket-Key
    static std::string extractKey(const std::string& request);
};

} // namespace puppet
