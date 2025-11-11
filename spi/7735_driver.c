#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>

static struct st7735_priv {
    struct spi_device *spi;
    struct gpio_desc *reset; // BCM 22
    struct gpio_desc *dc; // BCM 17
    struct gpio_desc *bl; // BCM 27
};

// spi 통신 함수
// dc가 low : command byte
// dc가 high : as a parameter
static void st7735_write_cmd(struct st7735_priv *priv, unsigned char cmd) {
    gpiod_set_value(priv->dc, 0); // 0으로 설정 command 모드로 설정
    spi_write(priv->spi, &cmd, 1); // @priv->spi: 데이터를 쓸 디바이스
                                  // @cmd: data buffer
                                  // @1: data buffer size
}

static void st7735_write_data(struct st7735_priv *priv, const char *buf, size_t len) {
    gpiod_set_value(priv->dc, 1); // 1로 설정 data write 모드로 설정
    spi_write(priv->spi, buf, len); // 쓰기모드에서 buf데이터를 len크기만큼 write
}

// spi드라이버를 디바이스에 바인딩
static int st7735_custom_probe(struct spi_device *spi) {
    struct st7735_priv *priv;
    int ret;

    priv = devm_kzalloc(&(spi->dev), sizeof(struct st7735_priv), GFP_KERNEL); // spi디바이스를 위한 메모리할당
    priv->spi = spi;

    // gpio핀 가져와서 priv에 저장
    priv->reset = devm_gpiod_get(&(spi->dev), "reset", GPIOD_OUT_HIGH);
    priv->dc = devm_gpiod_get(&(spi->dev), "dc", GPIOD_OUT_HIGH);
    priv->bl = devm_gpiod_get(&(spi->dev), "bl", GPIOD_OUT_HIGH);

    spi_set_drvdata(spi, priv);

    printk(KERN_INFO "Probe func success: reset=%d dc=%d bl=%d\n", desc_to_gpio(priv->reset), desc_to_gpio(priv->dc), desc_to_gpio(priv->bl));

    // reset 0, 1 토글
    gpiod_set_value(priv->reset, 0);
    msleep(10);
    gpiod_set_value(priv->reset, 1);
    msleep(100);

    // back light 토글(테스트)
    gpiod_set_value(priv->bl, 0);
    msleep(100);
    gpiod_set_value(priv->bl, 1);
    msleep(100);
    gpiod_set_value(priv->bl, 0);
    msleep(100);
    gpiod_set_value(priv->bl, 1);
    msleep(100);

    // software reset
    st7735_write_cmd(priv, 0x1); // 0x1은 software reset command

    // sleep mode 해제
    st7735_write_cmd(priv, 0x11);

    // turn on backlight
    gpiod_set_value(priv->bl, 1);
    printk(KERN_INFO "Backlight On\n");

    return 0;
}

// spi드라이버를 디바이스에 언바인딩
static void st7735_custom_remove(struct spi_device *spi) {
    printk(KERN_INFO "Remove func success\n");
}

static const struct of_device_id st7735_custom_id[] = {
    {.compatible = "my-custom,st7735"}, // .dts하고 일치
    {},
};
MODULE_DEVICE_TABLE(of, st7735_custom_id);

static struct spi_driver st7735_custom_driver = {
    .driver = {
        .name = "st7735_custom",
        .of_match_table = st7735_custom_id,
    },
    .probe = st7735_custom_probe,
    .remove = st7735_custom_remove,
};

module_spi_driver(st7735_custom_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JIN MINU");