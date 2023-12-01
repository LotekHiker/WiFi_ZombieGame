#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <FastLED.h>

const char *password = "Thecakeisalie!"; // not important, we only scan

// How long will the game last in minutes
int gameLength = 2.5;
// Convert game duration to millis.
const unsigned long eventInterval = gameLength * 60000;

// WS2812 - LED-strip
#define DATA_PIN 12
#define NUM_LEDS 9
CRGB leds[NUM_LEDS];

int rssis[3];

// Game wifi channel
#define GAME_CHANNEL 13 // Lucky 13

#define ZOMBIE 0
const char *ZOMBIE_SSID = "zombie";
#define HUMAN  1
const char *HUMAN_SSID = "human";
#define GAMEOVER 3 // Make the program do something when the game is over some day.

// Array of color indexed from 0, Red is colour[0]. Easy way to change the different led colors.
uint32_t colour[] = {
  CRGB::Red, // colour[0] - Zombie
  CRGB::Green, // colour[1] - Human
  CRGB::Blue, // colour[2] - Start game scan color
  CRGB::Black, // colour[3]
  CRGB::Purple, // colour[4] - End of game color // Currently not used.
  };

// rssi equal/lower than this means no LED is on
#define FAR_LIMIT     -100
// rssi equal/higher than this means all LEDs are on
#define NEAR_LIMIT    -60
// game starts whenn other players are this 'far' away
#define START_LIMIT   -90
// infection is transmitted of rssi is equal/higher than this
#define INFECT_LIMIT  -50

int i_am;
const char *my_ssid;
const char *other_ssid;

// Let's see who is around
int scanWifi(const char *ssid) {
  // scan WiFi for ssid, select strongest signal, return average over the last three scans
  int rssi = FAR_LIMIT;
  int n = WiFi.scanNetworks(false, false, GAME_CHANNEL);
  if (n > 0) {
    for (int i = 0; i < n; ++i) {
      if (WiFi.SSID(i) == ssid && WiFi.RSSI(i) > rssi)
        rssi = WiFi.RSSI(i);
    }
  }
  rssis[2] = rssis[1];  rssis[1] = rssis[0];  rssis[0] = rssi;  // store last three scans
  return (rssis[0] + rssis[1] + rssis[2] ) / 3; // average
}

void   resetRSSIs() { // reset stored rssis
  rssis[0] = rssis[1] = rssis[2] = FAR_LIMIT;
}

// Check to see if the game duration has expired
void timeCheck(){
  unsigned long currentTime = millis();  //get the current "time" (actually the number of milliseconds since the program started)
  if (currentTime >= eventInterval) {  //test whether the period has elapsed
  CRGB color = i_am == ZOMBIE ? colour[0] : colour[1];
  i_am = GAMEOVER;
  while (i_am == GAMEOVER)    flashLEDs(color); // Flash lights to indicate end of game and team we're on
   }
   return;
}

// Blinky lights!
void flashLEDs(CRGB color) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = color;
    FastLED.show();
    delay(10);
  }
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = colour[3];
    FastLED.show();
    delay(10);
  }
}

void setup() {
  
  // turn builtin led off
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);
  
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 10); // limit power used by LEDs

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();
  WiFi.begin("blahblah", "blahblah");

  resetRSSIs();
  int i, rssi;
  for (i = 0; i < 10; i++) { // scan for max ten times (~10s)
    flashLEDs(colour[2]);
    if ((rssi = scanWifi(ZOMBIE_SSID)) > FAR_LIMIT)
      break;
  }
  if (rssi == FAR_LIMIT) { // No zombie arround... I'm the zombie
    i_am = ZOMBIE;
    my_ssid = ZOMBIE_SSID;
    other_ssid = HUMAN_SSID;
  } else {
    i_am = HUMAN;
    my_ssid = HUMAN_SSID;
    other_ssid = ZOMBIE_SSID;
  }

  resetRSSIs();
  CRGB color = i_am == ZOMBIE ? colour[0] : colour[1];
  flashLEDs(color); // show one flash sequence early, beacaus WiFi.softAP might need some seconds
  WiFi.softAP(my_ssid, password, GAME_CHANNEL, 0, 0);

  for (int j = 0; j < 10; j++) // show what we play as
    flashLEDs(color);

    color = i_am == ZOMBIE ? colour[0] : colour[1];
  while (scanWifi(other_ssid) <= FAR_LIMIT)  flashLEDs(color); // wait for other players to show up
    color = i_am == ZOMBIE ? colour[0] : colour[1];
  while (scanWifi(other_ssid) > START_LIMIT)    flashLEDs(color); // wait for other players go hiding

  resetRSSIs();
}

void loop() {
  int rssi;
  
 timeCheck(); // Time to end the game?

  rssi = scanWifi(other_ssid);

  // Uh oh, got too close to a Zombie!
  if (i_am == HUMAN && rssi > INFECT_LIMIT) { // become a zombie
    // inform of infection
    for (int i = 0; i < 10; i++)
      flashLEDs(colour[0]);
    i_am = ZOMBIE;
    my_ssid = ZOMBIE_SSID;
    other_ssid = HUMAN_SSID;
    WiFi.softAP(my_ssid, password, GAME_CHANNEL, 0, 0);
    resetRSSIs();
    rssi = scanWifi(other_ssid);
  }

  if (rssi < FAR_LIMIT) rssi = FAR_LIMIT;
  if (rssi > NEAR_LIMIT) rssi = NEAR_LIMIT;
  // calculate number of LEDs to light
  // linear range transformation from FAR_LIMIT..NEAR_LIMIT to 0..NUM_LEDS
  int numleds = (rssi - FAR_LIMIT) * NUM_LEDS / (NEAR_LIMIT - FAR_LIMIT);

  leds[NUM_LEDS - 1] = i_am == ZOMBIE ? colour[0] : colour[1]; // top LED is 'what am i' marker if unused

  CRGB color = i_am == HUMAN ? colour[0] : colour[1];
  int i = 0;
  for (; i < numleds; i++)
    leds[i] = color;
  for (; i < NUM_LEDS - 1; i++) // remaining black/dark
    leds[i] = colour[3];

  FastLED.show();
}
