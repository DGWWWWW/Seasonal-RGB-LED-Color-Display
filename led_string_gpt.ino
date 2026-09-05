
// version 1.1 Aug  5, 2026
#include <FastLED.h>
#include <WiFiS3.h>

// Enter your WiFi network name and password between the quotation marks.
#define SECRET_SSID "??????????????"
#define SECRET_PASS "????????????????????"

#define DATA_PIN     8
#define NUM_LEDS     50
#define LED_TYPE     WS2811
#define COLOR_ORDER  RGB

CRGBArray<NUM_LEDS> leds;
CRGB currentPattern[NUM_LEDS];
CRGB targetPattern[NUM_LEDS];

uint8_t twinkleLevel[NUM_LEDS];
bool twinkleRising[NUM_LEDS];

uint8_t fireflyLevel[NUM_LEDS];
bool fireflyRising[NUM_LEDS];
bool fireflyEnabled[NUM_LEDS];
unsigned long nextFireflyTime[NUM_LEDS];

const CRGB FIREFLY_COLOR = CRGB::Yellow;
const uint8_t NORMAL_LEVEL = 70;

const uint8_t brightnessLevels[4] = {
  153,  // B1: 60%
  179,  // B2: 70%
  204,  // B3: 80%
  230   // B4: 90%
};

uint8_t currentBrightness = brightnessLevels[0];
uint8_t twinkleRate = 0;
uint8_t fireflyRate = 0;

unsigned long previousEffectUpdate = 0;

WiFiServer webServer(80);
bool wifiConnected = false;

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(100);
  delay(2000);

  // Connect before FastLED is initialized. This matches the basic WiFi test.
  connectWiFi();

  pinMode(DATA_PIN, OUTPUT);
  digitalWrite(DATA_PIN, LOW);

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(currentBrightness);

  fill_solid(currentPattern, NUM_LEDS, CRGB::Black);
  fill_solid(targetPattern, NUM_LEDS, CRGB::Black);
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  clearTwinkles();
  clearFireflies();
  FastLED.show();

  randomSeed(analogRead(A0) ^ micros());
  printMenu();
}

void loop() {
  checkSerial();
  checkWebClient();

  if (millis() - previousEffectUpdate >= 20) {
    previousEffectUpdate = millis();

    if (fireflyRate > 0) {
      updateFireflies();
      displayFireflies();
    } else if (twinkleRate > 0) {
      updateTwinkles();
      displayCurrentPattern();
    }
  }
}

void checkSerial() {
  if (!Serial.available()) {
    return;
  }

  String command = Serial.readStringUntil('\n');
  command.trim();
  command.toUpperCase();

  if (command.length() == 0) {
    return;
  }

  processCommand(command);
}

