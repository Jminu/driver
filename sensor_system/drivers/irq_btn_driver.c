#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>

#define BTN 538
#define IRQ_NAME "button irq"

static int irq_num;
int g_system_mode = 0;

static irqreturn_t irq_btn_handler(int irq, void *data) {
	if (g_system_mode == 0) {
		g_system_mode = 1;
	}
	else {
		g_system_mode = 0;
	}
	return IRQ_HANDLED;
}

static int __init btn_init(void) {
	int ret;
	
	irq_num = gpio_to_irq(BTN);
	if (irq_num < 0) {
		printk(KERN_ERR "gpio to irq fail\n");
		return -1;
	}

	ret = request_irq(irq_num, irq_btn_handler, IRQF_TRIGGER_RISING, IRQ_NAME, NULL);
	if (ret < 0) {
		printk(KERN_ERR "request irq fail\n");
		return -1;
	}

	printk(KERN_INFO "btn module init\n");
	return 0;
}

static void __exit btn_exit(void) {
	free_irq(irq_num, NULL);
	return;
}

module_init(btn_init);
module_exit(btn_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JIN MINU");
MODULE_DESCRIPTION("BUTTON IRQ DRIVER");
