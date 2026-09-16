#pragma once

#include "sim/World.hpp"

namespace view {

class NeighborhoodRenderer {
public:
    explicit NeighborhoodRenderer(sim::World& world);
    int run();

private:
    sim::World& world_;
};

} // namespace view
