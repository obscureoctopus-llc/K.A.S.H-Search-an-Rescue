#include "sonar.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

struct Canvas {
    std::vector<uint16_t> pixels =
        std::vector<uint16_t>(cyd::Width * cyd::Height, cyd::Black);
    int targets = 0;
    void pixel(int x, int y, uint16_t color) {
        if (x >= 0 && x < cyd::Width && y >= 0 && y < cyd::Height)
            pixels[y * cyd::Width + x] = color;
    }
    uint16_t at(int x, int y) const { return pixels[y * cyd::Width + x]; }
    void fillSprite(uint16_t color) {
        std::fill(pixels.begin(), pixels.end(), color);
        targets = 0;
    }
    void drawLine(int x, int y, int endX, int endY, uint16_t color) {
        const int dx = std::abs(endX - x), sx = x < endX ? 1 : -1;
        const int dy = -std::abs(endY - y), sy = y < endY ? 1 : -1;
        int error = dx + dy;
        while (true) {
            pixel(x, y, color);
            if (x == endX && y == endY) break;
            const int twice = 2 * error;
            if (twice >= dy) { error += dy; x += sx; }
            if (twice <= dx) { error += dx; y += sy; }
        }
    }
    void drawCircle(int x, int y, int radius, uint16_t color) {
        for (int angle = 0; angle < 360; ++angle) {
            const double radians = angle * 3.14159265358979323846 / 180.0;
            pixel(x + static_cast<int>(std::lround(radius * std::cos(radians))),
                  y + static_cast<int>(std::lround(radius * std::sin(radians))), color);
        }
    }
    void fillCircle(int x, int y, int radius, uint16_t color) {
        ++targets;
        for (int dy = -radius; dy <= radius; ++dy)
            for (int dx = -radius; dx <= radius; ++dx)
                if (dx * dx + dy * dy <= radius * radius) pixel(x + dx, y + dy, color);
    }
};

bool packet(cyd::State& state, const std::string& text, uint32_t now) {
    bool accepted = false;
    for (unsigned char byte : text) accepted = state.receive(byte, now) || accepted;
    return accepted;
}

void parsing() {
    cyd::Frame frame;
    assert(cyd::parseFrame("R,3,1200,100,100,1200,1200\r", frame));
    assert(frame.status == 3 && frame.distanceCm == 1200 && frame.staticDistanceCm == 1200);
    const char* invalid[] = {
        "", "R", "R,", "X,1,250,50,0,0,250", "R,1,250,50,0,250",
        "R,1,250,50,0,0,250,1", "R,,250,50,0,0,250",
        "R,-1,250,50,0,0,250", "R,+1,250,50,0,0,250",
        "R,4,250,50,0,0,250", "R,1,1201,50,0,0,250",
        "R,1,250,101,0,0,250", "R,1,250,50,101,0,250",
        "R,1,250,50,0,1201,250", "R,1,250,50,0,0,1201",
        "R,1,250,50,0,0,65536", "R,1,250,50,0,0,250junk",
        "R,1,250,50,0,0, 250", "R,1,250,50,0,0,250\rjunk",
        "K.A.S.H CROWPANEL|state=PRESENCE|raw=1"
    };
    for (const char* text : invalid) {
        assert(!cyd::parseFrame(text, frame));
        assert(frame.distanceCm == 1200); // Failed parses are atomic.
    }
    assert(!cyd::parseFrame(nullptr, frame));
    cyd::State state;
    assert(!packet(state, "R,1,250,50,0,0,250", 0));
    assert(!state.hasTelemetry);
    assert(packet(state, "\n", 100));
    assert(packet(state, "R,0,0,0,0,0,0\r\n", 101));
    assert(!state.targetVisible());
    assert(!packet(state, std::string(80, '9') + "R,1,250,50,0,0,250\n", 102));
    assert(packet(state, "R,1,250,50,0,0,250\n", 103));
    std::string nul = "R,1,250";
    nul.push_back('\0');
    assert(!packet(state, nul + ",50,0,0,250\n", 104));
    assert(!packet(state, "R,1,250", 105));
    assert(!packet(state, ",50,0,0,250\n", 606));
    assert(packet(state, "R,1,250,50,0,0,250\n", 607));
}

