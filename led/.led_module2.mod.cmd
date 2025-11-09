savedcmd_/project/driver_prac/led/led_module2.mod := printf '%s\n'   led_module2.o | awk '!x[$$0]++ { print("/project/driver_prac/led/"$$0) }' > /project/driver_prac/led/led_module2.mod
