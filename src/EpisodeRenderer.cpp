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

namespace view {
namespace {

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

Vector2 citizenPosition(const sim::EpisodePlan& plan, const sim::CitizenSnapshot& citizen) {
    const auto* place = placeById(plan, citizen.currentPlaceId);
    if (!place) return Vector2{320.0f, 180.0f};

    const float angle = static_cast<float>((citizen.id * 97) % 360) * 3.14159265f / 180.0f;
    const float radius = 5.0f + static_cast<float>((citizen.id * 13) % 8);
    Vector2 result{place->position.x + std::cos(angle) * radius,
                   place->position.y + std::sin(angle) * radius};

    switch (place->type) {
        case sim::PlaceType::Home: result.y += 8.0f; break;
        case sim::PlaceType::Workplace: result.y += 10.0f; break;
        case sim::PlaceType::Cafe:
        case sim::PlaceType::Shop: result.y += 7.0f; break;
        case sim::PlaceType::Park:
            result.x += std::cos(angle * 1.7f) * 13.0f;
            result.y += std::sin(angle * 1.3f) * 10.0f;
            break;
    }
    return result;
}

Vector2 lerp(Vector2 a, Vector2 b, float t) {
    return Vector2{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

float smoothstep(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
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
    for (const auto& citizen : step.citizens) {
        if (citizen.currentPlaceId == placeId) return true;
    }
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
                 Vector2{static_cast<float>(x + 25), static_cast<float>(y - 10)},
                 rgb(111, 71, 59));
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

void drawSceneBase(const sim::EpisodePlan& plan, const sim::EpisodeStep& step, bool night) {
    ClearBackground(rgb(101, 123, 82));
    for (int x = 0; x < 640; x += 16) {
        for (int y = 0; y < 360; y += 16) {
            if (((x / 16) + (y / 16)) % 3 == 0) DrawRectangle(x + 2, y + 4, 1, 2, rgb(89, 113, 74));
        }
    }
    drawRoads();
    const std::array<Vector2, 13> trees = {
        Vector2{14, 18}, Vector2{80, 188}, Vector2{155, 184}, Vector2{250, 184},
        Vector2{307, 172}, Vector2{20, 300}, Vector2{195, 344}, Vector2{316, 334},
        Vector2{384, 18}, Vector2{470, 16}, Vector2{535, 92}, Vector2{596, 178}, Vector2{605, 332}
    };
    for (const auto& tree : trees) drawTree(static_cast<int>(tree.x), static_cast<int>(tree.y), 0.8f);

    for (const auto& place : plan.places) {
        switch (place.type) {
            case sim::PlaceType::Home: drawHome(step, place, night); break;
            case sim::PlaceType::Workplace: drawWorkplace(step, place, night); break;
            case sim::PlaceType::Cafe: drawCafe(step, place, night); break;
            case sim::PlaceType::Shop: drawShop(step, place, night); break;
            case sim::PlaceType::Park: drawPark(place); break;
        }
    }

    drawStreetLight(45, 193, night);
    drawStreetLight(168, 193, night);
    drawStreetLight(286, 193, night);
    drawStreetLight(410, 193, night);
    drawStreetLight(532, 193, night);
    drawStreetLight(328, 70, night);
    drawStreetLight(328, 285, night);
}

void drawCitizen(const sim::CitizenSnapshot& citizen, Vector2 position, float phase) {
    if (citizen.activity == sim::Activity::Sleeping) return;
    const int x = static_cast<int>(position.x);
    const int y = static_cast<int>(position.y + std::sin(phase) * 0.7f);
    DrawCircle(x, y + 4, 4.0f, Fade(BLACK, 0.22f));
    DrawRectangle(x - 2, y - 1, 5, 7, shirtColor(citizen.id));
    DrawCircle(x, y - 3, 3.0f, rgb(211, 172, 135));
    DrawRectangle(x - 2, y + 6, 2, 3, rgb(58, 62, 66));
    DrawRectangle(x + 1, y + 6, 2, 3, rgb(58, 62, 66));
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
        const auto* b = citizenById(next, shot.personId);
        if (a && b) return lerp(citizenPosition(plan, *a), citizenPosition(plan, *b), interpolation);
        if (a) return citizenPosition(plan, *a);
    }
    if (shot.kind == sim::ShotKind::Place && shot.placeId >= 0) {
        if (const auto* place = placeById(plan, shot.placeId)) return Vector2{place->position.x, place->position.y};
    }
    return Vector2{320.0f, 180.0f};
}

float desiredZoom(const sim::DirectedShot& shot) {
    switch (shot.kind) {
        case sim::ShotKind::Person: return 1.55f;
        case sim::ShotKind::Place: return 1.32f;
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

    for (std::size_t shotIndex = 0; shotIndex < plan.shots.size(); ++shotIndex) {
        if (options.maxVideoSeconds > 0.0f && elapsedVideo >= options.maxVideoSeconds) break;

        const auto& shot = plan.shots[shotIndex];
        const auto& current = plan.steps[shot.stepIndex];
        const auto& next = plan.steps[std::min(shot.stepIndex + 1, plan.steps.size() - 1)];
        int frames = std::max(1, static_cast<int>(std::round(shot.seconds * static_cast<float>(options.renderFps))));

        for (int localFrame = 0; localFrame < frames; ++localFrame) {
            if (options.maxVideoSeconds > 0.0f && elapsedVideo >= options.maxVideoSeconds) break;
            const float rawT = frames > 1 ? static_cast<float>(localFrame) / static_cast<float>(frames - 1) : 1.0f;
            const float t = smoothstep(rawT);

            Vector2 wantedTarget = desiredCameraTarget(plan, current, next, shot, t);
            float wantedZoom = desiredZoom(shot);
            wantedTarget = clampCamera(wantedTarget, wantedZoom);
            camera.target = lerp(camera.target, wantedTarget, 0.085f);
            camera.zoom += (wantedZoom - camera.zoom) * 0.085f;

            BeginTextureMode(canvas);
            BeginMode2D(camera);
            const bool night = isNight(current.minute);
            drawSceneBase(plan, current, night);

            for (const auto& citizen : current.citizens) {
                const auto* nextCitizen = citizenById(next, citizen.id);
                Vector2 position = citizenPosition(plan, citizen);
                if (nextCitizen) position = lerp(position, citizenPosition(plan, *nextCitizen), t);
                drawCitizen(citizen, position, static_cast<float>(frameIndex) * 0.22f + static_cast<float>(citizen.id));
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
            << " -vf " << shellQuote("scale=" + std::to_string(options.width) + ":" + std::to_string(options.height) + ":flags=neighbor,fps=" + std::to_string(options.outputFps))
            << " -c:v libx264 -preset medium -crf 18 -pix_fmt yuv420p -movflags +faststart "
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
