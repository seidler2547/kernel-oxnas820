//*****************************************************************************
// file: mp_led.h
// function: LED access function for ix1 board
// version: 1.0 (2010/04/12)
// Author: Ken Cheng <kencheng@mapower.com.tw>
//*****************************************************************************
#ifndef __MP_LED_H__
#define __MP_LED_H__

#define GPIO_MAX        32

#define LED_POWER		7	// A7
#define LED_SYSTEM_NG		8	// A8
#define HDD1_LED_BLUE		11	// A11
#define LED_SYSTEM_OK		25	// A25
#define OTB_LED_BLUE		28	// A28

#define GPIO_VAL(gpio)  (0x1 << (gpio))

enum MP_LED_IDX {
	IX1_POWER_LED = 0,
	IX1_SYS_NG_LED,
	IX1_HDD1_LED,
	IX1_SYS_OK_LED,
	IX1_OTB_LED,
	MP_LED_IDX_MAX
};

#define LED_LEVEL_LOW           0
#define LED_LEVEL_MED           1
#define LED_LEVEL_HIGH          2

/* led Control range 0~255 */
/* Low level */
#define PWR_WHITE_LED_LOW       1
#define SYSTEM_RED_LED_LOW      1
#define HDD1_BLUE_LED_LOW       1
#define SYSTEM_WHITE_LED_LOW    1
#define BACKUP_BLUE_LED_LOW     1


/* Medium level */
#define PWR_WHITE_LED_MED       21
#define SYSTEM_RED_LED_MED      26
#define HDD1_BLUE_LED_MED       21
#define SYSTEM_WHITE_LED_MED    21
#define BACKUP_BLUE_MED         10


/* High level */
#define PWR_WHITE_LED_HIGH      160
#define SYSTEM_RED_LED_HIGH     170
#define HDD1_BLUE_LED_HIGH      150
#define SYSTEM_WHITE_LED_HIGH   160
#define BACKUP_BLUE_HIGH        125


#define LED_BLINK_RATE_LOW     (HZ)
#define LED_BLINK_RATE_MED     (HZ / 2)
#define LED_BLINK_RATE_HIGH    (HZ / 4)

#endif	// __MP_LED_H__
