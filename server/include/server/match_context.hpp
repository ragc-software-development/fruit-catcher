#pragma once

#include <array>
#include <algorithm>
#include <iostream>
#include "common/protocol.hpp"
#include "common/types.hpp"
#include "server/match_config.hpp"
#include "server/network_acceptor.hpp"

namespace ragc::Server {

template <ValidMatchConfig Config>
class MatchContext
{
private:
    std::array<Common::Player, Config::max_players> players_;
    std::array<Common::Fruit, Config::max_fruits> fruits_;

    uint64_t current_tick_{0};
    size_t connected_players_count_{0};
    bool is_match_active_{false};
    bool is_match_finished_{false};

public:
    MatchContext()
    {
        spawn_fruits();
    }

    ~MatchContext() = default;

    MatchContext(const MatchContext&) = delete;
    MatchContext& operator=(const MatchContext&) = delete;
    MatchContext(MatchContext&&) = delete;
    MatchContext& operator=(MatchContext&&) = delete;

    auto add_player(int client_fd) noexcept -> bool
    {
        if (connected_players_count_ >= Config::max_players) {
            return false;
        }

        for (auto& player : players_) {
            if (!player.is_connected) {
                player.client_fd = client_fd;
                player.position = Common::Vec2{10.0f * static_cast<float>(connected_players_count_ + 1), 25.0f};
                player.score = 0;
                player.is_connected = true;

                ++connected_players_count_;

                std::cout << "[Server] Player connected on FD: " << client_fd 
                          << " (" << connected_players_count_ << "/" << Config::max_players << ")" << std::endl;

                if (connected_players_count_ == Config::max_players) {
                    is_match_active_ = true;
                    std::cout << "[Server] Match starting! All players connected." << std::endl;
                }
                return true;
            }
        }
        return false;
    }

    auto remove_player(int client_fd) noexcept -> void
    {
        for (auto& player : players_) {
            if (player.is_connected && player.client_fd == client_fd) {
                player.is_connected = false;
                player.client_fd = -1;

                if (connected_players_count_ > 0) {
                    --connected_players_count_;
                }

                std::cout << "[Server] Player disconnected on FD: " << client_fd 
                          << " (" << connected_players_count_ << "/" << Config::max_players << ")" << std::endl;

                is_match_active_ = false;
                break;
            }
        }
    }

    auto process_player_input(int client_fd, const Common::Vec2& direction, float delta_time) noexcept -> void
    {
        if (!is_match_active_) {
            return;
        }

        for (auto& player : players_) {
            if (player.is_connected && player.client_fd == client_fd) {
                constexpr float SPEED = 15.0f;
                player.position.x += direction.x * SPEED * delta_time;
                player.position.y += direction.y * SPEED * delta_time;

                player.position.x = std::clamp(player.position.x, 0.0f, Config::map_bounds);
                player.position.y = std::clamp(player.position.y, 0.0f, Config::map_bounds);

                break;
            }
        }
    }

    auto update() noexcept -> void
    {
        if (!is_match_active_) {
            return;
        }

        ++current_tick_;

        for (auto& player : players_) {
            if (!player.is_connected) [[unlikely]] {
                continue;
            }

            for (auto& fruit : fruits_) {
                if (!fruit.is_active) {
                    continue;
                }

                if (player.position.distance_sq(fruit.position) < Config::collision_radius_sq) {
                    fruit.is_active = false;
                    player.score += fruit.points_value;
                }
            }
        }

        bool any_fruit_active = false;
        for (const auto& fruit : fruits_) {
            if (fruit.is_active) {
                any_fruit_active = true;
                break;
            }
        }

        if (!any_fruit_active) {
            is_match_active_ = false;
            is_match_finished_ = true;
            std::cout << "[Server] Match Finished! All fruits collected." << std::endl;
            std::cout << "Final Scores:" << std::endl;
            for (size_t i = 0; i < Config::max_players; ++i) {
                std::cout << "  - Player " << (i + 1) << " (FD: " << players_[i].client_fd << "): " << players_[i].score << std::endl;
            }
        }
    }

    [[nodiscard]] auto generate_match_state_packet() const noexcept
        -> Common::Network::StandardMatchStatePacket
    {
        static_assert(Config::max_players <= Common::Network::PROTOCOL_MAX_PLAYERS, "Config::max_players exceeds protocol limit");
        static_assert(Config::max_fruits <= Common::Network::PROTOCOL_MAX_FRUITS, "Config::max_fruits exceeds protocol limit");

        Common::Network::StandardMatchStatePacket packet{};

        packet.op_code = Common::Network::OpCode::SERVER_MATCH_STATE;
        packet.tick_number = current_tick_;

        for (size_t i = 0; i < Config::max_players; ++i) {
            packet.players[i] = Common::Network::NetworkPlayerState{.id = players_[i].client_fd,
                                                                    .position = players_[i].position,
                                                                    .score = players_[i].score,
                                                                    .is_active = players_[i].is_connected};
        }

        for (size_t i = 0; i < Config::max_fruits; ++i) {
            packet.fruits[i] = Common::Network::NetworkFruitState{.id = static_cast<uint32_t>(fruits_[i].id),
                                                                  .position = fruits_[i].position,
                                                                  .is_active = fruits_[i].is_active};
        }

        return packet;
    }

    [[nodiscard]] constexpr auto is_active() const noexcept -> bool
    {
        return is_match_active_;
    }

    [[nodiscard]] constexpr auto is_finished() const noexcept -> bool
    {
        return is_match_finished_;
    }

    [[nodiscard]] constexpr auto get_players() const noexcept -> const std::array<Common::Player, Config::max_players>&
    {
        return players_;
    }

    [[nodiscard]] constexpr auto get_match_config() const noexcept -> MatchConfigSnapshot
    {
        return MatchConfigSnapshot{
            .max_players = static_cast<uint32_t>(Config::max_players),
            .max_fruits  = static_cast<uint32_t>(Config::max_fruits),
            .map_bounds  = Config::map_bounds
        };
    }

private:
    auto spawn_fruits() noexcept -> void
    {
        for (uint32_t i = 0; i < Config::max_fruits; ++i) {
            fruits_[i] = Common::Fruit{
                .id = static_cast<Common::EntityId>(i + 1),
                .position = Common::Vec2{.x = static_cast<float>((i * 7) % static_cast<int>(Config::map_bounds)),
                                         .y = static_cast<float>((i * 13) % static_cast<int>(Config::map_bounds))},
                .points_value = 10,
                .is_active = true};
        }
    }
};

} // namespace ragc::Server