#include "ipc.h"
#include "logger.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <arpa/inet.h>

namespace puppet {

// ============================================================
// 辅助函数
// ============================================================
namespace {

bool setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

void closeSafely(int& fd) {
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

} // anonymous namespace

// ============================================================
// IpcServer 实现
// ============================================================
IpcServer::IpcServer() = default;

IpcServer::~IpcServer() {
    stop();
}

bool IpcServer::start(const std::string& socketPath, FrameCallback callback) {
    socketPath_ = socketPath;
    callback_ = std::move(callback);

    // 删除已存在的 socket 文件
    unlink(socketPath_.c_str());

    // 创建 Unix socket
    serverFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverFd_ < 0) {
        LOG_ERROR("IPC 创建 socket 失败: " + std::string(strerror(errno)));
        return false;
    }

    // 设置非阻塞
    if (!setNonBlocking(serverFd_)) {
        LOG_WARN("IPC 设置非阻塞失败");
    }

    // 绑定
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(serverFd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("IPC 绑定失败 (" + socketPath_ + "): " + std::string(strerror(errno)));
        closeSafely(serverFd_);
        return false;
    }

    // 监听
    if (listen(serverFd_, constants::IPC_BACKLOG) < 0) {
        LOG_ERROR("IPC 监听失败: " + std::string(strerror(errno)));
        closeSafely(serverFd_);
        return false;
    }

    running_ = true;
    acceptThread_ = std::thread(&IpcServer::acceptLoop, this);

    LOG_INFO("IPC 服务器启动成功: " + socketPath_);
    return true;
}

void IpcServer::acceptLoop() {
    // 使用 epoll 监听新连接和客户端数据
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        LOG_ERROR("IPC epoll 创建失败");
        running_ = false;
        return;
    }

    struct epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = serverFd_;
    epoll_ctl(epfd, EPOLL_CTL_ADD, serverFd_, &ev);

    struct epoll_event events[constants::EPOLL_MAX_EVENTS];

    while (running_) {
        int nfds = epoll_wait(epfd, events, constants::EPOLL_MAX_EVENTS, 1000); // 1s 超时
        if (nfds < 0) {
            if (errno == EINTR) continue;
            LOG_ERROR("IPC epoll_wait 错误: " + std::string(strerror(errno)));
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == serverFd_) {
                // 新连接
                int clientFd = accept(serverFd_, nullptr, nullptr);
                if (clientFd < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        LOG_WARN("IPC accept 失败: " + std::string(strerror(errno)));
                    }
                    continue;
                }

                setNonBlocking(clientFd);

                {
                    std::lock_guard<std::mutex> lock(clientsMutex_);
                    clientFds_.push_back(clientFd);
                }

                struct epoll_event cev{};
                cev.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP;
                cev.data.fd = clientFd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clientFd, &cev);

                LOG_DEBUG("IPC 新客户端连接: fd=" + std::to_string(clientFd));
            } else {
                // 客户端数据
                if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                    // 客户端断开
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                    closeSafely(fd);
                    {
                        std::lock_guard<std::mutex> lock(clientsMutex_);
                        auto it = std::find(clientFds_.begin(), clientFds_.end(), fd);
                        if (it != clientFds_.end()) {
                            clientFds_.erase(it);
                        }
                    }
                    LOG_DEBUG("IPC 客户端断开: fd=" + std::to_string(fd));
                } else if (events[i].events & EPOLLIN) {
                    IpcFrame frame;
                    if (readFrame(fd, frame)) {
                        if (callback_) {
                            callback_(frame);
                        }
                    } else {
                        // 读取失败，关闭
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                        closeSafely(fd);
                        {
                            std::lock_guard<std::mutex> lock(clientsMutex_);
                            auto it = std::find(clientFds_.begin(), clientFds_.end(), fd);
                            if (it != clientFds_.end()) {
                                clientFds_.erase(it);
                            }
                        }
                    }
                }
            }
        }
    }

    close(epfd);
}

bool IpcServer::send(int clientFd, const IpcFrame& frame) {
    return writeFrame(clientFd, frame);
}

void IpcServer::broadcast(const IpcFrame& frame) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (int fd : clientFds_) {
        writeFrame(fd, frame);
    }
}

