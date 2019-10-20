#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_NeoMatrix.h>

#define PIN 3
#define MATRIX_WIDTH 128
#define MATRIX_HEIGHT 8

#define MAX_BRIGHT 0.2

#define MIN_SHOW_DELAY 5

#define MIN_COMET_LENGTH 4
#define MAX_COMET_LENGTH 8
#define MIN_COMET_DELAY 5
#define MAX_COMET_DELAY 50
#define NUM_COMETS 10

#define NUM_LIGHTS 50
#define MIN_UPDATE_DELAY 10
#define MAX_UPDATE_DELAY 20
#define FLASH_UPDATE_DELAY 100

#define APSSID "ESPap"
#define APPSK  "thereisnospoon"

typedef struct HsvColor {
    double h;       // angle in degrees
    double s;       // a fraction between 0 and 1
    double v;       // a fraction between 0 and 1
} HsvColor;

typedef struct Pixel {
  int x;
  int y;
  uint16_t c;  
} Pixel;

typedef struct Comet {
  int xdir;
  int ydir;
  int length;
  int next_update;
  int delay;
  Pixel body[MAX_COMET_LENGTH];
} Comet;

typedef struct Light {
  uint8_t x;
  uint8_t y;
  HsvColor hsv;
  unsigned long next_update;
  double inc;
  bool flash;
} Light; 

// MATRIX DECLARATION:
// Parameter 1 = width of NeoPixel matrix
// Parameter 2 = height of matrix
// Parameter 3 = pin number (most are valid)
// Parameter 4 = matrix layout flags, add together as needed:
//   NEO_MATRIX_TOP, NEO_MATRIX_BOTTOM, NEO_MATRIX_LEFT, NEO_MATRIX_RIGHT:
//     Position of the FIRST LED in the matrix; pick two, e.g.
//     NEO_MATRIX_TOP + NEO_MATRIX_LEFT for the top-left corner.
//   NEO_MATRIX_ROWS, NEO_MATRIX_COLUMNS: LEDs are arranged in horizontal
//     rows or in vertical columns, respectively; pick one or the other.
//   NEO_MATRIX_PROGRESSIVE, NEO_MATRIX_ZIGZAG: all rows/columns proceed
//     in the same order, or alternate lines reverse direction; pick one.
//   See example below for these values in action.
// Parameter 5 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix( MATRIX_WIDTH, MATRIX_HEIGHT, PIN,
  NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);

/* --------------------------------------------------------------------------------
 *  UTILS
 * --------------------------------------------------------------------------------
 */

boolean show_needed = false;
unsigned long cur_time = 0;
unsigned long next_show = 0;
int programMode = 1;

double randomDouble( double minf, double maxf )
{
  return minf + random(1UL << 31) * (maxf - minf) / (1UL << 31);  // use 1ULL<<63 for max double values)
}

uint16_t HsvToRgb( HsvColor in )
{
    double      hh, p, q, t, ff, r, g, b;
    long        i;
   
    if( in.s <= 0.0 ) {
        r = in.v;
        g = in.v;
        b = in.v;
    } else {
    
      hh = in.h;
      if( hh >= 360.0 ) hh = 0.0;
      hh /= 60.0;
    
      i = (long)hh;
      ff = hh - i;
    
      p = in.v * (1.0 - in.s);
      q = in.v * (1.0 - (in.s * ff));
      t = in.v * (1.0 - (in.s * (1.0 - ff)));

      switch( i ) {
      case 0:
        r = in.v;
        g = t;
        b = p;
        break;
      case 1:
        r = q;
        g = in.v;
        b = p;
        break;
      case 2:
        r = p;
        g = in.v;
        b = t;
        break;
      case 3:
        r = p;
        g = q;
        b = in.v;
        break;
      case 4:
        r = t;
        g = p;
        b = in.v;
        break;
      case 5:
      default:
        r = in.v;
        g = p;
        b = q;
        break;
      }
    }

    return ((uint16_t)((uint8_t)(r * 255) & 0xF8) << 8) |
           ((uint16_t)((uint8_t)(g * 255) & 0xFC) << 3) |
                      ((uint8_t)(b * 255)         >> 3);
}