void processCommand(String command) {
  command.trim();
  command.toUpperCase();

  if (command == "0") {
    stopFireflies();
    fill_solid(targetPattern, NUM_LEDS, CRGB::Black);
    fadeToTarget();
    Serial.println(F("OFF"));
  } else if (command == "1") {
    stopFireflies();
    makeTwoColorPattern(CRGB::Red, CRGB::Green);
    fadeToTarget();
    Serial.println(F("RED / GREEN"));
  } else if (command == "2") {
    stopFireflies();
    makeTwoColorPattern(CRGB::Red, CRGB::White);
    fadeToTarget();
    Serial.println(F("RED / WHITE"));
  } else if (command == "3") {
    stopFireflies();
    makeTwoColorPattern(CRGB::Green, CRGB::White);
    fadeToTarget();
    Serial.println(F("GREEN / WHITE"));
  } else if (command == "4") {
    stopFireflies();
    makeOriginalOrangeVioletPattern();
    fadeToTarget();
    Serial.println(F("ORANGE / VIOLET"));
  } else if (command == "5") {
    stopFireflies();
    makeTwoColorPattern(CRGB::White, CRGB::Blue);
    fadeToTarget();
    Serial.println(F("WHITE / BLUE"));
  } else if (command == "6") {
    stopFireflies();
    makeThreeColorPattern(CRGB::Red, CRGB::White, CRGB::Blue);
    fadeToTarget();
    Serial.println(F("RED / WHITE / BLUE"));
  } else if (command == "7") {
    stopFireflies();
    fill_solid(targetPattern, NUM_LEDS, CRGB::White);
    fadeToTarget();
    Serial.println(F("SOLID WHITE"));
  } else if (command == "8") {
    stopFireflies();
    makeRainbowPattern();
    fadeToTarget();
    Serial.println(F("RAINBOW"));
  } else if (command == "9") {
    stopFireflies();
    makeOriginalVioletYellowPattern();
    fadeToTarget();
    Serial.println(F("VIOLET / YELLOW"));
  } else if (command == "T0") {
    twinkleRate = 0;
    clearTwinkles();
    displayCurrentPattern();
    Serial.println(F("TWINKLE OFF"));
  } else if (command == "T1" || command == "T2" || command == "T3") {
    stopFireflies();
    twinkleRate = command.charAt(1) - '0';
    clearTwinkles();
    displayCurrentPattern();
    Serial.print(F("TWINKLE RATE "));
    Serial.println(twinkleRate);
  } else if (command == "L1" || command == "L2" || command == "L3") {
    startFireflies(command.charAt(1) - '0');
  } else if (command == "S") {
    if (fireflyRate > 0) {
      Serial.println(F("Select a color pattern before using S."));
    } else {
      runSwell();
      Serial.println(F("SWELL COMPLETE"));
    }
  } else if (command.length() == 2 && command.charAt(0) == 'B' &&
             command.charAt(1) >= '1' && command.charAt(1) <= '4') {
    uint8_t level = command.charAt(1) - '1';
    fadeBrightness(brightnessLevels[level]);
    Serial.print(F("BRIGHTNESS B"));
    Serial.println(level + 1);
  } else if (command == "M") {
    printMenu();
  } else {
    Serial.println(F("Unknown command. Enter M for menu."));
  }
}

void connectWiFi() {
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("WiFi module not found. Serial control remains available."));
    return;
  }

  Serial.print(F("Connecting to WiFi: "));
  Serial.println(SECRET_SSID);

  WiFi.begin(SECRET_SSID, SECRET_PASS);

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 30000) {
    Serial.print('.');
    delay(500);
  }

  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi connection failed. Serial control remains available."));
    return;
  }

  // Confirm that DHCP supplied the same information seen in the basic test.
  IPAddress localAddress = WiFi.localIP();

  Serial.print(F("IP address: "));
  Serial.println(localAddress);
  Serial.print(F("Subnet mask: "));
  Serial.println(WiFi.subnetMask());
  Serial.print(F("Gateway: "));
  Serial.println(WiFi.gatewayIP());

  if (localAddress[0] == 0 && localAddress[1] == 0 &&
      localAddress[2] == 0 && localAddress[3] == 0) {
    Serial.print(F("Waiting for DHCP"));

    for (int attempt = 0;
       attempt < 80 &&
       localAddress[0] == 0 && localAddress[1] == 0 &&
       localAddress[2] == 0 && localAddress[3] == 0;
       attempt++) {
    delay(250);
    Serial.print('.');
    localAddress = WiFi.localIP();
  }

    Serial.println();
  }

  if (localAddress[0] == 0 && localAddress[1] == 0 &&
      localAddress[2] == 0 && localAddress[3] == 0) {
    Serial.println(F("DHCP failed: no IP address was assigned."));
    Serial.println(F("Serial control remains available."));
    return;
  }

  wifiConnected = true;
  webServer.begin();

  Serial.println(F("WiFi connected."));
  Serial.print(F("Open this address in a browser: http://"));
  Serial.println(localAddress);
}

void checkWebClient() {
  if (!wifiConnected) {
    return;
  }

  WiFiClient client = webServer.available();

  if (!client) {
    return;
  }

  client.setTimeout(250);
  String requestLine = client.readStringUntil('\r');

  int commandStart = requestLine.indexOf("GET /?c=");

  if (commandStart >= 0) {
    commandStart += 8;
    int commandEnd = requestLine.indexOf(' ', commandStart);

    if (commandEnd > commandStart) {
      String command = requestLine.substring(commandStart, commandEnd);
      processCommand(command);
    }
  }

  while (client.available()) {
    client.read();
  }

  sendWebPage(client);
  delay(1);
  client.stop();
}

void webButton(WiFiClient &client, const __FlashStringHelper *command,
               const __FlashStringHelper *label) {
  client.print(F("<a class='button' href='/?c="));
  client.print(command);
  client.print(F("'>"));
  client.print(label);
  client.println(F("</a>"));
}

