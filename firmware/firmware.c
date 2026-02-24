#include "pico/stdlib.h"
#include "tusb.h"

#define LED_PIN PICO_DEFAULT_LED_PIN

static bool led_state = false;

static uint8_t rx_buf[64];

void tud_mount_cb(void) {}
void tud_unmount_cb(void) {}

void tud_suspend_cb(bool remote_wakeup_en) {
  (void) remote_wakeup_en;
}

void tud_resume_cb(void) {}

// Called when data received
void tud_vendor_rx_cb(uint8_t itf, uint8_t const* buffer, uint16_t bufsize)
{
    (void) itf;

    if (bufsize)
    {
        led_state = !led_state;
        gpio_put(LED_PIN, led_state);

        // drain the OUT FIFO
        uint8_t tmp[64];
        tud_vendor_read(tmp, bufsize);

        // echo back
        tud_vendor_write(tmp, bufsize);
        tud_vendor_flush();
    }
}

int main(void) {
  stdio_init_all();

  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  gpio_put(LED_PIN, 0);

  tusb_init();

  while (true) {
    tud_task();
  }
}