#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <Base64.h>
#include "esp_camera.h"
#include <Adafruit_GFX.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include "Adafruit_VL53L0X.h"
#include <WebSocketsServer.h>

// WiFi credentials
// Wifi network station credentials
#define WIFI_SSID "admin"
#define WIFI_PASSWORD "admin1234"

// Wifi network station credentials

const IPAddress dns(8, 8, 8, 8);

const unsigned long BOT_MTBS = 1000;
const int MAX_CHAT_IDS = 10;
const int EEPROM_SIZE = 1024;
unsigned long bot_lasttime;
bool sendPhoto = false;  // Flag to indicate photo sending request

// // For Device 1
// // OpenAI API key
// const String apiKey = "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";
// // Telegram BOT Token (Get from Botfather)
// #define BOT_TOKEN "8264688459:AAGx3qVy6ukhEL5hEdnOO23xjisxnjix4d0"  // for VisionarySpecs1Bot

// For Device 2
const String apiKey = "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";
#define BOT_TOKEN ":AAHCbgcMqmDVXyR5kiT4cK_FCJ3LYlU-RY0"  // for VisionarySpec2bot

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

String Question = "Summarize the image";

// Create a WebSocket server on port 80
WebSocketsServer webSocket = WebSocketsServer(80);

const int MPU_addr = 0x68;
int16_t AcX, AcY, AcZ;

// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define SDA_PIN 14  // GPIO14 for SDA
#define SCL_PIN 15  // GPIO15 for SCL
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pin definitions for ESP32-CAM AI-Thinker module
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22
#define FLASH_LED_PIN 4
#define BUTTON_PIN 2

void displayCenteredText(const String &text, int textSize = 1) {
  display.clearDisplay();
  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);

  int maxLineLength = 16;  // Assuming 16 characters fit per line at textSize 1
  String lineBuffer = "";
  String wordBuffer = "";
  int16_t x1, y1;
  uint16_t textWidth, textHeight;

  // Calculate line height
  display.getTextBounds("A", 0, 0, &x1, &y1, &textWidth, &textHeight);
  int lineHeight = textHeight + 2;

  // Calculate the total number of lines needed
  int lineCount = 0;
  for (size_t i = 0; i <= text.length(); i++) {
    char c = text.charAt(i);
    if (c == ' ' || c == '\n' || c == '\0') {
      if (lineBuffer.length() + wordBuffer.length() > maxLineLength) {
        lineCount++;
        lineBuffer = wordBuffer;
      } else {
        lineBuffer += (lineBuffer.isEmpty() ? "" : " ") + wordBuffer;
      }
      wordBuffer = "";

      if (c == '\n') {
        lineCount++;
        lineBuffer = "";
      }
    } else {
      wordBuffer += c;
    }
  }
  if (!lineBuffer.isEmpty())
    lineCount++;  // Count the last line

  // Calculate the vertical offset to center the block of text
  int totalTextHeight = lineCount * lineHeight;
  int yOffset = (SCREEN_HEIGHT - totalTextHeight) / 2;

  // Render the text line by line, vertically centered
  int yPos = yOffset;
  lineBuffer = "";
  wordBuffer = "";
  for (size_t i = 0; i <= text.length(); i++) {
    char c = text.charAt(i);
    if (c == ' ' || c == '\n' || c == '\0') {
      if (lineBuffer.length() + wordBuffer.length() > maxLineLength) {
        // Render the current line
        display.setCursor((SCREEN_WIDTH - lineBuffer.length() * textWidth) / 2, yPos);
        display.print(lineBuffer);
        yPos += lineHeight;
        lineBuffer = wordBuffer;
      } else {
        lineBuffer += (lineBuffer.isEmpty() ? "" : " ") + wordBuffer;
      }
      wordBuffer = "";

      if (c == '\n' || c == '\0') {
        display.setCursor((SCREEN_WIDTH - lineBuffer.length() * textWidth) / 2, yPos);
        display.print(lineBuffer);
        yPos += lineHeight;
        lineBuffer = "";
      }
    } else {
      wordBuffer += c;
    }
  }

  display.display();
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %s\n", num, ip.toString().c_str());
        break;
      }
    case WStype_DISCONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Disconnected from %s\n", num, ip.toString().c_str());
        break;
      }
    case WStype_TEXT:
      Serial.printf("[%u] Received: %s\n", num, payload);  // Handle incoming text messages
      break;
    default:
      break;
  }
}

