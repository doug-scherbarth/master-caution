// src/pintest/pintest.cpp
// Standalone board bring-up diagnostic — compiled with env:pintest only.
// Reads all 14 alarm inputs + ack switch and reports state over USB serial.
// Also sweeps the three button LEDs once on startup to confirm output paths.
//
// Build & upload:  pio run -e pintest -t upload
// Monitor:         pio device monitor (115200 baud)
//
// Output: on startup, LED sweep (R→G→B, 300 ms each).
//         Then continuous input monitoring — prints a full table every 5 s
//         and an immediate line whenever any pin changes state.

#include <Arduino.h>

// --- Pin assignments (must match hal_teensy.cpp) -------------------------

static const struct {
    uint8_t     pin;
    const char *name;
    uint8_t     db15_pin;   // DB-15 #1 connector pin
} ALARM_CH[14] = {
    {  0, "CO_DETECT        ", 1  },
    {  1, "L_MAG_FAIL       ", 2  },
    {  5, "R_MAG_FAIL       ", 3  },
    {  6, "OIL_PRESS_LOW    ", 4  },
    {  8, "CHT_OVERTEMP     ", 5  },
    {  9, "EGT_OVERTEMP     ", 6  },
    { 11, "PRIM_ALT_FAIL    ", 7  },
    { 12, "SEC_ALT_FAIL     ", 8  },
    { 14, "PITOT_HEAT_FAIL  ", 9  },
    { 15, "OIL_TEMP_HIGH    ", 10 },
    { 16, "FUEL_PRESS_LOW   ", 11 },
    { 17, "FLAPS_DEPLOYED   ", 12 },
    { 22, "AIRSPEED_OVER_VFE", 13 },
    { 23, "BOOST_PUMP_ON    ", 14 },
};

#define PIN_BUTTON  18
#define PIN_LED_R   25
#define PIN_LED_G   24
#define PIN_LED_B   28

// --- Helpers -------------------------------------------------------------

static bool g_last[15];   // [0..13] = alarm channels, [14] = button

static bool read_alarm(uint8_t ch) {
    return digitalRead(ALARM_CH[ch].pin);    // active-high
}

static bool read_button(void) {
    return !digitalRead(PIN_BUTTON);          // active-low
}

static void led(uint8_t pin, bool on) {
    // cathode-sink: LOW = on, HIGH = off
    digitalWrite(pin, on ? LOW : HIGH);
}

static void print_state(uint8_t ch, bool asserted) {
    Serial.printf("  CH%02u  %-20s  pin%2u  DB15-#1 pin%2u  [%s]\n",
                  ch,
                  ALARM_CH[ch].name,
                  ALARM_CH[ch].pin,
                  ALARM_CH[ch].db15_pin,
                  asserted ? "ASSERTED" : "idle    ");
}

static void print_button(bool asserted) {
    Serial.printf("  ACK   SW_MC               pin%2u  DB15-#2 pin10  [%s]\n",
                  PIN_BUTTON,
                  asserted ? "ASSERTED" : "idle    ");
}

static void print_full_table(void) {
    Serial.println("\n--- MC input state ---");
    for (uint8_t i = 0; i < 14; i++) print_state(i, g_last[i]);
    print_button(g_last[14]);
    Serial.println("----------------------");
}

// --- Arduino entry points ------------------------------------------------

void setup(void) {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {}   // wait up to 3 s for USB enumeration

    // Configure alarm inputs
    for (uint8_t i = 0; i < 14; i++) {
        pinMode(ALARM_CH[i].pin, INPUT_PULLDOWN);
        g_last[i] = false;
    }
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    g_last[14] = false;

    // Configure LED outputs (cathode-sink, start off = HIGH)
    pinMode(PIN_LED_R, OUTPUT); digitalWrite(PIN_LED_R, HIGH);
    pinMode(PIN_LED_G, OUTPUT); digitalWrite(PIN_LED_G, HIGH);
    pinMode(PIN_LED_B, OUTPUT); digitalWrite(PIN_LED_B, HIGH);

    // LED sweep to confirm output paths
    Serial.println("\n=== MC Board Diagnostic (pintest) ===");
    Serial.println("LED sweep: R...");
    led(PIN_LED_R, true);  delay(300); led(PIN_LED_R, false);
    Serial.println("LED sweep: G...");
    led(PIN_LED_G, true);  delay(300); led(PIN_LED_G, false);
    Serial.println("LED sweep: B...");
    led(PIN_LED_B, true);  delay(300); led(PIN_LED_B, false);
    Serial.println("LED sweep done.\n");
    Serial.println("Monitoring inputs. Assert each pin (pull to GND) to verify.");
    Serial.println("Full table printed every 5 s; changes reported immediately.\n");

    print_full_table();
}

void loop(void) {
    static uint32_t last_table_ms = 0;

    // Read current state
    bool cur[15];
    for (uint8_t i = 0; i < 14; i++) cur[i] = read_alarm(i);
    cur[14] = read_button();

    // Report any changes immediately
    for (uint8_t i = 0; i < 14; i++) {
        if (cur[i] != g_last[i]) {
            Serial.printf("CHANGE ");
            print_state(i, cur[i]);
            g_last[i] = cur[i];
        }
    }
    if (cur[14] != g_last[14]) {
        Serial.printf("CHANGE ");
        print_button(cur[14]);
        g_last[14] = cur[14];
    }

    // Periodic full table
    if ((uint32_t)(millis() - last_table_ms) >= 5000) {
        print_full_table();
        last_table_ms = millis();
    }

    delay(5);   // 200 Hz poll — fast enough to catch any assertion
}
