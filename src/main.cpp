#include "app/Application.h"

#include <spdlog/spdlog.h>

#include <cstdlib>

int main() {
#ifndef NDEBUG
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::info);
#endif
    spdlog::info("Chess3D — starting");

    chess3d::Application app;
    const int rc = app.run();

    spdlog::info("Chess3D — bye (rc={})", rc);
    return (rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
