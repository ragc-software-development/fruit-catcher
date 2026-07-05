#pragma once

#include <array>
#include <algorithm>
#include <iostream>
#include <vector>
#include <random>
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
                player.position = Common::IVec2{
                    static_cast<int32_t>(3 * (connected_players_count_ + 1)),
                    static_cast<int32_t>(Config::map_bounds / 2)
                };
                player.score = 0;
                player.is_connected = true;
                player.last_move_tick = 0;

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

    auto process_player_input(int client_fd, const Common::IVec2& direction, float /*delta_time*/) noexcept -> void
    {
        if (!is_match_active_) {
            return;
        }

        // Throttle player movement: maximum 1 move per 6 ticks (~100ms)
        constexpr uint64_t MOVE_COOLDOWN_TICKS = 6;

        for (auto& player : players_) {
            if (player.is_connected && player.client_fd == client_fd) {
                if (current_tick_ - player.last_move_tick >= MOVE_COOLDOWN_TICKS) {
                    if (direction.x != 0 || direction.y != 0) {
                        player.position.x += direction.x;
                        player.position.y += direction.y;

                        player.position.x = std::clamp(player.position.x, 0, Config::map_bounds - 1);
                        player.position.y = std::clamp(player.position.y, 0, Config::map_bounds - 1);

                        player.last_move_tick = current_tick_;
                    }
                }
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

                if (player.position == fruit.position) {
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
            .map_bounds  = static_cast<float>(Config::map_bounds)
        };
    }

private:
    auto spawn_fruits() noexcept -> void
    {
        const int width = static_cast<int>(Config::map_bounds);
        const int height = static_cast<int>(Config::map_bounds);
        const size_t total_cells = static_cast<size_t>(width * height);

        // Cap the number of fruits to the total cells available to avoid infinite layout or overlap
        const size_t spawn_count = std::min(Config::max_fruits, total_cells);
        if (spawn_count < Config::max_fruits) {
            std::cerr << "[Warning] Configured max_fruits (" << Config::max_fruits 
                      << ") exceeds total map cells (" << total_cells 
                      << "). Spawning only " << spawn_count << " fruits." << std::endl;
        }

        // Generate all unique grid coordinate slots
        std::vector<Common::IVec2> cell_slots;
        cell_slots.reserve(total_cells);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                cell_slots.push_back(Common::IVec2{x, y});
            }
        }

        // Shuffle the slots
        std::mt19937 g(1337); // Use a fixed seed for deterministic layouts
        std::shuffle(cell_slots.begin(), cell_slots.end(), g);

        for (uint32_t i = 0; i < Config::max_fruits; ++i) {
            bool is_active = (i < spawn_count);
            Common::IVec2 pos = is_active ? cell_slots[i] : Common::IVec2{0, 0};

            fruits_[i] = Common::Fruit{
                .id = static_cast<Common::EntityId>(i + 1),
                .position = pos,
                .points_value = 10,
                .is_active = is_active};
        }
    }
};

} // namespace ragc::Server