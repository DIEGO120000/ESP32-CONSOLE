#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "Arduboy2.h"
#include "ArduboyTones.h"

// Forward references to globals declared in main.cpp
extern Arduboy2 arduboy;
extern ArduboyTones sound;
void triggerRgbLed(uint8_t r, uint8_t g, uint8_t b, uint16_t durationMs);

enum MathState : uint8_t {
    MATH_STATE_MODE_SELECT = 0,
    MATH_STATE_CAT_SELECT,
    MATH_STATE_PLAYING,
    MATH_STATE_FEEDBACK,
    MATH_STATE_GAMEOVER
};

enum MathMode : uint8_t {
    MATH_MODE_TIME_ATTACK = 0,
    MATH_MODE_CHILL = 1
};

enum MathCategory : uint8_t {
    MATH_CAT_BASIC = 0,
    MATH_CAT_ARITHMETIC = 1,
    MATH_CAT_CALCULUS = 2,
    MATH_CAT_MIX = 3
};

struct MathQuestion {
    char questionText[32];
    char options[4][16];
    uint8_t correctOption; // 0..3
};

struct MathButtonInput {
    uint8_t pin;
    bool isDown;              // Debounced state: true = held LOW (pressed)
    uint32_t lastTransition;  // Timestamp of last edge change in ms
    bool triggered;           // One-shot falling edge flag (true only for single frame on HIGH -> LOW)
};

struct MathChallengeGame {
    MathState state;
    MathMode mode;
    MathCategory category;

    int8_t modeCursor;
    int8_t catCursor;

    uint32_t score;
    uint32_t highScore;
    uint16_t currentStreak;
    uint16_t maxStreak;
    int8_t lives; // In Chill mode (starts at 3)

    // Time Attack variables
    int32_t timeLeftMs; // Starts at 10000 ms (10.0s), max 15000 ms (15.0s)
    uint32_t lastTickTime;

    // Active Question & Selection
    MathQuestion currentQ;
    int8_t playerSelection;
    bool isSelectionCorrect;
    uint32_t feedbackEndTime;

    // Pause & Exit flags
    bool isPaused;
    bool exitRequested;

    // Dedicated 1:1 Hardware Button Management with 50ms Debounce & One-Shot Falling Edge
    static const uint32_t MATH_DEBOUNCE_MS = 50;
    static const uint8_t NUM_MATH_BUTTONS = 5;
    MathButtonInput buttons[NUM_MATH_BUTTONS];

    // Anti-repetition and dynamic stochastic rotation (Fisher-Yates Bag)
    static const uint8_t BASIC_OP_COUNT = 7;
    uint8_t basicOpBag[BASIC_OP_COUNT];
    uint8_t basicBagIdx;
    int8_t lastBasicOp;
    int8_t lastMixCat;
    int8_t lastArithSubtype;
    int8_t lastCalcSubtype;

    void initOpBag() {
        for (uint8_t i = 0; i < BASIC_OP_COUNT; i++) {
            basicOpBag[i] = i;
        }
        for (int i = BASIC_OP_COUNT - 1; i > 0; i--) {
            int j = random(0, i + 1);
            uint8_t temp = basicOpBag[i];
            basicOpBag[i] = basicOpBag[j];
            basicOpBag[j] = temp;
        }
        basicBagIdx = 0;
        lastBasicOp = -1;
    }

    uint8_t getNextBasicOp() {
        if (basicBagIdx >= BASIC_OP_COUNT) {
            // Refill bag with all 7 operations
            for (uint8_t i = 0; i < BASIC_OP_COUNT; i++) {
                basicOpBag[i] = i;
            }
            // Shuffle with Fisher-Yates
            for (int i = BASIC_OP_COUNT - 1; i > 0; i--) {
                int j = random(0, i + 1);
                uint8_t temp = basicOpBag[i];
                basicOpBag[i] = basicOpBag[j];
                basicOpBag[j] = temp;
            }
            // Guarantee no consecutive repeat across bag boundaries
            if (basicOpBag[0] == lastBasicOp && BASIC_OP_COUNT > 1) {
                uint8_t swapIdx = random(1, BASIC_OP_COUNT);
                uint8_t temp = basicOpBag[0];
                basicOpBag[0] = basicOpBag[swapIdx];
                basicOpBag[swapIdx] = temp;
            }
            basicBagIdx = 0;
        }

        uint8_t op = basicOpBag[basicBagIdx++];
        lastBasicOp = op;
        return op;
    }

    void resetInputs() {
        uint32_t now = millis();
        for (uint8_t i = 0; i < NUM_MATH_BUTTONS; i++) {
            buttons[i].triggered = false;
            buttons[i].isDown = (digitalRead(buttons[i].pin) == LOW);
            buttons[i].lastTransition = now;
        }
    }

    void updateInputs() {
        uint32_t now = millis();
        for (uint8_t i = 0; i < NUM_MATH_BUTTONS; i++) {
            buttons[i].triggered = false;
            bool rawDown = (digitalRead(buttons[i].pin) == LOW);

            if (!buttons[i].isDown) {
                // Button was idle (HIGH) -> Check for falling edge (HIGH -> LOW)
                if (rawDown && (now - buttons[i].lastTransition >= MATH_DEBOUNCE_MS)) {
                    buttons[i].isDown = true;
                    buttons[i].lastTransition = now;
                    buttons[i].triggered = true; // One-shot trigger on edge
                }
            } else {
                // Button was held (LOW) -> Check for rising edge (LOW -> HIGH)
                if (!rawDown && (now - buttons[i].lastTransition >= MATH_DEBOUNCE_MS)) {
                    buttons[i].isDown = false;
                    buttons[i].lastTransition = now;
                }
            }
        }
    }

    bool btnTriggered(uint8_t idx) const {
        if (idx < NUM_MATH_BUTTONS) return buttons[idx].triggered;
        return false;
    }

