#include <Arduino.h>
#include <ArduinoJson.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <SimpleDHT.h>
#include <WiFiManager.h>

#include <ESP8266WiFi.h>
#include <UniversalTelegramBot.h>

#include <config.h>

//// ###### User configuration space for AC library classes ##########

#include <ir_Tcl.h>  //  replace library based on your AC unit model, check https://github.com/crankyoldgit/IRremoteESP8266

#define AUTO_MODE kTcl112AcAuto
#define COOL_MODE kTcl112AcCool
#define DRY_MODE kTcl112AcDry
#define HEAT_MODE kTcl112AcHeat
#define FAN_MODE kTcl112AcFan

#define FAN_AUTO kTcl112AcFanAuto
#define FAN_LOW kTcl112AcFanLow
#define FAN_MED kTcl112AcFanMed
#define FAN_HIGH kTcl112AcFanHigh

#define SWING_OFF kTcl112AcSwingVOff
#define SWING_ON kTcl112AcSwingVOn

std::string mode_to_str(uint8_t mode)
{
  switch (mode)
  {
  case 8:
    return "<b>AUTO</b>";
    break;

  case 3:
    return "<b>COOL</b>";
    break;

  case 1:
    return "<b>HEAT</b>";
    break;

  case 2:
    return "<b>DRY</b>";
    break;
  
  default:
    return "";
  }
}

std::string fan_to_str(uint8_t mode)
{
  switch (mode)
  {
  case 0:
    return "<b>AUTO</b>";
    break;

  case 2:
    return "<b>LOW</b>";
    break;

  case 3:
    return "<b>MEDIUM</b>";
    break;

  case 5:
    return "<b>HIGH</b>";
    break;
  
  default:
    return "";
  }
}


std::string swing_to_str(uint8_t mode)
{
  switch (mode)
  {
  case 7:
    return "<b>ON</b>";
    break;

  case 0:
    return "<b>OFF</b>";
    break;

  default:
    return "";
  }
}


std::string power_to_str(uint8_t mode)
{
  switch (mode)
  {
  case 1:
    return "<b>ON</b>";
    break;

  case 0:
    return "<b>OFF</b>";
    break;

  default:
    return "";
  }
}


// ESP8266 GPIO pin to use for IR blaster.
const uint16_t kIrLed = 4;
const int pinDHT11 = 12;
int pinLed = LED_BUILTIN;

// Library initialization, change it according to the imported library file.
IRTcl112Ac ac(kIrLed);
SimpleDHT11 dht11(pinDHT11);
bool power_on_start = false;
/// ##### End user configuration ######

unsigned long led_lasttime;
const unsigned long BOT_MTBS = 200; // mean time between scan messages
WiFiClientSecure secured_client;
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
unsigned long bot_lasttime;          // last time messages' scan has been done
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
DynamicJsonDocument root(1024);

byte temperature = 0;
byte humidity = 0;
struct state {
  uint8_t temperature = 26, 
          fan = 0, 
          mode = COOL_MODE, 
          swingVertical = SWING_OFF;
  bool powerStatus = power_on_start;
};

// core

state acState;

// settings
char deviceName[] = "AC Remote Control";


void blink(int timeout = 1000) 
{
  digitalWrite(pinLed, LOW);
  
  if (millis() - led_lasttime > timeout)
  {
    digitalWrite(pinLed, HIGH);
    led_lasttime = millis();
  }
}


void getDHT() {
  Serial.println("=================================");
  Serial.println("Sample DHT11...");
  int err = SimpleDHTErrSuccess;
  if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT11 failed, err="); 
    Serial.println(err);
    return;
  }
  
  Serial.print("Sample OK: ");
  Serial.print((int)temperature); Serial.print(" *C, "); 
  Serial.print((int)humidity); Serial.println(" H");
  }


String get_keyboard() {
  String keyboardJson = "[";
    keyboardJson += "[\"AC_ON\", \"AC_OFF\"]";
    keyboardJson += ",[\"Fan_Auto\",\"Fan_Low\",\"Fan_Med\",\"Fan_High\"]";
    keyboardJson += ",[\"Auto\",\"Cool\",\"Heat\",\"Dry\"]";
    keyboardJson += ",[\"swing_on\",\"swing_off\"]";
    keyboardJson += ",[\"State\"]";
  keyboardJson += "]";
  return keyboardJson;
}

void save_config() {
  DynamicJsonDocument jsonConfig(1024);
  jsonConfig["mode"] = ac.getMode();
  jsonConfig["temperature"] = ac.getTemp();
  jsonConfig["swingVertical"] = ac.getSwingVertical();
  jsonConfig["fan"] = ac.getFan();
  File config = SPIFFS.open("/config.json", "w");
  serializeJson(jsonConfig, config);
  config.close();
}