/* --------------------------------------------------------------------------------
 *  COMETS
 * --------------------------------------------------------------------------------
 */

unsigned long nextCometUpdate;

Comet comet[NUM_COMETS];

boolean row_used[MATRIX_HEIGHT];
boolean col_used[MATRIX_WIDTH];

void init_comet( int cnum, double hue, int xstart, int ystart, int xdir, int ydir, int length, int delay ) {

  HsvColor hsvcolor;

  for( int i = 0; i < length; i++ ) {

    if( xdir == 1 ) {
      comet[cnum].body[i].x = xstart + i - length + 1;
      comet[cnum].body[i].y = ystart;
    }

    if( xdir == -1 ) {
      comet[cnum].body[i].x = xstart + MATRIX_WIDTH + length - i - 1;
      comet[cnum].body[i].y = ystart;
    }

    if( ydir == 1 ) {
      comet[cnum].body[i].x = xstart;
      comet[cnum].body[i].y = ystart + i - length + 1;
    }

    if( ydir == -1 ) {
      comet[cnum].body[i].x = xstart;
      comet[cnum].body[i].y = ystart + MATRIX_HEIGHT + length - i - 1;
    }

    if( i == 0 ) {
      comet[cnum].body[i].c = 0;
      
    } else {

      hsvcolor.h = hue;
      hsvcolor.s = 1.0;
      hsvcolor.v = MAX_BRIGHT - (MAX_BRIGHT * ((double)length - (double)i) / (double)length);
      comet[cnum].body[i].c = HsvToRgb( hsvcolor );
    }
  }

  comet[cnum].xdir = xdir;
  comet[cnum].ydir = ydir;
  comet[cnum].length = length;
  comet[cnum].delay = delay;
  comet[cnum].next_update = cur_time + delay;
}

void gen_comet( int cnum ) {
  double hue = randomDouble( 0.0, 360.0 );
  int length = random( MIN_COMET_LENGTH, MAX_COMET_LENGTH );
  int delay = random( MIN_COMET_DELAY, MAX_COMET_DELAY );
  int type, xstart, ystart;
  boolean good = false;

  while( !good ) {
    type = random( 4 );

    if( type > 1 ) {
      xstart = random( MATRIX_WIDTH / 2 );
      ystart = random( MATRIX_HEIGHT );

      if( row_used[ystart] ) {
        continue;
      }

      row_used[ystart] = true;
      
    } else {
      xstart = random( MATRIX_WIDTH );
      ystart = random( MATRIX_HEIGHT / 2 );

      if( col_used[xstart] ) {
        continue;
      }

      col_used[xstart] = true;
    }

    good = true;
  }

  if( type == 0 ) {
    init_comet( cnum, hue, xstart, ystart, 0, -1, length, delay );
  } else if( type == 1 ) {
    init_comet( cnum, hue, xstart, ystart, 0, 1, length, delay );
  } else if( type == 2 ) {
    init_comet( cnum, hue, xstart, ystart, -1, 0, length, delay );
  } else if( type == 3 ) {
    init_comet( cnum, hue, xstart, ystart, 1, 0, length, delay );
  }
}

void gen_all_comets() {
  for( int x = 0; x < MATRIX_HEIGHT; x++ ) {
    row_used[x] = false;
  }

  for( int x = 0; x < MATRIX_WIDTH; x++ ) {
    col_used[x] = false;
  }
  
  for( int cnum = 0; cnum < NUM_COMETS; cnum++ ) {
    gen_comet( cnum );
  }

  nextCometUpdate = cur_time + MIN_SHOW_DELAY;
}

void move_comet( int cnum ) {
  
  for( int i = 0; i < comet[cnum].length - 1; i++ ) {
    comet[cnum].body[i].x = comet[cnum].body[i+1].x;
    comet[cnum].body[i].y = comet[cnum].body[i+1].y;
  }

  comet[cnum].body[comet[cnum].length-1].x += comet[cnum].xdir;
  comet[cnum].body[comet[cnum].length-1].y += comet[cnum].ydir;
  comet[cnum].next_update = cur_time + comet[cnum].delay;
}

