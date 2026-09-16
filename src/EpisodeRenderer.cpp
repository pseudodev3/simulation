#include "view/EpisodeRenderer.hpp"

#include "raylib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace view {
namespace {

constexpr float kPi = 3.14159265358979323846f;

struct AudioSegment {
    float start{};
    float end{};
    int minute{};
    bool walking{};
    bool interaction{};
    float importance{};
};

Color rgb(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
    return Color{r, g, b, a};
}

const sim::Place* placeById(const sim::EpisodePlan& plan, int id) {
    for (const auto& place : plan.places) if (place.id == id) return &place;
    return nullptr;
}

const sim::CitizenSnapshot* citizenById(const sim::EpisodeStep& step, int id) {
    for (const auto& citizen : step.citizens) if (citizen.id == id) return &citizen;
    return nullptr;
}

Vector2 lerp(Vector2 a, Vector2 b, float t) {
    return Vector2{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

float smoothstep(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float distance(Vector2 a, Vector2 b) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

bool indoorPlace(sim::PlaceType type) {
    return type == sim::PlaceType::Home || type == sim::PlaceType::Workplace
        || type == sim::PlaceType::Cafe || type == sim::PlaceType::Shop;
}

float buildingHalfHeight(sim::PlaceType type) {
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
    if (place.type == sim::PlaceType::Park) return Vector2{place.position.x, place.position.y};
    const float towardRoad = place.position.y < 190.0f ? 1.0f : -1.0f;
    return Vector2{place.position.x, place.position.y + towardRoad * (buildingHalfHeight(place.type) + 3.0f)};
}

Vector2 curbPoint(const sim::Place& place) {
    const auto door = doorPosition(place);
    return Vector2{door.x, place.position.y < 190.0f ? 187.0f : 238.0f};
}

std::vector<Vector2> walkingRoute(const sim::EpisodePlan& plan, int originId, int destinationId) {
    const auto* origin = placeById(plan, originId);
    const auto* destination = placeById(plan, destinationId);
    if (!origin || !destination) return {Vector2{320.0f, 180.0f}};

    const Vector2 start = doorPosition(*origin);
    const Vector2 finish = doorPosition(*destination);
    const Vector2 startCurb = curbPoint(*origin);
    const Vector2 finishCurb = curbPoint(*destination);

    std::vector<Vector2> route{start, startCurb};
    const bool sameSide = (startCurb.y < 200.0f) == (finishCurb.y < 200.0f);
    if (sameSide) {
        route.push_back(Vector2{finishCurb.x, startCurb.y});
    } else {
        const bool bothWest = startCurb.x < 325.0f && finishCurb.x < 325.0f;
        const bool bothEast = startCurb.x > 371.0f && finishCurb.x > 371.0f;
        const float crossingX = bothWest ? 322.0f : bothEast ? 374.0f : 348.0f;
        route.push_back(Vector2{crossingX, startCurb.y});
        route.push_back(Vector2{crossingX, finishCurb.y});
        route.push_back(Vector2{finishCurb.x, finishCurb.y});
    }
    route.push_back(finish);
    return route;
}

Vector2 pointOnRoute(const std::vector<Vector2>& route, float progress) {
    if (route.empty()) return Vector2{320.0f, 180.0f};
    if (route.size() == 1) return route.front();
    progress = std::clamp(progress, 0.0f, 1.0f);

    float total = 0.0f;
    for (std::size_t i = 1; i < route.size(); ++i) total += distance(route[i - 1], route[i]);
    if (total <= 0.001f) return route.back();

    float target = progress * total;
    for (std::size_t i = 1; i < route.size(); ++i) {
        const float length = distance(route[i - 1], route[i]);
        if (target <= length || i + 1 == route.size()) {
            const float t = length > 0.001f ? target / length : 1.0f;
            return lerp(route[i - 1], route[i], std::clamp(t, 0.0f, 1.0f));
        }
        target -= length;
    }
    return route.back();
}

Vector2 stationaryPosition(const sim::EpisodePlan& plan, const sim::CitizenSnapshot& citizen) {
    const auto* place = placeById(plan, citizen.currentPlaceId);
    if (!place) return Vector2{320.0f, 180.0f};

    const float angle = static_cast<float>((citizen.id * 97) % 360) * kPi / 180.0f;
    if (place->type == sim::PlaceType::Park) {
        return Vector2{place->position.x + std::cos(angle) * (12.0f + static_cast<float>(citizen.id % 13)),
                       place->position.y + std::sin(angle) * (9.0f + static_cast<float>(citizen.id % 9))};
    }

    return Vector2{place->position.x + std::cos(angle) * 9.0f,
                   place->position.y + 3.0f + std::sin(angle) * 6.0f};
}

Vector2 snapshotPosition(const sim::EpisodePlan& plan, const sim::CitizenSnapshot& citizen) {
    if (citizen.activity == sim::Activity::Commuting
        && citizen.originPlaceId >= 0 && citizen.destinationPlaceId >= 0) {
        return pointOnRoute(walkingRoute(plan, citizen.originPlaceId, citizen.destinationPlaceId), citizen.travelProgress);
    }
    return stationaryPosition(plan, citizen);
}

Vector2 interpolatedCitizenPosition(const sim::EpisodePlan& plan,
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
        return pointOnRoute(walkingRoute(plan, current.originPlaceId, current.destinationPlaceId),
                            current.travelProgress + (endProgress - current.travelProgress) * t);
    }

    if (next->activity == sim::Activity::Commuting
        && next->originPlaceId >= 0 && next->destinationPlaceId >= 0) {
        const auto route = walkingRoute(plan, next->originPlaceId, next->destinationPlaceId);
        return pointOnRoute(route, next->travelProgress * t);
    }

    return lerp(snapshotPosition(plan, current), snapshotPosition(plan, *next), t);
}

Color shirtColor(int id) {
    static constexpr std::array<Color, 8> palette = {
        Color{170, 92, 82, 255}, Color{78, 115, 145, 255}, Color{176, 131, 73, 255},
        Color{103, 132, 88, 255}, Color{121, 90, 139, 255}, Color{172, 108, 137, 255},
        Color{78, 139, 132, 255}, Color{146, 108, 72, 255}
    };
    return palette[static_cast<std::size_t>(id) % palette.size()];
}

void drawTree(int x, int y, float scale = 1.0f) {
    DrawRectangle(x - static_cast<int>(2 * scale), y, static_cast<int>(4 * scale), static_cast<int>(7 * scale), rgb(85, 65, 45));
    DrawCircle(x, y - static_cast<int>(3 * scale), 6.0f * scale, rgb(68, 103, 60));
    DrawCircle(x - static_cast<int>(4 * scale), y, 4.0f * scale, rgb(73, 111, 65));
    DrawCircle(x + static_cast<int>(4 * scale), y, 4.0f * scale, rgb(73, 111, 65));
}

bool placeOccupied(const sim::EpisodeStep& step, int placeId) {
    for (const auto& citizen : step.citizens) if (citizen.currentPlaceId == placeId) return true;
    return false;
}

void drawStreetLight(int x, int y, bool on) {
    DrawRectangle(x, y - 11, 1, 12, rgb(74, 71, 67));
    DrawRectangle(x - 2, y - 12, 5, 2, rgb(64, 62, 59));
    if (on) {
        DrawCircle(x, y - 11, 6.0f, Fade(rgb(255, 219, 140), 0.15f));
        DrawCircle(x, y - 11, 2.0f, rgb(255, 225, 151));
    } else {
        DrawCircle(x, y - 11, 2.0f, rgb(135, 132, 120));
    }
}

void drawHome(const sim::EpisodeStep& step, const sim::Place& place, bool night) {
    const int x = static_cast<int>(place.position.x);
    const int y = static_cast<int>(place.position.y);
    const bool occupied = placeOccupied(step, place.id);
    DrawRectangle(x - 27, y - 22, 54, 45, rgb(94, 118, 77));
    DrawRectangle(x - 21, y - 13, 42, 28, rgb(201, 183, 150));
    DrawTriangle(Vector2{static_cast<float>(x - 25), static_cast<float>(y - 10)},
                 Vector2{static_cast<float>(x), static_cast<float>(y - 25)},
                 Vector2{static_cast<float>(x + 25), static_cast<float>(y - 10)}, rgb(111, 71, 59));
    DrawRectangle(x - 3, y + 2, 7, 13, rgb(92, 69, 55));
    const Color window = (night && occupied) ? rgb(246, 207, 126) : rgb(103, 133, 138);
    DrawRectangle(x - 16, y - 4, 7, 7, window);
    DrawRectangle(x + 9, y - 4, 7, 7, window);
}

void drawWorkplace(const sim::EpisodeStep& step, const sim::Place& place, bool night) {
    const int x = static_cast<int>(place.position.x);
    const int y = static_cast<int>(place.position.y);
    const bool occupied = placeOccupied(step, place.id);
    const Color window = (night && occupied) ? rgb(231, 202, 132) : rgb(104, 135, 143);
    DrawRectangle(x - 31, y - 18, 62, 37, rgb(170, 164, 147));
    DrawRectangle(x - 33, y - 21, 66, 5, rgb(94, 91, 82));
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
        DrawRectangle(x - 27 + i * 9, y - 22, 5, 5, (i % 2 == 0) ? rgb(134, 74, 61) : rgb(226, 214, 181));
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
    DrawRectangle(0, 190, 640, 45, sidewalk);
    DrawRectangle(0, 196, 640, 33, asphalt);
    for (int x = 8; x < 640; x += 28) DrawRectangle(x, 212, 14, 2, lane);
    DrawRectangle(325, 0, 46, 360, sidewalk);
    DrawRectangle(331, 0, 34, 360, asphalt);
    for (int y = 6; y < 360; y += 27) DrawRectangle(347, y, 2, 13, lane);

    // Crosswalks make the route residents take through the intersection readable.
    for (int x = 334; x <= 360; x += 6) {
        DrawRectangle(x, 198, 3, 7, rgb(200, 199, 184));
        DrawRectangle(x, 220, 3, 7, rgb(200, 199, 184));
    }
    for (int y = 199; y <= 224; y += 6) {
        DrawRectangle(326, y, 6, 3, rgb(200, 199, 184));
        DrawRectangle(364, y, 6, 3, rgb(200, 199, 184));
    }
}

bool isNight(int minute) {
    return minute >= 19 * 60 || minute < 6 * 60;
}

float nightAlpha(int minute) {
    if (minute >= 20 * 60 || minute < 5 * 60) return 0.48f;
    if (minute >= 18 * 60) return 0.48f * static_cast<float>(minute - 18 * 60) / 120.0f;
    if (minute < 7 * 60) return 0.48f * static_cast<float>(7 * 60 - minute) / 120.0f;
    return 0.0f;
}

void drawCutaway(const sim::Place& place) {
    if (!indoorPlace(place.type)) return;
    const int x = static_cast<int>(place.position.x);
    const int y = static_cast<int>(place.position.y);
    DrawRectangle(x - 19, y - 9, 38, 21, rgb(95, 83, 69));
    DrawRectangleLines(x - 19, y - 9, 38, 21, rgb(219, 194, 151));
    if (place.type == sim::PlaceType::Cafe) {
        DrawCircle(x, y + 2, 4.0f, rgb(119, 88, 62));
        DrawRectangle(x - 10, y + 8, 7, 2, rgb(77, 67, 57));
        DrawRectangle(x + 4, y + 8, 7, 2, rgb(77, 67, 57));
    } else if (place.type == sim::PlaceType::Home) {
        DrawRectangle(x - 14, y + 5, 10, 5, rgb(128, 105, 79));
        DrawRectangle(x + 5, y - 5, 9, 9, rgb(77, 89, 86));
    } else {
        DrawRectangle(x - 14, y + 6, 28, 3, rgb(119, 105, 83));
    }
}

void drawSceneBase(const sim::EpisodePlan& plan, const sim::EpisodeStep& step, bool night, int focusedPlaceId) {
    ClearBackground(rgb(101, 123, 82));
    for (int x = 0; x < 640; x += 16) {
        for (int y = 0; y < 360; y += 16) {
            if (((x / 16) + (y / 16)) % 3 == 0) DrawRectangle(x + 2, y + 4, 1, 2, rgb(89, 113, 74));
        }
    }
    drawRoads();

    const std::array<Vector2, 12> trees = {
        Vector2{14, 18}, Vector2{18, 180}, Vector2{305, 174}, Vector2{302, 338},
        Vector2{385, 178}, Vector2{458, 178}, Vector2{545, 178}, Vector2{620, 178},
        Vector2{385, 340}, Vector2{458, 340}, Vector2{535, 340}, Vector2{620, 340}
    };
    for (const auto& tree : trees) drawTree(static_cast<int>(tree.x), static_cast<int>(tree.y), 0.75f);

    for (const auto& place : plan.places) {
        switch (place.type) {
            case sim::PlaceType::Home: drawHome(step, place, night); break;
            case sim::PlaceType::Workplace: drawWorkplace(step, place, night); break;
            case sim::PlaceType::Cafe: drawCafe(step, place, night); break;
            case sim::PlaceType::Shop: drawShop(step, place, night); break;
            case sim::PlaceType::Park: drawPark(place); break;
        }
        if (place.id == focusedPlaceId) drawCutaway(place);
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

void drawCitizen(const sim::CitizenSnapshot& citizen, Vector2 position, float phase, bool walking) {
    const int x = static_cast<int>(position.x);
    const int y = static_cast<int>(position.y + std::sin(phase) * (walking ? 1.0f : 0.35f));
    DrawCircle(x, y + 4, 4.0f, Fade(BLACK, 0.22f));
    DrawRectangle(x - 2, y - 1, 5, 7, shirtColor(citizen.id));
    DrawCircle(x, y - 3, 3.0f, rgb(211, 172, 135));
    const int leg = walking && std::sin(phase * 1.7f) > 0.0f ? 1 : 0;
    DrawRectangle(x - 2 - leg, y + 6, 2, 3, rgb(58, 62, 66));
    DrawRectangle(x + 1 + leg, y + 6, 2, 3, rgb(58, 62, 66));
}

void drawNameTag(const std::string& name, Vector2 position) {
    const int size = 5;
    const int w = MeasureText(name.c_str(), size);
    const int x = static_cast<int>(position.x) - w / 2;
    const int y = static_cast<int>(position.y) - 15;
    DrawRectangle(x - 2, y - 1, w + 4, 7, Fade(rgb(25, 27, 28), 0.82f));
    DrawText(name.c_str(), x, y, size, rgb(238, 234, 216));
}

void drawInteractionCue(Vector2 a, Vector2 b, float phase) {
    const Vector2 mid{(a.x + b.x) * 0.5f, std::min(a.y, b.y) - 13.0f};
    DrawRectangleRounded(Rectangle{mid.x - 9.0f, mid.y - 5.0f, 18.0f, 9.0f}, 0.4f, 3, Fade(rgb(244, 239, 217), 0.94f));
    for (int i = 0; i < 3; ++i) {
        const float bounce = std::sin(phase + static_cast<float>(i) * 1.6f) * 0.7f;
        DrawCircle(static_cast<int>(mid.x - 5.0f + i * 5.0f), static_cast<int>(mid.y - 1.0f + bounce), 1.2f, rgb(72, 74, 70));
    }
}

void drawNightOverlay(int minute) {
    const float alpha = std::clamp(nightAlpha(minute), 0.0f, 0.52f);
    if (alpha > 0.0f) DrawRectangle(0, 0, 640, 360, Fade(rgb(27, 39, 66), alpha));
}

std::string clockText(const sim::EpisodeStep& step) {
    std::ostringstream out;
    out << "DAY " << step.day << "  " << std::setfill('0') << std::setw(2) << step.minute / 60
        << ':' << std::setw(2) << step.minute % 60;
    return out.str();
}

void drawOverlay(const sim::EpisodeStep& step, const sim::DirectedShot& shot) {
    DrawRectangle(8, 8, 122, 25, Fade(rgb(27, 30, 30), 0.86f));
    DrawText("MASON BLOCK", 14, 13, 9, rgb(241, 233, 202));
    const std::string time = clockText(step);
    DrawText(time.c_str(), 14, 23, 6, rgb(181, 185, 178));

    if (!shot.caption.empty()) {
        const int maxWidth = 430;
        const int textSize = 9;
        std::string caption = shot.caption;
        while (MeasureText(caption.c_str(), textSize) > maxWidth && caption.size() > 8) caption.pop_back();
        if (caption != shot.caption && caption.size() > 3) caption.replace(caption.size() - 3, 3, "...");
        const int w = MeasureText(caption.c_str(), textSize) + 18;
        DrawRectangle((640 - w) / 2, 319, w, 23, Fade(rgb(24, 26, 27), 0.88f));
        DrawText(caption.c_str(), (640 - MeasureText(caption.c_str(), textSize)) / 2, 326, textSize, rgb(242, 237, 216));
    }
}

Vector2 desiredCameraTarget(const sim::EpisodePlan& plan,
                            const sim::EpisodeStep& current,
                            const sim::EpisodeStep& next,
                            const sim::DirectedShot& shot,
                            float interpolation) {
    if (shot.kind == sim::ShotKind::Person && shot.personId >= 0) {
        const auto* a = citizenById(current, shot.personId);
        const auto* aNext = citizenById(next, shot.personId);
        if (a) {
            Vector2 target = interpolatedCitizenPosition(plan, *a, aNext, interpolation);
            if (shot.secondaryPersonId >= 0) {
                const auto* b = citizenById(current, shot.secondaryPersonId);
                const auto* bNext = citizenById(next, shot.secondaryPersonId);
                if (b) {
                    const Vector2 second = interpolatedCitizenPosition(plan, *b, bNext, interpolation);
                    target = Vector2{(target.x + second.x) * 0.5f, (target.y + second.y) * 0.5f};
                }
            }
            return target;
        }
    }
    if (shot.kind == sim::ShotKind::Place && shot.placeId >= 0) {
        if (const auto* place = placeById(plan, shot.placeId)) return Vector2{place->position.x, place->position.y};
    }
    return Vector2{320.0f, 180.0f};
}

float desiredZoom(const sim::DirectedShot& shot) {
    if (shot.secondaryPersonId >= 0) return 1.9f;
    switch (shot.kind) {
        case sim::ShotKind::Person: return 1.62f;
        case sim::ShotKind::Place: return 1.34f;
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

int focusedPlace(const sim::EpisodePlan& plan, const sim::EpisodeStep& step, const sim::DirectedShot& shot) {
    if (shot.placeId >= 0) return shot.placeId;
    if (shot.personId >= 0) {
        if (const auto* citizen = citizenById(step, shot.personId)) {
            return citizen->currentPlaceId >= 0 ? citizen->currentPlaceId : citizen->destinationPlaceId;
        }
    }
    return -1;
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

void writeU16(std::ofstream& out, std::uint16_t value) {
    const char bytes[2] = {static_cast<char>(value & 0xff), static_cast<char>((value >> 8) & 0xff)};
    out.write(bytes, 2);
}

void writeU32(std::ofstream& out, std::uint32_t value) {
    const char bytes[4] = {
        static_cast<char>(value & 0xff), static_cast<char>((value >> 8) & 0xff),
        static_cast<char>((value >> 16) & 0xff), static_cast<char>((value >> 24) & 0xff)
    };
    out.write(bytes, 4);
}

bool writeProceduralAudio(const std::filesystem::path& path,
                          float duration,
                          std::uint32_t seed,
                          const std::vector<AudioSegment>& segments) {
    constexpr int sampleRate = 24000;
    const int sampleCount = std::max(1, static_cast<int>(std::ceil(duration * sampleRate)));
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    out.write("RIFF", 4);
    writeU32(out, 36u + static_cast<std::uint32_t>(sampleCount * 2));
    out.write("WAVEfmt ", 8);
    writeU32(out, 16);
    writeU16(out, 1);
    writeU16(out, 1);
    writeU32(out, sampleRate);
    writeU32(out, sampleRate * 2);
    writeU16(out, 2);
    writeU16(out, 16);
    out.write("data", 4);
    writeU32(out, static_cast<std::uint32_t>(sampleCount * 2));

    std::uint32_t noiseState = seed ^ 0xa53c9e1du;
    std::size_t segmentIndex = 0;
    for (int i = 0; i < sampleCount; ++i) {
        const float t = static_cast<float>(i) / sampleRate;
        while (segmentIndex + 1 < segments.size() && t >= segments[segmentIndex].end) ++segmentIndex;
        const AudioSegment* segment = segments.empty() ? nullptr : &segments[std::min(segmentIndex, segments.size() - 1)];

        noiseState = noiseState * 1664525u + 1013904223u;
        const float noise = (static_cast<float>((noiseState >> 9) & 0x7fffff) / 4194303.5f) - 1.0f;
        float sample = noise * 0.009f;

        if (segment) {
            const bool night = isNight(segment->minute);
            const float local = t - segment->start;
            if (night) {
                const float cricketPeriod = 1.37f + static_cast<float>(seed % 17) * 0.009f;
                const float chirp = std::fmod(t + static_cast<float>(seed % 100) * 0.013f, cricketPeriod);
                if (chirp < 0.12f) {
                    const float env = std::sin(std::clamp(chirp / 0.12f, 0.0f, 1.0f) * kPi);
                    sample += std::sin(2.0f * kPi * 4100.0f * t) * env * 0.025f;
                }
            } else {
                const float birdPeriod = 4.9f + static_cast<float>(seed % 11) * 0.07f;
                const float chirp = std::fmod(t + static_cast<float>(seed % 31) * 0.11f, birdPeriod);
                if (chirp < 0.18f) {
                    const float env = std::sin(std::clamp(chirp / 0.18f, 0.0f, 1.0f) * kPi);
                    const float freq = 1650.0f + chirp * 2400.0f;
                    sample += std::sin(2.0f * kPi * freq * t) * env * 0.018f;
                }
            }

            if (segment->walking) {
                const float foot = std::fmod(local + 0.08f, 0.46f);
                if (foot < 0.055f) {
                    const float env = 1.0f - foot / 0.055f;
                    sample += std::sin(2.0f * kPi * 120.0f * foot) * env * 0.055f;
                    sample += noise * env * 0.025f;
                }
            }

            if (segment->interaction) {
                const float gate = std::fmod(local, 0.72f);
                if (gate < 0.48f) {
                    const float envelope = std::sin(std::clamp(gate / 0.48f, 0.0f, 1.0f) * kPi);
                    const float voiceA = std::sin(2.0f * kPi * (176.0f + 18.0f * std::sin(t * 7.0f)) * t);
                    const float voiceB = std::sin(2.0f * kPi * (224.0f + 22.0f * std::sin(t * 5.3f)) * t);
                    sample += (voiceA + voiceB) * 0.010f * envelope;
                }
            }
        }

        sample = std::clamp(sample, -0.92f, 0.92f);
        const auto value = static_cast<std::int16_t>(sample * 32767.0f);
        writeU16(out, static_cast<std::uint16_t>(value));
    }
    return static_cast<bool>(out);
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
    InitWindow(640, 360, "Mason Block episode renderer");
    RenderTexture2D canvas = LoadRenderTexture(640, 360);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);

    Camera2D camera{};
    camera.offset = Vector2{320.0f, 180.0f};
    camera.target = Vector2{320.0f, 180.0f};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    int frameIndex = 0;
    float elapsedVideo = 0.0f;
    bool exportFailed = false;
    std::vector<AudioSegment> audioSegments;

    for (std::size_t shotIndex = 0; shotIndex < plan.shots.size(); ++shotIndex) {
        if (options.maxVideoSeconds > 0.0f && elapsedVideo >= options.maxVideoSeconds) break;

        const auto& shot = plan.shots[shotIndex];
        const auto& current = plan.steps[shot.stepIndex];
        const auto& next = plan.steps[std::min(shot.stepIndex + 1, plan.steps.size() - 1)];
        const float segmentStart = elapsedVideo;
        int frames = std::max(1, static_cast<int>(std::round(shot.seconds * static_cast<float>(options.renderFps))));

        bool focusedWalking = false;
        if (shot.personId >= 0) {
            if (const auto* focused = citizenById(current, shot.personId)) {
                focusedWalking = focused->activity == sim::Activity::Commuting;
            }
        }

        for (int localFrame = 0; localFrame < frames; ++localFrame) {
            if (options.maxVideoSeconds > 0.0f && elapsedVideo >= options.maxVideoSeconds) break;
            const float rawT = frames > 1 ? static_cast<float>(localFrame) / static_cast<float>(frames - 1) : 1.0f;
            const float t = smoothstep(rawT);

            Vector2 wantedTarget = desiredCameraTarget(plan, current, next, shot, t);
            const float wantedZoom = desiredZoom(shot);
            wantedTarget = clampCamera(wantedTarget, wantedZoom);
            camera.target = lerp(camera.target, wantedTarget, 0.075f);
            camera.zoom += (wantedZoom - camera.zoom) * 0.075f;

            BeginTextureMode(canvas);
            BeginMode2D(camera);
            const bool night = isNight(current.minute);
            const int focusPlace = focusedPlace(plan, current, shot);
            drawSceneBase(plan, current, night, focusPlace);

            Vector2 primaryPos{};
            Vector2 secondaryPos{};
            bool havePrimary = false;
            bool haveSecondary = false;

            for (const auto& citizen : current.citizens) {
                if (!shouldDrawCitizen(plan, citizen, shot)) continue;
                const auto* nextCitizen = citizenById(next, citizen.id);
                Vector2 position = interpolatedCitizenPosition(plan, citizen, nextCitizen, t);

                if (shot.secondaryPersonId >= 0 && focusPlace >= 0
                    && (citizen.id == shot.personId || citizen.id == shot.secondaryPersonId)
                    && citizen.activity != sim::Activity::Commuting) {
                    if (const auto* place = placeById(plan, focusPlace)) {
                        const float offset = citizen.id == shot.personId ? -7.0f : 7.0f;
                        position = Vector2{place->position.x + offset, place->position.y + 4.0f};
                    }
                }

                const bool walking = citizen.activity == sim::Activity::Commuting;
                drawCitizen(citizen, position,
                            static_cast<float>(frameIndex) * (walking ? 0.36f : 0.12f) + static_cast<float>(citizen.id),
                            walking);

                if (citizen.id == shot.personId) {
                    primaryPos = position;
                    havePrimary = true;
                }
                if (citizen.id == shot.secondaryPersonId) {
                    secondaryPos = position;
                    haveSecondary = true;
                }
            }

            if (shot.secondaryPersonId >= 0 && havePrimary && haveSecondary) {
                drawInteractionCue(primaryPos, secondaryPos, static_cast<float>(frameIndex) * 0.15f);
                if (const auto* a = citizenById(current, shot.personId)) drawNameTag(a->name, primaryPos);
                if (const auto* b = citizenById(current, shot.secondaryPersonId)) drawNameTag(b->name, secondaryPos);
            }

            drawNightOverlay(current.minute);
            EndMode2D();
            drawOverlay(current, shot);
            EndTextureMode();

            const std::string outputFrame = frameName(framesDirectory, frameIndex);
            if (!exportCanvas(canvas, outputFrame)) {
                exportFailed = true;
                break;
            }

            ++frameIndex;
            elapsedVideo += 1.0f / static_cast<float>(options.renderFps);
        }

        if (elapsedVideo > segmentStart) {
            audioSegments.push_back(AudioSegment{segmentStart, elapsedVideo, current.minute,
                                                 focusedWalking, shot.secondaryPersonId >= 0, shot.importance});
        }
        if (exportFailed) break;
    }

    UnloadRenderTexture(canvas);
    CloseWindow();

    if (exportFailed || frameIndex == 0) {
        if (errorMessage) *errorMessage = exportFailed ? "Failed while exporting PNG frames" : "No frames were rendered";
        return false;
    }

    const auto audioPath = options.outputDirectory / "mason-block-ambience.wav";
    if (!writeProceduralAudio(audioPath, elapsedVideo, plan.seed, audioSegments)) {
        if (errorMessage) *errorMessage = "Failed to generate procedural episode audio";
        return false;
    }

    const auto outputPath = options.outputDirectory / options.outputFileName;
    const std::string inputPattern = (framesDirectory / "frame_%06d.png").string();
    std::ostringstream command;
    command << "ffmpeg -y -loglevel error -framerate " << options.renderFps
            << " -i " << shellQuote(inputPattern)
            << " -i " << shellQuote(audioPath.string())
            << " -vf " << shellQuote("scale=" + std::to_string(options.width) + ":" + std::to_string(options.height) + ":flags=neighbor,fps=" + std::to_string(options.outputFps))
            << " -c:v libx264 -preset medium -crf 18 -pix_fmt yuv420p"
            << " -c:a aac -b:a 128k -shortest -movflags +faststart "
            << shellQuote(outputPath.string());

    const int ffmpegStatus = std::system(command.str().c_str());
    if (ffmpegStatus != 0 || !std::filesystem::exists(outputPath)) {
        if (errorMessage) *errorMessage = "ffmpeg failed. Make sure ffmpeg is installed and available in PATH";
        return false;
    }

    if (!options.keepFrames) std::filesystem::remove_all(framesDirectory);
    return true;
}

} // namespace view
