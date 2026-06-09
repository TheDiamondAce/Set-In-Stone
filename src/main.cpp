#include <Arduino.h>
#include <string>
#include <TFT_eSPI.h> // Graphics and font library

#pragma region Ports

//Button Ports Defintion
#define BUTTONRED 17
#define BUTTONGREEN 16
#define BUTTONBLUE 26
#define BUTTONYELLOW 25

//Buzzer Ports Defintion
#define BUZZER 13

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

//Switch Ports Definition
#define SWITCH 14

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
    bool switchState;
};

using namespace std;

//Display Variable
TFT_eSPI tft = TFT_eSPI(); 

//Game Variables
State state = START;
State lastState = GAME_OVER; // TRACKS STATE CHANGES TO PREVENT SCREEN FLICKER & SIMULATOR LAG
unsigned long gameTimer = 0;
unsigned long lastActionTime = 0;
unsigned long actionTimer = 0; // Tracks the countdown for the current input step
int score = 0;
int highScore = 0;
short hp = 3;
string inputList[20]; 
int currentInputIndex = 0;
int amountOfInputs = 0; 

//Function Prototypes
void checkControls(Inputs inputs);
void displayUpdate();
void gameUpdate(int scoreUpdate, bool gameOver = false);
void gameReset();
void generateNewInputs();

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
    pinMode(SWITCH, INPUT_PULLUP);
    // Turn on the display backlight
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
    
    tft.init();
    tft.setRotation(1); // Landscape mode
    tft.fillScreen(TFT_BLACK);
    
    state = START;
    Serial.println("Setup Complete!"); 
}

