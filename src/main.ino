#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WS2812FX.h>
#include <PubSubClient.h>

const char *_ssid = WIFI_SSID;
const char *_password = WIFI_PASS;

const char *_mqtt_server = MQTT_SERVER;
const int _mqtt_port = MQTT_PORT;
const int _mqtt_buffer_size = JSON_OBJECT_SIZE(64);

char _id[12];
char _hostname[18] = HOSTNAME_PREFIX;

WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);
WiFiClient wifi;
PubSubClient mqtt(wifi);

// Maintained state for reporting to HA
byte red = 0;
byte green = 123;
byte blue = 255;
byte white = 255;

uint32_t getColorInt()
{
    return ((uint32_t)white << 24) | ((uint32_t)green << 16) | ((uint32_t)red << 8) | blue;
}

void mqttProcessJson(char *message)
{
    StaticJsonBuffer<_mqtt_buffer_size> jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject(message);

    if (!root.success())
    {
        return;
    }

    if (root.containsKey("state"))
    {
        if (strcmp(root["state"], "ON") == 0)
        {
            ws2812fx.start();
        }
        else if (strcmp(root["state"], "OFF") == 0)
        {
            ws2812fx.stop();
        }
    }

    if (root.containsKey("effect"))
    {
        ws2812fx.setMode(effectNumber(root["effect"]));
    }

    if (root.containsKey("color"))
    {
        red = root["color"]["r"];
        green = root["color"]["g"];
        blue = root["color"]["b"];
        ws2812fx.setColor(getColorInt());
    }

    if (root.containsKey("white_value"))
    {
        white = root["white_value"];
        ws2812fx.setColor(getColorInt());
    }

    if (root.containsKey("brightness"))
    {
        ws2812fx.setBrightness(root["brightness"]);
    }

    if (root.containsKey("speed"))
    {
        ws2812fx.setSpeed(root["speed"]);
    }
}

void mqttSendState()
{
    const char *_on_cmd = "ON";
    const char *_off_cmd = "OFF";
    String _mqtt_state_topic = "leds/" + String(_id) + "/state";

    StaticJsonBuffer<_mqtt_buffer_size> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    root["state"] = (ws2812fx.isRunning()) ? _on_cmd : _off_cmd;
    if (ws2812fx.isRunning())
    {
        root["white_value"] = white;
        root["brightness"] = ws2812fx.getBrightness();
        root["effect"] = ws2812fx.getModeName(ws2812fx.getMode());
        root["speed"] = ws2812fx.getSpeed();
        JsonObject &color = root.createNestedObject("color");
        color["r"] = red;
        color["g"] = green;
        color["b"] = blue;
        color.end();
    }
    root.end();

    char *buffer = new char[root.measureLength() + 1];
    root.printTo(buffer, root.measureLength() + 1);
 
    mqtt.publish(_mqtt_state_topic.c_str(), buffer);
    Serial.print("- Send State: ");
    Serial.println(buffer);
    delete buffer;
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    char message[length + 1];
    for (unsigned i = 0; i < length; i++)
    {
        message[i] = (char)payload[i];
    }
    message[length] = '\0';
    Serial.println(message);

    mqttProcessJson(message);
    mqttSendState();
}

void mqttConnect()
{
    const char *_on_avail = "Online";
    const char *_off_avail = "Offline";
    String _mqtt_avail_topic = "leds/" + String(_id) + "/lwt";
    String _mqtt_cmd_topic = "leds/" + String(_id) + "/cmd";

    if (mqtt.connect(_hostname, _mqtt_avail_topic.c_str(), 1, true, _off_avail))
    {
        Serial.printf("MQTT: connected as %s\r\n", _hostname);
        mqtt.publish(_mqtt_avail_topic.c_str(), _on_avail, true);
        mqtt.subscribe(_mqtt_cmd_topic.c_str());
        mqttSendState();
    }
    else
    {
        Serial.printf("failed, rc=%d \n", mqtt.state());
    }
}

int effectNumber(String effect)
{
    String effects[MODE_COUNT];
    for (uint8_t i = 0; i < ws2812fx.getModeCount(); i++)
    {
        effects[i] = ws2812fx.getModeName(i);
    }
    int effectNo = -1;
    for (int i = 0; i < MODE_COUNT; i++)
    {
        if (effect == effects[i])
        {
            effectNo = i;
            break;
        }
    }
    return effectNo;
}

void setupOTA()
{
    ArduinoOTA.onStart([]() {
        ws2812fx.stop();
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
        else // U_SPIFFS
            type = "filesystem";
        Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
            Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
            Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    Serial.println("OTA: ready");
}

void setupWifi()
{
    WiFi.hostname(_hostname);
    WiFi.begin(_ssid, _password);
    Serial.printf("Connecting to %s (%s) ", _ssid, _id);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(100);
        Serial.print(".");
    }
    Serial.print(" OK");
    Serial.println();
    Serial.print("IP address: ");
    Serial.print(WiFi.localIP());
    Serial.printf(" (%s)", _hostname);
    Serial.println();
}

void setupMQTT()
{
    mqtt.setServer(_mqtt_server, _mqtt_port);
    mqtt.setCallback(mqttCallback);
    mqttConnect();
}

void setupFX()
{
    ws2812fx.init();
    ws2812fx.setBrightness(66);
    ws2812fx.setSpeed(1024);
    ws2812fx.setColor(getColorInt());
    ws2812fx.setMode(FX_MODE_RAINBOW_CYCLE);
    Serial.println("FX: ready");
}

void setup()
{
    Serial.begin(115200);
    Serial.println();

    String mac = WiFi.macAddress();
    mac.replace(":", "");
    mac.toLowerCase();
    strncpy(_id, mac.c_str(), sizeof(_id));
    strncat(_hostname, _id, sizeof(_id));

    setupWifi();
    setupOTA();
    setupFX();
    setupMQTT();

    Serial.println();
}

void loop()
{
    ArduinoOTA.handle();
    mqtt.loop();
    ws2812fx.service();
    if (!mqtt.connected())
    {
        // TODO: Wait 5 seconds before retrying
        mqttConnect();
    }
}