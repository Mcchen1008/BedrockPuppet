#include "websocket.h"
#include "logger.h"
#include <cstring>
#include <sstream>
#include <algorithm>
#include <unistd.h>
#include <openssl/sha.h>

namespace puppet {

// ============================================================
// 魔数
// ============================================================
static const std::string WS_MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// ============================================================
// Base64 编码（RFC 4648）
// ============================================================
static const char BASE64_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string WebSocketServer::base64Encode(const std::string& input) {
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i < input.size()) {
        uint32_t triple = 0;
        int pad = 0;

        triple |= (static_cast<uint8_t>(input[i]) << 16);
        if (i + 1 < input.size()) {
            triple |= (static_cast<uint8_t>(input[i + 1]) << 8);
        } else {
            pad++;
        }
        if (i + 2 < input.size()) {
            triple |= static_cast<uint8_t>(input[i + 2]);
        } else {
            pad++;
        }

        output += BASE64_TABLE[(triple >> 18) & 0x3F];
        output += BASE64_TABLE[(triple >> 12) & 0x3F];
        output += (pad >= 2) ? '=' : BASE64_TABLE[(triple >> 6) & 0x3F];
        output += (pad >= 1) ? '=' : BASE64_TABLE[triple & 0x3F];

        i += 3;
    }
    return output;
}

