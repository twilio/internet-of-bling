#include <math.h>
#include "application.h"
#include "cellular_hal.h"
#include "neopixel.h"
#include "sms.h"

/* ALL_LEVEL, TRACE_LEVEL, DEBUG_LEVEL, INFO_LEVEL, WARN_LEVEL, ERROR_LEVEL, PANIC_LEVEL, NO_LOG_LEVEL */
SerialDebugOutput debugOutput(9600, ALL_LEVEL);

/* Set up Twilio APN - required with Twilio SIM */
STARTUP(cellular_credentials_set("wireless.twilio.com", "", "", NULL));

/* Receive SMS callback. Requires base firmware 0.5.1.rc2 
https://github.com/spark/firmware/releases/tag/v0.5.1-rc.2 */
STARTUP(cellular_sms_received_handler_set(sms_received, NULL, NULL));

SYSTEM_MODE(AUTOMATIC);

/* Set up NeoPixel Ring */
#define PIXEL_PIN D6
#define PIXEL_COUNT 24
#define PIXEL_TYPE WS2812B
Adafruit_NeoPixel ring = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);

/* The pre-programmed states our light can support */
enum LightState {IDLE = 0, RED = 1, PINK = 2, WHITE = 3, RAINBOW = 4, WAKEUP = 5, DISCO = 6};
LightState lightState;

volatile int newSMS = 0;
String smsBody;

void setStateFromIncomingKeyword(String command);
bool cloudFunctionSetState(String command);
void pubsubSetState(const char *event, const char *data);
void smsCommandSetState(String body);
void colorWipe(uint32_t c, uint8_t wait);
void colorAll(uint32_t c, uint8_t wait);
void rainbow(uint8_t wait);
uint32_t Wheel(byte WheelPos);
void wakeup();
void disco();

void setup()
{
  /* Send a ping to Particle servers every 15 seconds to avoid UDP session timeouts.
  This is overly aggressive, but the battery life remains 9 hours or more when neopixel
  is not illuminated. */
  Particle.keepAlive(15);
  /*
  There are three ways to set the state of the NeoPixel ring. By Particle Cloud
  function, by Particle Pub/Sub or by Twilio Wireless Command (which arrives via SMS).
  */
  Particle.function("setState", cloudFunctionSetState);

  /* Public Particle Subscribe channel; create a channel named with a UUID, 
  or publish privately. See: https://docs.particle.io/reference/firmware/electron/#particle-publish-  */
  Particle.subscribe("ENTER_UNIQUE_CHANNEL_NAME_HERE", pubsubSetState);
  
  /* Delete all potentially stale SMS from the modem */
  SmsDeleteFlag flag = DELETE_ALL;
  smsDeleteAll(flag);

  /* Set Neopixel ring to unilluminated on boot. */
  ring.begin();
  lightState = IDLE;
  ring.show();
}

/* ----------------------------------------------

     BIG OL' STATE MACHINE FOR THE LIGHT RING

------------------------------------------------ */

void loop()
{
    if (newSMS > 0) {
      smsBody = checkUnreadSMS();
      if (smsBody != NULL) {
          smsCommandSetState(body);
      }
      if (newSMS > 0) newSMS--;
    }

    switch(lightState) {
     case IDLE:
        colorWipe(0, 0);
        break;
      case RED:
        colorWipe(ring.Color(255, 0, 0), 50);
        break;
      case PINK:
        colorWipe(ring.Color(243, 0, 143), 50);
        break;
      case WHITE:
        colorWipe(ring.Color(255, 255, 255), 50);
        break;
      case RAINBOW:
        rainbow(20);
        break;
      case WAKEUP:
        wakeup();
        break;
      case DISCO:
        disco();
        break;
    }
}

void setStateFromIncomingKeyword(String command) {
  command.toLowerCase();
  
  Serial.print("Setting light state to: ");
  Serial.println(command);
  
  if (command == "red") {
    lightState = RED;
  } else if  (command == "pink") {
    lightState = PINK;
  } else if  (command == "white") {
    lightState = WHITE;
  } else if  (command == "rainbow") {
    lightState = RAINBOW;
  } else if  (command == "wakeup") {
    lightState = WAKEUP;
  } else if  (command == "disco") {
    lightState = DISCO;
  } else {
    lightState = IDLE;
  }
}

/* ----------------------------------------------

     INCOMING FUNCTION CALLS FROM PARTICLE CLOUD

------------------------------------------------ */

/* Particle Cloud Function */
bool cloudFunctionSetState(String command) {
  String keyword = command.substring(0, command.indexOf(' '));
  setStateFromIncomingKeyword(keyword);
  return 0;
}

/* Particle Pub/Sub */
void pubsubSetState(const char *event, const char *data) {
  String command = String(data);
  String keyword = command.substring(0, command.indexOf(' '));
  setStateFromIncomingKeyword(keyword);
}

/* ----------------------------------------------

     INCOMING SMS-BASED COMMANDS FROM TWILIO

------------------------------------------------ */
void sms_received(void* data, int index)
{
    newSMS++;
}

void smsCommandSetState(String body) {
  /* Strip CommandSid from beginning of SMS body */
   String keyword = body.substring(body.indexOf(" ") + 1);
   setStateFromIncomingKeyword(keyword);
}

/* ----------------------------------------------

     NEOPIXEL PATTERNS

------------------------------------------------ */

/* Fill the dots one after the other with a color, wait (ms) after each one */
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<ring.numPixels(); i++) {
    ring.setPixelColor(i, c);
    ring.show();
    delay(wait);
  }
}

/* Set all pixels in the ring to a solid color, then wait (ms) */
void colorAll(uint32_t c, uint8_t wait) {
  uint16_t i;

  for(i=0; i<ring.numPixels(); i++) {
    ring.setPixelColor(i, c);
  }
  ring.show();
  delay(wait);
}

/* Make a rainbow! Depends on Wheel. */
void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<ring.numPixels(); i++) {
      ring.setPixelColor(i, Wheel((i+j) & 255));
    }
    ring.show();
    delay(wait);
  }
}

/* Input a value 0 to 255 to get a color value.
The colours are a transition r - g - b - back to r. */
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return ring.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return ring.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return ring.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

/* Wakeup - "Breathe" function - emulates Macbook style sleep light.
https://www.youtube.com/watch?v=mrojrDCI02k */
void wakeup() {
  float val = (exp(sin(millis()/2000.0 * 3.14159)) - 0.36787944) * 108.0;
  for (int i=0; i<ring.numPixels(); i++) {
    ring.setPixelColor(i, ring.Color(val, val, val));
  }
  ring.show();
}

/* Disco mode! */
int32_t whiteColor = ring.Color(200, 250, 250);
int32_t pinkColor = ring.Color(250, 50, 200);
int32_t blueColor = ring.Color(100, 250, 200);

void disco() {
  long r, r_bright;
  r = random(0, 100);
  r_bright = random(1, 4);

  for(int i=0; i<ring.numPixels(); i++ ){
    if( r < 30 ){
      ring.setPixelColor(i, pinkColor/r_bright);
    } else if( r < 60 ) {
      ring.setPixelColor(i, whiteColor/r_bright);
    } else {
      ring.setPixelColor(i, blueColor/r_bright);
    }
  }
  ring.show();
  delay(500/r_bright);
}
