#include <stdio.h>

#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspkernel.h>

PSP_MODULE_INFO("DETHRACE", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_VFPU | THREAD_ATTR_USER);

void resolve_full_path(char* path, const char* argv0) {
    return;
}

void OS_InstallSignalHandler(char* program_name) {
    return;
}

FILE* OS_fopen(const char* pathname, const char* mode) {
    FILE* f = fopen(pathname, mode);
    if (f != NULL) {
        return f;
    }
    return NULL;
}

size_t OS_ConsoleReadPassword(char* pBuffer, size_t pBufferLen) {
    return 0;
}

char* OS_Basename(const char* path) {
    return "CARMDEMO";
}

char* OS_GetWorkingDirectory(char* argv0) {
    return "CARMDEMO";
}