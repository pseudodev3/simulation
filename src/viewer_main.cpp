#include "sim/World.hpp"
#include "view/NeighborhoodRenderer.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::uint32_t seed = 20260916;
    int population = 30;

    try {
        if (argc > 1) {
            seed = static_cast<std::uint32_t>(std::stoul(argv[1]));
        }
        if (argc > 2) {
            population = std::clamp(std::stoi(argv[2]), 1, 120);
        }
    } catch (const std::exception& error) {
        std::cerr << "Invalid arguments: " << error.what() << '\n'
                  << "Usage: simulation_viewer [seed] [population]\n";
        return EXIT_FAILURE;
    }

    sim::World world(seed, population);
    view::NeighborhoodRenderer renderer(world);
    return renderer.run();
}
