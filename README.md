# Fruit Catcher Game

A real-time, low-latency, multiplayer console game written in **Modern C++ (C++20)**.

The project demonstrates high-performance networking architectures using platform-native event demultiplexers (`kqueue` on macOS / iOS and `epoll` on Linux) to power a single-threaded game loop running at a stable 60Hz.

---

## 🎮 Game Design

The objective of the game is simple: players run around a grid capturing spawned fruits.

- **Grid Resolution**: The game operates on a discrete 2D integer grid layout (1-to-1 visual-to-world mapping).
- **Match Logic**:
  - The match begins as soon as the configured number of players connect.
  - Fruits are spawned at unique locations at the beginning of the match.
  - Collisions are resolved using exact grid coordinate overlaps.
  - When all fruits are caught, the game finishes and a final score summary is displayed.
- **Controls**:
  - Use `W`, `A`, `S`, `D` or the **Arrow Keys** to move.
  - Press `Spacebar` to stop moving.
  - Press `Q` to quit at any time.

---

## 🏗️ Architecture & Network Protocol

The application is structured into three main modules:
1. **Common**: Shared headers defining structures, game configurations, and the binary protocol format.
2. **Server**: A high-performance single-threaded server listening on port `8080` containing:
   - Native OS network loops (`kqueue` or `epoll`).
   - A 60Hz game simulation loop.
3. **Client**: A lightweight console client that handles non-blocking keyboard input, performs a TCP handshake to fetch server limits, and renders a live ASCII representation of the game map.

### Decoupled Handshake Protocol
The client and server communicate via a packed binary TCP protocol:
- **Handshake (`ClientJoinRequest` / `ServerJoinResponse`)**: On connection, the client discovers configuration values (`map_bounds`, `max_players`, `max_fruits`) dynamically from the server.
- **Input (`ClientInputPacket`)**: Transmits movement direction integers (`-1`, `0`, `1`). Inputs are throttled on the server side (cooldown checks) to ensure smooth speed controls.
- **State Broadcast (`StandardMatchStatePacket`)**: The server broadcasts the entire game match state every tick. Decoupled capacities ensure the network packet size is uniform regardless of configuration limits.

---

## 🛠️ Build Instructions

The project requires a compiler that supports **C++20** and **CMake (version 3.15 or newer)**.

### Prerequisites

#### macOS
Ensure you have Xcode Command Line Tools installed:
```bash
xcode-select --install
```

#### Linux (Debian/Ubuntu)
Install build tools and CMake:
```bash
sudo apt-get update
sudo apt-get install build-essential cmake
```

### Building the Project

Run the following commands in the root directory:

```bash
# Generate the build system
cmake -B build

# Build the binaries
cmake --build build
```

This generates two executable binaries in the build directory:
- **Server**: `build/server/FruitCatcherServer`
- **Client**: `build/client/FruitCatcherClient`

---

## 🚀 Running the Game

### Step 1: Start the Server
Start the server in your terminal:
```bash
./build/server/FruitCatcherServer
```
You should see:
```text
Server listening on port 8080...
```

### Step 2: Connect Clients
Open a new terminal window/tab for each player and run:
```bash
./build/client/FruitCatcherClient
```

The server configuration defaults to a **1-player** or **2-player** match (depending on configuration parameters in `server/include/server/game_config.hpp`). Once the required number of players connect, the match starting sequence activates and the interactive game screen will render!
