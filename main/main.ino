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

#define DEBUG_VALUE(l, v) Serial.print(l); Serial.print(":"); Serial.println(v,DEC);

struct Element {
  float speed;
  float position;
  uint8_t ttl;
  uint16_t hue;
  uint8_t sat;
  uint8_t *pattern;
};

rgb_color colors[LED_COUNT];
const uint8_t snake[] = {10, 80, 72, 64, 56, 48, 40, 32, 24, 16, 8};
const uint8_t killer_snake[] = {10, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80};
uint8_t led_exp[MAX_BRIGHT+1];
Element elements[MAX_ELEMENTS];
uint8_t nbr_elements;

// Makes for a nicer LED display with values that are more exponential.
float init_led_exp(){
  for (int bright = 0; bright <= MAX_BRIGHT; bright++){
    float bright_exp = exp(float(bright)/float(MAX_BRIGHT) * EXP_FACTOR);
    led_exp[bright] = (bright_exp - 1) * MAX_BRIGHT / exp(EXP_FACTOR);
  }
}

// Adds a new element to the list. If the list is full, simply returns.
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

#define TTL_START 20

// Prints one pixel as a mixture of two elements from a pattern.
// Also takes into account an evenutal TTL > 0 field.
void print_diff(Element *e, int v1, int v2, int pos, float diff){
  uint16_t hue = e->hue;
  uint8_t sat = e->sat;
  uint8_t value1 = 0;
  if (v1 >= 0){
    value1 = e->pattern[v1];
  }
  uint8_t value2 = 0;
  if (v2 >= 0){
    value2 = e->pattern[v2];
  }
  if (pos < 0 || pos >= LED_COUNT){
    return;
  }
  int val = led_exp[int(value1 * (1-diff) + value2 * diff)];
  if (e->ttl > 0){
    val = val * e->ttl / TTL_START;
  }
  if (val > 0){
    add_color(pos, hsvToRgb(hue, sat, val));
  }
}

// Prints one element, taking into account the float position
// and drawing it accordingly.
void print_element(Element *e){
  int pos_int = e->position;
  float diff = e->position - pos_int;
  int pattern_len = e->pattern[0];
  uint8_t *pattern = &e->pattern[1];
  if (diff > 0){
    print_diff(e, -1, 0, pos_int, diff);
  }
  for (int j = 1; j < pattern_len-1; j++){
    print_diff(e, j-1, j, pos_int - j, diff);
  }
  print_diff(e, pattern_len-1, 0, pos_int - pattern_len + 1, diff); 
}

// Deletes one element by overwriting it with the next element.
void delete_element(int i){
  for (int j = i; j < MAX_ELEMENTS - 1; j++){
    elements[j] = elements[j+1];
  }
  nbr_elements--;
}

// Acceleration regions on the LED strip.
struct G {
  uint16_t pos;
  float speed_dif;
  float speed_extreme;
};
#define ACCELERATIONS_LEN 7
const G accelerations[ACCELERATIONS_LEN] = {
  {23, +0.007, 0},
  {43, -0.007, 0.1},
  {65, +0.007, 0},
  {84, -0.007, 0.1},
  {91, -0.015, 0.1},
  {142, +0.007, 0},
  {180, -0.01, 0.2},
};

// Accelerate or Deccelerate all elements that go down.
void accelerate(Element *e){
  if (e->speed < 0){
    return;
  }
  for (int i = 0; i < ACCELERATIONS_LEN; i++){
    G acc = accelerations[i];
    if (e->position < acc.pos){
      if (abs(e->speed) > acc.speed_extreme){
        e->speed += acc.speed_dif;
        return;
      }
    }
  }
}

// Updates the position of the element. Returns true if the element is still
// to be printed, false if the element left the stripe.
// If the ttl field is > 0, decrement it, and return false if ttl goes to 0.
bool update_element(Element *e){
  accelerate(e);
  e->position += e->speed;
  if (e->speed > 0 && e->pattern == killer_snake){
    e->pattern = snake;
  }
  if (e->ttl > 0){
    e->ttl--;
    if (e->ttl == 0){
      return false;
    }
  }
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
      add_element(Element{-2, LED_COUNT, 0, 240, 128, killer_snake});
    } else {
      add_element(Element{speed, 0, 0, 0, 0, snake});
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

// Draw one flat part.
void draw_flat(int pos, int len, uint16_t hue, uint8_t sat, uint8_t val){
  for (int i = pos; i < pos + len; i++){
    colors[i] = hsvToRgb(hue, sat, val);
  }
}

// Draw the static flat parts of the strip.
void draw_flats(){
  draw_flat(23, 21, 0, 100, 2);
  draw_flat(65, 18, 0, 100, 2);
  draw_flat(83, 8, 0, 100, 3);
  draw_flat(149, 143, 120, 100, 2);
}

// Remove snakes when they are overrun by the
// blue killer_snake.
void kill_snakes(){
  int killer_pos = -1;
  for (int i = 0; i< nbr_elements; i++){
    if (elements[i].pattern == killer_snake){
      killer_pos = elements[i].position;
      break;
    }
  }
  if (killer_pos == -1){
    return; 
  }
  for (int i = 0; i < nbr_elements; i++){
    Element *e = &elements[i];
    if (e->position > killer_pos && e->ttl == 0){
      e->ttl = TTL_START;
      e->hue = 0;
      e->sat = 220;
    }
  }
}

void loop(){
  random_spawn();
  colors_delete();
  draw_flats();
  update_all_elements();
  kill_snakes();
  colors_copy();

  // 60Hz refresh - well, the calculation is more than 0ms, so it's more like 30Hz refresh rate,
  // depending on the number of elements to print.
  delay(16);
}
