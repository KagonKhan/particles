 #define GLM_ENABLE_EXPERIMENTAL
#include "app/app.hpp"
#include "utils/console_sink.hpp"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <system_error>
#include <utility>


/*
    THE TODO LIST:
    - Refactors and cleanups
    - Scene view - adding and removing elements from a widget.

    - Attractors (pull particles according to some math representation)
    - Boundaries (kill particles outside of bounds)
    - Keybinds / better IO / mouse
    - Scene / Scene editor
    - Saving settings to files
    - Scene saver
    - Multiple emitters and emitter types.
*/

void preferHardwareRenderer()
{
#ifdef __linux__
    std::error_code error;

    if (std::filesystem::exists("/dev/dxg", error)) {
        setenv("GALLIUM_DRIVER", "d3d12", 0);
    }

#endif
}

int main()
{
    spdlog::set_level(spdlog::level::trace);

    // Before anything the console would want to show has had a chance to be logged.
    auto log_sink = std::make_shared<ImGuiConsoleSink>();
    spdlog::default_logger()->sinks().push_back(log_sink);

    preferHardwareRenderer();

    App app {"Template Project", std::move(log_sink)};
    app.run();
}
