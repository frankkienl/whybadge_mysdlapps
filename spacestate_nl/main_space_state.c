#include "curl/curl.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef WHY_BADGE
#include "badgevms/wifi.h"
#include <badgevms/compositor.h>
#include <badgevms/event.h>
#include <badgevms/framebuffer.h>
#include <badgevms/process.h>
#endif

#include "stb_image.h"
#include "font.h"
#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define WINDOW_WIDTH     720
#define WINDOW_HEIGHT    720
#ifdef WHY_BADGE
#define WINDOW_FLAGS     SDL_WINDOW_FULLSCREEN
#endif
#ifndef WHY_BADGE
#define WINDOW_FLAGS     0
#endif


#define APP_NAME "Space State NL"
#define APP_VERSION "3.0.0"
#define APP_ID "spacestate_nl"

#include <dirent.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define RGB565(r, g, b)           ((((r) & 0x1F) << 11) | (((g) & 0x3F) << 5) | ((b) & 0x1F))
#define RGB888_TO_RGB565(r, g, b) RGB565(((r) * 31 + 127) / 255, ((g) * 63 + 127) / 255, ((b) * 31 + 127) / 255)

#define BACKGROUND_IMAGE           "APPS:[SPACESTATE_NL]BACKGROUND.PNG"
#define PIN_GREEN                  "APPS:[SPACESTATE_NL]PIN_GREEN.PNG"
#define PIN_RED                    "APPS:[SPACESTATE_NL]PIN_RED.PNG"

#include "background.h"
#include "pin_green.h"
#include "pin_red.h"

int render_png_to_framebuffer(
    uint16_t *framebuffer, int fb_width, int fb_height, char const *filename, int dest_x, int dest_y
);

int stbi_info(char const *filename, int *x, int *y, int *comp);

int render_png_with_alpha_scaled(
    uint16_t *framebuffer, int fb_width, int fb_height, char const *filename, int dest_x, int dest_y, int scale_factor
);

uint32_t big_timestamp = 0;
uint32_t big_interval = 30*1000;
uint32_t small_timestamp = 0;
uint32_t small_interval = 250;
int current_hacker_space = 0;

// Data van 1 hacker space
typedef struct {
    char *display_name;
    const char *url;
    const int x;
    const int y;
    bool is_open;
    uint32_t last_checked;
} hacker_space_t;

// Enum van Nederlandse hacker spaces
typedef enum {
    ackspace, //0
    awesomespace,
    bitlair,
    hack42,
    hackalot,
    hs_drenthe,
    hs_nijmegen,
    maakplek_groningen,
    nurdspace,
    pixelbar,
    randomdata,
    revspace,
    space_leiden,
    tdvenlo,
    techinc,
    tkkrlab,
    COUNT
} hacker_spaces_e;

#define NUM_HACKER_SPACES 16

typedef struct {
    hacker_space_t hackerspaces[NUM_HACKER_SPACES/*COUNT*/];
} hacker_spaces_t;

static hacker_spaces_t g_space_state = {
    .hackerspaces = {
        [ackspace] = {"ACKspace", "https://ackspace.nl/spaceAPI", 445, 555, false, 0},
        [awesomespace] = {"AwesomeSpace", "https://state.awesomespace.nl", 345, 319, false, 0},
        [bitlair] = {"Bitlair", "https://bitlair.nl/statejson.php", 378, 343, false, 0},
        [hack42] = {"Hack42", "https://hack42.nl/spacestate/json.php", 438, 348, false, 0},
        [hackalot] = {"Hackalot", "https://hackalot.nl/statejson", 390, 443, false, 0},
        [hs_drenthe] = {"Hackerspace Drenthe", "https://mqtt.hackerspace-drenthe.nl/spaceapi", 527, 209, false, 0},
        [hs_nijmegen] = {"Hackerspace Nijmegen", "https://state.hackerspacenijmegen.nl/state.json", 416, 380, false, 0},
        [maakplek_groningen] = {"Maakplek Groningen", "https://maakplek.nl/api/", 530, 100, false, 0},
        [nurdspace] = {"NURDSpace", "https://space.nurdspace.nl/spaceapi/status.json", 385, 385, false, 0},
        [pixelbar] = {"Pixelbar", "https://spaceapi.pixelbar.nl/", 272, 368, false, 0},
        [randomdata] = {"RandomData", "", 326, 353, false, 0},
        [revspace] = {"RevSpace", "https://revspace.nl/status/status.php", 234, 345, false, 0},
        [space_leiden] = {"Space Leiden", "https://portal.spaceleiden.nl/api/public/status.json", 260, 320, false, 0},
        [tdvenlo] = {"TDvenlo", "https://spaceapi.tdvenlo.nl/spaceapi.json", 458, 470, false, 0},
        [techinc] = {"TechInc", "", 290, 270, false, 0},
        [tkkrlab] = {"TkkrLab", "https://spaceapi.tkkrlab.nl", 530, 310, false, 0}
    }
};

