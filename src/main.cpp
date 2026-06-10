#include <Arduino.h>
#include <string>
#include <TFT_eSPI.h> // Graphics and font library
#include <BluetoothSerial.h>
#include <BluetoothA2DPSource.h>
#include <SD.h>
#include <SPI.h>

#pragma region Ports

//Button Ports Defintion
#define BUTTONRED 17
#define BUTTONGREEN 16
#define BUTTONBLUE 26
#define BUTTONYELLOW 25

//Buzzer Ports Defintion
#define BUZZER 15

//Display Ports Defintion
#define CS 27
#define RST 19
#define DC 2
#define MOSI 23
#define SCK 18
#define LED 4
#define MISO 5
#define SCL 22
#define SDA 21

//Joystick Ports Defintion
#define VRX 35
#define VRY 34
#define SW 33

//Potentiometer Ports Definition
#define POT 32

//SD Ports Definition
#define CS_SD 13 

#pragma endregion

enum State 
{
    START,
    PLAYING,
    GAME_OVER
};

struct Inputs 
{
    bool buttonRed;
    bool buttonGreen;
    bool buttonBlue;
    bool buttonYellow;
    float joystickX;
    float joystickY;
    bool joystickButton;
    int potentiometerValue;
};

enum Difficulty
{
    EASY,
    MEDIUM,
    HARD
};

struct DifficultySettings
{
    int timeout;
    int inputVariety; 
    int precisionOfAngleError;
};

using namespace std;

//Display Variable
TFT_eSPI tft = TFT_eSPI(); 

//Game Variables
State state = START;
State lastState = GAME_OVER;
bool choseDifficulty = false;
Difficulty chosenDifficulty;

//Audio Variables
BluetoothA2DPSource a2dp_source;
File audioFile;
string bluetoothSpeakerName = "Flipper Of TheDiamondAce";
const int FRAME_SIZE_BYTES = sizeof(int16_t)*2;

// Track state adjustments
unsigned long gameTimer = 0;
unsigned long lastActionTime = 0;
unsigned long actionTimer = 0; // Tracks the countdown for the current input step
int score = 0;
int highScore = 0;
short hp = 3;
string inputList[20]; 
int currentInputIndex = 0;
int amountOfInputs = 0; 
bool lastPhysicalSwitchState;

// Global live potentiometer value tracking for display updates
int livePotValue = 0;

//Function Prototypes
void checkControls(Inputs inputs);
void displayUpdate();
void gameUpdate(int scoreUpdate, bool gameOver = false);
void gameReset();
void generateNewInputs();
void shortBuzz();

void buzz(int seconds) {
    digitalWrite(BUZZER, HIGH);
    delay(seconds);
    digitalWrite(BUZZER, LOW);
}

int32_t get_sd_audio_data(Frame *data, int32_t frameCount) {
    if (!audioFile || !audioFile.available()) {
        if (audioFile) {
            audioFile.close(); 
        }
        return 0; 
    }
    
    size_t bytesRead = audioFile.read((uint8_t*)data, frameCount * FRAME_SIZE_BYTES);
    return bytesRead / FRAME_SIZE_BYTES;
}

DifficultySettings getDifficultySettings(Difficulty difficulty) {
    DifficultySettings settings;
    switch (difficulty) {
        case EASY:
            settings.timeout = 4000;
            settings.inputVariety = 8; // Buttons + Joystick directions
            settings.precisionOfAngleError = 30; 
            break;
        case MEDIUM:
            settings.timeout = 3000; 
            settings.inputVariety = 10; // Buttons + Joystick directions + Potentiometer
            settings.precisionOfAngleError = 20; 
            break;
        case HARD:
            settings.timeout = 2000; 
            settings.inputVariety = 10; // Buttons + Joystick + Switch + Potentiometer
            settings.precisionOfAngleError = 10; 
            break;
    }
    return settings;
}

long getCurrentTimeout() {
    long baseTimeout = getDifficultySettings(chosenDifficulty).timeout;
    long adjustedTimeout = baseTimeout - (score * 200);
    return max(adjustedTimeout, 600L); 
}

void setup() {
    Serial.begin(115200);
    pinMode(BUTTONRED, INPUT_PULLUP);
    pinMode(BUTTONGREEN, INPUT_PULLUP);
    pinMode(BUTTONBLUE, INPUT_PULLUP);
    pinMode(BUTTONYELLOW, INPUT_PULLUP);
    pinMode(VRX, INPUT);
    pinMode(VRY, INPUT);
    pinMode(SW, INPUT_PULLUP);
    pinMode(POT, INPUT);
    pinMode(BUZZER, OUTPUT);
    
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
    
    tft.init();
    tft.setRotation(1); 
    tft.fillScreen(TFT_BLACK);

    SPI.begin(18, 5, 23, CS_SD);
    
    if (!SD.begin(CS_SD)) {
        Serial.println("SD Card initialization failed!");
        while (1); 
    }
    
    state = START;
    Serial.println("Setup Complete!"); 
}

