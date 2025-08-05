#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Servo.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#define MAX_FEEDS 8

// WiFi credentials
const char* ssid = " ";
const char* password = " ";

Servo feederServo;
const int servoPin = D1;

const int CLOSED_POS = 0;
const int SMALL_POS = 45;
const int MEDIUM_POS = 90;
const int LARGE_POS = 135;

int feedCount = 3;  // Number of feeds per day
int feedHours[MAX_FEEDS] = { 8, 16, 22 };
int feedMinutes[MAX_FEEDS] = { 0, 0, 0 };
int lastFeedDay[MAX_FEEDS] = { -1, -1, -1 };

int defaultPortionSize = MEDIUM_POS;
int utcOffset = 0;  // In hours

ESP8266WebServer server(80);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

const int EEPROM_SIZE = 512;
const int ADDR_PORTION = 0;       // int (4 bytes)
const int ADDR_UTCOFFSET = 4;       // int (4 bytes)
const int ADDR_FEEDCOUNT = 8;       // int (4 bytes)
const int ADDR_FEEDHOURS = 12;    // int[MAX_FEEDS] (4*8 = 32 bytes)
const int ADDR_FEEDMINUTES = 44;    // int[MAX_FEEDS] (4*8 = 32 bytes)

void setup() {
  Serial.begin(115200);
  Serial.println("Serial initialized");
  EEPROM.begin(EEPROM_SIZE);
  Serial.println("EEPROM initialized");
  loadSettings();
  Serial.println("Settings loaded");

  Serial.println("Loaded settings:");
  Serial.print("defaultPortionSize: ");
  Serial.println(defaultPortionSize);
  Serial.print("utcOffset: ");
  Serial.println(utcOffset);
  Serial.print("feedCount: ");
  Serial.println(feedCount);
  for (int i = 0; i < feedCount; i++) {
    Serial.print("Feed ");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(feedHours[i]);
    Serial.print(":");
    Serial.println(feedMinutes[i]);
  }

  feederServo.attach(servoPin);
  feederServo.write(CLOSED_POS);
  delay(1000);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  timeClient.setTimeOffset(utcOffset * 3600);

  server.on("/", handleRoot);
  server.on("/feed", handleFeed);
  server.on("/settings", handleSettings);
  server.on("/status", handleStatus);

  server.begin();
  Serial.println("Web server started");
  Serial.println("Hej världen!");
}

void loop() {
  server.handleClient();
  timeClient.update();

  unsigned long epochTime = timeClient.getEpochTime();
  int currentHour = (epochTime % 86400L) / 3600;
  int currentMinute = (epochTime % 3600) / 60;
  int today = epochTime / 86400L;

  for (int i = 0; i < feedCount; i++) {
    if (currentHour == feedHours[i] && currentMinute == feedMinutes[i] && lastFeedDay[i] != today) {
      Serial.print("Scheduled Feed: ");
      Serial.print(feedHours[i]);
      Serial.print(":");
      Serial.print(feedMinutes[i]);
      Serial.print(" on day ");
      Serial.println(today);
      feedPet(defaultPortionSize);
      lastFeedDay[i] = today;
    }
  }
  delay(1000);
}

void feedPet(int portionSize) {
  Serial.print("Feeding pet - Portion size: ");
  Serial.println(portionSize);
  feederServo.write(portionSize);
  delay(2000);
  feederServo.write(CLOSED_POS);
  delay(1000);
}

