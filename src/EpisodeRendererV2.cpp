#include "view/EpisodeRenderer.hpp"

#include "raylib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace view {
namespace {

constexpr int kCanvasWidth = 640;
constexpr int kCanvasHeight = 360;
constexpr float kPi = 3.14159265358979323846f;

Color rgb(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
    return Color{r, g, b, a};
}

const sim::Place* placeById(const sim::EpisodePlan& plan, int id) {
    for (const auto& place : plan.places) {
        if (place.id == id) return &place;
    }
    return nullptr;
}

const sim::CitizenSnapshot* citizenById(const sim::EpisodeStep& step, int id) {
    for (const auto& citizen : step.citizens) {
        if (citizen.id == id) return &citizen;
    }
    return nullptr;
}

Vector2 lerp(Vector2 a, Vector2 b, float t) {
    return Vector2{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

float smoothstep(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float length(Vector2 a, Vector2 b) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

Color shirtColor(int id) {
    static constexpr std::array<Color, 10> palette = {
        Color{170, 92, 82, 255}, Color{78, 115, 145, 255}, Color{176, 131, 73, 255},
        Color{103, 132, 88, 255}, Color{121, 90, 139, 255}, Color{172, 108, 137, 255},
        Color{78, 139, 132, 255}, Color{146, 108, 72, 255}, Color{148, 80, 105, 255},
        Color{92, 112, 150, 255}
    };
    return palette[static_cast<std::size_t>(std::abs(id)) % palette.size()];
}

bool indoor(sim::PlaceType type) {
    return type == sim::PlaceType::Home || type == sim::PlaceType::Workplace
        || type == sim::PlaceType::Cafe || type == sim::PlaceType::Shop;
}

float halfHeight(sim::PlaceType type) {
    switch (type) {
        case sim::PlaceType::Home: return 22.0f;
        case sim::PlaceType::Workplace: return 21.0f;
        case sim::PlaceType::Cafe: return 21.0f;
        case sim::PlaceType::Shop: return 22.0f;
        case sim::PlaceType::Park: return 0.0f;
    }
    return 20.0f;
}

Vector2 doorPosition(const sim::Place& place) {
    if (place.type == sim::PlaceType::Park) return place.position;
    const bool north = place.position.y < 190.0f;
    return Vector2{place.position.x, place.position.y + (north ? 1.0f : -1.0f) * (halfHeight(place.type) + 3.0f)};
}

Vector2 sidewalkPoint(const sim::Place& place, int citizenId) {
    const float laneOffset = static_cast<float>((citizenId % 3) - 1) * 1.8f;
    const bool north = place.position.y < 190.0f;
    return Vector2{place.position.x, (north ? 186.0f : 239.0f) + laneOffset};
}

struct Route {
    std::vector<Vector2> points;
    bool crossesRoad{false};
};

Route makeRoute(const sim::EpisodePlan& plan, int originId, int destinationId, int citizenId) {
    const auto* origin = placeById(plan, originId);
    const auto* destination = placeById(plan, destinationId);
    if (!origin || !destination) return Route{{Vector2{320.0f, 180.0f}}, false};

    const Vector2 start = doorPosition(*origin);
    const Vector2 finish = doorPosition(*destination);
    const Vector2 startWalk = sidewalkPoint(*origin, citizenId);
    const Vector2 finishWalk = sidewalkPoint(*destination, citizenId);
    const bool startNorth = startWalk.y < 210.0f;
    const bool finishNorth = finishWalk.y < 210.0f;

    Route route;
    route.crossesRoad = startNorth != finishNorth;
    route.points.push_back(start);
    route.points.push_back(startWalk);

    if (route.crossesRoad) {
        // Everyone uses the marked crossing at the center instead of cutting through traffic.
        const float crossingX = 348.0f + static_cast<float>((citizenId % 3) - 1) * 2.0f;
        route.points.push_back(Vector2{crossingX, startWalk.y});
        route.points.push_back(Vector2{crossingX, finishWalk.y});
        route.points.push_back(finishWalk);
    } else {
        // Stay on the same pavement lane. This keeps people from walking through lawns/buildings.
        route.points.push_back(Vector2{finishWalk.x, startWalk.y});
    }

    route.points.push_back(finish);
    return route;
}

float crossingPause(float progress, bool crossesRoad, int citizenId) {
    if (!crossesRoad) return progress;

    // Deterministic short wait before stepping into the road. It prevents the whole block
    // from looking like synchronized sprites gliding through an intersection.
    const float waitStart = 0.38f + static_cast<float>(citizenId % 3) * 0.012f;
    const float waitEnd = waitStart + 0.085f;
    if (progress <= waitStart) return progress;
    if (progress < waitEnd) return waitStart;

    const float remainingSource = 1.0f - waitEnd;
    const float remainingTarget = 1.0f - waitStart;
    return waitStart + (progress - waitEnd) / remainingSource * remainingTarget;
}

Vector2 pointOnRoute(const Route& route, float progress) {
    if (route.points.empty()) return Vector2{320.0f, 180.0f};
    if (route.points.size() == 1) return route.points.front();

    progress = std::clamp(progress, 0.0f, 1.0f);
    float total = 0.0f;
    for (std::size_t i = 1; i < route.points.size(); ++i) {
        total += length(route.points[i - 1], route.points[i]);
    }
    if (total <= 0.001f) return route.points.back();

    float cursor = progress * total;
    for (std::size_t i = 1; i < route.points.size(); ++i) {
        const float segment = length(route.points[i - 1], route.points[i]);
        if (cursor <= segment || i + 1 == route.points.size()) {
            const float t = segment > 0.001f ? std::clamp(cursor / segment, 0.0f, 1.0f) : 1.0f;
            return lerp(route.points[i - 1], route.points[i], t);
        }
        cursor -= segment;
    }
    return route.points.back();
}

Vector2 stationaryPosition(const sim::EpisodePlan& plan, const sim::CitizenSnapshot& citizen) {
    const auto* place = placeById(plan, citizen.currentPlaceId);
    if (!place) return Vector2{320.0f, 180.0f};

    const float angle = static_cast<float>((citizen.id * 97) % 360) * kPi / 180.0f;
    if (place->type == sim::PlaceType::Park) {
        return Vector2{place->position.x + std::cos(angle) * (13.0f + static_cast<float>(citizen.id % 9)),
                       place->position.y + std::sin(angle) * (9.0f + static_cast<float>(citizen.id % 7))};
    }

    return Vector2{place->position.x + std::cos(angle) * 8.0f,
                   place->position.y + 4.0f + std::sin(angle) * 5.0f};
}

Vector2 snapshotPosition(const sim::EpisodePlan& plan, const sim::CitizenSnapshot& citizen) {
    if (citizen.activity == sim::Activity::Commuting
        && citizen.originPlaceId >= 0 && citizen.destinationPlaceId >= 0) {
        const Route route = makeRoute(plan, citizen.originPlaceId, citizen.destinationPlaceId, citizen.id);
        return pointOnRoute(route, crossingPause(citizen.travelProgress, route.crossesRoad, citizen.id));
    }
    return stationaryPosition(plan, citizen);
}

Vector2 interpolatedPosition(const sim::EpisodePlan& plan,
                             const sim::CitizenSnapshot& current,
                             const sim::CitizenSnapshot* next,
                             float t) {
    if (!next) return snapshotPosition(plan, current);

    if (current.activity == sim::Activity::Commuting
        && current.originPlaceId >= 0 && current.destinationPlaceId >= 0) {
        float endProgress = current.travelProgress;
        if (next->activity == sim::Activity::Commuting
            && next->originPlaceId == current.originPlaceId
            && next->destinationPlaceId == current.destinationPlaceId) {
            endProgress = next->travelProgress;
        } else if (next->currentPlaceId == current.destinationPlaceId) {
            endProgress = 1.0f;
        }
        const Route route = makeRoute(plan, current.originPlaceId, current.destinationPlaceId, current.id);
        const float raw = current.travelProgress + (endProgress - current.travelProgress) * t;
        return pointOnRoute(route, crossingPause(raw, route.crossesRoad, current.id));
    }

    if (next->activity == sim::Activity::Commuting
        && next->originPlaceId >= 0 && next->destinationPlaceId >= 0) {
        const Route route = makeRoute(plan, next->originPlaceId, next->destinationPlaceId, next->id);
        return pointOnRoute(route, crossingPause(next->travelProgress * t, route.crossesRoad, next->id));
    }

    return lerp(snapshotPosition(plan, current), snapshotPosition(plan, *next), t);
}

bool placeOccupied(const sim::EpisodeStep& step, int placeId) {
    for (const auto& citizen : step.citizens) {
        if (citizen.currentPlaceId == placeId) return true;
    }
    return false;
}

void drawTree(int x, int y, float scale = 1.0f) {
    DrawRectangle(x - static_cast<int>(2 * scale), y, static_cast<int>(4 * scale), static_cast<int>(8 * scale), rgb(83, 64, 44));
    DrawCircle(x, y - static_cast<int>(4 * scale), 6.5f * scale, rgb(66, 102, 59));
    DrawCircle(x - static_cast<int>(4 * scale), y, 4.2f * scale, rgb(73, 112, 65));
    DrawCircle(x + static_cast<int>(4 * scale), y, 4.2f * scale, rgb(73, 112, 65));
}

void drawStreetLight(int x, int y, bool on) {
    DrawRectangle(x, y - 11, 1, 12, rgb(73, 72, 68));
    DrawRectangle(x - 2, y - 12, 5, 2, rgb(59, 59, 57));
    if (on) {
        DrawCircle(x, y - 11, 6.0f, Fade(rgb(255, 220, 143), 0.16f));
        DrawCircle(x, y - 11, 2.0f, rgb(255, 226, 154));
    } else {
        DrawCircle(x, y - 11, 2.0f, rgb(133, 131, 119));
    }
}

void drawHome(const sim::EpisodeStep& step, const sim::Place& place, bool night) {
    const int x = static_cast<int>(place.position.x);
    const int y = static_cast<int>(place.position.y);
    const bool occupied = placeOccupied(step, place.id);
    DrawRectangle(x - 27, y - 22, 54, 45, rgb(90, 116, 75));
    DrawRectangle(x - 21, y - 13, 42, 28, rgb(204, 184, 151));
    DrawTriangle(Vector2{static_cast<float>(x - 25), static_cast<float>(y - 10)},
                 Vector2{static_cast<float>(x), static_cast<float>(y - 25)},
                 Vector2{static_cast<float>(x + 25), static_cast<float>(y - 10)}, rgb(111, 71, 59));
    DrawRectangle(x - 3, y + 2, 7, 13, rgb(91, 68, 54));
    const Color window = (night && occupied) ? rgb(246, 207, 126) : rgb(102, 133, 139);
    DrawRectangle(x - 16, y - 4, 7, 7, window);
    DrawRectangle(x + 9, y - 4, 7, 7, window);
    DrawRectangle(x - 2, y + 15, 4, 7, rgb(152, 139, 112));
}

void drawWorkplace(const sim::EpisodeStep& step, const sim::Place& place, bool night) {
    const int x = static_cast<int>(place.position.x);
    const int y = static_cast<int>(place.position.y);
    const bool occupied = placeOccupied(step, place.id);
    const Color window = (night && occupied) ? rgb(231, 202, 132) : rgb(104, 135, 143);
    DrawRectangle(x - 31, y - 18, 62, 37, rgb(169, 163, 147));
    DrawRectangle(x - 33, y - 21, 66, 5, rgb(92, 89, 81));
    DrawRectangle(x - 24, y - 10, 13, 10, window);
    DrawRectangle(x - 6, y - 10, 13, 10, window);
    DrawRectangle(x + 12, y - 10, 13, 10, window);
    DrawRectangle(x - 4, y + 4, 9, 15, rgb(84, 78, 69));
}

void drawCafe(const sim::EpisodeStep& step, const sim::Place& place, bool night) {
    const int x = static_cast<int>(place.position.x);
    const int y = static_cast<int>(place.position.y);
    const bool occupied = placeOccupied(step, place.id);
    const Color window = (night && occupied) ? rgb(245, 200, 122) : rgb(116, 148, 151);
    DrawRectangle(x - 26, y - 17, 52, 35, rgb(182, 154, 126));
    DrawRectangle(x - 28, y - 21, 56, 6, rgb(80, 106, 119));
    DrawRectangle(x - 17, y - 8, 13, 12, window);
    DrawRectangle(x + 4, y - 8, 13, 12, window);
    DrawRectangle(x - 3, y + 5, 7, 13, rgb(82, 63, 53));
    DrawText("CAFE", x - 14, y - 18, 6, rgb(237, 231, 210));
}

void drawShop(const sim::EpisodeStep& step, const sim::Place& place, bool night) {
    const int x = static_cast<int>(place.position.x);
    const int y = static_cast<int>(place.position.y);
    const bool occupied = placeOccupied(step, place.id);
    const Color window = (night && occupied) ? rgb(245, 205, 128) : rgb(111, 146, 142);
    DrawRectangle(x - 27, y - 18, 54, 37, rgb(191, 177, 143));
    for (int i = 0; i < 6; ++i) {
        DrawRectangle(x - 27 + i * 9, y - 22, 5, 5,
                      (i % 2 == 0) ? rgb(134, 74, 61) : rgb(226, 214, 181));
    }
    DrawRectangle(x - 18, y - 8, 16, 13, window);
    DrawRectangle(x + 7, y - 8, 12, 27, rgb(92, 71, 55));
    DrawText("SHOP", x - 17, y - 16, 6, rgb(76, 69, 61));
}

void drawPark(const sim::Place& place) {
    const int x = static_cast<int>(place.position.x);
    const int y = static_cast<int>(place.position.y);
    DrawRectangle(x - 47, y - 29, 94, 58, rgb(88, 125, 76));
    DrawRectangle(x - 43, y - 3, 86, 6, rgb(164, 149, 117));
    DrawRectangle(x - 3, y - 25, 6, 50, rgb(164, 149, 117));
    drawTree(x - 30, y - 16, 0.85f);
    drawTree(x + 29, y + 17, 0.8f);
    drawTree(x + 31, y - 17, 0.72f);
    DrawRectangle(x - 19, y + 11, 17, 2, rgb(91, 67, 49));
}

void drawRoads() {
    const Color asphalt = rgb(73, 76, 76);
    const Color sidewalk = rgb(158, 157, 145);
    const Color lane = rgb(188, 176, 130);

    DrawRectangle(0, 181, kCanvasWidth, 62, sidewalk);
    DrawRectangle(0, 196, kCanvasWidth, 33, asphalt);
    for (int x = 8; x < kCanvasWidth; x += 28) DrawRectangle(x, 212, 14, 2, lane);

    DrawRectangle(318, 0, 60, kCanvasHeight, sidewalk);
    DrawRectangle(331, 0, 34, kCanvasHeight, asphalt);
    for (int y = 6; y < kCanvasHeight; y += 27) DrawRectangle(347, y, 2, 13, lane);

    for (int x = 334; x <= 360; x += 6) {
        DrawRectangle(x, 198, 3, 7, rgb(203, 202, 186));
        DrawRectangle(x, 220, 3, 7, rgb(203, 202, 186));
    }
    for (int y = 199; y <= 224; y += 6) {
        DrawRectangle(326, y, 6, 3, rgb(203, 202, 186));
        DrawRectangle(364, y, 6, 3, rgb(203, 202, 186));
    }
}

bool isNight(int minute) {
    return minute >= 19 * 60 || minute < 6 * 60;
}

float nightAlpha(int minute) {
    if (minute >= 20 * 60 || minute < 5 * 60) return 0.49f;
    if (minute >= 18 * 60) return 0.49f * static_cast<float>(minute - 18 * 60) / 120.0f;
    if (minute < 7 * 60) return 0.49f * static_cast<float>(7 * 60 - minute) / 120.0f;
    return 0.0f;
}

void drawCutaway(const sim::Place& place) {
    if (!indoor(place.type)) return;
    const int x = static_cast<int>(place.position.x);
    const int y = static_cast<int>(place.position.y);
    DrawRectangle(x - 20, y - 10, 40, 23, rgb(93, 82, 69));
    DrawRectangleLines(x - 20, y - 10, 40, 23, rgb(222, 197, 153));
    if (place.type == sim::PlaceType::Cafe) {
        DrawCircle(x, y + 2, 4.0f, rgb(119, 88, 62));
        DrawRectangle(x - 13, y + 8, 8, 2, rgb(77, 67, 57));
        DrawRectangle(x + 5, y + 8, 8, 2, rgb(77, 67, 57));
    } else if (place.type == sim::PlaceType::Home) {
        DrawRectangle(x - 14, y + 5, 10, 5, rgb(128, 105, 79));
        DrawRectangle(x + 5, y - 5, 9, 9, rgb(77, 89, 86));
    } else {
        DrawRectangle(x - 14, y + 6, 28, 3, rgb(119, 105, 83));
    }
}

int focusedPlace(const sim::EpisodeStep& step, const sim::DirectedShot& shot) {
    if (shot.placeId >= 0) return shot.placeId;
    if (shot.personId >= 0) {
        if (const auto* citizen = citizenById(step, shot.personId)) {
            if (citizen->currentPlaceId >= 0) return citizen->currentPlaceId;
            return citizen->destinationPlaceId;
        }
    }
    return -1;
}

void drawScene(const sim::EpisodePlan& plan, const sim::EpisodeStep& step, int focusPlaceId) {
    ClearBackground(rgb(101, 123, 82));
    for (int x = 0; x < kCanvasWidth; x += 16) {
        for (int y = 0; y < kCanvasHeight; y += 16) {
            if (((x / 16) + (y / 16)) % 3 == 0) DrawRectangle(x + 2, y + 4, 1, 2, rgb(89, 113, 74));
        }
    }

    drawRoads();
    const std::array<Vector2, 12> trees = {
        Vector2{14, 18}, Vector2{18, 173}, Vector2{305, 171}, Vector2{302, 338},
        Vector2{385, 174}, Vector2{458, 174}, Vector2{545, 174}, Vector2{620, 174},
        Vector2{385, 340}, Vector2{458, 340}, Vector2{535, 340}, Vector2{620, 340}
    };
    for (const auto& tree : trees) drawTree(static_cast<int>(tree.x), static_cast<int>(tree.y), 0.75f);

    const bool night = isNight(step.minute);
    for (const auto& place : plan.places) {
        switch (place.type) {
            case sim::PlaceType::Home: drawHome(step, place, night); break;
            case sim::PlaceType::Workplace: drawWorkplace(step, place, night); break;
            case sim::PlaceType::Cafe: drawCafe(step, place, night); break;
            case sim::PlaceType::Shop: drawShop(step, place, night); break;
            case sim::PlaceType::Park: drawPark(place); break;
        }
        if (place.id == focusPlaceId) drawCutaway(place);
    }

    drawStreetLight(45, 193, night);
    drawStreetLight(168, 193, night);
    drawStreetLight(286, 193, night);
    drawStreetLight(410, 193, night);
    drawStreetLight(532, 193, night);
    drawStreetLight(328, 70, night);
    drawStreetLight(328, 285, night);
}

bool shouldDrawCitizen(const sim::EpisodePlan& plan,
                       const sim::CitizenSnapshot& citizen,
                       const sim::DirectedShot& shot) {
    if (citizen.activity == sim::Activity::Sleeping) return false;
    if (citizen.activity == sim::Activity::Commuting) return true;
    if (citizen.id == shot.personId || citizen.id == shot.secondaryPersonId) return true;
    const auto* place = placeById(plan, citizen.currentPlaceId);
    return place && place->type == sim::PlaceType::Park;
}

void drawCitizen(const sim::CitizenSnapshot& citizen,
                 Vector2 position,
                 float phase,
                 bool walking,
                 int facing,
                 bool gesturing) {
    const int x = static_cast<int>(position.x);
    const float bobAmount = walking ? 1.0f : 0.25f;
    const int y = static_cast<int>(position.y + std::sin(phase) * bobAmount);

    DrawCircle(x, y + 4, 4.0f, Fade(BLACK, 0.22f));
    DrawRectangle(x - 2, y - 1, 5, 7, shirtColor(citizen.id));
    DrawCircle(x, y - 3, 3.0f, rgb(211, 172, 135));

    const int legSwing = walking && std::sin(phase * 1.7f) > 0.0f ? 1 : 0;
    DrawRectangle(x - 2 - legSwing, y + 6, 2, 3, rgb(58, 62, 66));
    DrawRectangle(x + 1 + legSwing, y + 6, 2, 3, rgb(58, 62, 66));

    if (gesturing) {
        const int handX = x + facing * 5;
        const int handY = y + static_cast<int>(std::sin(phase * 0.55f) * 2.0f);
        DrawLine(x + facing * 2, y + 1, handX, handY, rgb(211, 172, 135));
        DrawCircle(handX, handY, 1.2f, rgb(211, 172, 135));
    }
}

void drawNameTag(const std::string& name, Vector2 position) {
    const int size = 5;
    const int width = MeasureText(name.c_str(), size);
    const int x = static_cast<int>(position.x) - width / 2;
    const int y = static_cast<int>(position.y) - 16;
    DrawRectangle(x - 2, y - 1, width + 4, 7, Fade(rgb(25, 27, 28), 0.82f));
    DrawText(name.c_str(), x, y, size, rgb(238, 234, 216));
}

void drawDayNightOverlay(int minute) {
    const float alpha = std::clamp(nightAlpha(minute), 0.0f, 0.52f);
    if (alpha > 0.0f) DrawRectangle(0, 0, kCanvasWidth, kCanvasHeight, Fade(rgb(27, 39, 66), alpha));

    if (minute >= 5 * 60 && minute < 8 * 60) {
        const float center = 6.5f * 60.0f;
        const float dawn = 1.0f - std::abs((static_cast<float>(minute) - center) / 90.0f);
        if (dawn > 0.0f) DrawRectangle(0, 0, kCanvasWidth, kCanvasHeight, Fade(rgb(222, 143, 91), dawn * 0.08f));
    }
}

std::string clockText(const sim::EpisodeStep& step) {
    std::ostringstream out;
    out << "DAY " << step.day << "  " << std::setfill('0') << std::setw(2) << step.minute / 60
        << ':' << std::setw(2) << step.minute % 60;
    return out.str();
}

std::string shorten(const std::string& text, int maxWidth, int fontSize) {
    std::string result = text;
    while (MeasureText(result.c_str(), fontSize) > maxWidth && result.size() > 8) result.pop_back();
    if (result != text && result.size() > 3) result.replace(result.size() - 3, 3, "...");
    return result;
}

void drawInteractionOverlay(const sim::EpisodeStep& step,
                            const sim::DirectedShot& shot,
                            float shotProgress) {
    const auto* primary = citizenById(step, shot.personId);
    const auto* secondary = citizenById(step, shot.secondaryPersonId);
    if (!primary || !secondary) return;

    DrawRectangle(0, 0, kCanvasWidth, 12, Fade(BLACK, 0.60f));
    DrawRectangle(0, kCanvasHeight - 17, kCanvasWidth, 17, Fade(BLACK, 0.68f));

    std::string speaker;
    std::string line;
    if (shotProgress < 0.48f) {
        speaker = primary->name;
        line = shot.dialoguePrimary;
    } else {
        speaker = secondary->name;
        line = shot.dialogueSecondary;
    }

    if (!line.empty()) {
        line = shorten(line, 450, 8);
        const int lineWidth = MeasureText(line.c_str(), 8);
        const int boxWidth = std::max(220, lineWidth + 26);
        const int boxX = (kCanvasWidth - boxWidth) / 2;
        DrawRectangleRounded(Rectangle{static_cast<float>(boxX), 294.0f, static_cast<float>(boxWidth), 42.0f},
                             0.18f, 4, Fade(rgb(22, 24, 25), 0.92f));
        DrawText(speaker.c_str(), boxX + 12, 301, 6, rgb(178, 188, 178));
        DrawText(line.c_str(), boxX + 12, 315, 8, rgb(243, 239, 221));
    }
}

void drawHud(const sim::EpisodeStep& step, const sim::DirectedShot& shot, float shotProgress) {
    DrawRectangle(8, 8, 122, 25, Fade(rgb(27, 30, 30), 0.86f));
    DrawText("MASON BLOCK", 14, 13, 9, rgb(241, 233, 202));
    const std::string time = clockText(step);
    DrawText(time.c_str(), 14, 23, 6, rgb(181, 185, 178));

    if (shot.secondaryPersonId >= 0 && (!shot.dialoguePrimary.empty() || !shot.dialogueSecondary.empty())) {
        drawInteractionOverlay(step, shot, shotProgress);
        return;
    }

    if (!shot.caption.empty()) {
        const std::string caption = shorten(shot.caption, 430, 9);
        const int width = MeasureText(caption.c_str(), 9) + 18;
        DrawRectangle((kCanvasWidth - width) / 2, 319, width, 23, Fade(rgb(24, 26, 27), 0.88f));
        DrawText(caption.c_str(), (kCanvasWidth - MeasureText(caption.c_str(), 9)) / 2, 326, 9, rgb(242, 237, 216));
    }
}

Vector2 desiredCameraTarget(const sim::EpisodePlan& plan,
                            const sim::EpisodeStep& current,
                            const sim::EpisodeStep& next,
                            const sim::DirectedShot& shot,
                            float t) {
    if (shot.kind == sim::ShotKind::Person && shot.personId >= 0) {
        const auto* a = citizenById(current, shot.personId);
        const auto* aNext = citizenById(next, shot.personId);
        if (a) {
            Vector2 target = interpolatedPosition(plan, *a, aNext, t);
            if (shot.secondaryPersonId >= 0) {
                const auto* b = citizenById(current, shot.secondaryPersonId);
                const auto* bNext = citizenById(next, shot.secondaryPersonId);
                if (b) {
                    const Vector2 second = interpolatedPosition(plan, *b, bNext, t);
                    target = Vector2{(target.x + second.x) * 0.5f, (target.y + second.y) * 0.5f};
                }
            }
            return target;
        }
    }

    if (shot.kind == sim::ShotKind::Place && shot.placeId >= 0) {
        if (const auto* place = placeById(plan, shot.placeId)) return place->position;
    }
    return Vector2{320.0f, 180.0f};
}

float desiredZoom(const sim::DirectedShot& shot) {
    if (shot.secondaryPersonId >= 0) return 2.15f;
    switch (shot.kind) {
        case sim::ShotKind::Person: return 1.72f;
        case sim::ShotKind::Place: return 1.38f;
        case sim::ShotKind::Establishing: return 1.0f;
    }
    return 1.0f;
}

Vector2 clampCamera(Vector2 target, float zoom) {
    const float halfW = 320.0f / zoom;
    const float halfH = 180.0f / zoom;
    target.x = std::clamp(target.x, halfW, 640.0f - halfW);
    target.y = std::clamp(target.y, halfH, 360.0f - halfH);
    return target;
}

std::string frameName(const std::filesystem::path& framesDirectory, int index) {
    std::ostringstream out;
    out << "frame_" << std::setfill('0') << std::setw(6) << index << ".png";
    return (framesDirectory / out.str()).string();
}

std::string shellQuote(const std::string& value) {
#ifdef _WIN32
    std::string escaped = value;
    std::string::size_type pos = 0;
    while ((pos = escaped.find('"', pos)) != std::string::npos) {
        escaped.insert(pos, "\\");
        pos += 2;
    }
    return '"' + escaped + '"';
#else
    std::string escaped = "'";
    for (const char ch : value) {
        if (ch == '\'') escaped += "'\\''";
        else escaped += ch;
    }
    escaped += '\'';
    return escaped;
#endif
}

bool exportCanvas(RenderTexture2D canvas, const std::string& path) {
    Image image = LoadImageFromTexture(canvas.texture);
    ImageFlipVertical(&image);
    const bool ok = ExportImage(image, path.c_str());
    UnloadImage(image);
    return ok;
}

} // namespace

bool EpisodeRenderer::render(const sim::EpisodePlan& plan,
                             const EpisodeRenderOptions& rawOptions,
                             std::string* errorMessage) {
    if (plan.steps.empty() || plan.shots.empty()) {
        if (errorMessage) *errorMessage = "Episode plan is empty";
        return false;
    }

    EpisodeRenderOptions options = rawOptions;
    options.renderFps = std::clamp(options.renderFps, 1, 60);
    options.outputFps = std::clamp(options.outputFps, 1, 60);
    options.width = std::max(320, options.width);
    options.height = std::max(180, options.height);

    std::filesystem::create_directories(options.outputDirectory);
    const auto framesDirectory = options.outputDirectory / "frames";
    std::filesystem::remove_all(framesDirectory);
    std::filesystem::create_directories(framesDirectory);

    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNDECORATED);
    InitWindow(kCanvasWidth, kCanvasHeight, "Mason Block episode renderer v2");
    RenderTexture2D canvas = LoadRenderTexture(kCanvasWidth, kCanvasHeight);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);

    Camera2D camera{};
    camera.offset = Vector2{320.0f, 180.0f};
    camera.target = Vector2{320.0f, 180.0f};
    camera.zoom = 1.0f;

    int frameIndex = 0;
    float elapsedVideo = 0.0f;
    bool exportFailed = false;

    for (const auto& shot : plan.shots) {
        if (options.maxVideoSeconds > 0.0f && elapsedVideo >= options.maxVideoSeconds) break;

        const auto& current = plan.steps[shot.stepIndex];
        const auto& next = plan.steps[std::min(shot.stepIndex + 1, plan.steps.size() - 1)];
        const int frames = std::max(1, static_cast<int>(std::round(shot.seconds * static_cast<float>(options.renderFps))));

        for (int localFrame = 0; localFrame < frames; ++localFrame) {
            if (options.maxVideoSeconds > 0.0f && elapsedVideo >= options.maxVideoSeconds) break;

            const float rawT = frames > 1 ? static_cast<float>(localFrame) / static_cast<float>(frames - 1) : 1.0f;
            const float t = smoothstep(rawT);
            Vector2 wantedTarget = desiredCameraTarget(plan, current, next, shot, t);
            const float wantedZoom = desiredZoom(shot);
            wantedTarget = clampCamera(wantedTarget, wantedZoom);
            camera.target = lerp(camera.target, wantedTarget, shot.secondaryPersonId >= 0 ? 0.055f : 0.075f);
            camera.zoom += (wantedZoom - camera.zoom) * (shot.secondaryPersonId >= 0 ? 0.055f : 0.075f);

            BeginTextureMode(canvas);
            BeginMode2D(camera);
            const int focusPlaceId = focusedPlace(current, shot);
            drawScene(plan, current, focusPlaceId);

            Vector2 primaryPosition{};
            Vector2 secondaryPosition{};
            bool havePrimary = false;
            bool haveSecondary = false;

            for (const auto& citizen : current.citizens) {
                if (!shouldDrawCitizen(plan, citizen, shot)) continue;

                const auto* nextCitizen = citizenById(next, citizen.id);
                Vector2 position = interpolatedPosition(plan, citizen, nextCitizen, t);
                bool walking = citizen.activity == sim::Activity::Commuting;
                int facing = 1;
                bool gesturing = false;

                if (shot.secondaryPersonId >= 0
                    && (citizen.id == shot.personId || citizen.id == shot.secondaryPersonId)
                    && citizen.activity != sim::Activity::Commuting) {
                    if (const auto* place = placeById(plan, focusPlaceId)) {
                        const bool primary = citizen.id == shot.personId;
                        position = Vector2{place->position.x + (primary ? -7.0f : 7.0f), place->position.y + 4.0f};
                        facing = primary ? 1 : -1;
                        gesturing = primary ? rawT < 0.48f : rawT >= 0.48f;
                    }
                }

                drawCitizen(citizen, position,
                            static_cast<float>(frameIndex) * (walking ? 0.36f : 0.12f) + static_cast<float>(citizen.id),
                            walking, facing, gesturing);

                if (citizen.id == shot.personId) {
                    primaryPosition = position;
                    havePrimary = true;
                }
                if (citizen.id == shot.secondaryPersonId) {
                    secondaryPosition = position;
                    haveSecondary = true;
                }
            }

            if (shot.secondaryPersonId >= 0 && havePrimary && haveSecondary) {
                if (const auto* a = citizenById(current, shot.personId)) drawNameTag(a->name, primaryPosition);
                if (const auto* b = citizenById(current, shot.secondaryPersonId)) drawNameTag(b->name, secondaryPosition);
            }

            drawDayNightOverlay(current.minute);
            EndMode2D();

            if (shot.secondaryPersonId >= 0) {
                // Important social scenes switch from pure map coverage to a cinematic close view.
                DrawRectangleGradientV(0, 0, kCanvasWidth, 52, Fade(BLACK, 0.22f), BLANK);
                DrawRectangleGradientV(0, kCanvasHeight - 70, kCanvasWidth, 70, BLANK, Fade(BLACK, 0.34f));
            }
            drawHud(current, shot, rawT);
            EndTextureMode();

            const std::string outputFrame = frameName(framesDirectory, frameIndex);
            if (!exportCanvas(canvas, outputFrame)) {
                exportFailed = true;
                break;
            }

            ++frameIndex;
            elapsedVideo += 1.0f / static_cast<float>(options.renderFps);
        }

        if (exportFailed) break;
    }

    UnloadRenderTexture(canvas);
    CloseWindow();

    if (exportFailed || frameIndex == 0) {
        if (errorMessage) *errorMessage = exportFailed ? "Failed while exporting PNG frames" : "No frames were rendered";
        return false;
    }

    const auto outputPath = options.outputDirectory / options.outputFileName;
    const std::string inputPattern = (framesDirectory / "frame_%06d.png").string();
    std::ostringstream command;
    command << "ffmpeg -y -loglevel error -framerate " << options.renderFps
            << " -i " << shellQuote(inputPattern)
            << " -vf " << shellQuote("scale=" + std::to_string(options.width) + ":" + std::to_string(options.height)
                                  + ":flags=neighbor,fps=" + std::to_string(options.outputFps))
            << " -c:v libx264 -preset medium -crf 18 -pix_fmt yuv420p -an -movflags +faststart "
            << shellQuote(outputPath.string());

    const int status = std::system(command.str().c_str());
    if (status != 0 || !std::filesystem::exists(outputPath)) {
        if (errorMessage) *errorMessage = "ffmpeg failed while encoding the silent picture track";
        return false;
    }

    if (!options.keepFrames) std::filesystem::remove_all(framesDirectory);
    return true;
}

} // namespace view