struct ChatIDStorage {
  int count;
  String chatIDs[MAX_CHAT_IDS];
  String userNames[MAX_CHAT_IDS];
};

ChatIDStorage chatIDs;

void saveChatIDs() {
  Serial.println("Saving chat IDs to EEPROM...");
  EEPROM.begin(EEPROM_SIZE);
  int addr = 0;

  // Save count
  EEPROM.put(addr, chatIDs.count);
  addr += sizeof(chatIDs.count);

  for (int i = 0; i < chatIDs.count; i++) {
    // Save chat ID
    int len = chatIDs.chatIDs[i].length();
    EEPROM.put(addr, len);
    addr += sizeof(len);
    for (int j = 0; j < len; j++) {
      EEPROM.write(addr++, chatIDs.chatIDs[i].charAt(j));
    }

    // Save user name
    len = chatIDs.userNames[i].length();
    EEPROM.put(addr, len);
    addr += sizeof(len);
    for (int j = 0; j < len; j++) {
      EEPROM.write(addr++, chatIDs.userNames[i].charAt(j));
    }
  }

  // Clear remaining EEPROM space
  while (addr < EEPROM_SIZE) {
    EEPROM.write(addr++, 0);
  }

  if (EEPROM.commit()) {
    Serial.println("EEPROM commit successful");
  } else {
    Serial.println("EEPROM commit failed");
  }
  EEPROM.end();
}

void loadChatIDs() {
  Serial.println("Loading chat IDs from EEPROM...");
  EEPROM.begin(EEPROM_SIZE);
  int addr = 0;

  // Load count
  EEPROM.get(addr, chatIDs.count);
  addr += sizeof(chatIDs.count);

  // Safety check
  if (chatIDs.count > MAX_CHAT_IDS || chatIDs.count < 0) {
    Serial.printf("Invalid count: %d, resetting to 0\n", chatIDs.count);
    chatIDs.count = 0;
    EEPROM.end();
    return;
  }

  for (int i = 0; i < chatIDs.count; i++) {
    // Load chat ID
    int len;
    EEPROM.get(addr, len);
    addr += sizeof(len);

    // Prevent buffer overflow
    if (len > 50 || len < 0) {
      Serial.printf("Invalid chat ID length: %d at index %d\n", len, i);
      chatIDs.count = i;  // Truncate to valid entries
      break;
    }

    String chatID;
    for (int j = 0; j < len; j++) {
      chatID += char(EEPROM.read(addr++));
    }
    chatIDs.chatIDs[i] = chatID;

    // Load user name
    EEPROM.get(addr, len);
    addr += sizeof(len);

    if (len > 50 || len < 0) {
      Serial.printf("Invalid user name length: %d at index %d\n", len, i);
      chatIDs.count = i;
      break;
    }

    String userName;
    for (int j = 0; j < len; j++) {
      userName += char(EEPROM.read(addr++));
    }
    chatIDs.userNames[i] = userName;

    Serial.printf("Loaded: %s (%s)\n", userName.c_str(), chatID.c_str());
  }

  EEPROM.end();
  Serial.printf("Successfully loaded %d chat IDs\n", chatIDs.count);
}

bool addChatID(const String &newChatID, const String &userName) {
  Serial.printf("Attempting to add: %s (%s)\n", userName.c_str(), newChatID.c_str());

  if (chatIDs.count >= MAX_CHAT_IDS) {
    Serial.println("Max users reached");
    return false;
  }

  // Check if already exists
  for (int i = 0; i < chatIDs.count; i++) {
    if (chatIDs.chatIDs[i] == newChatID) {
      Serial.println("User already exists, updating name");
      chatIDs.userNames[i] = userName;
      saveChatIDs();
      return false;
    }
  }

  // Add new entry
  if (chatIDs.count < MAX_CHAT_IDS) {
    chatIDs.chatIDs[chatIDs.count] = newChatID;
    chatIDs.userNames[chatIDs.count] = userName;
    chatIDs.count++;
    Serial.printf("Added new user. Total: %d\n", chatIDs.count);
    saveChatIDs();
    return true;
  }

  return false;
}

