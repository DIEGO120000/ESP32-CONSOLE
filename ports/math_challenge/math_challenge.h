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
    // BASICA: solo el nucleo permitido (+ - x / y fracciones simples)
    static const uint8_t BASIC_OP_COUNT = 5;
    uint8_t basicOpBag[BASIC_OP_COUNT];
    uint8_t basicBagIdx;
    int8_t lastBasicOp;

    // ARITMETICA: nucleo completo + potencias + raices + despeje de x
    static const uint8_t ARITH_OP_COUNT = 8;
    uint8_t arithOpBag[ARITH_OP_COUNT];
    uint8_t arithBagIdx;
    int8_t lastArithOp;

    int8_t lastMixCat;
    int8_t lastCalcSubtype;
    int8_t lastFracSubtype;
    int8_t lastPowerKind;
    int8_t lastRootKind;
    int8_t lastSolveKind;

    void shuffleOpBag(uint8_t* bag, uint8_t count) {
        for (uint8_t i = 0; i < count; i++) {
            bag[i] = i;
        }
        for (int i = count - 1; i > 0; i--) {
            int j = random(0, i + 1);
            uint8_t temp = bag[i];
            bag[i] = bag[j];
            bag[j] = temp;
        }
    }

    uint8_t drawFromBag(uint8_t* bag, uint8_t& idx, int8_t& last, uint8_t count) {
        if (idx >= count) {
            shuffleOpBag(bag, count);
            // Sin repeticion consecutiva entre vueltas de bolsa
            if (count > 1 && bag[0] == last) {
                uint8_t swapIdx = random(1, count);
                uint8_t temp = bag[0];
                bag[0] = bag[swapIdx];
                bag[swapIdx] = temp;
            }
            idx = 0;
        }
        uint8_t op = bag[idx++];
        last = op;
        return op;
    }

    void initOpBag() {
        shuffleOpBag(basicOpBag, BASIC_OP_COUNT);
        basicBagIdx = 0;
        lastBasicOp = -1;
        shuffleOpBag(arithOpBag, ARITH_OP_COUNT);
        arithBagIdx = 0;
        lastArithOp = -1;
    }

    uint8_t getNextBasicOp() {
        return drawFromBag(basicOpBag, basicBagIdx, lastBasicOp, BASIC_OP_COUNT);
    }

    uint8_t getNextArithOp() {
        return drawFromBag(arithOpBag, arithBagIdx, lastArithOp, ARITH_OP_COUNT);
    }

    // Subtipo aleatorio sin repetir el anterior (evita rachas del mismo tipo)
    uint8_t nextVariant(uint8_t count, int8_t& last) {
        if (count < 2) return 0;
        uint8_t v;
        do {
            v = random(0, count);
        } while (v == (uint8_t)last);
        last = v;
        return v;
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

        // Mapeo 1:1 de la cruceta (respuesta instantanea, sin cursor):
        // ARRIBA -> Recuadro [1], ABAJO -> Recuadro [2]
        // DERECHA -> Recuadro [3], IZQUIERDA -> Recuadro [4]
        buttons[0].pin = ARDUBOY_PIN_UP;     // GPIO 14 (UP)
        buttons[1].pin = ARDUBOY_PIN_DOWN;   // GPIO 18 (DOWN)
        buttons[2].pin = ARDUBOY_PIN_LEFT;   // GPIO 5  (LEFT)
        buttons[3].pin = ARDUBOY_PIN_RIGHT;  // GPIO 19 (RIGHT)
        buttons[4].pin = ARDUBOY_PIN_SELECT; // GPIO 20 (CLICK CENTRAL / OK)
        resetInputs();

        // Initialize stochastic op bags & history
        initOpBag();
        lastMixCat = -1;
        lastCalcSubtype = -1;
        lastFracSubtype = -1;
        lastPowerKind = -1;
        lastRootKind = -1;
        lastSolveKind = -1;

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

    // --- Helpers de formato y empaquetado de opciones ---
    void emitNumericAnswer(int32_t ans, int32_t* d) {
        validateNumericDistractors(ans, d);
        snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "%ld", (long)ans);
        for (int i = 0; i < 3; i++) {
            snprintf(currentQ.options[i + 1], sizeof(currentQ.options[i + 1]), "%ld", (long)d[i]);
        }
    }

    static int32_t gcdAbs(int32_t a, int32_t b) {
        if (a < 0) a = -a;
        if (b < 0) b = -b;
        while (b != 0) {
            int32_t t = a % b;
            a = b;
            b = t;
        }
        return (a == 0) ? 1 : a;
    }

    void formatFraction(int32_t num, int32_t den, char* out, size_t n) {
        if (den == 0) den = 1;
        if (den < 0) {
            num = -num;
            den = -den;
        }
        int32_t g = gcdAbs(num, den);
        num /= g;
        den /= g;
        if (den == 1) snprintf(out, n, "%ld", (long)num);
        else snprintf(out, n, "%ld/%ld", (long)num, (long)den);
    }

    void pushFractionOption(uint8_t slot, const char* text) {
        snprintf(currentQ.options[slot], sizeof(currentQ.options[slot]), "%s", text);
    }

    bool fractionOptionUsed(const char* text, uint8_t placed) {
        for (uint8_t j = 0; j < placed; j++) {
            if (strcmp(currentQ.options[j + 1], text) == 0) return true;
        }
        return false;
    }

    // Respuesta fraccionaria + 3 distractores plausibles y distintos
    void emitFractionAnswer(int32_t num, int32_t den, int32_t n1, int32_t d1,
                            int32_t n2, int32_t d2) {
        char correct[16];
        formatFraction(num, den, correct, sizeof(correct));
        pushFractionOption(0, correct);

        char cand[6][16];
        formatFraction(n1 + n2, d1 + d2, cand[0], sizeof(cand[0]));          // suma de partes
        formatFraction(n1 * n2, d1 * d2, cand[1], sizeof(cand[1]));          // producto
        formatFraction(n1 * d2 + n2 * d1, d1 * d2, cand[2], sizeof(cand[2]));// suma sin reducir
        formatFraction(num + den, den, cand[3], sizeof(cand[3]));
        formatFraction(num, den + 1, cand[4], sizeof(cand[4]));
        formatFraction(n1 * d2 - n2 * d1, d1 * d2, cand[5], sizeof(cand[5]));// resta de numeradores

        uint8_t placed = 0;
        for (uint8_t c = 0; c < 6 && placed < 3; c++) {
            if (strcmp(cand[c], correct) == 0) continue;
            if (fractionOptionUsed(cand[c], placed)) continue;
            pushFractionOption(placed + 1, cand[c]);
            placed++;
        }

        int32_t bump = 1;
        while (placed < 3) {
            char extra[16];
            formatFraction(num + bump, den + 1, extra, sizeof(extra));
            if (strcmp(extra, correct) != 0 && !fractionOptionUsed(extra, placed)) {
                pushFractionOption(placed + 1, extra);
                placed++;
            }
            bump += 3;
        }
    }

    // Fracciones simples: 0=suma, 1=resta, 2=producto, 3=division
    void generateFractionQuestion(uint8_t ft) {
        int32_t d1 = random(2, 7);
        int32_t d2 = (random(0, 2) == 0) ? d1 : random(2, 7);
        int32_t n1 = random(1, d1);
        int32_t n2 = random(1, d2);
        int32_t num;
        int32_t den;

        if (ft == 0) {
            num = n1 * d2 + n2 * d1;
            den = d1 * d2;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText),
                     "%ld/%ld + %ld/%ld = ?", (long)n1, (long)d1, (long)n2, (long)d2);
        } else if (ft == 1) {
            int32_t diff = n1 * d2 - n2 * d1;
            if (diff < 0) {
                int32_t t = n1; n1 = n2; n2 = t;
                t = d1; d1 = d2; d2 = t;
                diff = -diff;
            }
            num = diff;
            den = d1 * d2;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText),
                     "%ld/%ld - %ld/%ld = ?", (long)n1, (long)d1, (long)n2, (long)d2);
        } else if (ft == 2) {
            num = n1 * n2;
            den = d1 * d2;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText),
                     "%ld/%ld x %ld/%ld = ?", (long)n1, (long)d1, (long)n2, (long)d2);
        } else {
            num = n1 * d2;
            den = d1 * n2;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText),
                     "%ld/%ld : %ld/%ld = ?", (long)n1, (long)d1, (long)n2, (long)d2);
        }

        emitFractionAnswer(num, den, n1, d1, n2, d2);
    }

    // NUCLEO DE BASICA: 0=suma, 1=resta, 2=multiplicacion, 3=division exacta, 4=fracciones
    void generateCoreQuestion(uint8_t opType) {
        int32_t ans = 0;
        int32_t d[3];

        if (opType == 0) {
            // --- Suma de enteros (2 o 3 operandos, hasta 3 digitos) ---
            if (random(0, 3) == 0) {
                int16_t a = random(6, 35);
                int16_t b = random(5, 30);
                int16_t c = random(4, 25);
                ans = a + b + c;
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d+%d+%d = ?", a, b, c);
            } else {
                int16_t a = random(15, 99);
                int16_t b = random(12, 89);
                ans = a + b;
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d + %d = ?", a, b);
            }
            d[0] = (ans > 10) ? (ans - 10) : (ans + 10);
            d[1] = ans + (random(0, 2) == 0 ? 1 : -1);
            d[2] = ans + (random(0, 2) == 0 ? 2 : -2);
        } else if (opType == 1) {
            // --- Resta de enteros (sin negativos: A > B) ---
            int16_t a = random(25, 130);
            int16_t b = random(6, a - 2);
            ans = a - b;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d - %d = ?", a, b);
            d[0] = ans + 10;
            d[1] = (ans > 10) ? (ans - 10) : (ans + 12);
            d[2] = ans + (random(0, 2) == 0 ? 1 : -1);
        } else if (opType == 2) {
            // --- Multiplicacion (tablas 2..15 o decenas) ---
            if (random(0, 4) == 0) {
                int16_t a = random(2, 10) * 10;
                int16_t b = random(3, 9);
                ans = a * b;
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d x %d = ?", a, b);
                d[0] = ans + a;
                d[1] = (ans > a) ? (ans - a) : (ans + 20);
                d[2] = (a / 10 + b) * 10;
            } else {
                int16_t a = random(2, 16);
                int16_t b = random(3, 15);
                ans = a * b;
                snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d x %d = ?", a, b);
                d[0] = ans + a;
                d[1] = (ans > b) ? (ans - b) : (ans + a * 2);
                d[2] = a + b; // Error comun: sumar en vez de multiplicar
            }
        } else if (opType == 3) {
            // --- Division entera exacta ---
            int16_t divisor = random(2, 15);
            int16_t quotient = random(2, 16);
            int16_t dividend = divisor * quotient;
            ans = quotient;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d / %d = ?", dividend, divisor);
            d[0] = (quotient > 2) ? (quotient - 1) : (quotient + 3);
            d[1] = quotient + 1;
            d[2] = (quotient > 3) ? (quotient - 2) : (quotient + 2);
        } else {
            // --- Fracciones simples (suma, resta, producto, division) ---
            uint8_t ft = nextVariant(4, lastFracSubtype);
            generateFractionQuestion(ft);
            return; // las fracciones emiten sus propias 4 opciones
        }

        emitNumericAnswer(ans, d);
    }

    // POTENCIACION BASICA (solo ARITMETICA): x^2, x^3, 2^n
    void generatePowerQuestion() {
        int32_t ans = 0;
        int32_t d[3];
        uint8_t kind = nextVariant(3, lastPowerKind);

        if (kind == 0) {
            int16_t b = random(2, 13);
            ans = b * b;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d^2 = ?", b);
            d[0] = b * 2; // Error comun: b * 2
            d[1] = (b > 2) ? ((b - 1) * (b - 1)) : ((b + 2) * (b + 2));
            d[2] = (b + 1) * (b + 1);
        } else if (kind == 1) {
            int16_t b = random(2, 9);
            ans = b * b * b;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%d^3 = ?", b);
            d[0] = b * 3; // Error comun: b * 3
            d[1] = b * b; // Error comun: b^2
            d[2] = (b > 2) ? ((b - 1) * (b - 1) * (b - 1)) : ((b + 1) * (b + 1) * (b + 1));
        } else {
            int16_t n = random(2, 8); // 2..7
            ans = (int32_t)1 << n;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "2^%d = ?", n);
            d[0] = n * 2;
            d[1] = (int32_t)1 << (n - 1);
            d[2] = ((int32_t)1 << (n + 1)) - 4;
        }

        emitNumericAnswer(ans, d);
    }

    // RADICACION EXACTA (solo ARITMETICA): raices cuadradas y cubicas
    void generateRootQuestion() {
        int32_t ans = 0;
        int32_t d[3];
        uint8_t kind = nextVariant(2, lastRootKind);

        if (kind == 0) {
            int16_t r = random(2, 21); // 2..20
            int32_t sq = (int32_t)r * (int32_t)r;
            ans = r;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "RAIZ(%ld) = ?", (long)sq);
        } else {
            int16_t r = random(2, 11); // 2..10
            int32_t cb = (int32_t)r * (int32_t)r * (int32_t)r;
            ans = r;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "RAIZ3(%ld) = ?", (long)cb);
        }

        d[0] = (ans > 2) ? (ans - 1) : (ans + 3);
        d[1] = ans + 1;
        d[2] = (ans > 3) ? (ans - 2) : (ans + 2);
        emitNumericAnswer(ans, d);
    }

    // DESPEJE BASICO (solo ARITMETICA): ax+b=c, ax-b=c, x/a=b
    void generateSolveXQuestion() {
        int32_t ans = 0;
        int32_t d[3];
        uint8_t kind = nextVariant(3, lastSolveKind);

        if (kind == 0) {
            int16_t a = random(2, 9);
            int16_t x = random(1, 15);
            int16_t b = random(1, 21);
            int16_t c = (a * x) + b;
            ans = x;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%dx + %d = %d  x=?", a, b, c);
        } else if (kind == 1) {
            int16_t a = random(2, 8);
            int16_t x = random(2, 15);
            int16_t maxB = (a * x) - 1;
            if (maxB < 1) maxB = 1;
            int16_t b = random(1, maxB + 1);
            int16_t c = (a * x) - b;
            ans = x;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "%dx - %d = %d  x=?", a, b, c);
        } else {
            int16_t a = random(2, 8);
            int16_t b = random(2, 13);
            ans = (int32_t)a * b;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "x/%d = %d  x=?", a, b);
        }

        d[0] = (ans > 1) ? (ans - 1) : (ans + 3);
        d[1] = ans + 1;
        d[2] = (ans > 2) ? (ans - 2) : (ans + 2);
        emitNumericAnswer(ans, d);
    }

    // 1. BASICA: EXCLUSIVAMENTE + - x / enteras y fracciones simples.
    //    Prohibido: potencias, raices, despeje de x, calculo.
    void generateBasicQuestion() {
        generateCoreQuestion(getNextBasicOp());
    }

    // 2. ARITMETICA: todo lo de BASICA + potencias + raices + despeje de x.
    void generateArithmeticQuestion() {
        uint8_t op = getNextArithOp();
        if (op <= 4) generateCoreQuestion(op);
        else if (op == 5) generatePowerQuestion();
        else if (op == 6) generateRootQuestion();
        else generateSolveXQuestion();
    }

    // 3. CALCULO (Derivadas Polinómicas Procedurales, Trigonométricas, etc.)
    void generateCalculusQuestion() {
        uint8_t subType;
        do {
            subType = random(0, 14);
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
        } else if (subType == 8) {
            // LIMITE polinomico: lim x->a (x^2 + b)
            int16_t a = random(1, 5);
            int16_t b = random(1, 10);
            int32_t ans = (int32_t)a * a + b;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "lim x->%d x^2+%d = ?", a, b);

            int32_t d[3];
            d[0] = ans + 2;
            d[1] = (ans > 3) ? (ans - 2) : (ans + 4);
            d[2] = ((int32_t)a * a) - b; // Error: restar el termino constante
            if (d[2] < 0) d[2] = ans + 6;
            emitNumericAnswer(ans, d);
        } else if (subType == 9) {
            // LIMITE lineal: lim x->a (bx + c)
            int16_t a = random(1, 5);
            int16_t b = random(2, 7);
            int16_t c = random(1, 12);
            int32_t ans = (int32_t)a * b + c;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "lim x->%d %dx+%d = ?", a, b, c);

            int32_t d[3];
            d[0] = (int32_t)a * b; // Error: olvidar el termino constante
            d[1] = ans + b;
            d[2] = (ans > 3) ? (ans - 3) : (ans + 5);
            emitNumericAnswer(ans, d);
        } else if (subType == 10) {
            // LIMITE notable trigonometrico
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "lim x->0 sen x/x = ?");
            snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "1");
            snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "0");
            snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "2");
            snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "-1");
        } else if (subType == 11) {
            // INTEGRAL inmediata polinomica: INT(k x^n) dx, con k = m*(n+1)
            int16_t n = random(2, 6);        // exponente 2..5
            int16_t m = random(1, 4);        // coeficiente resultante 1..3
            int16_t k = m * (n + 1);
            int16_t p = n + 1;
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "INT(%dx^%d)dx = ?", k, n);

            if (m == 1) snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "x^%d", p);
            else snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "%dx^%d", m, p);
            snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "%dx^%d", k, n);
            snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "%dx^%d/%d", k, p, p);
            snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "%dx^%d", k * (n + 1), p);
        } else if (subType == 12) {
            // INTEGRAL inmediata de coseno
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "INT(cos x)dx = ?");
            snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "sen x");
            snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "-sen x");
            snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "cos x");
            snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "-cos x");
        } else if (subType == 13) {
            // INTEGRAL inmediata de seno
            snprintf(currentQ.questionText, sizeof(currentQ.questionText), "INT(sen x)dx = ?");
            snprintf(currentQ.options[0], sizeof(currentQ.options[0]), "-cos x");
            snprintf(currentQ.options[1], sizeof(currentQ.options[1]), "cos x");
            snprintf(currentQ.options[2], sizeof(currentQ.options[2]), "sen x");
            snprintf(currentQ.options[3], sizeof(currentQ.options[3]), "-sen x");
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
        bool exitPressed = arduboy.justPressed(B_BUTTON) || arduboy.justPressed(LEFT_BUTTON);

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
        bool backPressed = arduboy.justPressed(B_BUTTON) || arduboy.justPressed(LEFT_BUTTON);

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

        // Respuesta rapida 1:1 con la cruceta (falling edge, sin cursor):
        // ARRIBA -> Opcion 1, ABAJO -> Opcion 2, DERECHA -> Opcion 3, IZQUIERDA -> Opcion 4
        int8_t chosen = -1;
        if (btnTriggered(0) || arduboy.justPressed(UP_BUTTON)) {
            chosen = 0; // Opcion 1 -> UP
        } else if (btnTriggered(1) || arduboy.justPressed(DOWN_BUTTON)) {
            chosen = 1; // Opcion 2 -> DOWN
        } else if (btnTriggered(3) || arduboy.justPressed(RIGHT_BUTTON)) {
            chosen = 2; // Opcion 3 -> RIGHT
        } else if (btnTriggered(2) || arduboy.justPressed(LEFT_BUTTON)) {
            chosen = 3; // Opcion 4 -> LEFT
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
        bool retryPressed = btnTriggered(4) || arduboy.justPressed(A_BUTTON);
        bool exitPressed = arduboy.justPressed(B_BUTTON) || arduboy.justPressed(LEFT_BUTTON);

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
        arduboy.setCursor(7, 57);
        arduboy.print("U/D:SEL A:OK B:MENU");
    }

    void drawCatSelect() {
        arduboy.fillRect(0, 0, 128, 9, WHITE);
        arduboy.setTextColor(BLACK);
        arduboy.setCursor(20, 1);
        arduboy.print("ELIGE CATEGORIA");
        arduboy.setTextColor(WHITE);

        // Lista limpia, alineada a la izquierda: solo texto ASCII, sin iconos
        const char* cats[4] = {
            "1. BASICA",
            "2. ARITMETICA",
            "3. CALCULO",
            "4. MIX TOTAL"
        };

        for (uint8_t i = 0; i < 4; i++) {
            int16_t y = 14 + (i * 10);
            if (catCursor == i) {
                arduboy.fillRect(2, y - 2, 124, 11, WHITE);
                arduboy.setTextColor(BLACK);
            } else {
                arduboy.setTextColor(WHITE);
            }
            arduboy.setCursor(4, y);
            arduboy.print((catCursor == i) ? ">" : " ");
            arduboy.setCursor(12, y);
            arduboy.print(cats[i]);
        }
        arduboy.setTextColor(WHITE);

        arduboy.drawFastHLine(0, 56, 128, WHITE);
        arduboy.setCursor(7, 57);
        arduboy.print("U/D:SEL A:OK B:MENU");
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

        // --- 2. CENTER QUESTION BOX (y = 12 to 27) ---
        arduboy.drawRoundRect(2, 12, 124, 16, 2, WHITE);
        arduboy.fillRect(3, 13, 122, 14, BLACK);

        // Center question text
        uint8_t qLen = strlen(currentQ.questionText);
        int16_t qWidth = qLen * 6;
        int16_t qX = (128 - qWidth) / 2;
        if (qX < 4) qX = 4;
        arduboy.setCursor(qX, 16);
        arduboy.print(currentQ.questionText);

        // --- 3. 4 OPCIONES (2x2, y = 29 to 53) ---
        // Opcion 1 = ARRIBA, Opcion 2 = ABAJO, Opcion 3 = DERECHA, Opcion 4 = IZQUIERDA
        // Box 0 (Top-Left):     x = 2,  y = 29 -> Opcion 1 (UP)
        // Box 1 (Top-Right):    x = 66, y = 29 -> Opcion 2 (DOWN)
        // Box 2 (Bottom-Left):  x = 2,  y = 42 -> Opcion 3 (RIGHT)
        // Box 3 (Bottom-Right): x = 66, y = 42 -> Opcion 4 (LEFT)
        const int16_t boxX[4] = {2, 66, 2, 66};
        const int16_t boxY[4] = {29, 29, 42, 42};
        const int16_t boxW = 60;
        const int16_t boxH = 12;
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

            arduboy.setCursor(bx + 3, by + 2);
            arduboy.print(btnPrefix[i]);
            arduboy.print(currentQ.options[i]);
        }
        arduboy.setTextColor(WHITE);

        // --- 4. AYUDA DE CONTROLES (respuesta directa con la cruceta) ---
        arduboy.drawFastHLine(0, 54, 128, WHITE);
        arduboy.setCursor(13, 55);
        arduboy.print("^:1  v:2  >:3  <:4");
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
