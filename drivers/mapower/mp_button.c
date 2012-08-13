//*****************************************************************************
// file: mp_button.c
// function: button access function for ix1 board
// version: 1.1 (2010/06/28)
// Author: Ken Cheng <kencheng@mapower.com.tw>
//*****************************************************************************
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/workqueue.h>
#include <linux/io.h>
#include <mach/hardware.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <asm/uaccess.h>

#include "mp_button.h"

//*****************************************************************************
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mapower Button Module for IX1");
MODULE_AUTHOR("Ken Cheng <kencheng@mapower.com.tw>");

//*****************************************************************************
#define GPIO_MAX        32

#define RESET_BUTTON_GPIO	5				// A5
#define OTB_BUTTON_GPIO		6				// A6
#define POWER_BUTTON_GPIO	(11+GPIO_MAX)	// B11

#define RESET_BUTTON_MASK	(1UL << (RESET_BUTTON_GPIO))
#define OTB_BUTTON_MASK		(1UL << (OTB_BUTTON_GPIO))

#define POWER_BUTTON_MASK	(1UL << (POWER_BUTTON_GPIO-GPIO_MAX))

#define SWITCH_MASK_A (RESET_BUTTON_MASK | OTB_BUTTON_MASK)
#define SWITCH_MASK_B (POWER_BUTTON_MASK)

#define IRQ_NUM_A             GPIOA_INTERRUPT

#define INT_STATUS_REG_A      GPIO_A_INTERRUPT_EVENT
#define RISING_INT_REG_A      GPIO_A_RISING_EDGE_ACTIVE_HIGH_ENABLE
#define FALLING_INT_REG_A     GPIO_A_FALLING_EDGE_ACTIVE_LOW_ENABLE

#define SWITCH_CLR_OE_REG_A   GPIO_A_OUTPUT_ENABLE_CLEAR
#define DEBOUNCE_REG_A        GPIO_A_INPUT_DEBOUNCE_ENABLE

#define IRQ_NUM_B             GPIOB_INTERRUPT

#define INT_STATUS_REG_B      GPIO_B_INTERRUPT_EVENT
#define RISING_INT_REG_B      GPIO_B_RISING_EDGE_ACTIVE_HIGH_ENABLE
#define FALLING_INT_REG_B     GPIO_B_FALLING_EDGE_ACTIVE_LOW_ENABLE

#define SWITCH_CLR_OE_REG_B   GPIO_B_OUTPUT_ENABLE_CLEAR
#define DEBOUNCE_REG_B        GPIO_B_INPUT_DEBOUNCE_ENABLE

//*****************************************************************************
extern spinlock_t oxnas_gpio_spinlock;

static spinlock_t mp_button_spinlock;

static int reset_btn_dev_id;
static int otb_btn_dev_id;
static int power_btn_dev_id;

static unsigned long reset_pressed_time = 0;
static unsigned long reset_released_time = 0;

static unsigned long otb_pressed_time = 0;
static unsigned long otb_released_time = 0;

static unsigned long power_pressed_time = 0;
static unsigned long power_released_time = 0;

static DECLARE_WAIT_QUEUE_HEAD(wq);

//*****************************************************************************
static void work_handler(struct work_struct * not_used) 
{
	wake_up_interruptible(&wq);
}

//*****************************************************************************
DECLARE_WORK(button_hotplug_work, work_handler);

//*****************************************************************************
static irqreturn_t reset_btn_int_handler(int irq, void* dev_id)
{
	int status = IRQ_NONE;
	unsigned int int_status = readl((volatile unsigned long *)INT_STATUS_REG_A);

	if (int_status & RESET_BUTTON_MASK) 
	{
		spin_lock(&oxnas_gpio_spinlock);
		
		writel(readl(RISING_INT_REG_A) & ~RESET_BUTTON_MASK, RISING_INT_REG_A);
		writel(readl(FALLING_INT_REG_A) & ~RESET_BUTTON_MASK, FALLING_INT_REG_A);
		
		writel(RESET_BUTTON_MASK, GPIO_A_RISING_EDGE_DETECT);
		writel(RESET_BUTTON_MASK, GPIO_A_FALLING_EDGE_DETECT);
		
		if (readl(GPIO_A_DATA) & RESET_BUTTON_MASK ) 
		{
			writel(readl(FALLING_INT_REG_A) | RESET_BUTTON_MASK, FALLING_INT_REG_A);
			spin_lock(&mp_button_spinlock);
			reset_released_time = jiffies;
			spin_unlock(&mp_button_spinlock);
			schedule_work(&button_hotplug_work);
		} 
		else 
		{
			writel(readl(RISING_INT_REG_A) | RESET_BUTTON_MASK, RISING_INT_REG_A);
			spin_lock(&mp_button_spinlock);
			reset_pressed_time = jiffies;
			spin_unlock(&mp_button_spinlock);
		}
		spin_unlock(&oxnas_gpio_spinlock);
		
		if (!readl((volatile unsigned long *)INT_STATUS_REG_A)) 
			status = IRQ_HANDLED;
	}

	return status;
}

