#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <FastLED.h>
#include <FontP16x16.h>

#define DATA_PIN 14
#define MATRIX_WIDTH 112
#define MATRIX_HEIGHT 16
#define MATRIX_SIZE (MATRIX_WIDTH * MATRIX_HEIGHT)

#define CANVAS_WIDTH (MATRIX_WIDTH * 2)
#define CANVAS_HEIGHT MATRIX_HEIGHT
#define CANVAS_SIZE (CANVAS_WIDTH * CANVAS_HEIGHT)

#define MAX_WEB_LENGTH 4096
#define MAX_MESSAGE_LENGTH 128

#define MAX_BRIGHT 100
#define DEFAULT_HSV_VAL 100
#define DEFAULT_HSV_SAT 225

#define MIN_SHOW_DELAY 25

#define MIN_COMET_LENGTH 4
#define MAX_COMET_LENGTH 8
#define MIN_COMET_DELAY 25
#define MAX_COMET_DELAY 50
#define MAX_NUM_COMETS 10

#define DEFAULT_NUM_GLOW_STARS 20
#define DEFAULT_NUM_FLASH_STARS 10
#define MAX_NUM_STARS 50
#define FLASH_UPDATE_DELAY 50
#define GLOW_UPDATE_DELAY 25

typedef struct Pixel {
  int x;
  int y;
  CRGB c;
} Pixel;

typedef struct Comet {
  Pixel body[MAX_COMET_LENGTH];
  unsigned long next_update;
  int xdir;
  int ydir;
  int length;
  int delay;
} Comet;

typedef struct Star {
  Pixel p;
  unsigned long next_update;
  byte hue;
  int value;
  int inc;
//  int delay;
} Star;

/* --------------------------------------------------------------------------------
 *  UTILS
 * --------------------------------------------------------------------------------
 */

// Define the array of leds
CRGB Matrix[MATRIX_SIZE];
CRGB Canvas[CANVAS_WIDTH][CANVAS_HEIGHT];

boolean enabled = true;
boolean show_needed = false;
boolean changed_mode = false;
unsigned long cur_time = 0;
uint8_t programMode = 1;

uint16_t canvasWidth = 0;
uint8_t canvasRainbowHue = 0;

uint8_t defaultHSVSat = DEFAULT_HSV_SAT;
uint8_t defaultHSVVal = DEFAULT_HSV_VAL;

boolean matrixPlot( Pixel p )
{

  if( p.x > MATRIX_WIDTH || p.y > MATRIX_HEIGHT ) {
    return false;
  }
  
  if( p.x & 0x01 ) {
    // Odd columns run backwards
    Matrix[(p.x * MATRIX_HEIGHT) + (MATRIX_HEIGHT - 1) - p.y] = p.c;
  } else {
    // Even columns run forwards
    Matrix[(p.x * MATRIX_HEIGHT) + p.y] = p.c;
  }

  show_needed = true;
  
  return true;
}

boolean matrixPlotXY( uint16_t x, uint16_t y, CRGB c )
{

  if( x > MATRIX_WIDTH || y > MATRIX_HEIGHT ) {
    Serial.printf( "Matrix Plot: Out of range! (%i, %i)\n", x, y );
    return false;
  }

  // Serial.printf( "(%i, %i)", x, y );
  
  if( x & 0x01 ) {
    // Odd columns run backwards
    Matrix[(x * MATRIX_HEIGHT) + (MATRIX_HEIGHT - 1) - y] = c;
  } else {
    // Even columns run forwards
    Matrix[(x * MATRIX_HEIGHT) + y] = c;
  }

  show_needed = true;

  return true;
}

void matrixClear()
{
  for( int i = 0; i < MATRIX_SIZE; i++ ) {
    Matrix[i] = 0;
  }

  show_needed = true;
}

boolean canvasPlotXY( uint16_t x, uint16_t y, CRGB c )
{
  if( x > CANVAS_WIDTH || y > CANVAS_HEIGHT ) {
    return false;
  }

  Canvas[x][y] = c;

  if( x > canvasWidth ) {
    canvasWidth = x;
  }

  return true;
}

