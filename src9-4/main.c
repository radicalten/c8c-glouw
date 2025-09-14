#include "chip.h"
#include "gfx.h"

int main(int nargs, char **args) {
    if (nargs != 2) {
        printf("Usage: c8 [ROM]\n");
        exit(0);
    }

    C8State *vm = vm_start();
    renderer_init();

    /* load rom */
    vm_load(vm, args[1]);

    while (renderer_step(vm));

    renderer_close();

    return 0;
}