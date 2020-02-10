#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_NeoMatrix.h>

#define PIN 3
#define MATRIX_WIDTH 96
#define MATRIX_HEIGHT 8

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix( MATRIX_WIDTH, MATRIX_HEIGHT, PIN,
  NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);

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

  Serial.println( "Starting Matrix ..." );  
  matrix.begin();
  matrix.fillScreen( 0 );
  matrix.show();

  cur_time = millis();
  next_show = cur_time + updateSpeed;
  nextShowRam = cur_time;

}

void loop() {
  server.handleClient();
  
  cur_time = millis();

  if( cur_time > nextShowRam ) {
    Serial.printf( "At %lu Free Memory: %i Heap Fragmentation: %i Max Free Block Size: %i Stations connected: %i Wifi Stats: %i\n",
      cur_time, ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize(), WiFi.softAPgetStationNum(), WiFi.status() );
    nextShowRam += 2000;
  }

  if( cur_time > next_show && matrix.canShow() ) {
    matrix.fillScreen( 0 );
    matrix.drawPixel( random( 0, MATRIX_WIDTH ), random( 0, MATRIX_HEIGHT ), matrix.Color( random( 0, 50 ), random( 0, 50 ), random( 0, 50 ) ));
    matrix.show();
    next_show = cur_time + updateSpeed;
  }

}
