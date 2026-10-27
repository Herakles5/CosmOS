#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <functional>
#include <thread>
#include <atomic>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    typedef int socket_t;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define CLOSE_SOCKET close
#endif

#include "json.hpp"

namespace Aion {

class AionIPCClient {
private:
    socket_t sock = INVALID_SOCKET;
    std::atomic<bool> connected{false};
    std::string rxBuffer;

public:
    AionIPCClient() {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }

    ~AionIPCClient() {
        disconnect();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    // Verbindet wahlweise per Unix Domain Socket (Linux) oder TCP (Cross-Platform)
    bool connectUnix(const std::string& socketPath = "/tmp/aion_nexus.sock") {
#ifndef _WIN32
        disconnect();
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) return false;

        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

        if (::connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            CLOSE_SOCKET(sock);
            sock = INVALID_SOCKET;
            return false;
        }

        // Set non-blocking
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        connected = true;
        std::cout << "[AION IPC] Erfolgreich via Unix Domain Socket verbunden: " << socketPath << std::endl;
        return true;
#else
        return connectTCP("127.0.0.1", 9099);
#endif
    }

    bool connectTCP(const std::string& host = "127.0.0.1", int port = 9099) {
        disconnect();
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) return false;

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

        if (::connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            CLOSE_SOCKET(sock);
            sock = INVALID_SOCKET;
            return false;
        }

#ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(sock, FIONBIO, &mode);
#else
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

        connected = true;
        std::cout << "[AION IPC] Erfolgreich via TCP verbunden: " << host << ":" << port << std::endl;
        return true;
    }

    void disconnect() {
        if (sock != INVALID_SOCKET) {
            CLOSE_SOCKET(sock);
            sock = INVALID_SOCKET;
        }
        connected = false;
    }

    bool isConnected() const {
        return connected;
    }

    // Sendet einen JSON-Befehl an die Python Evolution Engine
    bool sendCommand(const nlohmann::json& cmd) {
        if (!connected || sock == INVALID_SOCKET) return false;
        std::string payload = cmd.dump() + "\n";
        int sent = ::send(sock, payload.c_str(), (int)payload.size(), 0);
        return sent != SOCKET_ERROR;
    }

    // Fragt Events ab (Non-blocking für 60 FPS Game Loop)
    void pollEvents(std::function<void(const std::string& eventType, const nlohmann::json& data)> onEvent) {
        if (!connected || sock == INVALID_SOCKET) return;

        char buffer[1024];
        while (true) {
            int bytesRead = ::recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                rxBuffer += buffer;

                size_t pos;
                while ((pos = rxBuffer.find('\n')) != std::string::npos) {
                    std::string line = rxBuffer.substr(0, pos);
                    rxBuffer.erase(0, pos + 1);

                    if (line.empty()) continue;
                    try {
                        auto j = nlohmann::json::parse(line);
                        std::string eventType = j.value("event", "UNKNOWN");
                        if (onEvent) {
                            onEvent(eventType, j);
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "[AION IPC PARSE ERROR] " << e.what() << std::endl;
                    }
                }
            } else {
                break;
            }
        }
    }
};

} // namespace Aion
