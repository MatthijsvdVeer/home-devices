// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <DHT.h>
#include "src/iotc/common/string_buffer.h"
#include "src/iotc/iotc.h"

#define WIFI_SSID ""
#define WIFI_PASSWORD ""

#define DHTPIN D2
#define DHTTYPE DHT22
#define RELAYPIN D1
#define LDRPIN A0

const char *SCOPE_ID = "";
const char *DEVICE_ID = "";
const char *DEVICE_KEY = "";

DHT dht(DHTPIN, DHTTYPE);

int lamp = 0;

void on_event(IOTContext ctx, IOTCallbackInfo *callbackInfo);
#include "src/connection.h"

void on_event(IOTContext ctx, IOTCallbackInfo *callbackInfo)
{
  // ConnectionStatus
  if (strcmp(callbackInfo->eventName, "ConnectionStatus") == 0)
  {
    LOG_VERBOSE("Is connected ? %s (%d)",
                callbackInfo->statusCode == IOTC_CONNECTION_OK ? "YES" : "NO",
                callbackInfo->statusCode);
    isConnected = callbackInfo->statusCode == IOTC_CONNECTION_OK;
    return;
  }

  // payload buffer doesn't have a null ending.
  // add null ending in another buffer before print
  AzureIOT::StringBuffer buffer;
  if (callbackInfo->payloadLength > 0)
  {
    buffer.initialize(callbackInfo->payload, callbackInfo->payloadLength);
  }

  LOG_VERBOSE("- [%s] event was received. Payload => %s\n",
              callbackInfo->eventName, buffer.getLength() ? *buffer : "EMPTY");

  if (strcmp(callbackInfo->eventName, "Command") == 0)
  {
    LOG_VERBOSE("- Command name was => %s\r\n", callbackInfo->tag);
    if (strcmp(callbackInfo->tag, "lamp_on") == 0)
    {
      lamp = 1;
      return;
    }

    if (strcmp(callbackInfo->tag, "lamp_off") == 0)
    {
      lamp = 0;
      return;
    }
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(LDRPIN, INPUT); // Light sensor
  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(RELAYPIN, HIGH);
  dht.begin();

  connect_wifi(WIFI_SSID, WIFI_PASSWORD);
  connect_client(SCOPE_ID, DEVICE_ID, DEVICE_KEY);

  if (context != NULL)
  {
    lastTick = 0; // set timer in the past to enable first telemetry a.s.a.p
  }
}

void loop()
{
  digitalWrite(RELAYPIN, lamp == 1 ? LOW : HIGH);

  if (isConnected)
  {
    unsigned long ms = millis();
    if (ms - lastTick > 10000)
    { // send telemetry every 10 seconds

      int light = 1024 - analogRead(LDRPIN);
      float humidity = dht.readHumidity();
      float temperature = dht.readTemperature();
      float heatIndex = dht.computeHeatIndex(temperature, humidity, false);
      Serial.println(humidity);
      lastTick = ms;
      DynamicJsonDocument doc(300);

      // Telemetry
      doc["humidity"] = humidity;
      doc["temperature"] = temperature;
      doc["heatIndex"] = heatIndex;
      doc["light"] = light;
      doc["lampStatus"] = lamp;
      
      // State
      doc["lamp"] = lamp == 1 ? "on" : "off";

      int bufferLength = measureJson(doc) + 1;
      char buffer[bufferLength];
      serializeJson(doc, buffer, bufferLength);
      int errorCode = iotc_send_telemetry(context, buffer, bufferLength);

      if (errorCode != 0)
      {
        LOG_ERROR("Sending message has failed with error code %d", errorCode);
      }
    }

    iotc_do_work(context); // do background work for iotc
  }
  else
  {
    iotc_free_context(context);
    context = NULL;
    connect_client(SCOPE_ID, DEVICE_ID, DEVICE_KEY);
  }
}