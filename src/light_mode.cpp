/**
 * @file light_mode.cpp
 * @author Farès Chati (faresticha@gmail.com)
 * @date 2023-03-31
 *
 * @brief
 */

#include "light_mode.h"
#include <ArduinoLogger.h>
#include <Adafruit_DotStar.h>
#include "light_strip.h"
#include "Alarms.h" /* TODO: Remove */
#include "wifi_com.h"
#include "Resources.h"
#include "Utils.h"

#define LIGHT_MODE_START_DONE_TAIL_LENGTH LIGHT_STRIP_HALF_LENGTH

const static char *light_mode_strings[LIGHT_MODE_N] = { "Continuous",
	                                                    "Flash",
	                                                    "Strobe",
	                                                    "Fade",
	                                                    "Smooth",
	                                                    "Dawn",
	                                                    "Sunset",
	                                                    "Start" };

struct light_mode_task_queue_data {
	bool                   shutdown;
	uint8_t                mode;
	struct light_mode_data *mode_data;
};

static struct light_mode_callbacks *callbacks;

static TaskHandle_t light_mode_task_handle;
static QueueHandle_t light_mode_task_queue;

static volatile bool light_mode_start_done;

static void light_mode_task_create(uint8_t mode, struct light_mode_data *data);
static void light_mode_shutdown_task_create(void);

static void light_mode_task_delay(uint8_t power, uint32_t delay_ms)
{
	light_mode_task_queue_data queue_data;

	if (xQueueReceive(light_mode_task_queue, &queue_data, delay_ms / portTICK_PERIOD_MS)) {
		/* Message has been sent to the queue */

		if (queue_data.shutdown) {
			light_mode_shutdown_task_create();
		} else {
			light_mode_task_create(queue_data.mode, queue_data.mode_data);
		}

		/* Self delete */
		trace << "Self destructing, handle: " << hex << xTaskGetCurrentTaskHandle() << endl;
		vTaskDelete(NULL);
	} else {
		/* Queue timed out, nothing to do */
	}
}

static void light_mode_task_update_and_delay(uint8_t power, uint32_t delay_ms)
{
	light_strip_update(power);

	light_mode_task_delay(power, delay_ms);
}

/**
 * @brief Task running when the lights are off
 *
 * This periodically sends SPI commands to turn le LEDs off
 * to mitigate potential parasite signals.
 *
 * @param pvParameters Task parameters - Unused
 */
static void light_mode_shutdown_task(void *pvParameters)
{
	light_strip_color_all_set(0);

	while (true) {
		light_mode_task_update_and_delay(0, 5000);
	}
}

static void light_mode_continuous_task(void *pvParameters)
{
	struct light_mode_data *data = (light_mode_data *)pvParameters;

	inf << "Entering Continuous mode" << dendl;

	while (true) {
		light_strip_color_all_set(data->rgb);

		light_mode_task_update_and_delay(data->power, 1000);
	}
}

/* Flash mode */
static void light_mode_flash_task(void *pvParameters)
{
	struct light_mode_data *data = (light_mode_data *)pvParameters;

	inf << "Entering Flash mode" << dendl;

	while (true) {
		light_strip_color_all_set(0xFF0000);

		light_mode_task_update_and_delay(data->power, 1000U - data->speed * 10);

		light_strip_color_all_set(0x00FF00);

		light_mode_task_update_and_delay(data->power, 1000U - data->speed * 10);

		light_strip_color_all_set(0x0000FF);

		light_mode_task_update_and_delay(data->power, 1000U - data->speed * 10);
	}
}

/* Strobe mode */
static void light_mode_strobe_task(void *pvParameters)
{
	struct light_mode_data *data = (light_mode_data *)pvParameters;

	inf << "Entering Strobe mode" << dendl;

	while (true) {
		light_strip_color_all_set(data->rgb);

		light_mode_task_update_and_delay(data->power, 1000U - data->speed * 10);

		light_strip_color_all_set(0);

		light_mode_task_update_and_delay(data->power, 1000U - data->speed * 10);
	}
}

/* Fade Mode */
static void light_mode_fade_task(void *pvParameters)
{
	struct light_mode_data *data = (light_mode_data *)pvParameters;
	uint8_t state                = 0;
	int32_t counter              = 0;

	inf << "Entering Fade mode" << dendl;

	while (true) {
		light_strip_color_all_set(data->rgb * (counter / 255.));

		if (counter >= 255)
			state = 0;
		else if (counter <= 0)
			state = 1;

		if (state)
			counter++;
		else
			counter--;

		light_mode_task_update_and_delay(data->power, (1000U - data->speed * 10) / 50);
	}
}

