#pragma once

#include "spdlog/spdlog.h"

namespace cill {



#define TRACE(...) spdlog::trace(__VA_ARGS__)
#define INFO(...) spdlog::info(__VA_ARGS__)
#define WARN(...) spdlog::warn(__VA_ARGS__)
#define ERROR(...) spdlog::error(__VA_ARGS__)
#define CRITICAL(...) spdlog::critical(__VA_ARGS__)

#if 1
#define DEBUG(...) WARN("{}, {}, {}({})", __FUNCTION__, #__VA_ARGS__, __FILE__, __LINE__)
#else
#define DEBUG(...) spdlog::debug(__VA_ARGS__)
#endif

}  // namespace cill
