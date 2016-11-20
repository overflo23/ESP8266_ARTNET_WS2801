
// TODO initalize different chipset if we are not on WS2801..
void leds_setup()
{
  leds.begin();  
}


void leds_all(int r, int g, int b)
{
  for (int i = 0 ; i < numLeds ; i++)
    leds.setPixelColor(i, r, g, b);
  leds.show();
  
}


void leds_off()
{
  for (int i = 0 ; i < numLeds ; i++)
    leds.setPixelColor(i, 0,0,0);
  leds.show();
  
}


void leds_one(int lednum, int r, int g, int b)
{

  leds.setPixelColor(lednum, r, g, b);
  leds.show();
  
}



void leds_rainbow(uint8_t wait) {
  int i, j;
   
  for (j=0; j < 256; j++) {     // 3 cycles of all 256 colors in the wheel
    for (i=0; i < leds.numPixels(); i++) {
      leds.setPixelColor(i, Wheel( (i + j) % 255));
    }  
    leds.show();   // write all the pixels out
    delay(wait);
  }
}




// fill the dots one after the other with said color
// good for testing purposes
void colorWipe(uint32_t c, uint8_t wait) {
  int i;
  
  for (i=0; i < leds.numPixels(); i++) {
      leds.setPixelColor(i, c);
      leds.show();
      delay(wait);
  }
}

/* Helper functions */

// Create a 24 bit color value from R,G,B
uint32_t Color(byte r, byte g, byte b)
{
  uint32_t c;
  c = r;
  c <<= 8;
  c |= g;
  c <<= 8;
  c |= b;
  return c;
}




//Input a value 0 to 255 to get a color value.
//The colours are a transition r - g -b - back to r
uint32_t Wheel(byte WheelPos)
{
  if (WheelPos < 85) {
   return Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if (WheelPos < 170) {
   WheelPos -= 85;
   return Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170; 
   return Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}



