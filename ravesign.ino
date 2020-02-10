#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_NeoMatrix.h>

#define PIN 3
#define MATRIX_WIDTH 64
#define MATRIX_HEIGHT 8

#define CHAR_WIDTH 6
#define MATRIX_TEXT_WIDTH ((MATRIX_WIDTH / CHAR_WIDTH) + 1)

#define MAX_MESSAGE_LENGTH 79
#define MAX_WEB_LENGTH 8192

#define MAX_BRIGHT 0.2

#define MIN_SHOW_DELAY 50

#define MIN_COMET_LENGTH 4
#define MAX_COMET_LENGTH 8
#define MIN_COMET_DELAY 50
#define MAX_COMET_DELAY 100
#define MAX_NUM_COMETS 10

#define MAX_NUM_LIGHTS 50
#define MIN_UPDATE_DELAY 50
#define MAX_UPDATE_DELAY 100
#define FLASH_UPDATE_DELAY 100

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
  unsigned long next_update;
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

boolean enabled = true;
boolean show_needed = false;
boolean changed_mode = false;
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

int numComets = MAX_NUM_COMETS;

unsigned long nextCometUpdate;

Comet comet[MAX_NUM_COMETS];

boolean row_used[MATRIX_HEIGHT];
boolean col_used[MATRIX_WIDTH];
boolean light_used[MATRIX_WIDTH][MATRIX_HEIGHT];

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
  int length = random( MIN_COMET_LENGTH, MAX_COMET_LENGTH -1 );
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
  
  for( int cnum = 0; cnum < numComets; cnum++ ) {
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
    if( comet[cnum].body[i].x > -1 && comet[cnum].body[i].x < MATRIX_WIDTH && comet[cnum].body[i].y > -1 && comet[cnum].body[i].y < MATRIX_HEIGHT
      && !light_used[comet[cnum].body[i].x][comet[cnum].body[i].y] ) {
      matrix.drawPixel( comet[cnum].body[i].x, comet[cnum].body[i].y, comet[cnum].body[i].c );
    }
  }

  show_needed = true;
}

void clear_comet( int cnum ) {
  
  for( int i = 0; i < comet[cnum].length; i++ ) {
    if( comet[cnum].body[i].x > -1 && comet[cnum].body[i].x < MATRIX_WIDTH && comet[cnum].body[i].y > -1 && comet[cnum].body[i].y < MATRIX_HEIGHT ) {
      matrix.drawPixel( comet[cnum].body[i].x, comet[cnum].body[i].y, 0 );
    }
  }

  if( comet[cnum].xdir ) {
    row_used[comet[cnum].body[0].y] = false;
  } else {
    col_used[comet[cnum].body[0].x] = false;
  }

  show_needed = true;
}

