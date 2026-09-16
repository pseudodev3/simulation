#include "sim/Episode.hpp"
#include "view/EpisodeRenderer.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

void printUsage() {
    std::cout << "Usage:\n"
              << "  simulation_episode [seed] [population] [days] [output_dir] [options]\n\n"
              << "Options:\n"
              << "  --plan-only       simulate and write episode.json/timeline.txt without video\n"
              << "  --smoke           render only the first 5 seconds (CI/debug)\n"
              << "  --keep-frames     keep generated PNG frames after ffmpeg finishes\n"
              << "  --help            show this help\n";
}

} // namespace

int main(int argc, char** argv) {
    sim::EpisodeConfig config;
    std::filesystem::path outputDirectory = "episode";
    bool planOnly = false;
    bool smoke = false;
    bool keepFrames = false;

    try {
        int positional = 0;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help") {
                printUsage();
                return EXIT_SUCCESS;
            }
            if (arg == "--plan-only") {
                planOnly = true;
                continue;
            }
            if (arg == "--smoke") {
                smoke = true;
                continue;
            }
            if (arg == "--keep-frames") {
                keepFrames = true;
                continue;
            }
            if (!arg.empty() && arg.front() == '-') {
                std::cerr << "Unknown option: " << arg << '\n';
                return EXIT_FAILURE;
            }

            switch (positional++) {
                case 0: config.seed = static_cast<std::uint32_t>(std::stoul(arg)); break;
                case 1: config.population = std::clamp(std::stoi(arg), 1, 120); break;
                case 2: config.days = std::max(1, std::stoi(arg)); break;
                case 3: outputDirectory = arg; break;
                default:
                    std::cerr << "Too many positional arguments\n";
                    printUsage();
                    return EXIT_FAILURE;
            }
        }

        std::cout << "Mason Block autonomous episode\n"
                  << "seed=" << config.seed
                  << " population=" << config.population
                  << " days=" << config.days << "\n"
                  << "No runtime input is used after the simulation begins.\n\n";

        sim::EpisodePlan plan = sim::EpisodeRunner::run(config);
        sim::EpisodeRunner::writeArtifacts(plan, outputDirectory);

        float estimatedSeconds = 0.0f;
        for (const auto& shot : plan.shots) estimatedSeconds += shot.seconds;
        std::cout << "Simulation complete: " << plan.steps.size() << " deterministic steps\n"
                  << "Director complete: " << plan.shots.size() << " autonomous shots\n"
                  << "Estimated episode length: " << static_cast<int>(estimatedSeconds) << " seconds\n"
                  << "Timeline: " << (outputDirectory / "timeline.txt").string() << '\n'
                  << "Plan: " << (outputDirectory / "episode.json").string() << "\n";

        if (planOnly) {
            std::cout << "Plan-only run complete.\n";
            return EXIT_SUCCESS;
        }

        view::EpisodeRenderOptions renderOptions;
        renderOptions.outputDirectory = outputDirectory;
        renderOptions.keepFrames = keepFrames;
        renderOptions.maxVideoSeconds = smoke ? 5.0f : 0.0f;

        std::string renderError;
        if (!view::EpisodeRenderer::render(plan, renderOptions, &renderError)) {
            std::cerr << "Episode render failed: " << renderError << '\n';
            return EXIT_FAILURE;
        }

        std::cout << "Video: " << (outputDirectory / renderOptions.outputFileName).string() << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Episode generation failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
