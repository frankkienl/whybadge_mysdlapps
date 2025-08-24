//
// Created by FrankkieNL on 23/08/2025.
//

//#include "font.h"
#include <stdio.h>
#include <string.h>

#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "font.h"

#define WINDOW_WIDTH     720
#define WINDOW_HEIGHT    720

#define CDE_BG_COLOR      0x9CA0A0
#define CDE_PANEL_COLOR   0xAEB2B2
#define CDE_BORDER_LIGHT  0xFFFFFF
#define CDE_BORDER_DARK   0x636363
#define CDE_TEXT_COLOR    0x000000
#define CDE_SELECTED_BG   0x0078D4
#define CDE_SELECTED_TEXT 0xFFFFFF
#define CDE_BUTTON_COLOR  0xD4D0C8
#define CDE_TITLE_BG      0x808080
#define CDE_PROGRESS_BG   0x606060
#define CDE_PROGRESS_FG   0x0078D4
#define CDE_SUCCESS_COLOR 0x00AA00
#define CDE_ERROR_COLOR   0xA00000

#define APP_NAME "Random App"
#define APP_VERSION "1.0"
#define APP_ID "random_app"

static SDL_Joystick *joystick = NULL;

typedef enum {
    WELCOME_SCREEN,
    MENU_SCREEN,
    FILES_SCREEN,
    KEYBOARD_SCREEN,
    ABOUT_SCREEN
} RandomAppScreens;

typedef struct {
    int currentScreen;
} RandomAppContext;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *framebuffer;
    Uint16 *pixels;
    RandomAppContext *appCtx;
} AppState;

static const struct {
    char const *key;
    char const *value;
} extended_metadata[] = {
    {SDL_PROP_APP_METADATA_URL_STRING, "https://badge.why2025.org/page/project/spacestate_nl"},
    {SDL_PROP_APP_METADATA_CREATOR_STRING, "FrankkieNL"},
    {SDL_PROP_APP_METADATA_COPYRIGHT_STRING, "Placed in the public domain"},
    {SDL_PROP_APP_METADATA_TYPE_STRING, "tool"}
};

static inline Uint16 rgb888_to_rgb565(Uint32 rgb888) {
    Uint8 r = (rgb888 >> 16) & 0xFF;
    Uint8 g = (rgb888 >> 8) & 0xFF;
    Uint8 b = rgb888 & 0xFF;
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}


void draw_rect(AppState *ctx, int x, int y, int w, int h, Uint32 color) {
    Uint16 rgb565 = rgb888_to_rgb565(color);
    int x2 = x + w;
    int y2 = y + h;

    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x2 > WINDOW_WIDTH)
        x2 = WINDOW_WIDTH;
    if (y2 > WINDOW_HEIGHT)
        y2 = WINDOW_HEIGHT;

    for (int py = y; py < y2; py++) {
        Uint16 *row = &ctx->pixels[py * WINDOW_WIDTH + x];
        int width = x2 - x;
        for (int i = 0; i < width; i++) {
            row[i] = rgb565;
        }
    }
}

void draw_char(AppState *ctx, int x, int y, char c, Uint32 color) {
    if (c < FONT_FIRST_CHAR || c > FONT_LAST_CHAR)
        return;

    int char_index = c - FONT_FIRST_CHAR;
    uint16_t const *char_data = pixel_font[char_index];
    Uint16 rgb565 = rgb888_to_rgb565(color);

    for (int row = 0; row < FONT_HEIGHT; row++) {
        uint16_t row_data = char_data[row];
        int py = y + row;

        if (py < 0 || py >= WINDOW_HEIGHT)
            continue;

        for (int col = 0; col < FONT_WIDTH; col++) {
            if (row_data & (0x800 >> col)) {
                // Check bit from MSB
                int px = x + col;
                if (px >= 0 && px < WINDOW_WIDTH) {
                    ctx->pixels[py * WINDOW_WIDTH + px] = rgb565;
                }
            }
        }
    }
}

