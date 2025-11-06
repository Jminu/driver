#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>

#define DRIVER_NAME "led_btn_driver"
#define GPIO_LED 528
#define GPIO_BTN 537

static int btn_state = 0;
static int led_state = 0;
static int gpio_irq_num = 0;

static irqreturn_t btn_irq_handler(int irq, void *dev_id) {
    if (led_state == 0) {
        gpio_set_value(GPIO_LED, 1);
        printk(KERN_INFO "gpio set 1\n");
    }
    else {
        gpio_set_value(GPIO_LED, 0);
        printk(KERN_INFO "gpio set 0\n");
    }
    return IRQ_HANDLED;
}

static int __init led_btn_init(void) {
    int ret;

    if (gpio_request(GPIO_BTN, DRIVER_NAME) != 0) { // gpio설정 요청
        printk(KERN_INFO "BTN request fail\n");
        return -1;
    }

    if (gpio_direction_input(GPIO_BTN) != 0) { // input으로 설정
        printk(KERN_INFO "gpio direction input\n");
        return -1;
    }

    gpio_irq_num = gpio_to_irq(GPIO_BTN); // gpio를 irq에 등록 요청
    if (gpio_irq_num < 0) {
        printk(KERN_INFO "gpio irq fail\n");
        return -1;
    }

    ret = request_irq(gpio_irq_num, btn_irq_handler, IRQF_TRIGGER_FALLING,DRIVER_NAME ,NULL);
    if (ret < 0) {
        printk(KERN_INFO "irq request fail\n");
        return -1;
    }

    if (gpio_request(GPIO_LED, DRIVER_NAME) != 0) {
        printk(KERN_INFO "LED request fail\n");
        return -1;
    }

    if (gpio_direction_output(GPIO_LED, 0) != 0) {
        printk(KERN_INFO "LED setting output 0\n");
        return -1;
    }

    printk(KERN_INFO "GPIO set up success\n");
    return 0;
}

static void __exit led_btn_exit(void) {
    gpio_free(GPIO_BTN);
    gpio_free(GPIO_LED);
    free_irq(gpio_irq_num, NULL);
    printk(KERN_INFO "module unloaded\n");
}

module_init(led_btn_init);
module_exit(led_btn_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JIN MINU");