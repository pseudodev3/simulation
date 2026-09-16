#include "sim/World.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace sim {
namespace {

float clamp100(float value) {
    return std::clamp(value, 0.0f, 100.0f);
}

} // namespace

World::World(std::uint32_t seed, int population)
    : seed_(seed), rng_(seed), population_(std::max(1, population)) {
    buildNeighborhood();
    spawnCitizens();
    seedHouseholdRelationships();
    emit(-1, "world", "Neighborhood simulation started", 0.2f);
}

void World::buildNeighborhood() {
    int nextId = 0;

    for (int i = 0; i < 12; ++i) {
        const float x = 30.0f + static_cast<float>(i % 4) * 70.0f;
        const float y = 30.0f + static_cast<float>(i / 4) * 58.0f;
        places_.push_back(Place{nextId, "Apartment " + std::to_string(i + 1), PlaceType::Home, {x, y}, 6});
        homeIds_.push_back(nextId++);
    }

    const std::array<std::string, 5> jobs = {
        "Mason Garage", "Corner Market", "Riverside Diner", "Briar Office", "North Warehouse"
    };

    for (int i = 0; i < static_cast<int>(jobs.size()); ++i) {
        places_.push_back(Place{nextId, jobs[i], PlaceType::Workplace,
                                {360.0f + static_cast<float>(i % 2) * 85.0f,
                                 40.0f + static_cast<float>(i) * 58.0f}, 20});
        workplaceIds_.push_back(nextId++);
    }

    cafeId_ = nextId;
    places_.push_back(Place{nextId++, "Blue Cup Cafe", PlaceType::Cafe, {285.0f, 245.0f}, 24});

    shopId_ = nextId;
    places_.push_back(Place{nextId++, "Mason Convenience", PlaceType::Shop, {285.0f, 315.0f}, 18});

    parkId_ = nextId;
    places_.push_back(Place{nextId++, "Willow Park", PlaceType::Park, {105.0f, 245.0f}, 40});
}

void World::spawnCitizens() {
    static const std::array<std::string, 24> firstNames = {
        "Marcus", "Maya", "Daniel", "Nora", "Eli", "Sarah", "Jonah", "Lena",
        "Andre", "Tara", "Miles", "Rosa", "Noah", "Iris", "Caleb", "Jade",
        "Victor", "Amara", "Theo", "Naomi", "Isaac", "Mina", "Owen", "Leah"
    };

    static const std::array<std::string, 24> lastNames = {
        "Reed", "Cole", "Parker", "Brooks", "Hayes", "Kim", "Ortiz", "Stone",
        "Bennett", "Price", "Ward", "Foster", "Diaz", "Grant", "Turner", "Ross",
        "Morgan", "Bell", "Morris", "Bailey", "Gray", "Cooper", "Rivera", "James"
    };

    citizens_.reserve(population_);

    for (int i = 0; i < population_; ++i) {
        Person person;
        person.id = i;
        person.name = firstNames[static_cast<std::size_t>(randomInt(0, static_cast<int>(firstNames.size()) - 1))]
                    + " "
                    + lastNames[static_cast<std::size_t>(randomInt(0, static_cast<int>(lastNames.size()) - 1))];
        person.age = randomInt(19, 61);
        person.homeId = homeIds_[static_cast<std::size_t>(randomInt(0, static_cast<int>(homeIds_.size()) - 1))];
        person.workplaceId = workplaceIds_[static_cast<std::size_t>(randomInt(0, static_cast<int>(workplaceIds_.size()) - 1))];
        person.currentPlaceId = person.homeId;

        person.traits = Traits{random01(), random01(), random01(), random01(), random01()};
        person.cash = 80.0f + random01() * 270.0f;
        person.hunger = 5.0f + random01() * 22.0f;
        person.energy = 70.0f + random01() * 28.0f;
        person.stress = 5.0f + random01() * 28.0f;
        person.loneliness = 10.0f + random01() * 35.0f;
        person.jobSatisfaction = 35.0f + random01() * 55.0f;

        person.shiftStart = 7 * 60 + 30 + randomInt(0, 3) * 30;
        person.shiftEnd = person.shiftStart + 8 * 60;
        person.dailyWage = 65.0f + random01() * 55.0f;
        person.housingCostPerDay = 10.0f + random01() * 12.0f;

        citizens_.push_back(std::move(person));
    }
}

