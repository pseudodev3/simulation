#include "sim/Episode.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

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

std::pair<std::string, std::string> dialogueFor(const Event& event, const EpisodeStep& step) {
    if (event.personId < 0 || event.otherPersonId < 0) return {};
    const auto* a = citizenById(step, event.personId);
    const auto* b = citizenById(step, event.otherPersonId);
    if (!a || !b) return {};

    const int variant = std::abs(event.personId * 31 + event.otherPersonId * 17 + step.day * 7 + step.minute) % 4;

    if (event.type == "friendship") {
        static const std::pair<const char*, const char*> lines[] = {
            {"I'm glad we keep running into each other.", "Me too. Let's do this again."},
            {"You know, I actually look forward to these talks.", "Same here. It's nice having you around."},
            {"I needed that conversation today.", "Any time. Seriously."},
            {"We should hang out when neither of us is rushing somewhere.", "Deal. Let's make time for it."}
        };
        return lines[variant];
    }

    if (event.type == "relationship") {
        static const std::pair<const char*, const char*> lines[] = {
            {"Hey, I've seen you around a few times.", "Yeah. I was going to say hello."},
            {"Morning. You live around here, right?", "Just down the block. I'm here all the time."},
            {"I think we keep ending up in the same places.", "I noticed that too."},
            {"Hey. I don't think we've properly met.", "No, we haven't. Good to finally talk."}
        };
        return lines[variant];
    }

    if (event.type == "conversation") {
        if (step.minute < 10 * 60) {
            static const std::pair<const char*, const char*> morning[] = {
                {"Morning. You heading out?", "Yeah. Same routine as always."},
                {"You're up early today.", "Couldn't sleep much. Might as well get moving."},
                {"Looks like a quiet morning.", "Let's hope it stays that way."},
                {"Coffee before work?", "Absolutely. I need it today."}
            };
            return morning[variant];
        }
        if (step.minute >= 17 * 60) {
            static const std::pair<const char*, const char*> evening[] = {
                {"Long day?", "Very. I'm glad to be out of there."},
                {"You look tired.", "I am. Today just kept going."},
                {"How was work?", "Could've been better. Could've been worse."},
                {"You getting some air too?", "Yeah. I needed a minute outside."}
            };
            return evening[variant];
        }

        static const std::pair<const char*, const char*> daytime[] = {
            {"How's your day going?", "Not bad. Just trying to get through it."},
            {"You been busy today?", "Pretty much nonstop."},
            {"Everything good?", "Yeah. Just a normal day so far."},
            {"I keep seeing everybody out today.", "Same. The whole block feels awake."}
        };
        return daytime[variant];
    }

    return {};
}

DirectedShot directStep(const EpisodePlan& plan, std::size_t index, float baselineSeconds) {
    const auto& step = plan.steps[index];
    DirectedShot shot;
    shot.stepIndex = index;
    shot.seconds = baselineSeconds;

    const Event* dominant = nullptr;
    for (const auto& event : step.events) {
        if (!dominant || event.importance > dominant->importance) dominant = &event;
    }

    if (dominant && dominant->importance >= 0.15f) {
        shot.importance = dominant->importance;
        shot.personId = dominant->personId;
        shot.secondaryPersonId = dominant->otherPersonId;
        shot.placeId = dominant->placeId;
        shot.caption = dominant->text;

        if (dominant->personId >= 0) {
            shot.kind = ShotKind::Person;
            if (shot.placeId < 0) {
                if (const auto* citizen = citizenById(step, dominant->personId)) {
                    shot.placeId = citizen->currentPlaceId >= 0
                                 ? citizen->currentPlaceId : citizen->destinationPlaceId;
                }
            }
        } else {
            shot.kind = ShotKind::Place;
        }

        if (dominant->importance >= 0.70f) {
            shot.seconds = 7.5f + dominant->importance * 2.2f;
        } else if (dominant->importance >= 0.40f) {
            shot.seconds = 6.0f + dominant->importance * 1.8f;
        } else {
            shot.seconds = 3.8f + dominant->importance * 2.3f;
        }

        if (shot.secondaryPersonId >= 0) {
            const auto dialogue = dialogueFor(*dominant, step);
            shot.dialoguePrimary = dialogue.first;
            shot.dialogueSecondary = dialogue.second;
            shot.seconds = std::max(8.0f, shot.seconds);
        }
        return shot;
    }

    const bool dayOpening = step.minute == 6 * 60;
    const bool lunch = step.minute == 12 * 60;
    const bool evening = step.minute == 18 * 60;
    const bool night = step.minute == 22 * 60;
    if (dayOpening || lunch || evening || night) {
        shot.kind = ShotKind::Establishing;
        shot.seconds = dayOpening ? 2.8f : 1.6f;
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
            float progress = 1.0f;
            if (person.activity == Activity::Commuting && person.travelMinutesTotal > 0) {
                progress = 1.0f - static_cast<float>(person.travelMinutesRemaining)
                                      / static_cast<float>(person.travelMinutesTotal);
                progress = std::clamp(progress, 0.0f, 1.0f);
            }

            snapshot.citizens.push_back(CitizenSnapshot{
                person.id,
                person.name,
                person.currentPlaceId,
                person.originPlaceId,
                person.destinationPlaceId,
                progress,
                person.activity,
                person.cash,
                person.hunger,
                person.energy,
                person.stress,
                person.loneliness
            });
        }

        const auto& allEvents = world.events();
        for (; eventCursor < allEvents.size(); ++eventCursor) snapshot.events.push_back(allEvents[eventCursor]);

        plan.steps.push_back(std::move(snapshot));
        plan.shots.push_back(directStep(plan, plan.steps.size() - 1, config.baselineSecondsPerStep));
    }

    return plan;
}

void EpisodeRunner::writeArtifacts(const EpisodePlan& plan, const std::filesystem::path& outputDirectory) {
    std::filesystem::create_directories(outputDirectory);

    const auto jsonPath = outputDirectory / "episode.json";
    std::ofstream json(jsonPath);
    if (!json) throw std::runtime_error("Could not write " + jsonPath.string());

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
             << ",\"secondary_person_id\":" << shot.secondaryPersonId
             << ",\"place_id\":" << shot.placeId
             << ",\"seconds\":" << std::fixed << std::setprecision(2) << shot.seconds
             << ",\"importance\":" << std::fixed << std::setprecision(2) << shot.importance
             << ",\"caption\":\"" << escapeJson(shot.caption) << "\""
             << ",\"dialogue_primary\":\"" << escapeJson(shot.dialoguePrimary) << "\""
             << ",\"dialogue_secondary\":\"" << escapeJson(shot.dialogueSecondary) << "\""
             << ",\"events\":[";

        for (std::size_t e = 0; e < step.events.size(); ++e) {
            const auto& event = step.events[e];
            json << "{\"type\":\"" << escapeJson(event.type)
                 << "\",\"person_id\":" << event.personId
                 << ",\"other_person_id\":" << event.otherPersonId
                 << ",\"place_id\":" << event.placeId
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
    if (!timeline) throw std::runtime_error("Could not write " + timelinePath.string());

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
        if (!shot.dialoguePrimary.empty()) timeline << "    A: " << shot.dialoguePrimary << '\n';
        if (!shot.dialogueSecondary.empty()) timeline << "    B: " << shot.dialogueSecondary << '\n';
    }
}

} // namespace sim