void animate_comets() {

  for( int cnum = 0; cnum < numComets; cnum++ ) {

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

int numLights = MAX_NUM_LIGHTS;

Light lights[MAX_NUM_LIGHTS];

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

  for( int i = 0; i < numLights; i++ ) {
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

  else if( new_val <= 0 ) {
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
  
  for( int i = 0; i < numLights; i++ ) {
    
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

String textMessage = "Fun! Fun! Fun!";
String displayText;

int textLength;
int textPos;
int charPos;
int textDir = 1;
int textSpeed = 250;
unsigned long nextTextUpdate;
int textColorMode;
int textColor;
int textStartColor;

void init_scrollingText() {

  displayText = textMessage + " ";
  
  textLength = displayText.length() - 1;
  textPos = 0;
  charPos = 0;
  nextTextUpdate = cur_time + textSpeed;
  textColor = 0;
  textStartColor = 0;

  if( textDir != 1 && textDir != -1 ) {
    textDir = 1;
  }

  if( textSpeed < MIN_SHOW_DELAY || textSpeed > 1000 ) {
    textSpeed = 100;
  }
}

void animate_scrollingText() {
  int curChar, ch;
  HsvColor color;

  charPos += textDir;

  if( textDir == 1 ) {
    if( charPos > 0 ) {
      textPos--;
      charPos = -CHAR_WIDTH + 1;
    }

    if( textPos < 0 ) {
      textPos = textLength;
    }
  }

  else {
    if( charPos < 0 ) {
      textPos++;
      charPos = CHAR_WIDTH - 1;
    }

    if( textPos > textLength ) {
      textPos = 0;
    }
  }

  if( textColorMode == 0 ) {
    textColor = textStartColor;

    textStartColor += 10;

    if( textStartColor > 360 ) {
      textStartColor = 0;
    }
  }

  curChar = textPos;
  
  matrix.fillScreen( 0 );

  for( ch = -1; ch < MATRIX_TEXT_WIDTH + 1; ch++ ) {

    switch( textColorMode ) {
      case 2:
        if( displayText.charAt( curChar ) == 32 ) {
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
    
    matrix.setCursor( charPos + (ch * CHAR_WIDTH) , 0 );
    matrix.print( displayText.charAt( curChar ));

    curChar++;
    
    if( curChar > textLength ) {
      curChar = 0;
    }
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

const char* WifiSSID = "ESPap";
const char* WifiPSK = "thereisnospoon";

IPAddress local_IP(192,168,4,2);
IPAddress gateway(192,168,4,1);
IPAddress subnet(255,255,255,0);

ESP8266WebServer server(80);

const String webhead = "\
<html><head> \
    <title>Rave Sign</title> \
    <style> \
      body { font-size: 30px; } \
      input { font-size: 25px; } \
      input[type=button], input[type=submit], input[type=reset] { \
        border: solid; \
        color: white; \
        padding: 10px 20px; \
        margin: 5px; \
      } \
      input[type=radio] { \
        padding: 10px; \
        margin: 5px; \
        width: 30px; \
        height: 30px; \
      } \
      .controls { \
        width: 98%; \
        border: 3px solid blue; \
        margin: 5px; \
      } \
      .top { \
        width: 100%; \
        color: white; \
        background: blue; \
        display: inline-block; \
        padding: 5px 0px; \
      } \
      .cntrl { \
        width: 95%; \
        padding: 5px; \
        display: inline-block; \
      } \
      .left { \
        width: 50%; \
        padding: 2px 20px; \
        float: left; \
      } \
      .right { \
        width: auto; \
        padding: 2px 10px; \
        float: right; \
      } \
      .gobutton { \
        background-color: #4CAF50; \
      } \
      .stopbutton { \
        background-color: red; \
      } \
    </style></head><body>";

String webContent;

const String webContent_stopbutton = "stopbutton";
const String webContent_gobutton = "gobutton";
const String webContent_Stop = "Stop";
const String webContent_Start = "Start";
const String webContent_MatrixSize = String( MATRIX_WIDTH ) + " X " + String( MATRIX_HEIGHT );
const String webContent_checked = " checked";

void printTop() {
  webContent +=
    "<form action='/submit' method='POST'> \
     <div class='controls'> \
     <div class='top'> \
     <div class='left'><b>Rave Sign</b></div> \
     <div class='right'><input class='" + ( enabled ? webContent_stopbutton : webContent_gobutton ) +
      "' type='submit' name='enabled' value='" + ( enabled ? webContent_Stop : webContent_Start ) + "'></div></div>";
}

void printSection( String title, int progNum ) {
  webContent +=
    "<form action='/submit' method='POST'> \
     <input type='hidden' name='programMode' value='" + String( progNum ) +"'> \
     <div class='controls'> \
     <div class='top'> \
     <div class='left'><b>" + title + "<b></div> \
     <div class='right'><input class='gobutton' type='submit' name='submit' value='Go!'></div></div>";
}

void printControl( String title, String value ) { 
  webContent +=
    "<div class='cntrl'> \
     <div class='left'><b>" + title + "</b></div> \
     <div class='right'>" + value + "</div></div>";
}

void handleRoot() {
  Serial.println( "*** Running handleRoot" );

  webContent = "";
  webContent += webhead;

  printTop();
  printControl( "Uptime", String( cur_time ));
  printControl( "Free Heap", String( ESP.getFreeHeap() ));
  printControl( "Matrix Size", webContent_MatrixSize );
  webContent += "</div></form>";

  printSection( "Stary Night", 1 );
  printControl( "Number of Lights:", "<input type='text' name='numLights' value='" + String( numLights ) + "' size='5'>" );
  printControl( "Number of Comets:", "<input type='text' name='numComets' value='" + String( numComets ) + "' size='5'>" );
  webContent += "</div></form>";

  printSection( "Scrolling Text", 2 );
  printControl( "Message:", "<input type='text' name='textMessage' value='" + textMessage + "' size='20'>" );
  printControl( "Speed:", "<input type='text' name='textSpeed' value='" + String( textSpeed ) + "' size='5'>" );
  printControl( "Direction:", 
      "<input type='radio' name='textDir' value='-1'" + ( textDir == -1 ? webContent_checked : "" ) + "> Clockwise<br /> \
       <input type='radio' name='textDir' value='1'" + ( textDir == 1 ? webContent_checked : "" ) + "> Counterclockwise" );
  printControl( "Color Mode:", 
      "<input type='radio' name='textColorMode' value='0'" + ( textColorMode == 0 ? webContent_checked : "" ) + "> Rainbow<br /> \
       <input type='radio' name='textColorMode' value='1'" + ( textColorMode == 1 ? webContent_checked : "" ) + "> Random<br /> \
       <input type='radio' name='textColorMode' value='2'" + ( textColorMode == 2 ? webContent_checked : "" ) + "> Random Word" );
  webContent += "</div></form></body></html>";
    
  server.send( 200, "text/html", webContent );
  Serial.printf( "   Output is %i bytes.\n", webContent.length() );
} 

/* Handle submit */
void handleSubmit() {
  Serial.println( "*** Running: handleSumbit" );

  changed_mode = true;

  for( uint8_t i = 0; i < server.args(); i++ ) {
    Serial.print( "   Arg: " );
    Serial.print( server.argName(i) );
    Serial.print( " : " );
    Serial.println( server.arg(i) );
  }
  
  webContent = " \
    <html><head><meta http-equiv='refresh' content='5;url=http://" + local_IP.toString() + "/'></head><body> \
    <h1>Request received</h1> \
    <table style='border: 2px solid black;'><tr><th>Setting</th><th>Value</th></tr>";

  for( uint8_t i = 0; i < server.args(); i++ ) {
    webContent += "<tr><td>" + server.argName(i) + "</td><td>" + server.arg(i) + "</td></tr>";
  }
    
  webContent += "</table></body></html>";
  server.send( 200, "text/html", webContent );
  Serial.printf( "   Output is %i bytes.\n", webContent.length() );
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

/* --------------------------------------------------------------------------------
 *  SETUP & LOOP
 * --------------------------------------------------------------------------------
 */

unsigned long nextShowRam;
unsigned long nextHandleClient;

void setup() {
  Serial.begin( 115200 );
  delay( 1000 );

  randomSeed( analogRead( 0 ));

  Serial.print( "Setting soft-AP configuration ... " );
  WiFi.scanNetworks( false );
  WiFi.mode( WIFI_AP );
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
  matrix.setTextWrap( false );
  matrix.fillScreen( 0 );
  matrix.show();

  cur_time = millis();
  next_show = cur_time + MIN_SHOW_DELAY;
  nextShowRam = cur_time;
  nextHandleClient = cur_time;

  textMessage.reserve( MAX_MESSAGE_LENGTH );
  displayText.reserve( MAX_MESSAGE_LENGTH );
  webContent.reserve( MAX_WEB_LENGTH );

  init_scrollingText();
  gen_all_comets();
  gen_all_lights();
}

void loop() {

  cur_time = millis();

  if( show_needed && cur_time > next_show ) {
    matrix.show();
    show_needed = false;
    next_show = cur_time + MIN_SHOW_DELAY;
    return;
  } else if( cur_time > nextHandleClient ) {
    server.handleClient();
    nextHandleClient = cur_time + 100;
    return;
  }

  if( changed_mode ) {
    
    if( server.hasArg( "enabled" )) {
      enabled = !enabled;
    
    } else if( server.hasArg( "programMode" ) ) {
    
      programMode = server.arg( "programMode" ).toInt();

      if( programMode == 1 ) {
        numLights = server.arg( "numLights" ).toInt();
        numComets = server.arg( "numComets" ).toInt();

        if( numLights > MAX_NUM_LIGHTS ) {
          numLights = MAX_NUM_LIGHTS;
        }

        if( numComets > MAX_NUM_COMETS ) {
          numComets = MAX_NUM_COMETS;
        }
      
        gen_all_comets();
        gen_all_lights();
      }

      if( programMode == 2 ) {
        textMessage = server.arg( "textMessage" ).substring( 0, MAX_MESSAGE_LENGTH );
        textSpeed = server.arg( "textSpeed" ).toInt();
        textDir = server.arg( "textDir" ).toInt();
        textColorMode = server.arg( "textColorMode" ).toInt();
        init_scrollingText();
      }
    }

    matrix.fillScreen( 0 );

    show_needed = true;
    changed_mode = false;
  }

  if( cur_time > nextShowRam ) {
    Serial.printf( "At %lu Free Memory: %i Heap Fragmentation: %i Max Free Block Size: %i Stations connected: %i Wifi Stats: %i\n",
      cur_time, ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize(), WiFi.softAPgetStationNum(), WiFi.status() );
    nextShowRam += 2000;
  }

  if( enabled && programMode == 1 ) {
    if( cur_time > nextCometUpdate ) {
      nextCometUpdate = cur_time + MIN_SHOW_DELAY;
      animate_comets();
      animate_lights();
    }

  } else if( enabled && programMode == 2 ) {
    if( cur_time > nextTextUpdate ) {
      nextTextUpdate = cur_time + textSpeed;
      animate_scrollingText();
    }
  }
}