    void init() {
        state = MATH_STATE_MODE_SELECT;
        modeCursor = 0;
        catCursor = 0;
        score = 0;
        currentStreak = 0;
        maxStreak = 0;
        lives = 3;
        isPaused = false;
        exitRequested = false;
        playerSelection = -1;

        // Fixed 1:1 hardware button mapping (4 D-Pad directions):
        // Direction 1 (UP - GPIO 19)    -> Recuadro [1] (options[0])
        // Direction 2 (DOWN - GPIO 20)  -> Recuadro [2] (options[1])
        // Direction 3 (LEFT - GPIO 18)  -> Recuadro [3] (options[2])
        // Direction 4 (RIGHT - GPIO 15) -> Recuadro [4] (options[3])
        buttons[0].pin = ARDUBOY_PIN_UP;     // GPIO 14 (UP)
        buttons[1].pin = ARDUBOY_PIN_DOWN;   // GPIO 18 (DOWN)
        buttons[2].pin = ARDUBOY_PIN_LEFT;   // GPIO 5  (LEFT)
        buttons[3].pin = ARDUBOY_PIN_RIGHT;  // GPIO 19 (RIGHT)
        buttons[4].pin = ARDUBOY_PIN_SELECT; // GPIO 20 (CLICK CENTRAL / OK)
        resetInputs();

        // Initialize stochastic 7-op bag & history
        initOpBag();
        lastMixCat = -1;
        lastArithSubtype = -1;
        lastCalcSubtype = -1;

        loadHighScore();
        silenceAudio();
    }

    void silenceAudio() {
        #if defined(ESP32)
        if (ARDUBOY_PIN_BUZZER >= 0) {
            ledcDetach(ARDUBOY_PIN_BUZZER);
            pinMode(ARDUBOY_PIN_BUZZER, OUTPUT);
            digitalWrite(ARDUBOY_PIN_BUZZER, LOW);
        }
        #endif
        sound.noTone();
    }

    void loadHighScore() {
        Preferences prefs;
        prefs.begin("math_nvs", true);
        highScore = prefs.getUInt("math_hi", 0);
        prefs.end();
    }

    void saveHighScore() {
        Preferences prefs;
        prefs.begin("math_nvs", false);
        uint32_t savedHi = prefs.getUInt("math_hi", 0);
        if (highScore > savedHi) {
            prefs.putUInt("math_hi", highScore);
        }
        prefs.end();
    }

    void onBootShortPress() {
        if (state == MATH_STATE_PLAYING || state == MATH_STATE_FEEDBACK) {
            isPaused = !isPaused;
            if (isPaused) {
                sound.tone(600, 30);
            } else {
                sound.tone(900, 30);
                lastTickTime = millis();
            }
        } else if (state == MATH_STATE_GAMEOVER) {
            init();
        } else if (state == MATH_STATE_MODE_SELECT) {
            exitRequested = true;
        }
    }

    void startNewGame() {
        score = 0;
        currentStreak = 0;
        maxStreak = 0;
        lives = 3;
        isPaused = false;
        playerSelection = -1;

        if (mode == MATH_MODE_TIME_ATTACK) {
            timeLeftMs = 10000; // 10 seconds initial
        } else {
            timeLeftMs = 0;
        }

        lastTickTime = millis();
        initOpBag();
        generateQuestion();
        resetInputs();
        state = MATH_STATE_PLAYING;
    }

    // --- PROCEDURAL QUESTION GENERATION ENGINE ---
    void generateQuestion() {
        yield();
        MathCategory activeCat = category;
        if (category == MATH_CAT_MIX) {
            uint8_t r;
            do {
                r = random(0, 3);
            } while (r == lastMixCat);
            lastMixCat = r;

            if (r == 0) activeCat = MATH_CAT_BASIC;
            else if (r == 1) activeCat = MATH_CAT_ARITHMETIC;
            else activeCat = MATH_CAT_CALCULUS;
        }

        switch (activeCat) {
            case MATH_CAT_BASIC:
                generateBasicQuestion();
                break;
            case MATH_CAT_ARITHMETIC:
                generateArithmeticQuestion();
                break;
            case MATH_CAT_CALCULUS:
                generateCalculusQuestion();
                break;
            default:
                generateBasicQuestion();
                break;
        }

        finalizeOptions();
    }

