#include <stdbool.h>

#define LED_ERROR 0
#define LED_SEARCHING  1
#define LED_LOGGING  2
#define LED_UPLOADING 3
#define LED_BOOTING 4


void set_led_status(int status);
int get_led_status(void);


