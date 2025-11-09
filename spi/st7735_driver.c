/*
 * st7735_driver.c (v5 - pseudo_palette Fix)
 * - compatible = "my-custom,st7735"
 * - GPIOs: reset=22, dc=17, bl=27
 */
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/string.h>

/* (ST7735 정의, priv 구조체, 헬퍼 함수들은 v4와 동일) */
#define ST7735_WIDTH  128
#define ST7735_HEIGHT 160
#define ST7735_CMD_SWRESET 0x01
#define ST7735_CMD_SLPOUT  0x11
#define ST7735_CMD_DISPON  0x29
#define ST7735_CMD_CASET   0x2A
#define ST7735_CMD_RASET   0x2B
#define ST7735_CMD_RAMWR   0x2C
#define ST7735_CMD_MADCTL  0x36
#define ST7735_CMD_COLMOD  0x3A

#define ST7735_X_OFFSET 2
#define ST7735_Y_OFFSET 1

struct st7735_priv {
    struct spi_device *spi;
    struct gpio_desc *reset;
    struct gpio_desc *dc;
    struct gpio_desc *bl;
    struct fb_info *info;
    u8 *vmem;
};

static void st7735_write_cmd(struct st7735_priv *priv, u8 cmd) {
    if (!priv || !priv->dc) return;
    gpiod_set_value(priv->dc, 0);
    spi_write(priv->spi, &cmd, 1);
}

static void st7735_write_data(struct st7735_priv *priv, u8 *data, int len) {
    if (!priv || !priv->dc) return;
    gpiod_set_value(priv->dc, 1);
    spi_write(priv->spi, data, len);
}

static void st7735_hw_init(struct st7735_priv *priv) {
    gpiod_set_value(priv->reset, 0); mdelay(10);
    gpiod_set_value(priv->reset, 1); mdelay(120);
    st7735_write_cmd(priv, ST7735_CMD_SWRESET); mdelay(150);
    st7735_write_cmd(priv, ST7735_CMD_SLPOUT); mdelay(500);
    st7735_write_cmd(priv, ST7735_CMD_COLMOD);
    u8 colmod[] = { 0x05 }; // 16bpp
    st7735_write_data(priv, colmod, 1);
    st7735_write_cmd(priv, ST7735_CMD_MADCTL);
    u8 madctl[] = { 0xC0 }; // MY=1, MX=1, MV=0, BGR=1
    st7735_write_data(priv, madctl, 1);
    st7735_write_cmd(priv, ST7735_CMD_DISPON); mdelay(100);
    gpiod_set_value(priv->bl, 1);
    pr_info("st7735_custom: Backlight ON\n");
}

static void st7735_set_addr_window(struct st7735_priv *priv, int x, int y, int w, int h)
{
    u8 caset_data[4];
    u8 raset_data[4];
    
    /* 오프셋을 적용한 시작/종료 주소 계산 */
    int x_start = x + ST7735_X_OFFSET;
    int y_start = y + ST7735_Y_OFFSET;
    int x_end = x + w - 1 + ST7735_X_OFFSET;
    int y_end = y + h - 1 + ST7735_Y_OFFSET;

    // Column (X) 설정
    st7735_write_cmd(priv, ST7735_CMD_CASET);
    caset_data[0] = (x_start >> 8) & 0xFF; // 상위 바이트 (0x00)
    caset_data[1] = x_start & 0xFF;        // 하위 바이트 (x + 2)
    caset_data[2] = (x_end >> 8) & 0xFF;   // 상위 바이트 (0x00)
    caset_data[3] = x_end & 0xFF;          // 하위 바이트 (x + w - 1 + 2)
    st7735_write_data(priv, caset_data, 4);

    // Row (Y) 설정
    st7735_write_cmd(priv, ST7735_CMD_RASET);
    raset_data[0] = (y_start >> 8) & 0xFF; // 상위 바이트 (0x00)
    raset_data[1] = y_start & 0xFF;        // 하위 바이트 (y + 1)
    raset_data[2] = (y_end >> 8) & 0xFF;   // 상위 바이트 (0x00)
    raset_data[3] = y_end & 0xFF;          // 하위 바이트 (y + h - 1 + 1)
    st7735_write_data(priv, raset_data, 4);
}
static void st7735_deferred_update(struct fb_info *info, struct list_head *pagelist) {
    struct st7735_priv *priv = info->par;
    if (!priv || !priv->dc || !priv->vmem) return;
    st7735_set_addr_window(priv, 0, 0, info->var.xres, info->var.yres);
    st7735_write_cmd(priv, ST7735_CMD_RAMWR);
    gpiod_set_value(priv->dc, 1);
    spi_write(priv->spi, priv->vmem, info->fix.smem_len);
}

