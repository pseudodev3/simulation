#pragma once

#include "sim/Episode.hpp"

#include <filesystem>
#include <string>

namespace view {

struct EpisodeRenderOptions {
    std::filesystem::path outputDirectory{"episode"};
    std::string outputFileName{"mason-block.mp4"};
    int renderFps{10};
    int outputFps{24};
    int width{640};
    int height{360};
    bool keepFrames{false};
    float maxVideoSeconds{0.0f};
};

class EpisodeRenderer {
public:
    static bool render(const sim::EpisodePlan& plan,
                       const EpisodeRenderOptions& options,
                       std::string* errorMessage = nullptr);
};

} // namespace view
