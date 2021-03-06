#include "LedControl.h" // LedControl library is used for controlling a LED matrix. Find it using Library Manager or download zip here: https://github.com/wayoda/LedControl


// --------------------------------------------------------------- //
// ------------------------- user config ------------------------- //
// --------------------------------------------------------------- //

// some notes for the music
const int c = 261;
const int d = 294;
const int e = 329;
const int f = 349;
const int g = 391;
const int gS = 415;
const int a = 440;
const int aS = 466;
const int b = 494;
const int cH = 523;
const int cSH = 554;
const int dH = 587;
const int dSH = 622;
const int eH = 659;
const int fH = 698;
const int fSH = 740;
const int gH = 784;
const int gSH = 830;
const int aH = 880;

// there are defined all the pins
struct Pin {
  static const short joystickX = A2;   // joystick X axis pin
  static const short joystickY = A3;   // joystick Y axis pin
//  static const short joystickKEY = 18; // (not used) joystick KEY pin (Analog 4) (Z axis button)
  static const short joystickVCC = 15; // virtual VCC for the joystick (Analog 1) (to make the joystick connectable right next to the arduino nano)
  static const short joystickGND = 14; // virtual GND for the joystick (Analog 0) (to make the joystick connectable right next to the arduino nano)

  static const short potentiometer = A5; // potentiometer for snake speed control

  static const short CLK = 10;   // clock for LED matrix
  static const short CS  = 11;   // chip-select for LED matrix
  static const short DIN = 12;   // data-in for LED matrix
  
  static const short buzzerPin = 2;  // buzzer on pin
};

// LED matrix brightness: between 0(darkest) and 15(brightest)
const short intensity = 4;

// lower = faster message scrolling
const short messageSpeed = 4;

// initial snake length (1...63, recommended 3)
const short initialSnakeLength = 3;


void setup() {
  Serial.begin(115200);  // set the same baud rate on your Serial Monitor
  initialize();         // initialize pins & LED matrix
  calibrateJoystick(); // calibrate the joystick home (do not touch it)
  showSnakeMessage(); // scrolls the 'snake' message around the matrix
  startSound();      // first time, play the start sound
}


void loop() {
  generateFood();    // if there is no food, generate one
  scanJoystick();    // watches joystick movements & blinks with food
  calculateSnake();  // calculates snake parameters
  handleGameStates();

  // uncomment this if you want the current game board to be printed to the serial (slows down the game a bit)
  // dumpGameBoard();
  
}





// --------------------------------------------------------------- //
// -------------------- supporting variables --------------------- //
// --------------------------------------------------------------- //

LedControl matrix(Pin::DIN, Pin::CLK, Pin::CS, 1);

struct Point {
  int row = 0, col = 0;
  Point(int row = 0, int col = 0): row(row), col(col) {}
};

struct Coordinate {
  int x = 0, y = 0;
  Coordinate(int x = 0, int y = 0): x(x), y(y) {}
};

bool win = false;
bool gameOver = false;

// primary snake head coordinates (snake head), it will be randomly generated
Point snake;

// food is not anywhere yet
Point food(-1, -1);

// construct with default values in case the user turns off the calibration
Coordinate joystickHome(500, 500);

// snake parameters
int snakeLength = initialSnakeLength; // choosed by the user in the config section
int snakeSpeed = 1; // will be set according to potentiometer value, cannot be 0
int snakeDirection = 0; // if it is 0, the snake does not move

// direction constants
const short up     = 1;
const short right  = 2;
const short down   = 3; // 'down - 2' must be 'up'
const short left   = 4; // 'left - 2' must be 'right'

// threshold where movement of the joystick will be accepted
const int joystickThreshold = 200;

// artificial logarithmity (steepness) of the potentiometer (-1 = linear, 1 = natural, bigger = steeper (recommended 0...1))
const float logarithmity = 0.4;

// the age array: holds an 'age' of the every pixel in the matrix. If age > 0, it glows.
// on every frame, the age of all lit pixels is incremented.
// when the age of some pixel exceeds the length of the snake, it goes out.
// age 1 is added in the current snake direction next to the last position of the snake head.
int age[8][8] = {};




// --------------------------------------------------------------- //
// -------------------------- functions -------------------------- //
// --------------------------------------------------------------- //


// if there is no food, generate one, also check for victory
void generateFood() {
  if (food.row == -1 || food.col == -1) {
    // self-explanatory
    if (snakeLength >= 64) {
      win = true;
      return; // prevent the food generator from running, in this case it would run forever, because it will not be able to find a pixel without a snake
    }

    // generate food until it is in the right position
    do {
      food.col = random(8);
      food.row = random(8);
    } while (age[food.row][food.col] > 0);
  }
}


