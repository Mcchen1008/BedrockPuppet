#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <ctime>
#include <cstdint>
#include <functional>
#include <memory>

namespace puppet {

// ============================================================
// 事件类型枚举
// ============================================================
enum class EventType {
    Chat,
    PlayerJoin,
    PlayerLeave,
    PlayerDeath,
    Kill,
    Achievement,
    Item,
    Command,
    Block,
    Unknown
};

inline std::string eventTypeToString(EventType t) {
    switch (t) {
        case EventType::Chat:        return "ChatEvent";
        case EventType::PlayerJoin:  return "PlayerJoin";
        case EventType::PlayerLeave: return "PlayerLeave";
        case EventType::PlayerDeath: return "PlayerDeath";
        case EventType::Kill:        return "KillEvent";
        case EventType::Achievement: return "AchievementEvent";
        case EventType::Item:        return "ItemEvent";
        case EventType::Command:     return "CommandEvent";
        case EventType::Block:       return "BlockEvent";
        default:                     return "Unknown";
    }
}

inline EventType stringToEventType(const std::string& s) {
    if (s == "ChatEvent" || s == "Chat")        return EventType::Chat;
    if (s == "PlayerJoin" || s == "Join")       return EventType::PlayerJoin;
    if (s == "PlayerLeave" || s == "Leave")     return EventType::PlayerLeave;
    if (s == "PlayerDeath" || s == "Death")     return EventType::PlayerDeath;
    if (s == "KillEvent" || s == "Kill")        return EventType::Kill;
    if (s == "AchievementEvent" || s == "Achievement") return EventType::Achievement;
    if (s == "ItemEvent" || s == "Item")        return EventType::Item;
    if (s == "CommandEvent" || s == "Command")  return EventType::Command;
    if (s == "BlockEvent" || s == "Block")      return EventType::Block;
    if (s == "All")                             return EventType::Unknown; // Unknown = wildcard
    return EventType::Unknown;
}

// ============================================================
// 数据记录条目
// ============================================================
struct ChatRecord {
    std::string time;
    std::string player;
    std::string message;
};

struct JoinRecord {
    std::string time;
    std::string player;
    std::string type; // "join" or "leave"
};

struct DeathRecord {
    std::string time;
    std::string player;
    std::string cause;
};

struct KillRecord {
    std::string time;
    std::string killer;
    std::string victim;
    std::string type; // "player" or "mob"
};

struct AchievementRecord {
    std::string time;
    std::string player;
    std::string achievement;
};

struct ItemRecord {
    std::string time;
    std::string player;
    std::string action; // acquire/craft/destroy/drop/enchant/smelt/use
    std::string item;
    int count;
};

struct CommandRecord {
    std::string time;
    std::string player;
    std::string command;
};

struct BlockRecord {
    std::string time;
    std::string player;
    std::string action; // break/place
    std::string block;
    std::string position;
};

// ============================================================
// 通用事件消息（模块间传递）
// ============================================================
struct EventMessage {
    EventType type;
    std::string raw;        // 原始消息文本
    std::string player;
    std::string content;    // 消息内容 / 原因 / 命令等
    std::string extra;      // 额外字段
    std::string time;       // 时间戳
};

// ============================================================
// Webhook 订阅
// ============================================================
struct WebhookSubscription {
    EventType event;
    std::string url;
};

// ============================================================
// 配置结构体
// ============================================================
struct Config {
    // 端口
    int wsPort = 8000;
    int httpPort = 8001;

    // API
    std::string apiListen = "0.0.0.0:8001";
    std::string apiKey = "";

    // 数据记录开关
    struct {
        bool chat = true;
        bool join = true;
        bool death = true;
        bool kill = true;
        bool achievement = true;
        bool item = true;
        bool command = true;
        bool block = true;
    } logging;

    // 日志配置
    std::string logLevel = "info";
    std::string logFile = "logs/puppet.log";

    // BDS
    std::string bdsPath = "server/bedrock_server";
    std::string bdsArgs = "";

    // Webhook
    int webhookTimeout = 5;
    int webhookRetry = 3;
};

// ============================================================
// 服务器状态
// ============================================================
struct ServerStatus {
    std::string status = "unknown";
    int players = 0;
    int maxPlayers = 0;
    std::string serverName;
    std::string worldName;
    int64_t uptime = 0;
};

// ============================================================
// 玩家信息
// ============================================================
struct PlayerInfo {
    std::string name;
    std::string firstJoin;
    std::string lastSeen;
    int64_t playtime = 0;
    int kills = 0;
    int deaths = 0;
    int achievements = 0;
};

// ============================================================
// 日志级别
// ============================================================
enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};

// ============================================================
// IPC 消息类型（Unix socket 通信）
// ============================================================
enum class IpcMessageType : uint8_t {
    Event       = 0x01,  // 事件消息 (ws → logic)
    Command     = 0x02,  // 命令请求 (http_api → ws)
    CommandResp = 0x03,  // 命令响应 (ws → http_api)
    Query       = 0x04,  // 查询请求 (http_api → logic)
    QueryResp   = 0x05,  // 查询响应 (logic → http_api)
    StatusReq   = 0x06,  // 状态请求
    StatusResp  = 0x07,  // 状态响应
};

// ============================================================
// IPC 二进制帧格式
//   [4 bytes: payload length][1 byte: type][payload]
// ============================================================
struct IpcFrame {
    IpcMessageType type;
    std::string payload;
};

// ============================================================
// 常量
// ============================================================
namespace constants {
    constexpr int WS_DEFAULT_PORT    = 8000;
    constexpr int HTTP_DEFAULT_PORT  = 8001;
    constexpr int IPC_BACKLOG        = 128;
    constexpr int EPOLL_MAX_EVENTS   = 64;
    constexpr int MAX_WS_FRAME_SIZE  = 65536;
    constexpr int RESTART_DELAY_SEC  = 5;
    constexpr const char* IPC_SOCKET_PATH = "/tmp/bedrock_puppet_logic.sock";
    constexpr const char* IPC_WS_SOCKET_PATH = "/tmp/bedrock_puppet_ws.sock";
}

} // namespace puppet
