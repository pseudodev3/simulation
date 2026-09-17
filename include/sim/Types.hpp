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
    Relaxing
};

enum class PlaceType {
    Home,
    Workplace,
    Cafe,
    Shop,
    Park
};

struct Vec2 {
    float x{};
    float y{};

    template <typename T>
        requires requires(float a, float b) { T{a, b}; }
    operator T() const {
        return T{x, y};
    }
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

struct Relationship {
    int otherPersonId{-1};
    float familiarity{0.0f};
    float affinity{0.0f};
    int lastNotableInteractionDay{-1};
};

struct Memory {
    int day{};
    int minute{};
    std::string tag;
    int otherPersonId{-1};
    float intensity{0.0f};
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
