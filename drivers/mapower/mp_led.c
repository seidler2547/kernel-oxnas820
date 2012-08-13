//*****************************************************************************
// file: mp_led.c
// function: LED access function for ix1 board
// version: 1.0 (2010/04/12)
// Author: Ken Cheng <kencheng@mapower.com.tw>
//*****************************************************************************
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/device.h>

#include <linux/io.h>
#include <mach/hardware.h>

#include "mp_led.h"

//*****************************************************************************
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mapower LED Module for IX1");
MODULE_AUTHOR("Ken Cheng <kencheng@mapower.com.tw>");

//*****************************************************************************
#define PWM_VALUE_A7       (GPIO_A_BASE + 0xB8)
#define PWM_RR_A7          (GPIO_A_BASE + 0xBC)
#define PWM_VALUE_A8       (GPIO_A_BASE + 0xC0)
#define PWM_RR_A8          (GPIO_A_BASE + 0xC4)
#define PWM_VALUE_A9       (GPIO_A_BASE + 0xC8)
#define PWM_RR_A9          (GPIO_A_BASE + 0xCC)
#define PWM_VALUE_A11      (GPIO_A_BASE + 0xD8)
#define PWM_RR_A11         (GPIO_A_BASE + 0xDC)
#define PWM_VALUE_A25      (GPIO_A_BASE + 0x148)
#define PWM_RR_A25         (GPIO_A_BASE + 0x14C)
#define PWM_VALUE_A26      (GPIO_A_BASE + 0x150)
#define PWM_RR_A26         (GPIO_A_BASE + 0x154)
#define PWM_VALUE_A28      (GPIO_A_BASE + 0x160)
#define PWM_RR_A28         (GPIO_A_BASE + 0x164)

#define PWM_VALUE_B7       (GPIO_B_BASE + 0xB8)
#define PWM_RR_B7          (GPIO_B_BASE + 0xBC)
#define PWM_VALUE_B8       (GPIO_B_BASE + 0xC0)
#define PWM_RR_B8          (GPIO_B_BASE + 0xC4)
#define PWM_VALUE_B9       (GPIO_B_BASE + 0xC8)
#define PWM_RR_B9          (GPIO_B_BASE + 0xCC)
#define PWM_VALUE_B11      (GPIO_B_BASE + 0xD8)
#define PWM_RR_B11         (GPIO_B_BASE + 0xDC)
#define PWM_VALUE_B25      (GPIO_B_BASE + 0x148)
#define PWM_RR_B25         (GPIO_B_BASE + 0x14C)
#define PWM_VALUE_B26      (GPIO_B_BASE + 0x150)
#define PWM_RR_B26         (GPIO_B_BASE + 0x154)
#define PWM_VALUE_B28      (GPIO_B_BASE + 0x160)
#define PWM_RR_B28         (GPIO_B_BASE + 0x164)

//*****************************************************************************
#define DEVNAME "nasled"

//*****************************************************************************
extern spinlock_t oxnas_gpio_spinlock;

static int mp_led_map[MP_LED_IDX_MAX] = {
	LED_POWER,
	LED_SYSTEM_NG,
	HDD1_LED_BLUE,
	LED_SYSTEM_OK,
	OTB_LED_BLUE
};

struct led_action
{
	int	led_state;
	int	led_device;
};

static struct timer_list led_timer;

struct led_data
{
	struct led_action mp_led_action[MP_LED_IDX_MAX];
	int brightness[MP_LED_IDX_MAX];
	int brightness_level ;
	int blink_rate;
};

static struct led_data mp_led_data;
static int power_off_state ;
//*****************************************************************************
u32 pwm_value_reg(u32 gpio)
{
	if(gpio >= GPIO_MAX)
	{
		switch(gpio-GPIO_MAX)
		{
			case 7:
				return PWM_VALUE_B7;
				break;
			case 8:
				return PWM_VALUE_B8;
				break;
			case 9:
				return PWM_VALUE_B9;
				break;
			case 11:
				return PWM_VALUE_B11;
				break;
			case 25:
				return PWM_VALUE_B25;
				break;
			case 26:
				return PWM_VALUE_B26;
				break;
			case 28:
				return PWM_VALUE_B28;
				break;
		}
	}
	else
	{
		switch(gpio)
		{
			case 7:
				return PWM_VALUE_A7;
				break;
			case 8:
				return PWM_VALUE_A8;
				break;
			case 9:
				return PWM_VALUE_A9;
				break;
			case 11:
				return PWM_VALUE_A11;
				break;
			case 25:
				return PWM_VALUE_A25;
				break;
			case 26:
				return PWM_VALUE_A26;
				break;
			case 28:
				return PWM_VALUE_A28;
				break;
		}
	}
	return 0;
}

