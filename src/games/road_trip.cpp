#include "games/road_trip.h"
#include "games/road_trip_assets.h"

#include <math.h>

// =============================================================================
// ROAD TRIP - port del motor original de Filmote & Vampirics
// Road engine pseudo-3D, coches, HUD y dia/noche adaptados a 128x64 monocromo.
// =============================================================================

namespace Rt = RoadTripImages;

namespace {

constexpr int16_t UPM = 64;                    // unidades por metro
constexpr uint8_t DISPLAY_WIDTH = 128;
constexpr uint8_t DISPLAY_HEIGHT = 64;

constexpr uint8_t NoCollision = 127;
constexpr uint8_t NumberOfOtherCars = 4;
constexpr uint8_t NumberOfCactii = 3;

constexpr uint8_t PlayerCarWidthUnits = 78;
constexpr uint8_t PlayerCarLengthUnits = 60;
constexpr uint8_t PlayerCarHeightPX = 17;
constexpr uint8_t OtherCarWidthUnits = 74;
constexpr uint8_t OtherCarLengthUnits = 60;
constexpr uint8_t CollisionTollerance_X = 68;
constexpr uint8_t CollisionTollerance_Z = 50;

constexpr uint16_t SpeedDiv = 1800;
constexpr uint16_t ZReduction = 8192;
constexpr int16_t ZMax = 8192 + 2048;
constexpr uint8_t DistToRenewCars = 50;

constexpr uint8_t BrakeCloud_Accelerate = 10;
constexpr uint8_t BrakeCloud_DownGear = 10;
constexpr uint8_t BrakeCloud_OffRoad = 4;

constexpr uint16_t Gear1Max = 6400;
constexpr uint16_t Gear2Max = 11200;
constexpr uint16_t Gear3Max = 21000;
constexpr uint16_t Gear4Max = 40000;
constexpr uint16_t Gear5Max = 50000;

constexpr uint16_t GearsMax[] = { 0, Gear1Max, Gear2Max, Gear3Max, Gear4Max, Gear5Max };

constexpr uint8_t NewDayBannerDelay = 100;
constexpr uint8_t NewDayBannerDelay_CountDown = 130;
constexpr uint8_t CarsToPass_Init = 30;
constexpr uint8_t CarsToPass_Increase = 5;
constexpr uint8_t NudgeDistance = 10;

constexpr uint8_t Sky_BX______D = 6;
constexpr uint8_t Sky_B_X_____D = 4;
constexpr uint8_t Sky_B__X____D = 3;
constexpr uint8_t Sky_B___X___D = 2;
constexpr uint8_t Sky_B____X__D = 1;
constexpr uint8_t Sky_B_____X_D = 0;
constexpr uint8_t Sky_B______XD = 5;

constexpr uint8_t SpeedSteering[] = {
    0, 5, 5, 5, 5, 6, 7, 8, 8, 9, 9, 10, 10, 10,
    11, 11, 11, 12, 12, 12, 13, 13, 14, 14, 15, 15,
    16, 16, 17, 17, 18
};

#define ROAD_SEGMENTS 16
#define DRAW_DISTANCE 10

enum class Direction : uint8_t { None, Up, Down, Left, Right, Both };
enum class MovementType : uint8_t { Horizontal, Vertical };
enum class CarMovement : uint8_t { Accelerate, Coast, Decelerate, NoMovement };
enum class TimeOfDay : uint8_t { Day, Dawn, Night };

namespace Curves {
namespace Roads {
namespace Vertical {
const uint8_t Curve_00[] = { 1, 2, 3, 2, 1 };
const uint8_t Curve_01[] = { 1, 1, 2, 3, 3, 2, 1, 1 };
const uint8_t Curve_02[] = { 1, 1, 2, 3, 4, 4, 4, 3, 2, 1, 1 };
const uint8_t Curve_03[] = { 1, 1, 2, 3, 4, 5, 6, 6, 6, 5, 4, 3, 2, 1, 1 };
const uint8_t Curve_04[] = { 1, 1, 2, 3, 4, 5, 6, 7, 8, 8, 8, 8, 8, 7, 6, 5, 4, 3, 2, 1 };
const uint8_t *Data[] = { Curve_00, Curve_01, Curve_02, Curve_03, Curve_04 };
const uint8_t Length[] = { 5, 8, 11, 15, 20 };
const uint8_t Count = 5;
} // namespace Vertical

namespace Horizontal {
const uint8_t Curve_00[] = {
    1, 1, 2, 2, 3, 3, 4, 5, 6, 7, 9, 11, 12, 13, 14,
    14, 15, 15, 15, 14, 14, 13, 12, 11, 9, 7, 6, 5, 4, 3,
    3, 2, 2, 1, 1
};
const uint8_t Curve_01[] = {
    1, 1, 2, 2, 3, 3, 4, 5, 6, 7, 9, 11, 14, 17, 19,
    21, 22, 23, 24, 25, 25, 26, 26, 27, 27, 27, 27, 26, 26, 25,
    25, 24, 23, 22, 21, 19, 17, 14, 11, 9, 7, 6, 5, 4, 3,
    3, 2, 2, 1, 1
};
const uint8_t *Data[] = { Curve_00, Curve_01 };
const uint8_t Length[] = { 35, 50 };
const uint8_t Count = 2;
} // namespace Horizontal
} // namespace Roads

namespace OtherCars {
const uint8_t Length[] = { 16, 32 };
const uint8_t Count = 2;
} // namespace OtherCars
} // namespace Curves

struct Vec3 {
    int16_t x;
    int16_t y;
    int16_t z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(int16_t x, int16_t y, int16_t z) : x(x), y(y), z(z) {}

    int16_t getX() const { return x; }
    int16_t getY() const { return y; }
    int16_t getZ() const { return z; }

    void setX(int16_t v) { x = v; }
    void setY(int16_t v) { y = v; }
    void setZ(int16_t v) { z = v; }

    Vec3 operator-(const Vec3 &other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }

    Vec3 clone() const { return Vec3(x, y, z); }

    void decZ() { z = z - ZReduction; }
};

struct VecMovement {
    MovementType movementType;
    Direction direction;
    uint8_t count;
    uint8_t curveIdx;

