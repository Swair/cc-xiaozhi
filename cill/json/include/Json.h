#pragma once

#include "json.hpp"

namespace cill {
using json = nlohmann::json;

using json_exception = nlohmann::detail::exception;

}  // namespace cill
