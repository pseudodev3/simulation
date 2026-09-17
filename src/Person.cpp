#include "sim/Person.hpp"

namespace sim {

Relationship* findRelationship(Person& person, int otherPersonId) {
    for (auto& relationship : person.relationships) {
        if (relationship.otherPersonId == otherPersonId) return &relationship;
    }
    return nullptr;
}

const Relationship* findRelationship(const Person& person, int otherPersonId) {
    for (const auto& relationship : person.relationships) {
        if (relationship.otherPersonId == otherPersonId) return &relationship;
    }
    return nullptr;
}

Relationship& ensureRelationship(Person& person, int otherPersonId) {
    if (auto* existing = findRelationship(person, otherPersonId)) return *existing;
    Relationship relationship;
    relationship.otherPersonId = otherPersonId;
    person.relationships.push_back(relationship);
    return person.relationships.back();
}

} // namespace sim