static struct fb_ops st7735_fb_ops = {
    .owner      = THIS_MODULE,
    .fb_read    = fb_sys_read,
    .fb_write   = fb_sys_write,
    .fb_fillrect= cfb_fillrect,
    .fb_copyarea= cfb_copyarea,
    .fb_imageblit = cfb_imageblit,
};

static struct fb_deferred_io st7735_defio = {
    .delay          = HZ / 30,
    .deferred_io    = st7735_deferred_update,
};

/* --- 5. Probe (핵심) / Remove 함수 --- */

static int st7735_custom_probe(struct spi_device *spi)
{
    struct device *dev = &spi->dev;
    struct st7735_priv *priv;
    struct fb_info *info;
    int ret;

    pr_info("st7735_custom: ⭐️ Probe function called! ⭐️\n");

    // 1. 프레임버퍼(info)와 private(priv) 데이터 할당
    info = framebuffer_alloc(sizeof(struct st7735_priv), dev);
    if (!info) {
        pr_err("st7735_custom: Failed to allocate framebuffer\n");
        return -ENOMEM;
    }
    priv = info->par;
    priv->info = info;
    priv->spi = spi;
    
    // 2. 디바이스 트리에서 GPIO 핀 가져오기
    priv->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(priv->reset)) {
        ret = PTR_ERR(priv->reset); pr_err("st7735_custom: Failed to get reset GPIO\n");
        goto err_release_fb;
    }
    priv->dc = devm_gpiod_get(dev, "dc", GPIOD_OUT_HIGH);
    if (IS_ERR(priv->dc)) {
        ret = PTR_ERR(priv->dc); pr_err("st7735_custom: Failed to get dc GPIO\n");
        goto err_release_fb;
    }
    priv->bl = devm_gpiod_get(dev, "bl", GPIOD_OUT_HIGH);
    if (IS_ERR(priv->bl)) {
        ret = PTR_ERR(priv->bl); pr_err("st7735_custom: Failed to get backlight GPIO\n");
        goto err_release_fb;
    }

    // 3. 비디오 메모리(vmem) 할당
    int vmem_size = ST7735_WIDTH * ST7735_HEIGHT * 2;
    priv->vmem = vzalloc(vmem_size);
    if (!priv->vmem) {
        ret = -ENOMEM; goto err_release_fb;
    }
    info->screen_base = (char __iomem *)priv->vmem;
    info->fix.smem_start = (unsigned long)priv->vmem;
    info->fix.smem_len = vmem_size;

    // 4. 프레임버퍼 고정/가변 정보 설정
    strscpy(info->fix.id, "st7735_custom", sizeof(info->fix.id));
    info->fix.type = FB_TYPE_PACKED_PIXELS;
    info->fix.visual = FB_VISUAL_TRUECOLOR;
    info->fix.line_length = ST7735_WIDTH * 2;
    info->var.xres = ST7735_WIDTH; info->var.yres = ST7735_HEIGHT;
    info->var.xres_virtual = ST7735_WIDTH; info->var.yres_virtual = ST7735_HEIGHT;
    info->var.bits_per_pixel = 16;
    info->var.red.offset = 11; info->var.red.length = 5;
    info->var.green.offset = 5; info->var.green.length = 6;
    info->var.blue.offset = 0; info->var.blue.length = 5;
    
    // 5. 프레임버퍼 콜백(fb_ops) 및 deferred_io 설정
    info->fbops = &st7735_fb_ops;
    info->fbdefio = &st7735_defio;
    fb_deferred_io_init(info);

    /* * ⭐️⭐️⭐️ FIX ⭐️⭐️⭐️
     * fbcon이 TRUECOLOR 모드에서 커서를 그릴 때 info->pseudo_palette를 참조합니다.
     * 16개 엔트리(u32)를 할당해 주어야 NULL 참조 오류가 발생하지 않습니다.
     * devm_ (device-managed) 함수로 할당했으므로 remove에서 따로 해제할 필요 없습니다.
     */
    info->pseudo_palette = devm_kmalloc_array(dev, 16, sizeof(u32), GFP_KERNEL);
    if (!info->pseudo_palette) {
        ret = -ENOMEM;
        pr_err("st7735_custom: Failed to allocate pseudo_palette\n");
        goto err_cleanup_defio; // ⭐️ 에러 처리 레이블 수정
    }

    // 6. SPI 장치에 private 데이터 저장
    spi_set_drvdata(spi, priv);

    // 7. 하드웨어 초기화
    st7735_hw_init(priv);

    // 8. 프레임버퍼 시스템에 등록 (여기서 fbcon이 호출됨)
    ret = register_framebuffer(info);
    if (ret < 0) {
        pr_err("st7735_custom: Failed to register framebuffer (err %d)\n", ret);
        goto err_cleanup_defio; // ⭐️ 에러 처리 레이블 수정
    }

    pr_info("st7735_custom: Probe successful! Framebuffer registered as /dev/fb%d\n", info->node);
    return 0;

