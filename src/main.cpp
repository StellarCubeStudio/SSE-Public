#include "Engine.h"
#include <cstdlib>

int main() {
    Engine engine;

    if (!engine.initialize()) {
        LOG_ERROR("Engine initialization failed!");
        return EXIT_FAILURE;
    }

    LOG_INFO("Starting Engine Loop...");
    engine.run();

    return EXIT_SUCCESS;
}