void handleRoot() {
  String html = "<!DOCTYPE html>\n"
                "<html>\n"
                "<head>\n"
                "    <title>Pet Feeder Control</title>\n"
                "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
                "    <style>\n"
                "        body { font-family: Arial; margin: 20px; background: #f0f0f0; }\n"
                "        .container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }\n"
                "        .button { background: #4CAF50; color: white; padding: 15px 25px; border: none; border-radius: 5px; cursor: pointer; margin: 10px; font-size: 16px; }\n"
                "        .button:hover { background: #45a049; }\n"
                "        .button.small { background: #2196F3; }\n"
                "        .button.medium { background: #FF9800; }\n"
                "        .button.large { background: #f44336; }\n"
                "        .status { background: #e7e7e7; padding: 15px; border-radius: 5px; margin: 20px 0; }\n"
                "        input[type=\"number\"] { width: 80px; padding: 5px; }\n"
                "        .schedule { background: #fff3cd; padding: 15px; border-radius: 5px; margin: 10px 0; }\n"
                "        .feedTime { width: 100px; padding: 5px; margin-right: 10px; }\n"
                "    </style>\n"
                "</head>\n"
                "<body>\n"
                "    <div class=\"container\">\n"
                "        <h1>Mangus Matklocka &#128049</h1>\n"
                "        <div class=\"status\" id=\"status\">\n"
                "            <h3>Status</h3>\n"
                "            <p>WiFi: Connected</p>\n"
                "            <p>Current Time: <span id=\"now\">Loading...</span></p>\n"
                "            <p>Next Feeds: <span id=\"next\">Loading...</span></p>\n"
                "        </div>\n"
                "        <h3>Manual Feeding</h3>\n"
                "        <button class=\"button small\" onclick=\"feed('small')\">Small Portion</button>\n"
                "        <button class=\"button medium\" onclick=\"feed('medium')\">Medium Portion</button>\n"
                "        <button class=\"button large\" onclick=\"feed('large')\">Large Portion</button>\n"
                "        <div class=\"schedule\">\n"
                "            <h3>Feeding Schedule</h3>\n"
                "            <p>\n"
                "              <label>Feedings per day: </label>\n"
                "              <input type=\"number\" id=\"feedCount\" min=\"1\" max=\"8\" value=\"3\" onchange=\"updateFeedTimes()\">\n"
                "            </p>\n"
                "            <div id=\"feedTimes\"></div>\n"
                "            <p>\n"
                "              <label>UTC Offset (hours): </label>\n"
                "              <input type=\"number\" id=\"utcOffset\" min=\"-12\" max=\"14\" value=\"0\" onchange=\"updateSettings()\">\n"
                "            </p>\n"
                "            <p>\n"
                "              <label>Default Portion: </label>\n"
                "              <select id=\"portion\" onchange=\"updateSettings()\">\n"
                "                <option value=\"small\">Small</option>\n"
                "                <option value=\"medium\" selected>Medium</option>\n"
                "                <option value=\"large\">Large</option>\n"
                "              </select>\n"
                "            </p>\n"
                "        </div>\n"
                "        <button class=\"button\" onclick=\"location.reload()\">Refresh Status</button>\n"
                "    </div>\n"
                "    <script>\n"
                "        function feed(size) {\n"
                "            fetch('/feed?size=' + size)\n"
                "                .then(response => response.text())\n"
                "                .then(data => {\n"
                "                    alert('Fed: ' + size + ' portion');\n"
                "                    updateStatus();\n"
                "                });\n"
                "        }\n"
                "        function updateFeedTimes() {\n"
                "            let count = parseInt(document.getElementById('feedCount').value);\n"
                "            let box = document.getElementById('feedTimes');\n"
                "            box.innerHTML = '';\n"
                "            for (let i = 0; i < count; i++) {\n"
                "                let val = window.feedTimes && window.feedTimes[i] ? window.feedTimes[i] : (i==0?'08:00':i==1?'16:00':i==2?'22:00':'12:00');\n"
                "                if (!val || val.length !== 5 || val.indexOf(':') !== 2) val = '12:00'; // Defensive\n"
                "                box.innerHTML += `<input type='time' class='feedTime' value='${val}' onchange='updateSettings()'>`;\n"
                "            }\n"
                "        }\n"
                "        function updateSettings() {\n"
                "            const portion = document.getElementById('portion').value;\n"
                "            const utcOffset = document.getElementById('utcOffset').value;\n"
                "            const feedCount = document.getElementById('feedCount').value;\n"
                "            const times = Array.from(document.getElementsByClassName('feedTime')).map(input => input.value);\n"
                "            // Defensive: replace blanks or malformed times with '12:00'\n"
                "            const filteredTimes = times.map(t => (t && t.length === 5 && t.indexOf(':') === 2) ? t : '12:00');\n"
                "            window.feedTimes = filteredTimes;\n"
                "            fetch('/settings?portion=' + portion + '&utcOffset=' + utcOffset + '&feedCount=' + feedCount + '&feedTimes=' + filteredTimes.join(\",\"))\n"
                "                .then(response => response.text()).then(data => { showToast('Settings saved!'); updateStatus(); });\n"
                "        }\n"
                "        function updateStatus() {\n"
                "            fetch('/status')\n"
                "                .then(response => response.json())\n"
                "                .then(data => {\n"
                "                    document.getElementById('now').textContent = data.now;\n"
                "                    document.getElementById('next').textContent = data.next;\n"
                "                    document.getElementById('portion').value = data.portion;\n"
                "                    document.getElementById('utcOffset').value = data.utcOffset;\n"
                "                    document.getElementById('feedCount').value = data.feedCount;\n"
                "                    window.feedTimes = data.times;\n"
                "                    updateFeedTimes();\n"
                "                    Array.from(document.getElementsByClassName('feedTime')).forEach((el, i) => { el.value = data.times[i] || '12:00'; });\n"
                "                });\n"
                "        }\n"
                "        function showToast(msg) { var toast = document.getElementById('toast'); toast.textContent = msg; toast.className = 'show'; setTimeout(function(){ toast.className = toast.className.replace('show', ''); }, 2500); }\n"
                "        updateStatus();\n"
                "        setInterval(updateStatus, 30000);\n"
                "    </script>\n"
                "</body>\n"
                "</html>\n";
  html += "<div id=\"toast\">Settings saved!</div>\n";
  html += "<style>\n"
          "#toast {"
          "  visibility: hidden;"
          "  min-width: 200px;"
          "  margin-left: -100px;"
          "  background-color: #333;"
          "  color: #fff;"
          "  text-align: center;"
          "  border-radius: 2px;"
          "  padding: 16px;"
          "  position: fixed;"
          "  z-index: 1;"
          "  left: 50%;"
          "  bottom: 30px;"
          "  font-size: 17px;"
          "  opacity: 0;"
          "  transition: opacity 0.5s, bottom 0.5s;"
          "}\n"
          "#toast.show {"
          "  visibility: visible;"
          "  opacity: 1;"
          "  bottom: 50px;"
          "  animation: fadein 0.5s, fadeout 0.5s 2.0s;"
          "}\n"
          "@keyframes fadein {"
          "  from {bottom: 0; opacity: 0;}"
          "  to {bottom: 50px; opacity: 1;}"
          "}\n"
          "@keyframes fadeout {"
          "  from {bottom: 50px; opacity: 1;}"
          "  to {bottom: 0; opacity: 0;}"
          "}\n"
          "</style>\n";
  html += "<script>\n"
          "// ...rest of your JS already included above...\n"
          "</script>\n";
  server.send(200, "text/html", html);
}

