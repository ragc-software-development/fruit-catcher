#include "server/match_context.hpp"

using StandardFrutiMatchConfig = ragc::Server::MatchConfig<2, 50>;

int main()
{
    using namespace ragc::Server;

    MatchContext<StandardFrutiMatchConfig> game_loop;

    return 0;
}