bool removeChatID(const String &chatIDToRemove) {
  Serial.printf("Attempting to remove: %s\n", chatIDToRemove.c_str());

  for (int i = 0; i < chatIDs.count; i++) {
    if (chatIDs.chatIDs[i] == chatIDToRemove) {
      // Shift all elements after the found one
      for (int j = i; j < chatIDs.count - 1; j++) {
        chatIDs.chatIDs[j] = chatIDs.chatIDs[j + 1];
        chatIDs.userNames[j] = chatIDs.userNames[j + 1];
      }
      chatIDs.count--;
      Serial.printf("Removed user. Total: %d\n", chatIDs.count);
      saveChatIDs();
      return true;
    }
  }
  Serial.println("User not found for removal");
  return false;
}

void broadcastMessage(const String &message) {
  Serial.printf("Broadcasting to %d users: %s\n", chatIDs.count, message.c_str());
  for (int i = 0; i < chatIDs.count; i++) {
    Serial.printf("Sending to %s\n", chatIDs.chatIDs[i].c_str());
    if (!bot.sendMessage(chatIDs.chatIDs[i], message, "")) {
      Serial.println("Failed to send to " + chatIDs.chatIDs[i]);
    }
    delay(500);
    yield();  // Prevent watchdog
  }
}

void handleNewMessages(int numNewMessages) {
  Serial.printf("Handling %d new messages\n", numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    Serial.printf("Received '%s' from %s (%s)\n", text.c_str(), from_name.c_str(), chat_id.c_str());

    String answer;
    text.toLowerCase();

    if (text == "/help") {
      answer = "Available commands:\n"
               "/start - Register for notifications\n"
               "/stop - Unregister\n"
               "/status - Check status\n"
               "/capture - Capture Camera image\n"
               "/list - Show registered users (admin)";
    } else if (text == "/start") {
      if (chatIDs.count >= MAX_CHAT_IDS) {
        answer = "Max users reached (" + String(MAX_CHAT_IDS) + ")";
      } else {
        if (addChatID(chat_id, from_name)) {
          answer = "Welcome " + from_name + "! You're now registered.";
        } else {
          answer = "You're already registered, " + from_name + "!";
        }
      }
    } else if (text == "/stop") {
      if (removeChatID(chat_id)) {
        answer = "You've been unregistered.";
      } else {
        answer = "You weren't registered.";
      }
    } else if (text == "/status") {
      answer = "Status: OK\nUsers: " + String(chatIDs.count) + "/" + String(MAX_CHAT_IDS);
    } else if (text == "/list") {
      answer = "Registered users (" + String(chatIDs.count) + "):\n";
      for (int i = 0; i < chatIDs.count; i++) {
        answer += String(i + 1) + ". " + chatIDs.userNames[i] + " (" + chatIDs.chatIDs[i] + ")\n";
      }
      if (chatIDs.count == 0) {
        answer += "No users registered";
      }
    } else if (text == "/capture") {
      answer = "Capturing and sending photo...";
      sendPhoto = true;  // Set the flag to capture and send photo
    } else {
      answer = "Unknown command. Send /help for options.";
    }

    Serial.printf("Sending reply: %s\n", answer.c_str());
    if (!bot.sendMessage(chat_id, answer, "")) {
      Serial.println("Failed to send reply");
    }
    yield();  // Prevent watchdog reset
  }
}

void setupWiFi() {
  Serial.println("Connecting to WiFi...");
  displayCenteredText("Connecting to WiFi:" + String(WIFI_SSID));
  WiFi.config(INADDR_NONE, INADDR_NONE, dns);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
    Serial.print(".");
    display.print(".");
    delay(500);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFailed to connect");
    displayCenteredText("WiFi connection");
    displayCenteredText("failed. Restarting...");
    delay(2000);
    ESP.restart();
    return;
  }

  Serial.printf("\nWiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
  displayCenteredText("IP:" + WiFi.localIP().toString());
  delay(3000);
}