/* Smooth Mode */
static void light_mode_smooth_task(void *pvParameters)
{
	struct light_mode_data *data = (light_mode_data *)pvParameters;
	uint8_t state                = 0;
	uint8_t red                  = 0xFF;
	uint8_t green                = 0;
	uint8_t blue                 = 0;

	inf << "Entering Smooth mode" << dendl;

	while (true) {
		light_strip_color_all_set(LightRgb(red, green, blue));

		switch (state) {
		case 0: if (++green == 0xFF) state = 1; break;
		case 1: if (--red == 0x00) state = 2; break;
		case 2: if (++blue == 0xFF) state = 3; break;
		case 3: if (--green == 0x00) state = 4; break;
		case 4: if (++red == 0xFF) state = 5; break;
		case 5: if (--blue == 0x00) state = 0; break;
		}

		light_mode_task_update_and_delay(data->power, (1000U - data->speed * 10) / 8);
	}
}

/* Dawn Mode */
static void light_mode_dawn_task(void *pvParameters)
{
	struct light_mode_data *data = (light_mode_data *)pvParameters;
	int16_t counter1             = 0;
	int16_t counter2             = 1;
	uint32_t step                = alarms.getDawnDuration() * 1000.0 / (LIGHT_STRIP_HALF_LENGTH * 255);

	if (step == 0) {
		step = 1;
	}

	inf << "Entering Dawn mode for " << alarms.getDawnDuration() << dendl;

	light_strip_color_all_set(0);

	while (counter2 < 255) {
		light_strip_color_set(LIGHT_STRIP_HALF_LENGTH + counter1, data->rgb * (counter2 / 255.));
		light_strip_color_set(LIGHT_STRIP_HALF_LENGTH - counter1, data->rgb * (counter2 / 255.));

		if (counter1++ >= LIGHT_STRIP_HALF_LENGTH) {
			counter1 = 0;

			counter2++;
		}

		light_mode_task_update_and_delay(data->power, step);
	}

	if (callbacks && callbacks->on_mode_end) {
		callbacks->on_mode_end(LIGHT_MODE_DAWN);
	} else {
		err << "light_mode callback not defined" << endl;
	}

	/* Take the semaphore and start the requested task */
	light_mode_task_update_and_delay(0, 0);
}

/* Sunset Mode */
static void light_mode_sunset_task(void *pvParameters)
{
	struct light_mode_data *data = (light_mode_data *)pvParameters;
	int32_t counter1             = 0;
	int32_t counter2             = 255;

	inf << "Entering Sunset mode for " << alarms.getSunsetDuration() << dendl;

	light_strip_color_all_set(data->rgb);

	light_mode_task_update_and_delay(data->power, alarms.getSunsetDuration().value());

	trace << "Starting to shut down. Completely off in " << alarms.getSunsetDecreaseTime() << dendl;

	while (counter2 > 0) {
		light_strip_color_set(counter1, data->rgb * (counter2 / 255.));
		light_strip_color_set(LIGHT_STRIP_LENGTH - counter1, data->rgb * (counter2 / 255.));

		if (counter1++ >= LIGHT_STRIP_HALF_LENGTH) {
			counter1 = 0;

			counter2--;
		}

		light_mode_task_update_and_delay(data->power, alarms.getSunsetDecreaseTime() * (1000.0 / (LIGHT_STRIP_HALF_LENGTH * 255)));
	}

	if (callbacks && callbacks->on_mode_end) {
		callbacks->on_mode_end(LIGHT_MODE_SUNSET);
	} else {
		err << "light_mode callback not defined" << endl;
	}

	/* Take the semaphore and start the requested task */
	light_mode_task_update_and_delay(0, 0);
}