    VecMovement(MovementType type, Direction dir, uint8_t count, uint8_t curve)
        : movementType(type), direction(dir), count(count), curveIdx(curve) {}

    int16_t getMovement() {
        switch (direction) {
            case Direction::Left: {
                uint8_t v = Curves::Roads::Horizontal::Data[curveIdx][count - 1];
                return v * UPM / 4;
            }
            case Direction::Up: {
                uint8_t v = Curves::Roads::Vertical::Data[curveIdx][count - 1];
                return v * UPM / 4;
            }
            case Direction::Right: {
                uint8_t v = Curves::Roads::Horizontal::Data[curveIdx][count - 1];
                return -v * UPM / 4;
            }
            case Direction::Down: {
                uint8_t v = Curves::Roads::Vertical::Data[curveIdx][count - 1];
                return -v * UPM / 4;
            }
            default:
                return 0;
        }
    }

    void update() {
        count--;

        if (count == 0) {
            if (movementType == MovementType::Horizontal) {
                if (direction == Direction::Left || direction == Direction::Right) {
                    direction = Direction::None;
                    count = random(3, 20);
                } else {
                    direction = random(0, 2) == 0 ? Direction::Left : Direction::Right;
                    curveIdx = random(0, Curves::Roads::Horizontal::Count);
                    count = Curves::Roads::Horizontal::Length[curveIdx];
                }
            } else {
                if (direction == Direction::Up || direction == Direction::Down) {
                    direction = Direction::None;
                    count = random(3, 20);
                } else {
                    direction = random(0, 2) == 0 ? Direction::Up : Direction::Down;
                    curveIdx = random(0, Curves::Roads::Vertical::Count);
                    count = Curves::Roads::Vertical::Length[curveIdx];
                }
            }
        }
    }
};

struct World {
    VecMovement vecHeight;
    VecMovement vecTurn;
    Vec3 roadSegments[ROAD_SEGMENTS];

    World()
        : vecHeight(MovementType::Vertical, Direction::None, 8, 0),
          vecTurn(MovementType::Horizontal, Direction::None, 8, 0) {
        reset();
    }

    void decZ() {
        for (uint8_t i = 0; i < ROAD_SEGMENTS; i++) {
            roadSegments[i].decZ();
        }
    }

    void addRoadSegment(uint16_t index, bool noCurves) {
        uint8_t modOld = index % ROAD_SEGMENTS;
        uint8_t modNew = (index + 1) % ROAD_SEGMENTS;

        Vec3 segmentOld = roadSegments[modOld];
        Vec3 segmentNew = segmentOld.clone();

        segmentNew.setZ(segmentOld.getZ() + UPM);
        segmentNew.setY(vecHeight.getMovement());

        if (!noCurves) {
            segmentNew.setX(vecTurn.getMovement());
        }

        vecHeight.update();
        vecTurn.update();

        roadSegments[modNew] = segmentNew;
    }

    Vec3 getRoadSegment(uint16_t index) const {
        return roadSegments[index % ROAD_SEGMENTS];
    }

    int16_t roadHeightAt(int16_t roadPosition) const {
        int16_t index = roadPosition / UPM;
        int16_t fract = roadPosition % UPM;

        int16_t h1 = getRoadSegment(index).getY();
        int16_t h2 = getRoadSegment(index + 1).getY();

        return h1 + ((h2 - h1) * fract) / UPM;
    }

    Vec3 perspective(Vec3 worldPosition, Vec3 cameraPosition) const {
        Vec3 relativeWorldPos = worldPosition - cameraPosition;

        if (relativeWorldPos.getZ() == 0) relativeWorldPos.setZ(1);

        Vec3 result;
        result.setX(DISPLAY_WIDTH / 2 + (relativeWorldPos.getX() * UPM) / relativeWorldPos.getZ());
        result.setY(DISPLAY_HEIGHT / 2 - (relativeWorldPos.getY() * UPM) / relativeWorldPos.getZ());
        result.setZ((64 * DISPLAY_WIDTH) / relativeWorldPos.getZ());

        return result;
    }

    void reset() {
        for (uint8_t i = 0; i < ROAD_SEGMENTS; i++) {
            roadSegments[i].setX(0);
            roadSegments[i].setY(0);
            roadSegments[i].setZ(i * UPM);
        }

        vecHeight.direction = Direction::None;
        vecHeight.count = random(5, 20);

        vecTurn.direction = Direction::None;
        vecTurn.count = random(5, 20);
    }
};

struct Car : Vec3 {
    float speed;
    bool isOffroad;
    uint8_t gear;

    Car() : Vec3(), speed(0), isOffroad(false), gear(1) {}

    uint8_t getSpeedDisplay() const {
        return static_cast<uint8_t>(speed / SpeedDiv);
    }

    uint8_t getTacho() const {
        uint16_t s = static_cast<uint16_t>(speed);

        switch (gear) {
            case 0:
                return 8;
            case 1:
                if (s <= 1023) return 1;
                if (s <= Gear1Max) return 2 + ((s - 1024) / 800);
                return 8;
            case 2:
                if (s <= 3999) return 1;
                if (s <= Gear2Max) return 2 + ((s - 4000) / 1300);
                return 8;
            case 3:
                if (s <= 7999) return 1;
                if (s <= Gear3Max) return 2 + ((s - 8000) / 2400);
                return 8;
            case 4:
                if (s <= 13999) return 1;
                if (s <= Gear4Max) return 2 + ((s - 14000) / 3000);
                return 8;
            case 5:
                if (s <= 20999) return 1;
                if (s <= Gear5Max) return 2 + ((s - 21000) / 3900);
                return 8;
            default:
                return 0;
        }
    }

    void changeSpeed(CarMovement movement) {
        uint16_t s = static_cast<uint16_t>(speed);

        switch (movement) {
            case CarMovement::Accelerate:
                if (s <= 2047) speed = 2048;
                else if (s <= 8192) speed *= 1.15f;
                else if (s <= 18000) speed *= 1.05f;
                else if (s <= 36000) speed *= 1.02f;
                else if (s <= 48000) speed *= 1.01f;
                break;

            case CarMovement::Coast:
                if (s >= 1 && s <= 1024) speed = 0;
                else speed /= 1.1f;
                break;

            case CarMovement::Decelerate:
                if (s >= 1 && s <= 1024) speed = 0;
                else speed /= 1.4f;
                break;

            case CarMovement::NoMovement:
                break;
        }
    }