//*****************************************************************************
u32 pwm_rr_reg(u32 gpio)
{
	if(gpio >= GPIO_MAX)
	{
		switch(gpio-GPIO_MAX)
		{
			case 7:
				return PWM_RR_B7;
				break;
			case 8:
				return PWM_RR_B8;
				break;
			case 9:
				return PWM_RR_B9;
				break;
			case 11:
				return PWM_RR_B11;
				break;
			case 25:
				return PWM_RR_B25;
				break;
			case 26:
				return PWM_RR_B26;
				break;
			case 28:
				return PWM_RR_B28;
				break;
		}
	}
	else
	{
		switch(gpio)
		{
			case 7:
				return PWM_RR_A7;
				break;
			case 8:
				return PWM_RR_A8;
				break;
			case 9:
				return PWM_RR_A9;
				break;
			case 11:
				return PWM_RR_A11;
				break;
			case 25:
				return PWM_RR_A25;
				break;
			case 26:
				return PWM_RR_A26;
				break;
			case 28:
				return PWM_RR_A28;
				break;
		}
	}
	return 0;
}

//*****************************************************************************
void set_led(u32 gpio_no, char val)
{
	switch(val)
	{
		case 0:
			mp_led_data.mp_led_action[gpio_no].led_state = val;
			writel(0, pwm_value_reg(mp_led_map[gpio_no]));
			writel(0, pwm_rr_reg(mp_led_map[gpio_no]));
			break;
		case 1:
			mp_led_data.mp_led_action[gpio_no].led_state = val;
			writel(mp_led_data.brightness[gpio_no], pwm_value_reg(mp_led_map[gpio_no]));
			writel(0, pwm_rr_reg(mp_led_map[gpio_no]));
			break;
		case 2:
			mp_led_data.mp_led_action[gpio_no].led_state = val;
			break;
	}
}

//*****************************************************************************
int get_led(u32 gpio_no)
{
	return mp_led_data.mp_led_action[gpio_no].led_state;
}

void init_brightness(char val)
{
    mp_led_data.brightness_level = val ;
    switch(val)
	{
		case 0:
			mp_led_data.brightness[IX1_POWER_LED]   = PWR_WHITE_LED_LOW ;
			mp_led_data.brightness[IX1_SYS_NG_LED]  = SYSTEM_RED_LED_LOW ;
			mp_led_data.brightness[IX1_HDD1_LED]    = HDD1_BLUE_LED_LOW ;
			mp_led_data.brightness[IX1_SYS_OK_LED]  = SYSTEM_WHITE_LED_LOW ;
			mp_led_data.brightness[IX1_OTB_LED]     = BACKUP_BLUE_LED_LOW ;
			break;
		case 1:
		    mp_led_data.brightness[IX1_POWER_LED]   = PWR_WHITE_LED_MED ;
			mp_led_data.brightness[IX1_SYS_NG_LED]  = SYSTEM_RED_LED_MED ;
			mp_led_data.brightness[IX1_HDD1_LED]    = HDD1_BLUE_LED_MED ;
			mp_led_data.brightness[IX1_SYS_OK_LED]  = SYSTEM_WHITE_LED_MED ;
			mp_led_data.brightness[IX1_OTB_LED]     = BACKUP_BLUE_MED ;
			break;
		case 2:
		    mp_led_data.brightness[IX1_POWER_LED]   = PWR_WHITE_LED_HIGH ;
			mp_led_data.brightness[IX1_SYS_NG_LED]  = SYSTEM_RED_LED_HIGH ;
			mp_led_data.brightness[IX1_HDD1_LED]    = HDD1_BLUE_LED_HIGH ;
			mp_led_data.brightness[IX1_SYS_OK_LED]  = SYSTEM_WHITE_LED_HIGH ;
			mp_led_data.brightness[IX1_OTB_LED]     = BACKUP_BLUE_HIGH ;
			break;
	}
}

