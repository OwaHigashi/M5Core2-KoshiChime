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
int notes[4][8] = {
  { NOTE_G3, NOTE_C4, NOTE_E4, NOTE_F4, NOTE_G4, NOTE_C5, NOTE_E5, NOTE_G5 },
  { NOTE_A3, NOTE_D4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_D5, NOTE_F5, NOTE_A5 },
  { NOTE_A3, NOTE_C4, NOTE_E4, NOTE_A4, NOTE_B4, NOTE_C5, NOTE_E5, NOTE_B5 },
  { NOTE_G3, NOTE_B4, NOTE_D4, NOTE_G4, NOTE_B4, NOTE_D5, NOTE_G5, NOTE_A5 }
};
const char* noteNames[4][8] = {
  { "G3", "C4", "E4", "F4", "G4", "C5", "E5", "G5" },
  { "A3", "D4", "F4", "G4", "A4", "D5", "F5", "A5" },
  { "A3", "C4", "E4", "A4", "B4", "C5", "E5", "B5" },
  { "G3", "B4", "D4", "G4", "B4", "D5", "G5", "A5" }
};
const char* cylinderNames[4] = { "Terra", "Aqua", "Aria", "Ignis"};

uint16_t noteColors[] = { RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, ORANGE, WHITE };

float lastAccelX = 0, lastAccelY = 0, lastAccelZ = 0;
float lastGyroX = 0, lastGyroY = 0, lastGyroZ = 0;

int prevNoteX = -1;
int prevNoteY = -1;

const int centerX = 160;
const int centerY = 120;

// Define structure to keep track of active notes
struct ActiveNote {
  int note;
  unsigned long startTime;
};

const int MAX_ACTIVE_NOTES = 10;
ActiveNote activeNotes[MAX_ACTIVE_NOTES];

void addActiveNote(int note, unsigned long startTime) {
  for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
    if (activeNotes[i].note == -1) {
      activeNotes[i].note = note;
      activeNotes[i].startTime = startTime;
      return;
    }
  }
  Serial.println("Error: Too many active notes");
}

void removeActiveNoteAt(int index) {
  activeNotes[index].note = -1;
  activeNotes[index].startTime = 0;
}

int preCylNextButtonStatus = HIGH;
int preCylPrevButtonStatus = HIGH;
int cylID = 0;
int prevcylID = -1;

void setup() {
  M5.begin();
  Serial.begin(115200);
  Serial.println("KoshiChime");

  // Initialize accelerometer and gyroscope
  M5.IMU.Init();

  // Initialize synth
  synth.begin(&Serial2, UNIT_SYNTH_BAUD, 33, 32);
  synth.setInstrument(0, 0, Glockenspiel);  // Set to Glockenspiel sound

  // Initialize active notes
  for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
    activeNotes[i].note = -1;
    activeNotes[i].startTime = 0;
  }

  // Initialize screen
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.drawCircle(centerX, centerY, 100, WHITE);
}

void loop() {
  float accelX, accelY, accelZ;
  float gyroX, gyroY, gyroZ;

  M5.update();
  int cylNextbuttonStatus = M5.BtnC.wasPressed();
  if (preCylNextButtonStatus != cylNextbuttonStatus) {
    // スイッチ状態が変化していた場合
    if (cylNextbuttonStatus == LOW) {
      cylID++;
      if(cylID > 3) cylID = 0;
    }
  }
  int cylPrevbuttonStatus = M5.BtnA.wasPressed();
  if (preCylPrevButtonStatus != cylPrevbuttonStatus) {
    // スイッチ状態が変化していた場合
    if (cylPrevbuttonStatus == LOW) {
      cylID--;
      if(cylID < 0) cylID = 3;
    }
  }

  // 先に背景を塗りつぶしてからテキストを描画
  if(cylID != prevcylID){
    M5.Lcd.fillRect(0, 0, 100, 30, BLACK);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.drawCentreString(cylinderNames[cylID], 50, 0, 2); // X座標を50に調整
    prevcylID = cylID;
  }

  M5.IMU.getAccelData(&accelX, &accelY, &accelZ);
  M5.IMU.getGyroData(&gyroX, &gyroY, &gyroZ);

  unsigned long currentTime = millis();

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
  int note = notes[cylID][direction];

  // Play note if volume exceeds threshold
  if (volume > 10) {
    synth.setNoteOn(0, note, volume);
    addActiveNote(note, currentTime);
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
    float distanceFromCenter = (volume / 127.0) * 80; // max distance 80 pixels

    // Angle in radians
    float angle_rad = angle * PI / 180.0;

    // x and y offsets
    int x_offset = (int)(cos(angle_rad) * distanceFromCenter);
    int y_offset = (int)(sin(angle_rad) * distanceFromCenter);

    // Position to draw the note symbol
    int noteX = centerX + x_offset;
    int noteY = centerY + y_offset;

    // Get the note name
    const char* noteName = noteNames[cylID][direction];

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
  }

  // Check for notes that should be turned off after a certain duration
  unsigned long noteDuration = 1000; // Note duration in ms
  for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
    if (activeNotes[i].note != -1) {
      if (currentTime - activeNotes[i].startTime >= noteDuration) {
        // Send Note Off
        synth.setNoteOff(0, activeNotes[i].note, 0);
        // Remove note from activeNotes
        removeActiveNoteAt(i);
      }
    }
  }

  // Update last values
  lastAccelX = accelX;
  lastAccelY = accelY;
  lastAccelZ = accelZ;
  lastGyroX = gyroX;
  lastGyroY = gyroY;
  lastGyroZ = gyroZ;

  // CPU使用率を下げるためにディレイを追加
  delay(volume*0.8+70);
}