// Defensive parsing and debug!
void handleSettings() {
  Serial.println("--- handleSettings called ---");
  if (server.hasArg("portion")) {
    String portion = server.arg("portion");
    Serial.print("Portion: ");
    Serial.println(portion);
    if (portion == "small") defaultPortionSize = SMALL_POS;
    else if (portion == "medium") defaultPortionSize = MEDIUM_POS;
    else if (portion == "large") defaultPortionSize = LARGE_POS;
  }
  if (server.hasArg("utcOffset")) {
    utcOffset = server.arg("utcOffset").toInt();
    Serial.print("utcOffset: ");
    Serial.println(utcOffset);
    timeClient.setTimeOffset(utcOffset * 3600);
  }
  if (server.hasArg("feedCount")) {
    feedCount = server.arg("feedCount").toInt();
    feedCount = constrain(feedCount, 1, MAX_FEEDS);
    Serial.print("feedCount: ");
    Serial.println(feedCount);
  }
  if (server.hasArg("feedTimes")) {
    String timesStr = server.arg("feedTimes");
    Serial.print("Received feedTimes: ");
    Serial.println(timesStr);
    int idx = 0;
    int start = 0;
    while (idx < feedCount && start < timesStr.length()) {
      int end = timesStr.indexOf(',', start);
      if (end < 0) end = timesStr.length();
      String t = timesStr.substring(start, end);
      t.trim();
      Serial.print("Parsed time slot ");
      Serial.print(idx);
      Serial.print(": ");
      Serial.println(t);
      // Defensive: if empty or malformed, default to '12:00'
      if (t.length() != 5 || t.indexOf(':') != 2) {
        feedHours[idx] = 12;
        feedMinutes[idx] = 0;
        Serial.print("Set slot ");
        Serial.print(idx);
        Serial.println(" to 12:00 (default)");
      } else {
        int colon = t.indexOf(':');
        feedHours[idx] = t.substring(0, colon).toInt();
        feedMinutes[idx] = t.substring(colon + 1).toInt();
        // Treat "00:00" as blank/default
        if (feedHours[idx] == 0 && feedMinutes[idx] == 0) {
          feedHours[idx] = 12;
          feedMinutes[idx] = 0;
          Serial.print("Set slot ");
          Serial.print(idx);
          Serial.println(" to 12:00 (default due to 00:00)");
        } else {
          feedHours[idx] = constrain(feedHours[idx], 0, 23);
          feedMinutes[idx] = constrain(feedMinutes[idx], 0, 59);
          Serial.print("Set slot ");
          Serial.print(idx);
          Serial.print(" to ");
          Serial.print(feedHours[idx]);
          Serial.print(":");
          Serial.println(feedMinutes[idx]);
        }
      }
      idx++;
      start = end + 1;
    }
    // Zero out unused slots
    for (int i = feedCount; i < MAX_FEEDS; i++) {
      feedHours[i] = 0;
      feedMinutes[i] = 0;
      Serial.print("Zeroed slot ");
      Serial.println(i);
    }
  }
  saveSettings();

  Serial.println("Saved settings:");
  Serial.print("defaultPortionSize: ");
  Serial.println(defaultPortionSize);
  Serial.print("utcOffset: ");
  Serial.println(utcOffset);
  Serial.print("feedCount: ");
  Serial.println(feedCount);
  for (int i = 0; i < feedCount; i++) {
    Serial.print("Feed ");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(feedHours[i]);
    Serial.print(":");
    Serial.println(feedMinutes[i]);
  }

  server.send(200, "text/plain", "Settings updated");
}

