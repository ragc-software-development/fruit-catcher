#ifdef __APPLE__

#include "server/network_acceptor.hpp"
#include "server/match_context.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/event.h>
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

    poll_fd_ = kqueue();
    if (poll_fd_ == -1) [[unlikely]] {
        return unexpected(std::error_code(errno, std::generic_category()));
    }

    struct kevent change{};
    EV_SET(&change, listen_fd_, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    if (kevent(poll_fd_, &change, 1, nullptr, 0, nullptr) == -1) [[unlikely]] {
        return unexpected(std::error_code(errno, std::generic_category()));
    }

    return {};
}

template <GameMatchHandler Handler>
auto NetworkAcceptor::poll_events(Handler& match_context) noexcept -> void {
    if (poll_fd_ == -1 || listen_fd_ == -1) [[unlikely]] {
        return;
    }

    struct kevent events[16];
    struct timespec timeout{0, 1000000}; // 1ms

    int nevents = kevent(poll_fd_, nullptr, 0, events, 16, &timeout);
    for (int i = 0; i < nevents; ++i) {
        int fd = static_cast<int>(events[i].ident);

        if (events[i].flags & EV_EOF) {
            if (fd != listen_fd_) {
                match_context.remove_player(fd);
                struct kevent change{};
                EV_SET(&change, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
                kevent(poll_fd_, &change, 1, nullptr, 0, nullptr);
                close(fd);
            }
        } else if (fd == listen_fd_) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
            if (client_fd != -1) {
                int flags = fcntl(client_fd, F_GETFL, 0);
                if (flags != -1) {
                    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
                }
                
                int nodelay = 1;
                setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

                if (match_context.add_player(client_fd)) {
                    struct kevent change{};
                    EV_SET(&change, client_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
                    kevent(poll_fd_, &change, 1, nullptr, 0, nullptr);
                } else {
                    close(client_fd);
                }
            }
        } else {
            Common::Network::ClientInputPacket packet{};
            ssize_t bytes_read = recv(fd, &packet, sizeof(packet), 0);
            if (bytes_read > 0) {
                if (packet.op_code == Common::Network::OpCode::CLIENT_INPUT) {
                    match_context.process_player_input(fd, packet.direction, 0.016f);
                }
            } else if (bytes_read == 0 || (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                match_context.remove_player(fd);
                struct kevent change{};
                EV_SET(&change, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
                kevent(poll_fd_, &change, 1, nullptr, 0, nullptr);
                close(fd);
            }
        }
    }
}

template auto NetworkAcceptor::poll_events<MatchContext<MatchConfig<2, 50>>>(
    MatchContext<MatchConfig<2, 50>>& match_context) noexcept -> void;

} // namespace ragc::Server

#endif // __APPLE__
