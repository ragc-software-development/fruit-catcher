#pragma once

#include <concepts>
#include <system_error>
#include <cstdint>
#include <variant>

namespace ragc::Server {

template <typename E>
struct unexpected {
    E value;
    explicit constexpr unexpected(E val) : value(std::move(val)) {}
};

template <typename T, typename E>
class expected;

template <typename E>
class expected<void, E> {
    std::variant<std::monostate, E> var_;
public:
    constexpr expected() noexcept : var_(std::monostate{}) {}
    constexpr expected(unexpected<E> unexp) noexcept : var_(std::move(unexp.value)) {}

    constexpr bool has_value() const noexcept { return var_.index() == 0; }
    constexpr explicit operator bool() const noexcept { return has_value(); }
    constexpr const E& error() const& { return std::get<1>(var_); }
};

template <typename T>
concept GameMatchHandler = requires(T t, int client_fd) {
    { t.add_player(client_fd) } -> std::same_as<bool>;
    { t.remove_player(client_fd) } -> std::same_as<void>;
    { t.is_active() } -> std::same_as<bool>;
};

class NetworkAcceptor {
public:
    NetworkAcceptor() noexcept;
    ~NetworkAcceptor() noexcept;

    NetworkAcceptor(const NetworkAcceptor&) = delete;
    NetworkAcceptor& operator=(const NetworkAcceptor&) = delete;
    NetworkAcceptor(NetworkAcceptor&& other) noexcept;
    NetworkAcceptor& operator=(NetworkAcceptor&& other) noexcept;

    auto listen_on(uint16_t port) noexcept -> expected<void, std::error_code>;

    template <GameMatchHandler Handler>
    auto poll_events(Handler& match_context) noexcept -> void;

private:
    int listen_fd_{-1};
    int poll_fd_{-1};
};

} // namespace ragc::Server