#define ACTIVITY_LOADING 0
#define ACTIVITY_MAP 1
#define ACTIVITY_LOADING_NO_IMAGES 2
#define ACTIVITY_LOADING_DOWNLOADING_IMAGES 3

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *framebuffer;
    SDL_Texture *clean_background;
    Uint16 *pixels;
    int activity;
} AppState;

// For cURL response
typedef struct {
    char *memory;
    size_t size;
} MemoryStruct;

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, MemoryStruct *mem) {
    size_t realsize = size * nmemb;
    printf("Callback recieved %u bytes\n", realsize);

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

int my_isspace(char c) {
    return c == ' '  ||
           c == '\t' ||
           c == '\n' ||
           c == '\r' ||
           c == '\v' ||
           c == '\f';
}

void remove_whitespace(char *str) {
    char *src = str; // read
    char *dst = str; // write

    while (*src) {
        if (!my_isspace((unsigned char)*src)) {
            *dst++ = *src; // copy when not whitespace
        }
        src++;
    }
    *dst = '\0'; // end of string
}

bool get_space_state(const char *space_url) {
    CURL *curl;
    CURLcode res;
    MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, space_url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "BadgeVMS-libcurl/1.0");
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 128);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            printf("Received %lu bytes:\n%s\n", (unsigned long) chunk.size, chunk.memory);
            // Remove whitespace
            remove_whitespace(chunk.memory);
            // Check if hacker space is open
            if (strstr(chunk.memory, "\"open\":true") != NULL) {
                return true;
            }
        }

        curl_easy_cleanup(curl);
    }
    free(chunk.memory);
    return false;
}

void ensure_images_exist() {
    printf("Space State NL - Checking background image...\n");
    FILE *f_background = fopen(BACKGROUND_IMAGE, "r"); //read
    if (f_background == NULL) {
        printf("Space State NL - Background image not found\n");
        f_background = fopen(BACKGROUND_IMAGE, "wb"); //write binary
        fwrite(background_png, 1, background_png_len, f_background);
        fclose(f_background);
        printf("Space State NL - Background image created\n");
    }

    printf("Space State NL - Checking pin_green image...\n");
    FILE *f_pin_green = fopen(PIN_GREEN, "r"); //read
    if (f_pin_green == NULL) {
        printf("Space State NL - pin_green image not found\n");
        f_pin_green = fopen(PIN_GREEN, "wb"); //write binary
        fwrite(pin_green_png, 1, pin_green_png_len, f_pin_green);
        fclose(f_pin_green);
        printf("Space State NL - pin_green image created\n");
    }

    printf("Space State NL - Checking pin_red image...\n");
    FILE *f_pin_red = fopen(PIN_RED, "r"); //read
    if (f_pin_red == NULL) {
        printf("Space State NL - pin_red image not found\n");
        f_pin_red = fopen(PIN_RED, "wb"); //write binary
        fwrite(pin_red_png, 1, pin_red_png_len, f_pin_red);
        fclose(f_pin_red);
        printf("Space State NL - pin_red image created\n");
    }
}

