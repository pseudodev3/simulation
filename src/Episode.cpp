#include "sim/Episode.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace sim {
namespace {

std::string shotKindLabel(ShotKind kind) {
    switch (kind) {
        case ShotKind::Establishing: return "establishing";
        case ShotKind::Person: return "person";
        case ShotKind::Place: return "place";
    }
    return "establishing";
}

std::string activityLabel(Activity activity) {
    switch (activity) {
        case Activity::Sleeping: return "sleeping";
        case Activity::AtHome: return "at_home";
        case Activity::Commuting: return "commuting";
        case Activity::Working: return "working";
        case Activity::Eating: return "eating";
        case Activity::Shopping: return "shopping";
        case Activity::Socializing: return "socializing";
        case Activity::Relaxing: return "relaxing";
    }
    return "unknown";
}

std::string escapeJson(const std::string& value) {
    std::ostringstream out;
    for (const char ch : value) {
        switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(ch)) << std::dec;
                } else {
                    out << ch;
                }
        }
    }
    return out.str();
}

std::string clockLabel(int minute) {
    const int hour = minute / 60;
    const int mins = minute % 60;
    std::ostringstream out;
    out << std::setfill('0') << std::setw(2) << hour << ':' << std::setw(2) << mins;
    return out.str();
}

const CitizenSnapshot* citizenById(const EpisodeStep& step, int id) {
    for (const auto& citizen : step.citizens) {
        if (citizen.id == id) return &citizen;
    }
    return nullptr;
}

DirectedShot directStep(const EpisodePlan& plan, std::size_t index, float baselineSeconds) {
    const auto& step = plan.steps[index];
    DirectedShot shot;
    shot.stepIndex = index;
    shot.seconds = baselineSeconds;

    const Event* dominant = nullptr;
    for (const auto& event : step.events) {
        if (!dominant || event.importance > dominant->importance) {
            dominant = &event;
        }
    }

    if (dominant && dominant->importance >= 0.15f) {
        shot.importance = dominant->importance;
        shot.personId = dominant->personId;
        shot.caption = dominant->text;

        if (dominant->personId >= 0) {
            shot.kind = ShotKind::Person;
            if (const auto* citizen = citizenById(step, dominant->personId)) {
                shot.placeId = citizen->currentPlaceId;
            }
        } else {
            shot.kind = ShotKind::Place;
        }

        if (dominant->importance >= 0.70f) {
            shot.seconds = 3.8f + dominant->importance * 2.2f;
        } else if (dominant->importance >= 0.40f) {
            shot.seconds = 2.4f + dominant->importance * 1.8f;
        } else {
            shot.seconds = 1.0f + dominant->importance * 1.6f;
        }
        return shot;
    }

    const bool dayOpening = step.minute == 6 * 60;
    const bool lunch = step.minute == 12 * 60;
    const bool evening = step.minute == 18 * 60;
    const bool night = step.minute == 22 * 60;
    if (dayOpening || lunch || evening || night) {
        shot.kind = ShotKind::Establishing;
        shot.seconds = dayOpening ? 2.0f : 1.25f;
        if (dayOpening) {
            shot.caption = "DAY " + std::to_string(step.day);
            shot.importance = 0.20f;
        }
    }

    return shot;
}

} // namespace

