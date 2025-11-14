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
#include <asm/pgtable.h> // remap_pfn_range 용
#include <asm/io.h>
#include <linux/mm.h>    // vmalloc_to_pfn 용

#define LCD_WIDTH 128
#define LCD_HEIGHT 160
#define X_OFFSET 2
#define Y_OFFSET 1

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

static int st7735_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
    struct st7735_priv *priv = info->par;
    unsigned long start = vma->vm_start;
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long pfn;
    
    /* 1. 요청 크기 검증 */
    if (vma->vm_pgoff != 0)
        return -EINVAL; // 오프셋이 없어야 합니다.

    if (size > info->fix.smem_len)
        return -EINVAL; // 매핑 요청 크기가 vmem 크기보다 큽니다.

    /* 2. vmalloc 주소의 물리 페이지 프레임 번호(PFN)를 얻습니다. */
    pfn = vmalloc_to_pfn(priv->vmem);

    /* 3. VMA 플래그 설정 */
    vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;

    /* 4. 물리 페이지를 유저스페이스 가상 주소로 매핑합니다. */
    if (remap_pfn_range(vma, start, pfn, size, vma->vm_page_prot))
        return -EAGAIN; // 재시도 요청

    return 0;
}

static struct fb_ops st7735_fb_ops = {
    .owner      = THIS_MODULE,
    .fb_read    = fb_sys_read,   // 표준 읽기
    .fb_write   = fb_sys_write,  // 표준 쓰기 (vmem에 씀)
    .fb_fillrect= cfb_fillrect,  // cfb: vmem에 사각형 그리기
    .fb_copyarea= cfb_copyarea,  // cfb: vmem 영역 복사
    .fb_imageblit = cfb_imageblit, // cfb: vmem에 이미지 그리기
    .fb_mmap = st7735_fb_mmap,
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

static void st7735_hw_init(struct st7735_priv *priv)
{
    /* 1. 물리적 리셋 */
    gpiod_set_value(priv->reset, 0); mdelay(10);
    gpiod_set_value(priv->reset, 1); mdelay(120);

    /* 2. 소프트웨어 리셋 (0x01) */
    st7735_write_cmd(priv, 0x01); 
    mdelay(150); 

    /* 3. 슬립 아웃 (0x11) */
    st7735_write_cmd(priv, 0x11); 
    mdelay(500);

    st7735_write_cmd(priv, 0x38); // IDMOFF (Idle Mode Off)
    mdelay(10);

    /* 4. 프레임 레이트 제어 (0xB1) - 필수! */
    st7735_write_cmd(priv, 0xB1); // FRMCTR1
    u8 frmctr1[] = { 0x01, 0x2C, 0x2D };
    st7735_write_data(priv, frmctr1, 3);
    
    st7735_write_cmd(priv, 0xB2); // FRMCTR2 (idle mode)
    u8 frmctr2[] = { 0x01, 0x2C, 0x2D };
    st7735_write_data(priv, frmctr2, 3);
    
    st7735_write_cmd(priv, 0xB3); // FRMCTR3 (partial mode)
    u8 frmctr3[] = { 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D };
    st7735_write_data(priv, frmctr3, 6);

    /* 5. 디스플레이 인버전 제어 (0xB4) */
    st7735_write_cmd(priv, 0xB4); // INVCTR
    u8 invctr[] = { 0x07 };
    st7735_write_data(priv, invctr, 1);
    
    /* 6. 파워 컨트롤 1 (0xC0) - 필수! 내부 전압 설정 */
    st7735_write_cmd(priv, 0xC0); // PWCTR1
    u8 pwctr1[] = { 0xA2, 0x02, 0x84 };
    st7735_write_data(priv, pwctr1, 3);
    
    /* 7. 파워 컨트롤 2 (0xC1) */
    st7735_write_cmd(priv, 0xC1); // PWCTR2
    u8 pwctr2[] = { 0xC5 };
    st7735_write_data(priv, pwctr2, 1);
    
    /* 8. VCOM 제어 1 (0xC5) - 필수! 공통 전극 전압 설정 */
    st7735_write_cmd(priv, 0xC5); // VMCTR1
    u8 vmctr1[] = { 0x0E };
    st7735_write_data(priv, vmctr1, 1);

    /* 9. 픽셀 포맷 (0x3A) */
    st7735_write_cmd(priv, 0x3A); // COLMOD
    u8 colmod[] = { 0x05 }; // 0x05 = 16bpp (RGB565)
    st7735_write_data(priv, colmod, 1);

    /* 10. 메모리 접근 제어 (0x36) - 노이즈/방향 해결용 */
    st7735_write_cmd(priv, 0x36); // MADCTL
    u8 madctl[] = { 0xC8 }; // (BGR 비트 = 0)
    st7735_write_data(priv, madctl, 1);

    st7735_write_cmd(priv, 0x28); // DISPOFF (화면을 끈 상태로 설정)
    st7735_write_cmd(priv, 0x29); // DISPON (화면을 켬)
    mdelay(100);

    /* 12. 백라이트 ON (모든 준비 완료) */
    gpiod_set_value(priv->bl, 1);
    pr_info("st7735_custom: Backlight ON (Full Init)\n");
}

// spi드라이버를 디바이스에 바인딩
static int st7735_custom_probe(struct spi_device *spi) {
    struct device *dev = &spi->dev;
    struct st7735_priv *priv;
    struct fb_info *info;
    int ret;
    printk(KERN_INFO "probe function called\n");

    info = framebuffer_alloc(sizeof(struct st7735_priv), dev);
    if (info == NULL) {
        printk(KERN_ERR "framebuffer alloc fail\n");
        return -1;
    }

    priv = info->par;
    priv->info = info;
    priv->spi = spi;

    // gpio 가져오기
    priv->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
    priv->dc = devm_gpiod_get(dev, "dc", GPIOD_OUT_HIGH);
    priv->bl = devm_gpiod_get(dev, "bl", GPIOD_OUT_HIGH);

    spi_set_drvdata(spi, priv);

    st7735_hw_init(priv);

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
    
    info->pseudo_palette = devm_kmalloc_array(dev, 16, sizeof(u32), GFP_KERNEL);
    if (info->pseudo_palette == NULL) {
        printk(KERN_ERR "pseudo_palette alloc err\n");
        return -1;
    }

    gpiod_set_value(priv->bl, 1); // 백라이트 키기

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

    int x_start = x + X_OFFSET;
    int y_start = y + Y_OFFSET;
    int x_end = x + w - 1 + X_OFFSET;
    int y_end = y + h - 1 + Y_OFFSET;

    st7735_write_cmd(priv, 0x2A); // CASET
    caset_data[0] = (x_start >> 8) & 0xFF;
    caset_data[1] = x_start & 0xFF;
    caset_data[2] = (x_end >> 8) & 0xFF;
    caset_data[3] = x_end & 0xFF;
    st7735_write_data(priv, caset_data, 4);

    st7735_write_cmd(priv, 0x2B); //RASET
    raset_data[0] = (y_start >> 8) & 0xFF;
    raset_data[1] = y_start & 0xFF;
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