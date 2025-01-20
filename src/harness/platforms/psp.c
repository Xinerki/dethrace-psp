// #include "harness.h"
// #include "harness/config.h"
#include "compiler.h"
#include "harness/hooks.h"
#include "pixelmap.h"
#include <stdint.h>
// #include "harness/trace.h"

#define __GLIBC_USE

#include <memory.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <psprtc.h>

// #define STB_IMAGE_IMPLEMENTATION
// #include <stb_image.h>

PSP_MODULE_INFO("DETHRACE", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

int running;

int exit_callback(int arg1, int arg2, void* common) {
    running = 0;
    return 0;
}

int callback_thread(SceSize args, void* argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int setup_callbacks(void) {
    int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0)
        sceKernelStartThread(thid, 0, 0);
    return thid;
}

#define BUFFER_WIDTH 512
#define BUFFER_HEIGHT 272
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT BUFFER_HEIGHT

void* fbp0;
void* fbp1;
char list[0x20000] __attribute__((aligned(64)));

static void psp_quit(void* wnd) {
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
}

static void psp_sleep(uint32_t ms) {
    const uint32_t max_delay = 0xFFFFFFFFUL / 1000;
    if (ms > max_delay) {
        ms = max_delay;
    }
    sceKernelDelayThreadCB(ms * 1000);
}

typedef struct
{
    int width, height;
    uint32_t* data;
} Texture;

typedef struct
{
    float u, v;
    uint32_t colour;
    float x, y, z;
} TextureVertex;

// Texture* texture;

uint32_t converted_palette[256];
br_pixelmap* last_screen_src;

void* buffer = 0;
static void psp_present(br_pixelmap* src) {
    uint8_t* src_pixels = src->pixels;
    uint32_t* dest_pixels = 0;

    for (int i = 0; i < src->height * src->width; i++) {
        *dest_pixels = converted_palette[*src_pixels];
        dest_pixels++;
        src_pixels++;
    }

    // texture->data = dest_pixels;

    sceKernelDcacheWritebackInvalidateAll();

    // start frame
    sceGuStart(GU_DIRECT, list);
    sceGuClearColor(0xFF00FF00); // green!
    sceGuClear(GU_COLOR_BUFFER_BIT);

    // draw
    static TextureVertex vertices[2];

    vertices[0].u = 0.0f;
    vertices[0].v = 0.0f;
    vertices[0].colour = 0xFFFFFFFF;
    vertices[0].x = 0.0f;
    vertices[0].y = 0.0f;
    vertices[0].z = 0.0f;

    vertices[1].u = 480.0;
    vertices[1].v = 272.0;
    vertices[1].colour = 0xFFFFFFFF;
    vertices[1].x = 0.0f + 480.0;
    vertices[1].y = 0.0f + 272.0;
    vertices[1].z = 0.0f;

    // sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
    sceGuTexMode(GU_PSM_4444, 0, 0, GU_FALSE);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexImage(0, src->width, src->height, src->width, src_pixels);

    sceGuEnable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, vertices);
    sceGuDisable(GU_TEXTURE_2D);

    // end frame
    sceGuFinish();
    sceGuSync(0, 0);

    pspDebugScreenSetOffset((int)buffer);
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("swag");

    sceDisplayWaitVblankStart();
    buffer = sceGuSwapBuffers();

    last_screen_src = src;
}

static void set_palette(PALETTEENTRY_* pal) {
    for (int i = 0; i < 256; i++) {
        converted_palette[i] = (0xff << 24 | pal[i].peRed << 16 | pal[i].peGreen << 8 | pal[i].peBlue);
    }

    // sceGuClutMode(GU_PSM_4444, 0, 255, 0);
    // sceGuClutLoad(256 / 8, converted_palette);

    sceKernelDcacheWritebackAll();

    if (last_screen_src != NULL) {
        psp_present(last_screen_src);
    }
}

// thanks SDL2
uint64_t start_tick = 0;
uint64_t ticks;
static uint64_t PSP_Ticks(void) {
    sceRtcGetCurrentTick(&ticks);
    return ticks;
}

uint64_t psp_get_ticks64(void) {
    return (PSP_Ticks() - start_tick) / sceRtcGetTickResolution();
}

static uint32_t psp_get_ticks(void) {
    return (uint32_t)(psp_get_ticks64());
}

static int psp_get_and_handle_message(MSG_* msg) {
    // TODO(Xinerki):

    if (!running) {
        msg->message = WM_QUIT;
        return 1;
    }

    return 0;
}

static void psp_get_keyboard_state(unsigned int count, uint8_t* buffer) {
    // TODO(Xinerki):
}

static int psp_get_mouse_position(int* pX, int* pY) {
    // TODO(Xinerki):
    *pX = 0;
    *pY = 0;
    return 0;
}

static int psp_get_mouse_buttons(int* pButton1, int* pButton2) {
    // TODO(Xinerki):
    *pButton1 = 0;
    *pButton2 = 0;
    return 0;
}

static int psp_show_error_message(void* window, char* text, char* caption) {
    // TODO(Xinerki):
    return 0;
}

static void* psp_init(char* title, int x, int y, int width, int height) {
    running = 1;

    setup_callbacks();
    pspDebugScreenInit();
    sceGuInit();

    fbp0 = guGetStaticVramBuffer(BUFFER_WIDTH, BUFFER_HEIGHT, GU_PSM_8888);
    fbp1 = guGetStaticVramBuffer(BUFFER_WIDTH, BUFFER_HEIGHT, GU_PSM_8888);

    // Set up buffers
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, fbp0, BUFFER_WIDTH);
    sceGuDispBuffer(SCREEN_WIDTH, SCREEN_HEIGHT, fbp1, BUFFER_WIDTH);

    // We do not care about the depth buffer in this example
    sceGuDepthBuffer(fbp0, 0);   // Set depth buffer to a length of 0
    sceGuDisable(GU_DEPTH_TEST); // Disable depth testing

    // Set up viewport
    sceGuOffset(2048 - (SCREEN_WIDTH / 2), 2048 - (SCREEN_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Start a new frame and enable the display
    sceGuFinish();
    sceGuDisplay(GU_TRUE);

    pspDebugScreenInit();

    // texture = (Texture*)calloc(1, sizeof(Texture));
    if (start_tick == 0) {
        start_tick = PSP_Ticks();
    }

    return 0;
}

// not possible on psp sorry
static int show_cursor(int show) {
    return 0;
}

static int set_window_pos(void* hWnd, int x, int y, int nWidth, int nHeight) {
    return 0;
}

void Harness_Platform_Init(tHarness_platform* platform) {
    platform->CreateWindowAndRenderer = psp_init;
    platform->ProcessWindowMessages = psp_get_and_handle_message;
    platform->DestroyWindow = psp_quit;
    platform->Sleep = psp_sleep;
    platform->GetTicks = psp_get_ticks;
    platform->ShowCursor = show_cursor;
    platform->SetWindowPos = set_window_pos;
    platform->GetKeyboardState = psp_get_keyboard_state;
    platform->GetMousePosition = psp_get_mouse_position;
    platform->GetMouseButtons = psp_get_mouse_buttons;
    platform->ShowErrorMessage = psp_show_error_message;
    platform->Renderer_SetPalette = set_palette;
    platform->Renderer_Present = psp_present;
}