void sendWebPage(WiFiClient &client) {
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println(F("Connection: close"));
  client.println();

  client.println(F("<!DOCTYPE html><html><head>"));
  client.println(F("<meta name='viewport' content='width=device-width,initial-scale=1'>"));
  client.println(F("<title>Seasonal LEDs</title>"));
  client.println(F("<style>body{font-family:Arial;text-align:center;background:#151515;color:white;margin:18px}"));
  client.println(F("h2{margin-top:24px}.grid{display:grid;grid-template-columns:repeat(2,minmax(130px,1fr));gap:10px;max-width:520px;margin:auto}"));
  client.println(F(".button{display:block;padding:15px 8px;background:#356a9a;color:white;text-decoration:none;border-radius:8px;font-size:17px}"));
  client.println(F(".off{background:#8b3030}</style></head><body>"));
  client.println(F("<h1>Seasonal LED Control</h1>"));

  client.println(F("<h2>Displays</h2><div class='grid'>"));
  webButton(client, F("1"), F("Red / Green"));
  webButton(client, F("2"), F("Red / White"));
  webButton(client, F("3"), F("Green / White"));
  webButton(client, F("4"), F("Orange / Violet"));
  webButton(client, F("5"), F("White / Blue"));
  webButton(client, F("6"), F("Red / White / Blue"));
  webButton(client, F("7"), F("Solid White"));
  webButton(client, F("8"), F("Rainbow"));
  webButton(client, F("9"), F("Violet / Yellow"));
  client.println(F("<a class='button off' href='/?c=0'>Off</a></div>"));

  client.println(F("<h2>Twinkle</h2><div class='grid'>"));
  webButton(client, F("T0"), F("Twinkle Off"));
  webButton(client, F("T1"), F("Gentle"));
  webButton(client, F("T2"), F("Medium"));
  webButton(client, F("T3"), F("Lively"));
  client.println(F("</div>"));

  client.println(F("<h2>Fireflies</h2><div class='grid'>"));
  webButton(client, F("L1"), F("Sparse"));
  webButton(client, F("L2"), F("Medium"));
  webButton(client, F("L3"), F("Frequent"));
  client.println(F("</div>"));

  client.println(F("<h2>Brightness</h2><div class='grid'>"));
  webButton(client, F("B1"), F("B1"));
  webButton(client, F("B2"), F("B2"));
  webButton(client, F("B3"), F("B3"));
  webButton(client, F("B4"), F("B4"));
  client.println(F("</div>"));

  client.println(F("<h2>One-time Effect</h2><div class='grid'>"));
  webButton(client, F("S"), F("Center-to-Ends Swell"));
  client.println(F("</div></body></html>"));
}

void makeTwoColorPattern(CRGB color1, CRGB color2) {
  for (int i = 0; i < NUM_LEDS; i++) {
    targetPattern[i] = ((i % 2) == 0) ? color1 : color2;
  }
}

void makeThreeColorPattern(CRGB color1, CRGB color2, CRGB color3) {
  for (int i = 0; i < NUM_LEDS; i++) {
    switch (i % 3) {
      case 0: targetPattern[i] = color1; break;
      case 1: targetPattern[i] = color2; break;
      default: targetPattern[i] = color3; break;
    }
  }
}

void makeOriginalOrangeVioletPattern() {
  for (int i = 0; i < NUM_LEDS; i++) {
    if ((i % 2) == 0) {
      // Original code: hue 84, saturation 255. The original
      // NEOPIXEL setting exchanged red and green on this string.
      targetPattern[i].setHSV(84, 255, 255);
    } else {
      // Original code: hue 127, saturation 255.
      targetPattern[i].setHSV(127, 255, 255);
    }

    // Reproduce the colors as they appeared with the original code,
    // without disturbing the corrected RGB order used by other patterns.
    uint8_t savedRed = targetPattern[i].r;
    targetPattern[i].r = targetPattern[i].g;
    targetPattern[i].g = savedRed;
  }
}

void makeOriginalVioletYellowPattern() {
  for (int i = 0; i < NUM_LEDS; i++) {
    if ((i % 2) == 0) {
      // Original code: violet hue 127, saturation 255.
      targetPattern[i].setHSV(127, 255, 255);
    } else {
      // Original code: yellow hue 64, saturation 255.
      targetPattern[i].setHSV(64, 255, 255);
    }

    // Reproduce the colors as they appeared with the original code.
    uint8_t savedRed = targetPattern[i].r;
    targetPattern[i].r = targetPattern[i].g;
    targetPattern[i].g = savedRed;
  }
}