EpisodePlan EpisodeRunner::run(const EpisodeConfig& rawConfig) {
    EpisodeConfig config = rawConfig;
    config.population = std::clamp(config.population, 1, 120);
    config.days = std::max(1, config.days);
    config.stepMinutes = std::clamp(config.stepMinutes, 1, 60);
    config.baselineSecondsPerStep = std::clamp(config.baselineSecondsPerStep, 0.05f, 3.0f);

    World world(config.seed, config.population);
    EpisodePlan plan;
    plan.seed = config.seed;
    plan.population = config.population;
    plan.days = config.days;
    plan.stepMinutes = config.stepMinutes;
    plan.places = world.places();

    const int totalMinutes = config.days * 24 * 60;
    const int totalSteps = (totalMinutes + config.stepMinutes - 1) / config.stepMinutes;
    plan.steps.reserve(static_cast<std::size_t>(totalSteps));
    plan.shots.reserve(static_cast<std::size_t>(totalSteps));

    std::size_t eventCursor = world.events().size();
    for (int i = 0; i < totalSteps; ++i) {
        world.step(config.stepMinutes);

        EpisodeStep snapshot;
        snapshot.day = world.day();
        snapshot.minute = world.minute();
        snapshot.citizens.reserve(world.citizens().size());
        for (const auto& person : world.citizens()) {
            snapshot.citizens.push_back(CitizenSnapshot{
                person.id,
                person.name,
                person.currentPlaceId,
                person.activity,
                person.cash,
                person.hunger,
                person.energy,
                person.stress,
                person.loneliness
            });
        }

        const auto& allEvents = world.events();
        for (; eventCursor < allEvents.size(); ++eventCursor) {
            snapshot.events.push_back(allEvents[eventCursor]);
        }

        plan.steps.push_back(std::move(snapshot));
        plan.shots.push_back(directStep(plan, plan.steps.size() - 1, config.baselineSecondsPerStep));
    }

    return plan;
}

void EpisodeRunner::writeArtifacts(const EpisodePlan& plan, const std::filesystem::path& outputDirectory) {
    std::filesystem::create_directories(outputDirectory);

    const auto jsonPath = outputDirectory / "episode.json";
    std::ofstream json(jsonPath);
    if (!json) {
        throw std::runtime_error("Could not write " + jsonPath.string());
    }

    json << "{\n"
         << "  \"seed\": " << plan.seed << ",\n"
         << "  \"population\": " << plan.population << ",\n"
         << "  \"days\": " << plan.days << ",\n"
         << "  \"step_minutes\": " << plan.stepMinutes << ",\n"
         << "  \"shots\": [\n";

    for (std::size_t i = 0; i < plan.shots.size(); ++i) {
        const auto& shot = plan.shots[i];
        const auto& step = plan.steps[shot.stepIndex];
        json << "    {\"step\":" << shot.stepIndex
             << ",\"day\":" << step.day
             << ",\"minute\":" << step.minute
             << ",\"kind\":\"" << shotKindLabel(shot.kind) << "\""
             << ",\"person_id\":" << shot.personId
             << ",\"place_id\":" << shot.placeId
             << ",\"seconds\":" << std::fixed << std::setprecision(2) << shot.seconds
             << ",\"importance\":" << std::fixed << std::setprecision(2) << shot.importance
             << ",\"caption\":\"" << escapeJson(shot.caption) << "\""
             << ",\"events\":[";

        for (std::size_t e = 0; e < step.events.size(); ++e) {
            const auto& event = step.events[e];
            json << "{\"type\":\"" << escapeJson(event.type)
                 << "\",\"person_id\":" << event.personId
                 << ",\"importance\":" << std::fixed << std::setprecision(2) << event.importance
                 << ",\"text\":\"" << escapeJson(event.text) << "\"}";
            if (e + 1 < step.events.size()) json << ',';
        }
        json << "]}";
        if (i + 1 < plan.shots.size()) json << ',';
        json << '\n';
    }
    json << "  ]\n}\n";

    const auto timelinePath = outputDirectory / "timeline.txt";
    std::ofstream timeline(timelinePath);
    if (!timeline) {
        throw std::runtime_error("Could not write " + timelinePath.string());
    }

    timeline << "MASON BLOCK AUTONOMOUS EPISODE\n"
             << "seed=" << plan.seed << " population=" << plan.population
             << " days=" << plan.days << "\n\n";

    int previousDay = -1;
    for (const auto& shot : plan.shots) {
        const auto& step = plan.steps[shot.stepIndex];
        if (step.day != previousDay) {
            previousDay = step.day;
            timeline << "\n=== DAY " << step.day << " ===\n";
        }
        if (!shot.caption.empty()) {
            timeline << '[' << clockLabel(step.minute) << "] " << shot.caption
                     << "  (score " << std::fixed << std::setprecision(2) << shot.importance << ")\n";
        }
    }
}

} // namespace sim
