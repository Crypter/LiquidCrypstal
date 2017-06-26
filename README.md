# LiquidCrypstal - LiquidCrystal_SPI fork, using solely the GPIO pins
This is a fork from https://playground.arduino.cc/Main/LiquidCrystalI
I've modified the library to get rid of the SPI part and I bitbang the data for the shiftregister on three generic GPIO pins.
I use it on my NodeMCU board along with the Arduino core for it.