//*****************************************************************************
void set_brightness(char val)
{
	int set = 0;
	int i;
	mp_led_data.brightness_level = val ;
	
	switch(val)
	{
		case 0:
			mp_led_data.brightness[IX1_POWER_LED]   = PWR_WHITE_LED_LOW ;
			mp_led_data.brightness[IX1_SYS_NG_LED]  = SYSTEM_RED_LED_LOW ;
			mp_led_data.brightness[IX1_HDD1_LED]    = HDD1_BLUE_LED_LOW ;
			mp_led_data.brightness[IX1_SYS_OK_LED]  = SYSTEM_WHITE_LED_LOW ;
			mp_led_data.brightness[IX1_OTB_LED]     = BACKUP_BLUE_LED_LOW ;
			set = 1;
			break;
		case 1:
		    mp_led_data.brightness[IX1_POWER_LED]   = PWR_WHITE_LED_MED ;
			mp_led_data.brightness[IX1_SYS_NG_LED]  = SYSTEM_RED_LED_MED ;
			mp_led_data.brightness[IX1_HDD1_LED]    = HDD1_BLUE_LED_MED ;
			mp_led_data.brightness[IX1_SYS_OK_LED]  = SYSTEM_WHITE_LED_MED ;
			mp_led_data.brightness[IX1_OTB_LED]     = BACKUP_BLUE_MED ;
			set = 1;
			break;
		case 2:
		    mp_led_data.brightness[IX1_POWER_LED]   = PWR_WHITE_LED_HIGH ;
			mp_led_data.brightness[IX1_SYS_NG_LED]  = SYSTEM_RED_LED_HIGH ;
			mp_led_data.brightness[IX1_HDD1_LED]    = HDD1_BLUE_LED_HIGH ;
			mp_led_data.brightness[IX1_SYS_OK_LED]  = SYSTEM_WHITE_LED_HIGH ;
			mp_led_data.brightness[IX1_OTB_LED]     = BACKUP_BLUE_HIGH ;
			set = 1;
			break;
	}
	
	if(set == 1)
	{
		for(i=0; i < MP_LED_IDX_MAX; i++)
		{
			if(mp_led_data.mp_led_action[i].led_state == 1)
			{
				writel(mp_led_data.brightness[i], pwm_value_reg(mp_led_map[i]));
				writel(0, pwm_rr_reg(mp_led_map[i]));
			}
		}
	}
}

//*****************************************************************************
int get_brightness(void)
{
    return mp_led_data.brightness_level ;
}

//*****************************************************************************
void set_blink_rate(char val)
{
	int set = 0;
	switch(val)
	{
		case 0:
			mp_led_data.blink_rate = LED_BLINK_RATE_LOW;
			set = 1;
			break;
		case 1:
			mp_led_data.blink_rate = LED_BLINK_RATE_MED;
			set = 1;
			break;
		case 2:
			mp_led_data.blink_rate = LED_BLINK_RATE_HIGH;
			set = 1;
			break;
	}

	if(set == 1)
	{
		del_timer(&led_timer);
		led_timer.expires = jiffies + mp_led_data.blink_rate;
		add_timer(&led_timer);
	}
}

//*****************************************************************************
int get_blink_rate(void)
{
	switch(mp_led_data.blink_rate)
	{
		case LED_BLINK_RATE_LOW:
			return 0;
		case LED_BLINK_RATE_MED:
			return 1;
		case LED_BLINK_RATE_HIGH:
			return 2;
	}
	return -1;
}

// **************************************************************************************
static void mp_led_change(unsigned long data)
{
	static int bright_state = 0;
	int i;
	
	spin_lock(&oxnas_gpio_spinlock);

	if(bright_state == 0)
	{
		for(i=0; i < MP_LED_IDX_MAX; i++)
		{
			if(mp_led_data.mp_led_action[i].led_state == 2)
			{
				writel(0, pwm_value_reg(mp_led_map[i]));
				writel(0, pwm_rr_reg(mp_led_map[i]));
			}
		}
		bright_state = 1;
	}
	else
	{
		for(i=0; i < MP_LED_IDX_MAX; i++)
		{
			if(mp_led_data.mp_led_action[i].led_state == 2)
			{
				writel(mp_led_data.brightness[i], pwm_value_reg(mp_led_map[i]));
				writel(0, pwm_rr_reg(mp_led_map[i]));
			}
		}
		bright_state = 0;
	}
	
	led_timer.expires = jiffies + mp_led_data.blink_rate;
	add_timer(&led_timer);

	spin_unlock(&oxnas_gpio_spinlock);
}