    void incGear() {
        if (gear < 5) gear++;
    }

    bool decGear() {
        if (gear > 0) {
            gear--;
            if (speed > GearsMax[gear]) {
                speed = GearsMax[gear];
                return true;
            }
        }
        return false;
    }

    void reset() {
        gear = 1;
        speed = 0;
        setZ(95);
        setX(0);
        setY(UPM);
        isOffroad = false;
    }
};

struct OtherCar : Vec3 {
    float speed;
    int16_t xWorld;
    Direction direction;
    uint8_t count;
    uint8_t curveIdx;

    OtherCar() : Vec3(), speed(0), xWorld(0), direction(Direction::None),
                 count(0), curveIdx(0) {}

    uint8_t getSpeedDisplay() const {
        return static_cast<uint8_t>(speed / SpeedDiv);
    }

    int8_t getXMovement() const {
        if (count > 0) {
            if (direction == Direction::Left) return -curveIdx * 2;
            if (direction == Direction::Right) return curveIdx * 2;
        }
        return 0;
    }

    void update() {
        count--;

        if (direction == Direction::Left) setX(getX() - (curveIdx * 2));
        else if (direction == Direction::Right) setX(getX() + (curveIdx * 2));

        if (count == 0) {
            if (direction == Direction::Left || direction == Direction::Right) {
                direction = Direction::None;
                count = random(50, 200);
            } else {
                if (getX() < -20) direction = Direction::Right;
                else if (getX() < 20) direction = random(0, 2) == 0 ? Direction::Left : Direction::Right;
                else direction = Direction::Left;

                curveIdx = random(0, 2);
                count = Curves::OtherCars::Length[curveIdx];
            }
        }
    }
};

} // namespace

// =============================================================================
// Implementacion
// =============================================================================
struct RoadTripGame::Impl {
    enum State : uint8_t { ST_TITLE, ST_PLAY, ST_GAMEOVER };

    State state;
    World world;
    Car car;
    OtherCar otherCars[NumberOfOtherCars];
    Vec3 cameraPos;
    Vec3 cactii[NumberOfCactii];

    uint16_t distance;
    uint16_t oldIndexTo;
    uint16_t previousZ;
    uint16_t days;
    uint16_t timeOfDay;

    uint8_t numberOfOtherCars;
    uint8_t carsPassed;
    uint8_t showDayBannerCount;
    uint8_t brakeCount;
    Direction brakeSide;
    bool gameOver;
    uint8_t finalSpeed;

    Impl() : world(), car(), cameraPos(), distance(0), oldIndexTo(0),
             previousZ(0), days(1), timeOfDay(100), numberOfOtherCars(2),
             carsPassed(CarsToPass_Init), showDayBannerCount(NewDayBannerDelay_CountDown),
             brakeCount(0), brakeSide(Direction::None), gameOver(false),
             finalSpeed(0) {
        state = ST_TITLE;
    }

    TimeOfDay getTimeOfDay() const {
        if (timeOfDay <= 1700) return TimeOfDay::Day;
        if (timeOfDay <= 2100) return TimeOfDay::Dawn;
        if (timeOfDay <= 2800) return TimeOfDay::Night;
        return TimeOfDay::Dawn;
    }

    uint8_t getSkyPattern() const {
        if (timeOfDay <= 1400) return Sky_BX______D;
        if (timeOfDay <= 1500) return Sky_B_X_____D;
        if (timeOfDay <= 1600) return Sky_B__X____D;
        if (timeOfDay <= 1700) return Sky_B___X___D;
        if (timeOfDay <= 1800) return Sky_B____X__D;
        if (timeOfDay <= 1900) return Sky_B_____X_D;
        if (timeOfDay <= 2700) return Sky_B______XD;
        if (timeOfDay <= 2800) return Sky_B_____X_D;
        if (timeOfDay <= 2900) return Sky_B____X__D;
        if (timeOfDay <= 3000) return Sky_B___X___D;
        if (timeOfDay <= 3100) return Sky_B__X____D;
        return Sky_B_X_____D;
    }

    void resetGameVars() {
        distance = 0;
        oldIndexTo = 0;
        previousZ = 0;
        showDayBannerCount = NewDayBannerDelay_CountDown;
        days = 1;
        timeOfDay = 100;
        brakeCount = 0;
        brakeSide = Direction::None;
        carsPassed = CarsToPass_Init;
        gameOver = false;
        numberOfOtherCars = 2;
        finalSpeed = 0;
    }

    void resetRace() {
        world.reset();
        car.reset();
        resetGameVars();

        cameraPos.setX(0);
        cameraPos.setY(UPM);
        cameraPos.setZ(-40);

        for (uint8_t i = 0; i < numberOfOtherCars; i++) {
            OtherCar &otherCar = otherCars[i];
            otherCar.setX(0);
            otherCar.setZ(400 + (i * 200));
            otherCar.speed = random(7, 10) * SpeedDiv;
            otherCar.direction = Direction::None;
            otherCar.count = random(5, 20);
            otherCar.xWorld = 0;
        }

        for (uint8_t i = 1; i <= NumberOfCactii; i++) {
            cactii[i - 1].setX(i % 2);
            cactii[i - 1].setZ(400 + (i * 245));
        }
    }

