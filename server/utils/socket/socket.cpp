//
// Created by Kirill Zhukov on 09.01.2024.
//
#include "socket.h"

int unit::server::net::create_socket(int port, const std::string&ip_addr, int backlog, IPV ipv) {
    int soc_fd = socket((ipv == IPV::IPV4) ? AF_INET : AF_INET6, SOCK_STREAM, 0);
    if (soc_fd < 0) {
        error_log("Socket creation failed");
        exit(9);
    }

    int reuse = 1;
    if (setsockopt(soc_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        error_log("Setting SOL_SOCKET, SO_REUSEADDR for accept socket failed");
        close(soc_fd);
        exit(9);
    }

    if (setsockopt(soc_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        error_log("Setting SOL_SOCKET, SO_REUSEPORT for accept socket failed");
        close(soc_fd);
        exit(9);
    }

    sockaddr_in addr = {};
    addr.sin_family = (ipv == IPV::IPV4) ? AF_INET : AF_INET6;
    addr.sin_addr.s_addr = inet_addr(ip_addr.c_str());
    addr.sin_port = htons(port);

    if (bind(soc_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        error_log("Socket bind failed, port: %d", port);
        close(soc_fd);
        exit(9);
    }

    if (listen(soc_fd, backlog) < 0) {
        error_log("Listen for connections on a socket failed");
        exit(9);
    }

    if (int flags; (flags = fcntl(soc_fd, F_GETFL, 0)) < 0 || fcntl(soc_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        error_log("Error making socket non-blocking");
        exit(9);
    }
    return soc_fd;
}