void canvasClear() {
  for( uint16_t x = 0; x < CANVAS_WIDTH; x++ ) {
    for( uint16_t y = 0; y < CANVAS_HEIGHT; y++ ) {
      Canvas[x][y] = 0;
    }
  }

  canvasWidth = 0;
}

boolean canvasShow( uint16_t matrixStart, uint16_t canvasStart )
{
  
/*  if( displayStart + length > MATRIX_WIDTH ) {
    return false;
  } */
  
  /* if( canvasStart + length > canvasWidth ) {
    return false;
  } */
  
/*  if( length == 0 ) {
    length = MATRIX_WIDTH - displayStart;
  } */

  /* length *= MATRIX_HEIGHT;
  displayStart *= MATRIX_HEIGHT;
  canvasStart *= CANVAS_HEIGHT; */

  // Serial.printf( "canvasShow: %i %i %i\n", displayStart, canvasStart, length );

  matrixClear();

  uint16_t curMatrix = matrixStart;
  uint16_t curCanvas = canvasStart;
  int16_t invisible = canvasWidth - MATRIX_WIDTH;

  if( invisible < 0 ) {
    invisible = 0;
  }

/*  for( uint16_t x = 0; x < canvasWidth; x++ ) {
    // Serial.printf( "   set: %i %i\n", x + displayStart, x + canvasStart );
    // Matrix[x + displayStart] = Canvas[x + canvasStart];

    // Wrap around
    if( cur > MATRIX_WIDTH - 1 ) {
      cur = 0;
    }
    
    for( uint16_t y = 0; y < MATRIX_HEIGHT; y++ ) {      
      matrixPlotXY( cur, y, Canvas[x][y] );
    }

    cur++;
  } */

  for( uint16_t x = 0; x < canvasWidth; x++ ) {
    // Serial.printf( "canvasShow: %i %i %i\n", x, cur, canvasWidth );
    // Matrix[x + displayStart] = Canvas[x + canvasStart];

    for( uint16_t y = 0; y < MATRIX_HEIGHT; y++ ) {      
      matrixPlotXY( curMatrix, y, Canvas[curCanvas][y] );
    }

    curMatrix++;
    curCanvas++;

    // Wrap around
    if( curCanvas >= canvasWidth ) {
      curCanvas = 0;
    }

    if( curMatrix >= MATRIX_WIDTH ) {
      x += invisible;
      curMatrix = 0;
    }
  }
  
  return true;
}

/* void canvasRainbow()
{
  uint8_t curHue = canvasRainbowHue;

  for( uint16_t x = 0; x < canvasWidth; x++ ) {
    for( uint16_t y = 0; y < MATRIX_HEIGHT; y++ ) {
      if( Canvas[x][y] ) {
        Canvas[x][y] = CHSV( curHue, DEFAULT_HSV_SAT, DEFAULT_HSV_VAL );
        curHue++;
      }
    }
  }

  canvasRainbowHue++;
} */

void canvasRainbow()
{
  for( uint16_t x = 0; x < canvasWidth; x++ ) {
    for( uint16_t y = 0; y < MATRIX_HEIGHT; y++ ) {
      if( Canvas[x][y] ) {
        Canvas[x][y] = CHSV( canvasRainbowHue, defaultHSVSat, defaultHSVVal );
        canvasRainbowHue++;
      }
    }
  }
}

uint8_t printChar( char ch, const uint8_t *FontData, CHSV color, uint8_t xstart )
{ 
  uint16_t pos = ((ch - FontData[2]) * 33) + 4;
  uint8_t charWidth = FontData[pos];
  pos++;

  // Serial.printf( "%c : %i : %i\n", ch, pos, charWidth );

  for( uint8_t x = 0; x < charWidth; x++ ) {
    for( uint8_t y = 0; y < FontData[1]; y++ ) {
      // Serial.printf( "%i x %i = %x\n", x, y, FontData[pos + y] );

      uint16_t charData = ((uint16_t)FontData[pos + (y * 2)] << 8 ) + (uint16_t)FontData[pos + (y * 2) + 1];
      
      if( charData & (32768 >> x) ) {
        canvasPlotXY( x + xstart, y, color );
      }
    }
  }

  return charWidth;
}