// ============================================================
// SHA-1
// ============================================================
std::string WebSocketServer::sha1(const std::string& input) {
    unsigned char hash[SHA_DIGEST_LENGTH]; // 20 bytes
    SHA1(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
    return std::string(reinterpret_cast<char*>(hash), SHA_DIGEST_LENGTH);
}

// ============================================================
// Accept Key 计算
// ============================================================
std::string WebSocketServer::computeAcceptKey(const std::string& clientKey) {
    std::string combined = clientKey + WS_MAGIC;
    std::string hash = sha1(combined);
    return base64Encode(hash);
}

// ============================================================
// HTTP 工具
// ============================================================
bool WebSocketServer::isUpgradeRequest(const std::string& data) {
    return data.find("Upgrade: websocket") != std::string::npos ||
           data.find("upgrade: websocket") != std::string::npos ||
           data.find("Upgrade: WebSocket") != std::string::npos;
}

std::string WebSocketServer::extractPath(const std::string& request) {
    // GET /path HTTP/1.1
    size_t start = request.find("GET ");
    if (start == std::string::npos) return "";
    start += 4;
    size_t end = request.find(" HTTP", start);
    if (end == std::string::npos) return "";
    return request.substr(start, end - start);
}

std::string WebSocketServer::extractKey(const std::string& request) {
    const std::string header = "Sec-WebSocket-Key:";
    size_t pos = request.find(header);
    if (pos == std::string::npos) {
        // 尝试小写
        pos = request.find("Sec-Websocket-Key:");
        if (pos == std::string::npos) {
            pos = request.find("sec-websocket-key:");
            if (pos == std::string::npos) return "";
        }
    }

    pos += header.size();
    // 跳过空格
    while (pos < request.size() && (request[pos] == ' ' || request[pos] == '\t')) pos++;

    size_t end = request.find('\r', pos);
    if (end == std::string::npos) end = request.find('\n', pos);
    if (end == std::string::npos) return "";

    std::string key = request.substr(pos, end - pos);
    // 去除尾部 \r 和空格
    while (!key.empty() && (key.back() == '\r' || key.back() == ' ')) {
        key.pop_back();
    }
    return key;
}

// ============================================================
// WebSocket 握手
// ============================================================
bool WebSocketServer::handshake(int fd, const std::string& httpRequest) {
    std::string clientKey = extractKey(httpRequest);
    if (clientKey.empty()) {
        LOG_WARN("WebSocket 握手: 缺少 Sec-WebSocket-Key");
        return false;
    }

    std::string acceptKey = computeAcceptKey(clientKey);

    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << acceptKey << "\r\n"
             << "\r\n";

    std::string respStr = response.str();
    ssize_t n = ::write(fd, respStr.data(), respStr.size());
    if (n != static_cast<ssize_t>(respStr.size())) {
        LOG_ERROR("WebSocket 握手: 发送响应失败");
        return false;
    }

    LOG_INFO("WebSocket 握手成功, fd=" + std::to_string(fd));
    return true;
}

// ============================================================
// 帧解析
// ============================================================
bool WebSocketServer::parseFrame(int fd, const std::string& rawData,
                                  std::string& outMessage, bool& outCloseFrame) {
    if (rawData.size() < 2) {
        return false; // 不够最小帧大小
    }

    const uint8_t* data = reinterpret_cast<const uint8_t*>(rawData.data());
    size_t pos = 0;
    size_t totalLen = rawData.size();

    // Byte 0: FIN(1) + RSV(3) + Opcode(4)
    uint8_t firstByte = data[pos++];
    bool fin = (firstByte & 0x80) != 0;
    WsOpcode opcode = static_cast<WsOpcode>(firstByte & 0x0F);

    // Byte 1: MASK(1) + Payload length(7)
    if (pos >= totalLen) return false;
    uint8_t secondByte = data[pos++];
    bool masked = (secondByte & 0x80) != 0;
    uint64_t payloadLen = secondByte & 0x7F;

    // 扩展长度
    if (payloadLen == 126) {
        if (pos + 2 > totalLen) return false;
        payloadLen = (static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1];
        pos += 2;
    } else if (payloadLen == 127) {
        if (pos + 8 > totalLen) return false;
        payloadLen = 0;
        for (int i = 0; i < 8; i++) {
            payloadLen = (payloadLen << 8) | data[pos + i];
        }
        pos += 8;
    }

    // Mask key
    uint8_t maskKey[4] = {0};
    if (masked) {
        if (pos + 4 > totalLen) return false;
        memcpy(maskKey, data + pos, 4);
        pos += 4;
    }

    // Payload
    if (pos + payloadLen > totalLen) return false;
    std::string payload(reinterpret_cast<const char*>(data + pos), payloadLen);

    // 解掩码
    if (masked) {
        for (size_t i = 0; i < payload.size(); i++) {
            payload[i] ^= maskKey[i % 4];
        }
    }

    // 处理操作码
    switch (opcode) {
        case WsOpcode::Text:
        case WsOpcode::Binary:
            if (fin) {
                outMessage = payload;
                outCloseFrame = false;
                return true;
            }
            // 暂不支持分片，返回 false 等待更多数据
            LOG_WARN("WebSocket 分片消息暂未支持, fd=" + std::to_string(fd));
            return false;

        case WsOpcode::Close:
            outCloseFrame = true;
            // 发送回应关闭帧
            {
                std::string closeResp = buildCloseFrame(1000);
                ::write(fd, closeResp.data(), closeResp.size());
            }
            LOG_INFO("WebSocket 关闭帧收到, fd=" + std::to_string(fd));
            return false;

        case WsOpcode::Ping:
            {
                std::string pong = buildPongFrame(payload);
                ::write(fd, pong.data(), pong.size());
            }
            LOG_DEBUG("WebSocket Ping/Pong, fd=" + std::to_string(fd));
            outCloseFrame = false;
            return false;

        case WsOpcode::Pong:
            LOG_DEBUG("WebSocket Pong, fd=" + std::to_string(fd));
            outCloseFrame = false;
            return false;

        default:
            LOG_WARN("WebSocket 未知操作码: " + std::to_string(static_cast<int>(opcode)));
            outCloseFrame = false;
            return false;
    }
}

// ============================================================
// 帧构建
// ============================================================
std::string WebSocketServer::buildFrame(const std::string& message, WsOpcode opcode) {
    std::string frame;
    size_t msgLen = message.size();

    // Byte 0: FIN + Opcode
    frame.push_back(static_cast<char>(0x80 | static_cast<uint8_t>(opcode)));

    // Byte 1+: MASK(0, server not masked) + Payload length
    if (msgLen <= 125) {
        frame.push_back(static_cast<char>(msgLen));
    } else if (msgLen <= 65535) {
        frame.push_back(static_cast<char>(126));
        frame.push_back(static_cast<char>((msgLen >> 8) & 0xFF));
        frame.push_back(static_cast<char>(msgLen & 0xFF));
    } else {
        frame.push_back(static_cast<char>(127));
        for (int i = 7; i >= 0; i--) {
            frame.push_back(static_cast<char>((msgLen >> (i * 8)) & 0xFF));
        }
    }

    // Payload
    frame.append(message);

    return frame;
}

std::string WebSocketServer::buildCloseFrame(uint16_t code, const std::string& reason) {
    std::string payload;
    payload.push_back(static_cast<char>((code >> 8) & 0xFF));
    payload.push_back(static_cast<char>(code & 0xFF));
    payload.append(reason);

    // 直接构建（不使用 buildFrame 避免递归）
    std::string frame;
    frame.push_back(static_cast<char>(0x80 | static_cast<uint8_t>(WsOpcode::Close)));
    size_t len = payload.size();
    if (len <= 125) {
        frame.push_back(static_cast<char>(len));
    } else {
        frame.push_back(static_cast<char>(126));
        frame.push_back(static_cast<char>((len >> 8) & 0xFF));
        frame.push_back(static_cast<char>(len & 0xFF));
    }
    frame.append(payload);
    return frame;
}

std::string WebSocketServer::buildPongFrame(const std::string& pingPayload) {
    return buildFrame(pingPayload, WsOpcode::Pong);
}

} // namespace puppet