    // 1. BASICA (+, -, x, /, Potencias, Raices, Ecuaciones Lineales)
    void generateBasicQuestion() {
        uint8_t opType = getNextBasicOp();
        int32_t ans = 0;
        int32_t d[3];

        if (opType == 0) {
            // --- Tipo 0: Suma (A + B = ? o A + B + C = ?) ---
            if (random(0, 3) == 0) {
                // 3 operandos simples: a + b + c = ?
                int16_t a = random(6, 35);
                int16_t b = random(5, 30);
                int16_t c = random(4, 25);
                ans = a + b + c;
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d+%d+%d = ?", a, b, c);
            } else {
                // 2 operandos de 2 a 3 dígitos
                int16_t a = random(15, 99);
                int16_t b = random(12, 89);
                ans = a + b;
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d + %d = ?", a, b);
            }
            d[0] = (ans > 10) ? (ans - 10) : (ans + 10);
            d[1] = ans + (random(0, 2) == 0 ? 1 : -1);
            d[2] = ans + (random(0, 2) == 0 ? 2 : -2);
        } else if (opType == 1) {
            // --- Tipo 1: Resta (A - B = ?, sin negativos A > B) ---
            int16_t a = random(25, 130);
            int16_t b = random(6, a - 2);
            ans = a - b;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d - %d = ?", a, b);
            d[0] = ans + 10;
            d[1] = (ans > 10) ? (ans - 10) : (ans + 12);
            d[2] = ans + (random(0, 2) == 0 ? 1 : -1);
        } else if (opType == 2) {
            // --- Tipo 2: Multiplicación (Tablas 2..15 o decenas) ---
            if (random(0, 4) == 0) {
                // Por decenas: e.g. 40 x 6 o 30 x 7
                int16_t a = random(2, 10) * 10;
                int16_t b = random(3, 9);
                ans = a * b;
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d x %d = ?", a, b);
                d[0] = ans + a;
                d[1] = (ans > a) ? (ans - a) : (ans + 20);
                d[2] = (a / 10 + b) * 10;
            } else {
                // Tablas 2..15
                int16_t a = random(2, 16);
                int16_t b = random(3, 15);
                ans = a * b;
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d x %d = ?", a, b);
                d[0] = ans + a;
                d[1] = (ans > b) ? (ans - b) : (ans + a * 2);
                d[2] = a + b; // Error común de suma en vez de producto
            }
        } else if (opType == 3) {
            // --- Tipo 3: División exacta (A / B = ?) ---
            int16_t divisor = random(2, 15);
            int16_t quotient = random(2, 16);
            int16_t dividend = divisor * quotient;
            ans = quotient;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d / %d = ?", dividend, divisor);
            d[0] = (quotient > 2) ? (quotient - 1) : (quotient + 3);
            d[1] = quotient + 1;
            d[2] = (quotient > 3) ? (quotient - 2) : (quotient + 2);
        } else if (opType == 4) {
            // --- Tipo 4: Potenciación (A^2 bases 2..20 o A^3 bases 2..6) ---
            if (random(0, 4) != 0) {
                // Cuadrados: A^2
                int16_t b = random(2, 21);
                ans = b * b;
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d^2 = ?", b);
                d[0] = b * 2; // Error común: b * 2
                d[1] = (b > 2) ? ((b - 1) * (b - 1)) : ((b + 2) * (b + 2));
                d[2] = (b + 1) * (b + 1);
            } else {
                // Cubos: A^3
                int16_t b = random(2, 7);
                ans = b * b * b;
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d^3 = ?", b);
                d[0] = b * 3; // Error común: b * 3
                d[1] = b * b; // Error común: b^2
                d[2] = (b > 2) ? ((b - 1) * (b - 1) * (b - 1)) : ((b + 1) * (b + 1) * (b + 1));
            }
        } else if (opType == 5) {
            // --- Tipo 5: Radicación exacta (RAIZ(A) = ? hasta 400) ---
            int16_t r = random(2, 21);
            int32_t sq = (int32_t)r * (int32_t)r;
            ans = r;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "RAIZ(%ld) = ?", (long)sq);
            d[0] = (r > 2) ? (r - 1) : (r + 3);
            d[1] = r + 1;
            d[2] = (r > 3) ? (r - 2) : (r + 2);
        } else {
            // --- Tipo 6: Despeje lineal de X (aX + b = c o aX - b = c) ---
            if (random(0, 2) == 0) {
                int16_t a = random(2, 9);
                int16_t x = random(1, 15);
                int16_t b = random(1, 21);
                int16_t c = (a * x) + b;
                ans = x;
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%dx + %d = %d  x=?", a, b, c);
            } else {
                int16_t a = random(2, 8);
                int16_t x = random(2, 15);
                int16_t b = random(1, 16);
                int16_t c = (a * x) - b;
                ans = x;
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%dx - %d = %d  x=?", a, b, c);
            }
            d[0] = (ans > 1) ? (ans - 1) : (ans + 3);
            d[1] = ans + 1;
            d[2] = (ans > 2) ? (ans - 2) : (ans + 2);
        }

        snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "%ld", (long)ans);
        validateNumericDistractors(ans, d);
        for (int i = 0; i < 3; i++) {
            snprintf(currentQ.options[i + 1], sizeof(currentQ.options[i + 1]), "%ld", (long)d[i]);
        }
    }

    // 2. ARITMETICA (Potencias, Raíces Exactas, Operaciones Combinadas)
    void generateArithmeticQuestion() {
        uint8_t subType;
        do {
            subType = random(0, 4); // 0: Powers, 1: Exact Square Roots, 2: Combined 1, 3: Combined 2
        } while (subType == lastArithSubtype);
        lastArithSubtype = subType;

        int32_t ans = 0;
        int32_t d[3];

        if (subType == 0) {
            // Powers: b^2 (b in [2, 12]) or b^3 (b in [2, 8])
            if (random(0, 10) < 6) {
                // b^2
                int16_t b = random(2, 13);
                ans = b * b;
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d^2 = ?", b);
                d[0] = b * 2; // Common mistake: b * 2
                d[1] = (b > 2) ? ((b - 1) * (b - 1)) : ((b + 2) * (b + 2));
                d[2] = (b + 1) * (b + 1);
            } else {
                // b^3
                int16_t b = random(2, 9);
                ans = b * b * b;
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d^3 = ?", b);
                d[0] = b * 3; // Common mistake: b * 3
                d[1] = b * b; // Common mistake: b^2
                d[2] = (b > 2) ? ((b - 1) * (b - 1) * (b - 1)) : ((b + 1) * (b + 1) * (b + 1));
            }
            snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "%ld", (long)ans);
            validateNumericDistractors(ans, d);
            for (int i = 0; i < 3; i++) {
                snprintf(currentQ.options[i + 1], sizeof(currentQ.options[i + 1]), "%ld", (long)d[i]);
            }
        } else if (subType == 1) {
            // Exact Square Roots: RAIZ(r^2) = ? where r in [2, 25]
            int16_t r = random(2, 26);
            int32_t sq = (int32_t)r * (int32_t)r;
            ans = r;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "RAIZ(%ld) = ?", (long)sq);
            snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "%ld", (long)ans);
            
            d[0] = (r > 2) ? (r - 1) : (r + 3);
            d[1] = r + 1;
            d[2] = (r > 3) ? (r - 2) : (r + 2);
            validateNumericDistractors(ans, d);
            for (int i = 0; i < 3; i++) {
                snprintf(currentQ.options[i + 1], sizeof(currentQ.options[i + 1]), "%ld", (long)d[i]);
            }
        } else if (subType == 2) {
            // Combined: a + b x c OR a x b - c
            if (random(0, 2) == 0) {
                int16_t a = random(3, 16);
                int16_t b = random(2, 9);
                int16_t c = random(2, 9);
                ans = a + (b * c);
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d + %d x %d = ?", a, b, c);
                d[0] = (a + b) * c; // Precedence distractor
                d[1] = ans + b;
                d[2] = (ans > 4) ? (ans - 3) : (ans + 7);
            } else {
                int16_t a = random(3, 9);
                int16_t b = random(3, 9);
                int16_t c = random(1, (a * b) - 2);
                ans = (a * b) - c;
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d x %d - %d = ?", a, b, c);
                d[0] = a * (b - c); // Precedence distractor
                d[1] = ans + 4;
                d[2] = (ans > 5) ? (ans - 4) : (ans + 8);
            }
            snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "%ld", (long)ans);
            validateNumericDistractors(ans, d);
            for (int i = 0; i < 3; i++) {
                snprintf(currentQ.options[i + 1], sizeof(currentQ.options[i + 1]), "%ld", (long)d[i]);
            }
        } else {
            // Combined: (a + b) x c OR a x b + c x d
            if (random(0, 2) == 0) {
                int16_t a = random(2, 9);
                int16_t b = random(2, 9);
                int16_t c = random(2, 6);
                ans = (a + b) * c;
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "(%d+%d) x %d = ?", a, b, c);
                d[0] = a + (b * c); // Precedence error
                d[1] = ans + c;
                d[2] = (ans > c) ? (ans - c) : (ans + 6);
            } else {
                int16_t a = random(2, 6);
                int16_t b = random(2, 6);
                int16_t c = random(2, 6);
                int16_t dd = random(2, 6);
                ans = (a * b) + (c * dd);
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%dx%d + %dx%d = ?", a, b, c, dd);
                d[0] = (a * b) + c + dd; // Multiplied only one pair
                d[1] = ans + 5;
                d[2] = (ans > 5) ? (ans - 4) : (ans + 9);
            }
            snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "%ld", (long)ans);
            validateNumericDistractors(ans, d);
            for (int i = 0; i < 3; i++) {
                snprintf(currentQ.options[i + 1], sizeof(currentQ.options[i + 1]), "%ld", (long)d[i]);
            }
        }
    }

    // 3. CALCULO (Derivadas Polinómicas Procedurales, Trigonométricas, etc.)
    void generateCalculusQuestion() {
        uint8_t subType;
        do {
            subType = random(0, 8);
        } while (subType == lastCalcSubtype);
        lastCalcSubtype = subType;

        if (subType == 0) {
            // Procedural Polynomial Derivative: d/dx(a*x^n) where a in [2, 9], n in [2, 5]
            int16_t a = random(2, 10);
            int16_t n = random(2, 6);
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "d/dx(%dx^%d) = ?", a, n);

            int16_t coeff = a * n;
            int16_t p = n - 1;
            
            // Correct Answer: (a * n) * x^(n - 1)
            if (p == 1) snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "%dx", coeff);
            else snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "%dx^%d", coeff, p);

            // Distractor 1: Forgot to multiply by exponent -> a * x^(n - 1)
            if (p == 1) snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "%dx", a);
            else snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "%dx^%d", a, p);

            // Distractor 2: Forgot to decrease exponent -> (a * n) * x^n
            snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "%dx^%d", coeff, n);

            // Distractor 3: Added exponent instead of multiplying or different coeff
            int16_t altCoeff = a * (n + 1);
            if (altCoeff == coeff) altCoeff = (a + 1) * n;
            if (p == 1) snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "%dx", altCoeff);
            else snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "%dx^%d", altCoeff, p);
        } else if (subType == 1) {
            // d/dx(x^n)
            int16_t n = random(2, 7);
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "d/dx(x^%d) = ?", n);

            int16_t p = n - 1;
            if (p == 1) snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "%dx", n);
            else snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "%dx^%d", n, p);

            snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "x^%d", p);
            snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "%dx^%d", n, n);
            if (p == 1) snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "%d", n);
            else snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "%dx", n);
        } else if (subType == 2) {
            // d/dx(ax) or d/dx(C)
            if (random(0, 2) == 0) {
                int16_t a = random(2, 11);
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "d/dx(%dx) = ?", a);
                snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "%d", a);
                snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "%dx", a);
                snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "0");
                snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "1");
            } else {
                int16_t c = random(3, 50);
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "d/dx(%d) = ?", c);
                snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "0");
                snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "%d", c);
                snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "1");
                snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "%dx", c);
            }
        } else if (subType == 3) {
            // d/dx(x^2 + bx) or d/dx(ax^2 - bx)
            if (random(0, 2) == 0) {
                int16_t b = random(2, 10);
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "d/dx(x^2+%dx) = ?", b);
                snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "2x+%d", b);
                snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "2x");
                snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "x+%d", b);
                snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "2x^2+%d", b);
            } else {
                int16_t a = random(2, 5);
                int16_t b = random(2, 9);
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "d/dx(%dx^2-%dx) = ?", a, b);
                snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "%dx-%d", 2 * a, b);
                snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "%dx", 2 * a);
                snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "%dx-%d", a, b);
                snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "%dx^2-%d", 2 * a, b);
            }
        } else if (subType == 4) {
            // Trigonometric standard derivatives
            uint8_t t = random(0, 4);
            if (t == 0) {
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "d/dx(sin x) = ?");
                snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "cos x");
                snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "-cos x");
                snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "-sin x");
                snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "tan x");
            } else if (t == 1) {
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "d/dx(cos x) = ?");
                snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "-sin x");
                snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "sin x");
                snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "-cos x");
                snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "cos x");
            } else if (t == 2) {
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "d/dx(tan x) = ?");
                snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "sec^2 x");
                snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "sec x");
                snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "cot x");
                snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "-sec^2 x");
            } else {
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "d/dx(-sin x) = ?");
                snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "-cos x");
                snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "cos x");
                snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "sin x");
                snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "0");
            }
        } else if (subType == 5) {
            // Chain rule trig: d/dx(sin(kx)) or d/dx(cos(kx))
            int16_t k = random(2, 6);
            if (random(0, 2) == 0) {
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "d/dx(sin(%dx)) = ?", k);
                snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "%dcos(%dx)", k, k);
                snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "cos(%dx)", k);
                snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "-%dcos(%dx)", k, k);
                snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "%dsin(%dx)", k, k);
            } else {
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "d/dx(cos(%dx)) = ?", k);
                snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "-%dsin(%dx)", k, k);
                snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "%dsin(%dx)", k, k);
                snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "-sin(%dx)", k);
                snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "-%dcos(%dx)", k, k);
            }
        } else if (subType == 6) {
            // Exponential: d/dx(e^x) or d/dx(e^(kx))
            int16_t k = random(2, 6);
            if (random(0, 3) == 0) {
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "d/dx(e^x) = ?");
                snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "e^x");
                snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "x*e^(x-1)");
                snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "1/e^x");
                snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "ln x");
            } else {
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "d/dx(e^(%dx)) = ?", k);
                snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "%de^(%dx)", k, k);
                snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "e^(%dx)", k);
                snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "%de^x", k);
                snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "%de^(%dx)", k * k, k);
            }
        } else {
            // Logarithmic & Reciprocal: d/dx(ln x), d/dx(1/x)
            if (random(0, 2) == 0) {
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "d/dx(ln x) = ?");
                snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "1/x");
                snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "1/x^2");
                snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "-1/x");
                snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "e^x");
            } else {
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "d/dx(1/x) = ?");
                snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "-1/x^2");
                snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "1/x^2");
                snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "-1/x");
                snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "ln x");
            }
        }
    }

    void generateNumericDistractors(int32_t correctAns, int32_t* distArray) {
        int32_t d[3];
        int count = 0;
        int32_t pool[] = { 1, -1, 2, -2, 3, -3, 5, -5, 10, -10, 4, -4, 6, -6, 7, -7, 8, -8, 12, -12 };
        
        // Shuffle pool
        for (int i = 0; i < 20; i++) {
            int r = random(0, 20);
            int32_t tmp = pool[i];
            pool[i] = pool[r];
            pool[r] = tmp;
        }

        for (int i = 0; i < 20 && count < 3; i++) {
            int32_t cand = correctAns + pool[i];
            if (cand == correctAns) continue;
            if (cand < 0 && correctAns >= 0) continue;

            bool dup = false;
            for (int j = 0; j < count; j++) {
                if (d[j] == cand) { dup = true; break; }
            }
            if (!dup) d[count++] = cand;
        }

        int offset = 14;
        while (count < 3) {
            int32_t cand = correctAns + offset;
            bool dup = (cand == correctAns);
            for (int j = 0; j < count; j++) {
                if (d[j] == cand) { dup = true; break; }
            }
            if (!dup) d[count++] = cand;
            offset += 3;
        }

        distArray[0] = d[0];
        distArray[1] = d[1];
        distArray[2] = d[2];
    }

    void validateNumericDistractors(int32_t correctAns, int32_t* d) {
        for (int i = 0; i < 3; i++) {
            if (d[i] == correctAns || (correctAns >= 0 && d[i] < 0)) {
                d[i] = (correctAns >= 0) ? (correctAns + (i + 1) * 3 + 1) : (correctAns - (i + 1) * 3 - 1);
            }
            for (int j = 0; j < i; j++) {
                if (d[i] == d[j]) {
                    d[i] += (i + 1) * 4;
                    if (d[i] == correctAns) d[i] += 2;
                }
            }
        }
    }

    void finalizeOptions() {
        // Ensure all 4 options are distinct strings
        for (int i = 1; i < 4; i++) {
            bool isDup = false;
            if (strlen(currentQ.options[i]) == 0) isDup = true;
            for (int j = 0; j < i; j++) {
                if (strcmp(currentQ.options[i], currentQ.options[j]) == 0) {
                    isDup = true;
                    break;
                }
            }
            if (isDup) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%s+%d", currentQ.options[0], i);
                strncpy(currentQ.options[i], buf, sizeof(currentQ.options[i]) - 1);
                currentQ.options[i][sizeof(currentQ.options[i]) - 1] = '\0';
            }
        }

        // Shuffle options and assign correctOption slot (0..3)
        // Fisher-Yates shuffle that tracks the exact slot where the correct answer ends up
        uint8_t correctIdx = 0;
        for (int i = 3; i > 0; i--) {
            int j = random(0, i + 1);
            if (i != j) {
                char temp[16];
                strncpy(temp, currentQ.options[i], sizeof(temp));
                strncpy(currentQ.options[i], currentQ.options[j], sizeof(currentQ.options[i]));
                strncpy(currentQ.options[j], temp, sizeof(currentQ.options[j]));
                if (correctIdx == i) {
                    correctIdx = j;
                } else if (correctIdx == j) {
                    correctIdx = i;
                }
            }
        }
        currentQ.correctOption = correctIdx;
    }

    // --- MAIN GAME UPDATE ---
    void update() {
        yield();
        updateInputs();

        if (isPaused) {
            // In pause: Check OK (Resume), Change Mode, or BOOT handled by system
            bool resumePressed = btnTriggered(4) || arduboy.justPressed(A_BUTTON) || btnTriggered(2);
            bool changeModePressed = btnTriggered(3) || arduboy.justPressed(LEFT_BUTTON);
            if (resumePressed) {
                isPaused = false;
                sound.tone(900, 30);
                lastTickTime = millis();
                resetInputs();
            } else if (changeModePressed) {
                isPaused = false;
                init();
            }
            return;
        }

        switch (state) {
            case MATH_STATE_MODE_SELECT:
                updateModeSelect();
                break;
            case MATH_STATE_CAT_SELECT:
                updateCatSelect();
                break;
            case MATH_STATE_PLAYING:
                updatePlaying();
                break;
            case MATH_STATE_FEEDBACK:
                updateFeedback();
                break;
            case MATH_STATE_GAMEOVER:
                updateGameOver();
                break;
        }
    }

    void updateModeSelect() {
        bool upPressed = btnTriggered(0) || arduboy.justPressed(UP_BUTTON);
        bool downPressed = btnTriggered(1) || arduboy.justPressed(DOWN_BUTTON);
        bool confirmPressed = btnTriggered(4) || arduboy.justPressed(A_BUTTON) || btnTriggered(2) || arduboy.justPressed(RIGHT_BUTTON);
        bool exitPressed = arduboy.justPressed(LEFT_BUTTON);

        if (upPressed) {
            sound.tone(880, 15);
            modeCursor--;
            if (modeCursor < 0) modeCursor = 1;
        }
        if (downPressed) {
            sound.tone(700, 15);
            modeCursor++;
            if (modeCursor > 1) modeCursor = 0;
        }
        if (confirmPressed) {
            sound.tone(1760, 40);
            mode = (modeCursor == 0) ? MATH_MODE_TIME_ATTACK : MATH_MODE_CHILL;
            state = MATH_STATE_CAT_SELECT;
            catCursor = 0;
            resetInputs();
        } else if (exitPressed) {
            sound.tone(500, 30);
            exitRequested = true;
        }
    }

    void updateCatSelect() {
        bool upPressed = btnTriggered(0) || arduboy.justPressed(UP_BUTTON);
        bool downPressed = btnTriggered(1) || arduboy.justPressed(DOWN_BUTTON);
        bool confirmPressed = btnTriggered(4) || arduboy.justPressed(A_BUTTON) || btnTriggered(2) || arduboy.justPressed(RIGHT_BUTTON);
        bool backPressed = btnTriggered(3) || arduboy.justPressed(LEFT_BUTTON);

        if (upPressed) {
            sound.tone(880, 15);
            catCursor--;
            if (catCursor < 0) catCursor = 3;
        }
        if (downPressed) {
            sound.tone(700, 15);
            catCursor++;
            if (catCursor > 3) catCursor = 0;
        }
        if (confirmPressed) {
            sound.tone(1760, 50);
            category = (MathCategory)catCursor;
            startNewGame();
        } else if (backPressed) {
            sound.tone(500, 30);
            state = MATH_STATE_MODE_SELECT;
            resetInputs();
        }
    }

    void updatePlaying() {
        uint32_t now = millis();
        uint32_t dt = now - lastTickTime;
        lastTickTime = now;

        // In Time Attack: reduce time progressively
        if (mode == MATH_MODE_TIME_ATTACK) {
            timeLeftMs -= dt;
            if (timeLeftMs <= 0) {
                timeLeftMs = 0;
                triggerRgbLed(50, 0, 0, 300);
                sound.tone(200, 250);
                saveHighScore();
                state = MATH_STATE_GAMEOVER;
                return;
            }
        }

        // Fixed 1:1 hardware button to option mapping (Active LOW, Falling Edge Trigger):
        // Button 1 (GPIO 14) -> Recuadro [1] (options[0])
        // Button 2 (GPIO 0)  -> Recuadro [2] (options[1])
        // Button 3 (GPIO 7)  -> Recuadro [3] (options[2])
        // Button 4 (GPIO 18) -> Recuadro [4] (options[3])
        int8_t chosen = -1;
        if (btnTriggered(0) || arduboy.justPressed(UP_BUTTON)) {
            chosen = 0; // Box [1] -> UP (GPIO 14)
        } else if (btnTriggered(1) || arduboy.justPressed(DOWN_BUTTON)) {
            chosen = 1; // Box [2] -> DOWN (GPIO 18)
        } else if (btnTriggered(2) || arduboy.justPressed(LEFT_BUTTON)) {
            chosen = 2; // Box [3] -> LEFT (GPIO 5)
        } else if (btnTriggered(3) || arduboy.justPressed(RIGHT_BUTTON)) {
            chosen = 3; // Box [4] -> RIGHT (GPIO 19)
        }

        if (chosen >= 0) {
            playerSelection = chosen;
            if (chosen == currentQ.correctOption) {
                // Correct answer!
                isSelectionCorrect = true;
                score += 10;
                if (score > highScore) {
                    highScore = score;
                }

                if (mode == MATH_MODE_TIME_ATTACK) {
                    timeLeftMs += 3000; // +3s bonus
                    if (timeLeftMs > 15000) timeLeftMs = 15000; // Cap at 15s
                } else {
                    currentStreak++;
                    if (currentStreak > maxStreak) maxStreak = currentStreak;
                }

                // LED & Audio Feedback: Green flash 100ms, acute beep
                triggerRgbLed(0, 45, 0, 100);
                sound.tone(1900, 70);
            } else {
                // Wrong answer!
                isSelectionCorrect = false;

                if (mode == MATH_MODE_TIME_ATTACK) {
                    timeLeftMs -= 3000; // -3s penalty
                    if (timeLeftMs < 0) timeLeftMs = 0;
                } else {
                    lives--;
                    currentStreak = 0;
                }

                // LED & Audio Feedback: Red flash 150ms, grave tone
                triggerRgbLed(50, 0, 0, 150);
                sound.tone(220, 150);
            }

            feedbackEndTime = millis() + 350; // 350ms feedback display
            state = MATH_STATE_FEEDBACK;
        }
    }

    void updateFeedback() {
        if (millis() >= feedbackEndTime) {
            if (mode == MATH_MODE_TIME_ATTACK && timeLeftMs <= 0) {
                saveHighScore();
                state = MATH_STATE_GAMEOVER;
            } else if (mode == MATH_MODE_CHILL && lives <= 0) {
                saveHighScore();
                state = MATH_STATE_GAMEOVER;
            } else {
                generateQuestion();
                lastTickTime = millis();
                state = MATH_STATE_PLAYING;
            }
        }
    }

    void updateGameOver() {
        bool retryPressed = btnTriggered(4) || arduboy.justPressed(A_BUTTON) || btnTriggered(2);
        bool exitPressed = btnTriggered(3) || arduboy.justPressed(LEFT_BUTTON);

        if (retryPressed) {
            sound.tone(1760, 50);
            startNewGame();
        } else if (exitPressed) {
            sound.tone(600, 30);
            exitRequested = true;
        }
    }

    // --- DRAWING & OLED INTERFACE (128x64) ---
    void draw() {
        yield();
        switch (state) {
            case MATH_STATE_MODE_SELECT:
                drawModeSelect();
                break;
            case MATH_STATE_CAT_SELECT:
                drawCatSelect();
                break;
            case MATH_STATE_PLAYING:
            case MATH_STATE_FEEDBACK:
                drawGameScreen();
                break;
            case MATH_STATE_GAMEOVER:
                drawGameOverScreen();
                break;
        }

        if (isPaused) {
            drawPauseOverlay();
        }
    }

    void drawModeSelect() {
        // Top Banner
        arduboy.fillRect(0, 0, 128, 9, WHITE);
        arduboy.setTextColor(BLACK);
        arduboy.setCursor(16, 1);
        arduboy.print("MATH CHALLENGE");
        arduboy.setTextColor(WHITE);

        // Subheader
        arduboy.setCursor(20, 12);
        arduboy.print("MODO DE JUEGO");

        // Mode 1: TIME ATTACK
        int16_t y1 = 23;
        if (modeCursor == 0) {
            arduboy.fillRect(2, y1 - 1, 124, 15, WHITE);
            arduboy.setTextColor(BLACK);
            arduboy.setCursor(6, y1);
            arduboy.print("> TIME ATTACK");
            arduboy.setCursor(18, y1 + 7);
            arduboy.print("10s: +3s / -3s");
            arduboy.setTextColor(WHITE);
        } else {
            arduboy.setCursor(6, y1);
            arduboy.print("  TIME ATTACK");
            arduboy.setCursor(18, y1 + 7);
            arduboy.print("10s: +3s / -3s");
        }

        // Mode 2: CHILL MODE
        int16_t y2 = 39;
        if (modeCursor == 1) {
            arduboy.fillRect(2, y2 - 1, 124, 15, WHITE);
            arduboy.setTextColor(BLACK);
            arduboy.setCursor(6, y2);
            arduboy.print("> CHILL MODE");
            arduboy.setCursor(18, y2 + 7);
            arduboy.print("Sin reloj (3 vidas)");
            arduboy.setTextColor(WHITE);
        } else {
            arduboy.setCursor(6, y2);
            arduboy.print("  CHILL MODE");
            arduboy.setCursor(18, y2 + 7);
            arduboy.print("Sin reloj (3 vidas)");
        }

        // Bottom Bar
        arduboy.drawFastHLine(0, 56, 128, WHITE);
        arduboy.setCursor(2, 57);
        arduboy.print("UP/DN:Sel  OK:Entrar");
    }

    void drawCatSelect() {
        arduboy.fillRect(0, 0, 128, 9, WHITE);
        arduboy.setTextColor(BLACK);
        arduboy.setCursor(20, 1);
        arduboy.print("ELIGE CATEGORIA");
        arduboy.setTextColor(WHITE);

        const char* cats[4] = {
            "1. BASICA (+ - x / eq)",
            "2. ARITMETICA (^,RAIZ)",
            "3. CALCULO (d/dx)",
            "4. MIX TOTAL"
        };

        for (uint8_t i = 0; i < 4; i++) {
            int16_t y = 12 + (i * 11);
            if (catCursor == i) {
                arduboy.fillRect(2, y - 1, 124, 10, WHITE);
                arduboy.setTextColor(BLACK);
                arduboy.setCursor(4, y);
                arduboy.print("> ");
                arduboy.print(cats[i]);
                arduboy.setTextColor(WHITE);
            } else {
                arduboy.setCursor(4, y);
                arduboy.print("  ");
                arduboy.print(cats[i]);
            }
        }

        arduboy.drawFastHLine(0, 56, 128, WHITE);
        arduboy.setCursor(2, 57);
        arduboy.print("UP/DN:Sel  OK:Jugar");
    }

    void drawGameScreen() {
        // --- 1. TOP HUD ZONE (y = 0 to 11) ---
        if (mode == MATH_MODE_TIME_ATTACK) {
            // Score & High Score
            arduboy.setCursor(2, 0);
            arduboy.print("SC:");
            arduboy.print(score);

            arduboy.setCursor(76, 0);
            arduboy.print("HI:");
            arduboy.print(highScore);

            // Time Bar (0..15s -> max 120px)
            int16_t barW = (timeLeftMs * 120L) / 15000L;
            if (barW < 0) barW = 0;
            if (barW > 120) barW = 120;

            arduboy.drawRect(2, 8, 124, 4, WHITE);
            if (barW > 0) {
                arduboy.fillRect(4, 9, barW, 2, WHITE);
            }
        } else {
            // Chill Mode: Score, Racha (Streak), Lives
            arduboy.setCursor(2, 0);
            arduboy.print("SC:");
            arduboy.print(score);

            arduboy.setCursor(48, 0);
            arduboy.print("RA:");
            arduboy.print(currentStreak);

            arduboy.setCursor(90, 0);
            arduboy.print("V:");
            for (int8_t i = 0; i < 3; i++) {
                if (i < lives) {
                    arduboy.fillRect(104 + (i * 7), 1, 5, 6, WHITE);
                } else {
                    arduboy.drawRect(104 + (i * 7), 1, 5, 6, WHITE);
                }
            }
            arduboy.drawFastHLine(0, 8, 128, WHITE);
        }

        // --- 2. CENTER QUESTION BOX (y = 13 to 30) ---
        arduboy.drawRoundRect(2, 13, 124, 17, 2, WHITE);
        arduboy.fillRect(3, 14, 122, 15, BLACK);

        // Center question text
        uint8_t qLen = strlen(currentQ.questionText);
        int16_t qWidth = qLen * 6;
        int16_t qX = (128 - qWidth) / 2;
        if (qX < 4) qX = 4;
        arduboy.setCursor(qX, 18);
        arduboy.print(currentQ.questionText);

        // --- 3. BOTTOM 4 OPTIONS (2x2 Grid, y = 32 to 62) ---
        // Box 0 (Top-Left):     x = 2,  y = 32, w = 60, h = 14  -> Button 1 (GPIO 14)
        // Box 1 (Top-Right):    x = 66, y = 32, w = 60, h = 14  -> Button 2 (GPIO 0)
        // Box 2 (Bottom-Left):  x = 2,  y = 48, w = 60, h = 14  -> Button 3 (GPIO 7)
        // Box 3 (Bottom-Right): x = 66, y = 48, w = 60, h = 14  -> Button 4 (GPIO 18)
        const int16_t boxX[4] = {2, 66, 2, 66};
        const int16_t boxY[4] = {32, 32, 48, 48};
        const int16_t boxW = 60;
        const int16_t boxH = 14;
        const char btnPrefix[4][4] = {"1:", "2:", "3:", "4:"};

        for (uint8_t i = 0; i < 4; i++) {
            int16_t bx = boxX[i];
            int16_t by = boxY[i];

            bool isFeedback = (state == MATH_STATE_FEEDBACK);
            bool isCorrect = (i == currentQ.correctOption);
            bool isSelected = (i == playerSelection);

            if (isFeedback) {
                if (isCorrect) {
                    // Correct answer inverted in solid white
                    arduboy.fillRect(bx, by, boxW, boxH, WHITE);
                    arduboy.setTextColor(BLACK);
                } else if (isSelected && !isCorrect) {
                    // Selected wrong answer marked with double border
                    arduboy.drawRect(bx, by, boxW, boxH, WHITE);
                    arduboy.drawRect(bx + 1, by + 1, boxW - 2, boxH - 2, WHITE);
                    arduboy.setTextColor(WHITE);
                } else {
                    arduboy.drawRect(bx, by, boxW, boxH, WHITE);
                    arduboy.setTextColor(WHITE);
                }
            } else {
                arduboy.drawRect(bx, by, boxW, boxH, WHITE);
                arduboy.setTextColor(WHITE);
            }

            arduboy.setCursor(bx + 3, by + 3);
            arduboy.print(btnPrefix[i]);
            arduboy.print(currentQ.options[i]);
        }
        arduboy.setTextColor(WHITE);
    }

    void drawGameOverScreen() {
        arduboy.fillRect(0, 0, 128, 9, WHITE);
        arduboy.setTextColor(BLACK);
        arduboy.setCursor(34, 1);
        arduboy.print("GAME OVER");
        arduboy.setTextColor(WHITE);

        arduboy.setCursor(12, 13);
        arduboy.print("MODO: ");
        arduboy.print(mode == MATH_MODE_TIME_ATTACK ? "TIME ATTACK" : "CHILL MODE");

        arduboy.setCursor(12, 23);
        arduboy.print("PUNTAJE: ");
        arduboy.print(score);

        if (mode == MATH_MODE_TIME_ATTACK) {
            arduboy.setCursor(12, 33);
            arduboy.print("RECORD:  ");
            arduboy.print(highScore);

            if (score >= highScore && score > 0) {
                arduboy.fillRect(10, 43, 108, 10, WHITE);
                arduboy.setTextColor(BLACK);
                arduboy.setCursor(16, 44);
                arduboy.print("* NUEVO RECORD! *");
                arduboy.setTextColor(WHITE);
            }
        } else {
            arduboy.setCursor(12, 33);
            arduboy.print("MAX RACHA: ");
            arduboy.print(maxStreak);
        }

        arduboy.drawFastHLine(0, 56, 128, WHITE);
        arduboy.setCursor(2, 57);
        arduboy.print("OK:Reintentar LT:Menu");
    }

    void drawPauseOverlay() {
        arduboy.fillRect(12, 14, 104, 38, BLACK);
        arduboy.drawRect(12, 14, 104, 38, WHITE);
        arduboy.drawRect(14, 16, 100, 34, WHITE);

        arduboy.setCursor(46, 19);
        arduboy.print("PAUSA");
        arduboy.setCursor(22, 29);
        arduboy.print("OK: Reanudar");
        arduboy.setCursor(22, 38);
        arduboy.print("LT: Cambiar Modo");
    }
};