uint16_t printStr( String str, const uint8_t *FontData, CHSV color )
{
  uint8_t xstart = 0;
  
  canvasClear();
  
  for( uint8_t chpos = 0; chpos < str.length(); chpos++ ) {
    xstart += printChar( str[chpos], FontData, color, xstart ) + 1;

    if( xstart > CANVAS_WIDTH - 12 ) {
      break;
    }
  }

  canvasWidth += 11;

  return xstart;
}

/* --------------------------------------------------------------------------------
 *  COMETS
 * --------------------------------------------------------------------------------
 */

int numComets = MAX_NUM_COMETS;

Comet comet[MAX_NUM_COMETS];

boolean row_used[MATRIX_HEIGHT];
boolean col_used[MATRIX_WIDTH];
boolean light_used[MATRIX_WIDTH][MATRIX_HEIGHT];

/* void init_comet( int cnum, unsigned char hue, int xstart, int ystart, int xdir, int ydir, int length, int delay ) {

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
      comet[cnum].body[i].c = CHSV( hue, 255, (MAX_BRIGHT * (float)((float)i / (float)length )));
    }
  }

  comet[cnum].xdir = xdir;
  comet[cnum].ydir = ydir;
  comet[cnum].length = length;
  comet[cnum].delay = delay;
  comet[cnum].next_update = cur_time + delay;
} */

void gen_comet( int cnum ) {
  byte hue = random( 255 );
  int length = random( MIN_COMET_LENGTH, MAX_COMET_LENGTH -1 );
  int delay = random( MIN_COMET_DELAY, MAX_COMET_DELAY );
  int type, xstart, ystart;
  // boolean good = false;

  // while( !good ) {
  while( true ) {
    type = random( 4 );

    if( type > 1 ) {
      xstart = random( MATRIX_WIDTH / 2 );
      ystart = random( MATRIX_HEIGHT );

      if( row_used[ystart] ) {
        continue;
      }

      row_used[ystart] = true;
      break;
      
    } else {
      xstart = random( MATRIX_WIDTH );
      ystart = random( MATRIX_HEIGHT / 2 );

      if( col_used[xstart] ) {
        continue;
      }

      col_used[xstart] = true;
      break;
    }

    // good = true;
  }

/*  if( type == 0 ) {
    xdir = 0; ydir = -1;
    // init_comet( cnum, hue, xstart, ystart, 0, -1, length, delay );
  } else if( type == 1 ) {
    xdir = 0; ydir = 1;
    // init_comet( cnum, hue, xstart, ystart, 0, 1, length, delay );
  } else if( type == 2 ) {
    xdir = -1; ydir = 0;
    // init_comet( cnum, hue, xstart, ystart, -1, 0, length, delay );
  } else if( type == 3 ) {
    xdir = 1; ydir = 0;
    // init_comet( cnum, hue, xstart, ystart, 1, 0, length, delay );
  } */

  for( int i = 0; i < length; i++ ) {

    if( type == 3 ) {
      comet[cnum].body[i].x = xstart + i - length + 1;
      comet[cnum].body[i].y = ystart;
    }

    else if( type == 2 ) {
      comet[cnum].body[i].x = MATRIX_WIDTH - xstart + length - i - 1;
      comet[cnum].body[i].y = ystart;
    }

    else if( type == 1 ) {
      comet[cnum].body[i].x = xstart;
      comet[cnum].body[i].y = ystart + i - length + 1;
    }

    else if( type == 0 ) {
      comet[cnum].body[i].x = xstart;
      comet[cnum].body[i].y = MATRIX_HEIGHT - ystart + length - i - 1;
    }

    if( i == 0 ) {
      comet[cnum].body[i].c = 0;
    } else {
      comet[cnum].body[i].c = CHSV( hue, defaultHSVSat, (defaultHSVVal * (float)((float)i / (float)length )));
    }
  }

  if( type == 0 ) {
    comet[cnum].xdir = 0;
    comet[cnum].ydir = -1;
  } else if( type == 1 ) {
    comet[cnum].xdir = 0;
    comet[cnum].ydir = 1;
  } else if( type == 2 ) {
    comet[cnum].xdir = -1;
    comet[cnum].ydir = 0;
  } else if( type == 3 ) {
    comet[cnum].xdir = 1;
    comet[cnum].ydir = 0;
  }

  comet[cnum].length = length;
  comet[cnum].delay = delay;
  comet[cnum].next_update = cur_time + delay;
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
      matrixPlot( comet[cnum].body[i] );
    }
  }
}

