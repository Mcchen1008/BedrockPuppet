# BedrockPuppet

<p align="center">
  <strong>基岩傀儡 —— 你的基岩版服务器，提线木偶般随心操控</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg" alt="C++17">
  <img src="https://img.shields.io/badge/License-Apache%202.0-green.svg" alt="License">
  <img src="https://img.shields.io/badge/Platform-Linux-lightgrey.svg" alt="Platform">
  <img src="https://img.shields.io/badge/BDS-1.20+-orange.svg" alt="BDS">
</p>

---

## 📖 简介

**BedrockPuppet** 是一个专为 **Minecraft 基岩版官方服务器（BDS）** 设计的轻量级桥接管理器。

它以守护进程模式运行，通过 WebSocket 与 BDS 建立常驻连接，将事件流和命令通道封装为简洁的 **HTTP API**，让外部服务可以轻松操控你的服务器。

> 简单来说：**让 BDS 变成一个可编程的"木偶"，而你手中的 API 就是提线。**

---

## ✨ 特性

| 特性 | 说明 |
|------|------|
| 🔧 单二进制部署 | Linux 原生程序，运行时仅依赖 OpenSSL（libcrypto） |
| 📝 按需记录 | 支持 8 类事件日志，YAML 配置灵活开关 |
| ⚡ 轻量高效 | C++17 实现，epoll + 非阻塞 I/O，性能优异 |
| 👤 无需 root | 默认使用高位端口（8000/8001），普通用户即可运行 |
| 🔄 进程守护 | BDS 子进程崩溃后自动重启（5 秒间隔），稳定可靠 |
| 🔐 API 认证 | 支持 API Key 保护接口安全（X-API-Key / Bearer / ApiKey） |
| 📡 事件订阅 | 支持 Webhook 回调，事件驱动外部服务 |

---

## 📜 开源协议

本项目采用 **Apache 2.0** 协议，详见 [LICENSE](LICENSE) 文件。