// custom inverse logarithm with variable steepness (logarithmity), see https://www.desmos.com/calculator/qmyqv84xis (input = 0...1)
float lnx(float n) {
  if(n < 0) return 0;
  if(n > 1) return 1;
  n = -log(-n * logarithmity + 1); // natural logarithm
  if (isinf(n)) n = lnx(0.999999); // prevent returning 'inf'
  return n;
}


// watches joystick movements & blinks with food
void scanJoystick() {
  int previousDirection = snakeDirection; // save the last direction
  long timestamp = millis() + snakeSpeed; // when the next frame will be rendered

  while (millis() < timestamp) {
    // calculate snake speed logarithmically (10...1000ms)
    float raw = mapf(analogRead(Pin::potentiometer), 0, 1023, 0, 1);
    snakeSpeed = mapf(lnx(raw), lnx(0), lnx(1), 10, 1000);
    if (snakeSpeed == 0) snakeSpeed = 1; // safety: speed can not be 0

    // determine the direction of the snake
    analogRead(Pin::joystickY) < joystickHome.y - joystickThreshold ? snakeDirection = right : 0;
    analogRead(Pin::joystickY) > joystickHome.y + joystickThreshold ? snakeDirection = left  : 0;
    analogRead(Pin::joystickX) < joystickHome.x - joystickThreshold ? snakeDirection = up    : 0;
    analogRead(Pin::joystickX) > joystickHome.x + joystickThreshold ? snakeDirection = down  : 0;

    // ignore directional change by 180 degrees (no effect for non-moving snake)
    snakeDirection + 2 == previousDirection && previousDirection != 0 ? snakeDirection = previousDirection : 0;
    snakeDirection - 2 == previousDirection && previousDirection != 0 ? snakeDirection = previousDirection : 0;

    // intelligently blink with the food
    matrix.setLed(0, food.row, food.col, millis() % 100 < 50 ? 1 : 1);
  }
}


// calculate snake movement data
void calculateSnake() {
  switch (snakeDirection) {
  case up:
    snake.row--;
    fixEdge();
    matrix.setLed(0, snake.row, snake.col, 1);
    break;

  case right:
    snake.col++;
    fixEdge();
    matrix.setLed(0, snake.row, snake.col, 1);
    break;

  case down:
    snake.row++;
    fixEdge();
    matrix.setLed(0, snake.row, snake.col, 1);
    break;

  case left:
    snake.col--;
    fixEdge();
    matrix.setLed(0, snake.row, snake.col, 1);
    break;

  default: // if the snake is not moving, exit
    return;
  }

  // if there is any age (snake body), this will cause the end of the game (snake must be moving)
  if (age[snake.row][snake.col] != 0 && snakeDirection != 0) {
    gamoverSound();
    gameOver = true;
    return;
  }

  // check if the food was eaten
  if (snake.row == food.row && snake.col == food.col) {
    beep(fH, 30); //beep if the snake eat the dot (i think 30+10ms blocking is okay)
    snakeLength++;
    food.row = -1; // reset food
    food.col = -1;
  }

  // increment ages if all lit leds
  updateAges();

  // change the age of the snake head from 0 to 1
  age[snake.row][snake.col]++;
}


// causes the snake to appear on the other side of the screen if it gets out of the edge
void fixEdge() {
  snake.col < 0 ? snake.col += 8 : 0;
  snake.col > 7 ? snake.col -= 8 : 0;
  snake.row < 0 ? snake.row += 8 : 0;
  snake.row > 7 ? snake.row -= 8 : 0;
}


// increment ages if all lit leds, turn off too old ones depending on the length of the snake
void updateAges() {
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      // if the led is lit, increment it's age
      if (age[row][col] > 0 ) {
        age[row][col]++;
      }

      // if the age exceeds the length of the snake, switch it off
      if (age[row][col] > snakeLength) {
        matrix.setLed(0, row, col, 0);
        age[row][col] = 0;
      }
    }
  }
}


void handleGameStates() {
  if (gameOver || win) {
    unrollSnake();

    showScoreMessage(snakeLength);

    if (gameOver) showGameOverMessage();
    else if (win) showWinMessage();

    // re-init the game
    win = false;
    gameOver = false;
    snake.row = random(8);
    snake.col = random(8);
    food.row = -1;
    food.col = -1;
    snakeLength = initialSnakeLength;
    snakeDirection = 0;
    startSound();
    memset(age, 0, sizeof(age[0][0]) * 8 * 8);
    matrix.clearDisplay(0);
  }
}


void unrollSnake() {
  // switch off the food LED
  matrix.setLed(0, food.row, food.col, 0);

  delay(600);

  for (int i = 1; i <= snakeLength; i++) {
    for (int row = 0; row < 8; row++) {
      for (int col = 0; col < 8; col++) {
        if (age[row][col] == i) {
          matrix.setLed(0, row, col, 0);
          delay(100);
        }
      }
    }
  }
}


