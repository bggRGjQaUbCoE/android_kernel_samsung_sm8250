/*
 * leds-S2MPB02.h - Flash-led driver for Samsung S2MPB02
 *
 * Copyright (C) 2014 Samsung Electronics
 * XXX <xxx@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  This driver is based on leds-max77804.h
 */

#ifndef __LEDS_S2MPB02_H__
#define __LEDS_S2MPB02_H__

#define S2MPB02_FLED_CHANNEL_1  /* GPIOs connected to FLASH_EN1, TORCH_EN1 */
//#define S2MPB02_FLED_CHANNEL_2   /* GPIOs connected to FLASH_EN2, TORCH_EN2 */

#define S2MPB02_FLASH_TORCH_CURRENT_MAX 0xF
#define S2MPB02_TIMEOUT_MAX 0xF

/* S2MPB02_LV_SEL_VOUT */
#define S2MPB02_LV_SEL_VOUT_MASK 0x07
#define S2MPB02_LV_SEL_VOLT(mV)	\
		((mV) <= 2700 ? 0x00 : \
		((mV) <= 3400 ? ((mV) - 2700) / 100 : 0x07))

#define S2MPB02_FLASH_MASK 0xF0
#define S2MPB02_TORCH_MASK 0x0F

#define S2MPB02_FLED_ENABLE 1
#define S2MPB02_FLED_DISABLE 0
#define S2MPB02_FLED_ENABLE_SHIFT 7

#define S2MPB02_FLED_FLASH_MODE 0
#define S2MPB02_FLED_TORCH_MODE 1
#define S2MPB02_FLED_MODE_SHIFT 6

/* S2MPB02_LV_EN */
#define S2MPB02_FLED_CTRL1_LV_EN_MASK 0x08
#define S2MPB02_FLED_CTRL1_LV_ENABLE 1
#define S2MPB02_FLED_CTRL1_LV_DISABLE 0

#define S2MPB02_FLED_ENABLE_MODE_MASK 0xC0
#define S2MPB02_FLED2_MAX_TIME_MASK 0x1F
#define S2MPB02_FLED2_MAX_TIME_CLEAR_MASK 0x04
#define S2MPB02_FLED2_MAX_TIME_EN_MASK 0x01
#define S2MPB02_FLED2_IRON2_MASK 0xC0


enum s2mpb02_led_id {
	S2MPB02_FLASH_LED_1,
	S2MPB02_TORCH_LED_1,
	S2MPB02_LED_MAX,
};

enum s2mpb02_flash_current {
	S2MPB02_FLASH_OUT_I_100MA = 1,
	S2MPB02_FLASH_OUT_I_200MA,
	S2MPB02_FLASH_OUT_I_300MA,
	S2MPB02_FLASH_OUT_I_400MA,
	S2MPB02_FLASH_OUT_I_500MA,
	S2MPB02_FLASH_OUT_I_600MA,
	S2MPB02_FLASH_OUT_I_700MA,
	S2MPB02_FLASH_OUT_I_800MA,
	S2MPB02_FLASH_OUT_I_900MA,
	S2MPB02_FLASH_OUT_I_1000MA,
	S2MPB02_FLASH_OUT_I_1100MA,
	S2MPB02_FLASH_OUT_I_1200MA,
	S2MPB02_FLASH_OUT_I_1300MA,
	S2MPB02_FLASH_OUT_I_1400MA,
	S2MPB02_FLASH_OUT_I_1500MA,
	S2MPB02_FLASH_OUT_I_MAX,
};

