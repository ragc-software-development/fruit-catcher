#ifdef __linux__

#include "server/network_acceptor.hpp"
#include "server/match_context.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <system_error>

namespace ragc::Server {

NetworkAcceptor::NetworkAcceptor() noexcept : listen_fd_(-1), poll_fd_(-1) {}

NetworkAcceptor::~NetworkAcceptor() noexcept {
    if (listen_fd_ != -1) {
        close(listen_fd_);
    }
    if (poll_fd_ != -1) {
        close(poll_fd_);
    }
}

NetworkAcceptor::NetworkAcceptor(NetworkAcceptor&& other) noexcept
    : listen_fd_(other.listen_fd_), poll_fd_(other.poll_fd_) {
    other.listen_fd_ = -1;
    other.poll_fd_ = -1;
}

NetworkAcceptor& NetworkAcceptor::operator=(NetworkAcceptor&& other) noexcept {
    if (this != &other) {
        if (listen_fd_ != -1) close(listen_fd_);
        if (poll_fd_ != -1) close(poll_fd_);
        listen_fd_ = other.listen_fd_;
        poll_fd_ = other.poll_fd_;
        other.listen_fd_ = -1;
        other.poll_fd_ = -1;
    }
    return *this;
}

auto NetworkAcceptor::listen_on(uint16_t port) noexcept -> expected<void, std::error_code> {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ == -1) [[unlikely]] {
        return unexpected(std::error_code(errno, std::generic_category()));
    }

    int flags = fcntl(listen_fd_, F_GETFL, 0);
    if (flags == -1 || fcntl(listen_fd_, F_SETFL, flags | O_NONBLOCK) == -1) [[unlikely]] {
        return unexpected(std::error_code(errno, std::generic_category()));
    }

    int reuse = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) [[unlikely]] {
        return unexpected(std::error_code(errno, std::generic_category()));
    }

    int nodelay = 1;
    if (setsockopt(listen_fd_, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) == -1) [[unlikely]] {
        return unexpected(std::error_code(errno, std::generic_category()));
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) [[unlikely]] {
        return unexpected(std::error_code(errno, std::generic_category()));
    }

    if (listen(listen_fd_, SOMAXCONN) == -1) [[unlikely]] {
        return unexpected(std::error_code(errno, std::generic_category()));
    }

    poll_fd_ = epoll_create1(0);
    if (poll_fd_ == -1) [[unlikely]] {
        return unexpected(std::error_code(errno, std::generic_category()));
    }

    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listen_fd_;
    if (epoll_ctl(poll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev) == -1) [[unlikely]] {
        return unexpected(std::error_code(errno, std::generic_category()));
    }

    return {};
}

template <GameMatchHandler Handler>
auto NetworkAcceptor::poll_events(Handler& match_context) noexcept -> void {
    if (poll_fd_ == -1 || listen_fd_ == -1) [[unlikely]] {
        return;
    }

    epoll_event events[16];
    int nevents = epoll_wait(poll_fd_, events, 16, 1);
    for (int i = 0; i < nevents; ++i) {
        int fd = events[i].data.fd;

        if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
            if (fd != listen_fd_) {
                match_context.remove_player(fd);
                epoll_ctl(poll_fd_, EPOLL_CTL_DEL, fd, nullptr);
                close(fd);
            }
        } else if (fd == listen_fd_) {
            while (true) {
                sockaddr_in client_addr{};
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
                if (client_fd == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;
                    }
                    break;
                }

                int flags = fcntl(client_fd, F_GETFL, 0);
                if (flags != -1) {
                    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
                }
                
                int nodelay = 1;
                setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

                if (match_context.add_player(client_fd)) {
                    auto cfg = match_context.get_match_config();
                    Common::Network::ServerJoinResponsePacket resp{};
                    resp.assigned_client_id = client_fd;
                    resp.max_players        = cfg.max_players;
                    resp.max_fruits         = cfg.max_fruits;
                    resp.map_bounds         = cfg.map_bounds;
                    send(client_fd, &resp, sizeof(resp), 0);

                    epoll_event ev_client{};
                    ev_client.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
                    ev_client.data.fd = client_fd;
                    epoll_ctl(poll_fd_, EPOLL_CTL_ADD, client_fd, &ev_client);
                } else {
                    close(client_fd);
                }
            }
        } else {
            while (true) {
                uint8_t buf[256];
                ssize_t bytes_read = recv(fd, buf, sizeof(buf), 0);
                if (bytes_read > 0) {
                    auto opcode = static_cast<Common::Network::OpCode>(buf[0]);
                    if (opcode == Common::Network::OpCode::CLIENT_INPUT
                        && bytes_read == static_cast<ssize_t>(sizeof(Common::Network::ClientInputPacket))) {
                        Common::Network::ClientInputPacket packet{};
                        __builtin_memcpy(&packet, buf, sizeof(packet));
                        match_context.process_player_input(fd, packet.direction, 0.016f);
                    }
                } else {
                    if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        break;
                    }
                    match_context.remove_player(fd);
                    epoll_ctl(poll_fd_, EPOLL_CTL_DEL, fd, nullptr);
                    close(fd);
                    break;
                }
            }
        }
    }
}

template auto NetworkAcceptor::poll_events<MatchContext<MatchConfig<2, 50>>>(
    MatchContext<MatchConfig<2, 50>>& match_context) noexcept -> void;

} // namespace ragc::Server

#endif // __linux__