void loop() {
    Inputs inputs;
    checkControls(inputs);

    switch (state) {
        case START:
            break;
            
        case PLAYING: {  
            long currentTimeout = getCurrentTimeout();
            if (currentTimeout < 600) currentTimeout = 600; 

            if (millis() - actionTimer >= currentTimeout) {
                hp--;
                currentInputIndex = 0; 
                
                buzz(200); 
                actionTimer = millis();
                
                if (hp <= 0) {
                    gameUpdate(0, true);
                }
            }
            break;
        } 
            
        case GAME_OVER:
            highScore = max(score, highScore);
            break;
    }

    displayUpdate();
    delay(20); 
}

void gameUpdate(int scoreUpdate, bool gameOver) 
{
    score += scoreUpdate;

    if (gameOver) {
        state = GAME_OVER;
        tft.fillScreen(TFT_BLACK); 
    } else {
        state = PLAYING;
    }
}

void displayUpdate() 
{
    // =========================================================================
    // 1. STATE TRANSITION RENDERING (Runs ONLY once right when the state changes)
    // =========================================================================
    if (state != lastState) {
        tft.fillScreen(TFT_BLACK); 
        tft.setTextSize(2);

        if (state == START) {
            tft.setTextColor(TFT_BLUE, TFT_BLACK);
            tft.drawString("SET IN STONE", 65, 30);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString("Press GREEN to Start", 30, 110);
        }
        else if (state == GAME_OVER) {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.drawString("GAME OVER", 90, 50);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString("Press GREEN to Retry", 25, 120);
            
            tft.drawString("HP: 0 ", 10, 10);
            tft.drawString("Score: ", 110, 10);
            tft.drawNumber(score, 180, 10);
            buzz(500);
            delay(500);
            buzz(500);
            delay(500);
        }
        else if (state == PLAYING) {
            tft.setTextColor(TFT_BLUE, TFT_BLACK);
            tft.drawString("HP:", 10, 10);
            tft.drawString("Score:", 100, 10);
            tft.drawString("High:", 210, 10);

            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString("ACTION TO DO:", 20, 65);
            tft.drawString("STEPS LEFT:", 20, 122); 
            tft.drawRect(58, 148, 204, 14, TFT_WHITE); 
        }
        
        lastState = state; 
        return; 
    }

    // ==========================================================================
    // 2. ACTIVE GAMEPLAY AND MENU RENDERING ENGINE
    // ==========================================================================
    if (state == START && !choseDifficulty) {
        tft.setTextSize(2);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString("Choose Difficulty:", 40, 150);

        string difficultyOptions[3] = {"EASY", "MEDIUM", "HARD"};
        for (int i = 0; i < 3; i++) {
            if (i == chosenDifficulty) {
                tft.setTextColor(TFT_YELLOW, TFT_BLACK); 
            } else {
                tft.setTextColor(TFT_GREEN, TFT_BLACK);
            }
            tft.drawString(difficultyOptions[i].c_str(), 30 + i*80, 180);
        } 
    }
    
    if (state == PLAYING) {
        static unsigned long lastRenderTime = 0;
        if (millis() - lastRenderTime < 30) return;
        lastRenderTime = millis();

        tft.setTextSize(2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK); 

        tft.drawNumber(hp, 45, 10);
        
        char scoreBuf[10];
        sprintf(scoreBuf, "%-3d", score);
        tft.drawString(scoreBuf, 170, 10);
        tft.drawNumber(highScore, 270, 10);

        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        string targetAction = inputList[currentInputIndex];
        
        // Clean display text interceptor for encoded potentiometer targets
        string cleanDisplayAction = targetAction;
        if (targetAction.rfind("potentiometer:", 0) == 0) {
            cleanDisplayAction = "TURN DIAL!";
        }
        
        char actionBuf[25];
        sprintf(actionBuf, "%-16s", cleanDisplayAction.c_str()); 
        tft.drawString(actionBuf, 40, 95);

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        int stepsRemaining = amountOfInputs - currentInputIndex;
        if (stepsRemaining < 0) stepsRemaining = 0;
        tft.drawNumber(stepsRemaining, 160, 122);

        // --- POTENTIOMETER TARGET BRACKET VISUALIZER GRAPHICS ---
        if (targetAction.rfind("potentiometer:", 0) == 0) {
            int firstColon = targetAction.find(':');
            int secondColon = targetAction.find(':', firstColon + 1);
            int targetMin = stoi(targetAction.substr(firstColon + 1, secondColon - firstColon - 1));
            int targetMax = stoi(targetAction.substr(secondColon + 1));

            tft.setTextSize(1);
            tft.setTextColor(TFT_BLUE, TFT_BLACK);
            tft.drawString("0deg", 55, 185);
            tft.drawString("270deg", 235, 185);
            tft.drawRect(58, 170, 204, 12, TFT_BLUE); 

            // Clear display track box interior channel
            tft.fillRect(59, 171, 202, 10, TFT_BLACK);

            // Translate angle parameters cleanly to visual monitor space
            int bracketMinX = 60 + (targetMin * 200) / 270;
            int bracketMaxX = 60 + (targetMax * 200) / 270;
            int bracketWidth = bracketMaxX - bracketMinX;
            if (bracketWidth < 6) bracketWidth = 6;

            // Paint target window bracket strip bounds
            tft.fillRect(bracketMinX, 171, bracketWidth, 10, TFT_YELLOW);

            // Compute and position current dial knob tick indicator
            float currentAngle = (livePotValue * 270.0) / 4095.0;
            int cursorX = 60 + (currentAngle * 200) / 270;
            if (cursorX > 260) cursorX = 260;
            if (cursorX < 60) cursorX = 60;

            // Highlight bar tracker node depending on alignment evaluation status
            if (cursorX >= bracketMinX && cursorX <= bracketMaxX) {
                tft.fillRect(cursorX - 2, 171, 5, 10, TFT_GREEN);
            } else {
                tft.fillRect(cursorX - 2, 171, 5, 10, TFT_CYAN);
            }
            tft.setTextSize(2); 
        } else {
            tft.fillRect(55, 170, 215, 25, TFT_BLACK); 
        }

        long currentTimeout = getCurrentTimeout();
        long elapsed = millis() - actionTimer;
        long remaining = currentTimeout - elapsed;
        if (remaining < 0) remaining = 0;

        int barWidth = (remaining * 200) / currentTimeout;
        
        tft.fillRect(60, 150, barWidth, 10, TFT_GREEN); 
        tft.fillRect(60 + barWidth, 150, 200 - barWidth, 10, TFT_BLACK); 
    }
}