void draw_text(AppState *ctx, int x, int y, char const *text, Uint32 color) {
    int current_x = x;

    while (*text) {
        draw_char(ctx, current_x, y, *text, color);
        current_x += FONT_WIDTH;
        text++;
    }
}

void draw_text_bold(AppState *ctx, int x, int y, char const *text, Uint32 color) {
    draw_text(ctx, x, y, text, color);
    draw_text(ctx, x + 1, y, text, color);
}

int get_text_width(char const *text) {
    return strlen(text) * FONT_WIDTH;
}

void draw_text_centered(AppState *ctx, int x, int y, int width, char const *text, Uint32 color) {
    int text_w = get_text_width(text);
    int text_x = x + (width - text_w) / 2;
    draw_text(ctx, text_x, y, text, color);
}

void draw_3d_border(AppState *ctx, int x, int y, int w, int h, int inset) {
    Uint32 light_color = inset ? CDE_BORDER_DARK : CDE_BORDER_LIGHT;
    Uint32 dark_color = inset ? CDE_BORDER_LIGHT : CDE_BORDER_DARK;

    draw_rect(ctx, x, y, w, 3, light_color);
    draw_rect(ctx, x, y, 3, h, light_color);

    draw_rect(ctx, x, y + h - 3, w, 3, dark_color);
    draw_rect(ctx, x + w - 3, y, 3, h, dark_color);
}


static SDL_AppResult handle_key_event_(AppState *appstate, SDL_Scancode key_code) {
    printf("handle_key_event_\n");
    switch (key_code) {
        /* Quit. */
        case SDL_SCANCODE_ESCAPE:
        case SDL_SCANCODE_Q: return SDL_APP_SUCCESS;
        default: break;
    }
    return SDL_APP_CONTINUE;
}

static SDL_AppResult handle_hat_event_(AppState *appstate, Uint8 hat_value) {
    printf("handle_hat_event_\n");
    switch (hat_value) {
        case SDL_HAT_UP: /* Up */ break;
        case SDL_HAT_RIGHT: /* Right */ break;
        case SDL_HAT_DOWN: /* Down */ break;
        case SDL_HAT_LEFT: /* Left */ break;
        default: break;
    }
    return SDL_APP_CONTINUE;
}

void welcome_screen_logic(AppState *ctx) {
    int window_x = 30;
    int window_y = 30;
    int window_w = WINDOW_WIDTH - 60;
    int window_h = WINDOW_HEIGHT - 60;

    draw_rect(ctx, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, CDE_BG_COLOR);

    draw_rect(ctx, window_x, window_y, window_w, window_h, CDE_PANEL_COLOR);
    draw_3d_border(ctx, window_x, window_y, window_w, window_h, 0);

    int title_h = 45;
    draw_rect(ctx, window_x + 3, window_y + 3, window_w - 6, title_h, CDE_TITLE_BG);
    draw_text_bold(ctx, window_x + 15, window_y + 11, "Random App", CDE_SELECTED_TEXT);

    int content_y = window_y + window_h / 2 - 120;

    int icon_size = 80;
    int icon_x = window_x + (window_w - icon_size) / 2;
    int icon_y = content_y;

    content_y += icon_size + 40;
    draw_text_centered(ctx, window_x, content_y, window_w, "Press any key to continue...", CDE_TEXT_COLOR);
}

void menu_screen_logic(AppState *appstate) {
}

void files_screen_logic(AppState *appstate) {
}

void keyboard_screen_logic(AppState *appstate) {
}

void about_screen_logic(AppState *appstate) {
}