void setupTime() {
  Serial.println("Setting up time...");
  displayCenteredText("Setting up time...");

  // Set timezone to Indian (UTC+5.5)
  const int timezoneOffset = 5 * 3600 + 30 * 60;  // UTC+5:30 in seconds
  configTime(timezoneOffset, 0, "time.google.com", "time.cloudflare.com", "pool.ntp.org");

  time_t now = time(nullptr);
  unsigned long startAttemptTime = millis();
  while (now < 24 * 3600 && millis() - startAttemptTime < 10000) {
    delay(100);
    now = time(nullptr);
    yield();
  }

  if (now < 24 * 3600) {
    Serial.println("Failed to get time");
    displayCenteredText("Failed to get time");
    delay(2000);
  } else {
    // Get Indian time
    struct tm *timeinfo = localtime(&now);
    Serial.printf("IST Time: %02d:%02d:%02d %02d-%02d-%04d\n",
                  timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
                  timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900);
    displayCenteredText("Time: " + String(timeinfo->tm_hour) + ":" + String(timeinfo->tm_min) + ":" + String(timeinfo->tm_sec));
    delay(3000);  // Show for 3 seconds
  }
}

void bot_setup() {
  displayCenteredText("Setting up Telegram...");
  Serial.println("Setting up Telegram Bot...");

  // Initialize chatIDs structure
  chatIDs.count = 0;
  for (int i = 0; i < MAX_CHAT_IDS; i++) {
    chatIDs.chatIDs[i] = "";
    chatIDs.userNames[i] = "";
  }

  loadChatIDs();

  const String commands = F("["
                            "{\"command\":\"help\",\"description\":\"Get help\"},"
                            "{\"command\":\"start\",\"description\":\"Register\"},"
                            "{\"command\":\"stop\",\"description\":\"Unregister\"},"
                            "{\"command\":\"capture\",\"description\":\"Capture Photo\"},"
                            "{\"command\":\"status\",\"description\":\"Check status\"},"
                            "{\"command\":\"list\",\"description\":\"Show users\"}"
                            "]");
  if (!bot.setMyCommands(commands)) {
    Serial.println("Failed to set commands");
  }
}

void check_telegram_messages() {
  if (millis() - bot_lasttime > BOT_MTBS) {
    int numNewMessages = 0;
    try {
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    } catch (...) {
      Serial.println("Error getting updates");
    }

    if (numNewMessages > 0) {
      Serial.printf("Processing %d messages\n", numNewMessages);
      handleNewMessages(numNewMessages);
    }

    bot_lasttime = millis();
    yield();
  }

  // Check WiFi connection periodically
  static unsigned long lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 30000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, restarting...");
      ESP.restart();
    }
    lastWifiCheck = millis();
  }

  // Add small delay to prevent tight loop
  delay(10);
}

// VL53L0X sensor
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
uint16_t distance = 0;  // Variable to store distance reading

// Function to encode image to Base64
String encodeImageToBase64(const uint8_t *imageData, size_t imageSize) {
  return base64::encode(imageData, imageSize);
}

void captureAndAnalyzeImage() {
  Serial.println("Capturing image...");

  // Capture the image frame buffer
  camera_fb_t *fb = esp_camera_fb_get();  // Get the frame buffer
  if (!fb) {
    Serial.println("Camera capture failed");
    displayCenteredText("Capture Failed");
    return;
  }

  // After the new frame is obtained, ensure the buffer is returned (cleared)
  esp_camera_fb_return(fb);  // Release the frame buffer from the previous capture

  // Now, capture the new image
  fb = esp_camera_fb_get();  // Get the frame buffer again for the new image

  if (!fb) {
    Serial.println("Camera capture failed");
    displayCenteredText("Capture Failed");
    return;
  }

  Serial.println("Image captured");
  displayCenteredText("Image captured");
  String base64Image = encodeImageToBase64(fb->buf, fb->len);

  // Return the frame buffer after processing the image
  esp_camera_fb_return(fb);  // Return the frame buffer to free memory

  if (base64Image.isEmpty()) {
    Serial.println("Failed to encode the image!");
    displayCenteredText("Encode Failed");
    return;
  }
  // Send the image to OpenAI for analysis
  AnalyzeImage(base64Image);
}

