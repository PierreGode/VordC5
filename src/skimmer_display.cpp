#include "skimmer_display.h"

#if VORD_HAS_DISPLAY

#include "st7735.h"
#include "ble_scanner.h"

// Shared between the BLE task (writer, via skimmer_display_notify) and the
// display task (reader). Both are 32-bit aligned scalars, so plain volatile
// access is atomic on the ESP32 — no mutex needed, same as skimmer_led.
static volatile int32_t  s_lastRssi   = -100;
static volatile uint32_t s_lastSeenMs = 0;
static bool s_started = false;

static Adafruit_ST7735 s_tft(DISPLAY_PIN_CS, DISPLAY_PIN_DC, DISPLAY_PIN_RST,
                             DISPLAY_PIN_SCK, DISPLAY_PIN_MOSI);

// Logical screen size after rotation (odd rotations are landscape).
static const int16_t SCREEN_W = (DISPLAY_ROTATION & 1) ? 160 : 80;
static const int16_t SCREEN_H = (DISPLAY_ROTATION & 1) ? 80 : 160;

// Alert border: 3 px solid red + 2 px fading toward the white background.
#define BORDER_W 5

// Text scale for the count lines (5x7 font cell -> 10x14 px at scale 2).
#define TEXT_SCALE 2
#define CHAR_W (5 * TEXT_SCALE)
#define CHAR_H (7 * TEXT_SCALE)
#define CHAR_ADV (6 * TEXT_SCALE)   // 5 columns + 1 spacing column

// Vertical positions of the two centred count lines inside the border.
#define LINE1_Y 19
#define LINE2_Y 47

// Convert a logical RGB colour to the 16-bit value this panel expects. The
// ST7735 here runs with display inversion (INVON) and BGR sub-pixel order, so a
// plain RGB565 word lights up the wrong colour. This bakes both transforms in:
// invert each channel, then swap red/blue. Verified against the vendor LCD demo
// (its 0x001F "green" fill maps back through this to logical blue, as expected).
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    const uint8_t r5 = r >> 3, g6 = g >> 2, b5 = b >> 3;
    const uint8_t sr = 31 - b5, sg = 63 - g6, sb = 31 - r5;   // invert + R/B swap
    return (uint16_t)((sr << 11) | (sg << 5) | sb);
}

// writePixels/transferBytes send raw memory order; the panel wants MSB first.
static inline uint16_t swap16(uint16_t v) {
    return (uint16_t)((v << 8) | (v >> 8));
}

