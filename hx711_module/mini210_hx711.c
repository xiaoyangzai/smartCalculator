/*
 * linux/drivers/char/mini210_buttons.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <mach/hardware.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>

#include <mach/map.h>
#include <mach/gpio.h>
#include <mach/regs-clock.h>
#include <mach/regs-gpio.h>

#define DEVICE_NAME		"hx711"

struct hx711_desc {
	int gpio;
	int number;
	char *name;	
};

static struct hx711_desc gpioes[] = {
	{ S5PV210_GPH3(2), 0, "DOUT" },
	{ S5PV210_GPH3(3), 1, "SCK" }
};

static DECLARE_WAIT_QUEUE_HEAD(hx711_waitq);

static volatile int ev_press = 0;
static volatile int weight = 0;
static struct timer_list timer;

static void read_hx711_data(struct hx711_desc *data)
{
	struct hx711_desc *bdata = data;
	weight = 0;
	unsigned tmp;
	int dout = bdata->gpio;
	int sck = (bdata + 1)->gpio;

	gpio_set_value(sck,0);
	while(gpio_get_value(dout));
	ndelay(500);
	int i = 0;
	for(i = 0;i < 24; i++)
	{
		gpio_set_value(sck,1);
		weight = weight<<1;
		udelay(1);
		tmp = gpio_get_value(dout);
		udelay(25);
		gpio_set_value(sck,0);
		udelay(5);
		if(tmp)
			weight++;
	}
	gpio_set_value(sck,1);
	weight = weight ^ 0x800000;
	udelay(25);
	gpio_set_value(sck,0);
	//udelay(5);
	//gpio_set_value(sck,1);
	//udelay(500);
}
static void mini210_hx711_timer(unsigned long _data)
{
	struct hx711_desc *bdata = (struct hx711_desc*)_data;

	printk("timeout,check weight....\n");
	//读取电子秤获取重量
	read_hx711_data(bdata);
	weight = (weight - 8414300) < 0 ? 0 : (weight - 8414300); 
	printk("output weight: %d\n",weight+8414300);
	if (weight >= 0) {
		if(weight < 1000)
			weight = 0;
		ev_press = 1;
		wake_up_interruptible(&hx711_waitq);
	}
	mod_timer(&timer, jiffies + msecs_to_jiffies(200));
}



static int mini210_hx711_open(struct inode *inode, struct file *file)
{
	setup_timer(&timer,mini210_hx711_timer,(unsigned long)gpioes);
	//timer.expires = jiffies+HZ;
	mod_timer(&timer, jiffies + msecs_to_jiffies(200));
	//add_timer(&timer);
	return 0;
}

static int mini210_hx711_close(struct inode *inode, struct file *file)
{
	del_timer_sync(&timer);
	return 0;
}

static int mini210_hx711_read(struct file *filp, char __user *buff,
		size_t count, loff_t *offp)
{
	unsigned long err;

	while(!ev_press) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		else
			wait_event_interruptible(hx711_waitq, ev_press);
	}
	ev_press = 0;
	err = copy_to_user((void *)buff, (const void *)(&weight),
			min(sizeof(weight), count));
	return err ? -EFAULT : min(sizeof(weight), count);
}

static struct file_operations dev_fops = {
	.owner		= THIS_MODULE,
	.open		= mini210_hx711_open,
	.release	= mini210_hx711_close, 
	.read		= mini210_hx711_read,
};

static struct miscdevice misc = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= DEVICE_NAME,
	.fops		= &dev_fops,
};

static int __init hx711_dev_init(void)
{
	int ret;

	s3c_gpio_cfgpin(gpioes[1].gpio, S3C_GPIO_OUTPUT);
	gpio_set_value(gpioes[1].gpio,0);
	ret = misc_register(&misc);
	printk(DEVICE_NAME"\tinitialized\n");

	return ret;
}

static void __exit hx711_dev_exit(void)
{
	misc_deregister(&misc);
	printk(DEVICE_NAME"\t removed\n");

}

module_init(hx711_dev_init);
module_exit(hx711_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wang Da Yang Inc.");

