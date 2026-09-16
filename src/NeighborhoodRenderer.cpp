#include "view/NeighborhoodRenderer.hpp"

#include "raylib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace view {
namespace {

constexpr int kCanvasWidth = 640;
constexpr int kCanvasHeight = 360;
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kSimStepMinutes = 10;

struct CitizenVisual {
    Vector2 position{};
    Vector2 target{};
    int lastPlaceId{-1};
    float bobPhase{};
    Color shirt{255, 255, 255, 255};
};

Color rgb(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
    return Color{r, g, b, a};
}

Color shirtColor(int id) {
    static constexpr std::array<Color, 8> palette = {
        Color{170, 92, 82, 255}, Color{78, 115, 145, 255}, Color{176, 131, 73, 255},
        Color{103, 132, 88, 255}, Color{121, 90, 139, 255}, Color{172, 108, 137, 255},
        Color{78, 139, 132, 255}, Color{146, 108, 72, 255}
    };
    return palette[static_cast<std::size_t>(id) % palette.size()];
}

std::string activityLabel(sim::Activity activity) {
    switch (activity) {
        case sim::Activity::Sleeping: return "sleeping";
        case sim::Activity::AtHome: return "at home";
        case sim::Activity::Commuting: return "commuting";
        case sim::Activity::Working: return "working";
        case sim::Activity::Eating: return "eating";
        case sim::Activity::Shopping: return "shopping";
        case sim::Activity::Socializing: return "socializing";
        case sim::Activity::Relaxing: return "relaxing";
    }
    return "unknown";
}

const sim::Place* placeById(const sim::World& world, int id) {
    for (const auto& place : world.places()) {
        if (place.id == id) {
            return &place;
        }
    }
    return nullptr;
}

Vector2 citizenTarget(const sim::World& world, const sim::Person& person) {
    const auto* place = placeById(world, person.currentPlaceId);
    if (!place) {
        return Vector2{320.0f, 180.0f};
    }

    const float angle = static_cast<float>((person.id * 97) % 360) * 3.14159265f / 180.0f;
    const float radius = 5.0f + static_cast<float>((person.id * 13) % 8);
    Vector2 result{place->position.x + std::cos(angle) * radius,
                   place->position.y + std::sin(angle) * radius};

    switch (place->type) {
        case sim::PlaceType::Home:
            result.y += 8.0f;
            break;
        case sim::PlaceType::Workplace:
            result.y += 10.0f;
            break;
        case sim::PlaceType::Cafe:
        case sim::PlaceType::Shop:
            result.y += 7.0f;
            break;
        case sim::PlaceType::Park:
            result.x += std::cos(angle * 1.7f) * 13.0f;
            result.y += std::sin(angle * 1.3f) * 10.0f;
            break;
    }
    return result;
}

float distanceSquared(Vector2 a, Vector2 b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

void drawTree(int x, int y, float scale = 1.0f) {
    DrawRectangle(x - static_cast<int>(2 * scale), y, static_cast<int>(4 * scale), static_cast<int>(7 * scale), rgb(85, 65, 45));
    DrawCircle(x, y - static_cast<int>(3 * scale), 6.0f * scale, rgb(68, 103, 60));
    DrawCircle(x - static_cast<int>(4 * scale), y, 4.0f * scale, rgb(73, 111, 65));
    DrawCircle(x + static_cast<int>(4 * scale), y, 4.0f * scale, rgb(73, 111, 65));
}

void drawStreetLight(int x, int y, bool on) {
    DrawRectangle(x, y - 11, 1, 12, rgb(74, 71, 67));
    DrawRectangle(x - 2, y - 12, 5, 2, rgb(64, 62, 59));
    if (on) {
        DrawCircle(x, y - 11, 5.0f, Fade(rgb(255, 219, 140), 0.18f));
        DrawCircle(x, y - 11, 2.0f, rgb(255, 225, 151));
    } else {
        DrawCircle(x, y - 11, 2.0f, rgb(135, 132, 120));
    }
}

bool placeOccupied(const sim::World& world, int placeId) {
    for (const auto& citizen : world.citizens()) {
        if (citizen.currentPlaceId == placeId) {
            return true;
        }
    }
    return false;
}

void drawHome(const sim::World& world, const sim::Place& place, bool night) {
    const int x = static_cast<int>(place.position.x);
    const int y = static_cast<int>(place.position.y);
    const bool occupied = placeOccupied(world, place.id);

    DrawRectangle(x - 27, y - 22, 54, 45, rgb(94, 118, 77));
    DrawRectangle(x - 21, y - 13, 42, 28, rgb(201, 183, 150));
    DrawRectangle(x - 24, y - 17, 48, 7, rgb(123, 83, 67));
    DrawTriangle(Vector2{static_cast<float>(x - 25), static_cast<float>(y - 10)},
                 Vector2{static_cast<float>(x), static_cast<float>(y - 25)},
                 Vector2{static_cast<float>(x + 25), static_cast<float>(y - 10)},
                 rgb(111, 71, 59));
    DrawRectangle(x - 3, y + 2, 7, 13, rgb(92, 69, 55));

    const Color window = (night && occupied) ? rgb(246, 207, 126) : rgb(103, 133, 138);
    DrawRectangle(x - 16, y - 4, 7, 7, window);
    DrawRectangle(x + 9, y - 4, 7, 7, window);
    DrawRectangle(x - 1, y + 15, 2, 8, rgb(153, 139, 112));
}

void drawWorkplace(const sim::World& world, const sim::Place& place, bool night) {
    const int x = static_cast<int>(place.position.x);
    const int y = static_cast<int>(place.position.y);
    const bool occupied = placeOccupied(world, place.id);
    const Color window = (night && occupied) ? rgb(231, 202, 132) : rgb(104, 135, 143);

    DrawRectangle(x - 31, y - 18, 62, 37, rgb(170, 164, 147));
    DrawRectangle(x - 33, y - 21, 66, 5, rgb(94, 91, 82));
    DrawRectangle(x - 24, y - 10, 13, 10, window);
    DrawRectangle(x - 6, y - 10, 13, 10, window);
    DrawRectangle(x + 12, y - 10, 13, 10, window);
    DrawRectangle(x - 4, y + 4, 9, 15, rgb(84, 78, 69));
}

void drawCafe(const sim::World& world, const sim::Place& place, bool night) {
    const int x = static_cast<int>(place.position.x);
    const int y = static_cast<int>(place.position.y);
    const bool occupied = placeOccupied(world, place.id);
    const Color window = (night && occupied) ? rgb(245, 200, 122) : rgb(116, 148, 151);

    DrawRectangle(x - 26, y - 17, 52, 35, rgb(182, 154, 126));
    DrawRectangle(x - 28, y - 21, 56, 6, rgb(80, 106, 119));
    DrawRectangle(x - 17, y - 8, 13, 12, window);
    DrawRectangle(x + 4, y - 8, 13, 12, window);
    DrawRectangle(x - 3, y + 5, 7, 13, rgb(82, 63, 53));
    DrawText("CAFE", x - 14, y - 18, 6, rgb(237, 231, 210));
}

void drawShop(const sim::World& world, const sim::Place& place, bool night) {
    const int x = static_cast<int>(place.position.x);
    const int y = static_cast<int>(place.position.y);
    const bool occupied = placeOccupied(world, place.id);
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
    DrawRectangle(x - 17, y + 13, 2, 4, rgb(91, 67, 49));
    DrawRectangle(x - 5, y + 13, 2, 4, rgb(91, 67, 49));
}

void drawRoads() {
    const Color asphalt = rgb(73, 76, 76);
    const Color sidewalk = rgb(158, 157, 145);
    const Color lane = rgb(188, 176, 130);

    DrawRectangle(0, 190, kCanvasWidth, 45, sidewalk);
    DrawRectangle(0, 196, kCanvasWidth, 33, asphalt);
    for (int x = 8; x < kCanvasWidth; x += 28) {
        DrawRectangle(x, 212, 14, 2, lane);
    }

    DrawRectangle(325, 0, 46, kCanvasHeight, sidewalk);
    DrawRectangle(331, 0, 34, kCanvasHeight, asphalt);
    for (int y = 6; y < kCanvasHeight; y += 27) {
        DrawRectangle(347, y, 2, 13, lane);
    }

    DrawRectangle(0, 196, kCanvasWidth, 1, rgb(112, 112, 106));
    DrawRectangle(0, 228, kCanvasWidth, 1, rgb(112, 112, 106));
    DrawRectangle(331, 0, 1, kCanvasHeight, rgb(112, 112, 106));
    DrawRectangle(364, 0, 1, kCanvasHeight, rgb(112, 112, 106));
}

void drawBusStop(bool night) {
    const int x = 394;
    const int y = 185;
    DrawRectangle(x, y - 17, 2, 19, rgb(63, 66, 66));
    DrawRectangle(x - 2, y - 20, 13, 6, rgb(92, 121, 131));
    DrawRectangle(x + 2, y - 18, 6, 2, night ? rgb(224, 204, 140) : rgb(212, 218, 207));
    DrawRectangle(x + 13, y - 4, 18, 2, rgb(91, 69, 49));
    DrawRectangle(x + 15, y - 2, 2, 5, rgb(91, 69, 49));
    DrawRectangle(x + 27, y - 2, 2, 5, rgb(91, 69, 49));
}

void drawNeighborhood(const sim::World& world, bool night, bool labels) {
    ClearBackground(rgb(101, 123, 82));

    for (int x = 0; x < kCanvasWidth; x += 16) {
        for (int y = 0; y < kCanvasHeight; y += 16) {
            if (((x / 16) + (y / 16)) % 3 == 0) {
                DrawRectangle(x + 2, y + 4, 1, 2, rgb(89, 113, 74));
            }
        }
    }

    drawRoads();

    const std::array<Vector2, 13> trees = {
        Vector2{14, 18}, Vector2{80, 188}, Vector2{155, 184}, Vector2{250, 184},
        Vector2{307, 172}, Vector2{20, 300}, Vector2{195, 344}, Vector2{316, 334},
        Vector2{384, 18}, Vector2{470, 16}, Vector2{535, 92}, Vector2{596, 178}, Vector2{605, 332}
    };
    for (const auto& tree : trees) {
        drawTree(static_cast<int>(tree.x), static_cast<int>(tree.y), 0.8f);
    }

    for (const auto& place : world.places()) {
        switch (place.type) {
            case sim::PlaceType::Home: drawHome(world, place, night); break;
            case sim::PlaceType::Workplace: drawWorkplace(world, place, night); break;
            case sim::PlaceType::Cafe: drawCafe(world, place, night); break;
            case sim::PlaceType::Shop: drawShop(world, place, night); break;
            case sim::PlaceType::Park: drawPark(place); break;
        }

        if (labels) {
            const int textWidth = MeasureText(place.name.c_str(), 6);
            DrawRectangle(static_cast<int>(place.position.x) - textWidth / 2 - 2,
                          static_cast<int>(place.position.y) + 24,
                          textWidth + 4, 8, Fade(BLACK, 0.55f));
            DrawText(place.name.c_str(),
                     static_cast<int>(place.position.x) - textWidth / 2,
                     static_cast<int>(place.position.y) + 25,
                     6, rgb(234, 231, 218));
        }
    }

    drawBusStop(night);
    drawStreetLight(45, 193, night);
    drawStreetLight(168, 193, night);
    drawStreetLight(286, 193, night);
    drawStreetLight(410, 193, night);
    drawStreetLight(532, 193, night);
    drawStreetLight(328, 70, night);
    drawStreetLight(328, 285, night);
}

void drawCitizen(const sim::Person& person, const CitizenVisual& visual, bool selected) {
    if (person.activity == sim::Activity::Sleeping) {
        return;
    }

    const float bob = std::sin(visual.bobPhase) * 0.8f;
    const int x = static_cast<int>(visual.position.x);
    const int y = static_cast<int>(visual.position.y + bob);

    DrawCircle(x, y + 4, 4.0f, Fade(BLACK, 0.22f));
    DrawRectangle(x - 2, y - 1, 5, 7, visual.shirt);
    DrawCircle(x, y - 3, 3.0f, rgb(211, 172, 135));
    DrawRectangle(x - 2, y + 6, 2, 3, rgb(58, 62, 66));
    DrawRectangle(x + 1, y + 6, 2, 3, rgb(58, 62, 66));

    if (selected) {
        DrawCircle(x, y + 1, 8.0f, Fade(rgb(245, 232, 173), 0.20f));
        DrawRectangleLinesEx(Rectangle{static_cast<float>(x - 6), static_cast<float>(y - 8), 12.0f, 17.0f}, 1.0f, rgb(246, 232, 171));
    }
}

float nightAlpha(int minute) {
    if (minute >= 20 * 60 || minute < 5 * 60) {
        return 0.48f;
    }
    if (minute >= 18 * 60) {
        return 0.48f * static_cast<float>(minute - 18 * 60) / 120.0f;
    }
    if (minute < 7 * 60) {
        return 0.48f * static_cast<float>(7 * 60 - minute) / 120.0f;
    }
    return 0.0f;
}

bool isNight(int minute) {
    return minute >= 19 * 60 || minute < 6 * 60;
}

void drawDayNightOverlay(int minute) {
    const float alpha = std::clamp(nightAlpha(minute), 0.0f, 0.52f);
    if (alpha > 0.0f) {
        DrawRectangle(0, 0, kCanvasWidth, kCanvasHeight, Fade(rgb(27, 39, 66), alpha));
    }

    if (minute >= 5 * 60 && minute < 8 * 60) {
        const float dawn = 1.0f - std::abs(static_cast<float>(minute - 6 * 60 - 30) / 90.0f);
        if (dawn > 0.0f) {
            DrawRectangle(0, 0, kCanvasWidth, kCanvasHeight, Fade(rgb(222, 143, 91), dawn * 0.08f));
        }
    }
}

std::string moneyLabel(float cash) {
    std::ostringstream out;
    out << '$' << static_cast<int>(std::round(cash));
    return out.str();
}

void drawGauge(int x, int y, int width, const char* label, float value, Color fill) {
    DrawText(label, x, y, 7, rgb(212, 211, 201));
    DrawRectangle(x + 48, y + 1, width, 5, rgb(57, 60, 60));
    const int filled = static_cast<int>(static_cast<float>(width) * std::clamp(value, 0.0f, 100.0f) / 100.0f);
    DrawRectangle(x + 48, y + 1, filled, 5, fill);
}

void drawSelectedPanel(const sim::World& world, int selectedId) {
    if (selectedId < 0 || selectedId >= static_cast<int>(world.citizens().size())) {
        return;
    }

    const auto& person = world.citizens()[static_cast<std::size_t>(selectedId)];
    const int x = 474;
    const int y = 10;
    DrawRectangle(x, y, 156, 102, Fade(rgb(29, 31, 31), 0.92f));
    DrawRectangleLinesEx(Rectangle{static_cast<float>(x), static_cast<float>(y), 156.0f, 102.0f}, 1.0f, rgb(115, 116, 109));
    DrawText(person.name.c_str(), x + 8, y + 8, 9, rgb(243, 238, 216));

    std::ostringstream info;
    info << person.age << "  |  " << activityLabel(person.activity) << "  |  " << moneyLabel(person.cash);
    DrawText(info.str().c_str(), x + 8, y + 22, 6, rgb(177, 181, 174));

    drawGauge(x + 8, y + 38, 88, "energy", person.energy, rgb(105, 149, 111));
    drawGauge(x + 8, y + 50, 88, "hunger", person.hunger, rgb(180, 139, 79));
    drawGauge(x + 8, y + 62, 88, "stress", person.stress, rgb(171, 93, 83));
    drawGauge(x + 8, y + 74, 88, "social", 100.0f - person.loneliness, rgb(92, 128, 159));

    const auto* place = placeById(world, person.currentPlaceId);
    const std::string placeName = place ? place->name : "somewhere";
    DrawText(placeName.c_str(), x + 8, y + 89, 6, rgb(193, 190, 176));
}

void drawEventFeed(const sim::World& world) {
    constexpr int maxEvents = 3;
    std::array<const sim::Event*, maxEvents> recent{};
    int count = 0;

    for (auto it = world.events().rbegin(); it != world.events().rend() && count < maxEvents; ++it) {
        if (it->importance < 0.15f) {
            continue;
        }
        recent[static_cast<std::size_t>(count++)] = &(*it);
    }

    if (count == 0) {
        return;
    }

    const int panelHeight = 13 + count * 12;
    DrawRectangle(8, kCanvasHeight - panelHeight - 8, 347, panelHeight, Fade(rgb(26, 29, 29), 0.82f));
    DrawText("NEIGHBORHOOD LOG", 14, kCanvasHeight - panelHeight - 4, 6, rgb(185, 185, 171));

    for (int i = count - 1, row = 0; i >= 0; --i, ++row) {
        const auto* event = recent[static_cast<std::size_t>(i)];
        const int yy = kCanvasHeight - panelHeight + 7 + row * 12;
        DrawText(event->text.c_str(), 14, yy, 6, rgb(227, 224, 207));
    }
}

void drawHud(const sim::World& world, bool paused, int speedIndex, bool labels, int selectedId) {
    DrawRectangle(8, 8, 196, 39, Fade(rgb(27, 30, 30), 0.87f));
    DrawText("MASON BLOCK", 14, 13, 11, rgb(241, 233, 202));

    std::ostringstream line;
    line << "DAY " << world.day() << "  " << world.clockLabel() << "  " << world.citizens().size() << " RESIDENTS";
    DrawText(line.str().c_str(), 14, 28, 7, rgb(183, 188, 179));

    const char* state = paused ? "PAUSED" : (speedIndex == 0 ? "1X" : speedIndex == 1 ? "3X" : "8X");
    DrawRectangle(214, 8, 55, 19, Fade(rgb(27, 30, 30), 0.84f));
    DrawText(state, 225, 14, 8, paused ? rgb(222, 173, 105) : rgb(158, 190, 151));

    DrawRectangle(382, 334, 248, 18, Fade(rgb(27, 30, 30), 0.78f));
    DrawText("SPACE pause  1/2/3 speed  N step  TAB labels  click person", 388, 340, 6, rgb(183, 185, 177));

    if (!labels) {
        DrawText("labels off", 274, 15, 6, rgb(148, 151, 145));
    }

    drawSelectedPanel(world, selectedId);
    drawEventFeed(world);
}

Vector2 mouseToCanvas() {
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    const float scale = std::min(static_cast<float>(screenWidth) / static_cast<float>(kCanvasWidth),
                                 static_cast<float>(screenHeight) / static_cast<float>(kCanvasHeight));
    const float drawWidth = static_cast<float>(kCanvasWidth) * scale;
    const float drawHeight = static_cast<float>(kCanvasHeight) * scale;
    const float offsetX = (static_cast<float>(screenWidth) - drawWidth) * 0.5f;
    const float offsetY = (static_cast<float>(screenHeight) - drawHeight) * 0.5f;
    const Vector2 mouse = GetMousePosition();
    return Vector2{(mouse.x - offsetX) / scale, (mouse.y - offsetY) / scale};
}

void drawTextureToWindow(RenderTexture2D target) {
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    const float scale = std::min(static_cast<float>(screenWidth) / static_cast<float>(kCanvasWidth),
                                 static_cast<float>(screenHeight) / static_cast<float>(kCanvasHeight));
    const float drawWidth = static_cast<float>(kCanvasWidth) * scale;
    const float drawHeight = static_cast<float>(kCanvasHeight) * scale;
    const float offsetX = (static_cast<float>(screenWidth) - drawWidth) * 0.5f;
    const float offsetY = (static_cast<float>(screenHeight) - drawHeight) * 0.5f;

    DrawTexturePro(target.texture,
                   Rectangle{0.0f, 0.0f, static_cast<float>(kCanvasWidth), -static_cast<float>(kCanvasHeight)},
                   Rectangle{offsetX, offsetY, drawWidth, drawHeight},
                   Vector2{0.0f, 0.0f}, 0.0f, WHITE);
}

} // namespace

NeighborhoodRenderer::NeighborhoodRenderer(sim::World& world) : world_(world) {}

int NeighborhoodRenderer::run() {
    InitWindow(kWindowWidth, kWindowHeight, "Crime City - Mason Block");
    SetTargetFPS(60);

    RenderTexture2D target = LoadRenderTexture(kCanvasWidth, kCanvasHeight);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    while (world_.minute() < 5 * 60 + 30) {
        world_.step(kSimStepMinutes);
    }

    std::vector<CitizenVisual> visuals(world_.citizens().size());
    for (std::size_t i = 0; i < visuals.size(); ++i) {
        const auto& person = world_.citizens()[i];
        visuals[i].position = citizenTarget(world_, person);
        visuals[i].target = visuals[i].position;
        visuals[i].lastPlaceId = person.currentPlaceId;
        visuals[i].bobPhase = static_cast<float>(i) * 0.73f;
        visuals[i].shirt = shirtColor(person.id);
    }

    bool paused = false;
    bool labels = false;
    int speedIndex = 0;
    int selectedId = -1;
    float stepTimer = 0.0f;
    constexpr std::array<float, 3> stepIntervals = {0.42f, 0.14f, 0.0525f};

    while (!WindowShouldClose()) {
        const float dt = std::min(GetFrameTime(), 0.05f);

        if (IsKeyPressed(KEY_SPACE)) paused = !paused;
        if (IsKeyPressed(KEY_ONE)) { speedIndex = 0; paused = false; }
        if (IsKeyPressed(KEY_TWO)) { speedIndex = 1; paused = false; }
        if (IsKeyPressed(KEY_THREE)) { speedIndex = 2; paused = false; }
        if (IsKeyPressed(KEY_TAB)) labels = !labels;

        bool stepped = false;
        if (IsKeyPressed(KEY_N)) {
            world_.step(kSimStepMinutes);
            stepped = true;
        }

        if (!paused) {
            stepTimer += dt;
            while (stepTimer >= stepIntervals[static_cast<std::size_t>(speedIndex)]) {
                stepTimer -= stepIntervals[static_cast<std::size_t>(speedIndex)];
                world_.step(kSimStepMinutes);
                stepped = true;
            }
        }

        if (stepped) {
            const auto& citizens = world_.citizens();
            if (visuals.size() != citizens.size()) visuals.resize(citizens.size());
            for (std::size_t i = 0; i < citizens.size(); ++i) {
                visuals[i].lastPlaceId = citizens[i].currentPlaceId;
                visuals[i].target = citizenTarget(world_, citizens[i]);
                if (visuals[i].shirt.a == 0) visuals[i].shirt = shirtColor(citizens[i].id);
            }
        }

        for (auto& visual : visuals) {
            const float dx = visual.target.x - visual.position.x;
            const float dy = visual.target.y - visual.position.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance > 0.15f) {
                const float speed = 52.0f;
                const float amount = std::min(distance, speed * dt);
                visual.position.x += dx / distance * amount;
                visual.position.y += dy / distance * amount;
                visual.bobPhase += dt * 9.0f;
            } else {
                visual.position = visual.target;
                visual.bobPhase += dt * 2.0f;
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            const Vector2 mouse = mouseToCanvas();
            float best = 9.0f * 9.0f;
            int bestId = -1;
            for (std::size_t i = 0; i < visuals.size(); ++i) {
                if (world_.citizens()[i].activity == sim::Activity::Sleeping) continue;
                const float dist = distanceSquared(mouse, visuals[i].position);
                if (dist < best) {
                    best = dist;
                    bestId = static_cast<int>(i);
                }
            }
            selectedId = bestId;
        }

        BeginTextureMode(target);
        const bool night = isNight(world_.minute());
        drawNeighborhood(world_, night, labels);

        std::vector<std::size_t> order(visuals.size());
        for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            return visuals[a].position.y < visuals[b].position.y;
        });
        for (const std::size_t i : order) {
            drawCitizen(world_.citizens()[i], visuals[i], selectedId == static_cast<int>(i));
        }

        drawDayNightOverlay(world_.minute());
        if (night) {
            drawStreetLight(45, 193, true);
            drawStreetLight(168, 193, true);
            drawStreetLight(286, 193, true);
            drawStreetLight(410, 193, true);
            drawStreetLight(532, 193, true);
            drawStreetLight(328, 70, true);
            drawStreetLight(328, 285, true);
        }
        drawHud(world_, paused, speedIndex, labels, selectedId);
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        drawTextureToWindow(target);
        EndDrawing();
    }

    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}

} // namespace view
