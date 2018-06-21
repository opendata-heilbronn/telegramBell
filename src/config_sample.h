// rename this file to 'config.h'

// WiFi credentials
char ssid[] = "XXXXXX";     
char password[] = "YYYYYY"; 

#define HOSTNAME "telegramBell"
#define otaPassword "admin"


// Initialize Telegram BOT
#define BOTtoken "XXXXXXXXX:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"  // your Bot Token (Get from Botfather)
#define BOTname "botUsernameWithoutAt"
// only allow messages from authorized group
#define authorizedChatID "12345678"
#define adminChatID "11223344"

//put user ID of banned users here, empty string for no bans
String bannedUsers[] = {"12345678"};