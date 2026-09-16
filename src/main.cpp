#include "sim/World.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::uint32_t seed = 1337;
    int population = 50;
    int days = 7;

    if (argc > 1) {
        seed = static_cast<std::uint32_t>(std::stoul(argv[1]));
    }
    if (argc > 2) {
        population = std::max(1, std::stoi(argv[2]));
    }
    if (argc > 3) {
        days = std::max(1, std::stoi(argv[3]));
    }

    sim::World world(seed, population);

    std::cout << "Crime City / life simulation\n"
              << "seed=" << seed
              << " population=" << population
              << " days=" << days << "\n\n";

    for (int day = 0; day < days; ++day) {
        const std::size_t eventStart = world.events().size();
        world.runDays(1);

        std::cout << "=== END OF DAY " << day + 1 << " ===\n";
        std::cout << world.dailySummary() << "\n";

        for (std::size_t i = eventStart; i < world.events().size(); ++i) {
            const auto& event = world.events()[i];
            if (event.importance >= 0.40f) {
                const int hour = event.minute / 60;
                const int minute = event.minute % 60;
                std::cout << "  [" << (hour < 10 ? "0" : "") << hour << ':'
                          << (minute < 10 ? "0" : "") << minute << "] "
                          << event.text << '\n';
            }
        }
        std::cout << '\n';
    }

    std::cout << "Recorded " << world.events().size()
              << " structured events for the future director/camera layer.\n";
    return EXIT_SUCCESS;
}