void makeRainbowPattern() {
  fill_rainbow(targetPattern, NUM_LEDS, 0, 10);
}

void fadeToTarget() {
  CRGB startingPattern[NUM_LEDS];
  CRGB scaledTarget[NUM_LEDS];

  clearTwinkles();

  for (int i = 0; i < NUM_LEDS; i++) {
    startingPattern[i] = leds[i];
    scaledTarget[i] = targetPattern[i];
    scaledTarget[i].nscale8_video(NORMAL_LEVEL);
  }

  for (int step = 0; step <= 100; step++) {
    uint8_t amount = map(step, 0, 100, 0, 255);

    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = blend(startingPattern[i], scaledTarget[i], amount);
    }

    FastLED.show();
    delay(15);
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    currentPattern[i] = targetPattern[i];
  }

  displayCurrentPattern();
}

void displayCurrentPattern() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = currentPattern[i];
    uint8_t level = (twinkleLevel[i] == 0) ? NORMAL_LEVEL : twinkleLevel[i];
    leds[i].nscale8_video(level);
  }

  FastLED.show();
}

void updateTwinkles() {
  uint8_t rampStep;
  uint8_t startChance;

  switch (twinkleRate) {
    case 1: rampStep = 6;  startChance = 1; break;
    case 3: rampStep = 20; startChance = 4; break;
    default: rampStep = 12; startChance = 2; break;
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    if (twinkleLevel[i] == 0 && random(1000) < startChance) {
      twinkleLevel[i] = NORMAL_LEVEL;
      twinkleRising[i] = true;
    }

    if (twinkleLevel[i] == 0) {
      continue;
    }

    if (twinkleRising[i]) {
      if (twinkleLevel[i] >= 255 - rampStep) {
        twinkleLevel[i] = 255;
        twinkleRising[i] = false;
      } else {
        twinkleLevel[i] += rampStep;
      }
    } else {
      if (twinkleLevel[i] <= NORMAL_LEVEL + rampStep) {
        twinkleLevel[i] = 0;
      } else {
        twinkleLevel[i] -= rampStep;
      }
    }
  }
}

void clearTwinkles() {
  for (int i = 0; i < NUM_LEDS; i++) {
    twinkleLevel[i] = 0;
    twinkleRising[i] = false;
  }
}

void startFireflies(uint8_t rate) {
  twinkleRate = 0;
  fireflyRate = 0;
  clearTwinkles();
  clearFireflies();

  fill_solid(targetPattern, NUM_LEDS, CRGB::Black);
  fadeToTarget();

  fireflyRate = rate;

  uint8_t enabledCount;

  switch (fireflyRate) {
    case 1: enabledCount = 25; break;
    case 2: enabledCount = NUM_LEDS; break;
    default: enabledCount = NUM_LEDS; break;
  }

  while (enabledCount > 0) {
    int i = random(NUM_LEDS);

    if (!fireflyEnabled[i]) {
      fireflyEnabled[i] = true;
      scheduleNextFirefly(i);
      enabledCount--;
    }
  }

  Serial.print(F("FIREFLIES L"));
  Serial.println(fireflyRate);
}

void stopFireflies() {
  fireflyRate = 0;
  clearFireflies();
}

void scheduleNextFirefly(int ledNumber) {
  unsigned long waitTime;

  switch (fireflyRate) {
    case 1: waitTime = random(15000, 20001); break;
    case 2: waitTime = random(10000, 15001); break;
    default: waitTime = random(8000, 12001); break;
  }

  nextFireflyTime[ledNumber] = millis() + waitTime;
}

void updateFireflies() {
  unsigned long now = millis();

  for (int i = 0; i < NUM_LEDS; i++) {
    if (fireflyEnabled[i] && (long)(now - nextFireflyTime[i]) >= 0) {
      fireflyLevel[i] = 18;
      fireflyRising[i] = true;
      scheduleNextFirefly(i);
    }
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    if (fireflyLevel[i] == 0) {
      continue;
    }

    if (fireflyRising[i]) {
      if (fireflyLevel[i] >= 201) {
        fireflyLevel[i] = 255;
        fireflyRising[i] = false;
      } else {
        fireflyLevel[i] += 54;
      }
    } else {
      if (fireflyLevel[i] <= 18) {
        fireflyLevel[i] = 0;
      } else {
        fireflyLevel[i] -= 18;
      }
    }
  }
}