void World::seedHouseholdRelationships() {
    for (std::size_t i = 0; i < citizens_.size(); ++i) {
        for (std::size_t j = i + 1; j < citizens_.size(); ++j) {
            if (citizens_[i].homeId != citizens_[j].homeId) {
                continue;
            }

            auto& a = ensureRelationship(citizens_[i], citizens_[j].id);
            auto& b = ensureRelationship(citizens_[j], citizens_[i].id);
            const float affinity = 25.0f + random01() * 45.0f;
            a.familiarity = b.familiarity = 55.0f;
            a.affinity = b.affinity = affinity;
        }
    }
}

void World::step(int minutes) {
    const int stepMinutes = std::max(1, minutes);

    if (minute_ == 0) {
        for (auto& citizen : citizens_) {
            citizen.paidToday = false;
        }
    }

    for (auto& citizen : citizens_) {
        applyNeeds(citizen, stepMinutes);
        handleEconomy(citizen);
        updateRoutine(citizen);
    }

    handleInteractions();

    minute_ += stepMinutes;
    while (minute_ >= 24 * 60) {
        minute_ -= 24 * 60;
        ++day_;
    }
}

void World::runDays(int days, int minutesPerStep) {
    const int totalMinutes = std::max(0, days) * 24 * 60;
    const int stepMinutes = std::max(1, minutesPerStep);
    for (int elapsed = 0; elapsed < totalMinutes; elapsed += stepMinutes) {
        step(stepMinutes);
    }
}

void World::applyNeeds(Person& person, int stepMinutes) {
    const float hours = static_cast<float>(stepMinutes) / 60.0f;

    if (person.activity == Activity::Sleeping) {
        person.energy += 16.0f * hours;
        person.hunger += 2.0f * hours;
        person.stress -= 6.0f * hours;
    } else {
        person.energy -= (person.activity == Activity::Working ? 6.0f : 4.0f) * hours;
        person.hunger += 6.5f * hours;

        if (person.activity == Activity::Working) {
            const float dissatisfaction = (100.0f - person.jobSatisfaction) / 100.0f;
            person.stress += (2.0f + dissatisfaction * 5.0f) * hours;
            person.loneliness += 0.6f * hours;
        } else if (person.activity == Activity::Relaxing || person.activity == Activity::Socializing) {
            person.stress -= 4.0f * hours;
        } else {
            person.stress -= 0.6f * hours;
        }
    }

    if (person.hunger > 80.0f) {
        person.stress += 2.0f * hours;
    }
    if (person.energy < 20.0f) {
        person.stress += 2.0f * hours;
    }

    person.energy = clamp100(person.energy);
    person.hunger = clamp100(person.hunger);
    person.stress = clamp100(person.stress);
    person.loneliness = clamp100(person.loneliness);
}

void World::updateCitizen(Person& person, int stepMinutes) {
    applyNeeds(person, stepMinutes);
    handleEconomy(person);
    updateRoutine(person);
}

