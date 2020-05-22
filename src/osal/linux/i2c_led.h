// i2c_led.h
// manage LED on i2c bus
// 

#ifndef i2c_led_h_included
#define i2c_led_h_included

#include <stdbool.h>
#include <stdint.h>

int open_led();
void close_led(int fd);
void set_led(int fd, uint16_t id, bool on);

#endif //  i2c_led_h_included