void draw_comet( int cnum ) {
  for( int i = 0; i < comet[cnum].length; i++ ) {
    matrix.drawPixel( comet[cnum].body[i].x, comet[cnum].body[i].y, comet[cnum].body[i].c );
  }

  show_needed = true;
}

void clear_comet( int cnum ) {
  for( int i = 0; i < comet[cnum].length; i++ ) {
    matrix.drawPixel( comet[cnum].body[i].x, comet[cnum].body[i].y, 0 );
  }

  if( comet[cnum].xdir ) {
    row_used[comet[cnum].body[0].y] = false;
  } else {
    col_used[comet[cnum].body[0].x] = false;
  }

  show_needed = true;
}

void animate_comets() {

  for( int cnum = 0; cnum < NUM_COMETS; cnum++ ) {

    if( comet[cnum].next_update < cur_time ) {
  
      if( comet[cnum].body[comet[cnum].length-1].x > -comet[cnum].length &&
          comet[cnum].body[comet[cnum].length-1].x < MATRIX_WIDTH + comet[cnum].length - 2 &&
          comet[cnum].body[comet[cnum].length-1].y > -comet[cnum].length &&
          comet[cnum].body[comet[cnum].length-1].y < MATRIX_HEIGHT + comet[cnum].length - 2 ) {

        draw_comet( cnum );
        move_comet( cnum );
    
      } else {
      
        clear_comet( cnum );
        gen_comet( cnum );
      }
    }
  }   
}

/* --------------------------------------------------------------------------------
 *  LIGHTS
 * --------------------------------------------------------------------------------
 */

bool light_used[MATRIX_WIDTH][MATRIX_HEIGHT];

Light lights[NUM_LIGHTS];

void init_light( uint8_t light_num ) {
  
  do {
    lights[light_num].x = random( MATRIX_WIDTH );
    lights[light_num].y = random( MATRIX_HEIGHT );
  } while( light_used[lights[light_num].x][lights[light_num].y] );

  light_used[lights[light_num].x][lights[light_num].y] = true;
  
  lights[light_num].hsv.h = randomDouble( 0.0, 360.0 );
  lights[light_num].hsv.s = 1.0;
  lights[light_num].hsv.v = 0.0;

  lights[light_num].inc = randomDouble( 0.005, 0.02 );
  lights[light_num].flash = random( 4 ) > 2;

  if( lights[light_num].flash ) {
    lights[light_num].next_update = cur_time + FLASH_UPDATE_DELAY;
  } else {
    lights[light_num].next_update = cur_time + random( MIN_UPDATE_DELAY, MAX_UPDATE_DELAY );
  }

}

void gen_all_lights() {
  for( int x = 0; x < MATRIX_WIDTH; x++ ) {
    for( int y = 0; y < MATRIX_HEIGHT; y++ ) {
      light_used[x][y] = false;
    }
  }

  for( int i = 0; i < NUM_LIGHTS; i++ ) {
    init_light( i );
  }
}

bool fade_light( uint8_t light_num ) {
  double new_val = lights[light_num].hsv.v + lights[light_num].inc;
  bool new_light = false;

  if( new_val >= MAX_BRIGHT ) {
     new_val = MAX_BRIGHT;
     lights[light_num].inc = -lights[light_num].inc;
  }

  if( new_val <= 0 ) {
    new_val = 0;
    new_light = true;
  }

  lights[light_num].hsv.v = new_val;

  return new_light;
}

bool flash_light( uint8_t light_num ) {
  bool new_light = false;
  
  if( lights[light_num].inc > 0 ) {
    lights[light_num].hsv.v = MAX_BRIGHT;
    lights[light_num].inc = -1;
  } else {
    lights[light_num].hsv.v = 0.0;
    new_light = true;
  }

  return new_light;
}

void animate_lights() {
  boolean new_light = false;
  
  for( int i = 0; i < NUM_LIGHTS; i++ ) {
    
    if( lights[i].next_update <= cur_time ) {
      show_needed = true;

      if( lights[i].flash ) {
        new_light = flash_light(i);
        lights[i].next_update = cur_time + FLASH_UPDATE_DELAY;
      } else {
        new_light = fade_light(i);
        lights[i].next_update = cur_time + random( MIN_UPDATE_DELAY, MAX_UPDATE_DELAY );
      }

      matrix.drawPixel( lights[i].x, lights[i].y, HsvToRgb( lights[i].hsv ));

      if( new_light ) {
        light_used[lights[i].x][lights[i].y] = false;
        init_light( i );
      }
    }
  }
}