//*****************************************************************************
static irqreturn_t otb_btn_int_handler(int irq, void* dev_id)
{
	int status = IRQ_NONE;
	unsigned int int_status = readl((volatile unsigned long *)INT_STATUS_REG_A);

	if (int_status & OTB_BUTTON_MASK) 
	{
		spin_lock(&oxnas_gpio_spinlock);
		
		writel(readl(RISING_INT_REG_A) & ~OTB_BUTTON_MASK, RISING_INT_REG_A);
		writel(readl(FALLING_INT_REG_A) & ~OTB_BUTTON_MASK, FALLING_INT_REG_A);
		
		writel(OTB_BUTTON_MASK, GPIO_A_RISING_EDGE_DETECT);
		writel(OTB_BUTTON_MASK, GPIO_A_FALLING_EDGE_DETECT);
		
		if (readl(GPIO_A_DATA) & OTB_BUTTON_MASK ) 
		{
			writel(readl(FALLING_INT_REG_A) | OTB_BUTTON_MASK, FALLING_INT_REG_A);
			spin_lock(&mp_button_spinlock);
			otb_released_time = jiffies;
			spin_unlock(&mp_button_spinlock);
			schedule_work(&button_hotplug_work);
		} 
		else 
		{
			writel(readl(RISING_INT_REG_A) | OTB_BUTTON_MASK, RISING_INT_REG_A);
			spin_lock(&mp_button_spinlock);
			otb_pressed_time = jiffies;
			spin_unlock(&mp_button_spinlock);
		}
		spin_unlock(&oxnas_gpio_spinlock);
		
		if (!readl((volatile unsigned long *)INT_STATUS_REG_A)) 
			status = IRQ_HANDLED;
	}

	return status;
}

//*****************************************************************************
static irqreturn_t power_btn_int_handler(int irq, void* dev_id)
{
	int status = IRQ_NONE;
	unsigned int int_status = readl((volatile unsigned long *)INT_STATUS_REG_B);

	if (int_status & POWER_BUTTON_MASK) 
	{
		spin_lock(&oxnas_gpio_spinlock);
		
		writel(readl(RISING_INT_REG_B) & ~POWER_BUTTON_MASK, RISING_INT_REG_B);
		writel(readl(FALLING_INT_REG_B) & ~POWER_BUTTON_MASK, FALLING_INT_REG_B);
		
		writel(POWER_BUTTON_MASK, GPIO_B_RISING_EDGE_DETECT);
		writel(POWER_BUTTON_MASK, GPIO_B_FALLING_EDGE_DETECT);
		
		if (readl(GPIO_B_DATA) & POWER_BUTTON_MASK ) 
		{
			writel(readl(FALLING_INT_REG_B) | POWER_BUTTON_MASK, FALLING_INT_REG_B);
			spin_lock(&mp_button_spinlock);
			power_released_time = jiffies;
			spin_unlock(&mp_button_spinlock);
			schedule_work(&button_hotplug_work);
		} 
		else 
		{
			writel(readl(RISING_INT_REG_B) | POWER_BUTTON_MASK, RISING_INT_REG_B);
			spin_lock(&mp_button_spinlock);
			power_pressed_time = jiffies;
			spin_unlock(&mp_button_spinlock);
		}
		spin_unlock(&oxnas_gpio_spinlock);
		
		if (!readl((volatile unsigned long *)INT_STATUS_REG_B)) 
			status = IRQ_HANDLED;
	}

	return status;
}

