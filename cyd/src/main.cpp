#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "sonar.h"

TFT_eSPI tft;
TFT_eSprite radar(&tft);
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(33);
cyd::State state;
uint32_t lastSweepMs = 0;
uint32_t lastTouchMs = 0;
bool touchHeld = false;
int consoleX = 10;
int consoleY = 70;

void drawMode() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(state.mode == cyd::Mode::Sonar ? "KASH RANGE" : "60G RAW HEX", 10, 10, 2);
    tft.drawRect(210, 5, 105, 30, TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(state.mode == cyd::Mode::Sonar ? "GO TO 60G" : "GO TO 24G", 262, 20, 2);
    tft.setTextDatum(TL_DATUM);
    consoleX = 10;
    consoleY = 70;
}

void drawSonar() {
    cyd::renderRadar(radar, state);
    radar.pushSprite(0, 70);
    char metricsText[32] = {};
    if (state.hasTelemetry) {
        snprintf(metricsText, sizeof(metricsText), "%3ucm  %.1fm",
                 static_cast<unsigned int>(state.frame.distanceCm),
                 static_cast<double>(state.frame.distanceCm) / 100.0);
    } else {
        snprintf(metricsText, sizeof(metricsText), "NO TELEMETRY");
    }
    tft.fillRect(0, 36, 320, 34, TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(metricsText, 10, 40, 4);
}

void setup() {
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, 16, 17);
    tft.init();
    tft.setRotation(1);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
    touchSPI.begin(25, 39, 32, 33);
    touch.begin(touchSPI);
    touch.setRotation(1);
    radar.setColorDepth(8);
    if (!radar.createSprite(cyd::Width, cyd::Height)) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("Radar buffer allocation failed", 10, 40, 2);
        while (true) delay(1000);
    }
    drawMode();
    drawSonar();
}

void loop() {
    const uint32_t now = millis();
    const bool touched = touch.touched();
    cyd::Point position = {};
    bool inButton = false;
    if (touched) {
        const TS_Point raw = touch.getPoint();
        inButton = cyd::calibratedTouch(raw.x, raw.y, position) &&
                   position.x >= 210 && position.x < 315 &&
                   position.y >= 5 && position.y < 35;
    }
    if (touched && !touchHeld && now - lastTouchMs > 350 && inButton) {
        lastTouchMs = now;
        state.switchMode();
        // Discard queued bytes from the previous mode, including partial lines.
        const int queued = Serial2.available();
        for (int i = 0; i < queued; ++i) Serial2.read();
        drawMode();
        if (state.mode == cyd::Mode::Sonar) drawSonar();
    }
    touchHeld = touched;

    // Bound work per loop even if the UART continuously receives garbage.
    for (int i = 0; i < 128 && Serial2.available(); ++i) {
        const uint8_t byte = static_cast<uint8_t>(Serial2.read());
        if (state.mode == cyd::Mode::Sonar) {
            state.receive(byte, millis());
        } else {
            char debugChar[4] = {};
            snprintf(debugChar, sizeof(debugChar), "%02X ",
                     static_cast<unsigned int>(byte));
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.drawString(debugChar, consoleX, consoleY, 2);
            consoleX += 26;
            if (consoleX > 290) {
                consoleX = 10;
                consoleY += 16;
                if (consoleY > 220) {
                    tft.fillRect(0, 70, 320, 170, TFT_BLACK);
                    consoleY = 70;
                }
            }
        }
    }
    const uint32_t renderNow = millis();
    const bool expired = state.expire(renderNow);
    if (state.mode == cyd::Mode::Sonar &&
        (expired || renderNow - lastSweepMs >= 20)) {
        lastSweepMs = renderNow;
        drawSonar();
        state.advance();
    }
}
