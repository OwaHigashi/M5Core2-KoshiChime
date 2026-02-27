/**
 * @file M5Core2-KoshiChime.ino
 * @brief M5Core2 Koshi Chime Emulator using UNIT-SYNTH
 * @version 2.0
 * @date 2024
 *
 * A Koshi Chime emulator with IMU tilt-based ball physics
 * Uses M5UnitSynth library for sound via UNIT-SYNTH (Serial2, pins 33, 32)
 * Uses M5Core2 IMU for motion detection
 *
 * Pococha Owa Higashi 尾和/東
 *
 * @Hardwares: M5Core2 + Unit Synth
 * @Platform Version: Arduino M5Stack Board Manager v2.1.0
 * @Dependent Library:
 * M5UnitSynth: https://github.com/m5stack/M5Unit-Synth
 * M5Core2: https://github.com/m5stack/M5Core2
 */

#include <M5Core2.h>

// for SD-Updater
#define SDU_ENABLE_GZ
#include <M5StackUpdater.h>

#include "M5UnitSynth.h"

// 尾和東@Pococha技術枠

// UNIT-SYNTH instance
M5UnitSynth synth;

// Koshi Chime types with 8 notes each
enum ChimeType {
    TERRA = 0,
    AQUA = 1,
    ARIA = 2,
    IGNIS = 3
};

const char* chime_names[] = {"Terra", "Aqua", "Aria", "Ignis"};

// Chime note configurations (MIDI note numbers)
uint8_t chime_notes[4][8] = {
    // Terra (Earth): G3, C4, E4, G4, B4, E5, G5, B5
    {NOTE_G3, NOTE_C4, NOTE_E4, NOTE_G4, NOTE_B4, NOTE_E5, NOTE_G5, NOTE_B5},
    // Aqua (Water): A3, D4, F4, A4, C5, E5, A5, C6
    {NOTE_A3, NOTE_D4, NOTE_F4, NOTE_A4, NOTE_C5, NOTE_E5, NOTE_A5, NOTE_C6},
    // Aria (Air): A3, C4, E4, A4, B4, E5, A5, C6
    {NOTE_A3, NOTE_C4, NOTE_E4, NOTE_A4, NOTE_B4, NOTE_E5, NOTE_A5, NOTE_C6},
    // Ignis (Fire): G3, B3, D4, G4, A4, D5, G5, A5
    {NOTE_G3, NOTE_B3, NOTE_D4, NOTE_G4, NOTE_A4, NOTE_D5, NOTE_G5, NOTE_A5}
};

ChimeType current_chime = TERRA;

// Note name lookup
const char* note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

// Display layout
const int16_t CX = 160;        // circle center X
const int16_t CY = 115;        // circle center Y
const int16_t TUBE_R = 65;     // tube inner radius
const int16_t ROD_ORBIT = 55;  // rod distance from center
const int16_t ROD_R = 13;      // rod visual radius
const int16_t BALL_R = 10;     // ball radius
const float HIT_DIST = 24.0f;  // collision distance (> gap between rods)
const int ROD_DEBOUNCE = 80;   // short debounce for rapid rattling

// Colors
const uint16_t COL_BG = 0x2945;         // dark bamboo interior
const uint16_t COL_TUBE_WALL = 0x8C51;  // bamboo wall
const uint16_t COL_TUBE_INNER = 0x1082;  // very dark inside
const uint16_t COL_ROD = 0xC618;        // silver rod
const uint16_t COL_ROD_GLOW = 0xFFE0;   // yellow glow when hit
const uint16_t COL_BALL = 0xFFFF;       // white ball

// Chime type accent colors (for UI elements)
const uint16_t chime_colors[] = {
    0x8AA2,  // Terra: earthy green-brown
    0x055F,  // Aqua: ocean blue
    0x867F,  // Aria: sky blue-white
    0xF920   // Ignis: fire orange
};

// IMU calibration offset (set by BtnB)
float cal_ax = 0, cal_ay = 0;

// Ball physics
float ball_x = 0, ball_y = 0;       // position relative to center (pixels)
float ball_vx = 0, ball_vy = 0;     // velocity
const float GRAVITY_SCALE = 120.0f; // strong tilt response for vigorous movement
const float FRICTION = 0.97f;       // less damping = longer momentum
const float MAX_SPEED = 20.0f;      // faster ball
const float WALL_BOUNCE = -0.5f;    // bounce coefficient off wall
const float ROD_BOUNCE = 2.0f;      // bounce speed off rod