//*****************************************************************************
int init_hardware_register(void)
{
	unsigned long flags;

	if (request_irq(IRQ_NUM_A, reset_btn_int_handler, IRQF_SHARED, "mp_button", &reset_btn_dev_id)) 
	{
		printk(KERN_ERR "mp_button: cannot register IRQ %d\n", IRQ_NUM_A);
		return -EIO;
	}

	if (request_irq(IRQ_NUM_A, otb_btn_int_handler, IRQF_SHARED, "mp_button", &otb_btn_dev_id)) 
	{
		printk(KERN_ERR "mp_button: cannot register IRQ %d\n", IRQ_NUM_A);
		return -EIO;
	}

	if (request_irq(IRQ_NUM_B, power_btn_int_handler, IRQF_SHARED, "mp_button", &power_btn_dev_id)) 
	{
		printk(KERN_ERR "mp_button: cannot register IRQ %d\n", IRQ_NUM_B);
		return -EIO;
	}

	spin_lock_irqsave(&oxnas_gpio_spinlock, flags);
	
    writel(readl(SYS_CTRL_SECONDARY_SEL)   & ~SWITCH_MASK_A, SYS_CTRL_SECONDARY_SEL);
    writel(readl(SYS_CTRL_TERTIARY_SEL)    & ~SWITCH_MASK_A, SYS_CTRL_TERTIARY_SEL);
    writel(readl(SYS_CTRL_QUATERNARY_SEL)  & ~SWITCH_MASK_A, SYS_CTRL_QUATERNARY_SEL);
    writel(readl(SYS_CTRL_DEBUG_SEL)       & ~SWITCH_MASK_A, SYS_CTRL_DEBUG_SEL);
    writel(readl(SYS_CTRL_ALTERNATIVE_SEL) & ~SWITCH_MASK_A, SYS_CTRL_ALTERNATIVE_SEL);

    writel(readl(SEC_CTRL_SECONDARY_SEL)   & ~SWITCH_MASK_B, SEC_CTRL_SECONDARY_SEL);
    writel(readl(SEC_CTRL_TERTIARY_SEL)    & ~SWITCH_MASK_B, SEC_CTRL_TERTIARY_SEL);
    writel(readl(SEC_CTRL_QUATERNARY_SEL)  & ~SWITCH_MASK_B, SEC_CTRL_QUATERNARY_SEL);
    writel(readl(SEC_CTRL_DEBUG_SEL)       & ~SWITCH_MASK_B, SEC_CTRL_DEBUG_SEL);
    writel(readl(SEC_CTRL_ALTERNATIVE_SEL) & ~SWITCH_MASK_B, SEC_CTRL_ALTERNATIVE_SEL);

	writel(SWITCH_MASK_A, SWITCH_CLR_OE_REG_A);
	writel(SWITCH_MASK_B, SWITCH_CLR_OE_REG_B);
	
	writel(SWITCH_MASK_A, GPIO_A_RISING_EDGE_DETECT);
	writel(SWITCH_MASK_A, GPIO_A_FALLING_EDGE_DETECT);

	writel(SWITCH_MASK_B, GPIO_B_RISING_EDGE_DETECT);
	writel(SWITCH_MASK_B, GPIO_B_FALLING_EDGE_DETECT);
	
	writel(readl(DEBOUNCE_REG_A)    | SWITCH_MASK_A, DEBOUNCE_REG_A);
	writel(readl(FALLING_INT_REG_A) | SWITCH_MASK_A, FALLING_INT_REG_A);
	
	writel(readl(DEBOUNCE_REG_B)    | SWITCH_MASK_B, DEBOUNCE_REG_B);
	writel(readl(FALLING_INT_REG_B) | SWITCH_MASK_B, FALLING_INT_REG_B);
	
	spin_unlock_irqrestore(&oxnas_gpio_spinlock, flags);
	
	return 0;
}

//*****************************************************************************
static int mp_button_major = 0;

static struct button_structure button_data;

//*****************************************************************************
int check_button(void)
{
	spin_lock(&mp_button_spinlock);
	if(power_released_time != 0)
	{
		button_data.buttonid = WIX_BTN_POWER;
		button_data.state = BUTTON_EVENT;
		power_released_time = 0;
		power_pressed_time = 0;
		spin_unlock(&mp_button_spinlock);
		
		return 1;
	}
	
	if(otb_released_time != 0)
	{
		button_data.buttonid = WIX_BTN_LCM_SELECT;
		button_data.state = BUTTON_EVENT;
		otb_released_time = 0;
		otb_pressed_time = 0;
		spin_unlock(&mp_button_spinlock);
		
		return 2;
	}
	
	if(reset_released_time != 0)
	{
		int during_time = (reset_released_time - reset_pressed_time)/HZ;
		if(during_time > LONG_TIME)
			button_data.state = BUTTON_LONG_EVENT;
		else
			button_data.state = BUTTON_EVENT;

		button_data.buttonid = WIX_BTN_RESET;
		reset_released_time = 0;
		reset_pressed_time = 0;
		spin_unlock(&mp_button_spinlock);
		
		return 3;
	}
	
	button_data.buttonid = WIX_NO_BTN;
	button_data.state = BUTTON_EVENT;

	spin_unlock(&mp_button_spinlock);
	
	return 0;
}			

