#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/gpio.h>

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

#define LCD_CLEARDISPLAY 0x1
#define LCD_RETURNHOME 0x2
#define LCD_FUNCTIONSET 0x28
#define LCD_DISPLAYON 0x0C
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
 */
static void lcd_send_nibble(struct i2c_client *client, u8 data, u8 mode) {
	u8 result_byte;
	
	result_byte = data | BL | E | RW | mode; // pulse한번 주고 
	i2c_lcd_write_byte(client, result_byte);

	udelay(10);

	result_byte = data | BL | RW | mode; // pulse 빼주고
	i2c_lcd_write_byte(client, result_byte);

	udelay(50);
}

/*
 * 상위 4비트와 하위 4비트를 보냄
 */
static void lcd_send_byte(struct client *client, u8 data, u8 mode) {
	lcd_send_nibble(client, data & 0xF0, mode); // 상위 4비트 보냄
	lcd_send_nibble(client, (data << 4), mode); // 하위 4비트 보냄
}

/*
 * lcd에 명령 전송-> 어떤 모드로 할지
 * ex) 4비트 모드-> 상위비트 0010 보냄
 */
static void lcd_write_cmd(struct i2c_client *client, u8 cmd) {
	lcd_send_byte(client, cmd, 0x00); // 0x00은 하위비트이므로 상관X
}

static void lcd_write_data(struct i2c_client *client, char data) {
	lcd_send_byte(client, data, RS);
}


