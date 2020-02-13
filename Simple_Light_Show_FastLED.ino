#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <FastLED.h>

#define DATA_PIN 3
#define MATRIX_WIDTH 96
#define MATRIX_HEIGHT 8
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

// Define the array of leds
CRGB leds[NUM_LEDS];

unsigned long cur_time, next_show, nextShowRam;
int updateSpeed = 100;

const char* WifiSSID = "ESPap";

IPAddress local_IP(192,168,4,2);
IPAddress gateway(192,168,4,1);
IPAddress subnet(255,255,255,0);

ESP8266WebServer server(80);

void handleRoot() {
  server.send(200, "text/html",
    "<html><body><form action='/submit' method='POST'><b>Speed:</b> <input type='text' name='speed'><br /><input type='submit' value='Submit'></form></body></html>" );
}

void handleSubmit() {
  if( server.hasArg( "speed" )) {
    updateSpeed = server.arg( "speed" ).toInt();
  }
}

// 404 Error Handler
void handleNotFound() {
  Serial.println( "*** Running: handleNotFound" );
  
  String message = "404 Resource Not Found\n\n";
  
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  
  for( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  
  server.send( 404, "text/plain", message );
  Serial.printf( "   Output is %i bytes.\n", message.length() );
}

void setup() {
  Serial.begin( 115200 );
  delay( 1000 );

  FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS);

  randomSeed( analogRead( 0 ));

  Serial.print( "Setting soft-AP configuration ... " );
  Serial.println( WiFi.softAPConfig( local_IP, gateway, subnet ) ? "Started" : "FAILED!");

  Serial.print( "Starting soft-AP ... " );
  Serial.println( WiFi.softAP( WifiSSID ) ? "Started" : "FAILED!");

  Serial.print( "Soft-AP IP address : " );
  Serial.println( WiFi.softAPIP() );
  
  // Setup Server
  server.on( "/", handleRoot );
  server.on( "/submit", handleSubmit );
  server.onNotFound( handleNotFound );
  
  Serial.println( "Starting web server ... " );
  server.begin();

  cur_time = millis();
  next_show = cur_time + updateSpeed;
  nextShowRam = cur_time;

}

int cur_pixel = 0;

void loop() {
  server.handleClient();
  
  cur_time = millis();

  if( cur_time > nextShowRam ) {
    Serial.printf( "At %lu Free Memory: %i Heap Fragmentation: %i Max Free Block Size: %i Stations connected: %i Wifi Stats: %i\n",
      cur_time, ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize(), WiFi.softAPgetStationNum(), WiFi.status() );
    nextShowRam += 2000;
  }

  if( cur_time > next_show ) {
    leds[cur_pixel] = 0;
    cur_pixel = random( 0, NUM_LEDS );
    leds[cur_pixel] = CHSV( random( 0, 255 ), 255, 100 );
    FastLED.show();
    next_show = cur_time + updateSpeed;
  }

}
