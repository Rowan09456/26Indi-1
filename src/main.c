#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#import "hog_gamepad.h"

int main()
{
    cyw43_init(&cyw43_state);
    if (cyw43_arch_init()) {
        printf("cyw43 init failed\n");
        return -1;
    }

    //cyw43_wifi_pm(&cyw43_state, CYW43_NONE_PM);                      // set to address bluetooth flakiness

    stdio_init_all();
    sleep_ms(10000);

    printf("Initializing Bluetooth connection\n");

    sleep_ms(2000);

    gamepad_init();

    while (true) {
        //printf("Hello, world!\n");
        cyw43_arch_poll();

        tight_loop_contents();
    }
    return 0;
}