// Rod state
unsigned long rod_last_hit[8] = {0};
uint8_t rod_glow[8] = {0};

// Rod positions (pre-calculated in setup)
int16_t rod_px[8], rod_py[8];  // screen positions
float rod_fx[8], rod_fy[8];    // relative to center (float)

// Previous ball screen position (for erasure)
int16_t prev_ball_sx = CX, prev_ball_sy = CY;

void setup() {
    M5.begin();

    // for SD-Updater
    checkSDUpdater( SD, MENU_BIN, 2000, TFCARD_CS_PIN );

    Serial.begin(115200);

    // Initialize UNIT-SYNTH
    synth.begin(&Serial2, UNIT_SYNTH_BAUD, 33, 32);

    // Celesta: warm, pleasant metallic chime sound
    synth.setInstrument(0, 0, Celesta);

    // Initialize IMU
    M5.IMU.Init();

    // Pre-calculate rod positions (8 rods, 45 degrees apart)
    for (int i = 0; i < 8; i++) {
        float angle = i * 3.14159f * 2.0f / 8.0f - 3.14159f / 2.0f;  // start from top
        rod_fx[i] = cos(angle) * ROD_ORBIT;
        rod_fy[i] = sin(angle) * ROD_ORBIT;
        rod_px[i] = CX + (int16_t)rod_fx[i];
        rod_py[i] = CY + (int16_t)rod_fy[i];
    }

    // Initial calibration (current position = center)
    float az_init;
    M5.IMU.getAccelData(&cal_ax, &cal_ay, &az_init);

    Serial.println("M5Core2 Koshi Chime v2.0 Started");

    drawFullScreen();
}

void loop() {
    M5.update();

    // Button: switch chime type
    if (M5.BtnA.wasPressed()) {
        current_chime = (ChimeType)((current_chime + 3) % 4);  // prev
        drawFullScreen();
    }
    if (M5.BtnB.wasReleased()) {
        // Calibrate: current tilt becomes center
        float az_tmp;
        M5.IMU.getAccelData(&cal_ax, &cal_ay, &az_tmp);
        ball_x = 0; ball_y = 0;
        ball_vx = 0; ball_vy = 0;
        drawFullScreen();
    }
    if (M5.BtnC.wasPressed()) {
        current_chime = (ChimeType)((current_chime + 1) % 4);  // next
        drawFullScreen();
    }

    // 1. Read IMU tilt (subtract calibration offset)
    float ax, ay, az_dummy;
    M5.IMU.getAccelData(&ax, &ay, &az_dummy);
    ax -= cal_ax;
    ay -= cal_ay;

    // 2. Update ball velocity (tilt = gravity on ball)
    ball_vx -= ax * GRAVITY_SCALE * 0.02f;
    ball_vy += ay * GRAVITY_SCALE * 0.02f;

    // Apply friction
    ball_vx *= FRICTION;
    ball_vy *= FRICTION;

    // Clamp speed
    float speed = sqrt(ball_vx * ball_vx + ball_vy * ball_vy);
    if (speed > MAX_SPEED) {
        ball_vx = ball_vx / speed * MAX_SPEED;
        ball_vy = ball_vy / speed * MAX_SPEED;
    }

    // 3. Update position
    ball_x += ball_vx;
    ball_y += ball_vy;

    // 4. Constrain to tube interior (bounce off wall)
    float dist_from_center = sqrt(ball_x * ball_x + ball_y * ball_y);
    float max_dist = TUBE_R - BALL_R - 4;
    if (dist_from_center > max_dist) {
        // Normalize and push back
        float nx = ball_x / dist_from_center;
        float ny = ball_y / dist_from_center;
        ball_x = nx * max_dist;
        ball_y = ny * max_dist;
        // Reflect velocity
        float dot = ball_vx * nx + ball_vy * ny;
        ball_vx -= 2 * dot * nx;
        ball_vy -= 2 * dot * ny;
        ball_vx *= 0.5f;  // lose energy on wall hit
        ball_vy *= 0.5f;
    }

    // 5. Check rod collisions
    unsigned long now = millis();
    for (int i = 0; i < 8; i++) {
        float dx = ball_x - rod_fx[i];
        float dy = ball_y - rod_fy[i];
        float d = sqrt(dx * dx + dy * dy);

        if (d < HIT_DIST && (now - rod_last_hit[i]) > ROD_DEBOUNCE) {
            rod_last_hit[i] = now;
            rod_glow[i] = 15;  // glow for 15 frames

            // Play note - velocity based on ball speed
            float hit_speed = sqrt(ball_vx * ball_vx + ball_vy * ball_vy);
            uint8_t velocity = (uint8_t)(30 + hit_speed * 5);
            if (velocity > 127) velocity = 127;
            uint8_t note = chime_notes[current_chime][i];
            synth.setNoteOn(0, note, velocity);

            // Bounce ball away from rod
            if (d > 0.1f) {
                float bnx = dx / d;
                float bny = dy / d;
                ball_vx = bnx * ROD_BOUNCE;
                ball_vy = bny * ROD_BOUNCE;
            }
        }
    }

    // 6. Draw update
    drawUpdate();

    delay(20);
}

