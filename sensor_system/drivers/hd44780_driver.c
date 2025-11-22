#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/string.h>

#define DRIVER_NAME "hd44780_driver"
#define DEVICE_COUNT 1
#define DEVICE_NAME "hd44780_device"
#define CLASS_NAME "hd44780_class"

/*
 * Write / Read mode 로 구분,
 * Read mode: input data가 port에서 mcu로
 * Write mode: output data가 port로
 * 
 * i2c address: 0100 A2 A1 A0 -> 0x27
 *
 * RS: select register
 * 	0: instruction register (for writing)
 * 	1: data register (for read)
 *
 * RW: select Write or Read
 * 	0: Write
 * 	1: Read
 *
 * E: Start data read / write
 *
 * DB4 ~ DB7: 상위 4비트
 *
 * DB0 ~ DB3: 하위 4비트
 * 
 * Example:
 * 	|RS|RW|DB7|DB6|DB5|DB4|DB3|DB2|DB1|DB0|
 * 	 0  0   0   0   0   0   0   0   0   1
 * 	 = CLEAR DISPLAY instruction
 *
 * 
 * D: display
 * C: cursor
 * B: blink
 * I/D: increment, decrement
 * DL: set data length
 * N: set number of data line
 * F: set charactor font	
 */

/*
 *
 * BL|E|RW|RS -> 하위 4비트에 들어감
 * 1 |1|0 |1
 *
 */
#define RS (1 << 0)
#define RW (0 << 1) // write만 할거임
#define E (1 << 2) // 펄스
#define BL (1 << 3) // 백라이트

#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_FUNCTIONSET 0x28
#define LCD_DISPLAYON 0x0C
#define LCD_DISPLAYOFF 0x08
#define LCD_ENTRYMODESET 0x06

static struct hd44780_device {
	struct i2c_client *client;
	dev_t dev_num;
	struct cdev hd44780_cdev;
	struct class *class;
};

static const struct of_device_id hd44780_ids[] = {
	{.compatible = "jmw,hd44780"},
	{},
};
MODULE_DEVICE_TABLE(of, hd44780_ids);

static int i2c_lcd_write_byte(struct i2c_client *client, u8 byte) { // u8: unsigned char
	int ret;
	ret = i2c_smbus_write_byte(client, byte); // 1바이트 i2c에 write
	if (ret < 0) {
		printk(KERN_ERR "i2c write fail\n");
		return -1;
	}

	return 0;
}

/*
 * 4비트(데이터)에 나머지 4비트(제어비트) 결합 총 8비트 전송
 * @mode: register set (RS)
 * 	RS:0 명령 전송
 * 	RS:1 데이터 전송
 */
static void lcd_send_nibble(struct i2c_client *client, u8 data, u8 mode) {
	u8 byte_no_e = data | BL | mode;
	u8 byte_with_e = data | BL | E | mode;
	
	i2c_lcd_write_byte(client, byte_no_e);
	udelay(10);

	i2c_lcd_write_byte(client, byte_with_e);
	udelay(10);

	i2c_lcd_write_byte(client, byte_no_e);
	udelay(50);
}

/*
 * 상위 4비트와 하위 4비트를 보냄
 */
static void lcd_send_byte(struct i2c_client *client, u8 data, u8 mode) {
	lcd_send_nibble(client, data & 0xF0, mode); // 상위 4비트 보냄
	lcd_send_nibble(client, (data << 4) & 0xF0, mode); // 하위 4비트 보냄
}

/*
 * lcd에 명령 전송-> 어떤 모드로 할지
 * ex) 4비트 모드-> 상위비트 0010 보냄
 */
static void lcd_write_cmd(struct i2c_client *client, u8 cmd) {
	lcd_send_byte(client, cmd, 0x00); // 0x00: RS=0
}

static void lcd_write_data(struct i2c_client *client, char data) {
	lcd_send_byte(client, data, RS); // 0x01: RS=1
}

