/* LedStripRainbow: Example Arduino sketch that shows
 * how to make a moving rainbow pattern on an
 * Addressable RGB LED Strip from Pololu.
 *
 * To use this, you will need to plug an Addressable RGB LED
 * strip from Pololu into pin 12.  After uploading the sketch,
 * you should see a moving rainbow.
 */

#include <PololuLedStrip.h>

// Create an ledStrip object and specify the pin it will use.
PololuLedStrip<13> ledStrip;

// Create a buffer for holding the colors (3 bytes per color).
#define LED_COUNT 292

#define MAX_BRIGHT 80
#define MAX_ELEMENTS 10

#define EXP_FACTOR 5

struct Element {
  float speed;
  float position;
  uint16_t hue;
  uint8_t sat;
  uint8_t *pattern;
};

rgb_color colors[LED_COUNT];
const uint8_t snake[] = {10, 80, 72, 64, 56, 48, 40, 32, 24, 16, 8};
const uint8_t snake_inv[] = {10, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80};
uint8_t led_exp[MAX_BRIGHT+1];
Element elements[MAX_ELEMENTS];
uint8_t nbr_elements;

float init_led_exp(){
  for (int bright = 0; bright <= MAX_BRIGHT; bright++){
    float bright_exp = exp(float(bright)/float(MAX_BRIGHT) * EXP_FACTOR);
    led_exp[bright] = (bright_exp - 1) * MAX_BRIGHT / exp(EXP_FACTOR);
  }
}

void add_element(Element e){
  if (nbr_elements < MAX_ELEMENTS){
    elements[nbr_elements] = e;
    nbr_elements++;
  }
}

void setup(){
  Serial.begin(115200);
  init_led_exp();
}

// Converts a color from HSV to RGB.
// h is hue, as a number between 0 and 360.
// s is the saturation, as a number between 0 and 255.
// v is the value, as a number between 0 and 255.
rgb_color hsvToRgb(uint16_t h, uint8_t s, uint8_t v){
    uint8_t f = (h % 60) * 255 / 60;
    uint8_t p = (255 - s) * (uint16_t)v / 255;
    uint8_t q = (255 - f * (uint16_t)s / 255) * (uint16_t)v / 255;
    uint8_t t = (255 - (255 - f) * (uint16_t)s / 255) * (uint16_t)v / 255;
    uint8_t r = 0, g = 0, b = 0;
    switch((h / 60) % 6){
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
    }
    return rgb_color(r, g, b);
}

void add_color(int pos, rgb_color col){
  colors[pos].red = min(255, int(colors[pos].red + col.red));
  colors[pos].blue = min(255, int(colors[pos].blue + col.blue));
  colors[pos].green = min(255, int(colors[pos].green + col.green));
}

void print_diff(uint16_t hue, uint8_t sat, uint8_t value1, uint8_t value2, int pos, float diff){
  if (pos < 0 || pos >= LED_COUNT){
    return;
  }
  int val = led_exp[int(value1 * (1-diff) + value2 * diff)];
  if (val > 0){
//    colors[pos] = hsvToRgb(hue, sat, val);
    add_color(pos, hsvToRgb(hue, sat, val));
  }
}

void print_element(Element *e){
  int pos_int = e->position;
  float diff = e->position - pos_int;
  int pattern_len = e->pattern[0];
  uint8_t *pattern = &e->pattern[1];
  if (diff > 0){
    print_diff(e->hue, e->sat, 0, pattern[0], pos_int, diff);
  }
  for (int j = 1; j < pattern_len-1; j++){
    print_diff(e->hue, e->sat, pattern[j-1], pattern[j], pos_int - j, diff);
  }
  print_diff(e->hue, e->sat, pattern[pattern_len-1], 0, pos_int - pattern_len + 1, diff); 
}

void delete_element(int i){
  for (int j = i; j < MAX_ELEMENTS - 1; j++){
    elements[j] = elements[j+1];
  }
  nbr_elements--;
  if (i < nbr_elements){
    return print_element(i);
  }
}

// Updates the position of the element. Returns true if the element is still
// to be printed, false if the element left the stripe.
bool update_element(Element *e){
    e->position += e->speed;
    uint8_t pattern_len = e->pattern[0];
    return e->position < LED_COUNT + pattern_len &&
      e->position + pattern_len > 0;
}

// Updates all elements and deletes those that fell out of the strip.
void update_all_elements(){
  for (int i = 0; i < nbr_elements; i++){
    if (update_element(&elements[i])){
      print_element(&elements[i]);
    } else {
      delete_element(i);
      i--;
    }
  }
}

// Randomly spawn a snake - sometimes spawn a snake that goes fast the other way.
void random_spawn(){
  static int spawn;
  
  if (random(10000) + 30 < spawn && 
      nbr_elements < MAX_ELEMENTS){
    spawn = 0;
    float speed = 0.2 + random(10) / 20.;
    if (speed > 0.60){
      add_element(Element{-2, LED_COUNT, 240, 128, snake_inv});
    } else {
      add_element(Element{speed, 0, 0, 0, snake});
    }
  }
  spawn++;  
}

void colors_delete(){
  for(uint16_t i = 0; i < LED_COUNT; i++)  {
    colors[i] = rgb_color(0, 0, 0);
  }
}

void colors_copy(){
  ledStrip.write(colors, LED_COUNT);  
}

void draw_flat(int pos, int len, uint16_t hue, uint8_t sat, uint8_t val){
  for (int i = pos; i < pos + len; i++){
    colors[i] = hsvToRgb(hue, sat, val);
  }
}

void draw_flats(){
  draw_flat(20, 23, 0, 100, 2);
  draw_flat(64, 23, 0, 100, 2);
  draw_flat(150, 142, 120, 100, 2);
}

void loop(){
  random_spawn();
  colors_delete();
  draw_flats();
  update_all_elements();
  colors_copy();

  // 60Hz refresh - well, the calculation is more than 0ms, so it's more like 30Hz refresh rate,
  // depending on the number of elements to print.
  delay(16);
}
