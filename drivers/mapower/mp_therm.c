//*****************************************************************************
// file: mp_therm.c
// function: thermal access function for IX1 board
// version: 1.0 (2010/04/13)
// Author: Ken Cheng <kencheng@mapower.com.tw>
//*****************************************************************************
#include <linux/types.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/device.h>

#include <linux/io.h>
#include <asm/system.h>
#include <asm/sections.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>

#include "taco_820.h"
#include "mp_therm.h"

//*****************************************************************************
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mapower Thermal Module for IX1");
MODULE_AUTHOR("Ken Cheng <kencheng@mapower.com.tw>");

//*****************************************************************************
#define NUMBER_OF_ITERATIONS 	10
#define TEMP_MEASURE_NORMAL 	0
#define TEMP_MEASURE_ERROR 		1

//*****************************************************************************
static int hot_limit = 16; 
static int temps_counter = 0;

//*****************************************************************************
static int write_reg( int reg, u32 data )
{
	writel(data, reg);
	return 0;
}

//*****************************************************************************
static int read_reg( int reg )
{
	int data = 0;
	data = readl(reg);
	return data;
}

//*****************************************************************************
static int init_hardware_register(void)
{
	int rc;
	
	/* release fan/tacho from system reset */		
	*((volatile unsigned long *)SYS_CTRL_RSTEN_CLR_CTRL) = (1UL << SYS_CTRL_RSTEN_MISC_BIT);
	
	/* Pull Down the GPIO 0 from the software */
	*((volatile unsigned long *)SYS_CTRL_PULLUP_SEL) |= TEMP_TACHO_PULLUP_CTRL_VALUE;

	/* Enable secondary use */	
	*((volatile unsigned long *)SYS_CTRL_SECONDARY_SEL) |= (1UL << PRIMARY_FUNCTION_ENABLE_FAN_TEMP);
	
	rc = read_reg(TACHO_CLOCK_DIVIDER);
	if (rc < 0) 
	{
		printk(KERN_ERR "mp_therm: Thermostat failed to read config\n");
		return -ENODEV;
	}
	
	/* Set the Tacho clock divider up */
	write_reg( TACHO_CLOCK_DIVIDER, TACHO_CORE_TACHO_DIVIDER_VALUE );
	
	/* check tacho divider set correctly */	
	rc = read_reg(TACHO_CLOCK_DIVIDER);
	/* Comparing a 10 bit value to a 32 bit return value */
	if ((rc & TACHO_CORE_TACHO_DIVIDER_VALUE) != TACHO_CORE_TACHO_DIVIDER_VALUE) 
	{
		printk(KERN_ERR "mp_therm: Set Tacho Divider Value Failed readback:%d\n", rc);
		return -ENODEV;
	}
	
	write_reg(PWM_CLOCK_DIVIDER, PWM_CORE_CLK_DIVIDER_VALUE);
	
	printk(KERN_INFO "mp_therm: initializing - NAS7820\n");

	return 0;
}

//*****************************************************************************
int GetTemperatureCounter(void)
{
	u32 res;
	int count = 1;
	
	while (!(read_reg( TACHO_THERMISTOR_CONTROL ) & 
		   (1 << TACHO_THERMISTOR_CONTROL_THERM_VALID))) 
	{
		if(count == NUMBER_OF_ITERATIONS) 
		{
			return hot_limit;
		}
		count++;
		msleep(100);
	}

	res = read_reg(TACHO_THERMISTOR_RC_COUNTER) & TACHO_THERMISTOR_RC_COUNTER_MASK;

	return res;
}

//*****************************************************************************
static void read_sensors(int *p_temps_counter)
{
	*p_temps_counter  = GetTemperatureCounter();

	/* Set to Temperature measurement */
	write_reg( TACHO_THERMISTOR_CONTROL, 
		((1 << TACHO_THERMISTOR_CONTROL_THERM_ENABLE) 
		| (0 << TACHO_THERMISTOR_CONTROL_THERM_VALID)));
}

//*****************************************************************************
static int monitor_task(void *arg)
{
	int* p_temps_counter = arg;
	
	while(!kthread_should_stop()) 
	{
		if (unlikely(freezing(current)))
		{
			refrigerator();
		}

		msleep_interruptible(2000);

		read_sensors(p_temps_counter);
	}

	return 0;
}

//*****************************************************************************
static struct task_struct*	thread_therm = NULL;

//*****************************************************************************
static int init_therm_thread(void)
{
	/* Start the thermister measuring */
	write_reg(TACHO_THERMISTOR_CONTROL, (1 << TACHO_THERMISTOR_CONTROL_THERM_ENABLE));
	
	thread_therm = kthread_run(monitor_task, &temps_counter, "kfand");

	if (thread_therm == ERR_PTR(-ENOMEM)) 
	{
		printk(KERN_INFO "mp_therm: Kthread creation failed\n");
		thread_therm = NULL;
		return -ENOMEM;
	}

	return 0;
}

//*****************************************************************************
#define counter_to_ohm	(156)

//*****************************************************************************
static int get_temperature(int counter)
{
	unsigned long ohm_value;
	int i;
	
	ohm_value = counter * counter_to_ohm;
	
	for(i=0; i < TABLE_LEN; i++)
	{
		if(ohm_value >= ohms_list[i])
		{
			break;
		}
	}

	return i-40;
}

//*****************************************************************************
#define DEVNAME "nasmonitor"

//*****************************************************************************
static ssize_t mp_therm_mbtemp_show(struct class *class, char *buf)
{
	return sprintf(buf, "%d\n", get_temperature(temps_counter));
}

//*****************************************************************************
struct class *mp_therm_class;

static struct class_attribute mp_therm_mbtemp =
	__ATTR(mbtemp, S_IRUGO , mp_therm_mbtemp_show, NULL);

//*****************************************************************************
int init_sys_fs(void)
{
	int err;
	
	mp_therm_class = class_create(THIS_MODULE, DEVNAME);
	if (IS_ERR(mp_therm_class)) {
		err = PTR_ERR(mp_therm_class);
		printk(KERN_ALERT "mp_therm: cannot create mp_therm class");
		goto out_class;
	}

	err = class_create_file(mp_therm_class, &mp_therm_mbtemp);
	if (err) {
		printk(KERN_ALERT "mp_therm: cannot create sysfs file, mp_therm_mbtemp\n");
		goto out_mp_therm_mbtemp;
	}

    return 0;

out_mp_therm_mbtemp:
	class_destroy(mp_therm_class);
out_class:	
	return err;
}

//*****************************************************************************
static int __init mp_therm_init(void)
{
	int ret;

	if((ret = init_hardware_register()) != 0)
		return ret;
	
	if((ret = init_therm_thread()) != 0)
		return ret;

	if((ret = init_sys_fs()) != 0)
		return ret;
		
	return 0;
}

//*****************************************************************************
static void __exit mp_therm_exit(void)
{
	if (thread_therm)
		kthread_stop(thread_therm);
	
	remove_proc_entry("mp_therm", NULL);
}

//*****************************************************************************
module_init(mp_therm_init);
module_exit(mp_therm_exit);
