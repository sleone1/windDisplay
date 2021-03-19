#include <LCDWIKI_GUI.h>  // core graphics library
#include <LCDWIKI_KBV.h>  // hardware-specific library

// constructed function for known IC model
LCDWIKI_KBV mylcd(ILI9486, A3, A2, A1, A0, A4);  // model, cs, cd, wr, rd, reset

// define color values
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

/* direction variables */
const int SENSOR = A5;

const int OFFSET = 0;  // this should let you calibrate the whole rotation sensor

int currValue;
int prevValue = 0;

int dir;
int prevDir = 0;

/* speed variables */
long startTime, endTime;
const int BUTTON = 12; // speed instrument pin

const int TIME_INT = 1000;  // ms
const int TIME_SPAN = 2;  // STILL ALSO HAVE TO CHANGE ARRAY INITALIZATION STATEMENT
const int RADIUS = 9;  // cm
const int MY_PI = 3.14;

volatile int pushCount = 0;

int cmPerS;
float knots;

int currState = 0;
int prevState = 0;

// display parameter as string
void displayDegree(float x) {
  mylcd.Set_Text_Mode(0);
  
  mylcd.Set_Text_Back_colour(WHITE);
  mylcd.Set_Text_colour(BLACK);  
  mylcd.Set_Text_Size(5);

  // correct to read degrees off wind (0 to 180)
  if (x > 180) {
    x = 360 - x;
  }
  
  mylcd.Print_Number_Float(x, 1, RIGHT, 240, '.', 5, ' ');
}

// display parameter as string
void displaySpeed(float x)
{
  mylcd.Set_Text_Mode(0);
  
  mylcd.Set_Text_Back_colour(WHITE);
  mylcd.Set_Text_colour(BLACK);  
  mylcd.Set_Text_Size(5);
  
  mylcd.Print_Number_Float(x, 1, RIGHT, 0, '.', 5, ' ');
}

// draw or remove arrow at given degree
void displayArrow(int degree, bool toRemove) {
  int x1, y1, x2, y2, x3, y3;

  degree -= 90;
  float angle = degree * PI / 180;  // convert degrees to radians
  
  x1 = 160 + (145 * cos(angle));
  y1 = 160 + (145 * sin(angle));

  x2 = 160 + (150 * cos(angle));
  y2 = 160 + (150 * sin(angle));

  x3 = 160 + (157 * cos(angle));
  y3 = 160 + (157 * sin(angle));

  if (toRemove) {
    /*if (degree > 25 and degree < 35) {
      markZone();
    }*/

    mylcd.Set_Draw_color(WHITE);
  } else {
    mylcd.Set_Draw_color(BLACK);
  }

  for (int w = 0; w == 0; w++) {
    mylcd.Draw_Line(160 + w, 160, x1 + w, y1);
    //mylcd.Draw_Line(160, 160 + w, x1, y1 + w);
    
    mylcd.Draw_Line(x2 + w, y2, x3 + w, y3);
    //mylcd.Draw_Line(x2, y2 + w, x3, y3 + w);
  }
}

void markZone() {
  int x1, y1, x2, y2;
  
  x1 = 160 + (157 * cos(240 * PI / 180));
  y1 = 160 + (157 * sin(240 * PI / 180));

  x2 = 160 + (157 * cos(300 * PI / 180));
  y2 = 160 + (157 * sin(300 * PI / 180));
  
  mylcd.Set_Draw_color(RED);
  mylcd.Draw_Line(89, 36, 82, 24);
  mylcd.Draw_Line(232, 36, 239, 24);
}

ISR (PCINT0_vect) {    
  pushCount += 1;
  mylcd.Print_Number_Float(pushCount, 1, RIGHT, 70, '.', 5, ' ');
}

/* implement a five-long queue-like structure to track over time */
int queue[2] = {0, 0};  // CHANGE IF YOU CHANGED TIME SPAN

/* insert new element x at end, replacing oldest element */
void push(int x) {
  // fill in if empty array
  for (byte i = 0; i < TIME_SPAN; i++) {
    if (queue[i] == 0) {
      queue[i] = x;
    }
  }
  // insert at end of array
  for (byte i = 0; i < TIME_SPAN - 1; i++) {
    queue[i] = queue[i + 1];
  }
  queue[TIME_SPAN - 1] = x;
}

// runs once
void setup() {
  Serial.begin(9600);
  pinMode(BUTTON, INPUT);
  mylcd.Init_LCD();

  // inital screen setup
  mylcd.Set_Rotation(1);
  mylcd.Fill_Screen(WHITE);

  // wind circle setup
  mylcd.Set_Draw_color(BLACK);
  mylcd.Fill_Circle(160, 160, 150);
  mylcd.Set_Draw_color(WHITE);
  mylcd.Fill_Circle(160, 160, 145);
  markZone();

  // set up inturrupt for speed rotation count
  *digitalPinToPCMSK(BUTTON) |= bit (digitalPinToPCMSKbit(BUTTON));  // enable pin
  PCIFR  |= bit (digitalPinToPCICRbit(BUTTON));  // clear any outstanding interrupt
  PCICR  |= bit (digitalPinToPCICRbit(BUTTON));  // enable interrupt for the group

  // initalize timing
  startTime = millis();
  endTime = millis();
}

// runs forever
void loop() {
  /* direction */
  // read sensor value
  currValue = analogRead(SENSOR);
  
  // scale sensor value to be between 0 and 360
  dir = map(currValue, 0, 1023, 0, 360);
  
  dir = dir + OFFSET;
  // ensure all directions are within first rotation (ex: 400 -> 40)
  if (dir > 360) {
    dir -= 360;
  } else if (dir < 0) {
    dir += 360;
  }

  // update display
  displayDegree(dir);
    
  displayArrow(prevDir, true);
  displayArrow(dir, false);

  prevDir = dir;

  /* speed */
  // do every (TIME_INT/1000) seconds
  if (endTime - startTime >= TIME_INT) {
    // calculate knots
    noInterrupts();
    pushCount /= 2;
    cmPerS = (pushCount * (2 * MY_PI * RADIUS)) / ((endTime - startTime) / 1000);  // atomic
    interrupts();
    knots = cmPerS / 51.444;

    // update running speed queue
    push(knots);

    // calculate overall speed
    int overall = 0;
    for (byte i = 0; i < TIME_SPAN; i++) {
      overall += queue[i];
    }
    overall /= TIME_SPAN;
    
    // update display
    displaySpeed(overall);

    // reset counters
    noInterrupts();
    pushCount = 0;  // atomic
    interrupts();
    startTime = millis();
  }
  
  endTime = millis();
}
