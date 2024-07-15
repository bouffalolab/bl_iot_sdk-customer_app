# Pin definitions 

- BL706 runs as SPI master
  ```
  MISO = GPIO4
  MSOI = GPIO5
  CLOCK = GPIO3
  CS = GPIO6
  ```

- BL602 runs as SPI slave.
  ```
  MISO = GPIO0
  MSOI = GPIO1
  CLOCK = GPIO3
  CS = GPIO2  
  ```

- BL602 Power Supply

  Connect `PIN 3v3` and `PIN GND` to BL706 Board. 

# Prebuilt_images
  - [bl602_slave prebuilt image](../prebuilt_images/bl602_slave-15bfed04d.bin)

## Test

UART command lines to connect to a Wi-Fi AP
```shell
# connec to AP. 
# The assigned IPv4 address will print after it connects to AP.
wifi_connect <wifi_ssid> <wifi_password>
```