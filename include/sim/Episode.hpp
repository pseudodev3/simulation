#pragma once

#include "sim/World.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sim {

enum class ShotKind {
    Establishing,
    Person,
    Place
};

struct CitizenSnapshot {
    int id{};
    std::string name;
    int currentPlaceId{-1};
    Activity activity{Activity::AtHome};
    float cash{};
    float hunger{};
    float energy{};
    float stress{};
    float loneliness{};
};

struct EpisodeStep {
    int day{1};
    int minute{};
    std::vector<CitizenSnapshot> citizens;
    std::vector<Event> events;
};

struct DirectedShot {
    std::size_t stepIndex{};
    ShotKind kind{ShotKind::Establishing};
    int personId{-1};
    int placeId{-1};
    float seconds{0.35f};
    float importance{};
    std::string caption;
};

struct EpisodePlan {
    std::uint32_t seed{};
    int population{};
    int days{};
    int stepMinutes{10};
    std::vector<Place> places;
    std::vector<EpisodeStep> steps;
    std::vector<DirectedShot> shots;
};

struct EpisodeConfig {
    std::uint32_t seed{20260916};
    int population{30};
    int days{7};
    int stepMinutes{10};
    float baselineSecondsPerStep{0.35f};
};

class EpisodeRunner {
public:
    static EpisodePlan run(const EpisodeConfig& config);
    static void writeArtifacts(const EpisodePlan& plan, const std::filesystem::path& outputDirectory);
};

} // namespace sim