//*****************************************************************************
void init_gpio_register(void)
{
	int i;
	unsigned long flags;
	u32 led_a_mask = 0;
	u32 led_b_mask = 0;

	for(i=0; i < MP_LED_IDX_MAX; i++)
	{
		if(mp_led_map[i] >= GPIO_MAX)
			led_b_mask |= GPIO_VAL(mp_led_map[i]-GPIO_MAX);
		else
			led_a_mask |= GPIO_VAL(mp_led_map[i]);
	}
	
	spin_lock_irqsave(&oxnas_gpio_spinlock, flags);

    writel(readl(SYS_CTRL_SECONDARY_SEL)   & ~led_a_mask, SYS_CTRL_SECONDARY_SEL);
    writel(readl(SYS_CTRL_TERTIARY_SEL)    & ~led_a_mask, SYS_CTRL_TERTIARY_SEL);
    writel(readl(SYS_CTRL_QUATERNARY_SEL)  & ~led_a_mask, SYS_CTRL_QUATERNARY_SEL);
    writel(readl(SYS_CTRL_DEBUG_SEL)       & ~led_a_mask, SYS_CTRL_DEBUG_SEL);
    writel(readl(SYS_CTRL_ALTERNATIVE_SEL) & ~led_a_mask, SYS_CTRL_ALTERNATIVE_SEL);
	writel(led_a_mask, GPIO_A_OUTPUT_ENABLE_SET);

    writel(readl(SEC_CTRL_SECONDARY_SEL)   & ~led_b_mask, SEC_CTRL_SECONDARY_SEL);
    writel(readl(SEC_CTRL_TERTIARY_SEL)    & ~led_b_mask, SEC_CTRL_TERTIARY_SEL);
    writel(readl(SEC_CTRL_QUATERNARY_SEL)  & ~led_b_mask, SEC_CTRL_QUATERNARY_SEL);
    writel(readl(SEC_CTRL_DEBUG_SEL)       & ~led_b_mask, SEC_CTRL_DEBUG_SEL);
    writel(readl(SEC_CTRL_ALTERNATIVE_SEL) & ~led_b_mask, SEC_CTRL_ALTERNATIVE_SEL);
	writel(led_b_mask, GPIO_B_OUTPUT_ENABLE_SET);

	spin_unlock_irqrestore(&oxnas_gpio_spinlock, flags);
}

//*****************************************************************************
void init_mp_led_data(void)
{
	int i;

	for (i=0; i < MP_LED_IDX_MAX; i++) 
	{
		mp_led_data.mp_led_action[i].led_state = 0;
		mp_led_data.mp_led_action[i].led_device = mp_led_map[i];
	}
	
	power_off_state = 0 ;
	
	init_brightness(LED_LEVEL_MED) ;        // initial led level
	mp_led_data.blink_rate = LED_BLINK_RATE_HIGH;

	init_timer(&led_timer);
	
	led_timer.function = mp_led_change;
	led_timer.data = (unsigned long)NULL;
	led_timer.expires = jiffies + mp_led_data.blink_rate;
	
	add_timer(&led_timer);
}

//*****************************************************************************
static ssize_t plx_led_brightness_show(struct class *class, char *buf)
{
	return sprintf(buf, "%d\n", get_brightness());
}

//*****************************************************************************
static ssize_t plx_brightness_store(struct class *class, const char *buf, size_t size)
{
	int data;
	ssize_t result;
	
	result = sscanf(buf, "%d", &data);
	
	if (result != 1)
		return -EINVAL;
	
	set_brightness(data);
	
	return size;
}

//*****************************************************************************
static ssize_t plx_led_blink_rate_show(struct class *class, char *buf)
{
	return sprintf(buf, "%d\n", get_blink_rate());
}

//*****************************************************************************
static ssize_t plx_blink_rate_store(struct class *class, const char *buf, size_t size)
{
	int data;
	ssize_t result;
	
	result = sscanf(buf, "%d", &data);
	
	if (result != 1)
		return -EINVAL;
	
	set_blink_rate(data);
	
	return size;
}

//*****************************************************************************
static ssize_t mp_led_power_show(struct class *class, char *buf)
{
	return sprintf(buf, "%d\n", get_led(IX1_POWER_LED));
}

