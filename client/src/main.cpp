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
#include <termios.h>

struct TerminalRawMode {
    termios orig_termios;
    bool enabled{false};

    TerminalRawMode() {
        if (tcgetattr(STDIN_FILENO, &orig_termios) == 0) {
            termios raw = orig_termios;
            raw.c_lflag &= ~(ECHO | ICANON);
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 0;
            if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0) {
                enabled = true;
            }
        }
    }

    ~TerminalRawMode() {
        if (enabled) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        }
    }
};

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

    ragc::Common::Network::ServerJoinResponsePacket join_resp{};
    ssize_t handshake_bytes = recv(sock_fd, &join_resp, sizeof(join_resp), MSG_WAITALL);
    if (handshake_bytes != static_cast<ssize_t>(sizeof(join_resp))
        || join_resp.op_code != ragc::Common::Network::OpCode::SERVER_JOIN_RESPONSE) {
        std::cerr << "[Client] Handshake failed." << std::endl;
        close(sock_fd);
        return 1;
    }
    const int grid_size = static_cast<int>(join_resp.map_bounds);
    const size_t max_players = join_resp.max_players;
    const size_t max_fruits  = join_resp.max_fruits;
    std::cout << "[Client] Handshake OK — assigned FD: " << join_resp.assigned_client_id
              << " | map_bounds: " << join_resp.map_bounds
              << " | max_players: " << max_players
              << " | max_fruits: " << max_fruits << std::endl;

    int flags = fcntl(sock_fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
    }

    int nodelay = 1;
    setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    TerminalRawMode raw_mode;
    std::cout << "[Client] Successfully connected to server. Socket options configured (O_NONBLOCK, TCP_NODELAY)." << std::endl;
    std::cout << "Controls: WASD or Arrow Keys to move. Q to quit." << std::endl;

    constexpr auto tick_duration = std::chrono::microseconds(16667);
    uint64_t frame_count = 0;

    float move_x = 0.0f;
    float move_y = 0.0f;

    while (true) {
        auto start_time = std::chrono::steady_clock::now();

        char ch;
        while (read(STDIN_FILENO, &ch, 1) > 0) {
            if (ch == 'q' || ch == 'Q') {
                close(sock_fd);
                return 0;
            }
            if (ch == '\033') { // Escape sequence
                char seq[2];
                if (read(STDIN_FILENO, &seq[0], 1) > 0 && read(STDIN_FILENO, &seq[1], 1) > 0) {
                    if (seq[0] == '[') {
                        switch (seq[1]) {
                            case 'A': move_y = -1.0f; move_x = 0.0f; break; // Up
                            case 'B': move_y = 1.0f;  move_x = 0.0f; break; // Down
                            case 'C': move_x = 1.0f;  move_y = 0.0f; break; // Right
                            case 'D': move_x = -1.0f; move_y = 0.0f; break; // Left
                        }
                    }
                }
            } else {
                switch (ch) {
                    case 'w': case 'W': move_y = -1.0f; move_x = 0.0f; break;
                    case 's': case 'S': move_y = 1.0f;  move_x = 0.0f; break;
                    case 'a': case 'A': move_x = -1.0f; move_y = 0.0f; break;
                    case 'd': case 'D': move_x = 1.0f;  move_y = 0.0f; break;
                    case ' ':           move_x = 0.0f;  move_y = 0.0f; break; // Space stops
                }
            }
        }

        // Normalize direction vector if moving diagonally
        float len = std::sqrt(move_x * move_x + move_y * move_y);
        if (len > 0.0f) {
            move_x /= len;
            move_y /= len;
        }

        ragc::Common::Network::ClientInputPacket input_packet{};
        input_packet.op_code = ragc::Common::Network::OpCode::CLIENT_INPUT;
        input_packet.direction.x = move_x;
        input_packet.direction.y = move_y;

        send(sock_fd, &input_packet, sizeof(input_packet), 0);

        ragc::Common::Network::StandardMatchStatePacket state_packet{};
        ssize_t bytes_read = recv(sock_fd, &state_packet, sizeof(state_packet), 0);
        if (bytes_read > 0) {
            if (state_packet.op_code == ragc::Common::Network::OpCode::SERVER_MATCH_STATE) {
                const int GRID_SIZE = grid_size;
                char grid[GRID_SIZE][GRID_SIZE];
                for (int y = 0; y < GRID_SIZE; ++y) {
                    for (int x = 0; x < GRID_SIZE; ++x) {
                        grid[y][x] = '.';
                    }
                }

                bool any_fruit_active = false;
                for (size_t i = 0; i < max_fruits; ++i) {
                    const auto& f = state_packet.fruits[i];
                    if (f.is_active) {
                        any_fruit_active = true;
                        int gx = std::clamp(static_cast<int>(f.position.x), 0, GRID_SIZE - 1);
                        int gy = std::clamp(static_cast<int>(f.position.y), 0, GRID_SIZE - 1);
                        grid[gy][gx] = 'F';
                    }
                }

                if (!any_fruit_active) {
                    std::cout << "\033[2J\033[H";
                    std::cout << "=========================================\n";
                    std::cout << "     GAME OVER - ALL FRUITS COLLECTED    \n";
                    std::cout << "=========================================\n";
                    std::cout << "Final Scores:\n";
                    for (size_t i = 0; i < max_players; ++i) {
                        const auto& p = state_packet.players[i];
                        if (p.is_active) {
                            std::cout << "  - Player " << (i + 1) << " (FD: " << p.id << "): " << p.score << "\n";
                        }
                    }
                    std::cout << "=========================================\n";
                    break;
                }

                for (size_t i = 0; i < max_players; ++i) {
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
                for (size_t i = 0; i < max_players; ++i) {
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