void World::updateRoutine(Person& person) {
    if (minute_ < 6 * 60) {
        moveTo(person, person.homeId, Activity::Sleeping);
        return;
    }

    if (minute_ < person.shiftStart) {
        moveTo(person, person.homeId, Activity::AtHome);
        return;
    }

    const bool lunchWindow = minute_ >= 12 * 60 && minute_ < 12 * 60 + 40;
    if (minute_ < person.shiftEnd) {
        if (lunchWindow && person.hunger > 48.0f && person.cash >= 8.0f) {
            moveTo(person, cafeId_, Activity::Eating);
            if (person.lastCafeDay != day_) {
                person.cash -= 8.0f;
                person.hunger = std::max(0.0f, person.hunger - 38.0f);
                person.lastCafeDay = day_;
                emit(person.id, "purchase", person.name + " bought lunch at Blue Cup Cafe", 0.08f);
            }
        } else {
            moveTo(person, person.workplaceId, Activity::Working);
        }
        return;
    }

    if (minute_ < 20 * 60) {
        if (person.eveningPlanDay != day_) {
            person.eveningPlaceId = chooseEveningPlace(person);
            person.eveningPlanDay = day_;
        }

        if (person.eveningPlaceId == parkId_) {
            moveTo(person, parkId_, Activity::Relaxing);
        } else if (person.eveningPlaceId == cafeId_) {
            moveTo(person, cafeId_, Activity::Socializing);
            if (person.lastCafeDay != day_ && person.cash >= 6.0f) {
                person.cash -= 6.0f;
                person.hunger = std::max(0.0f, person.hunger - 20.0f);
                person.loneliness = std::max(0.0f, person.loneliness - 12.0f);
                person.lastCafeDay = day_;
            }
        } else if (person.eveningPlaceId == shopId_) {
            moveTo(person, shopId_, Activity::Shopping);
            if (person.lastPurchaseDay != day_ && person.cash >= 12.0f) {
                const float spend = 12.0f + random01() * 18.0f;
                person.cash = std::max(0.0f, person.cash - spend);
                person.hunger = std::max(0.0f, person.hunger - 15.0f);
                person.lastPurchaseDay = day_;
                emit(person.id, "purchase", person.name + " stopped at Mason Convenience", 0.06f);
            }
        } else {
            moveTo(person, person.homeId, Activity::AtHome);
        }
        return;
    }

    moveTo(person, person.homeId, minute_ >= 23 * 60 ? Activity::Sleeping : Activity::AtHome);
}

void World::handleEconomy(Person& person) {
    if (minute_ == person.shiftEnd && !person.paidToday) {
        person.cash += person.dailyWage;
        person.paidToday = true;
        emit(person.id, "income", person.name + " finished a shift and was paid", 0.12f);
    }

    if (minute_ == 21 * 60) {
        if (person.cash >= person.housingCostPerDay) {
            person.cash -= person.housingCostPerDay;
        } else {
            person.stress = clamp100(person.stress + 10.0f);
            person.memories.push_back(Memory{day_, minute_, "money_pressure", -1, 0.55f});
            emit(person.id, "money_pressure", person.name + " could not fully cover today's living costs", 0.55f);
        }
    }
}

int World::chooseEveningPlace(Person& person) {
    if (person.energy < 25.0f) {
        return person.homeId;
    }
    if (person.stress > 68.0f) {
        return parkId_;
    }
    if (person.loneliness > 58.0f && person.cash >= 6.0f) {
        return cafeId_;
    }
    if (person.hunger > 62.0f && person.cash >= 12.0f) {
        return shopId_;
    }

    const float roll = random01();
    const float socialBias = person.traits.sociability * 0.30f;
    if (roll < 0.18f + socialBias && person.cash >= 6.0f) {
        return cafeId_;
    }
    if (roll < 0.45f) {
        return parkId_;
    }
    if (roll < 0.60f && person.cash >= 12.0f) {
        return shopId_;
    }
    return person.homeId;
}

