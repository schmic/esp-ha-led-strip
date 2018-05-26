## ESP8266 LED-Strip Firmware for Home-Assistant 

Attention, don't break the single-double-quotes for the defines in the platformio.ini!

### What you need
- ESP8266 (Wemos D1 Mini or similar)
- WS2812b based LED stripe
    - sufficient power supply, every led draws up to 60mA

### Home-Assistant Integration
Home-Assistants [mqtt_light-component](https://www.home-assistant.io/components/light.mqtt/) is used to control everything. Check the example-directory.

![Screeshot](/schmic/esp-ha-led-strip/raw/master/example/ha.png)