    // -------------------------------------------------------------------------
    // Utilidades de dibujo directo (mismas que Arduboy2Ext del original)
    // -------------------------------------------------------------------------
    void drawFastDottedLine(int16_t startPos, int16_t endPos, int16_t y, bool &color) {
        uint16_t rowOffset = (y / 8) * 128;
        uint8_t mask = 1 << (y % 8);

        for (int16_t x = startPos; x < endPos; ++x) {
            if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
                if (color) Arduboy2Core::sBuffer[rowOffset + x] |= mask;
                else Arduboy2Core::sBuffer[rowOffset + x] &= ~mask;
            }
            color = !color;
        }
    }

    void drawFastSolidLine(int16_t startPos, int16_t endPos, int16_t y, bool color) {
        uint16_t rowOffset = (y / 8) * 128;
        uint8_t mask = 1 << (y % 8);

        for (int16_t x = startPos; x < endPos; ++x) {
            if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
                if (color) Arduboy2Core::sBuffer[rowOffset + x] |= mask;
                else Arduboy2Core::sBuffer[rowOffset + x] &= ~mask;
            }
        }
    }

    void renderRoadEdge_Black(int16_t &startPos, int16_t endPos, int16_t y, bool &) {
        drawFastSolidLine(startPos, endPos + 1, y, BLACK);
        startPos = endPos + 1;
    }

    void renderRoadEdge_Dark(int16_t &startPos, int16_t endPos, int16_t y, bool &color) {
        drawFastDottedLine(startPos, endPos + 1, y, color);
        startPos = endPos + 1;
    }

    void renderRoadEdge_Light(int16_t &startPos, int16_t endPos, int16_t y) {
        drawFastSolidLine(startPos, endPos + 1, y, WHITE);
        startPos = endPos + 1;
    }

    void renderRoad_Black(int16_t &startPos, int16_t endPos, int16_t y, bool &) {
        drawFastSolidLine(startPos, endPos, y, BLACK);
        startPos = endPos;
    }

    void renderRoad_Dither_Dark(int16_t &startPos, int16_t endPos, int16_t y,
                                bool &dither, bool &color) {
        if (dither) {
            drawFastDottedLine(startPos, endPos, y, color);
            startPos = endPos;
        } else {
            drawFastSolidLine(startPos, endPos, y, BLACK);
            startPos = endPos;
        }
    }

    void renderRoad_Dither_Light(int16_t &startPos, int16_t endPos, int16_t y, bool &color) {
        drawFastDottedLine(startPos, endPos, y, color);
        startPos = endPos;
    }

    void applySkyPattern() {
        static const uint8_t pattern[] = {
            32, 2, 136, 34, 85, 170, 119, 221, 223, 253, 0, 0, 255, 255
        };
        uint8_t idx = getSkyPattern();
        uint8_t even = pattern[idx * 2];
        uint8_t odd = pattern[idx * 2 + 1];

        for (uint16_t x = 0; x < 5 * WIDTH; x += 2) {
            Arduboy2Core::sBuffer[x + 0] = even;
            Arduboy2Core::sBuffer[x + 1] = odd;
        }
    }

    // -------------------------------------------------------------------------
    // Escena pseudo-3D
    // -------------------------------------------------------------------------
    void drawScene(bool drawOtherCars, bool noCurves) {
        (void)noCurves;
        Vec3 startPos, endPos;

        int16_t indexFrom = cameraPos.getZ() / UPM;
        int16_t indexTo = indexFrom + DRAW_DISTANCE;

        for (int16_t i = indexTo; i > indexFrom; --i) {
            Vec3 startVec = world.getRoadSegment(i);
            Vec3 endVec = world.getRoadSegment(i + 1);

            startPos = world.perspective(startVec, cameraPos);
            startPos.setY(startPos.getY() - ((i - 1) - indexFrom));
            endPos = world.perspective(endVec, cameraPos);
            endPos.setY(endPos.getY() - (i - indexFrom));

            int16_t x1Bottom = startPos.getX() - startPos.getZ() * 1.25;
            int16_t x2Bottom = x1Bottom + startPos.getZ() * 2.5;
            int16_t x1Top = endPos.getX() - endPos.getZ() * 1.25;
            int16_t x2Top = x1Top + endPos.getZ() * 2.5;

            int16_t x1Diff = x1Top - x1Bottom;
            int16_t x2Diff = x2Top - x2Bottom;

            if (i == indexTo) {
                Vec3 frontMost = world.getRoadSegment(indexFrom);
                int16_t horizonX = 64 + frontMost.getX() / 4;
                uint8_t idx = getTimeOfDay() == TimeOfDay::Day ? 0 : 1;

                for (int16_t hx = horizonX - 256; hx < WIDTH; hx += 128) {
                    Sprites::drawExternalMask(hx, endPos.getY() - 9,
                                              Rt::Horizons, Rt::Horizons_Mask,
                                              idx, idx);
                }
            }

            bool odd = i % 2;

            if (startPos.getY() > endPos.getY() &&
                startPos.getZ() > 0 && endPos.getZ() > 0) {

                int16_t segmentHeight = startPos.getY() - endPos.getY();
                int16_t y = startPos.getY();

                for (int16_t j = 0; j < segmentHeight; ++j) {
                    int16_t pos1 = x1Bottom + (j * x1Diff) / segmentHeight;
                    int16_t pos2 = x2Bottom + (j * x2Diff) / segmentHeight;

                    if (y >= DISPLAY_HEIGHT) {
                        y--;
                        continue;
                    }

                    if (pos1 < 0) pos1 = 0;
                    if (pos2 > DISPLAY_WIDTH) pos2 = DISPLAY_WIDTH;

                    bool dither = j % 2;
                    bool color = y % 2;

                    if (odd) {
                        int16_t k = 0;
                        switch (getTimeOfDay()) {
                            case TimeOfDay::Day:
                                renderRoadEdge_Light(k, pos1, y);
                                renderRoad_Dither_Light(k, pos2, y, color);
                                renderRoadEdge_Light(k, DISPLAY_WIDTH, y);
                                break;
                            case TimeOfDay::Dawn:
                                renderRoadEdge_Black(k, pos1, y, color);
                                renderRoad_Dither_Light(k, pos2, y, color);
                                renderRoadEdge_Black(k, DISPLAY_WIDTH, y, color);
                                break;
                            case TimeOfDay::Night:
                                color = y % 2;
                                renderRoadEdge_Dark(k, pos1, y, color);
                                renderRoad_Black(k, pos2, y, color);
                                renderRoadEdge_Dark(k, DISPLAY_WIDTH, y, color);
                                break;
                        }
                    } else {
                        int16_t k = 0;
                        switch (getTimeOfDay()) {
                            case TimeOfDay::Day:
                                color = y % 2;
                                renderRoadEdge_Dark(k, pos1, y, color);
                                dither = j % 2;
                                renderRoad_Dither_Dark(k, pos2, y, dither, color);
                                renderRoadEdge_Dark(k, DISPLAY_WIDTH, y, color);
                                break;
                            case TimeOfDay::Dawn:
                                color = y % 2;
                                renderRoadEdge_Dark(k, pos1, y, color);
                                dither = j % 2;
                                renderRoad_Dither_Dark(k, pos2, y, dither, color);
                                renderRoadEdge_Dark(k, DISPLAY_WIDTH, y, color);
                                break;
                            case TimeOfDay::Night:
                                renderRoadEdge_Black(k, pos1, y, color);
                                dither = j % 2;
                                renderRoad_Dither_Dark(k, pos2, y, dither, color);
                                renderRoadEdge_Black(k, DISPLAY_WIDTH, y, color);
                                break;
                        }
                    }

                    y--;
                }

                if (!odd) {
                    int16_t pos1Top = x1Bottom + x1Diff;
                    int16_t pos2Top = x2Bottom + x2Diff;

                    int16_t pBottom = (x2Bottom - x1Bottom) / 6;
                    int16_t pTop = (pos2Top - pos1Top) / 6;

                    int16_t centreTop = (x1Top + x2Top) / 2;
                    int16_t centreBottom = (x1Bottom + x2Bottom) / 2;

                    arduboy.drawLine(centreBottom - pBottom, startPos.getY(),
                                     centreTop - pTop, endPos.getY(), WHITE);
                    arduboy.drawLine(centreBottom + pBottom, startPos.getY(),
                                     centreTop + pTop, endPos.getY(), WHITE);
                }
            }

            if (drawOtherCars) {
                for (uint8_t j = 0; j < numberOfOtherCars; j++) {
                    OtherCar &otherCar = otherCars[j];

                    if (otherCar.getZ() > startVec.getZ() &&
                        otherCar.getZ() <= endVec.getZ()) {

                        uint8_t imageIndex = getTimeOfDay() == TimeOfDay::Night ? 1 : 0;

                        int16_t otherCarXWorld = (startVec.getX() + endVec.getX()) / 2;

                        Vec3 otherCarNonRef = otherCar.clone();
                        otherCarNonRef.setX(otherCar.getX() + otherCarXWorld);

                        Vec3 carPerspective = world.perspective(otherCarNonRef, cameraPos);
                        carPerspective.setY(carPerspective.getY() - (i - indexFrom));

                        int16_t distToCar = otherCar.getZ() - cameraPos.getZ();
                        uint8_t index = 0;

                        if (distToCar > -50 && distToCar < 650) {
                            if (distToCar >= 460) index = 5;
                            else if (distToCar >= 360) index = 4;
                            else if (distToCar >= 290) index = 3;
                            else if (distToCar >= 220) index = 2;
                            else if (distToCar >= 180) index = 1;

                            Sprites::drawPlusMask(
                                carPerspective.getX() - (pgm_read_byte(&Rt::OtherCar_Width[index]) / 2),
                                carPerspective.getY() - pgm_read_byte(&Rt::OtherCar_Height[index]) + 1,
                                Rt::OtherCar[index], imageIndex);
                        }
                    }
                }
            }

            for (uint8_t j = 0; j < NumberOfCactii; j++) {
                Vec3 &cactiiPos = cactii[j];

                if (cactiiPos.getZ() > startVec.getZ() &&
                    cactiiPos.getZ() <= endVec.getZ()) {

                    uint8_t cactiiIdx = 0;
                    if (i > indexFrom + 2 && i < indexFrom + 4) cactiiIdx = 4;
                    if (i >= indexFrom + 4 && i < indexFrom + 6) cactiiIdx = 3;
                    if (i >= indexFrom + 6 && i < indexFrom + 8) cactiiIdx = 2;
                    if (i >= indexFrom + 8 && i < indexFrom + 10) cactiiIdx = 1;
                    if (i >= indexFrom + 10) cactiiIdx = 0;

                    if (cactiiPos.getX() == 0) {
                        Sprites::drawPlusMask(
                            x1Bottom - 12 - (pgm_read_byte(&Rt::Cactii_Width[cactiiIdx]) / 2),
                            startPos.getY() - pgm_read_byte(&Rt::Cactii_Height[cactiiIdx]),
                            Rt::Cactii[cactiiIdx], 0);
                    } else {
                        Sprites::drawPlusMask(
                            x2Bottom + 13 - (pgm_read_byte(&Rt::Cactii_Width[cactiiIdx]) / 2),
                            startPos.getY() - pgm_read_byte(&Rt::Cactii_Height[cactiiIdx]),
                            Rt::Cactii[cactiiIdx], 0);
                    }
                }
            }

            if (odd && i > indexFrom + 2) {
                uint8_t rockIdx = 0;
                if (i > indexFrom + 2 && i < indexFrom + 4) rockIdx = 3;
                if (i >= indexFrom + 4 && i < indexFrom + 6) rockIdx = 2;
                if (i >= indexFrom + 6 && i < indexFrom + 8) rockIdx = 1;
                if (i >= indexFrom + 8) rockIdx = 0;

                int16_t spriteSize = pgm_read_byte(&Rt::Rocks_Width[rockIdx]);

                Sprites::drawPlusMask(x1Bottom - spriteSize,
                                      startPos.getY() - spriteSize,
                                      Rt::Rocks[rockIdx], 0);
                Sprites::drawPlusMask(x2Bottom,
                                      startPos.getY() - spriteSize,
                                      Rt::Rocks[rockIdx], 0);
            }
        }
    }

    // -------------------------------------------------------------------------
    // HUD y coche
    // -------------------------------------------------------------------------
    void renderPlayerCar() {
        uint8_t index = 0;
        uint8_t carSpeed = car.getSpeedDisplay();

        if (carSpeed <= 1) index = 0;
        else if (carSpeed <= 14) index = 9 - (carSpeed / 2);
        else index = 2;

        if (index > 0) {
            index = (arduboy.frameCount % index) >= (index / 2) ? 1 : 0;
        }

        car.setX(cameraPos.getX());
        car.setY(world.roadHeightAt(car.getZ()));

        Sprites::drawPlusMask(46, 64 - PlayerCarHeightPX, Rt::Player, index);

        if (brakeCount > 0) {
            uint8_t idx = (brakeCount / 2) % 3;

            if (brakeSide != Direction::Right)
                Sprites::drawPlusMask(40, 58, Rt::Dirt_Cloud_Left, idx);
            if (brakeSide != Direction::Left)
                Sprites::drawPlusMask(71, 58, Rt::Dirt_Cloud_Right, idx);
        }
    }

    void renderHud() {
        Sprites::drawExternalMask(1, 1, Rt::AutoD, Rt::AutoMask, 0, 0);
        Sprites::drawOverwrite(18, 6, Rt::AutoNumbers, car.gear - 1);

        uint8_t tacho = car.getTacho();
        const uint8_t offset = 4;

        Sprites::drawExternalMask(26 + offset, 1, Rt::Tacho, Rt::Tacho_Mask, 0, 0);

        if (tacho == 1) {
            if ((arduboy.frameCount % 32) >= 16)
                arduboy.drawRect(28 + offset, 3, 2, 5, BLACK);
        } else {
            for (uint8_t i = 0, x = 28; i < tacho; i++, x += 3) {
                if (i <= 5) arduboy.drawFastVLine(x + offset, 3, 5, BLACK);
                else arduboy.drawRect(x + offset, 3, 2, 5, BLACK);
            }
        }

        uint8_t digits[5] = {};
        uint16_t value = distance / 10;
        for (uint8_t i = 0; i < 5; i++) { digits[i] = value % 10; value /= 10; }

        for (uint8_t i = 0, x = 84; i < 5; i++, x -= 6) {
            Sprites::drawOverwrite(x, 1, Rt::numbers_white, digits[i]);
        }

        Sprites::drawOverwrite(90, 1, Rt::numbers_black, distance % 10);
        arduboy.drawFastHLine(60, 1, 37, BLACK);
        arduboy.drawFastHLine(60, 9, 37, BLACK);
        arduboy.drawFastVLine(90, 1, 8, BLACK);

        if (getTimeOfDay() == TimeOfDay::Day) {
            arduboy.drawFastVLine(96, 1, 8, BLACK);
        } else {
            arduboy.drawFastVLine(97, 2, 7, BLACK);
            arduboy.drawFastVLine(96, 2, 7, WHITE);
        }

        Sprites::drawPlusMask(104, 1, Rt::CarCounter, 0);

        if ((carsPassed > 0 && timeOfDay > 3000 && (arduboy.frameCount % 32) >= 16) ||
            timeOfDay <= 3000) {
            uint8_t carDigits[3] = {};
            value = carsPassed;
            for (uint8_t i = 0; i < 3; i++) { carDigits[i] = value % 10; value /= 10; }

            for (uint8_t i = 0, x = 118; i < 3; i++, x -= 6) {
                Sprites::drawOverwrite(x, 3, Rt::numbers_white, carDigits[i]);
            }
        }
    }

    void renderDayBanner() {
        if (showDayBannerCount == 0) return;

        uint8_t x = 0;

        Sprites::drawExternalMask(16, 8, Rt::StartOfDay, Rt::StartOfDay_Mask, 0, 0);

        if (days <= 9) x = 53;
        else if (days <= 99) x = 50;
        else x = 48;

        Sprites::drawOverwrite(x, 18, Rt::Day, 0);
        x += 18;

        uint8_t newDay = days;
        if (newDay > 100) {
            Sprites::drawOverwrite(x, 18, Rt::Font4x6, (newDay / 100) + 26);
            newDay -= (newDay / 100) * 100;
            x += 5;
        }
        if (newDay > 10) {
            Sprites::drawOverwrite(x, 18, Rt::Font4x6, (newDay / 10) + 26);
            newDay %= 10;
            x += 5;
        }
        Sprites::drawOverwrite(x, 18, Rt::Font4x6, newDay + 26);
    }

    void renderLights() {
        if (showDayBannerCount == 0) return;

        Sprites::drawExternalMask(3, 18, Rt::Lights, Rt::Lights_Mask, 0, 0);

        if (showDayBannerCount <= 19) {
            arduboy.fillRect(11, 36, 3, 3, WHITE);
        } else if (showDayBannerCount <= 49) {
            arduboy.fillRect(11, 36, 3, 3, WHITE);
        } else if (showDayBannerCount <= 79) {
            arduboy.fillRect(11, 30, 3, 3, WHITE);
        } else {
            arduboy.fillRect(11, 24, 3, 3, WHITE);
        }
    }

    // -------------------------------------------------------------------------
    // Movimiento y colisiones
    // -------------------------------------------------------------------------
    void sortCars() {
        for (uint8_t i = 0; i < numberOfOtherCars - 1; i++) {
            for (uint8_t j = 0; j < numberOfOtherCars - i - 1; j++) {
                if (otherCars[j].getZ() < otherCars[j + 1].getZ()) {
                    OtherCar temp = otherCars[j];
                    otherCars[j] = otherCars[j + 1];
                    otherCars[j + 1] = temp;
                }
            }
        }
    }

    int8_t moveCar(int16_t xMovement, int16_t zMovement) {
        uint8_t collide = NoCollision;

        for (uint8_t i = 0; i < numberOfOtherCars; i++) {
            OtherCar &otherCar = otherCars[i];

            int16_t xOverlap = (car.getX() + xMovement) -
                               (otherCar.getX() + otherCar.xWorld);
            int16_t zOverlap = (car.getZ() + zMovement) - otherCar.getZ();

            if (abs(xOverlap) < OtherCarWidthUnits &&
                abs(zOverlap) < OtherCarLengthUnits) {

                if (xMovement != 0 && abs(zOverlap) < CollisionTollerance_Z) {
                    collide = i;
                    if (!sound.playing()) sound.tone(110, 25);
                    break;
                }

                if (zMovement != 0 && abs(xOverlap) < CollisionTollerance_X) {
                    collide = i;
                    if (!sound.playing()) sound.tone(110, 25);

                    if (car.getZ() < otherCars[i].getZ() && nudgeCarForward(i)) {
                        collide = collide * -1;
                    }
                    break;
                }
            }
        }

        return collide;
    }

    bool nudgeCarForward(uint8_t carIdx) {
        OtherCar &otherCar = otherCars[carIdx];
        bool collide = false;

        int16_t otherCarZ = otherCar.getZ() + otherCar.getSpeedDisplay() + NudgeDistance;
        int16_t otherCarX = otherCar.getX() + otherCar.xWorld + otherCar.getXMovement();

        for (uint8_t i = 0; i < numberOfOtherCars; i++) {
            if (i != carIdx) {
                OtherCar &testCar = otherCars[i];
                if (abs((testCar.getX() + testCar.xWorld) - otherCarX) < OtherCarWidthUnits &&
                    abs(testCar.getZ() - otherCarZ) < OtherCarLengthUnits) {
                    collide = true;
                }
            }
        }

        if (!collide) {
            otherCar.setZ(otherCar.getZ() + NudgeDistance);
            return true;
        }
        return false;
    }

    void moveOtherCars() {
        sortCars();

        for (uint8_t i = 0; i < numberOfOtherCars; i++) {
            bool collide = false;
            OtherCar &otherCar = otherCars[i];
            int16_t otherCarX = otherCar.getX() + otherCar.xWorld + otherCar.getXMovement();

            for (uint8_t j = i; j < numberOfOtherCars; j++) {
                if (i != j) {
                    OtherCar &testCar = otherCars[j];
                    if (abs((testCar.getX() + testCar.xWorld) - otherCarX) < OtherCarWidthUnits &&
                        abs(testCar.getZ() - (otherCar.getZ() + otherCar.getSpeedDisplay())) < OtherCarLengthUnits) {
                        collide = true;
                    }
                }
            }

            if (abs(car.getX() - otherCarX) < OtherCarWidthUnits &&
                abs((car.getZ() + car.getSpeedDisplay()) -
                    (otherCar.getZ() + otherCar.getSpeedDisplay())) < OtherCarLengthUnits) {
                collide = true;
            }

            if (!collide) {
                otherCar.setZ(otherCar.getZ() + otherCar.getSpeedDisplay());

                if (otherCar.getZ() < cameraPos.getZ() - DistToRenewCars) {
                    if (numberOfOtherCars > 1) {
                        for (uint8_t j = 0; j < numberOfOtherCars; j++) {
                            if (j != i) {
                                int16_t dist = otherCar.getZ() - cameraPos.getZ();
                                if (dist > 0) otherCar.setZ(otherCars[j].getZ() + random(700 - dist, 1200 - dist));
                                else otherCar.setZ(cameraPos.getZ() + random(700, 1200));
                                break;
                            }
                        }
                    } else {
                        otherCar.setZ(cameraPos.getZ() + random(700, 1200));
                    }

                    otherCar.speed = random(9, 14) * SpeedDiv;

                    if (carsPassed > 0 && !gameOver) {
                        carsPassed--;
                        if (carsPassed > 0) sound.tone(880, 25);
                        else sound.tone(1320, 80);
                    }
                }

                otherCar.update();
            }
        }
    }

    void moveCactii() {
        for (uint8_t i = 0; i < NumberOfCactii; i++) {
            Vec3 &cactiiPos = cactii[i];
            if (cactiiPos.getZ() < cameraPos.getZ() - DistToRenewCars) {
                cactiiPos.setZ(cameraPos.getZ() + random(600, 900));
                cactiiPos.setX(random(0, 2));
            }
        }
    }

    void houseKeeping() {
        if (cameraPos.getZ() > ZMax) {
            cameraPos.decZ();
            car.decZ();
            world.decZ();

            for (uint8_t i = 0; i < numberOfOtherCars; i++) otherCars[i].decZ();
            for (uint8_t i = 0; i < NumberOfCactii; i++) cactii[i].decZ();
        }

        if (!gameOver && (cameraPos.getZ() - previousZ > 64)) {
            previousZ = cameraPos.getZ();
            distance++;
        }

        timeOfDay++;
        if (timeOfDay > 3200) timeOfDay = 0;

        if (timeOfDay == 40) {
            days++;
            if (days == 3) numberOfOtherCars++;
            if (days == 7 && numberOfOtherCars < NumberOfOtherCars) numberOfOtherCars++;

            if (carsPassed > 0) {
                gameOver = true;
                car.speed = 0;
                sound.tone(196, 200);
            } else {
                showDayBannerCount = NewDayBannerDelay;
                carsPassed = CarsToPass_Init + (days * CarsToPass_Increase);
            }
        }

        if (showDayBannerCount > 0) showDayBannerCount--;
    }

    // -------------------------------------------------------------------------
    // Actualizacion
    // -------------------------------------------------------------------------
    void updateTitle() {
        if (arduboy.justPressed(A_BUTTON)) {
            resetRace();
            state = ST_PLAY;
            sound.tone(880, 40);
        }
    }

    void updatePlay() {
        bool starting = (showDayBannerCount > 59 && days == 1);

        if (arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(A_BUTTON)) {
            if (!starting) car.incGear();
        }

        if (arduboy.justPressed(DOWN_BUTTON)) {
            if (car.decGear()) {
                brakeCount = BrakeCloud_DownGear;
                brakeSide = Direction::Both;
            }
        }

        uint8_t speed = car.getSpeedDisplay();
        uint8_t speedForSteering = SpeedSteering[speed > 30 ? 30 : speed];

        Vec3 segClosest = world.getRoadSegment(0);
        for (uint8_t i = 0; i < ROAD_SEGMENTS; i++) {
            if (world.roadSegments[i].getZ() > car.getZ()) {
                segClosest = world.roadSegments[i];
                break;
            }
        }
        int16_t carOffsetOnRoad = car.getX() - segClosest.getX();

        if (arduboy.pressed(LEFT_BUTTON) && carOffsetOnRoad > -300) {
            if (moveCar(-speedForSteering, 0) == NoCollision) {
                cameraPos.setX(cameraPos.getX() - speedForSteering);
                car.setX(car.getX() - speedForSteering);
            }
        }

        if (arduboy.pressed(RIGHT_BUTTON) && carOffsetOnRoad < 300) {
            if (moveCar(speedForSteering, 0) == NoCollision) {
                cameraPos.setX(cameraPos.getX() + speedForSteering);
                car.setX(car.getX() + speedForSteering);
            }
        }

        if (speed > 0 && brakeCount == 0) {
            if (carOffsetOnRoad < -191 || carOffsetOnRoad > 191) {
                brakeCount = BrakeCloud_OffRoad;
                brakeSide = Direction::Both;
                car.isOffroad = true;
            } else if (carOffsetOnRoad < -130) {
                brakeCount = BrakeCloud_OffRoad;
                brakeSide = Direction::Left;
                car.isOffroad = true;
            } else if (carOffsetOnRoad > 130) {
                brakeCount = BrakeCloud_OffRoad;
                brakeSide = Direction::Right;
                car.isOffroad = true;
            } else {
                car.isOffroad = false;
            }
        }

        if (starting) {
            car.speed = 0;
        } else if (arduboy.pressed(UP_BUTTON) || arduboy.pressed(A_BUTTON)) {
            if (car.speed < 1000) {
                brakeCount = BrakeCloud_Accelerate;
                brakeSide = Direction::Both;
            }
            car.changeSpeed(CarMovement::Accelerate);
        } else if (arduboy.pressed(DOWN_BUTTON)) {
            car.changeSpeed(CarMovement::Decelerate);
        } else {
            car.changeSpeed(CarMovement::Coast);
        }

        if (arduboy.frameCount % 4 == 0) {
            speed = car.getSpeedDisplay();
            int8_t collide = moveCar(0, speed);

            if (collide != NoCollision && collide >= 0) {
                OtherCar &otherCar = otherCars[abs(collide)];
                if (arduboy.pressed(A_BUTTON) || arduboy.pressed(UP_BUTTON)) {
                    car.speed = otherCar.speed;
                    otherCar.setZ(car.getZ() + PlayerCarLengthUnits);
                } else if (!car.isOffroad) {
                    car.changeSpeed(CarMovement::NoMovement);
                } else {
                    car.changeSpeed(CarMovement::Coast);
                }
            }
        }

        if (!starting || days > 1) {
            speed = car.getSpeedDisplay();
            car.setZ(car.getZ() + speed);
            cameraPos.setZ(cameraPos.getZ() + speed);
            cameraPos.setY(world.roadHeightAt(cameraPos.getZ() + 2 * UPM) + UPM);

            int16_t indexTo = cameraPos.getZ() / UPM + DRAW_DISTANCE;
            if (oldIndexTo != static_cast<uint16_t>(indexTo + 5)) {
                world.addRoadSegment(indexTo + 5, false);
                oldIndexTo = indexTo + 5;
            }

            moveOtherCars();

            for (uint8_t i = 0; i < numberOfOtherCars; i++) {
                otherCars[i].xWorld = world.getRoadSegment(otherCars[i].getZ() / UPM).getX();
                otherCars[i].setY(world.roadHeightAt(otherCars[i].getZ()));
            }
        }

        if (brakeCount > 0) brakeCount--;

        moveCactii();
        houseKeeping();

        if (gameOver && state == ST_PLAY) {
            state = ST_GAMEOVER;
            finalSpeed = car.getSpeedDisplay();
        }
    }

    void updateGameOver() {
        if (arduboy.justPressed(A_BUTTON)) {
            resetRace();
            state = ST_PLAY;
        }
    }

    void update() {
        switch (state) {
            case ST_TITLE:    updateTitle(); break;
            case ST_PLAY:     updatePlay(); break;
            case ST_GAMEOVER: updateGameOver(); break;
        }
    }

    // -------------------------------------------------------------------------
    // Dibujo
    // -------------------------------------------------------------------------
    void drawTitle() {
        drawScene(false, true);

        arduboy.fillRect(12, 14, 104, 36, BLACK);
        arduboy.drawRect(12, 14, 104, 36, WHITE);

        arduboy.setTextColor(WHITE);
        arduboy.setTextSize(2);
        arduboy.setCursor(35, 19);
        arduboy.print("ROAD TRIP");
        arduboy.setTextSize(1);
        arduboy.setCursor(28, 38);
        arduboy.print("A: EMPEZAR  B: MENU");
    }

    void drawPlay() {
        drawScene(true, false);
        renderPlayerCar();
        renderHud();

        if (showDayBannerCount > 0) {
            if (days > 1) renderDayBanner();
            else renderLights();
        }
    }

    void drawGameOver() {
        drawScene(false, true);
        renderPlayerCar();
        renderHud();

        arduboy.fillRect(14, 14, 100, 38, BLACK);
        arduboy.drawRoundRect(14, 14, 100, 38, 3, WHITE);
        arduboy.setTextColor(WHITE);
        arduboy.setTextSize(2);
        arduboy.setCursor(22, 18);
        arduboy.print("GAME OVER");
        arduboy.setTextSize(1);
        arduboy.setCursor(22, 36);
        arduboy.print("Dist: ");
        arduboy.print(distance);
        arduboy.setCursor(18, 46);
        arduboy.print("A:REINTENTAR B:MENU");
    }

    void draw() {
        applySkyPattern();
        arduboy.setTextColor(WHITE);
        arduboy.setTextSize(1);

        switch (state) {
            case ST_TITLE:    drawTitle(); break;
            case ST_PLAY:     drawPlay(); break;
            case ST_GAMEOVER: drawGameOver(); break;
        }
    }
};

// =============================================================================
// Interfaz publica
// =============================================================================
RoadTripGame::RoadTripGame() : exitRequested(false), gameOver(false) {
    impl = new Impl();
}

void RoadTripGame::init() {
    exitRequested = false;
    gameOver = false;
    impl->state = Impl::ST_TITLE;
    impl->resetGameVars();
    impl->world.reset();
    impl->cameraPos.setX(0);
    impl->cameraPos.setY(UPM);
    impl->cameraPos.setZ(0);
    impl->car.reset();
    impl->gameOver = false;
}

void RoadTripGame::update() {
    impl->update();
    gameOver = impl->gameOver;

    if (impl->state == Impl::ST_TITLE && arduboy.justPressed(B_BUTTON)) {
        exitRequested = true;
    }
    if (impl->state == Impl::ST_GAMEOVER && arduboy.justPressed(B_BUTTON)) {
        exitRequested = true;
    }
}

void RoadTripGame::draw() {
    impl->draw();
}

bool RoadTripGame::isPlaying() const {
    return impl->state == Impl::ST_PLAY && !impl->gameOver;
}
