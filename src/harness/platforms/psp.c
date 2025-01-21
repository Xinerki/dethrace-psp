// #include "harness.h"
// #include "harness/config.h"
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

#define ENABLE_RENDERING

#define BUFFER_WIDTH 512
#define BUFFER_HEIGHT 272
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT BUFFER_HEIGHT

void* fbp0;
void* fbp1;
char list[0x40000] __attribute__((aligned(64)));

int running;

uint64_t start_tick = 0;

static uint64_t PSP_Ticks(void) {

    uint64_t ticks;
    sceRtcGetCurrentTick(&ticks);
    return ticks;
}

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

static void* psp_init(char* title, int x, int y, int width, int height) {
    running = 1;

    setup_callbacks();

    // texture = (Texture*)calloc(1, sizeof(Texture));
    if (start_tick == 0) {
        start_tick = PSP_Ticks();
    }
#ifdef ENABLE_RENDERING
    // pspDebugScreenInit();
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

    return fbp0;
#else
    return 0;
#endif
}

static void psp_quit(void* wnd) {
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
    running = 0;
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
static unsigned int __attribute__((aligned(16))) pixels[512 * 272];

void* framebuffer = 0;
static void psp_present(br_pixelmap* src) {
#ifdef ENABLE_RENDERING
    uint8_t* src_pixels = src->pixels;
    // uint32_t* dest_pixels = 0;

    for (unsigned int x = 0; x < 512; x++) {
        for (unsigned int y = 0; y < 272; y++) {
            if (x < src->width && y < src->height) {
                int i = (y * 512) + x;
                uint8_t pixel = src_pixels[(y * src->width) + x];

                uint32_t c = converted_palette[pixel];
                pixels[i] = c;
            }
        }
    }

    sceKernelDcacheWritebackAll();

    sceGuStart(GU_DIRECT, list);

    sceGuCopyImage(GU_PSM_8888, 0, 0, src->width, src->height, 512, pixels, 0, 0, 512,
        (void*)(0x04000000 + (u32)framebuffer));
    sceGuTexSync();

    sceGuFinish();
    sceGuSync(0, 0);

    framebuffer = sceGuSwapBuffers();
#endif
    last_screen_src = src;
}

static void set_palette(PALETTEENTRY_* pal) {
    for (int i = 0; i < 256; i++) {
        // converted_palette[i] = (0xff << 24 | pal[i].peRed << 16 | pal[i].peGreen << 8 | pal[i].peBlue);
        converted_palette[i] = (0xff << 24 | pal[i].peBlue << 16 | pal[i].peGreen << 8 | pal[i].peRed);
    }

    sceGuClutMode(GU_PSM_4444, 0, 255, 0);
    // sceGuClutLoad(256 / 8, converted_palette);

    // sceKernelDcacheWritebackAll();

    if (last_screen_src != NULL) {
        psp_present(last_screen_src);
    }
}

uint64_t psp_get_ticks64(void) {
    if (start_tick == 0) {
        start_tick = PSP_Ticks();
    }

    return (PSP_Ticks() - start_tick) / sceRtcGetTickResolution();
}

static uint32_t psp_get_ticks(void) {
    return (uint32_t)(psp_get_ticks64() & 0xFFFFFFFF);
}

static void psp_sleep(uint32_t ms) {
    // const uint32_t max_delay = 0xFFFFFFFFUL / 1000;
    // if (ms > max_delay) {
    // ms = max_delay;
    // }
    // sceKernelDelayThreadCB(ms * 1000);
    sceKernelDelayThread(ms * 1000);
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