void timeout() {
    cyd::State state;
    assert(packet(state, "R,1,250,50,0,0,250\n", 100));
    assert(!state.expire(600));
    assert(state.hasTelemetry);
    assert(!packet(state, "R,1,garbage\n", 600));
    assert(state.expire(601));
    assert(!state.targetVisible() && !state.hasTelemetry && state.frame.distanceCm == 0);
    assert(packet(state, "R,1,250,50,0,0,250\n", UINT32_MAX - 100));
    assert(!state.expire(399)); // Exactly 500 ms across rollover.
    assert(state.expire(400));
    assert(packet(state, "R,0,0,0,0,0,0\n", 401));
    assert(!state.expire(901));
    assert(state.expire(902)); // No-target frames also have a timestamp.
}

void coordinates() {
    cyd::Point touch;
    assert(cyd::calibratedTouch(287, 483, touch) && touch.x == 0 && touch.y == 0);
    assert(cyd::calibratedTouch(3112, 759, touch) && touch.x == 262 && touch.y == 19);
    assert(!cyd::calibratedTouch(0, 0, touch));
    assert(!cyd::calibratedTouch(4095, 4095, touch));
    const cyd::Point center = cyd::polarPoint(0, 90);
    assert(center.x == 160 && center.y == 170);
    for (int angle = 20; angle <= 160; ++angle) {
        const cyd::Point point = cyd::polarPoint(cyd::Radius, angle);
        assert(point.y >= 0 && point.y < cyd::Height);
        assert(point.x >= 0 && point.x < cyd::Width);
    }
    cyd::State state;
    assert(packet(state, "R,1,250,50,0,0,250\n", 0));
    assert(state.targetPoint().x == 160 && state.targetPoint().y == 88);
    assert(packet(state, "R,1,1200,50,0,0,1200\n", 1));
    assert(state.targetPoint().y == 5);
    for (int i = 0; i < 600; ++i) {
        state.advance();
        assert(state.sweepAngle >= 20 && state.sweepAngle <= 160);
    }
}

void modes() {
    cyd::State state;
    assert(packet(state, "R,1,250,50,0,0,250\nR,1,", 0));
    state.switchMode();
    assert(state.mode == cyd::Mode::Hex && !state.hasTelemetry);
    assert(!packet(state, "R,1,250,50,0,0,250\n", 1));
    state.switchMode();
    assert(state.mode == cyd::Mode::Sonar && state.sweepAngle == 20);
    assert(!packet(state, "250,50,0,0,250\n", 2));
    assert(packet(state, "R,1,250,50,0,0,250\n", 3));
}

void displayIntegration() {
    cyd::State state;
    Canvas actual, expected;
    cyd::renderRadar(actual, state);
    assert(actual.targets == 0);
    assert(actual.at(160, 88) == cyd::Black);
    assert(packet(state, "R,1,250,50,0,0,250\n", 0));
    cyd::renderRadar(actual, state);
    assert(actual.targets == 1 && actual.at(160, 88) == cyd::Green);
    for (int i = 0; i < 10; ++i) {
        state.advance();
        cyd::renderRadar(actual, state);
        assert(actual.targets == 1); // Cached range never becomes multiple targets.
        assert(state.targetPoint().x == 160 && state.targetPoint().y == 88);
    }
    assert(packet(state, "R,2,0,0,40,100,100\n", 200));
    cyd::renderRadar(actual, state);
    assert(actual.targets == 1 && actual.at(160, 137) == 0xf800);
    assert(actual.at(160, 88) == cyd::Black); // Previous range erased.
    assert(state.expire(701));
    cyd::renderRadar(actual, state);
    cyd::State idle;
    idle.sweepAngle = state.sweepAngle;
    cyd::renderRadar(expected, idle);
    assert(actual.pixels == expected.pixels && actual.targets == 0);
    for (int i = 0; i < 300; ++i) {
        state.advance();
        cyd::renderRadar(actual, state);
        cyd::State fresh;
        fresh.sweepAngle = state.sweepAngle;
        cyd::renderRadar(expected, fresh);
        assert(actual.pixels == expected.pixels); // No beam trails at reversals.
    }
    assert(packet(state, "R,1,250,50,0,0,250\n", 702));
    for (int i = 0; i < 17; ++i) state.advance();
    cyd::renderRadar(actual, state);
    assert(actual.targets == 0); // Decay cannot regenerate a cached blip.
    assert(packet(state, "R,1,250,50,0,0,250\n", 703));
    assert(packet(state, "R,0,0,0,0,0,0\n", 704));
    cyd::renderRadar(actual, state);
    assert(actual.targets == 0);
}

int main() {
    parsing(); puts("PASS: parsing");
    timeout(); puts("PASS: timeout");
    coordinates(); puts("PASS: coordinates");
    modes(); puts("PASS: modes");
    displayIntegration(); puts("PASS: display integration");
    return 0;
}