//*****************************************************************************
static int mp_button_ioctl (struct inode *inode, struct file *filp,
                           unsigned int cmd, unsigned long arg)
{
    int err = 0;
	int ret = 0;

	if (_IOC_TYPE(cmd) != BUTTON_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > BUTTON_IOC_MAXNR)
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ) 
		err = !access_ok(VERIFY_WRITE, (void __user*)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE) 
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err)
		return -EFAULT;
	
    err = 0;
    switch (cmd)
	{
        case READ_BUTTON_NONBLOCKING:
			check_button();
			
			if (copy_to_user((struct button_structure *)arg, &button_data, sizeof(struct button_structure)))
				return -ENOMEM;
            break;
			
		case READ_BUTTON_BLOCKING:
			ret = check_button();
			
			if(ret == 0)
			{
				wait_event_interruptible(wq, 
					(power_pressed_time != 0) || (otb_pressed_time != 0) || (reset_pressed_time != 0));
				check_button();
			}

			if (copy_to_user((struct button_structure *)arg, &button_data, sizeof(struct button_structure)))
				return -ENOMEM;
			break;
			
		case READ_POWER_BTN_STATUS:
			button_data.buttonid = WIX_BTN_POWER;
			
			if((readl(GPIO_B_DATA) & POWER_BUTTON_MASK) == 0)
				button_data.state = BUTTON_PRESSED;
			else
				button_data.state = BUTTON_NOT_PRESSED;
	
			if (copy_to_user((struct button_structure *)arg, &button_data, sizeof(struct button_structure)))
				return -ENOMEM;
			break;
			
		case READ_RESET_BTN_STATUS:
			button_data.buttonid = WIX_BTN_RESET;
			
			if((readl(GPIO_A_DATA) & RESET_BUTTON_MASK) == 0)
				button_data.state = BUTTON_PRESSED;
			else
				button_data.state = BUTTON_NOT_PRESSED;
	
			if (copy_to_user((struct button_structure *)arg, &button_data, sizeof(struct button_structure)))
				return -ENOMEM;
			break;
			
		case READ_LCM_SELECT_BTN_STATUS:
			button_data.buttonid = WIX_BTN_LCM_SELECT;
			
			if((readl(GPIO_A_DATA) & OTB_BUTTON_MASK) == 0)
				button_data.state = BUTTON_PRESSED;
			else
				button_data.state = BUTTON_NOT_PRESSED;
	
			if (copy_to_user((struct button_structure *)arg, &button_data, sizeof(struct button_structure)))
				return -ENOMEM;
			break;
			
        default:
            printk(KERN_ERR "mp_button: Unknown ioctl command (%d)\n", cmd);
            break;
    }

    return 0;
}

//*****************************************************************************
struct file_operations mp_button_fops = 
{
	.owner = THIS_MODULE,
	.ioctl = mp_button_ioctl,
};

//*****************************************************************************
int init_chrdev(void)
{
	int result;

	result = register_chrdev(mp_button_major, "buttons", &mp_button_fops);
	if (result < 0)
		return result;
	if (mp_button_major == 0)
		mp_button_major = result;
	return 0;
}

//*****************************************************************************
int init_mp_button_module(void)
{
	int ret = 0;
	
	if((ret = init_hardware_register()) != 0)
		return ret;
		
	if((ret = init_chrdev()) != 0)
		return ret;
	
	return 0;
}

//*****************************************************************************
void cleanup_mp_button_module(void)
{
	unsigned long flags;

	unregister_chrdev(mp_button_major, "buttons");
	
	spin_lock_irqsave(&oxnas_gpio_spinlock, flags);
	writel(readl(FALLING_INT_REG_A) & ~SWITCH_MASK_A, FALLING_INT_REG_A);
	writel(readl(FALLING_INT_REG_B) & ~SWITCH_MASK_B, FALLING_INT_REG_B);
	spin_unlock_irqrestore(&oxnas_gpio_spinlock, flags);

	free_irq(IRQ_NUM_A, &reset_btn_dev_id);
	free_irq(IRQ_NUM_A, &otb_btn_dev_id);
	free_irq(IRQ_NUM_B, &power_btn_dev_id);
}

//*****************************************************************************
module_init(init_mp_button_module);
module_exit(cleanup_mp_button_module);