void loop() {
    // Step 1: Check for updates on controls
    Inputs inputs;
    checkControls(inputs);

    // Step 2: Update game state/timers
    switch (state) {
        case START:
            break;
            
        case PLAYING: {  
            long currentTimeout = 4000 - (score * 200); 
            if (currentTimeout < 600) currentTimeout = 600; 

            if (millis() - actionTimer >= currentTimeout) {
                hp--;
                currentInputIndex = 0; 
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

    // Step 3: Render everything to the display
    displayUpdate();

    // EVERY LOOP PAUSE: Gives the Wokwi SPI simulator room to breathe!
    delay(20); 
}

void gameUpdate(int scoreUpdate, bool gameOver) 
{
    score += scoreUpdate;

    if (gameOver) {
        state = GAME_OVER;
        tft.fillScreen(TFT_BLACK); // Clear layout for game over message
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
        tft.fillScreen(TFT_BLACK); // Fresh layout canvas brush clear
        tft.setTextSize(2);

        if (state == START) {
            tft.setTextColor(TFT_BLUE, TFT_BLACK);
            tft.drawString("SET IN STONE", 85, 30);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString("Press GREEN to Start", 40, 110);
        }
        else if (state == GAME_OVER) {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.drawString("GAME OVER", 110, 50);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString("Press GREEN to Retry", 45, 120);
            
            // Render fixed final snapshot scores
            tft.drawString("HP: 0 ", 10, 10);
            tft.drawString("Score: ", 110, 10);
            tft.drawNumber(score, 180, 10);
        }
        else if (state == PLAYING) {
            // Draw static layout backgrounds once to eliminate performance drop
            tft.setTextColor(TFT_BLUE, TFT_BLACK);
            tft.drawString("HP:", 10, 10);
            tft.drawString("Score:", 100, 10);
            tft.drawString("High:", 210, 10);

            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString("ACTION TO DO:", 20, 65);
            tft.drawString("STEPS LEFT:", 20, 122); // Static baseline progress indicator label
            tft.drawRect(58, 148, 204, 14, TFT_WHITE); // Outer visual border line
        }
        
        lastState = state; // Synchronization match check updated
        return; 
    }

    // =========================================================================
    // 2. ACTIVE GAMEPLAY RENDERING (Runs sequentially ONLY when state == PLAYING)
    // =========================================================================
    if (state == PLAYING) {
        // Limits dynamic data refreshes to a stable 30ms frame block window
        static unsigned long lastRenderTime = 0;
        if (millis() - lastRenderTime < 30) return;
        lastRenderTime = millis();

        tft.setTextSize(2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK); // Background color eraser active

        // Update real-time variables cleanly without over-writing neighboring digits
        tft.drawNumber(hp, 45, 10);
        
        char scoreBuf[10];
        sprintf(scoreBuf, "%-3d", score);
        tft.drawString(scoreBuf, 170, 10);
        tft.drawNumber(highScore, 270, 10);

        // Display current target string action
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        string targetAction = inputList[currentInputIndex];
        char actionBuf[25];
        sprintf(actionBuf, "%-16s", targetAction.c_str()); 
        tft.drawString(actionBuf, 40, 95);

        // Display remaining item steps in current sequence to gain a point
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        int stepsRemaining = amountOfInputs - currentInputIndex;
        if (stepsRemaining < 0) stepsRemaining = 0;
        tft.drawNumber(stepsRemaining, 160, 122);

        // Calculate and scale countdown visual bar width metrics
        long currentTimeout = 4000 - (score * 200);
        if (currentTimeout < 600) currentTimeout = 600;

        long elapsed = millis() - actionTimer;
        long remaining = currentTimeout - elapsed;
        if (remaining < 0) remaining = 0;

        int barWidth = (remaining * 200) / currentTimeout;
        
        // Render bar alterations cleanly
        tft.fillRect(60, 150, barWidth, 10, TFT_GREEN); // Current filling progress width
        tft.fillRect(60 + barWidth, 150, 200 - barWidth, 10, TFT_BLACK); // Overwrites trailing bar trail cleanly
    }
}

void gameReset()
{
    score = 0;
    state = PLAYING;
}

void checkControls(Inputs inputs)
{
    // 1. Gather current physical readings from pins first
    inputs.buttonRed = !digitalRead(BUTTONRED);
    inputs.buttonGreen = !digitalRead(BUTTONGREEN);
    inputs.buttonBlue = !digitalRead(BUTTONBLUE);
    inputs.buttonYellow = !digitalRead(BUTTONYELLOW);
    //2048 is value for esp32 512 is arduino uno
    inputs.joystickX = analogRead(VRX)/2048.0 - 1;
    inputs.joystickY = analogRead(VRY)/2048.0 - 1;
    inputs.joystickButton = !digitalRead(SW);
    inputs.potentiometerValue = analogRead(POT);
    inputs.switchState = !digitalRead(SWITCH);

    // 2. MOVE THIS UP: Map physical inputs to string representations first!
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

    // --- ADD THIS TEMPORARY DEBUG LINE HERE ---
    if (currentInput != "") {
        Serial.print("Button Detected: ");
        Serial.println(currentInput.c_str());
    }
    // ------------------------------------------

    static bool readyForInput = true;


    // =========================================================================
    // 3. MENU & GAME OVER CONTROLLER (Now it can read currentInput perfectly!)
    // =========================================================================
    if (state == START || state == GAME_OVER) {
        if (currentInput == "buttonGreen" && readyForInput) {
            readyForInput = false; // Lock input to prevent bouncing acceleration

            // Wipe out variables for a fresh gameplay canvas configuration
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
            actionTimer = millis(); // Fire up countdown window baseline
        }
        
        if (currentInput == "") {
            readyForInput = true;
        }
        return; // Exits menu checks safely
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
        readyForInput = false; // Debounce system lock until user lifts finger

        string expectedInput = inputList[currentInputIndex];

        if (currentInput == expectedInput) {
            // --- MATCH SUCCESS ---
            currentInputIndex++;
            actionTimer = millis(); // Refresh countdown window frame for next sequence step

            if (currentInputIndex >= amountOfInputs) {
                currentInputIndex = 0;
                gameUpdate(1);
                generateNewInputs();
            }
        }
        else 
        {
            // --- MISMATCH FAIL ---
            hp--;
            currentInputIndex = 0; // Reset sequence checkpoint back to index 0
            
            // Deduct exactly 1000ms (1 second) off the active countdown window 
            // as an input penalty instead of offering a clean reset timeout frame
            actionTimer -= 1000; 
            
            if (hp <= 0) {
                gameUpdate(0, true);
            }
        }
    }
}

void generateNewInputs() 
{
    int randomInput = random(0, 9);
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
    }
    amountOfInputs++;
}