#pragma once

#include "sim/Person.hpp"
#include "sim/Types.hpp"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace sim {

class World {
public:
    World(std::uint32_t seed, int population = 50);

    void step(int minutes = 10);
    void runDays(int days, int minutesPerStep = 10);

    [[nodiscard]] int day() const { return day_; }
    [[nodiscard]] int minute() const { return minute_; }
    [[nodiscard]] std::uint32_t seed() const { return seed_; }

    [[nodiscard]] const std::vector<Person>& citizens() const { return citizens_; }
    [[nodiscard]] const std::vector<Place>& places() const { return places_; }
    [[nodiscard]] const std::vector<Event>& events() const { return events_; }

    [[nodiscard]] std::string clockLabel() const;
    [[nodiscard]] std::string dailySummary() const;

private:
    std::uint32_t seed_{};
    std::mt19937 rng_;
    int day_{1};
    int minute_{0};
    int population_{50};

    std::vector<Place> places_;
    std::vector<Person> citizens_;
    std::vector<Event> events_;

    std::vector<int> homeIds_;
    std::vector<int> workplaceIds_;
    int cafeId_{-1};
    int shopId_{-1};
    int parkId_{-1};

    void buildNeighborhood();
    void spawnCitizens();
    void seedHouseholdRelationships();

    void updateCitizen(Person& person, int stepMinutes);
    void applyNeeds(Person& person, int stepMinutes);
    void updateRoutine(Person& person);
    void handleInteractions();
    void handleEconomy(Person& person);

    void moveTo(Person& person, int placeId, Activity activity, const std::string& reason = {});
    int chooseEveningPlace(Person& person);

    Place* placeById(int id);
    const Place* placeById(int id) const;

    void emit(int personId, std::string type, std::string text, float importance);
    float random01();
    int randomInt(int minInclusive, int maxInclusive);
};

} // namespace sim