// calibrate the joystick home for 10 times
void calibrateJoystick() {
  Coordinate values;

  for (int i = 0; i < 10; i++) {
    values.x += analogRead(Pin::joystickX);
    values.y += analogRead(Pin::joystickY);
  }

  joystickHome.x = values.x / 10;
  joystickHome.y = values.y / 10;
}


void initialize() {
  pinMode(Pin::joystickVCC, OUTPUT);
  digitalWrite(Pin::joystickVCC, HIGH);

  pinMode(Pin::joystickGND, OUTPUT);
  digitalWrite(Pin::joystickGND, LOW);

//  pinMode(Pin::joystickKEY, INPUT_PULLUP);

  matrix.shutdown(0, false);
  matrix.setIntensity(0, intensity);
  matrix.clearDisplay(0);

  randomSeed(analogRead(A5));
  snake.row = random(8);
  snake.col = random(8);
}


void dumpGameBoard() {
  String buff = "\n\n\n";
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      if (age[row][col] < 10) buff += " ";
      if (age[row][col] != 0) buff += age[row][col];
      else if (col == food.col && row == food.row) buff += "@";
      else buff += "-";
      buff += " ";
    }
    buff += "\n";
  }
  Serial.println(buff);
}





// --------------------------------------------------------------- //
// -------------------------- messages --------------------------- //
// --------------------------------------------------------------- //


const PROGMEM bool snejkMessage[8][56] = {
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,1,1,0,0,0,1,1,0,0,1,1,1,1,1,1,0,0,0,0,1,1,1,1,0,0,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,1,0,0,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,1,1,0,0,0,1,1,0,1,1,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,1,1,0,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,1,1,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,1,1,0,1,1,1,1,0,0,1,1,1,1,1,0,0,0,0,0,0,1,1,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,1,0,0,1,1,0,0,0,0,0,0,1,1,0,1,1,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,1,1,0,1,1,0,0,0,1,1,0,1,1,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,1,1,0,0,0,1,1,0,0,1,1,1,1,1,1,0,0,0,1,1,1,0,0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0}
};

const PROGMEM bool gameOverMessage[8][90] = {
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,1,1,0,0,0,1,1,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,1,1,1,1,0,0,0,1,1,0,0,1,1,0,0,1,1,1,1,1,1,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,1,0,1,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,1,1,1,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,1,1,1,1,0,0,1,1,0,1,0,1,1,0,0,1,1,1,1,1,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,1,1,1,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,1,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,0,1,1,1,1,0,0,0,1,1,0,0,0,0,0,0,1,1,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,1,1,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,0,0,0,0,1,1,1,1,1,1,0,0,1,1,0,0,1,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0}
};

const PROGMEM bool scoreMessage[8][58] = {
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,1,1,1,1,1,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,1,1,1,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,1,1,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,1,1,0,0,1,1,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0}
};

const PROGMEM bool digits[][8][8] = {
  {
    {0,0,0,0,0,0,0,0},
    {0,0,1,1,1,1,0,0},
    {0,1,1,0,0,1,1,0},
    {0,1,1,0,1,1,1,0},
    {0,1,1,1,0,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,0,1,1,1,1,0,0}
  },
  {
    {0,0,0,0,0,0,0,0},
    {0,0,0,1,1,0,0,0},
    {0,0,0,1,1,0,0,0},
    {0,0,1,1,1,0,0,0},
    {0,0,0,1,1,0,0,0},
    {0,0,0,1,1,0,0,0},
    {0,0,0,1,1,0,0,0},
    {0,1,1,1,1,1,1,0}
  },
  {
    {0,0,0,0,0,0,0,0},
    {0,0,1,1,1,1,0,0},
    {0,1,1,0,0,1,1,0},
    {0,0,0,0,0,1,1,0},
    {0,0,0,0,1,1,0,0},
    {0,0,1,1,0,0,0,0},
    {0,1,1,0,0,0,0,0},
    {0,1,1,1,1,1,1,0}
  },
  {
    {0,0,0,0,0,0,0,0},
    {0,0,1,1,1,1,0,0},
    {0,1,1,0,0,1,1,0},
    {0,0,0,0,0,1,1,0},
    {0,0,0,1,1,1,0,0},
    {0,0,0,0,0,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,0,1,1,1,1,0,0}
  },
  {
    {0,0,0,0,0,0,0,0},
    {0,0,0,0,1,1,0,0},
    {0,0,0,1,1,1,0,0},
    {0,0,1,0,1,1,0,0},
    {0,1,0,0,1,1,0,0},
    {0,1,1,1,1,1,1,0},
    {0,0,0,0,1,1,0,0},
    {0,0,0,0,1,1,0,0}
  },
  {
    {0,0,0,0,0,0,0,0},
    {0,1,1,1,1,1,1,0},
    {0,1,1,0,0,0,0,0},
    {0,1,1,1,1,1,0,0},
    {0,0,0,0,0,1,1,0},
    {0,0,0,0,0,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,0,1,1,1,1,0,0}
  },
  {
    {0,0,0,0,0,0,0,0},
    {0,0,1,1,1,1,0,0},
    {0,1,1,0,0,1,1,0},
    {0,1,1,0,0,0,0,0},
    {0,1,1,1,1,1,0,0},
    {0,1,1,0,0,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,0,1,1,1,1,0,0}
  },
  {
    {0,0,0,0,0,0,0,0},
    {0,1,1,1,1,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,0,0,0,1,1,0,0},
    {0,0,0,0,1,1,0,0},
    {0,0,0,1,1,0,0,0},
    {0,0,0,1,1,0,0,0},
    {0,0,0,1,1,0,0,0}
  },
  {
    {0,0,0,0,0,0,0,0},
    {0,0,1,1,1,1,0,0},
    {0,1,1,0,0,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,0,1,1,1,1,0,0},
    {0,1,1,0,0,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,0,1,1,1,1,0,0}
  },
  {
    {0,0,0,0,0,0,0,0},
    {0,0,1,1,1,1,0,0},
    {0,1,1,0,0,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,0,1,1,1,1,1,0},
    {0,0,0,0,0,1,1,0},
    {0,1,1,0,0,1,1,0},
    {0,0,1,1,1,1,0,0}
  }
};