/* ⭐️ 에러 처리 레이블 순서 수정 ⭐️ */
err_cleanup_defio:
    fb_deferred_io_cleanup(info);
err_free_vmem: // (defio cleanup 후 vmem free)
    vfree(priv->vmem);
err_release_fb:
    framebuffer_release(info);
    return ret;
}

static void st7735_custom_remove(struct spi_device *spi)
{
    struct st7735_priv *priv = spi_get_drvdata(spi);
    if (priv) {
        gpiod_set_value(priv->bl, 0);
        fb_deferred_io_cleanup(priv->info);
        unregister_framebuffer(priv->info);
        vfree(priv->vmem);
        framebuffer_release(priv->info);
    }
    pr_info("st7735_custom: Remove function called. Driver unloaded.\n");
}

/* --- 6. 드라이버 등록 --- */

/* (선택 사항) spi_device_id 테이블. 로그 경고 메시지 제거용. */
static const struct spi_device_id st7735_custom_spi_id[] = {
    { .name = "my-custom,st7735" },
    { }
};
MODULE_DEVICE_TABLE(spi, st7735_custom_spi_id);

/* 디바이스 트리 매칭 테이블 (dts의 compatible과 일치) */
static const struct of_device_id st7735_custom_of_match[] = {
    { .compatible = "my-custom,st7735" },
    { },
};
MODULE_DEVICE_TABLE(of, st7735_custom_of_match);

/* SPI 드라이버 구조체 */
static struct spi_driver st7735_custom_spi_driver = {
    .id_table = st7735_custom_spi_id, // ⭐️ 경고 제거용 id_table 추가
    .driver = {
        .name = "st7735_custom",
        .of_match_table = st7735_custom_of_match,
    },
    .probe = st7735_custom_probe,
    .remove = st7735_custom_remove,
};

module_spi_driver(st7735_custom_spi_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Custom ST7735 SPI Driver with Framebuffer");
