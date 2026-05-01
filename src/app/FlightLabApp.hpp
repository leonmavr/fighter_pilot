#pragma once

namespace planet_aces::app {

class FlightLabApp {
public:
    int run(int argc, char** argv);

private:
    int run_headless_smoke();
    int run_interactive();
};

}  // namespace planet_aces::app