void IpcServer::stop() {
    if (!running_) return;
    running_ = false;

    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }

    // 关闭所有客户端连接
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (int fd : clientFds_) {
            closeSafely(fd);
        }
        clientFds_.clear();
    }

    closeSafely(serverFd_);
    unlink(socketPath_.c_str());
    LOG_INFO("IPC 服务器已停止: " + socketPath_);
}

bool IpcServer::isRunning() const {
    return running_;
}

bool IpcServer::readFrame(int fd, IpcFrame& frame) {
    // 读取 4 字节长度
    uint32_t length = 0;
    ssize_t n = ::read(fd, &length, 4);
    if (n <= 0) return false;
    length = ntohl(length);

    if (length == 0 || length > 1024 * 1024) { // 最大 1MB
        LOG_WARN("IPC 帧长度异常: " + std::to_string(length));
        return false;
    }

    // 读取 1 字节类型
    uint8_t typeByte = 0;
    n = ::read(fd, &typeByte, 1);
    if (n <= 0) return false;
    frame.type = static_cast<IpcMessageType>(typeByte);

    // 读取 payload
    if (length > 0) {
        frame.payload.resize(length);
        size_t totalRead = 0;
        while (totalRead < length) {
            n = ::read(fd, &frame.payload[totalRead], length - totalRead);
            if (n <= 0) return false;
            totalRead += n;
        }
    } else {
        frame.payload.clear();
    }

    return true;
}

bool IpcServer::writeFrame(int fd, const IpcFrame& frame) {
    uint32_t length = htonl(static_cast<uint32_t>(frame.payload.size()));
    uint8_t type = static_cast<uint8_t>(frame.type);

    // 写入头部
    ssize_t n = ::write(fd, &length, 4);
    if (n != 4) return false;

    n = ::write(fd, &type, 1);
    if (n != 1) return false;

    // 写入 payload
    if (!frame.payload.empty()) {
        n = ::write(fd, frame.payload.data(), frame.payload.size());
        if (n != static_cast<ssize_t>(frame.payload.size())) return false;
    }

    return true;
}

// ============================================================
// IpcClient 实现
// ============================================================
IpcClient::IpcClient() = default;

IpcClient::~IpcClient() {
    disconnect();
}

bool IpcClient::connect(const std::string& socketPath) {
    socketPath_ = socketPath;

    fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd_ < 0) {
        LOG_ERROR("IPC 客户端创建 socket 失败: " + std::string(strerror(errno)));
        return false;
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("IPC 客户端连接失败 (" + socketPath_ + "): " + std::string(strerror(errno)));
        closeSafely(fd_);
        return false;
    }

    LOG_INFO("IPC 客户端已连接到: " + socketPath_);
    return true;
}

bool IpcClient::send(const IpcFrame& frame) {
    if (fd_ < 0) return false;

    uint32_t length = htonl(static_cast<uint32_t>(frame.payload.size()));
    uint8_t type = static_cast<uint8_t>(frame.type);

    if (::write(fd_, &length, 4) != 4) return false;
    if (::write(fd_, &type, 1) != 1) return false;
    if (!frame.payload.empty()) {
        if (::write(fd_, frame.payload.data(), frame.payload.size()) !=
            static_cast<ssize_t>(frame.payload.size())) return false;
    }

    return true;
}

bool IpcClient::recv(IpcFrame& frame) {
    if (fd_ < 0) return false;

    // 读取 4 字节长度
    uint32_t length = 0;
    ssize_t n = ::read(fd_, &length, 4);
    if (n != 4) return false;
    length = ntohl(length);

    if (length > 1024 * 1024) return false;

    // 读取 1 字节类型
    uint8_t typeByte = 0;
    n = ::read(fd_, &typeByte, 1);
    if (n != 1) return false;
    frame.type = static_cast<IpcMessageType>(typeByte);

    // 读取 payload
    if (length > 0) {
        frame.payload.resize(length);
        size_t totalRead = 0;
        while (totalRead < length) {
            n = ::read(fd_, &frame.payload[totalRead], length - totalRead);
            if (n <= 0) return false;
            totalRead += n;
        }
    } else {
        frame.payload.clear();
    }

    return true;
}

void IpcClient::disconnect() {
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
        LOG_INFO("IPC 客户端已断开: " + socketPath_);
    }
}

bool IpcClient::isConnected() const {
    return fd_ >= 0;
}

int IpcClient::fd() const {
    return fd_;
}

} // namespace puppet
