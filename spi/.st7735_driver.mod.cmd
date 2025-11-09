savedcmd_/project/driver_prac/spi/st7735_driver.mod := printf '%s\n'   st7735_driver.o | awk '!x[$$0]++ { print("/project/driver_prac/spi/"$$0) }' > /project/driver_prac/spi/st7735_driver.mod
