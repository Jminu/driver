#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>

#define DRIVER_NAME "switch_driver"
#define GPIO_LED 529
#define GPIO_BTN 531

static int btn_state = 0; // 초기 0
static int gpio_irq_num = 0;

static irqreturn_t btn_interrupt_handler(int irq, void *dev_id) {
	int led_state = gpio_get_value(GPIO_LED);
	
	if (led_state == 0) {
		gpio_set_value(GPIO_LED, 1);
		printk(KERN_ERR "LED set 1\n");
	}
	else {
		gpio_set_value(GPIO_LED, 0);
		printk(KERN_ERR "LED set 0\n");
	}
	return IRQ_HANDLED;
}

static int __init switch_module_init(void) {
	if (gpio_request(GPIO_BTN, DRIVER_NAME) != 0) {
		printk(KERN_ERR "switch gpio request fail\n");
		return -1;
	}

	if (gpio_direction_input(GPIO_BTN) != 0) {
		printk(KERN_ERR "switch to input fail\n");
		return -1;
	}

	gpio_irq_num = gpio_to_irq(GPIO_BTN);
	if (gpio_irq_num < 0) {
		printk(KERN_ERR "fail to get irq number\n");
		gpio_free(GPIO_BTN);
		return -1;
	}

	// 상승 엣지일때 핸들러 작동
	int ret = request_irq(gpio_irq_num, btn_interrupt_handler, IRQF_TRIGGER_FALLING, DRIVER_NAME, NULL);

	if (ret < 0) {
		printk(KERN_ERR "fail to request interrupt handler\n");
		return -1;
	}

	printk(KERN_INFO "gpio switch module loaded\n");
	return 1;
}

static void __exit switch_module_exit(void) {
	gpio_free(GPIO_BTN);
	free_irq(gpio_irq_num, NULL);
	printk(KERN_INFO "gpio switch module unloaded\n");
}

module_init(switch_module_init);
module_exit(switch_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JIN MINU");
