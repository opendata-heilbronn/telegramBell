/** Telegram Bell
 *
 * Author: LeoDJ
 *
 * This sketch implements a self-contained telegram bot that can ring a bell based on trigger words it detects.
 *
 * TODO: HTTP Endpoint, OTA
 *
 * Current problems: Due to the implementation of getUpdates in the UniversalTelegramBot library,
 * the reaction time is very limited and is prone to lock up the main loop for seconds.
 * Also, the library crashes if the message received is too big.
 */

#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>

#include "config.h"

/////////////////////////////////////
// User Settings
/////////////////////////////////////

String triggerWords[] = {"#ping", "unten aufmachen", "/klingel"};

#define BELL_PIN 2  // output pin
#define ON_TIME 200 // on-time of output pin in ms

// rate limiting on a per-user basis in ms, set to 0 to disable
#define RATE_LIMIT (5 * 60 * 1000)

#define POLL_INTERVAL 1000
#define RATE_LIMIT_MAX_USERS 32

/////////////////////////////////////
// END User Settings
/////////////////////////////////////

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

String rateLimitUserID[RATE_LIMIT_MAX_USERS];
unsigned long rateLimitUserTime[RATE_LIMIT_MAX_USERS];
byte rateLimitIdx = 0;

unsigned long lastPoll = 0; // last time messages' scan has been done
bool Start = false;
bool triggered = false;

bool checkRateLimit(String userId) {
    unsigned long now = millis();

    for (byte i = 0; i < RATE_LIMIT_MAX_USERS; i++) {
        // have to count down so the for loop always sees the newest entry first
        byte idx = (rateLimitIdx - i) % RATE_LIMIT_MAX_USERS;
        if (rateLimitUserID[idx] == userId) {
            return (now < rateLimitUserTime[idx] + RATE_LIMIT); // returns true if user is rate-limited
        }
    }
    return false;
}

void updateRateLimit(String userId, unsigned long time = millis()) {
    rateLimitUserID[rateLimitIdx] = userId;
    rateLimitUserTime[rateLimitIdx] = time;

    rateLimitIdx++;
    if (rateLimitIdx >= RATE_LIMIT_MAX_USERS)
        rateLimitIdx = 0;
}

void handleNewMessages(int numNewMessages) {
    for (int i = 0; i < numNewMessages; i++) {
        telegramMessage msg = bot.messages[i];
        String chat_id = String(msg.chat_id);
        String text = msg.text;
        Serial.println(chat_id + "  >  " + text);

        if (msg.chat_id == authorizedChatID) {
            if (text == "/help" || text == "/help@" + String(BOTname) || text == "/start" || text == "/start@" + String(BOTname)) {
                String s = "Ich plinge eine Glocke im CoWo, wenn ich ein Triggerwort höre. Die aktuellen Trigger sind:\n";
                for (byte i = 0; i < sizeof(triggerWords) / sizeof(triggerWords[0]); i++) {
                    s += triggerWords[i];
                    s += "\n";
                }
                bot.sendMessage(chat_id, s);
            }

            if (!checkRateLimit(msg.from_id)) { // check if user is rate limited
                triggered = false;
                text.toLowerCase();
                for (byte i = 0; i < sizeof(triggerWords) / sizeof(triggerWords[0]); i++) {
                    if (text.indexOf(triggerWords[i]) > -1) {
                        triggered = true;
                        Serial.println("TRIGGERED");
                        updateRateLimit(msg.from_id); // save that user has used the bell just now
                        break;
                    }
                }
            }
        }
    }
}

void initOTA() {
    // Hostname defaults to esp8266-[ChipID]
    ArduinoOTA.setHostname(HOSTNAME);

    // No authentication by default
    ArduinoOTA.setPassword(otaPassword);

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
        else // U_SPIFFS
            type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
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
}

void setup() {
    Serial.begin(115200);
    digitalWrite(BELL_PIN, HIGH); // low active, switch off
    pinMode(BELL_PIN, OUTPUT);

    WiFi.hostname(HOSTNAME);
    // Set WiFi to station mode and disconnect from an AP if it was previously connected
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    // attempt to connect to Wifi network:
    Serial.print("Connecting to: ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(200);
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    initOTA();

    // massively improves response time, but latency to telegram servers must not exceed this, or else no updates will be received
    bot.waitForResponse = 500;
}

unsigned long lastTrigger = 0;
bool messageReceived = false;
byte numNewMessages = 0;

void loop() {
    ArduinoOTA.handle();
    if (triggered && !messageReceived) {
        lastTrigger = millis();
        triggered = false;
        digitalWrite(BELL_PIN, LOW);
        delay(ON_TIME); // due to blocking behaviour of bot.getUpdates()
        digitalWrite(BELL_PIN, HIGH);
        lastPoll = millis(); // delay next poll to enable more accurate timing
    }
    /* //will be used again, once bot.getUpdates() is not as blocking
    if (millis() > lastTrigger + ON_TIME) { // todo only turn off once
        digitalWrite(BELL_PIN, HIGH);
    }*/

    // pause telegram polling, while sensitive pin timing is running
    if (millis() > lastTrigger + ON_TIME) {
        // This implementation of telegram polling does not lock up the whole code if more than one message arrives, but still needs much
        // time for the getUpdates() function
        if (millis() > lastPoll + POLL_INTERVAL || numNewMessages > 0) {
            numNewMessages = bot.getUpdates(bot.last_message_received + 1);
            if (numNewMessages > 0) {
                handleNewMessages(numNewMessages);
            }
        }
    }
}