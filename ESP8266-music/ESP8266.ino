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

#define SPEAKERPIN D8

const char *SCOPE_ID = "";
const char *DEVICE_ID = "";
const char *DEVICE_KEY = "";

int music = 0;

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
    if (strcmp(callbackInfo->tag, "music_on") == 0)
    {
      music = 1;
      return;
    }

    if (strcmp(callbackInfo->tag, "music_off") == 0)
    {
      music = 0;
      return;
    }
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(SPEAKERPIN, OUTPUT);
  connect_wifi(WIFI_SSID, WIFI_PASSWORD);
  connect_client(SCOPE_ID, DEVICE_ID, DEVICE_KEY);

  if (context != NULL)
  {
    lastTick = 0; // set timer in the past to enable first telemetry a.s.a.p
  }
}

int length = 70;
String notes[] = {"G4", "G4", "G4", "D#4/Eb4", "A#4/Bb4", "G4", "D#4/Eb4", "A#4/Bb4", "G4", "D5", "D5", "D5", "D#5/Eb5", "A#4/Bb4", "F#4/Gb4", "D#4/Eb4", "A#4/Bb4", "G4", "G5", "G4", "G4", "G5", "F#5/Gb5", "F5", "E5", "D#5/Eb5", "E5", "rest", "G4", "rest", "C#5/Db5", "C5", "B4", "A#4/Bb4", "A4", "A#4/Bb4", "rest", "D#4/Eb4", "rest", "F#4/Gb4", "D#4/Eb4", "A#4/Bb4", "G4", "D#4/Eb4", "A#4/Bb4", "G4"};
int beats[] = {8, 8, 8, 6, 2, 8, 6, 2, 16, 8, 8, 8, 6, 2, 8, 6, 2, 16, 8, 6, 2, 8, 6, 2, 2, 2, 2, 6, 2, 2, 8, 6, 2, 2, 2, 2, 6, 2, 2, 9, 6, 2, 8, 6, 2, 16};
int tempo = 50;

void playTone(int tone, int duration)
{
  for (long i = 0; i < duration * 1000L; i += tone * 2)
  {
    digitalWrite(SPEAKERPIN, HIGH);
    delayMicroseconds(tone);
    digitalWrite(SPEAKERPIN, LOW);
    delayMicroseconds(tone);
  }
}

void playNote(String note, int duration)
{
  String noteNames[] = {"D#4/Eb4", "E4", "F4", "F#4/Gb4", "G4", "G#4/Ab4", "A4", "A#4/Bb4", "B4", "C5", "C#5/Db5", "D5", "D#5/Eb5", "E5", "F5", "F#5/Gb5", "G5", "G#5/Ab5", "A5", "A#5/Bb5", "B5", "C6", "C#6/Db6", "D6", "D#6/Eb6", "E6", "F6", "F#6/Gb6", "G6"};
  int tones[] = {1607, 1516, 1431, 1351, 1275, 1203, 1136, 1072, 1012, 955, 901, 851, 803, 758, 715, 675, 637, 601, 568, 536, 506, 477, 450, 425, 401, 379, 357, 337, 318};
  for (int i = 0; i < 29; i++)
  {
    if (noteNames[i] == note)
    {
      playTone(tones[i], duration);
    }
  }
}

void loop()
{
  if (music == 1)
  {
    for (int i = 0; i < length; i++) {
      if (notes[i] == "rest") {
        delay(beats[i] * tempo);
      } else {
        playNote(notes[i], beats[i] * tempo);      
      }
      delay(tempo / 2);
    }
  }

  if (isConnected)
  {
    unsigned long ms = millis();
    if (ms - lastTick > 10000)
    { // send telemetry every 10 seconds

      lastTick = ms;
      DynamicJsonDocument doc(300);

      // State
      doc["playing"] = music == 1 ? "on" : "off";

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