void AnalyzeImage(const String &base64Image) {
  Serial.println("Sending image for analysis...");
  displayCenteredText("Processing...");

  String result;

  // Prepare the payload for the OpenAI API
  String url = "data:image/jpeg;base64," + base64Image;
  Serial.println(url);

  DynamicJsonDocument doc(4096);
  doc["model"] = "gpt-4.1";
  JsonArray messages = doc.createNestedArray("messages");
  JsonObject message = messages.createNestedObject();
  message["role"] = "user";
  JsonArray content = message.createNestedArray("content");
  JsonObject textContent = content.createNestedObject();
  textContent["type"] = "text";
  textContent["text"] = "Summarize the context of this image?";

  JsonObject imageContent = content.createNestedObject();
  imageContent["type"] = "image_url";
  JsonObject imageUrlObject = imageContent.createNestedObject("image_url");
  imageUrlObject["url"] = url;
  imageContent["image_url"]["detail"] = "auto";

  doc["max_tokens"] = 400;

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  // Send request and validate response
  if (sendPostRequest(jsonPayload, result)) {
    Serial.print("[ChatGPT] Response: ");
    Serial.println(result);

    // Clear the display before showing the new response
    display.clearDisplay();
    display.display();

    DynamicJsonDocument responseDoc(4096);
    deserializeJson(responseDoc, result);

    String responseContent = responseDoc["choices"][0]["message"]["content"].as<String>();
    Serial.println("[ChatGPT] Parsed response: " + responseContent);

    webSocket.broadcastTXT(responseContent.c_str(), responseContent.length());
    // Smooth scrolling and proper word wrapping
    display.clearDisplay();
    int lineHeight = 8;     // Height of each line in pixels
    int maxLineChars = 21;  // Approx. max characters per line
    int visibleLines = 7;
    int scrollDelay = 2000;  // Delay for scrolling in milliseconds

    std::vector<String> lines;  // Store formatted lines for display

    // Split responseContent into words for word wrapping
    String word = "";
    String currentLine = "";

    for (int i = 0; i < responseContent.length(); i++) {
      char c = responseContent.charAt(i);
      if (c == ' ' || c == '\n') {
        if (currentLine.length() + word.length() <= maxLineChars) {
          currentLine += (currentLine.isEmpty() ? "" : " ") + word;
        } else {
          lines.push_back(currentLine);
          currentLine = word;
        }
        word = "";
      } else {
        word += c;
      }
    }
    if (!currentLine.isEmpty())
      lines.push_back(currentLine);
    if (!word.isEmpty())
      lines.push_back(word);

    // Display lines with scrolling effect
    for (size_t i = 0; i < lines.size(); i++) {
      display.clearDisplay();
      for (size_t j = 0; j < visibleLines && (i + j) < lines.size(); j++) {
        display.setCursor(0, j * lineHeight);
        display.print(lines[i + j]);
      }
      display.display();
      delay(scrollDelay);
    }

    // Clear display after the response
    display.clearDisplay();
    display.display();

    displayCenteredText("Press button to capture");
  } else {
    Serial.print("[ChatGPT] Error: ");
    Serial.println(result);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("API Error");
    display.display();
  }
}

void sendPhotoTelegram() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  Serial.println("Sending photo to Telegram...");

  for (int i = 0; i < chatIDs.count; i++) {
    // Direct HTTPS POST (fast and reliable)
    if (secured_client.connect("api.telegram.org", 443)) {
      String head = "--boundary\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
      head += chatIDs.chatIDs[i].c_str();
      head += "\r\n--boundary\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
      String tail = "\r\n--boundary--\r\n";

      uint32_t totalLen = fb->len + head.length() + tail.length();

      secured_client.println("POST /bot" + String(BOT_TOKEN) + "/sendPhoto HTTP/1.1");
      secured_client.println("Host: api.telegram.org");
      secured_client.println("Content-Length: " + String(totalLen));
      secured_client.println("Content-Type: multipart/form-data; boundary=boundary");
      secured_client.println();
      secured_client.print(head);

      uint8_t *fbBuf = fb->buf;
      size_t fbLen = fb->len;
      for (size_t n = 0; n < fbLen; n += 1024) {
        if (n + 1024 < fbLen) {
          secured_client.write(fbBuf, 1024);
          fbBuf += 1024;
        } else if (fbLen % 1024 > 0) {
          size_t remainder = fbLen % 1024;
          secured_client.write(fbBuf, remainder);
        }
      }
      secured_client.print(tail);

      Serial.println("Photo sent to User " + String(chatIDs.userNames[i]) + " chat ID: " + String(chatIDs.chatIDs[i]));
    } else {
      Serial.println("Connection to Telegram failed");
    }
    secured_client.stop();
    delay(1000);
  }

  esp_camera_fb_return(fb);
}

