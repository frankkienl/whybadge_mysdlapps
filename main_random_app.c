//
// Created by FrankkieNL on 23/08/2025.
//

//#define WHY_BADGE 1 // CAN BE SET IN THE CMAKE FILE


#include <stdio.h>
#include <string.h>

#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "font.h"
#include "stdlib.h"

#ifdef WHY_BADGE
#include "badgevms/device.h" // needed for orientation sensor
#include "sys/unistd.h" // needed for sleep
#endif

#define WINDOW_WIDTH     720
#define WINDOW_HEIGHT    720
#ifdef WHY_BADGE
#define WINDOW_FLAGS     SDL_WINDOW_FULLSCREEN
#endif
#ifndef WHY_BADGE
#define WINDOW_FLAGS     0
#endif

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
    KEYBOARD_SCREEN,
    FILES_SCREEN,
    SENSORS_SCREEN,
    ABOUT_SCREEN
} RandomAppScreens;

typedef struct {
    bool showWelcomeScreenDesc;
    Uint64 lastChange;
} WelcomeScreenContext;

typedef struct {
    char *name;
    char *version;
    char *description;
} MenuScreenOption_t;

typedef enum {
    MENU_KEYS, MENU_FILES, MENU_SENSORS, MENU_ABOUT, MENU_COUNT
} MenuScreenOptions;

typedef struct {
    int scroll_offset;
    int selected_item;
    int total_items;
    int items_per_page;
    MenuScreenOption_t menu_options[MENU_COUNT];
    bool shouldRepaint;
    Uint16 lastChange;
} MenuScreenContext;

typedef struct {
    int scroll_offset;
    int selected_item;
    int total_items;
    int items_per_page;
    // Add other fields as needed for file explorer
} FilesScreenContext;

typedef struct {
    bool shouldRepaint;
    Uint16 lastChange;
    int latestScancode;
} KeyboardScreenContext;

typedef struct {
    bool shouldRepaint;
    Uint16 lastChange;
    void *orientationSensor;
    void *gasSensor;
} SensorsScreenContext;

