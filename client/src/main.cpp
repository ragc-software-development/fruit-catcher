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

    if (connect(sock_fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == -1) {
        std::cerr << "Connection failed" << std::endl;
        close(sock_fd);
        return 1;
    }

    int flags = fcntl(sock_fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
    }

    int nodelay = 1;
    setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    std::cout << "Connected to server. Playing game..." << std::endl;

    constexpr auto tick_duration = std::chrono::microseconds(16667);
    uint64_t frame_count = 0;

    while (true) {
        auto start_time = std::chrono::steady_clock::now();

        float angle = static_cast<float>(frame_count) * 0.05f;
        ragc::Common::Network::ClientInputPacket input_packet{};
        input_packet.op_code = ragc::Common::Network::OpCode::CLIENT_INPUT;
        input_packet.direction.x = std::cos(angle);
        input_packet.direction.y = std::sin(angle);

        send(sock_fd, &input_packet, sizeof(input_packet), 0);

        ragc::Common::Network::ServerMatchStatePacket<2, 50> state_packet{};
        ssize_t bytes_read = recv(sock_fd, &state_packet, sizeof(state_packet), 0);
        if (bytes_read > 0) {
            if (state_packet.op_code == ragc::Common::Network::OpCode::SERVER_MATCH_STATE) {
                std::cout << "\033[2J\033[H";
                std::cout << "=================================\n";
                std::cout << "        FRUIT CATCHER CLIENT     \n";
                std::cout << "=================================\n";
                std::cout << "Tick Number: " << state_packet.tick_number << "\n\n";

                std::cout << "Players:\n";
                for (size_t i = 0; i < 2; ++i) {
                    const auto& p = state_packet.players[i];
                    if (p.is_active) {
                        std::cout << "  [Player " << p.id << "] Position: ("
                                  << p.position.x << ", " << p.position.y
                                  << ") | Score: " << p.score << "\n";
                    }
                }

                std::cout << "\nActive Fruits:\n";
                int active_fruit_count = 0;
                for (size_t i = 0; i < 50; ++i) {
                    const auto& f = state_packet.fruits[i];
                    if (f.is_active) {
                        if (++active_fruit_count <= 5) {
                            std::cout << "  [Fruit " << f.id << "] Position: ("
                                      << f.position.x << ", " << f.position.y << ")\n";
                        }
                    }
                }
                if (active_fruit_count > 5) {
                    std::cout << "  ... and " << (active_fruit_count - 5) << " more active fruits.\n";
                }
                std::cout << "=================================\n";
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