// ---- Tiny 5x7 font (classic Adafruit glyphs), just the characters the count
// lines use: digits, "BLE", "SKIM" and space. Each byte is one column, bit 0
// is the top row.
struct Glyph { char c; uint8_t col[5]; };
static const Glyph FONT[] = {
    {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
    {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
    {'2', {0x42, 0x61, 0x51, 0x49, 0x46}},
    {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}},
    {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
    {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
    {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}},
    {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
    {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
    {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},
    {'B', {0x7F, 0x49, 0x49, 0x49, 0x36}},
    {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
    {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}},
    {'K', {0x7F, 0x08, 0x14, 0x22, 0x41}},
    {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
    {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}},
    {'S', {0x46, 0x49, 0x49, 0x49, 0x31}},
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
};

static const uint8_t* glyphFor(char c) {
    for (size_t i = 0; i < sizeof(FONT) / sizeof(FONT[0]); i++)
        if (FONT[i].c == c) return FONT[i].col;
    return FONT[sizeof(FONT) / sizeof(FONT[0]) - 1].col;   // unknown -> blank
}

// Render one scaled character into a small buffer and push it in one bulk
// write, so text costs one SPI transaction per glyph instead of per pixel.
static void drawChar(int16_t x, int16_t y, char c, uint16_t ink, uint16_t bg) {
    const uint8_t* col = glyphFor(c);
    const uint16_t inkBe = swap16(ink), bgBe = swap16(bg);

    uint16_t buf[CHAR_W * CHAR_H];
    int n = 0;
    for (int row = 0; row < 7; row++)
        for (int sy = 0; sy < TEXT_SCALE; sy++)
            for (int ci = 0; ci < 5; ci++)
                for (int sx = 0; sx < TEXT_SCALE; sx++)
                    buf[n++] = ((col[ci] >> row) & 1) ? inkBe : bgBe;

    s_tft.setAddrWindow(x, y, x + CHAR_W - 1, y + CHAR_H - 1);
    s_tft.startWrite();
    s_tft.writePixels(buf, (uint32_t)n);
    s_tft.endWrite();
}

// Clear the line's band inside the border, then draw the text centred.
static void drawCountLine(int16_t y, const char* text, uint16_t ink, uint16_t bg) {
    s_tft.fillRect(BORDER_W, y, SCREEN_W - 2 * BORDER_W, CHAR_H, bg);
    int16_t x = (SCREEN_W - ((int16_t)strlen(text) * CHAR_ADV - TEXT_SCALE)) / 2;
    for (const char* p = text; *p; p++) {
        drawChar(x, y, *p, ink, bg);
        x += CHAR_ADV;
    }
}

// Draw (or erase, when off) the alert border: BORDER_W concentric 1-px rings,
// outer 3 solid red, inner 2 fading toward the background.
static void drawBorder(bool on, uint16_t bg) {
    static const struct { uint8_t r, g, b; } SHADES[BORDER_W] = {
        {255, 0, 0}, {255, 0, 0}, {255, 0, 0}, {255, 96, 96}, {255, 170, 170}
    };
    for (int16_t i = 0; i < BORDER_W; i++) {
        const uint16_t c = on ? rgb565(SHADES[i].r, SHADES[i].g, SHADES[i].b) : bg;
        s_tft.fillRect(i, i, SCREEN_W - 2 * i, 1, c);                      // top
        s_tft.fillRect(i, SCREEN_H - 1 - i, SCREEN_W - 2 * i, 1, c);       // bottom
        s_tft.fillRect(i, i + 1, 1, SCREEN_H - 2 * i - 2, c);              // left
        s_tft.fillRect(SCREEN_W - 1 - i, i + 1, 1, SCREEN_H - 2 * i - 2, c); // right
    }
}

// Map an RSSI to the border blink half-period (ms): closer (stronger signal,
// RSSI nearer 0) blinks faster. RSSI is clamped to the NEAR..FAR window then
// linearly interpolated across FAST..SLOW. Independent of the LED's mapping so
// the display works even when the LED feature is compiled out.
static uint32_t rssiToHalfPeriod(int rssi) {
    if (rssi > DISPLAY_RSSI_NEAR) rssi = DISPLAY_RSSI_NEAR;
    if (rssi < DISPLAY_RSSI_FAR)  rssi = DISPLAY_RSSI_FAR;

    const long span = (long)DISPLAY_RSSI_NEAR - (long)DISPLAY_RSSI_FAR;   // > 0
    const long pos  = (long)rssi - (long)DISPLAY_RSSI_FAR;                // 0..span
    return (uint32_t)((long)DISPLAY_FLASH_SLOW_MS -
        (pos * ((long)DISPLAY_FLASH_SLOW_MS - (long)DISPLAY_FLASH_FAST_MS)) / span);
}

static void skimmer_display_task(void*) {
    const uint16_t BG      = rgb565(255, 255, 255);   // white background
    const uint16_t INK     = rgb565(20, 25, 20);      // near-black count text
    const uint16_t RED_INK = rgb565(200, 0, 0);       // skimmer count once > 0

    s_tft.fillScreen(BG);

    int  lastBle = -1, lastSkim = -1;
    bool borderOn = false;

    for (;;) {
        // Session totals ("seen since boot"), same source as the dashboard.
        const int ble  = ble_scanner_session_count();
        const int skim = ble_scanner_session_skimmer_count();
        if (ble != lastBle) {
            char buf[16];
            snprintf(buf, sizeof(buf), "BLE %d", ble);
            drawCountLine(LINE1_Y, buf, INK, BG);
            lastBle = ble;
        }
        if (skim != lastSkim) {
            char buf[16];
            snprintf(buf, sizeof(buf), "SKIM %d", skim);
            drawCountLine(LINE2_Y, buf, skim > 0 ? RED_INK : INK, BG);
            lastSkim = skim;
        }

        const uint32_t age = millis() - (uint32_t)s_lastSeenMs;
        if (s_lastSeenMs != 0 && age <= DISPLAY_HOLD_MS) {
            // A flagged device is (recently) in range — blink the red border,
            // faster the closer it is. The counts stay readable in the middle.
            borderOn = !borderOn;
            drawBorder(borderOn, BG);
            vTaskDelay(pdMS_TO_TICKS(rssiToHalfPeriod((int)s_lastRssi)));
        } else {
            if (borderOn) {
                drawBorder(false, BG);
                borderOn = false;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void skimmer_display_init() {
    if (s_started) return;
    s_started = true;

    s_tft.begin();
    s_tft.setRotation(DISPLAY_ROTATION);

    pinMode(DISPLAY_PIN_BL, OUTPUT);
    digitalWrite(DISPLAY_PIN_BL, DISPLAY_BL_ON_LEVEL);   // backlight on

    // Pinned to core 0 so it coexists with the scan cycle on single-core C5.
    xTaskCreatePinnedToCore(skimmer_display_task, "skimmer_display",
                            DISPLAY_TASK_STACK, NULL, 1, NULL, 0);
}

void skimmer_display_notify(int rssi, SkimmerLedAlert type) {
    (void)type;
    s_lastRssi   = rssi;
    s_lastSeenMs = millis();
}

#endif // VORD_HAS_DISPLAY