//*****************************************************************************
static ssize_t mp_led_power_store(struct class *class, const char *buf, size_t size)
{
	int data;
	ssize_t result;
	
	result = sscanf(buf, "%d", &data);
	
	if (result != 1)
		return -EINVAL;
	
	set_led(IX1_POWER_LED, data);
	
	return size;
}

//*****************************************************************************
static ssize_t mp_led_sys_ng_show(struct class *class, char *buf)
{
	return sprintf(buf, "%d\n", get_led(IX1_SYS_NG_LED));
}

//*****************************************************************************
static ssize_t mp_led_sys_ng_store(struct class *class, const char *buf, size_t size)
{
	int data;
	ssize_t result;
	
	result = sscanf(buf, "%d", &data);
	
	if (result != 1)
		return -EINVAL;
	
	set_led(IX1_SYS_NG_LED, data);
	
	return size;
}

//*****************************************************************************
static ssize_t mp_led_sys_ok_show(struct class *class, char *buf)
{
	return sprintf(buf, "%d\n", get_led(IX1_SYS_OK_LED));
}

//*****************************************************************************
static ssize_t mp_led_sys_ok_store(struct class *class, const char *buf, size_t size)
{
	int data;
	ssize_t result;
	
	result = sscanf(buf, "%d", &data);
	
	if (result != 1)
		return -EINVAL;
	
	set_led(IX1_SYS_OK_LED, data);
	
	return size;
}

//*****************************************************************************
static ssize_t mp_led_hdd1_show(struct class *class, char *buf)
{
	return sprintf(buf, "%d\n", get_led(IX1_HDD1_LED));
}

//*****************************************************************************
static ssize_t mp_led_hdd1_store(struct class *class, const char *buf, size_t size)
{
	int data;
	ssize_t result;
	
	result = sscanf(buf, "%d", &data);
	
	if (result != 1)
		return -EINVAL;
	
	set_led(IX1_HDD1_LED, data);
	
	return size;
}

//*****************************************************************************
static ssize_t mp_led_otb_show(struct class *class, char *buf)
{
	return sprintf(buf, "%d\n", get_led(IX1_OTB_LED));
}

//*****************************************************************************
static ssize_t mp_led_otb_store(struct class *class, const char *buf, size_t size)
{
	int data;
	ssize_t result;
	
	result = sscanf(buf, "%d", &data);
	
	if (result != 1)
		return -EINVAL;
	
	set_led(IX1_OTB_LED, data);
	
	return size;
}

//*****************************************************************************
static ssize_t mp_power_off_show(struct class *class, char *buf)
{
	return sprintf(buf, "%d\n", power_off_state);
}

//*****************************************************************************
static ssize_t mp_power_off_store(struct class *class, const char *buf, size_t size)
{
	int data;
	ssize_t result;
	int gpio_power_off_mask = 1 << 16 ;
	result = sscanf(buf, "%d", &data);
	if (result != 1)
		return -EINVAL;    
    if(1 == data)
    {
        writel(gpio_power_off_mask, GPIO_B_OUTPUT_CLEAR);
        writel(gpio_power_off_mask, GPIO_B_OUTPUT_ENABLE_SET);
    }
    power_off_state = data ;
	return size;
}

//*****************************************************************************
struct class *mp_led_class;

static struct class_attribute mp_led_brightness =
	__ATTR(led_brightness, S_IRUGO | S_IWUGO, plx_led_brightness_show, plx_brightness_store);
	
static struct class_attribute mp_led_blink_rate =
	__ATTR(led_blinkrate, S_IRUGO | S_IWUGO, plx_led_blink_rate_show, plx_blink_rate_store);

static struct class_attribute mp_led_power =
	__ATTR(pwr_white_led, S_IRUGO | S_IWUGO, mp_led_power_show, mp_led_power_store);
	
static struct class_attribute mp_led_sys_ng =
	__ATTR(system_red_led, S_IRUGO | S_IWUGO, mp_led_sys_ng_show, mp_led_sys_ng_store);
	
static struct class_attribute mp_led_sys_ok =
	__ATTR(system_white_led, S_IRUGO | S_IWUGO, mp_led_sys_ok_show, mp_led_sys_ok_store);
	
static struct class_attribute mp_led_hdd1 =
	__ATTR(hdd1_blue_led, S_IRUGO | S_IWUGO, mp_led_hdd1_show, mp_led_hdd1_store);
	
