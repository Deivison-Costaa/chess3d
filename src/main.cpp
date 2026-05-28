#include "app/Application.h"
#include "app/HeadlessRunner.h"
#include "platform/PayloadBootstrap.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string_view>

static bool wantsHeadless(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string_view a(argv[i]);
        if (a == "--headless" || a == "--auto" || a == "--print-result") return true;
        if (a == "--mode" && i + 1 < argc) {
            std::string_view m(argv[i + 1]);
            if (m == "lan-host" || m == "lan-client") return true;
        }
    }
    return false;
}

int main(int argc, char** argv) {
#ifndef NDEBUG
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::info);
#endif
    spdlog::info("Chess3D — starting");

    // Build empacotado do Windows: extrai assets+engines embutidos na 1ª execução.
    // No-op em dev / Linux / AppImage.
    chess3d::platform::bootstrapExtractPayload();

    int rc = 0;

    if (wantsHeadless(argc, argv)) {
        chess3d::HeadlessConfig cfg;
        if (!chess3d::parseHeadlessCli(argc, argv, cfg)) return EXIT_FAILURE;
        rc = chess3d::runHeadless(cfg);
    } else {
        chess3d::Application app;
        rc = app.run();
    }

    spdlog::info("Chess3D — bye (rc={})", rc);
    return (rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