bool load_config() {
  File file = SPIFFS.open("/config.json", "r");
  if (file) {
    String jsonConfig = file.readString();
    deserializeJson(root, jsonConfig);
    file.close();
    serializeJson(root, Serial);

    ac.setTemp(root["temperature"]);
    ac.setFan(root["fan"]);
    ac.setMode(root["mode"]);
    ac.setSwingVertical(root["swingVertical"]);
    ac.setPower(power_on_start);
    return true;
  }
  else return false;
}

void send_ok(String& chat_id){
  bot.sendMessage(chat_id, "Ok");
  save_config();
}


void handleNewMessages(int numNewMessages)
{
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    if (chat_id != ADMIN_CHAT) {
      bot.sendMessage(chat_id, "Ваш ID в списке разрешенных отсутствует");
      continue;
    }

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if (text == "/send_test_action")
    {
      bot.sendChatAction(chat_id, "typing");
      delay(4000);
      bot.sendMessage(chat_id, "Did you see the action message?");

      // You can't use own message, just choose from one of bellow

      //typing for text messages
      //upload_photo for photos
      //record_video or upload_video for videos
      //record_audio or upload_audio for audio files
      //upload_document for general files
      //find_location for location data

      //more info here - https://core.telegram.org/bots/api#sendchataction
    }

    else if (text == "AC_ON"){
      ac.on(); ac.send(); send_ok(chat_id);
      continue;
    }

    else if (text == "AC_OFF"){
      ac.off(); ac.send();
    }

    else if (text == "Fan_Auto"){
      ac.setFan(FAN_AUTO);
    }

    else if (text == "Fan_Low"){
      ac.setFan(FAN_LOW);
    }

    else if (text == "Fan_Med"){
      ac.setFan(FAN_MED);
    }

    else if (text == "Fan_High"){
      ac.setFan(FAN_HIGH);
    }

    else if (text == "Auto"){
      ac.setMode(AUTO_MODE);
    }

    else if (text == "Cool"){
      ac.setMode(COOL_MODE);
    }

    else if (text == "Heat"){
      ac.setMode(HEAT_MODE);
    }

    else if (text == "Dry"){
      ac.setMode(DRY_MODE);
    }

    else if (text == "swing_on"){
      ac.setSwingVertical(SWING_ON);
    }

    else if (text == "swing_off"){
      ac.setSwingVertical(SWING_OFF);
    }

    else if (text.toInt()){
      uint8_t temp = text.toInt();
      if (temp >= 16 && temp <= 31)
      {
        ac.setTemp(text.toInt());
      }
      else
      {
        bot.sendMessage(chat_id, "Неверное значение температуры. Введите от 16 до 31 градуса");
        continue;
      }
    }

    else if (text == "State"){
      String message;
      getDHT();
      root["powerStatus"] = power_to_str(ac.getPower());
      root["mode"] = mode_to_str(int(ac.getMode()));
      root["temperature"] = "<b>" + std::to_string(int(ac.getTemp())) + "</b>";
      root["swingVertical"] = swing_to_str(int(ac.getSwingVertical()));
      root["fan"] = fan_to_str(int(ac.getFan()));
      root["currentTemperature"] = temperature;
      root["currentHumidity"] = humidity;
      serializeJson(root, message);
      bot.sendMessage(chat_id, message);
      Serial.println(message);
      continue;
    }
    else if (text == "/restart" or text == "/reset"){
      send_ok(chat_id);
      bot.getUpdates(bot.last_message_received + 1);
      ESP.restart();
    }
      else
    {
      String welcome = "Welcome to Universal Arduino Telegram Bot library, " + from_name + ".\n";
      welcome += "This is Chat Action Bot example.\n\n";
      welcome += "/send_test_action : to send test chat action message\n";
      bot.sendMessageWithReplyKeyboard(chat_id, welcome, "", get_keyboard(), true);
      continue;
    }
    send_ok(chat_id);

    if (ac.getPower()){
      ac.send();
    }
  }
}


void setup() {
  Serial.begin(115200);
  Serial.println();
  ac.begin();
  SPIFFS.begin();

  if (load_config()) {
    ac.setTemp(root["temperature"]);
    ac.setFan(root["fan"]);
    ac.setMode(root["mode"]);
    ac.setSwingVertical(root["swingVertical"]);
    ac.setPower(power_on_start);
  }
  else {
    ac.setTemp(acState.temperature);
    ac.setFan(acState.fan);
    ac.setMode(acState.mode);
    ac.setSwingVertical(acState.swingVertical);
    ac.setPower(acState.powerStatus);
  }

  pinMode(pinLed, OUTPUT);
  digitalWrite(pinLed, HIGH);

  secured_client.setTrustAnchors(&cert);


  WiFiManager wifiManager;
  if (!wifiManager.autoConnect(deviceName)) {
    delay(3000);
    ESP.restart();
    delay(5000);
  }

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
    
  #ifdef ADMIN_CHAT
  bot.sendMessage(ADMIN_CHAT, "<b>Я запустился!</b>");
  #endif
}

void loop() {
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages)
    {
      // blink(3000);
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
    // blink(1000);
  }
}
