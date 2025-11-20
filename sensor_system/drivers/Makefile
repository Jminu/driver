obj-m += led_driver.o\
	 irq_btn_driver.o\
	 sht20_driver.o 
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
