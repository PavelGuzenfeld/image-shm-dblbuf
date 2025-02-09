#include <cerrno>       // errno
#include <fmt/format.h> // fmt::format
#include <stdexcept>    // std::runtime_error
#include <string_view>  // std::string_view

namespace shm::impl
{
    constexpr inline void handle_errorno(int errorno, std::string_view trace_fn_name)
    {
        if (errorno == 0)
        {
            return;
        }
        throw std::runtime_error(fmt::format("{} failed: {}", trace_fn_name, strerror(errorno)));
    }
} // namespace shm::impl