#include <badgevms/wifi.h>
#include <curl/curl.h>

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h> // voor isspace()

#include <badgevms/compositor.h>
#include <badgevms/event.h>
#include <badgevms/framebuffer.h>
#include <badgevms/process.h>

#include <dirent.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define RGB565(r, g, b)           ((((r) & 0x1F) << 11) | (((g) & 0x3F) << 5) | ((b) & 0x1F))
#define RGB888_TO_RGB565(r, g, b) RGB565(((r) * 31 + 127) / 255, ((g) * 63 + 127) / 255, ((b) * 31 + 127) / 255)

#define BACKGROUND_IMAGE           "APPS:[SPACESTATE_NL]BACKGROUND.PNG"
#define PIN_IMAGES_DIR             "APPS:[SPACESTATE_NL.IMAGES]"
#define PIN_GREEN                  "APPS:[SPACESTATE_NL.IMAGES]PIN_GREEN.PNG"
#define PIN_RED                    "APPS:[SPACESTATE_NL.IMAGES]PIN_RED.PNG"


int main(void) {
    return 0;
}