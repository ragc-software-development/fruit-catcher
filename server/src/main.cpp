#include "server/match_context.hpp"

using StandardFrutiMatchConfig = ragc::server::MatchConfig<2, 50>;

int main()
{
    using namespace ragc::server;

    MatchContext<StandardFrutiMatchConfig> game_loop;

    return 0;
}