/* --------------------------------------------------------------------------------
 *  SCROLLING TEXT
 * --------------------------------------------------------------------------------
 */

const uint16_t textColors[] = {
  matrix.Color(50, 0, 0), matrix.Color(0, 50, 0), matrix.Color(50, 50, 0),matrix.Color(0, 0, 50), matrix.Color(50, 0, 50), matrix.Color(0, 50, 50), matrix.Color(50, 50, 50)};

String textMessage = "Fun Fun Fun!";

int textLength;
int textPos;
int textDir = 1;
int textSpeed = 250;
unsigned long nextTextUpdate;
int textColorMode;
int textColor;
int textStartColor;

void init_scrollingText() {
  textLength = textMessage.length() * 6;
  textPos =  MATRIX_WIDTH - textLength - (textLength / 2);
  nextTextUpdate = cur_time + textSpeed;
  textColor = 0;
  textStartColor = 0;

  if( textDir != 1 && textDir != -1 ) {
    textDir = 1;
  }

  if( textSpeed < 10 || textSpeed > 1000 ){
    textSpeed = 100;
  }
}

void animate_scrollingText() {
  int charPos, ch;
  HsvColor color;

  textPos += textDir;
  
  if( textPos > MATRIX_WIDTH - 6 ) {
    textPos = 0;
  }

  if( textPos < 0 ) {
    textPos = MATRIX_WIDTH - 6;
  }
  
  matrix.fillScreen( 0 );

  if( textColorMode == 0 ) {
    textColor = textStartColor;

    textStartColor += 10;

    if( textStartColor > 360 ) {
      textStartColor = 0;
    }
  }

  for( ch = 0; ch < textMessage.length(); ch++ ) {
    charPos = textPos + (ch * 6);

    if( charPos > MATRIX_WIDTH - 3 ) {
      charPos = charPos - MATRIX_WIDTH;
    }

    else if( charPos < 0 ) {
      charPos = MATRIX_WIDTH + charPos - 6;
    }

    switch( textColorMode ) {
      case 2:
        if( textMessage.charAt( ch ) == 32 ) {
          textColor = random( 6 );
        }
        
        matrix.setTextColor( textColors[textColor] );
        break;

      case 1:
        matrix.setTextColor( textColors[random( 6 )] );
        break;

      case 0:
      default:
        textColor += 10;
        
        if( textColor > 360 ) {
          textColor = 0;
        }
        
        color.h = (double)textColor;
        color.s = 1.0;
        color.v = MAX_BRIGHT;
        matrix.setTextColor( HsvToRgb( color ));
        
        break;

    }
    
    matrix.setCursor( charPos , 0 );
    matrix.print( textMessage.charAt( ch ));
  }

  show_needed = true;
}

/* --------------------------------------------------------------------------------
 *  WIFI INTERFACE
 * --------------------------------------------------------------------------------
 */

/* Create web interface. Go to http://192.168.4.2 in a web browser
   connected to this access point to see it.
*/

IPAddress local_IP(192,168,4,2);
IPAddress gateway(192,168,4,1);
IPAddress subnet(255,255,255,0);

ESP8266WebServer server(80);