void World::handleInteractions() {
    for (std::size_t i = 0; i < citizens_.size(); ++i) {
        for (std::size_t j = i + 1; j < citizens_.size(); ++j) {
            auto& a = citizens_[i];
            auto& b = citizens_[j];
            if (a.currentPlaceId != b.currentPlaceId) {
                continue;
            }

            const auto* place = placeById(a.currentPlaceId);
            if (!place || place->type == PlaceType::Shop) {
                continue;
            }

            float contact = 0.0f;
            switch (place->type) {
                case PlaceType::Home: contact = 0.55f; break;
                case PlaceType::Workplace: contact = 0.22f; break;
                case PlaceType::Cafe: contact = 0.85f; break;
                case PlaceType::Park: contact = 0.65f; break;
                case PlaceType::Shop: break;
            }

            auto& ab = ensureRelationship(a, b.id);
            auto& ba = ensureRelationship(b, a.id);
            const float oldFamiliarity = ab.familiarity;

            ab.familiarity = clamp100(ab.familiarity + contact);
            ba.familiarity = ab.familiarity;

            const float compatibility = ((a.traits.kindness + b.traits.kindness) * 0.5f - 0.35f) * 0.18f;
            const float noise = (random01() - 0.5f) * 0.12f;
            ab.affinity = clamp100(ab.affinity + compatibility + noise);
            ba.affinity = ab.affinity;

            a.loneliness = clamp100(a.loneliness - contact * 0.35f);
            b.loneliness = clamp100(b.loneliness - contact * 0.35f);

            if (oldFamiliarity < 20.0f && ab.familiarity >= 20.0f) {
                emit(a.id, "relationship", a.name + " and " + b.name + " are becoming familiar", 0.18f);
            }
            if (oldFamiliarity < 55.0f && ab.familiarity >= 55.0f && ab.affinity > 45.0f) {
                a.memories.push_back(Memory{day_, minute_, "friendship", b.id, 0.45f});
                b.memories.push_back(Memory{day_, minute_, "friendship", a.id, 0.45f});
                emit(a.id, "friendship", a.name + " and " + b.name + " have become friends", 0.42f);
            }
        }
    }
}

void World::moveTo(Person& person, int placeId, Activity activity, const std::string& reason) {
    const bool changedPlace = person.currentPlaceId != placeId;
    person.currentPlaceId = placeId;
    person.activity = activity;

    if (changedPlace && !reason.empty()) {
        emit(person.id, "routine", person.name + " " + reason, 0.03f);
    }
}

Place* World::placeById(int id) {
    for (auto& place : places_) {
        if (place.id == id) {
            return &place;
        }
    }
    return nullptr;
}

const Place* World::placeById(int id) const {
    for (const auto& place : places_) {
        if (place.id == id) {
            return &place;
        }
    }
    return nullptr;
}

void World::emit(int personId, std::string type, std::string text, float importance) {
    events_.push_back(Event{day_, minute_, personId, std::move(type), std::move(text), importance});
}

float World::random01() {
    return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng_);
}

int World::randomInt(int minInclusive, int maxInclusive) {
    return std::uniform_int_distribution<int>(minInclusive, maxInclusive)(rng_);
}

std::string World::clockLabel() const {
    std::ostringstream out;
    out << "Day " << day_ << ' '
        << std::setfill('0') << std::setw(2) << minute_ / 60
        << ':' << std::setw(2) << minute_ % 60;
    return out.str();
}

std::string World::dailySummary() const {
    if (citizens_.empty()) {
        return "No citizens";
    }

    float totalCash = 0.0f;
    float totalStress = 0.0f;
    float totalHunger = 0.0f;
    int moneyPressure = 0;
    int relationships = 0;

    for (const auto& citizen : citizens_) {
        totalCash += citizen.cash;
        totalStress += citizen.stress;
        totalHunger += citizen.hunger;
        if (citizen.cash < 25.0f) {
            ++moneyPressure;
        }
        relationships += static_cast<int>(citizen.relationships.size());
    }

    const float count = static_cast<float>(citizens_.size());
    std::ostringstream out;
    out << std::fixed << std::setprecision(1)
        << "population=" << citizens_.size()
        << " avg_cash=$" << totalCash / count
        << " avg_stress=" << totalStress / count
        << " avg_hunger=" << totalHunger / count
        << " low_cash=" << moneyPressure
        << " social_links=" << relationships / 2;
    return out.str();
}

} // namespace sim
