#pragma once

#include "types.h"
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>

namespace puppet {

// ============================================================
// IPC 服务器 — Unix Domain Socket (SOCK_STREAM)
// ============================================================
class IpcServer {
public:
    using FrameCallback = std::function<void(const IpcFrame&)>;

    IpcServer();
    ~IpcServer();

    IpcServer(const IpcServer&) = delete;
    IpcServer& operator=(const IpcServer&) = delete;

    bool start(const std::string& socketPath, FrameCallback callback);
    bool send(int clientFd, const IpcFrame& frame);
    void broadcast(const IpcFrame& frame);
    void stop();
    bool isRunning() const;

private:
    void acceptLoop();
    void handleClient(int clientFd);
    bool readFrame(int fd, IpcFrame& frame);
    bool writeFrame(int fd, const IpcFrame& frame);

    std::string socketPath_;
    int serverFd_ = -1;
    std::atomic<bool> running_{false};
    std::thread acceptThread_;
    std::mutex clientsMutex_;
    std::vector<int> clientFds_;
    FrameCallback callback_;
};

// ============================================================
// IPC 客户端
// ============================================================
class IpcClient {
public:
    IpcClient();
    ~IpcClient();

    IpcClient(const IpcClient&) = delete;
    IpcClient& operator=(const IpcClient&) = delete;

    bool connect(const std::string& socketPath);
    bool send(const IpcFrame& frame);
    bool recv(IpcFrame& frame);
    void disconnect();
    bool isConnected() const;
    int fd() const;

private:
    int fd_ = -1;
    std::string socketPath_;
};

} // namespace puppet
