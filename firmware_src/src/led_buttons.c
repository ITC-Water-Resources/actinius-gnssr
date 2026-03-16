
/* LED and button stuff */
#include "led_buttons.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(GNSSR,CONFIG_GNSSR_LOG_LEVEL);

#define STACKSIZE 1024
#define PRIORITY 7
#define KTHREADDELAY 1000

#define GPIO_NODE 		DT_NODELABEL(gpio0)
#define BUTTON_NODE    		DT_ALIAS(sw0)

#define RED_LED_NODE		DT_ALIAS(led0)
#define GREEN_LED_NODE		DT_ALIAS(led1)
#define BLUE_LED_NODE		DT_ALIAS(led2)

#define LED_ON 1
#define LED_OFF !LED_ON

#ifdef CONFIG_ADC
/* Include adc drivers for battery voltage measurement */
#include <zephyr/drivers/adc.h>
#define BATVOLT_R1 4.7f                 // MOhm
#define BATVOLT_R2 10.0f                // MOhm
#define INPUT_VOLT_RANGE 3.6f           // Volts
#define VALUE_RANGE_10_BIT 1.023        // (2^10 - 1) / 1000

#define ADC_NODE DT_NODELABEL(adc)

#define ADC_RESOLUTION 10
#define ADC_GAIN ADC_GAIN_1_6
#define ADC_REFERENCE ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 10)
#define ADC_1ST_CHANNEL_ID 0
#define ADC_1ST_CHANNEL_INPUT SAADC_CH_PSELP_PSELP_AnalogInput0
#define BUFFER_SIZE 1
#endif 






static struct device *gpio_dev;
static struct gpio_callback gpio_cb;

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

static struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(RED_LED_NODE, gpios);
static struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(GREEN_LED_NODE, gpios);
static struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(BLUE_LED_NODE, gpios);

static int led_status= LED_BOOTING;
extern struct k_sem rollover_event_sem;


void button_pressed_callback(const struct device *gpiob, struct gpio_callback *cb, gpio_port_pins_t pins)
{
	/* pressing the button induces a rollover_event */
	k_sem_give(&rollover_event_sem);
}

bool init_button(void)
{
	int ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to configure %s pin %d\n",
				ret, button.port->name, button.pin);
		
		return false;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d\n",
			   ret, button.port->name, button.pin);

		return false;
	}

	gpio_init_callback(&gpio_cb, button_pressed_callback, BIT(button.pin));
	gpio_add_callback(button.port, &gpio_cb);

	return true;
}



void set_led_status(int status){
	/*only set it when it is something else) */
	if (led_status != status){
		led_status=status;
	}
}

int get_led_status(void){
	return led_status;
}

void turn_leds_off(void)
{
	gpio_pin_set_dt(&red_led, LED_OFF);
	gpio_pin_set_dt(&green_led, LED_OFF);
	gpio_pin_set_dt(&blue_led, LED_OFF);
}

void init_leds(void)
{
	gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&blue_led, GPIO_OUTPUT_INACTIVE);
}


int led_button_checker(void){
	gpio_dev = DEVICE_DT_GET(GPIO_NODE);

	if (!gpio_dev) {
		LOG_ERR("Error getting GPIO device binding\r\n");

		return -1;
	}

	if (!init_button()) {
		return -1;
	}

	init_leds();


	while(1){
		switch (led_status){
		case LED_SEARCHING:
			/*blinking yellow for a second every 5 seconds*/	
			gpio_pin_set_dt(&red_led, LED_ON);
			gpio_pin_set_dt(&green_led, LED_ON);
			k_sleep(K_MSEC(1000));
			turn_leds_off();
			k_sleep(K_MSEC(4000));
			break;
		case LED_LOGGING:
			/*flash green for a 10th of a second second every 10 seconds*/	
			gpio_pin_set_dt(&green_led, LED_ON);
			k_sleep(K_MSEC(100));
			turn_leds_off();
			k_sleep(K_MSEC(14900));
			break;
		case LED_ERROR:
			/*blinking red a second every 10 seconds*/	
			gpio_pin_set_dt(&red_led, LED_ON);
			k_sleep(K_MSEC(1000));
			turn_leds_off();
			k_sleep(K_MSEC(9000));
			break;
		case LED_UPLOADING:
			/*blinking blue a second every 5 seconds*/	
			gpio_pin_set_dt(&blue_led, LED_ON);
			k_sleep(K_MSEC(1000));
			turn_leds_off();
			k_sleep(K_MSEC(4000));
			break;
		case LED_BOOTING:
			/*light up green  on of at 1 sec pulse*/	
			gpio_pin_set_dt(&green_led, LED_ON);
			k_sleep(K_MSEC(1000));
			turn_leds_off();
			k_sleep(K_MSEC(1000));
			break;
		default:
			/* shouldn't occur really, but allow sleep so this function does not spin */
			k_sleep(K_MSEC(1000));
			break;
		}

	}
	return 0;

}


/*Battery voltage measurement*/

#ifdef CONFIG_ADC
static int16_t m_sample_buffer[BUFFER_SIZE];

static const struct device *adc_dev;

static const struct adc_channel_cfg m_1st_channel_cfg = {
	.gain = ADC_GAIN,
	.reference = ADC_REFERENCE,
	.acquisition_time = ADC_ACQUISITION_TIME,
	.channel_id = ADC_1ST_CHANNEL_ID,
	.input_positive   = ADC_1ST_CHANNEL_INPUT,
};

int get_battery_voltage(uint16_t *battery_voltage)
{
	int err;

	const struct adc_sequence sequence = {
		.channels = BIT(ADC_1ST_CHANNEL_ID),
		.buffer = m_sample_buffer,
		.buffer_size = sizeof(m_sample_buffer),
		.resolution = ADC_RESOLUTION,
	};

	if (!adc_dev) {
		return -1;
	}

	err = adc_read(adc_dev, &sequence);
	if (err) {
		printk("ADC read err: %d\n", err);
		return err;
	}

	float sample_value = 0;

	for (int i = 0; i < BUFFER_SIZE; i++) {
		sample_value += (float)m_sample_buffer[i];
	}
	sample_value /= BUFFER_SIZE;

	*battery_voltage = (uint16_t)(sample_value *
		(INPUT_VOLT_RANGE / VALUE_RANGE_10_BIT) *
		((BATVOLT_R1 + BATVOLT_R2) / BATVOLT_R2));

	return 0;
}

bool init_adc(void)
{
	int err;

	adc_dev = DEVICE_DT_GET(ADC_NODE);
	if (!device_is_ready(adc_dev)) {
		printk("Error: ADC device not ready\n");
		return false;
	}

	err = adc_channel_setup(adc_dev, &m_1st_channel_cfg);
	if (err) {
		printk("Error in ADC setup: %d\n", err);
		return false;
	}

	return true;
}



#endif


K_THREAD_DEFINE(led_button_checker_id, STACKSIZE, led_button_checker, NULL, NULL, NULL, PRIORITY, 0, KTHREADDELAY);

