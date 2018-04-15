#include <ESP8266WiFi.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>

#include "config.h"

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

#define POLL_INTERVAL 1000

unsigned long lastPoll = 0; // last time messages' scan has been done
bool Start = false;

#define BELL_PIN 2
#define ON_TIME 100 // on-time in ms

bool triggered = false;

String triggerWords[] = {"#ping", "unten aufmachen", "/klingel"};

void handleNewMessages(int numNewMessages) {
    for (int i = 0; i < numNewMessages; i++) {
        telegramMessage msg = bot.messages[i];
        String chat_id = String(msg.chat_id);
        String text = msg.text;
        Serial.println(chat_id + "  >  " + text);

        String from_name = bot.messages[i].from_name;
        if (from_name == "")
            from_name = "Guest";

        if (text == "/help" || text == "/help@" + String(BOTname) || text == "/start" || text == "/start@" + String(BOTname)) {
            String s = "Ich plinge eine Glocke im CoWo, wenn ich ein Triggerwort höre. Die aktuellen Trigger sind:\n";
            for (byte i = 0; i < sizeof(triggerWords) / sizeof(triggerWords[0]); i++) {
                s += triggerWords[i];
                s += "\n";
            }
            bot.sendMessage(chat_id, s);
        }

        triggered = false;
        text.toLowerCase();
        for (byte i = 0; i < sizeof(triggerWords) / sizeof(triggerWords[0]); i++) {
            if (text.indexOf(triggerWords[i]) > -1) {
                triggered = true;
                break;
            }
        }
        if (triggered) {
            Serial.println("TRIGGERED");
        }
    }
}

void setup() {
    Serial.begin(115200);
    digitalWrite(BELL_PIN, HIGH); // low active, switch off
    pinMode(BELL_PIN, OUTPUT);

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
}

unsigned long lastTrigger = 0;
bool moreMessages = false;

void loop() {
    if (millis() > lastPoll + POLL_INTERVAL || moreMessages) {
        int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        if (numNewMessages > 0) {
            handleNewMessages(numNewMessages);
            numNewMessages = bot.getUpdates(bot.last_message_received + 1);
            moreMessages = (numNewMessages > 0);
        }
        lastPoll = millis();
    }
    // Serial.println(millis());
    if (triggered && !moreMessages) {
        lastTrigger = millis();
        triggered = false;
        digitalWrite(BELL_PIN, LOW);
        // lastPoll = millis(); // delay next poll to enable more accurate timing
    }
    if (millis() > lastTrigger + ON_TIME) { // todo only turn off once
        digitalWrite(BELL_PIN, HIGH);
    }
}