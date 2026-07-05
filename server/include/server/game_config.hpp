#pragma once

#include "server/match_context.hpp"

namespace ragc::Server {

using StandardMatchConfig = MatchConfig<2, 60>;
using StandardMatchContext = MatchContext<StandardMatchConfig>;

} // namespace ragc::Server