void handleFeed() {
  String size = server.arg("size");
  int portionSize = MEDIUM_POS;
  if (size == "small") portionSize = SMALL_POS;
  else if (size == "medium") portionSize = MEDIUM_POS;
  else if (size == "large") portionSize = LARGE_POS;
  Serial.print("Manual feed requested: ");
  Serial.println(size);
  feedPet(portionSize);
  server.send(200, "text/plain", "Fed: " + size + " portion");
}

void handleStatus() {
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  int currentHour = (epochTime % 86400L) / 3600;
  int currentMinute = (epochTime % 3600) / 60;

  char nowStr[6];
  sprintf(nowStr, "%02d:%02d", currentHour, currentMinute);

  // Find next feed times
  String nextFeed = "";
  for (int i = 0; i < feedCount; i++) {
    if (currentHour < feedHours[i] || (currentHour == feedHours[i] && currentMinute < feedMinutes[i])) {
      char buf[6];
      sprintf(buf, "%02d:%02d", feedHours[i], feedMinutes[i]);
      if (nextFeed.length() > 0) nextFeed += ", ";
      nextFeed += buf;
    }
  }
  if (nextFeed == "") {
    char buf[12];
    sprintf(buf, "Tomorrow %02d:%02d", feedHours[0], feedMinutes[0]);
    nextFeed = String(buf);
  }

  String portionStr = "medium";
  if (defaultPortionSize == SMALL_POS) portionStr = "small";
  else if (defaultPortionSize == LARGE_POS) portionStr = "large";

  String json = "{";
  json += "\"now\":\"" + String(nowStr) + "\",";
  json += "\"next\":\"" + nextFeed + "\",";
  json += "\"portion\":\"" + portionStr + "\",";
  json += "\"utcOffset\":" + String(utcOffset) + ",";
  json += "\"feedCount\":" + String(feedCount) + ",";
  json += "\"times\":[";
  for (int i = 0; i < feedCount; i++) {
    if (i > 0) json += ",";
    char buf[8];
    sprintf(buf, "\"%02d:%02d\"", feedHours[i], feedMinutes[i]);
    json += buf;
  }
  json += "]";
  json += "}";
  server.send(200, "application/json", json);
}

void saveSettings() {
  EEPROM.put(ADDR_PORTION, defaultPortionSize);
  EEPROM.put(ADDR_UTCOFFSET, utcOffset);
  EEPROM.put(ADDR_FEEDCOUNT, feedCount);
  EEPROM.put(ADDR_FEEDHOURS, feedHours);
  EEPROM.put(ADDR_FEEDMINUTES, feedMinutes);
  EEPROM.commit();
  Serial.print("EEPROM.commit()");
}

void loadSettings() {
  Serial.print("loadSettings()");
  EEPROM.get(ADDR_PORTION, defaultPortionSize);
  EEPROM.get(ADDR_UTCOFFSET, utcOffset);
  EEPROM.get(ADDR_FEEDCOUNT, feedCount);
  EEPROM.get(ADDR_FEEDHOURS, feedHours);
  EEPROM.get(ADDR_FEEDMINUTES, feedMinutes);
  // Validate
  feedCount = constrain(feedCount, 1, MAX_FEEDS);
  for (int i = 0; i < feedCount; i++) {
    feedHours[i] = constrain(feedHours[i], 0, 23);
    feedMinutes[i] = constrain(feedMinutes[i], 0, 59);
  }
}
