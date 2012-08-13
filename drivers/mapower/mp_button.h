//*****************************************************************************
// file: mp_button.h
// function: button access function for ix1 board
// version: 1.1 (2010/06/28)
// Author: Ken Cheng <kencheng@mapower.com.tw>
//*****************************************************************************
#ifndef __MP_BUTTON_H__
#define __MP_BUTTON_H__

struct button_structure
{
	int buttonid;	// button id
	int state;		// button state
};

enum button_id
{
	WIX_NO_BTN,			// no button
	WIX_BTN_POWER,		// power button
	WIX_BTN_RESET,		// reset button
	WIX_BTN_LCM_SCROLL,	// LCM scroll button
	WIX_BTN_LCM_SELECT,	// LCM Select button (Or Quick Transfer button)
	WIX_BTN_HDD_ALARM	// Over the threadhold of the HDD thermal (Just support the press state)
};

enum button_state
{
	BUTTON_EVENT,		// indicate button press and release
	BUTTON_LONG_EVENT,	// indicate button Long press and release (Only reset button support this state)
	
	BUTTON_PRESSED,		// indicate button is pressed right now
	BUTTON_NOT_PRESSED	// indicate button is not pressed right now
};

#define LONG_TIME	1	// seconds

#define BUTTON_IOC_MAGIC     'k'

// read button state with non-blocking way, return WIX_NO_BTN when no button event happen.
#define READ_BUTTON_NONBLOCKING		_IOR(BUTTON_IOC_MAGIC, 1, int)
// read button state until button event happens
#define READ_BUTTON_BLOCKING		_IOR(BUTTON_IOC_MAGIC, 2, int)

// read power button state if pressed or not pressed
#define READ_POWER_BTN_STATUS		_IOR(BUTTON_IOC_MAGIC, 3, int)
// read reset button state if pressed or not pressed
#define READ_RESET_BTN_STATUS		_IOR(BUTTON_IOC_MAGIC, 4, int)
// read otb button state if pressed or not pressed
#define READ_LCM_SELECT_BTN_STATUS	_IOR(BUTTON_IOC_MAGIC, 5, int)

#define BUTTON_IOC_MAXNR     5

#endif	// __MP_BUTTON_H__