/**
 * Draw the full screen (background, tube, rods, labels)
 */
void drawFullScreen() {
    M5.Lcd.fillScreen(COL_BG);

    // Title bar
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(10, 5);
    M5.Lcd.print("KOSHI CHIME");

    // Chime type name
    M5.Lcd.setTextColor(chime_colors[current_chime]);
    M5.Lcd.setCursor(230, 5);
    M5.Lcd.print(chime_names[current_chime]);

    // Tube outline (double ring for thickness)
    M5.Lcd.fillCircle(CX, CY, TUBE_R + 6, COL_TUBE_WALL);
    M5.Lcd.fillCircle(CX, CY, TUBE_R, COL_TUBE_INNER);

    // Draw rods with note labels
    for (int i = 0; i < 8; i++) {
        drawRod(i);
    }

    // Bottom: instructions
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(0x7BEF);  // light grey
    M5.Lcd.setCursor(20, 225);
    M5.Lcd.print("<< BtnA");
    M5.Lcd.setCursor(130, 225);
    M5.Lcd.print("Tilt!");
    M5.Lcd.setCursor(260, 225);
    M5.Lcd.print("BtnC >>");

    // Reset ball
    ball_x = 0; ball_y = 0;
    ball_vx = 0; ball_vy = 0;
    prev_ball_sx = CX; prev_ball_sy = CY;

    // Draw initial ball
    M5.Lcd.fillCircle(CX, CY, BALL_R, COL_BALL);
}

/**
 * Draw a single rod (with glow state)
 */
void drawRod(int i) {
    uint16_t color = (rod_glow[i] > 0) ? COL_ROD_GLOW : COL_ROD;
    M5.Lcd.fillCircle(rod_px[i], rod_py[i], ROD_R, color);
    M5.Lcd.drawCircle(rod_px[i], rod_py[i], ROD_R, 0x6B4D);  // dark edge

    // Note label (outside the rod)
    uint8_t note = chime_notes[current_chime][i];
    uint8_t note_num = note % 12;

    // Position label further from center
    float angle = atan2(rod_fy[i], rod_fx[i]);
    int16_t lx = CX + (int16_t)(cos(angle) * (ROD_ORBIT + 16));
    int16_t ly = CY + (int16_t)(sin(angle) * (ROD_ORBIT + 16));

    M5.Lcd.setTextSize(1);
    uint16_t text_col = (rod_glow[i] > 0) ? COL_ROD_GLOW : 0x9CF3;
    M5.Lcd.setTextColor(text_col, COL_TUBE_INNER);
    M5.Lcd.setCursor(lx - 4, ly - 4);
    M5.Lcd.print(note_names[note_num]);
}

/**
 * Incremental draw update (erase old ball, draw rods if glow changed, draw new ball)
 */
void drawUpdate() {
    // Erase previous ball position
    int16_t ball_sx = CX + (int16_t)ball_x;
    int16_t ball_sy = CY + (int16_t)ball_y;

    // Only redraw if ball moved
    if (ball_sx != prev_ball_sx || ball_sy != prev_ball_sy) {
        M5.Lcd.fillCircle(prev_ball_sx, prev_ball_sy, BALL_R, COL_TUBE_INNER);
    }

    // Update rod glow states
    for (int i = 0; i < 8; i++) {
        if (rod_glow[i] > 0) {
            rod_glow[i]--;
            drawRod(i);
        }
    }

    // Draw ball at new position
    M5.Lcd.fillCircle(ball_sx, ball_sy, BALL_R, COL_BALL);

    prev_ball_sx = ball_sx;
    prev_ball_sy = ball_sy;
}
