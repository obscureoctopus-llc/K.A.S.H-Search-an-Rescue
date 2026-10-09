#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace cyd {

constexpr uint32_t TelemetryTimeoutMs = 500;
constexpr uint16_t MaxDistanceCm = 1200;
constexpr int DisplayRangeCm = 500;
constexpr int Width = 320;
constexpr int Height = 170;
constexpr int OriginX = 160;
constexpr int OriginY = 170;
constexpr int Radius = 165;
constexpr uint16_t Black = 0x0000;
constexpr uint16_t Grid = 0x7bef;
constexpr uint16_t Beam = 0x03e0;
constexpr uint16_t Green = 0x07e0;

struct Frame {
    uint8_t status = 0;
    uint16_t movingDistanceCm = 0;
    uint8_t movingEnergy = 0;
    uint8_t staticEnergy = 0;
    uint16_t staticDistanceCm = 0;
    uint16_t distanceCm = 0;
};

// R,status,moving_cm,moving_energy,static_energy,static_cm,detection_cm
inline bool parseFrame(const char* text, Frame& result) {
    if (!text || text[0] != 'R' || text[1] != ',') return false;
    const uint16_t limits[] = {3, MaxDistanceCm, 100, 100,
                               MaxDistanceCm, MaxDistanceCm};
    uint16_t fields[6] = {};
    const char* cursor = text + 2;
    for (unsigned i = 0; i < 6; ++i) {
        if (*cursor < '0' || *cursor > '9') return false;
        unsigned digits = 0;
        while (*cursor >= '0' && *cursor <= '9') {
            if (++digits > 4) return false;
            fields[i] = fields[i] * 10 + (*cursor++ - '0');
            if (fields[i] > limits[i]) return false;
        }
        if (i < 5) {
            if (*cursor++ != ',') return false;
        }
    }
    if (*cursor == '\r') ++cursor;
    if (*cursor != '\0') return false;
    Frame frame;
    frame.status = static_cast<uint8_t>(fields[0]);
    frame.movingDistanceCm = fields[1];
    frame.movingEnergy = static_cast<uint8_t>(fields[2]);
    frame.staticEnergy = static_cast<uint8_t>(fields[3]);
    frame.staticDistanceCm = fields[4];
    frame.distanceCm = fields[5];
    result = frame;
    return true;
}

class LineParser {
public:
    bool feed(uint8_t byte, uint32_t now, Frame& result) {
        if ((length_ || dropping_) &&
            static_cast<uint32_t>(now - lastByteMs_) > TelemetryTimeoutMs) {
            // An interrupted/overlong line must end before resynchronizing.
            length_ = 0;
            dropping_ = true;
        }
        lastByteMs_ = now;
        if (byte == '\n') {
            buffer_[length_] = '\0';
            const bool valid = !dropping_ && parseFrame(buffer_, result);
            reset();
            return valid;
        }
        if (dropping_) return false;
        if (byte == 0 || length_ == sizeof(buffer_) - 1) {
            length_ = 0;
            dropping_ = true;
            return false;
        }
        buffer_[length_++] = static_cast<char>(byte);
        return false;
    }

    void reset() { length_ = 0; dropping_ = false; }

private:
    char buffer_[64] = {};
    size_t length_ = 0;
    bool dropping_ = false;
    uint32_t lastByteMs_ = 0;
};

struct Point { int x; int y; };

// TFT_eSPI calibration format: x offset/span, y offset/span, swap/invert flags.
constexpr uint16_t TouchCalibration[5] = {287, 3450, 483, 3314, 1};
static_assert(TouchCalibration[1] > 0 && TouchCalibration[3] > 0,
              "Touch calibration spans must be nonzero");

inline bool calibratedTouch(int xptX, int xptY, Point& point) {
    // XPT2046 rotation 1 reports the opposite raw axis order to TFT_eSPI.
    const int rawX = TouchCalibration[4] & 1 ? xptX : xptY;
    const int rawY = TouchCalibration[4] & 1 ? xptY : xptX;
    int x = (rawX - TouchCalibration[0]) * Width / TouchCalibration[1];
    int y = (rawY - TouchCalibration[2]) * 240 / TouchCalibration[3];
    if (TouchCalibration[4] & 2) x = Width - x;
    if (TouchCalibration[4] & 4) y = 240 - y;
    if (rawX < TouchCalibration[0] || rawY < TouchCalibration[2] ||
        x < 0 || x >= Width || y < 0 || y >= 240) return false;
    point = {x, y};
    return true;
}

inline Point polarPoint(int radius, int angle) {
    const double radians = angle * 3.14159265358979323846 / 180.0;
    return {OriginX + static_cast<int>(std::lround(radius * std::cos(radians))),
            OriginY - static_cast<int>(std::lround(radius * std::sin(radians)))};
}

enum class Mode { Sonar, Hex };

class State {
public:
    Frame frame;
    Mode mode = Mode::Sonar;
    bool hasTelemetry = false;
    int sweepAngle = 20;
    int sweepDirection = 1;
    uint8_t intensity = 0;
    LineParser parser;

    bool receive(uint8_t byte, uint32_t now) {
        Frame incoming;
        if (mode != Mode::Sonar || !parser.feed(byte, now, incoming)) return false;
        frame = incoming;
        lastFrameMs_ = now;
        hasTelemetry = true;
        intensity = frame.status && frame.distanceCm > 15 ? 255 : 0;
        return true;
    }

    bool expire(uint32_t now) {
        if (!hasTelemetry ||
            static_cast<uint32_t>(now - lastFrameMs_) <= TelemetryTimeoutMs) return false;
        clearTelemetry();
        return true;
    }

    void advance() {
        sweepAngle += sweepDirection;
        if (sweepAngle >= 160 || sweepAngle <= 20) sweepDirection = -sweepDirection;
        intensity = intensity > 15 ? intensity - 15 : 0;
    }

    void switchMode() {
        mode = mode == Mode::Sonar ? Mode::Hex : Mode::Sonar;
        clearTelemetry();
        parser.reset();
        sweepAngle = 20;
        sweepDirection = 1;
    }

    bool targetVisible() const { return hasTelemetry && intensity > 0; }
    Point targetPoint() const {
        const int distance = frame.distanceCm > DisplayRangeCm ?
                             DisplayRangeCm : frame.distanceCm;
        // Range-only telemetry cannot provide a measured target bearing.
        return polarPoint(Radius * distance / DisplayRangeCm, 90);
    }

private:
    uint32_t lastFrameMs_ = 0;
    void clearTelemetry() {
        frame = Frame{};
        hasTelemetry = false;
        intensity = 0;
    }
};

// Reconstruct the whole radar viewport so no previous beam/blip pixels survive.
template <typename Canvas>
void renderRadar(Canvas& canvas, const State& state) {
    canvas.fillSprite(Black);
    for (int radius : {60, 110, 160}) {
        canvas.drawCircle(OriginX, OriginY, radius, Grid);
    }
    const Point beam = polarPoint(Radius, state.sweepAngle);
    canvas.drawLine(OriginX, OriginY, beam.x, beam.y, Beam);
    if (state.targetVisible()) {
        const Point point = state.targetPoint();
        const uint16_t color = state.frame.movingEnergy == 0 &&
                               state.frame.staticEnergy > 10 ?
                               static_cast<uint16_t>((state.intensity >> 3) << 11) :
                               static_cast<uint16_t>((state.intensity >> 2) << 5);
        canvas.fillCircle(point.x, point.y, 3, color);
    }
}

} // namespace cyd