typedef struct {
    int currentScreen;
    WelcomeScreenContext *welcomeScreenCtx;
    MenuScreenContext *menuScreenCtx;
    KeyboardScreenContext *keyboardScreenCtx;
    FilesScreenContext *filesScreenCtx;
    SensorsScreenContext *sensorsScreenCtx;
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

void welcome_screen_logic(AppState *ctx) {
    if (ctx->appCtx->currentScreen != WELCOME_SCREEN) {
        return;
    }

    // Don't render screen if nothing changed.
    bool shouldRender = false;
    //First time here?
    if (ctx->appCtx->welcomeScreenCtx->lastChange == 0) {
        //Then initialize
        ctx->appCtx->welcomeScreenCtx->lastChange = SDL_GetTicks();
        ctx->appCtx->welcomeScreenCtx->showWelcomeScreenDesc = true;
        shouldRender = true; //repaint
    }

    const int blink_interval = 500; // milliseconds
    const Uint64 now = SDL_GetTicks();
    if (now - ctx->appCtx->welcomeScreenCtx->lastChange >= blink_interval) {
        ctx->appCtx->welcomeScreenCtx->showWelcomeScreenDesc = !ctx->appCtx->welcomeScreenCtx->showWelcomeScreenDesc;
        ctx->appCtx->welcomeScreenCtx->lastChange = now;
        shouldRender = true; //repaint
    }

    if (!shouldRender) {
        return;
    }

    const int window_x = 30;
    const int window_y = 30;
    const int window_w = WINDOW_WIDTH - 60;
    const int window_h = WINDOW_HEIGHT - 60;

    draw_rect(ctx, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, CDE_BG_COLOR);

    draw_rect(ctx, window_x, window_y, window_w, window_h, CDE_PANEL_COLOR);
    draw_3d_border(ctx, window_x, window_y, window_w, window_h, 0);

    const int title_h = 45;
    draw_rect(ctx, window_x + 3, window_y + 3, window_w - 6, title_h, CDE_TITLE_BG);
    draw_text_bold(ctx, window_x + 15, window_y + 11, "Random App - Welcome", CDE_SELECTED_TEXT);

    int content_y = window_y + window_h / 2;

    if (ctx->appCtx->welcomeScreenCtx->showWelcomeScreenDesc) {
        draw_text_centered(ctx, window_x, content_y, window_w, "Press any key to continue...", CDE_TEXT_COLOR);
    }
    // Render everything
    SDL_RenderClear(ctx->renderer);
    SDL_UpdateTexture(ctx->framebuffer, NULL, ctx->pixels, WINDOW_WIDTH * sizeof(Uint16));
    SDL_RenderTexture(ctx->renderer, ctx->framebuffer, NULL, NULL);
    SDL_RenderPresent(ctx->renderer);
}

void menu_screen_logic(AppState *ctx) {
    if (ctx->appCtx->menuScreenCtx->total_items == 0) {
        ctx->appCtx->menuScreenCtx->total_items = MENU_COUNT;
        // Fill menu
        const MenuScreenOption_t items[MENU_COUNT] = {
            {"Keyboard test", "1.0", "Check keyboard scancodes"},
            {"File explorer", "0.1", "Read files and directories"},
            {"Sensors", "1.0", "Read sensor data"},
            {"About", "1.0", "About this app"}
        };
        ctx->appCtx->menuScreenCtx->menu_options[0] = items[0];
        ctx->appCtx->menuScreenCtx->menu_options[1] = items[1];
        ctx->appCtx->menuScreenCtx->menu_options[2] = items[2];
        ctx->appCtx->menuScreenCtx->menu_options[3] = items[3];
        ctx->appCtx->menuScreenCtx->shouldRepaint = true;
    }

    if (!ctx->appCtx->menuScreenCtx->shouldRepaint) {
        return;
    }

    int window_x = 30;
    int window_y = 30;
    int window_w = WINDOW_WIDTH - 60;
    int window_h = WINDOW_HEIGHT - 60;

    draw_rect(ctx, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, CDE_BG_COLOR);

    draw_rect(ctx, window_x, window_y, window_w, window_h, CDE_PANEL_COLOR);
    draw_3d_border(ctx, window_x, window_y, window_w, window_h, 0);

    int title_h = 45;
    draw_rect(ctx, window_x + 3, window_y + 3, window_w - 6, title_h, CDE_TITLE_BG);
    draw_text_bold(ctx, window_x + 15, window_y + 11, "Random App - Menu", CDE_SELECTED_TEXT);

    char count_text[64];
    snprintf(count_text, sizeof(count_text), "Menu Options Available: %d", ctx->appCtx->menuScreenCtx->total_items);
    draw_text(ctx, window_x + 15, window_y + title_h + 20, count_text, CDE_TEXT_COLOR);

    int list_y = window_y + title_h + 55;
    int list_h = window_h - title_h - 110;
    int item_height = 80;

    draw_rect(ctx, window_x + 15, list_y, window_w - 30, list_h, 0xFFFFFF);
    draw_3d_border(ctx, window_x + 15, list_y, window_w - 30, list_h, 1);

    ctx->appCtx->menuScreenCtx->items_per_page = (list_h - 6) / item_height;
    int visible_start = ctx->appCtx->menuScreenCtx->scroll_offset;
    int visible_end = visible_start + ctx->appCtx->menuScreenCtx->items_per_page;
    if (visible_end > ctx->appCtx->menuScreenCtx->total_items)
        visible_end = ctx->appCtx->menuScreenCtx->total_items;

    for (int i = visible_start; i < visible_end; i++) {
        int item_y = list_y + 3 + (i - visible_start) * item_height;
        int item_x = window_x + 18;
        int item_w = window_w - 36;

        if (i == ctx->appCtx->menuScreenCtx->selected_item) {
            draw_rect(ctx, item_x, item_y, item_w, item_height - 2, CDE_SELECTED_BG);
        }

        Uint32 text_color = (i == ctx->appCtx->menuScreenCtx->selected_item) ? CDE_SELECTED_TEXT : CDE_TEXT_COLOR;

        draw_text_bold(ctx, item_x + 8, item_y + 6, ctx->appCtx->menuScreenCtx->menu_options[i].name, text_color);

        char version_text[64];
        snprintf(version_text, sizeof(version_text), "Version: %s",
                 ctx->appCtx->menuScreenCtx->menu_options[i].version);
        draw_text(ctx, item_x + 8, item_y + 30, version_text, text_color);

        char desc[60] = {0};
        int max_desc_chars = (item_w - 16) / FONT_WIDTH;
        if (max_desc_chars > 59)
            max_desc_chars = 59;
        if (ctx->appCtx->menuScreenCtx->menu_options[i].description) {
            strncpy(desc, ctx->appCtx->menuScreenCtx->menu_options[i].description, max_desc_chars);
            desc[max_desc_chars] = '\0';
            if (strlen(ctx->appCtx->menuScreenCtx->menu_options[i].description) > max_desc_chars) {
                desc[max_desc_chars - 3] = '.';
                desc[max_desc_chars - 2] = '.';
                desc[max_desc_chars - 1] = '.';
            }
        }
        draw_text(ctx, item_x + 8, item_y + 54, desc, text_color);

        if (i < visible_end - 1) {
            draw_rect(ctx, item_x, item_y + item_height - 2, item_w, 1, CDE_BORDER_DARK);
        }
    }

    if (ctx->appCtx->menuScreenCtx->total_items > ctx->appCtx->menuScreenCtx->items_per_page) {
        int scrollbar_x = window_x + window_w - 35;
        int scrollbar_y = list_y + 3;
        int scrollbar_h = list_h - 6;

        draw_rect(ctx, scrollbar_x, scrollbar_y, 20, scrollbar_h, CDE_BUTTON_COLOR);
        draw_3d_border(ctx, scrollbar_x, scrollbar_y, 20, scrollbar_h, 1);

        int thumb_h = (scrollbar_h * ctx->appCtx->menuScreenCtx->items_per_page) / ctx->appCtx->menuScreenCtx->
                      total_items;
        if (thumb_h < 30)
            thumb_h = 30; // Minimum thumb size
        int thumb_y = scrollbar_y;
        if (ctx->appCtx->menuScreenCtx->total_items > ctx->appCtx->menuScreenCtx->items_per_page) {
            thumb_y += ((scrollbar_h - thumb_h) * ctx->appCtx->menuScreenCtx->scroll_offset) / (
                ctx->appCtx->menuScreenCtx->total_items - ctx->appCtx->menuScreenCtx->items_per_page);
        }

        draw_rect(ctx, scrollbar_x + 3, thumb_y, 14, thumb_h, CDE_PANEL_COLOR);
        draw_3d_border(ctx, scrollbar_x + 3, thumb_y, 14, thumb_h, 0);
    }

    draw_text(
        ctx,
        window_x + 15,
        window_y + window_h - 35,
        "UP/DOWN to navigate, SPACE to open, ESC to exit",
        CDE_TEXT_COLOR
    );

    // Render everything
    SDL_RenderClear(ctx->renderer);
    SDL_UpdateTexture(ctx->framebuffer, NULL, ctx->pixels, WINDOW_WIDTH * sizeof(Uint16));
    SDL_RenderTexture(ctx->renderer, ctx->framebuffer, NULL, NULL);
    SDL_RenderPresent(ctx->renderer);
}

void menu_screen_handle_key(AppState *as, const SDL_Scancode key_code) {
    if (as->appCtx->currentScreen != MENU_SCREEN) {
        return;
    }
    as->appCtx->menuScreenCtx->shouldRepaint = true;
    MenuScreenContext *ctx = as->appCtx->menuScreenCtx;
    switch (key_code) {
        case SDL_SCANCODE_UP:
            if (ctx->selected_item > 0) {
                ctx->selected_item--;
                if (ctx->selected_item < ctx->scroll_offset) {
                    ctx->scroll_offset = ctx->selected_item;
                }
            }
            printf("menu_screen_handle_key; (up) selected_item: %d\n", ctx->selected_item);
            break;

        case SDL_SCANCODE_DOWN:
            if (ctx->selected_item < ctx->total_items - 1) {
                ctx->selected_item++;
                if (ctx->selected_item >= ctx->scroll_offset + ctx->items_per_page) {
                    ctx->scroll_offset = ctx->selected_item - ctx->items_per_page + 1;
                }
            }
            printf("menu_screen_handle_key; (down) selected_item: %d\n", ctx->selected_item);
            break;

        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_SPACE:
            printf("menu_screen_handle_key; (space/return) selected_item: %d\n", ctx->selected_item);
            switch (ctx->selected_item) {
                case MENU_KEYS: {
                    as->appCtx->currentScreen = KEYBOARD_SCREEN;
                    break;
                }
                case MENU_FILES: {
                    as->appCtx->currentScreen = FILES_SCREEN;
                    break;
                }
                case MENU_SENSORS: {
                    as->appCtx->currentScreen = SENSORS_SCREEN;
                    break;
                }
                case MENU_ABOUT: {
                    as->appCtx->currentScreen = ABOUT_SCREEN;
                    break;
                }
                default: break;
            }
        default: break;
    }
}

void files_screen_logic(AppState *ctx) {
    if (ctx->appCtx->currentScreen != FILES_SCREEN) {
        return;
    }
    // Don't render screen if nothing changed.
    bool shouldRender = true;

    if (!shouldRender) {
        return;
    }

    const int window_x = 30;
    const int window_y = 30;
    const int window_w = WINDOW_WIDTH - 60;
    const int window_h = WINDOW_HEIGHT - 60;

    draw_rect(ctx, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, CDE_BG_COLOR);

    draw_rect(ctx, window_x, window_y, window_w, window_h, CDE_PANEL_COLOR);
    draw_3d_border(ctx, window_x, window_y, window_w, window_h, 0);

    const int title_h = 45;
    draw_rect(ctx, window_x + 3, window_y + 3, window_w - 6, title_h, CDE_TITLE_BG);
    draw_text_bold(ctx, window_x + 15, window_y + 11, "Random App - About", CDE_SELECTED_TEXT);

    int content_y = 120;

    char const *lines[] = {
        "File Explorer",
        "TODO",
        "",
        "Press any key to return.",
    };
    for (int i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        draw_text_centered(ctx, window_x, content_y, window_w, lines[i], CDE_TEXT_COLOR);
        content_y += FONT_HEIGHT + 8;
    }

    // Render everything
    SDL_RenderClear(ctx->renderer);
    SDL_UpdateTexture(ctx->framebuffer, NULL, ctx->pixels, WINDOW_WIDTH * sizeof(Uint16));
    SDL_RenderTexture(ctx->renderer, ctx->framebuffer, NULL, NULL);
    SDL_RenderPresent(ctx->renderer);
}

void keyboard_screen_logic(AppState *ctx) {
    if (ctx->appCtx->currentScreen != KEYBOARD_SCREEN) {
        return;
    }
    // First time here?
    if (ctx->appCtx->keyboardScreenCtx->lastChange == 0) {
        ctx->appCtx->keyboardScreenCtx->lastChange = SDL_GetTicks();
        ctx->appCtx->keyboardScreenCtx->shouldRepaint = true;
    }

    if (!ctx->appCtx->keyboardScreenCtx->shouldRepaint) {
        // Don't render screen if nothing changed.
        return;
    }

    printf("Rendering keyboard screen\n");
    ctx->appCtx->keyboardScreenCtx->shouldRepaint = false;

    const int window_x = 30;
    const int window_y = 30;
    const int window_w = WINDOW_WIDTH - 60;
    const int window_h = WINDOW_HEIGHT - 60;

    draw_rect(ctx, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, CDE_BG_COLOR);

    draw_rect(ctx, window_x, window_y, window_w, window_h, CDE_PANEL_COLOR);
    draw_3d_border(ctx, window_x, window_y, window_w, window_h, 0);

    const int title_h = 45;
    draw_rect(ctx, window_x + 3, window_y + 3, window_w - 6, title_h, CDE_TITLE_BG);
    draw_text_bold(ctx, window_x + 15, window_y + 11, "Random App - Keyboard", CDE_SELECTED_TEXT);

    int content_y = 120;

    char latestScanCodeAsString[128];
    snprintf(latestScanCodeAsString, sizeof(latestScanCodeAsString), "0x%02X",
             ctx->appCtx->keyboardScreenCtx->latestScancode);

    char const *lines[] = {
        "Keyboard test",
        "Press any key, to see its scancode.",
        "",
        "Scan code of latest key pressed will be shown below:",
        latestScanCodeAsString,
        "",
        "Press ESC key to return to menu.",
        "(Press ESC to exit the app)",
    };
    for (int i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        draw_text_centered(ctx, window_x, content_y, window_w, lines[i], CDE_TEXT_COLOR);
        content_y += FONT_HEIGHT + 8;
    }

    // Render everything
    SDL_RenderClear(ctx->renderer);
    SDL_UpdateTexture(ctx->framebuffer, NULL, ctx->pixels, WINDOW_WIDTH * sizeof(Uint16));
    SDL_RenderTexture(ctx->renderer, ctx->framebuffer, NULL, NULL);
    SDL_RenderPresent(ctx->renderer);
}

void about_screen_logic(AppState *ctx) {
    if (ctx->appCtx->currentScreen != ABOUT_SCREEN) {
        return;
    }
    // Don't render screen if nothing changed.
    bool shouldRender = true;

    if (!shouldRender) {
        return;
    }

    const int window_x = 30;
    const int window_y = 30;
    const int window_w = WINDOW_WIDTH - 60;
    const int window_h = WINDOW_HEIGHT - 60;

    draw_rect(ctx, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, CDE_BG_COLOR);

    draw_rect(ctx, window_x, window_y, window_w, window_h, CDE_PANEL_COLOR);
    draw_3d_border(ctx, window_x, window_y, window_w, window_h, 0);

    const int title_h = 45;
    draw_rect(ctx, window_x + 3, window_y + 3, window_w - 6, title_h, CDE_TITLE_BG);
    draw_text_bold(ctx, window_x + 15, window_y + 11, "Random App - About", CDE_SELECTED_TEXT);

    int content_y = 120;

    char const *lines[] = {
        "RandomApp is a simple demo application",
        "showcasing SDL3 features on the WHY Badge.",
        "",
        "Created by FrankkieNL, 2025.",
        "Version:",
        APP_VERSION,
        "",
        "Visit https://badge.why2025.org for more info.",
        "",
        "Press any key to return.",
    };
    for (int i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        draw_text_centered(ctx, window_x, content_y, window_w, lines[i], CDE_TEXT_COLOR);
        content_y += FONT_HEIGHT + 8;
    }

    // Render everything
    SDL_RenderClear(ctx->renderer);
    SDL_UpdateTexture(ctx->framebuffer, NULL, ctx->pixels, WINDOW_WIDTH * sizeof(Uint16));
    SDL_RenderTexture(ctx->renderer, ctx->framebuffer, NULL, NULL);
    SDL_RenderPresent(ctx->renderer);
}

void sensors_screen_logic(AppState *ctx) {
    if (ctx->appCtx->currentScreen != SENSORS_SCREEN) {
        return;
    }
    // Don't render screen if nothing changed.
    bool shouldRender = true;

    if (!shouldRender) {
        return;
    }

    const int window_x = 30;
    const int window_y = 30;
    const int window_w = WINDOW_WIDTH - 60;
    const int window_h = WINDOW_HEIGHT - 60;

    draw_rect(ctx, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, CDE_BG_COLOR);

    draw_rect(ctx, window_x, window_y, window_w, window_h, CDE_PANEL_COLOR);
    draw_3d_border(ctx, window_x, window_y, window_w, window_h, 0);

    const int title_h = 45;
    draw_rect(ctx, window_x + 3, window_y + 3, window_w - 6, title_h, CDE_TITLE_BG);
    draw_text_bold(ctx, window_x + 15, window_y + 11, "Random App - Sensors", CDE_SELECTED_TEXT);

    int content_y = 120;

#ifdef WHY_BADGE
    char sensor1[128];
    char sensor2[128];
    if (ctx->appCtx->sensorsScreenCtx->orientationSensor != NULL) {
        orientation_device_t *orientation_device = ctx->appCtx->sensorsScreenCtx->orientationSensor;
        snprintf(sensor1, sizeof(sensor1), "orientation: %d", orientation_device->_get_orientation(orientation_device));
        snprintf(sensor2, sizeof(sensor2), "orientation degress: %d", orientation_device->_get_orientation_degrees(orientation_device));
    }
    char sensor3[128];
    char sensor4[128];
    char sensor5[128];
    char sensor6[128];
    if (ctx->appCtx->sensorsScreenCtx->gasSensor != NULL) {
        gas_device_t *gas = ctx->appCtx->sensorsScreenCtx->gasSensor;
        snprintf(sensor3, sizeof(sensor3),"Temperature in Celsius: %d \n", gas->_get_temperature(gas));
        snprintf(sensor4, sizeof(sensor4),"Humidity in Rel. Percentage: %d \n", gas->_get_humidity(gas));
        snprintf(sensor5, sizeof(sensor5),"Pressure in Pascal: %d \n", gas->_get_pressure(gas));
        snprintf(sensor6, sizeof(sensor6),"Gas Resistance in Ohm: %d \n", gas->_get_gas_resistance(gas));
    }
    char const *lines[] = {
        "Sensors screen",
        "",
        "BMI 270 - Orientation sensor",
        sensor1,
        sensor2,
        "",
        "BME 690 - Gas sensor",
        sensor3,
        sensor4,
        sensor5,
        sensor6,
        "",
        "Press any key to return.",
    };
#else
    char const *lines[] = {
        "Sensors screen",
        "No sensors found, ",
        "as this is not a WHY Badge build of this app",
        "Press any key to return.",
    };
#endif

    for (int i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        draw_text_centered(ctx, window_x, content_y, window_w, lines[i], CDE_TEXT_COLOR);
        content_y += FONT_HEIGHT + 8;
    }

    // Render everything
    SDL_RenderClear(ctx->renderer);
    SDL_UpdateTexture(ctx->framebuffer, NULL, ctx->pixels, WINDOW_WIDTH * sizeof(Uint16));
    SDL_RenderTexture(ctx->renderer, ctx->framebuffer, NULL, NULL);
    SDL_RenderPresent(ctx->renderer);
}

static SDL_AppResult handle_key_event_(AppState *ctx, SDL_Scancode key_code) {
    printf("handle_key_event_\n");
    if (ctx->appCtx->currentScreen != KEYBOARD_SCREEN) {
        switch (key_code) {
            /* Quit. */
            case SDL_SCANCODE_ESCAPE:
            case SDL_SCANCODE_Q: return SDL_APP_SUCCESS;
            default: break;
        }
    }

    if (ctx->appCtx->currentScreen == WELCOME_SCREEN) {
        // Any key to continue
        ctx->appCtx->currentScreen = MENU_SCREEN;
        return SDL_APP_CONTINUE;
    }

    if (ctx->appCtx->currentScreen == MENU_SCREEN) {
        menu_screen_handle_key(ctx, key_code);
        return SDL_APP_CONTINUE;
    }

    if (ctx->appCtx->currentScreen == KEYBOARD_SCREEN) {
        // Update latest scancode
        ctx->appCtx->keyboardScreenCtx->latestScancode = key_code;
        ctx->appCtx->keyboardScreenCtx->shouldRepaint = true;
        // If ESC pressed, go back to menu
        if (key_code == SDL_SCANCODE_ESCAPE) {
            ctx->appCtx->currentScreen = MENU_SCREEN;
            // Force redraw
            ctx->appCtx->menuScreenCtx->shouldRepaint = true;
        }
        return SDL_APP_CONTINUE;
    }

    if (ctx->appCtx->currentScreen == ABOUT_SCREEN) {
        // Any key to continue
        ctx->appCtx->currentScreen = MENU_SCREEN;
        // Force redraw
        ctx->appCtx->welcomeScreenCtx->lastChange = 0;
        return SDL_APP_CONTINUE;
    }

    if (ctx->appCtx->currentScreen == SENSORS_SCREEN) {
        // Any key to continue
        ctx->appCtx->currentScreen = MENU_SCREEN;
        // Force redraw
        ctx->appCtx->welcomeScreenCtx->lastChange = 0;
        return SDL_APP_CONTINUE;
    }

    if (ctx->appCtx->currentScreen == FILES_SCREEN) {
        // Any key to continue
        ctx->appCtx->currentScreen = MENU_SCREEN;
        // Force redraw
        ctx->appCtx->welcomeScreenCtx->lastChange = 0;
        return SDL_APP_CONTINUE;
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

SDL_AppResult SDL_AppIterate(void *appstate) {
    // printf("SDL_AppIterate\n");
    AppState *as = (AppState *) appstate;
    RandomAppContext *ctx = as->appCtx;
    Uint64 const now = SDL_GetTicks();

    switch (ctx->currentScreen) {
        case WELCOME_SCREEN: welcome_screen_logic(appstate);
            break;
        case MENU_SCREEN: menu_screen_logic(appstate);
            break;
        case KEYBOARD_SCREEN: keyboard_screen_logic(appstate);
            break;
        case FILES_SCREEN: files_screen_logic(appstate);
            break;
        case SENSORS_SCREEN: sensors_screen_logic(appstate);
            break;
        case ABOUT_SCREEN: about_screen_logic(appstate);
            break;
        default: break;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    //printf("SDL_AppEvent\n");
    //RandomAppContext *ctx = ((AppState *) appstate)->appCtx;
    AppState *as = (AppState *) appstate;
    switch (event->type) {
        case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
        case SDL_EVENT_JOYSTICK_ADDED:
            if (joystick == NULL) {
#ifndef WHY_BADGE
                joystick = SDL_OpenJoystick(event->jdevice.which);
                if (!joystick) {
                    printf("Failed to open joystick ID %u: %s", (unsigned int) event->jdevice.which, SDL_GetError());
                }
#endif
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

#ifndef WHY_BADGE
    if (!SDL_SetAppMetadata(APP_NAME, APP_VERSION, APP_ID)) {
        return SDL_APP_FAILURE;
    }
#endif

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    AppState *as = (AppState *) SDL_calloc(1, sizeof(AppState));
    if (!as) {
        return SDL_APP_FAILURE;
    }

    *appstate = as;

    as->appCtx = (RandomAppContext *) SDL_calloc(1, sizeof(RandomAppContext));
    if (!as->appCtx) {
        SDL_free(as);
        return SDL_APP_FAILURE;
    }
    as->appCtx->currentScreen = WELCOME_SCREEN;

    as->appCtx->welcomeScreenCtx = (WelcomeScreenContext *) SDL_calloc(1, sizeof(WelcomeScreenContext));
    as->appCtx->menuScreenCtx = (MenuScreenContext *) SDL_calloc(1, sizeof(MenuScreenContext));
    as->appCtx->filesScreenCtx = (FilesScreenContext *) SDL_calloc(1, sizeof(FilesScreenContext));
    as->appCtx->keyboardScreenCtx = (KeyboardScreenContext *) SDL_calloc(1, sizeof(KeyboardScreenContext));
    as->appCtx->sensorsScreenCtx = (SensorsScreenContext *) SDL_calloc(1, sizeof(SensorsScreenContext));

    //Create window first
    as->window = SDL_CreateWindow(APP_NAME, WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_FLAGS);
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

#ifdef WHY_BADGE
    //sleep(5);
    orientation_device_t *orientation;
    orientation = (orientation_device_t *)device_get("ORIENTATION0");
    as->appCtx->sensorsScreenCtx->orientationSensor = orientation;
    if (orientation == NULL) {
        printf("Well, no device found");
    } else {
        printf("Get BMI270 accel...\n");
        int ret = orientation->_get_orientation(orientation);
        printf("Orientation: %d \n", ret);
        int degrees = orientation->_get_orientation_degrees(orientation);
        printf("Orientation degrees: %d \n", degrees);
    }

    gas_device_t *gas;
    gas = (gas_device_t *)device_get("GAS0");
    as->appCtx->sensorsScreenCtx->gasSensor = gas;
    if (gas == NULL) {
        printf("Well, no device found");
    } else {
        printf("Get BME690...\n");
        printf("Temperature in Celsius: %d \n", gas->_get_temperature(gas));
        printf("Humidity in Rel. Percentage: %d \n", gas->_get_humidity(gas));
        printf("Pressure in Pascal: %d \n", gas->_get_pressure(gas));
        printf("Gas Resistance in Ohm: %d \n", gas->_get_gas_resistance(gas));
    }
#endif

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    printf("SDL_AppQuit\n");
    if (joystick) {
        SDL_CloseJoystick(joystick);
    }
    if (appstate != NULL) {
        AppState *as = (AppState *) appstate;
        SDL_free(as->appCtx->keyboardScreenCtx);
        SDL_free(as->appCtx->sensorsScreenCtx);
        SDL_free(as->appCtx->filesScreenCtx);
        SDL_free(as->appCtx->menuScreenCtx);
        SDL_free(as->appCtx->welcomeScreenCtx);
        SDL_free(as->appCtx);
        SDL_free(as->pixels);
        SDL_DestroyTexture(as->framebuffer);
        SDL_DestroyRenderer(as->renderer);
        SDL_DestroyWindow(as->window);
        SDL_free(as);
    }
}
