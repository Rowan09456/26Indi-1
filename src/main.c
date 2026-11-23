#include <stdio.h>
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "gamepad_HID.h"

int main()
{
    stdio_init_all();
    sleep_ms(2000);

    gamepad_init();

    while (true) {
        //printf("Hello, world!\n");
        //sleep_ms(1000);

        tight_loop_contents();
    }
    return 0;
}