// scrolls the 'snake' message around the matrix
void showSnakeMessage() {

  for (int d = 0; d < sizeof(snejkMessage[0]) - 7; d++) {
    for (int col = 0; col < 8; col++) {
      delay(messageSpeed);
      for (int row = 0; row < 8; row++) {
        // this reads the byte from the PROGMEM and displays it on the screen
        matrix.setLed(0, row, col, pgm_read_byte(&(snejkMessage[row][col + d])));
      }
    }
  }
}


// scrolls the 'game over' message around the matrix
void showGameOverMessage() {
  for (int d = 0; d < sizeof(gameOverMessage[0]) - 7; d++) {
    for (int col = 0; col < 8; col++) {
      delay(messageSpeed);
      for (int row = 0; row < 8; row++) {
        // this reads the byte from the PROGMEM and displays it on the screen
        matrix.setLed(0, row, col, pgm_read_byte(&(gameOverMessage[row][col + d])));
      }
    }
  }
}


// scrolls the 'win' message around the matrix
void showWinMessage() {
  // not implemented yet // TODO: implement it
}


// scrolls the 'score' message with numbers around the matrix
void showScoreMessage(int score) {
  if (score < 0 || score > 99) return;

  // specify score digits
  int second = score % 10;
  int first = (score / 10) % 10;

  for (int d = 0; d < sizeof(scoreMessage[0]) + 2 * sizeof(digits[0][0]); d++) {
    for (int col = 0; col < 8; col++) {
      delay(messageSpeed);
      for (int row = 0; row < 8; row++) {
        if (d <= sizeof(scoreMessage[0]) - 8) {
          matrix.setLed(0, row, col, pgm_read_byte(&(scoreMessage[row][col + d])));
        }

        int c = col + d - sizeof(scoreMessage[0]) + 6; // move 6 px in front of the previous message

        // if the score is < 10, shift out the first digit (zero)
        if (score < 10) c += 8;

        if (c >= 0 && c < 8) {
          if (first > 0) matrix.setLed(0, row, col, pgm_read_byte(&(digits[first][row][c]))); // show only if score is >= 10 (see above)
        } else {
          c -= 8;
          if (c >= 0 && c < 8) {
            matrix.setLed(0, row, col, pgm_read_byte(&(digits[second][row][c]))); // show always
          }
        }
      }
    }
  }
}


// standard map function, but with floats
float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void startSound() {
  beep(c, 200);
  beep(d, 200);
  beep(f, 200);
  beep(gS, 800);
  beep(gSH, 32);
  beep(gH, 17);
  beep(fSH, 12);
  beep(fH, 12);    
  beep(fSH, 25);
}

void gamoverSound() {
  beep(f, 200);
  beep(f, 200);
  beep(f, 200);
  beep(c, 800);
  beep(gSH, 32);
  beep(gH, 27);
  beep(fSH, 22);
  beep(fH, 22);    
  beep(fSH, 25);
}

void beep(int note, int duration) {
  tone(Pin::buzzerPin, note, duration);
  delay(duration);
  noTone(Pin::buzzerPin);
  delay(10);
}