SDL_AppResult SDL_AppIterate(void *appstate) {

    AppState *as = (AppState *) appstate;
    uint32_t current_time = time(NULL) * 1000;
    if (current_time - big_timestamp >= big_interval) {
        if (current_time - small_timestamp >= small_interval) {
            // Check Spaces
            printf("Space State NL - Checking %s %s", g_space_state.hackerspaces[current_hacker_space].display_name, "...");
            bool isOpen = get_space_state(g_space_state.hackerspaces[current_hacker_space].url);
            g_space_state.hackerspaces[current_hacker_space].is_open = isOpen;
            if (isOpen) {
                printf("Space State NL - Checking %s %s", g_space_state.hackerspaces[current_hacker_space].display_name, " is OPEN");
                render_png_with_alpha_scaled(as->pixels, WINDOW_WIDTH, WINDOW_HEIGHT,
                                             PIN_GREEN, g_space_state.hackerspaces[current_hacker_space].x,
                                             g_space_state.hackerspaces[current_hacker_space].y, 1);
            } else {
                printf("Space State NL - Checking %s %s", g_space_state.hackerspaces[current_hacker_space].display_name, " is CLOSED");
                render_png_with_alpha_scaled(as->pixels, WINDOW_WIDTH, WINDOW_HEIGHT,
                                             PIN_RED, g_space_state.hackerspaces[current_hacker_space].x,
                                             g_space_state.hackerspaces[current_hacker_space].y, 1);
            }
            current_hacker_space++;
            small_timestamp = current_time;
            if (current_hacker_space >= NUM_HACKER_SPACES) {
                printf("Space State NL - Checked all, waiting for about 30 seconds");
                current_hacker_space = 0;
                big_timestamp = current_time;
            }
        }
    }

    // Render everything
    SDL_RenderClear(as->renderer);
    SDL_UpdateTexture(as->framebuffer, NULL, as->pixels, WINDOW_WIDTH * sizeof(Uint16));
    SDL_RenderTexture(as->renderer, as->framebuffer, NULL, NULL);
    SDL_RenderPresent(as->renderer);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {

    AppState *as = (AppState *) appstate;

    switch (event->type) {
        case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_DOWN: {
            if (event->key.scancode == SDL_SCANCODE_ESCAPE) {
                printf("Space State NL - ESCAPE KEY\n");
                return SDL_APP_SUCCESS; //exit loop
            }
        }
        default: break;
    }

    // Currently not used
    return SDL_APP_CONTINUE;
}


SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    printf("Space State NL app\n");

#ifndef WHY_BADGE
    if (!SDL_SetAppMetadata(APP_NAME, APP_VERSION, APP_ID)) {
        return SDL_APP_FAILURE;
    }
#endif

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

#ifdef WHY_BADGE
    wifi_connect();
#endif
    curl_global_init(0);

    AppState *as = (AppState *) SDL_calloc(1, sizeof(AppState));
    if (!as) {
        return SDL_APP_FAILURE;
    }
    *appstate = as;

    //Create window
    as->window = SDL_CreateWindow(APP_NAME, WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_FLAGS);
    if (!as->window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Check display capabilities
    SDL_DisplayID display = SDL_GetDisplayForWindow(as->window);
    SDL_DisplayMode const *current_mode = SDL_GetCurrentDisplayMode(display);
    if (current_mode) {
        SDL_Log(
            "Current display mode: %dx%d @%.2fHz, format: %s\n",
            current_mode->w,
            current_mode->h,
            current_mode->refresh_rate,
            SDL_GetPixelFormatName(current_mode->format)
        );
    }

    // Create renderer
    as->renderer = SDL_CreateRenderer(as->window, NULL);
    if (!as->renderer) {
        SDL_Log("Failed to create renderer: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Check renderer properties
    SDL_PropertiesID props = SDL_GetRendererProperties(as->renderer);
    if (props) {
        char const *name = SDL_GetStringProperty(props, SDL_PROP_RENDERER_NAME_STRING, "Unknown");
        SDL_Log("Renderer: %s\n", name);

        SDL_PixelFormat const *formats =
                (SDL_PixelFormat const *)
                SDL_GetPointerProperty(props, SDL_PROP_RENDERER_TEXTURE_FORMATS_POINTER, NULL);
        if (formats) {
            SDL_Log("Supported texture formats:\n");
            for (int j = 0; formats[j] != SDL_PIXELFORMAT_UNKNOWN; j++) {
                SDL_Log("  Format %d: %s\n", j, SDL_GetPixelFormatName(formats[j]));
            }
        }
    }

    as->framebuffer = SDL_CreateTexture(
        as->renderer,
        SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING,
        WINDOW_WIDTH,
        WINDOW_HEIGHT
    );
    as->clean_background = SDL_CreateTexture(
        as->renderer,
        SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING,
        WINDOW_WIDTH,
        WINDOW_HEIGHT
    );

    if (!as->framebuffer) {
        SDL_Log("Framebuffer texture could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(as->renderer);
        SDL_DestroyWindow(as->window);
        SDL_free(as);
        return SDL_APP_FAILURE;
    }

    as->pixels = (Uint16 *) SDL_calloc(WINDOW_WIDTH * WINDOW_HEIGHT, sizeof(Uint16));
    if (!as->pixels) {
        SDL_Log("Could not allocate pixel buffer!\n");
        SDL_DestroyRenderer(as->renderer);
        SDL_DestroyWindow(as->window);
        SDL_free(as);
        return SDL_APP_FAILURE;
    }

    printf("Space State NL - created window\n");

    ensure_images_exist();

    printf("Space State NL - ensured images exist\n");

    // Render background
    render_png_to_framebuffer(
        as->pixels,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        BACKGROUND_IMAGE,
        0,
        0
    );
    SDL_UpdateTexture(as->clean_background, NULL, as->pixels, WINDOW_WIDTH * sizeof(Uint16));
    printf("Space State NL - rendered background\n");

    printf("Space State NL - end of AppInit\n");
    return 0;
}


void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    SDL_Log("SDL_AppQuit\n");
    AppState *as = (AppState *) appstate;

    if (as) {
        if (as->pixels) {
            SDL_free(as->pixels);
        }
        if (as->framebuffer) {
            SDL_DestroyTexture(as->framebuffer);
        }
        if (as->clean_background) {
            SDL_DestroyTexture(as->clean_background);
        }
        if (as->renderer) {
            SDL_DestroyRenderer(as->renderer);
        }
        if (as->window) {
            SDL_DestroyWindow(as->window);
        }
        SDL_free(as);
    }

    curl_global_cleanup();
}