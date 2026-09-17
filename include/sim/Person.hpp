#pragma once

#include "sim/Types.hpp"

#include <string>
#include <vector>

namespace sim {

struct Person {
    int id{};
    std::string name;
    int age{};

    int homeId{-1};
    int workplaceId{-1};
    int currentPlaceId{-1};
    int originPlaceId{-1};
    int destinationPlaceId{-1};
    Activity destinationActivity{Activity::AtHome};
    int travelMinutesTotal{0};
    int travelMinutesRemaining{0};

    int eveningPlaceId{-1};
    int eveningPlanDay{-1};

    Activity activity{Activity::AtHome};
    Traits traits{};
    BehaviorSignature behavior{};
    Intent intent{};

    // Perception / social state. These IDs are simulation truth, not renderer hints.
    std::vector<int> visiblePeople;
    int activeInteractionId{-1};
    int interactionCooldownUntil{-1};

    float cash{100.0f};
    float hunger{10.0f};
    float energy{85.0f};
    float stress{10.0f};
    float loneliness{20.0f};
    float jobSatisfaction{60.0f};

    int shiftStart{8 * 60};
    int shiftEnd{17 * 60};
    float dailyWage{75.0f};
    float housingCostPerDay{14.0f};

    int lastPurchaseDay{-1};
    int lastCafeDay{-1};
    bool paidToday{false};

    std::vector<Relationship> relationships;
    std::vector<Memory> memories;
};

Relationship* findRelationship(Person& person, int otherPersonId);
const Relationship* findRelationship(const Person& person, int otherPersonId);
Relationship& ensureRelationship(Person& person, int otherPersonId);

} // namespace sim