void displayFireflies() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  for (int i = 0; i < NUM_LEDS; i++) {
    if (fireflyLevel[i] != 0) {
      leds[i] = FIREFLY_COLOR;
      leds[i].nscale8_video(fireflyLevel[i]);
    }
  }

  FastLED.show();
}

void clearFireflies() {
  for (int i = 0; i < NUM_LEDS; i++) {
    fireflyLevel[i] = 0;
    fireflyRising[i] = false;
    fireflyEnabled[i] = false;
    nextFireflyTime[i] = 0;
  }
}

void runSwell() {
  const unsigned long travelTime = 3000;
  const unsigned long frameTime = 20;
  const int lastDistance = (NUM_LEDS / 2) - 1;
  const int waveHalfWidth = 3 * 256;

  clearTwinkles();

  // Continue long enough for the trailing half of the wave to leave the ends.
  const unsigned long tailTime =
    (travelTime * 3UL) / lastDistance;
  const unsigned long totalTime = travelTime + tailTime;
  unsigned long startTime = millis();

  while (millis() - startTime <= totalTime) {
    unsigned long elapsed = millis() - startTime;

    // Fixed-point wave position: 256 units equals one LED position.
    long wavePosition =
      ((long)elapsed * lastDistance * 256L) / travelTime;

    for (int i = 0; i < NUM_LEDS; i++) {
      int distanceFromCenter;

      if (i < NUM_LEDS / 2) {
        distanceFromCenter = (NUM_LEDS / 2 - 1) - i;
      } else {
        distanceFromCenter = i - (NUM_LEDS / 2);
      }

      long separation =
        labs((long)distanceFromCenter * 256L - wavePosition);

      uint8_t level = NORMAL_LEVEL;

      if (separation < waveHalfWidth) {
        long boost =
          (185L * (waveHalfWidth - separation)) / waveHalfWidth;
        level = NORMAL_LEVEL + boost;
      }

      leds[i] = currentPattern[i];
      leds[i].nscale8_video(level);
    }

    FastLED.show();
    delay(frameTime);
  }

  displayCurrentPattern();
}

void fadeBrightness(uint8_t newBrightness) {
  int startingBrightness = currentBrightness;
  int difference = (int)newBrightness - startingBrightness;

  for (int step = 1; step <= 30; step++) {
    int value = startingBrightness + ((difference * step) / 30);
    FastLED.setBrightness(value);
    FastLED.show();
    delay(20);
  }

  currentBrightness = newBrightness;
  FastLED.setBrightness(currentBrightness);
}

void printMenu() {
  Serial.println();
  Serial.println(F("SEASONAL LED CONTROL"));
  Serial.println(F("0  = Off"));
  Serial.println(F("1  = Red / Green"));
  Serial.println(F("2  = Red / White"));
  Serial.println(F("3  = Green / White"));
  Serial.println(F("4  = Orange / Violet (original HSV values)"));
  Serial.println(F("5  = White / Blue"));
  Serial.println(F("6  = Red / White / Blue"));
  Serial.println(F("7  = Solid white"));
  Serial.println(F("8  = Rainbow"));
  Serial.println(F("9  = Violet / Yellow (original HSV values)"));
  Serial.println();
  Serial.println(F("T0 = Twinkle off"));
  Serial.println(F("T1 = Gentle twinkle"));
  Serial.println(F("T2 = Medium twinkle"));
  Serial.println(F("T3 = Lively twinkle"));
  Serial.println();
  Serial.println(F("L1 = 25 fireflies; each flashes every 15-20 seconds"));
  Serial.println(F("L2 = 50 fireflies; each flashes every 10-15 seconds"));
  Serial.println(F("L3 = 50 fireflies; each flashes every 8-12 seconds"));
  Serial.println(F("     Every firefly has its own independent timer"));
  Serial.println();
  Serial.println(F("S  = One center-to-ends brightness swell"));
  Serial.println();
  Serial.println(F("B1 through B4 = 60%, 70%, 80%, 90% brightness"));
  Serial.println(F("M = Display menu"));
}
