#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/gpio.h>

#define BTN 533

static int irq_num;
static int cnt = 0;

static irqreturn_t btn_isr(int irq, void *data) {
	printk(KERN_INFO "cnt=%d\n", cnt);
	cnt++;

	return IRQ_HANDLED;
}

static int __init btn_init(void) {
	int ret;

	ret = gpio_request_one(BTN, GPIOF_IN, "btn gpio");
	if (ret != 0) {
		printk(KERN_INFO "gpio request one fail\n");
		return -1;
	}
	
	irq_num = gpio_to_irq(BTN);
	if (irq_num < 0) {
		printk(KERN_INFO "gpio to irq fail\n");
		return -1;
	}

	ret = request_irq(irq_num, btn_isr, IRQF_TRIGGER_FALLING, "btn irq", NULL);
	if (ret < 0) {
		printk(KERN_INFO "request irq fail\n");
		return -1;
	}

	printk(KERN_INFO "module load\n");
	return 0;
}


static void __exit btn_exit(void) {
	gpio_free(BTN);
	free_irq(irq_num, NULL);
	return;
}

module_init(btn_init);
module_exit(btn_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JIN MINU");
