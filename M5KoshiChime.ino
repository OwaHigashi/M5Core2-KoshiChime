/**
 * Koshi Chime (Tilt Instrument) using M5Stack Core2
 * 
 * Produced by Pococha OwaHigashi 尾和/東
 *
 * This program uses the accelerometer to detect tilt direction and the gyroscope to detect tilt speed.
 * Depending on the tilt direction, it plays a note. The volume of the note depends on the speed of tilting.
 * 
 * The notes corresponding to each direction can be freely set in the notes array.
 * 
 * ぴにゃぴにゃさん枠で鳴らしていたKoshiChimeをM5Stackで作ってみました
 * 処理、特にジャイロの計測遅延が長い、秒3サンプルぐらいしかできず、ちょっと残念な感じですがお試しください
 */

#include <M5Core2.h>
#include <M5UnitSynth.h>

M5UnitSynth synth;

// User can set the notes corresponding to each direction here
int notes[8] = { NOTE_C4, NOTE_D4, NOTE_E4, NOTE_G4, NOTE_B4, NOTE_C5, NOTE_E5, NOTE_G5 };
const char* noteNames[] = { "C4", "D4", "E4", "G4", "B4", "C5", "E5", "G5"};
uint16_t noteColors[] = { RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, ORANGE, WHITE };

float lastAccelX = 0, lastAccelY = 0, lastAccelZ = 0;
float lastGyroX = 0, lastGyroY = 0, lastGyroZ = 0;
int lastnote = -1;
unsigned long lastTime = 0;
unsigned long minNoteInterval = 200; // milliseconds
unsigned long lastNoteTime = 0;

int prevNoteX = -1;
int prevNoteY = -1;

const int centerX = 160;
const int centerY = 120;

// Function to get note name from MIDI note number
const char* getNoteName(int midiNote) {
  static char name[4]; // e.g., "C4"
  int noteIndex = midiNote % 12;
  int octave = midiNote / 12 - 1; // MIDI note 0 is C-1
  sprintf(name, "%s%d", noteNames[noteIndex], octave);
  return name;
}

void setup() {
  M5.begin();
  Serial.begin(115200);
  Serial.println("Tilt Instrument");

  // Initialize accelerometer and gyroscope
  M5.IMU.Init();

  // Initialize synth
  synth.begin(&Serial2, UNIT_SYNTH_BAUD, 33, 32);
  synth.setInstrument(0, 0, Glockenspiel);  // Set to Glockenspiel sound

  // Initialize lastTime
  lastTime = millis();

  // Initialize screen
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.drawCircle(centerX, centerY, 100, WHITE);
}

void loop() {
  float accelX, accelY, accelZ;
  float gyroX, gyroY, gyroZ;

  M5.IMU.getAccelData(&accelX, &accelY, &accelZ);
  M5.IMU.getGyroData(&gyroX, &gyroY, &gyroZ);

  unsigned long currentTime = millis();
  float deltaTime = (currentTime - lastTime) / 1000.0; // Convert to seconds

  // Calculate tilt angle in the XY plane
  float angle = atan2(accelY, accelX) * 180 / PI; // Convert radians to degrees
  if (angle < 0) angle += 360;
  int direction = (int)((angle + 22.5) / 45.0) % 8; // Divide into 8 sectors

  // Compute angular velocity magnitude
  float angularVelocity = sqrt(gyroX * gyroX + gyroY * gyroY + gyroZ * gyroZ);

  // Map angularVelocity to volume (adjust scale factor as needed)
  int volume = (int)(angularVelocity * 4) - 20;

  if (volume > 127) volume = 127;
  if (volume < 0) volume = 0;

  // Get the note corresponding to the current direction
  int note = notes[direction];

  // Play note if volume exceeds threshold and minimum interval has passed
  if ((volume > 10 && (currentTime - lastNoteTime) > minNoteInterval) && (lastnote != note)) {
    synth.setNoteOn(0, note, volume);
    Serial.printf("Vol: %d, Note %d\n", volume, note);

    // Erase previous note symbol
    if (prevNoteX != -1 && prevNoteY != -1) {
      // Define area to clear
      int clearSize = 60; // Adjust this size as needed to cover the text and circle
      M5.Lcd.fillRect(prevNoteX - clearSize / 2, prevNoteY - clearSize / 2, clearSize, clearSize, BLACK);

      // Redraw circle border if overlapped
      float distance = sqrt(pow(prevNoteX - centerX, 2) + pow(prevNoteY - centerY, 2)) + clearSize / 2;
      if (distance >= 100 - clearSize / 2 && distance <= 100 + clearSize / 2) {
        M5.Lcd.drawCircle(centerX, centerY, 100, WHITE);
      }
    }

    // Compute the position to draw the new note symbol
    // Distance from center proportional to volume
    float distance = (volume / 127.0) * 80; // max distance 80 pixels

    // Angle in radians
    float angle_rad = angle * PI / 180.0;

    // x and y offsets
    int x_offset = (int)(cos(angle_rad) * distance);
    int y_offset = (int)(sin(angle_rad) * distance);

    // Position to draw the note symbol
    int noteX = centerX + x_offset;
    int noteY = centerY + y_offset;

    // Get the note name
    const char* noteName = getNoteName(note);

    // Set the text color
    uint16_t color = noteColors[direction % 8];
    M5.Lcd.setTextColor(color, BLACK);
    M5.Lcd.setTextSize(2);

    // Draw the note symbol
    M5.Lcd.drawCentreString(noteName, noteX, noteY - 8, 2); // Adjust Y position
    // Draw a colored circle around the note
    M5.Lcd.drawCircle(noteX, noteY, 15, color);

    // Update prevNoteX and prevNoteY
    prevNoteX = noteX;
    prevNoteY = noteY;

    lastNoteTime = currentTime;
    lastnote = note;
  }

  // Update last values
  lastAccelX = accelX;
  lastAccelY = accelY;
  lastAccelZ = accelZ;
  lastGyroX = gyroX;
  lastGyroY = gyroY;
  lastGyroZ = gyroZ;
  lastTime = currentTime;

  delay(20); // Loop delay
}