bool sendPostRequest(const String &payload, String &result) {
  HTTPClient http;
  http.begin("https://api.openai.com/v1/chat/completions");

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + apiKey);
  http.setTimeout(20000);

  Serial.print("Payload size: ");
  Serial.println(payload.length());

  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    result = http.getString();
    Serial.println("HTTP Response Code: " + String(httpResponseCode));
    Serial.println("Response Body: " + result);
    http.end();
    return true;
  } else {
    result = "HTTP request failed, response code: " + String(httpResponseCode);
    Serial.println("Error Code: " + String(httpResponseCode));
    Serial.println("Error Message: " + http.errorToString(httpResponseCode));
    http.end();
    return false;
  }
}

void getacceldata() {
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);  // Starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr, 14, true);  // Request a total of 14 registers
  AcX = Wire.read() << 8 | Wire.read();  // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
  AcY = Wire.read() << 8 | Wire.read();  // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  AcZ = Wire.read() << 8 | Wire.read();  // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  // Serial.println((String) "AX:" + AcX + "\tAY:" + AcY + "\tAZ:" + AcZ);
  // int16_t temp = (Wire.read() << 8) | Wire.read(); // Temperature data (optional)
  // int16_t gyroX = (Wire.read() << 8) | Wire.read(); // X-axis gyroscope data
  // int16_t gyroY = (Wire.read() << 8) | Wire.read(); // Y-axis gyroscope data
  // int16_t gyroZ = (Wire.read() << 8) | Wire.read(); // Z-axis gyroscope data
  // Serial.println((String) "Temp:" + temp + "\tGX:" + gyroX + "\tGY:" + gyroY + "\tGZ" + gyroZ);
}

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for (;;)
      ;
  }
  display.clearDisplay();
  display.setRotation(2);
  display.setTextSize(2);
  // Initialize VL53L0X
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    displayCenteredText("VL53L0X Failed!");
    while (true)
      ;
  }
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);  // PWR_MGMT_1 register
  Wire.write(0x00);  // Set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);
  // Display the project title on power-on
  displayCenteredText("MPU-Initialiside");
  delay(1000);  // Hold the title screen for 3 seconds

  setupWiFi();
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  setupTime();
  bot_setup();
  displayCenteredText("WiFi Connected!");
  delay(2000);
  // Camera configuration

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    displayCenteredText("Camera Init Failed");
    return;
  }
  // Start continuous ranging
  lox.startRangeContinuous();
  displayCenteredText("Camera Initialized");
  delay(2000);
  displayCenteredText("AI VISION Project");
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  Serial.println("WebSocket server started.");
  delay(3000);  // Hold the title screen for 3 seconds
  String welcome_msg = "Smart Specs is now online!";
  broadcastMessage(welcome_msg);
  webSocket.broadcastTXT(welcome_msg.c_str(), welcome_msg.length());
  displayCenteredText("Press button to capture");
  Serial.printf("Initialized with %d users\n", chatIDs.count);
  delay(2000);
}

void loop() {
  webSocket.loop();
  // Send photo if requested
  if (sendPhoto) {
    displayCenteredText("Sending photo...");
    Serial.println("Preparing photo...");
    sendPhotoTelegram();
    sendPhoto = false;
    displayCenteredText("Press button to capture");
  }
  // Check for new Telegram messages
  check_telegram_messages();
  if (lox.isRangeComplete()) {
    distance = lox.readRange();
  }
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("Button pressed! Capturing image...");
    displayCenteredText("Capturing...");
    captureAndAnalyzeImage();
    delay(1000);  // Small delay to debounce button press
    displayCenteredText("Press button to capture");
  }
  getacceldata();  // Call the function to get accelerometer data
  if ((AcX > 15000 && AcY > 30000) || (AcX > 15000 && AcZ > 30000) || (AcY > 30000 && AcZ > 30000)) {
    broadcastMessage("Accident Detected");  // Send alert message
    Serial.println("Accident Detected");
    displayCenteredText("Accident Detected");
    sendPhotoTelegram();  // Send photo to all registered users
    delay(10000);
    displayCenteredText("Press button to capture");
  }
}