void gameReset()
{
    score = 0;
    state = PLAYING;
}

void checkControls(Inputs inputs)
{
    inputs.buttonRed = !digitalRead(BUTTONRED);
    inputs.buttonGreen = !digitalRead(BUTTONGREEN);
    inputs.buttonBlue = !digitalRead(BUTTONBLUE);
    inputs.buttonYellow = !digitalRead(BUTTONYELLOW);
    inputs.joystickX = analogRead(VRX)/2048.0 - 1;
    inputs.joystickY = analogRead(VRY)/2048.0 - 1;
    inputs.joystickButton = !digitalRead(SW);
    inputs.potentiometerValue = analogRead(POT);
    
    // Sync live data out globally for display update sweeps
    livePotValue = inputs.potentiometerValue;

    string currentInput = "";
    if (inputs.buttonRed) currentInput = "buttonRed";
    else if (inputs.buttonGreen) currentInput = "buttonGreen";
    else if (inputs.buttonBlue) currentInput = "buttonBlue";
    else if (inputs.buttonYellow) currentInput = "buttonYellow";
    else if (inputs.joystickButton) currentInput = "joystickButton";
    else if (inputs.joystickX > 0.5) currentInput = "joystickRight";
    else if (inputs.joystickX < -0.5) currentInput = "joystickLeft";
    else if (inputs.joystickY > 0.5) currentInput = "joystickUp";
    else if (inputs.joystickY < -0.5) currentInput = "joystickDown";

    static bool readyForInput = true;

    if (currentInput != "" && readyForInput) {
        Serial.print("Button Detected: ");
        Serial.println(currentInput.c_str());
    }

    // =========================================================================
    // 3. MENU & GAME OVER CONTROLLER
    // =========================================================================
    if (state == START) {
        if (currentInput == "joystickRight" && readyForInput && !choseDifficulty) {
            chosenDifficulty = (Difficulty)(((int)chosenDifficulty + 1) % 3);
            readyForInput = false;
            Serial.print("Difficulty Chosen: ");
            switch (chosenDifficulty) {
                case EASY: Serial.println("EASY"); break;
                case MEDIUM: Serial.println("MEDIUM"); break;
                case HARD: Serial.println("HARD"); break;
            }
        }
        if (currentInput == "joystickLeft" && readyForInput && !choseDifficulty) {
            chosenDifficulty = (Difficulty)(((int)chosenDifficulty - 1 + 3) % 3);
            readyForInput = false;
            Serial.print("Difficulty Chosen: ");
            switch (chosenDifficulty) {
                case EASY: Serial.println("EASY"); break;
                case MEDIUM: Serial.println("MEDIUM"); break;
                case HARD: Serial.println("HARD"); break;
            }
        }

        if (currentInput == "buttonGreen" && readyForInput && choseDifficulty == false) {
            Serial.println("Difficulty Confirmed! Starting Game...");
            readyForInput = false; 
            choseDifficulty = true; 
            hp = 3;
            score = 0;
            amountOfInputs = 0;
            currentInputIndex = 0;
            for (int i = 0; i < 20; i++) {
                inputList[i] = "";
            }

            generateNewInputs();       
            state = PLAYING;           
            tft.fillScreen(TFT_BLACK); 
            actionTimer = millis(); 
        }
        
        if (currentInput == "") {
            readyForInput = true;
        }
        return; 
    }
    if (state == GAME_OVER) {
        if (currentInput == "buttonGreen" && readyForInput) {
            Serial.println("Starting Over! Returning to Difficulty Selection...");
            choseDifficulty = false;   
            
            readyForInput = false;
            state = START;           
        }
        if (currentInput == "") {
            readyForInput = true;
        }
        return; 
    }

    // =========================================================================
    // 4. ACTIVE GAMEPLAY INPUT PROCESSING ENGINE
    // =========================================================================
    if (currentInput == "")
    {
        readyForInput = true;
    }
    else if (readyForInput)
    {
        readyForInput = false; 

        string expectedInput = inputList[currentInputIndex];

        // Parse and check real-time target window alignment parameters
        if (expectedInput.rfind("potentiometer:", 0) == 0) { 
            int firstColon = expectedInput.find(':');
            int secondColon = expectedInput.find(':', firstColon + 1);
        
            int targetMin = stoi(expectedInput.substr(firstColon + 1, secondColon - firstColon - 1));
            int targetMax = stoi(expectedInput.substr(secondColon + 1));
        
            float currentAngle = (inputs.potentiometerValue * 270.0) / 4095.0;
        
            if (currentAngle >= targetMin && currentAngle <= targetMax) {
                currentInput = expectedInput; 
            }
        }

        if (currentInput == expectedInput) {
            currentInputIndex++;
            
            if (audioFile) {
                audioFile.close(); 
            }
            audioFile = SD.open("/rightanswer.mp3", FILE_READ);

            Serial.println("Correct Input! Playing Audio!");
            actionTimer = millis(); 

            if (currentInputIndex >= amountOfInputs) {
                currentInputIndex = 0;
                gameUpdate(1);
                generateNewInputs();
            }
        }
        else 
        {
            hp--;
            currentInputIndex = 0; 
            
            buzz(1000); 
            actionTimer = millis(); 
            
            if (hp <= 0) {
                gameUpdate(0, true);
            }
        }
    }
}

void generateNewInputs() 
{
    int randomInput = random(0, getDifficultySettings(chosenDifficulty).inputVariety);
    switch(randomInput) 
    {
        case 0: inputList[amountOfInputs] = "buttonRed"; break;
        case 1: inputList[amountOfInputs] = "buttonGreen"; break;
        case 2: inputList[amountOfInputs] = "buttonBlue"; break;
        case 3: inputList[amountOfInputs] = "buttonYellow"; break;
        case 4: inputList[amountOfInputs] = "joystickUp"; break;
        case 5: inputList[amountOfInputs] = "joystickDown"; break;
        case 6: inputList[amountOfInputs] = "joystickLeft"; break;
        case 7: inputList[amountOfInputs] = "joystickRight"; break;
        case 8: inputList[amountOfInputs] = "joystickButton"; break;
        case 9: {
            int errorWindow = getDifficultySettings(chosenDifficulty).precisionOfAngleError;
            int randomAngleMin = random(0, 270 - errorWindow);
            int randomAngleMax = randomAngleMin + errorWindow;
            
            inputList[amountOfInputs] = "potentiometer:" + to_string(randomAngleMin) + ":" + to_string(randomAngleMax);
        }
        break;
    }
    amountOfInputs++;
}