static struct class_attribute mp_led_otb =
	__ATTR(backup_blue_led, S_IRUGO | S_IWUGO, mp_led_otb_show, mp_led_otb_store);

static struct class_attribute mp_power_off =
	__ATTR(power_off, S_IRUGO | S_IWUGO, mp_power_off_show, mp_power_off_store);
	
//*****************************************************************************
int init_sys_fs(void)
{
	int err;
	
	mp_led_class = class_create(THIS_MODULE, DEVNAME);
	if (IS_ERR(mp_led_class)) {
		err = PTR_ERR(mp_led_class);
		printk(KERN_ALERT "mp_led: cannot create mp_led class");
		goto out_class;
	}

	err = class_create_file(mp_led_class, &mp_led_brightness);
	if (err) {
		printk(KERN_ALERT "mp_led: cannot create sysfs file, mp_led_brightness\n");
		goto out_mp_led_brightness;
	}

	err = class_create_file(mp_led_class, &mp_led_blink_rate);
	if (err) {
		printk(KERN_ALERT "mp_led: cannot create sysfs file, mp_led_blink_rate\n");
		goto out_mp_led_blink_rate;
	}

	err = class_create_file(mp_led_class, &mp_led_power);
	if (err) {
		printk(KERN_ALERT "mp_led: cannot create sysfs file, mp_led_power\n");
		goto out_mp_led_power;
	}

	err = class_create_file(mp_led_class, &mp_led_sys_ng);
	if (err) {
		printk(KERN_ALERT "mp_led: cannot create sysfs file, mp_led_sys_ng\n");
		goto out_mp_led_sys_ng;
	}

	err = class_create_file(mp_led_class, &mp_led_sys_ok);
	if (err) {
		printk(KERN_ALERT "mp_led: cannot create sysfs file, mp_led_sys_ok\n");
		goto out_mp_led_sys_ok;
	}

	err = class_create_file(mp_led_class, &mp_led_hdd1);
	if (err) {
		printk(KERN_ALERT "mp_led: cannot create sysfs file, mp_led_hdd1\n");
		goto out_mp_led_hdd1;
	}

	err = class_create_file(mp_led_class, &mp_led_otb);
	if (err) {
		printk(KERN_ALERT "mp_led: cannot create sysfs file, mp_led_otb\n");
		goto out_mp_led_otb;
	}
	
	err = class_create_file(mp_led_class, &mp_power_off);
	if (err) {
		printk(KERN_ALERT "mp_led: cannot create sysfs file, mp_power_off\n");
		goto out_mp_power_off ;
	}	
    return 0;
out_mp_power_off:
	class_remove_file(mp_led_class, &mp_led_otb);    
out_mp_led_otb:
	class_remove_file(mp_led_class, &mp_led_hdd1);
out_mp_led_hdd1:
	class_remove_file(mp_led_class, &mp_led_sys_ok);
out_mp_led_sys_ok:
	class_remove_file(mp_led_class, &mp_led_sys_ng);
out_mp_led_sys_ng:
	class_remove_file(mp_led_class, &mp_led_power);
out_mp_led_power:
	class_remove_file(mp_led_class, &mp_led_blink_rate);
out_mp_led_blink_rate:
	class_remove_file(mp_led_class, &mp_led_brightness);
out_mp_led_brightness:
	class_destroy(mp_led_class);
out_class:	
	return err;
}

//*****************************************************************************
int init_mp_led_module(void)
{
	int ret = 0;

	init_gpio_register();
	
	init_mp_led_data();

	ret = init_sys_fs();
	
	return ret;
}

//*****************************************************************************
void cleanup_mp_led_module(void)
{
	del_timer(&led_timer);
	class_remove_file(mp_led_class, &mp_led_otb);
	class_remove_file(mp_led_class, &mp_led_hdd1);
	class_remove_file(mp_led_class, &mp_led_sys_ok);
	class_remove_file(mp_led_class, &mp_led_sys_ng);
	class_remove_file(mp_led_class, &mp_led_power);

	class_remove_file(mp_led_class, &mp_led_blink_rate);
	class_remove_file(mp_led_class, &mp_led_brightness);

	class_destroy(mp_led_class);
	printk(KERN_INFO "mp_led: Module unloaded.\n");
}

//*****************************************************************************
module_init(init_mp_led_module);
module_exit(cleanup_mp_led_module);
