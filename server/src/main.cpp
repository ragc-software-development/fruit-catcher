#include <chrono>
#include <iostream>
#include <sys/socket.h>
#include <thread>
#include "server/game_config.hpp"
#include "server/network_acceptor.hpp"

int main()
{
    using namespace ragc::Server;

    StandardMatchContext game_loop;
    NetworkAcceptor acceptor;

    auto result = acceptor.listen_on(8080);
    if (!result) {
        std::cerr << "Failed to start listening on port 8080: " << result.error().message() << std::endl;
        return 1;
    }

    std::cout << "Server listening on port 8080..." << std::endl;

    constexpr auto tick_duration = std::chrono::microseconds(16667);

    while (true) {
        auto start_time = std::chrono::steady_clock::now();

        acceptor.poll_events(game_loop);
        game_loop.update();

        if (game_loop.is_active()) {
            auto packet = game_loop.generate_match_state_packet();
            for (const auto& player : game_loop.get_players()) {
                if (player.is_connected && player.client_fd != -1) {
                    send(player.client_fd, &packet, sizeof(packet), 0);
                }
            }
        }

        auto elapsed =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start_time);
        if (elapsed < tick_duration) {
            std::this_thread::sleep_for(tick_duration - elapsed);
        }
    }

    return 0;
}
