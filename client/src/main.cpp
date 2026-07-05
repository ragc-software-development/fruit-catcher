#include "common/protocol.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cmath>
#include <random>

int main()
{
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address" << std::endl;
        close(sock_fd);
        return 1;
    }

    std::cout << "[Client] Attempting to connect to 127.0.0.1:8080..." << std::endl;

    if (connect(sock_fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == -1) {
        std::cerr << "[Client] Connection failed" << std::endl;
        close(sock_fd);
        return 1;
    }

    int flags = fcntl(sock_fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
    }

    int nodelay = 1;
    setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    std::cout << "[Client] Successfully connected to server. Socket options configured (O_NONBLOCK, TCP_NODELAY)." << std::endl;

    constexpr auto tick_duration = std::chrono::microseconds(16667);
    constexpr int  direction_change_interval = 60; // ticks (~1 second)

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * 3.14159265f);

    float current_angle = angle_dist(rng);
    uint64_t frame_count = 0;

    while (true) {
        auto start_time = std::chrono::steady_clock::now();

        if (frame_count % direction_change_interval == 0) {
            current_angle = angle_dist(rng);
        }

        ragc::Common::Network::ClientInputPacket input_packet{};
        input_packet.op_code = ragc::Common::Network::OpCode::CLIENT_INPUT;
        input_packet.direction.x = std::cos(current_angle);
        input_packet.direction.y = std::sin(current_angle);

        send(sock_fd, &input_packet, sizeof(input_packet), 0);

        ragc::Common::Network::ServerMatchStatePacket<2, 50> state_packet{};
        ssize_t bytes_read = recv(sock_fd, &state_packet, sizeof(state_packet), 0);
        if (bytes_read > 0) {
            if (state_packet.op_code == ragc::Common::Network::OpCode::SERVER_MATCH_STATE) {
                constexpr int GRID_SIZE = 30;
                char grid[GRID_SIZE][GRID_SIZE];
                for (int y = 0; y < GRID_SIZE; ++y) {
                    for (int x = 0; x < GRID_SIZE; ++x) {
                        grid[y][x] = '.';
                    }
                }

                for (size_t i = 0; i < 50; ++i) {
                    const auto& f = state_packet.fruits[i];
                    if (f.is_active) {
                        int gx = std::clamp(static_cast<int>(f.position.x), 0, GRID_SIZE - 1);
                        int gy = std::clamp(static_cast<int>(f.position.y), 0, GRID_SIZE - 1);
                        grid[gy][gx] = 'F';
                    }
                }

                for (size_t i = 0; i < 2; ++i) {
                    const auto& p = state_packet.players[i];
                    if (p.is_active) {
                        int gx = std::clamp(static_cast<int>(p.position.x), 0, GRID_SIZE - 1);
                        int gy = std::clamp(static_cast<int>(p.position.y), 0, GRID_SIZE - 1);
                        grid[gy][gx] = static_cast<char>('1' + i);
                    }
                }

                std::cout << "\033[2J\033[H";
                std::cout << "=========================================\n";
                std::cout << "          FRUIT CATCHER CLIENT           \n";
                std::cout << "=========================================\n";
                std::cout << "Tick Number: " << state_packet.tick_number << "\n\n";

                std::cout << " +" << std::string(GRID_SIZE * 2, '-') << "+\n";
                for (int y = 0; y < GRID_SIZE; ++y) {
                    std::cout << " |";
                    for (int x = 0; x < GRID_SIZE; ++x) {
                        std::cout << grid[y][x] << ' ';
                    }
                    std::cout << "|\n";
                }
                std::cout << " +" << std::string(GRID_SIZE * 2, '-') << "+\n";
                std::cout << " Legend: 1/2 = Players, F = Fruit, . = Empty\n\n";

                std::cout << "Players Status:\n";
                for (size_t i = 0; i < 2; ++i) {
                    const auto& p = state_packet.players[i];
                    if (p.is_active) {
                        std::cout << "  - [Player " << (i + 1) << " (FD: " << p.id << ")] Score: " << p.score
                                  << " | Pos: (" << p.position.x << ", " << p.position.y << ")\n";
                    }
                }
                std::cout << "=========================================\n";
            }
        } else if (bytes_read == 0) {
            std::cout << "Server closed connection." << std::endl;
            break;
        }

        ++frame_count;

        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start_time);
        if (elapsed < tick_duration) {
            std::this_thread::sleep_for(tick_duration - elapsed);
        }
    }

    close(sock_fd);
    return 0;
}