static void lcd_init(struct i2c_client *client) {
	msleep(50);

	// 처음은 8비트 모드
	lcd_send_nibble(client, 0x30, 0x00);
	msleep(10);
	lcd_send_nibble(client, 0x30, 0x00);
	udelay(150);
	lcd_send_nibble(client, 0x30, 0x00);
	udelay(150);
	printk(KERN_INFO "8비트 모드로 변경\n");


	lcd_send_nibble(client, 0x20, 0x00);
	udelay(100);	
	printk(KERN_INFO "4비트 모드로 변경\n");

	lcd_write_cmd(client, LCD_FUNCTIONSET);
	lcd_write_cmd(client, LCD_DISPLAYON); // display on
	lcd_write_cmd(client, LCD_CLEARDISPLAY); // 화면 지움
	lcd_write_cmd(client, LCD_ENTRYMODESET); // 커서 우측 이동
	

	printk(KERN_INFO "lcd init success\n");
}

static void lcd_print(struct i2c_client *client, const char *str, int len) {
	printk(KERN_INFO "strlen: %d\n", len);

	for (int i = 0; i < 16; i++) {
		if (*str == '\n') {
			lcd_write_data(client, ' ');
			continue;
		}
		lcd_write_data(client, *str++);
	}
}

static ssize_t hd44780_write(struct file *file, const char __user *buf, size_t len, loff_t *pos) {
	struct hd44780_device *hd44780 = file->private_data;
	char kbuf[32];

	if (len > 31)
		len = 31;

	int ret;
	ret = copy_from_user(kbuf, buf, len);

	lcd_write_cmd(hd44780->client, LCD_CLEARDISPLAY);
	lcd_print(hd44780->client, kbuf, strlen(kbuf));

	return len;
}

static int hd44780_open(struct inode *inode, struct file *file) {
	struct hd44780_device *hd44780;
	hd44780 = container_of(inode->i_cdev, struct hd44780_device, hd44780_cdev);
	file->private_data = hd44780;

	return 0;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = hd44780_open,
	.write = hd44780_write,
};


static int hd44780_probe(struct i2c_client *client) {
	struct hd44780_device *hd44780;
	int ret;

	hd44780 = devm_kzalloc(&client->dev, sizeof(struct hd44780_device), GFP_KERNEL);
	if (hd44780 < 0) {
		printk(KERN_ERR "devm kzalloc fail\n");
		return -1;
	}

	hd44780->client = client;

	i2c_set_clientdata(client, hd44780);

	lcd_init(client); // 초기화 작업

	ret = alloc_chrdev_region(&(hd44780->dev_num), 0, 1, DEVICE_NAME);
	if (ret < 0) {
		printk(KERN_ERR "alloc chrdev region fail\n");
		return -1;
	}

	cdev_init(&(hd44780->hd44780_cdev), &fops);
	ret = cdev_add(&(hd44780->hd44780_cdev), hd44780->dev_num, DEVICE_COUNT);
	if (ret < 0) {
		printk(KERN_ERR "cdev add fail\n");
		return -1;
	}

	hd44780->class = class_create(CLASS_NAME);
	device_create(hd44780->class, NULL, hd44780->dev_num, NULL, DEVICE_NAME);

	printk(KERN_INFO "probe success\n");

	return 0;
}

static void hd44780_remove(struct i2c_client *client) {
	struct hd44780_device *hd44780 = i2c_get_clientdata(client);
	
	device_destroy(hd44780->class, hd44780->dev_num);
	class_destroy(hd44780->class);
	cdev_del(&(hd44780->hd44780_cdev));
	unregister_chrdev_region(hd44780->dev_num, 1);

	printk(KERN_INFO "remove success\n");
	return;
}

static struct i2c_driver hd44780_driver = {
	.driver = {
		.name = "jmw_hd44780",
		.of_match_table = hd44780_ids,
	},
	.probe = hd44780_probe,
	.remove = hd44780_remove,
};

module_i2c_driver(hd44780_driver);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("JIN MINU");
MODULE_DESCRIPTION("HD44780+extension module driver");