SDL_AppResult SDL_AppIterate(void *appstate) {
    //printf("SDL_AppIterate\n");
    AppState *as = (AppState *) appstate;
    RandomAppContext *ctx = &as->appCtx;
    Uint64 const now = SDL_GetTicks();

    SDL_RenderClear(as->renderer);

    switch (ctx->currentScreen) {
        case WELCOME_SCREEN: welcome_screen_logic(appstate);
            break;
        case MENU_SCREEN: menu_screen_logic(appstate);
            break;
        case FILES_SCREEN: files_screen_logic(appstate);
            break;
        case KEYBOARD_SCREEN: keyboard_screen_logic(appstate);
            break;
        case ABOUT_SCREEN: about_screen_logic(appstate);
            break;
        default: break;
    }

    SDL_UpdateTexture(as->framebuffer, NULL, as->pixels, WINDOW_WIDTH * sizeof(Uint16));
    SDL_RenderTexture(as->renderer, as->framebuffer, NULL, NULL);
    SDL_RenderPresent(as->renderer);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    //printf("SDL_AppEvent\n");
    RandomAppContext *ctx = &((AppState *) appstate)->appCtx;
    AppState *as = (AppState *) appstate;
    switch (event->type) {
        case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
        case SDL_EVENT_JOYSTICK_ADDED:
            if (joystick == NULL) {
                joystick = SDL_OpenJoystick(event->jdevice.which);
                if (!joystick) {
                    printf("Failed to open joystick ID %u: %s", (unsigned int) event->jdevice.which, SDL_GetError());
                }
            }
            break;
        case SDL_EVENT_JOYSTICK_REMOVED:
            if (joystick && (SDL_GetJoystickID(joystick) == event->jdevice.which)) {
                SDL_CloseJoystick(joystick);
                joystick = NULL;
            }
            break;
        case SDL_EVENT_JOYSTICK_HAT_MOTION: return handle_hat_event_(as, event->jhat.value);
        case SDL_EVENT_KEY_DOWN: return handle_key_event_(as, event->key.scancode);
        default: break;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    printf("SDL_AppInit\n");
    if (!SDL_SetAppMetadata(APP_NAME, APP_VERSION, APP_ID)) {
        return SDL_APP_FAILURE;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    AppState *as = (AppState *) SDL_calloc(1, sizeof(AppState));
    if (!as) {
        return SDL_APP_FAILURE;
    }

    *appstate = as;

    //Create window first
    as->window = SDL_CreateWindow(APP_NAME, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!as->window) {
        printf("Failed to create window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Check display capabilities
    SDL_DisplayID display = SDL_GetDisplayForWindow(as->window);
    SDL_DisplayMode const *current_mode = SDL_GetCurrentDisplayMode(display);
    if (current_mode) {
        printf(
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
        printf("Failed to create renderer: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Check renderer properties
    SDL_PropertiesID props = SDL_GetRendererProperties(as->renderer);
    if (props) {
        char const *name = SDL_GetStringProperty(props, SDL_PROP_RENDERER_NAME_STRING, "Unknown");
        printf("Renderer: %s\n", name);

        SDL_PixelFormat const *formats =
                (SDL_PixelFormat const *)
                SDL_GetPointerProperty(props, SDL_PROP_RENDERER_TEXTURE_FORMATS_POINTER, NULL);
        if (formats) {
            printf("Supported texture formats:\n");
            for (int j = 0; formats[j] != SDL_PIXELFORMAT_UNKNOWN; j++) {
                printf("  Format %d: %s\n", j, SDL_GetPixelFormatName(formats[j]));
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

    if (!as->framebuffer) {
        printf("Framebuffer texture could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(as->renderer);
        SDL_DestroyWindow(as->window);
        SDL_free(as);
        return SDL_APP_FAILURE;
    }

    as->pixels = (Uint16 *) SDL_calloc(WINDOW_WIDTH * WINDOW_HEIGHT, sizeof(Uint16));
    if (!as->pixels) {
        printf("Could not allocate pixel buffer!\n");
        SDL_DestroyRenderer(as->renderer);
        SDL_DestroyWindow(as->window);
        SDL_free(as);
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    printf("SDL_AppQuit\n");
    if (joystick) {
        SDL_CloseJoystick(joystick);
    }
    if (appstate != NULL) {
        AppState *as = (AppState *) appstate;
        SDL_free(as->pixels);
        SDL_DestroyTexture(as->framebuffer);
        SDL_DestroyRenderer(as->renderer);
        SDL_DestroyWindow(as->window);
        SDL_free(as);
    }
}
