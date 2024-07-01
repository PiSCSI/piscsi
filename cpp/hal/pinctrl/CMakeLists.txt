cmake_minimum_required(VERSION 3.10...3.27)
include(GNUInstallDirs)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -pedantic")

#set project name
project(pinctrl)

#add executables
add_executable(pinctrl pinctrl.c gpiolib.c util.c gpiochip_bcm2835.c gpiochip_bcm2712.c gpiochip_rp1.c)
install(TARGETS pinctrl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES pinctrl-completion.bash RENAME pinctrl DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/bash-completion/completions")
