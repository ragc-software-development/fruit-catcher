#pragma once

#include "server/match_context.hpp"

namespace ragc::Server {

using StandardMatchConfig = MatchConfig<2, 50>;
using StandardMatchContext = MatchContext<StandardMatchConfig>;

} // namespace ragc::Server