static void light_mode_start(void *pvParameters)
{
	struct light_mode_data *data = (light_mode_data *)pvParameters;
	int32_t counter              = 0;
	uint8_t state                = 0;
	LightRgb rainbow[LIGHT_STRIP_HALF_LENGTH + 1];

	for (uint8_t i = 0; i < LIGHT_STRIP_HALF_LENGTH + 1; i++)
		rainbow[i].setHue(i * 360 / (LIGHT_STRIP_HALF_LENGTH + 1));

	while (!light_mode_start_done) {
		counter = 0;
		state   = 0;

		while (counter >= 0) {
			for (uint8_t i = 0; i < LIGHT_STRIP_LENGTH; i++) {
				if (abs(i - LIGHT_STRIP_HALF_LENGTH) < counter) {
					light_strip_color_set(i, rainbow[LIGHT_STRIP_HALF_LENGTH - abs(i - LIGHT_STRIP_HALF_LENGTH)]);
				} else {
					light_strip_color_set(i, 0);
				}
			}

			light_mode_task_update_and_delay(data->power, abs(Adafruit_DotStar::sine8(counter * (8 * 16) / LIGHT_STRIP_LENGTH + 64) - 80) / 7);

			if (counter > LIGHT_STRIP_HALF_LENGTH) {
				state = 1;
			} else if (counter < 0) {
				state = 0;
			}

			if (state) {
				counter--;
			} else {
				counter++;
			}
		}
	}

	counter = 0;

	while (counter < LIGHT_STRIP_HALF_LENGTH + LIGHT_MODE_START_DONE_TAIL_LENGTH + 2) {
		for (int8_t i = 0; i < LIGHT_STRIP_LENGTH; i++)
			if (counter > abs(i - LIGHT_STRIP_HALF_LENGTH) && abs(i - LIGHT_STRIP_HALF_LENGTH) >= counter - LIGHT_MODE_START_DONE_TAIL_LENGTH)
				light_strip_color_set(i, rainbow[LIGHT_STRIP_HALF_LENGTH - abs(i - LIGHT_STRIP_HALF_LENGTH)]);
			else
				light_strip_color_set(i, 0);

		light_mode_task_update_and_delay(data->power, 15);

		counter++;
	}

	if (callbacks && callbacks->on_mode_end) {
		callbacks->on_mode_end(LIGHT_MODE_START);
	} else {
		err << "light_mode callback not defined" << endl;
	}

	/* Take the semaphore and start the requested task */
	light_mode_task_update_and_delay(0, 0);
}

static const TaskFunction_t light_mode_tasks[] = { light_mode_continuous_task,
	                                               light_mode_flash_task,
	                                               light_mode_strobe_task,
	                                               light_mode_fade_task,
	                                               light_mode_smooth_task,
	                                               light_mode_dawn_task,
	                                               light_mode_sunset_task,
	                                               light_mode_start };

static void light_mode_task_create(uint8_t mode, struct light_mode_data *data)
{
	if (mode >= ARRAY_SIZE(light_mode_tasks) || !light_mode_tasks[mode]) {
		err << "No handler for mode " << mode << endl;
		return;
	}

	xTaskCreatePinnedToCore(light_mode_tasks[mode], "LightModeTask",
	                        10240, data, 1, &light_mode_task_handle, 0);

	verb << "Light mode task started for " << mode << ", handle: " << hex << (uint32_t)light_mode_task_handle << endl;
}

static void light_mode_shutdown_task_create(void)
{
	xTaskCreatePinnedToCore(light_mode_shutdown_task, "LightModeShutdownTask",
	                        4096, NULL, 1, &light_mode_task_handle, 0);

	verb << "Shutdown task started, handle: " << hex << (uint32_t)light_mode_task_handle << endl;
}

static void on_wifi_com_connected()
{
	light_mode_start_done = true;
}

static const struct wifi_com_conn_callbacks wifi_com_conn_callbacks = {
	.on_connected = on_wifi_com_connected
};

void light_mode_init(void)
{
	light_mode_task_queue = xQueueCreate(1, sizeof(struct light_mode_task_queue_data));

	if (light_mode_task_queue == NULL) {
		err << "Could not create queue!" << endl;
		return; /* TODO : Error handling */
	}

	light_mode_shutdown_task_create();

	wifi_com_register_conn_callbacks((struct wifi_com_conn_callbacks *)&wifi_com_conn_callbacks);
}

void light_mode_start(uint8_t mode, struct light_mode_data *data)
{
	light_mode_task_queue_data queue_data = {
		.shutdown  = false,
		.mode      = mode,
		.mode_data = data
	};

	xQueueSendToBack(light_mode_task_queue, &queue_data, 0);

	verb << "Light mode task creation request sent, mode: " << mode << endl;
}

void light_mode_stop(void)
{
	light_mode_task_queue_data data = {
		.shutdown = true
	};

	xQueueSendToBack(light_mode_task_queue, &data, 0);

	verb << "Light mode task deletion request sent" << endl;
}

void light_mode_register_callbacks(struct light_mode_callbacks *cbks)
{
	callbacks = cbks;
}

const char * light_mode_string_get(uint8_t mode)
{
	if (mode < ARRAY_SIZE(light_mode_strings)) {
		return light_mode_strings[mode];
	} else {
		return "Unknown";
	}
}
