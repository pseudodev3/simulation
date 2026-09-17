#pragma once

#include <string>
#include <vector>

namespace sim {

enum class Activity {
    Sleeping,
    AtHome,
    Commuting,
    Working,
    Eating,
    Shopping,
    Socializing,
    Relaxing,
    Wandering,
    Visiting,
    Waiting
};

enum class PlaceType { Home, Workplace, Cafe, Shop, Park };

enum class IntentKind {
    None, Sleep, StayHome, Work, Eat, Shop, Relax, Socialize, Visit, Wander, ReturnHome
};

enum class InteractionPhase { None, Notice, Approach, Engage, Disengage };

struct Vec2 {
    float x{};
    float y{};
    template <typename T> requires requires(float a, float b) { T{a, b}; }
    operator T() const { return T{x, y}; }
};

struct Place {
    int id{};
    std::string name;
    PlaceType type{PlaceType::Home};
    Vec2 position{};
    int capacity{1};
};

struct Traits {
    float patience{0.5f};
    float ambition{0.5f};
    float sociability{0.5f};
    float impulsiveness{0.5f};
    float kindness{0.5f};
};

struct BehaviorSignature {
    float walkingSpeed{1.0f};
    float routinePreference{0.5f};
    float talkativeness{0.5f};
    float spontaneity{0.5f};
    float sleepTendency{0.5f};
};

struct Intent {
    IntentKind kind{IntentKind::None};
    int targetPlaceId{-1};
    int targetPersonId{-1};
    int createdDay{-1};
    int createdMinute{-1};
    int commitUntilMinute{-1};
    float utility{0.0f};
};

struct Relationship {
    int otherPersonId{-1};
    float familiarity{0.0f};
    float affinity{0.0f};
    float trust{0.0f};
    float tension{0.0f};
    int lastNotableInteractionDay{-1};
    int lastSeenDay{-1};
    int lastSeenMinute{-1};
    int interactionCount{0};
};

struct Memory {
    int day{};
    int minute{};
    std::string tag;
    int otherPersonId{-1};
    float intensity{0.0f};
};

struct Interaction {
    int id{-1};
    std::vector<int> participants;
    int initiatorId{-1};
    int placeId{-1};
    InteractionPhase phase{InteractionPhase::None};
    int startedDay{-1};
    int startedMinute{-1};
    int phaseMinute{-1};
};

struct Event {
    int day{};
    int minute{};
    int personId{-1};
    int otherPersonId{-1};
    int placeId{-1};
    std::string type;
    std::string text;
    float importance{0.0f};
};

} // namespace sim