void clear_comet( int cnum ) {
  
  for( int i = 0; i < comet[cnum].length; i++ ) {
    if( comet[cnum].body[i].x > -1 && comet[cnum].body[i].x < MATRIX_WIDTH && comet[cnum].body[i].y > -1 && comet[cnum].body[i].y < MATRIX_HEIGHT ) {
      comet[cnum].body[i].c = 0;
      matrixPlot( comet[cnum].body[i] );
    }
  }

  if( comet[cnum].xdir ) {
    row_used[comet[cnum].body[0].y] = false;
  } else {
    col_used[comet[cnum].body[0].x] = false;
  }
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
 *  STARS
 * --------------------------------------------------------------------------------
 */

int num_glow_stars = DEFAULT_NUM_GLOW_STARS;
int num_flash_stars = DEFAULT_NUM_FLASH_STARS;

Star glow_stars[MAX_NUM_STARS];
Star flash_stars[MAX_NUM_STARS];

void init_star( Star *star, boolean flash ) {
  
  do {
    star->p.x = random( MATRIX_WIDTH );
    star->p.y = random( MATRIX_HEIGHT );
  } while( light_used[star->p.x][star->p.y] );

  light_used[star->p.x][star->p.y] = true;

  star->hue = random( 255 );
  star->inc = random( 20 );
  star->value = 0;

  if( flash ) {
    star->next_update = cur_time + FLASH_UPDATE_DELAY;
  } else {
    star->next_update = cur_time + GLOW_UPDATE_DELAY;
  }

}

void gen_all_stars() {
  for( int x = 0; x < MATRIX_WIDTH; x++ ) {
    for( int y = 0; y < MATRIX_HEIGHT; y++ ) {
      light_used[x][y] = false;
    }
  }

  for( int i = 0; i < num_glow_stars; i++ ) {
    init_star( &glow_stars[i], false );
  }

  for( int i = 0; i < num_flash_stars; i++ ) {
    init_star( &flash_stars[i], true );
  }
}

/* bool fade_star( Star *star ) {
  bool new_star = false;

  star->value += star->inc;

  if( star->value >= MAX_BRIGHT ) {
     star->value = MAX_BRIGHT;
     star->inc = -star->inc;
  }

  else if( star->value <= 0 ) {
    star->value = 0;
    new_star = true;
  }

  star->p.c = CHSV( star->hue, 255, star->value );

  return new_star;
}

bool flash_star( Star *star ) {
  bool new_star = false;
  
  if( star->inc > 0 ) {
    star->p.c = CHSV( star->hue, 255, MAX_BRIGHT );
    star->inc = -1;
  } else {
    star->p.c = CHSV( star->hue, 255, 0 );
    new_star = true;
  }

  return new_star;
} */

void animate_stars() {
  boolean new_star = false;
  
  for( int i = 0; i < num_glow_stars; i++ ) {
    
    if( glow_stars[i].next_update <= cur_time ) {

      new_star = false;
      // new_star = fade_star( &glow_stars[i] );

      glow_stars[i].value += glow_stars[i].inc;

      if( glow_stars[i].value >= defaultHSVVal ) {
        glow_stars[i].value = defaultHSVVal;
        glow_stars[i].inc = -glow_stars[i].inc;
      }

      else if( glow_stars[i].value <= 0 ) {
        glow_stars[i].value = 0;
        new_star = true;
      }

      glow_stars[i].p.c = CHSV( glow_stars[i].hue, defaultHSVSat, glow_stars[i].value );
      glow_stars[i].next_update = cur_time + GLOW_UPDATE_DELAY;

      matrixPlot( glow_stars[i].p );

      if( new_star ) {
        light_used[glow_stars[i].p.x][glow_stars[i].p.y] = false;
        init_star( &glow_stars[i], false );
      }
    }
  }

  for( int i = 0; i < num_flash_stars; i++ ) {

    if( flash_stars[i].next_update <= cur_time ) {

      new_star = false;
      // new_star = flash_star( &flash_stars[i] );

      if( flash_stars[i].inc > 0 ) {
        flash_stars[i].p.c = CHSV( flash_stars[i].hue, defaultHSVSat, defaultHSVVal );
        flash_stars[i].inc = -1;
      } else {
        flash_stars[i].p.c = 0;
        new_star = true;
      }
      
      flash_stars[i].next_update = cur_time + FLASH_UPDATE_DELAY;

      matrixPlot( flash_stars[i].p );

      if( new_star ) {
        light_used[flash_stars[i].p.x][flash_stars[i].p.y] = false;
        init_star( &flash_stars[i], true );
      }
    }
  }
}

/* --------------------------------------------------------------------------------
 *  SCROLLING TEXT
 * --------------------------------------------------------------------------------
 */

String scrollingTextMessage = "Cookies";
int8_t scrollingTextDir = 1;
int16_t scrollingTextPos = 0;
uint16_t scrollingTextSpeed = 250;
uint8_t scrollingTextColorMode = 0;
unsigned long nextTextUpdate;
CHSV scrollingTextColor;

void init_scrollingText() {

  scrollingTextPos = 0;
  nextTextUpdate = cur_time + scrollingTextSpeed;
  
  printStr( scrollingTextMessage, FontP16x16Data, scrollingTextColor );
  
}

void animate_scrollingText()
{
  if( cur_time > nextTextUpdate ) {
  
    scrollingTextPos += scrollingTextDir;

    if( canvasWidth >= MATRIX_WIDTH ) {
      
      if( scrollingTextPos >= canvasWidth ) {
        scrollingTextPos = 0;
      }

      if( scrollingTextPos < 0 ) {
        scrollingTextPos = canvasWidth - 1;
      }

    } else {

      if( scrollingTextPos >= MATRIX_WIDTH ) {
        scrollingTextPos = 0;
      }

      if( scrollingTextPos < 0 ) {
        scrollingTextPos = MATRIX_WIDTH - 1;
      }
    }

    nextTextUpdate += scrollingTextSpeed;
  }

  canvasRainbow();

  if( canvasWidth >= MATRIX_WIDTH ) {
    canvasShow( 0, scrollingTextPos );
  } else {
    canvasShow( scrollingTextPos, 0 );
  }
}

/* --------------------------------------------------------------------------------
 *  WIFI INTERFACE
 * --------------------------------------------------------------------------------
 */

/* Create web interface. Go to http://192.168.4.2 in a web browser
   connected to this access point to see it.
*/

const char* WifiSSID = "ESPap";

IPAddress local_IP(192,168,4,2);
IPAddress gateway(192,168,4,1);
IPAddress subnet(255,255,255,0);

ESP8266WebServer server(80);

const PROGMEM char webStyle[] = "\
body { font-size: 30px; }\
input { font-size: 25px; }\
input[type=button], input[type=submit], input[type=reset] {\
  border: solid;\
  color: white;\
  padding: 10px 20px;\
  margin: 5px;\
}\
input[type=radio] {\
  padding: 10px;\
  margin: 5px;\
  width: 30px;\
  height: 30px;\
}\
.controls {\
  width: 98%;\
  border: 3px solid blue;\
  margin: 5px;\
}\
.top {\
  width: 100%;\
  color: white;\
  background: blue;\
  display: inline-block;\
  padding: 5px 0px;\
}\
.cntrl {\
  width: 95%;\
  padding: 5px;\
  display: inline-block;\
}\
.left {\
  width: 50%;\
  padding: 2px 20px;\
  float: left;\
}\
.right {\
  width: auto;\
  padding: 2px 10px;\
  float: right;\
}\
.gobutton {\
  background-color: #4CAF50;\
}\
.stopbutton {\
  background-color: red;\
}";

// const String webhead = "\

String webContent;

/* const PROGMEM char webContent_stopbutton[] = "stopbutton";
const PROGMEM char webContent_gobutton[] = "gobutton";
const PROGMEM char webContent_Stop[] = "Stop";
const PROGMEM char webContent_Start[] = "Start";
const PROGMEM char webContent_checked[] = " checked"; */
const String webContent_checked = " checked";

void printTop() {
  webContent +=
    "<form action='/submit' method='POST'>\
       <div class='controls'>\
       <div class='top'>\
       <div class='left'><b>Rave Sign</b></div>\
       <div class='right'><input class='" + ( enabled ? String( "stopbutton" ) : String( "gobutton" )) +
    "' type='submit' name='enabled' value='" + ( enabled ? String( "Stop" ) : String( "Start" )) + "'>\
       <input class='gobutton' type='submit' name='Update' value='Update'></div></div>";
}

void printSection( String title, int progNum ) {
  webContent +=
    "<form action='/submit' method='POST'>\
     <input type='hidden' name='programMode' value='" + String( progNum ) +"'>\
     <div class='controls'>\
     <div class='top'>\
     <div class='left'><b>" + title + "<b></div>\
     <div class='right'><input class='gobutton' type='submit' name='submit' value='Go!'></div></div>";
}

void printControl( String title, String value ) { 
  webContent +=
    "<div class='cntrl'>\
     <div class='left'><b>" + title + "</b></div>\
     <div class='right'>" + value + "</div></div>";
}

void handleRoot() {
  Serial.println( "*** Running handleRoot" );

  webContent = "<html><head><title>Rave Sign</title><link rel='stylesheet' href='style.css'></head><body>";

  printTop();
  printControl( "Uptime", String( cur_time ));
  printControl( "Free Heap", String( ESP.getFreeHeap() ));
  printControl( "Matrix Size", String( MATRIX_WIDTH ) + " X " + String( MATRIX_HEIGHT ) );
  printControl( "HSV Saturation", "<input type='text' name='defaultHSVSat' value='" + String( defaultHSVSat ) + "' size='5'>" );
  printControl( "HSV Brightness", "<input type='text' name='defaultHSVVal' value='" + String( defaultHSVVal ) + "' size='5'>" );
  webContent += "</div></form>";

  printSection( "Stary Night", 1 );
  printControl( "Number of Glowing Stars:", "<input type='text' name='num_glow_stars' value='" + String( num_glow_stars ) + "' size='5'>" );
  printControl( "Number of Flashing Stars:", "<input type='text' name='num_flash_stars' value='" + String( num_flash_stars ) + "' size='5'>" );
  printControl( "Number of Comets:", "<input type='text' name='numComets' value='" + String( numComets ) + "' size='5'>" );
  webContent += "</div></form>";

  printSection( "Scrolling Text", 2 );
  printControl( "Message:", "<input type='text' name='textMessage' value='" + scrollingTextMessage + "' size='20'>" );
  printControl( "Speed:", "<input type='text' name='textSpeed' value='" + String( scrollingTextSpeed ) + "' size='5'>" );
  printControl( "Direction:", 
      "<input type='radio' name='textDir' value='-1'" + ( scrollingTextDir == -1 ? webContent_checked : "" ) + "> Clockwise<br /> \
       <input type='radio' name='textDir' value='1'" + ( scrollingTextDir == 1 ? webContent_checked : "" ) + "> Counterclockwise" );
  printControl( "Color Mode:", 
      "<input type='radio' name='textColorMode' value='0'" + ( scrollingTextColorMode == 0 ? webContent_checked : "" ) + "> Rainbow<br /> \
       <input type='radio' name='textColorMode' value='1'" + ( scrollingTextColorMode == 1 ? webContent_checked : "" ) + "> Random<br /> \
       <input type='radio' name='textColorMode' value='2'" + ( scrollingTextColorMode == 2 ? webContent_checked : "" ) + "> Random Word" );
  webContent += "</div></form>";
       
  webContent += "</body></html>";
    
  server.send( 200, "text/html", webContent );
  Serial.printf( "   Output is %i bytes.\n", webContent.length() );
}

/* Handle style sheet */
void handleStyle() {
  Serial.println( "*** Running handleStyle" );
  server.send( 200, "text/css", webStyle );
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
  delay( 100 );

  randomSeed( analogRead( 0 ));

  Serial.print( "Setting soft-AP configuration ... " );
  WiFi.scanNetworks( false );
  WiFi.mode( WIFI_AP );
  Serial.println( WiFi.softAPConfig( local_IP, gateway, subnet ) ? "Started" : "FAILED!");

  Serial.print( "Starting soft-AP ... " );
  Serial.println( WiFi.softAP( WifiSSID ) ? "Started" : "FAILED!");

  Serial.print( "Soft-AP IP address : " );
  Serial.println( WiFi.softAPIP() );

  // Reserve memory for web output string
  webContent.reserve( MAX_WEB_LENGTH );

  // Reserve memory for scrolling text
  scrollingTextMessage.reserve( MAX_MESSAGE_LENGTH );
  
  // Setup Server
  server.on( "/", handleRoot );
  server.on( "/style.css", handleStyle );
  server.on( "/submit", handleSubmit );
  server.onNotFound( handleNotFound );
  
  Serial.println( "Starting web server ... " );
  server.begin();

  Serial.println( "Starting Matrix ..." );
  FastLED.addLeds<WS2812B, DATA_PIN, RGB>( Matrix, MATRIX_SIZE );
  FastLED.clear( true );
  FastLED.show();

  cur_time = millis();
  nextShowRam = cur_time;
  nextHandleClient = cur_time;

  gen_all_comets();
  gen_all_stars();

  delay( 100 );
}

void loop() {

  cur_time = millis();

/*  if( show_needed && cur_time > next_show ) {
    matrix.show();
    show_needed = false;
    next_show = cur_time + MIN_SHOW_DELAY;
    return;
  } else if( cur_time > nextHandleClient ) {
    server.handleClient();
    nextHandleClient = cur_time + 100;
    return;
  } */

  server.handleClient();

  if( changed_mode ) {
    
    if( server.hasArg( "enabled" )) {
      enabled = !enabled;

    } else if( server.hasArg( "Update" )) {
      defaultHSVSat = server.arg( "defaultHSVSat" ).toInt();
      defaultHSVVal = server.arg( "defaultHSVVal" ).toInt();
    
    } else if( server.hasArg( "programMode" )) {
    
      programMode = server.arg( "programMode" ).toInt();

      if( programMode == 1 ) {
        num_glow_stars = server.arg( "num_glow_stars" ).toInt();
        num_flash_stars = server.arg( "num_flash_stars" ).toInt();
        numComets = server.arg( "numComets" ).toInt();

        if( num_glow_stars > MAX_NUM_STARS ) {
          num_glow_stars = MAX_NUM_STARS;
        }

        if( num_flash_stars > MAX_NUM_STARS ) {
          num_flash_stars = MAX_NUM_STARS;
        }

        if( numComets > MAX_NUM_COMETS ) {
          numComets = MAX_NUM_COMETS;
        }
      
        gen_all_comets();
        gen_all_stars();
      }

      if( programMode == 2 ) {
        scrollingTextMessage = server.arg( "textMessage" ).substring( 0, MAX_MESSAGE_LENGTH - 2 );
        scrollingTextSpeed = server.arg( "textSpeed" ).toInt();
        scrollingTextDir = server.arg( "textDir" ).toInt();
        scrollingTextColorMode = server.arg( "textColorMode" ).toInt();

        scrollingTextColor = CHSV( random( 255 ), defaultHSVSat, defaultHSVVal );
        
        init_scrollingText();
      }
    }

    // clear();

    FastLED.clear( true );

    // show_needed = true;
    changed_mode = false;
  }

  if( show_needed ) {
    FastLED.show();
    show_needed = false;
  }

  if( cur_time > nextShowRam ) {
    Serial.printf( "At %lu Free Memory: %i Heap Fragmentation: %i Max Free Block Size: %i Stations connected: %i Wifi Stats: %i\n",
      cur_time, ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize(), WiFi.softAPgetStationNum(), WiFi.status() );
    nextShowRam += 2000;
  }

  if( enabled && programMode == 1 ) {
    animate_comets();
    animate_stars();
  } else if( enabled && programMode == 2 ) {
    animate_scrollingText();
  }

  delay( MIN_SHOW_DELAY );
}