void handleRoot() {
  Serial.println( "handleRoot" );
  
  String content = "\
    <html><body> \
    <form action='/submit' method='POST'> \
    <div style='border:1px dotted black;'> \
    <h1>Show Stary Night</h1> \
    <p><input type='submit' name='programMode' value='0'> \
    </p></div></form> \
    <form action='/submit' method='POST'> \
    <div style='border:1px dotted black;'> \
    <h1>Display Text</h1> \
    <p><b>Message:</b> <input type='text' name='textMessage' value='" + textMessage + "'> \
    <br /><b>Speed:</b> <input type='text' name='textSpeed' value='" + String( textSpeed ) + "'> \
    <br /><b>Direction:</b> \
    <input type='radio' name='textDir' value='-1'" + (textDir == -1 ? "checked" : "") + "> Clockwise | \
    <input type='radio' name='textDir' value='1'" + (textDir == 1 ? "checked" : "") + "> Counterclockwise \
    <br \><b>Color Mode:</b> \
    <input type='radio' name='textColorMode' value='0'" + (textColorMode == 0 ? "checked" : "") + "> Rainbow | \
    <input type='radio' name='textColorMode' value='1'" + (textColorMode == 1 ? "checked" : "") + "> Random | \
    <input type='radio' name='textColorMode' value='2'" + (textColorMode == 2 ? "checked" : "") + "> Random Word \
    <br \><input type='submit' name='programMode' value='1'> \
    </p></div></form> \
    </body></html>";
  
  server.send( 200, "text/html", content );
} 

/* Handle submit */
void handleSubmit() {
  Serial.println( "handleSumbit" );

  matrix.fillScreen( 0 );

  for( uint8_t i = 0; i < server.args(); i++ ) {
    Serial.print( "Arg: " );
    Serial.print( server.argName(i) );
    Serial.print( " : " );
    Serial.println( server.arg(i) );
  }

  programMode = server.arg( "programMode" ).toInt();

  if( programMode == 0 ) {
    gen_all_comets();
    gen_all_lights();
  }

  if( programMode == 1 ) {
    textMessage = String( server.arg( "textMessage" ));
    textSpeed = server.arg( "textSpeed" ).toInt();
    textDir = server.arg( "textDir" ).toInt();
    textColorMode = server.arg( "textColorMode" ).toInt();
    init_scrollingText();
  }
  
  String content = " \
    <html><head><meta http-equiv='refresh' content='5;url=http://" + local_IP.toString() + "/'></head><body> \
    <p>Mode: " + String( programMode ) + "</p> \
    <p>Message: " + textMessage + "</p> \
    <p>Speed: " + String( textSpeed ) + "</p> \
    <p>Dir: " + String( textDir ) + "</p> \
    <p>ColorMode: " + String( textColorMode ) + "</p> \
    </body></html>";

  server.send( 200, "text/html", content );
}

// 404 Error Handler
void handleNotFound() {
  Serial.println( "handleNotFound" );
  
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
}

/* --------------------------------------------------------------------------------
 *  SETUP & LOOP
 * --------------------------------------------------------------------------------
 */

int nextCheckWifi;

void setup() {
  Serial.begin( 115200 );
  delay( 1000 );

  randomSeed( analogRead( 0 ));

  Serial.print( "Setting soft-AP configuration ... " );
  WiFi.disconnect();
  WiFi.setAutoConnect( true );
  WiFi.setAutoReconnect( true );
  WiFi.scanNetworks( true );
  WiFi.mode( WIFI_AP );
  Serial.println( WiFi.softAPConfig( local_IP, gateway, subnet ) ? "Started" : "FAILED!");

  Serial.print( "Starting soft-AP ... " );
  Serial.println( WiFi.softAP( APSSID, APPSK ) ? "Started" : "FAILED!");

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
  matrix.setTextWrap( false );
  matrix.fillScreen( 0 );

  cur_time = millis();
  next_show = cur_time + MIN_SHOW_DELAY;
  nextCheckWifi = cur_time + 1000;

  init_scrollingText();
  gen_all_comets();
  gen_all_lights();
}



void loop() {
  server.handleClient();

  cur_time = millis();

/*  if( cur_time > nextCheckWifi ) {
    nextCheckWifi = cur_time + 1000;
    Serial.printf("Stations connected = %d\n", WiFi.softAPgetStationNum());  
  } */

  if( programMode == 0 ) {
    if( cur_time > nextCometUpdate ) {
      nextCometUpdate = cur_time + MIN_SHOW_DELAY;
      animate_comets();
      animate_lights();
    }

  } else {
    if( cur_time > nextTextUpdate ) {
      nextTextUpdate = cur_time + textSpeed;
      animate_scrollingText();
    }
  }

  if( show_needed && cur_time > next_show ) {
    matrix.show();
    show_needed = false;
    next_show = cur_time + MIN_SHOW_DELAY;
  }

}
