#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/string.h>

#define LCD_WIDTH 160
#define LCD_HEIGHT 128

static void update_st7735_lcd(struct fb_info *info, struct list_head *pagelist);



static struct st7735_priv {
    struct spi_device *spi;
    struct gpio_desc *reset; // BCM 22
    struct gpio_desc *dc; // BCM 17
    struct gpio_desc *bl; // BCM 27

    //frame buffer
    struct fb_info *info;
    u8 *vmem;
};

static struct fb_ops st7735_fb_ops = {
    .owner      = THIS_MODULE,
    .fb_read    = fb_sys_read,   // 표준 읽기
    .fb_write   = fb_sys_write,  // 표준 쓰기 (vmem에 씀)
    .fb_fillrect= cfb_fillrect,  // cfb: vmem에 사각형 그리기
    .fb_copyarea= cfb_copyarea,  // cfb: vmem 영역 복사
    .fb_imageblit = cfb_imageblit, // cfb: vmem에 이미지 그리기
};

/* 타이머 설정 */
static struct fb_deferred_io st7735_defio = {
    .delay          = HZ / 30, // 30 FPS (초당 30번)
    .deferred_io    = update_st7735_lcd, // 4-2 함수를 호출
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
    struct device *dev = &spi->dev;
    struct st7735_priv *priv;
    struct fb_info *info;
    int ret;
    printk(KERN_INFO "probe function called\n");

    info == framebuffer_alloc(sizeof(struct st7735_priv), dev);
    priv = info->par;
    priv->info = info;
    priv->spi = spi;

    // gpio 가져오기
    priv->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
    priv->dc = devm_gpiod_get(dev, "dc", GPIOD_OUT_HIGH);
    priv->bl = devm_gpiod_get(dev, "lcd", GPIOD_OUT_HIGH);

    spi_set_drvdata(spi, priv);
    gpiod_set_value(priv->reset, 0);
    mdelay(100);
    gpiod_set_value(priv->reset, 1);
    mdelay(100);

    st7735_write_cmd(priv, 0x01); // SWRESET
    mdelay(100); 
    st7735_write_cmd(priv, 0x11); // SLPOUT
    mdelay(100);

    st7735_write_cmd(priv, 0x3A);
    u8 colmod[] = {0x05};
    st7735_write_data(priv, colmod, 1);

    // frame buffer 등록
    int vmem_size = LCD_WIDTH * LCD_HEIGHT * 2;
    priv->vmem = vzalloc(vmem_size); // ram 공간 할당
    
    info->screen_base = (char __iomem *)priv->vmem;
    info->fix.smem_start = (unsigned long)priv->vmem;
    info->fix.smem_len = vmem_size;

    strscpy(info->fix.id, "st7735_custom", sizeof(info->fix.id));
    info->fix.type = FB_TYPE_PACKED_PIXELS;
    info->fix.visual = FB_VISUAL_TRUECOLOR;
    info->fix.line_length = LCD_WIDTH * 2; // 한 줄의 바이트 수
    info->var.xres = LCD_WIDTH; info->var.yres = LCD_HEIGHT;
    info->var.xres_virtual = LCD_WIDTH;
    info->var.yres_virtual = LCD_HEIGHT;
    info->var.bits_per_pixel = 16;
    info->var.red.offset = 11; info->var.red.length = 5;     // R(5)
    info->var.green.offset = 5; info->var.green.length = 6;  // G(6)
    info->var.blue.offset = 0; info->var.blue.length = 5;     // B(5) => RGB565

    // 4-3. 콜백(fb_ops) 및 타이머(deferred_io) 연결
    info->fbops = &st7735_fb_ops;
    info->fbdefio = &st7735_defio;
    fb_deferred_io_init(info);

    ret = register_framebuffer(info);
    if (ret < 0) {
        pr_err("st7735_custom: Failed to register framebuffer (err %d)\n", ret);
        return -1;
    }

    return 0;
}

// spi드라이버를 디바이스에 언바인딩
static void st7735_custom_remove(struct spi_device *spi) {
    struct st7735_priv *priv = spi_get_drvdata(spi);
    gpiod_set_value(priv->bl, 0); // 백라이트 끄기
    fb_deferred_io_cleanup(priv->info);
    unregister_framebuffer(priv->info);
    vfree(priv->vmem); // "가짜 캔버스" 메모리 해제
    framebuffer_release(priv->info); // "신청서" 메모리 해제

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



// lcd에 그릴 영역 설정
static void st7735_set_addr_window(struct st7735_priv *priv, int x, int y, int w, int h) {
    u8 caset_data[4];
    u8 raset_data[4];

    int x_end = x + w - 1;
    int y_end = y + h - 1;

    st7735_write_cmd(priv, 0x2A); // CASET: 열 주소를 가져옴
    caset_data[0] = (x >> 8) & 0xFF; // 8칸 오른쪽으로 밀고 (1111 1111)하고 and연산: 상위 8비트
    caset_data[1] = x & 0xFF; // 하위 8비트
    caset_data[2] = (x_end >> 8) & 0xFF;
    caset_data[3] = x_end & 0xFF;
    st7735_write_data(priv, caset_data, 4);

    st7735_write_cmd(priv, 0x2B); //RASET: 행 주소 가져옴
    raset_data[0] = (y >> 8) & 0xFF;
    raset_data[1] = y & 0xFF;
    raset_data[2] = (y_end >> 8) & 0xFF;
    raset_data[3] = y_end & 0xFF;
    st7735_write_data(priv, raset_data, 4);
}

static void update_st7735_lcd(struct fb_info *info, struct list_head *pagelist) {
    struct st7735_priv *priv = info->par;

    /* LCD의 전체 화면을 주소창으로 설정 */
    st7735_set_addr_window(priv, 0, 0, info->var.xres, info->var.yres);
    
    /* LCD 메모리에 쓰기 시작 */
    st7735_write_cmd(priv, 0x2C); // RAMWR: lcd memory write

    /* vmem의 모든 픽셀 데이터를 SPI로 전송 */
    gpiod_set_value(priv->dc, 1); // Data 모드
    spi_write(priv->spi, priv->vmem, info->fix.smem_len);
}

module_spi_driver(st7735_custom_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JIN MINU");