enum s2mpb02_torch_current {
	S2MPB02_TORCH_OUT_I_20MA = 1,
	S2MPB02_TORCH_OUT_I_40MA,
	S2MPB02_TORCH_OUT_I_60MA,
	S2MPB02_TORCH_OUT_I_80MA,
	S2MPB02_TORCH_OUT_I_100MA,
	S2MPB02_TORCH_OUT_I_120MA,
	S2MPB02_TORCH_OUT_I_140MA,
	S2MPB02_TORCH_OUT_I_160MA,
	S2MPB02_TORCH_OUT_I_180MA,
	S2MPB02_TORCH_OUT_I_200MA,
	S2MPB02_TORCH_OUT_I_220MA,
	S2MPB02_TORCH_OUT_I_240MA,
	S2MPB02_TORCH_OUT_I_260MA,
	S2MPB02_TORCH_OUT_I_280MA,
	S2MPB02_TORCH_OUT_I_300MA,
	S2MPB02_TORCH_OUT_I_MAX,
};

enum s2mpb02_flash_timeout {
	S2MPB02_FLASH_TIMEOUT_62P5MS,
	S2MPB02_FLASH_TIMEOUT_125MS,
	S2MPB02_FLASH_TIMEOUT_187P5MS,
	S2MPB02_FLASH_TIMEOUT_250MS,
	S2MPB02_FLASH_TIMEOUT_312P5MS,
	S2MPB02_FLASH_TIMEOUT_375MS,
	S2MPB02_FLASH_TIMEOUT_437P5MS,
	S2MPB02_FLASH_TIMEOUT_500MS,
	S2MPB02_FLASH_TIMEOUT_562P5MS,
	S2MPB02_FLASH_TIMEOUT_625MS,
	S2MPB02_FLASH_TIMEOUT_687P5MS,
	S2MPB02_FLASH_TIMEOUT_750MS,
	S2MPB02_FLASH_TIMEOUT_812P5MS,
	S2MPB02_FLASH_TIMEOUT_875MS,
	S2MPB02_FLASH_TIMEOUT_937P5MS,
	S2MPB02_FLASH_TIMEOUT_1000MS,
	S2MPB02_FLASH_TIMEOUT_MAX,
};


enum s2mpb02_torch_timeout {
	S2MPB02_TORCH_TIMEOUT_1S,
	S2MPB02_TORCH_TIMEOUT_2S,
	S2MPB02_TORCH_TIMEOUT_3S,
	S2MPB02_TORCH_TIMEOUT_4S,
	S2MPB02_TORCH_TIMEOUT_5S,
	S2MPB02_TORCH_TIMEOUT_6S,
	S2MPB02_TORCH_TIMEOUT_7S,
	S2MPB02_TORCH_TIMEOUT_8S,
	S2MPB02_TORCH_TIMEOUT_9S,
	S2MPB02_TORCH_TIMEOUT_10S,
	S2MPB02_TORCH_TIMEOUT_11S,
	S2MPB02_TORCH_TIMEOUT_12S,
	S2MPB02_TORCH_TIMEOUT_13S,
	S2MPB02_TORCH_TIMEOUT_14S,
	S2MPB02_TORCH_TIMEOUT_15S,
	S2MPB02_TORCH_TIMEOUT_16S,
	S2MPB02_TORCH_TIMEOUT_MAX,
};

enum s2mpb02_led_turn_way {
	S2MPB02_LED_TURN_WAY_I2C,
	S2MPB02_LED_TURN_WAY_GPIO,
	S2MPB02_LED_TURN_WAY_MAX,
};

struct s2mpb02_led {
	const char *name;
	int id;
	int brightness;
	int timeout;
	const char *default_trigger; /* Trigger to use */
	uint32_t gpio;
};

struct s2mpb02_led_platform_data {
	int num_leds;
	struct s2mpb02_led leds[S2MPB02_LED_MAX];
};

extern int s2mpb02_led_en(int mode, int onoff, enum s2mpb02_led_turn_way turn_way);
#if defined(CONFIG_SAMSUNG_SECURE_CAMERA)
extern int s2mpb02_ir_led_init(void);
extern int s2mpb02_ir_led_current(int32_t current_value);
extern int s2mpb02_ir_led_pulse_width(int32_t width);
extern int s2mpb02_ir_led_pulse_delay(int32_t delay);
extern int s2mpb02_ir_led_max_time(int32_t max_time);
#endif

#endif
