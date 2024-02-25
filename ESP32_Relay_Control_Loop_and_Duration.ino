#include <WiFiManager.h>
#include <EEPROM.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <time.h>  // Include time library for NTP
#include <Preferences.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

Preferences preferences;

const char* adminUsername = "admin";
const char* adminPassword = "admin";

bool isAuthenticated = false;

const int relayPin1 = 26;
const int relayPin2 = 27;

int loopInterval1 = -1;
int relayDuration1 = -1;
unsigned long loopStartTime1 = millis();
bool relayIsOn1 = false;

int loopInterval2 = -1;
int relayDuration2 = -1;
unsigned long loopStartTime2 = millis();
bool relayIsOn2 = false;

bool countdownPaused1 = false;
unsigned long pauseStartTime1 = 0;
bool countdownPaused2 = false;
unsigned long pauseStartTime2 = 0;

AsyncWebServer server(80);

void setup();
void loop();
void handleRelay(int relayPin, bool& relayIsOn, unsigned long& loopStartTime, bool& countdownPaused, unsigned long& pauseStartTime, int loopInterval, int relayDuration);
String calculateCountdown(unsigned long loopStartTime, int loopInterval, bool relayIsOn, bool countdownPaused, unsigned long pauseStartTime, int relayDuration);
void setupServerRoutes();

const char* loginPage = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Login</title>
<style>
body { font-family: Arial, sans-serif; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; min-height: 100vh; background-color: #f0f0f0; }
.login-container { background-color: #fff; padding: 20px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); text-align: center; max-width: 400px; width: 100%; }
.login-container h1 { margin-top: 0; }
.login-container input[type='text'], .login-container input[type='password'] { width: 100%; padding: 12px; margin: 10px 0; border: 1px solid #ccc; border-radius: 5px; box-sizing: border-box; }
.login-container button { width: 100%; padding: 12px; border: none; border-radius: 5px; background-color: #0084ff; color: white; cursor: pointer; transition: background-color 0.3s ease; }
.login-container button:hover { background-color: #0056b3; }
</style>
<script>
window.onload = function() {
    function updateCountdown() {
        fetch('/countdown1').then(response => response.text()).then(text => {
            let parts = text.split(',');
            let countdown = parts[0];
            let state = parts[1];
            document.getElementById('relay1Countdown').innerText = 'Relay 1 Countdown: ' + countdown;
            document.getElementById('relay1Countdown').className = state === 'on' ? 'countdown red' : 'countdown';
            sessionStorage.setItem('relay1Countdown', countdown);
            sessionStorage.setItem('relay1Paused', state === 'on' ? 'true' : 'false');
        });
        fetch('/countdown2').then(response => response.text()).then(text => {
            let parts = text.split(',');
            let countdown = parts[0];
            let state = parts[1];
            document.getElementById('relay2Countdown').innerText = 'Relay 2 Countdown: ' + countdown;
            document.getElementById('relay2Countdown').className = state === 'on' ? 'countdown red' : 'countdown';
            sessionStorage.setItem('relay2Countdown', countdown);
            sessionStorage.setItem('relay2Paused', state === 'on' ? 'true' : 'false');
        });
    }
    updateCountdown();
    setInterval(updateCountdown, 1000);
    function displayDateTime() {
        let currentDate = new Date().toLocaleDateString();
        let currentTime = new Date().toLocaleTimeString();
        document.getElementById('dateTime').innerText = "Date: " + currentDate + " Time: " + currentTime;
    }
    displayDateTime();
    setInterval(displayDateTime, 1000);
}
</script>
</head>
<body>
<div class='login-container'>
<h1>Relay Control Loop and Duration.</h1>
<h2 id="dateTime"></h2>
<h3>
<div id='relay1Countdown' class='countdown'></div>
<div id='relay2Countdown' class='countdown'></div>
</h3>
<form action='/login' method='POST'>
<input type='text' name='username' placeholder='Username' required><input type='password' name='password' placeholder='Password' required>
<button type='submit'>Login</button>
</form>
</div>
</body>
</html>
)=====";

void setup() {
    Serial.begin(115200);
    pinMode(relayPin1, OUTPUT);
    pinMode(relayPin2, OUTPUT);
    digitalWrite(relayPin1, LOW);
    digitalWrite(relayPin2, LOW);

    WiFiManager wifiManager;
    if (!wifiManager.autoConnect("ESP32WiFi")) {
        Serial.println("Failed to connect and hit timeout");
        delay(3000);
        ESP.restart();
        delay(5000);
    }
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    configTime(0, 0, "pool.ntp.org");
    while (!time(nullptr)) {
        delay(1000);
        Serial.println("Waiting for time to be set...");
    }

    setupServerRoutes();
    server.begin();
    preferences.begin("working_hours", false);
    timeClient.begin();
    EEPROM.begin(512);
    EEPROM.get(0, loopInterval1);
    EEPROM.get(4, relayDuration1);
    EEPROM.get(8, loopInterval2);
    EEPROM.get(12, relayDuration2);
    loopStartTime1 = millis();
    loopStartTime2 = millis();
    countdownPaused1 = false;
    countdownPaused2 = false;
}

void loop() {
    timeClient.update();
    int currentHour = timeClient.getHours();
    int currentMinute = timeClient.getMinutes();

    String startHourStr = preferences.getString("startHour", "00:00");
    String stopHourStr = preferences.getString("stopHour", "00:00");
    int startHour = startHourStr.substring(0, 2).toInt();
    int startMinute = startHourStr.substring(3).toInt();
    int stopHour = stopHourStr.substring(0, 2).toInt();
    int stopMinute = stopHourStr.substring(3).toInt();
    bool withinWorkingHours = (currentHour > startHour || (currentHour == startHour && currentMinute >= startMinute)) &&
                              (currentHour < stopHour || (currentHour == stopHour && currentMinute <= stopMinute));

    if (withinWorkingHours) {
        handleRelay(relayPin1, relayIsOn1, loopStartTime1, countdownPaused1, pauseStartTime1, loopInterval1, relayDuration1);
        handleRelay(relayPin2, relayIsOn2, loopStartTime2, countdownPaused2, pauseStartTime2, loopInterval2, relayDuration2);
    } else {
        // Outside of working hours, ensure relays are off and countdown is paused
        loopInterval1 = -1;
        loopInterval2 = -1;
        relayDuration1 = -1;
        relayDuration2 = -1;
        digitalWrite(relayPin1, LOW);
        digitalWrite(relayPin2, LOW);
        relayIsOn1 = false;
        relayIsOn2 = false;
        countdownPaused1 = true;
        countdownPaused2 = true;
    }

    delay(1000);
}

void handleRelay(int relayPin, bool& relayIsOn, unsigned long& loopStartTime, bool& countdownPaused, unsigned long& pauseStartTime, int loopInterval, int relayDuration) {
    unsigned long currentTime = millis();
    if (!relayIsOn && !countdownPaused && currentTime - loopStartTime >= (unsigned long)loopInterval * 60000) {
        digitalWrite(relayPin, HIGH);
        relayIsOn = true;
        countdownPaused = true;
        pauseStartTime = currentTime;
    } else if (relayIsOn && countdownPaused && currentTime - pauseStartTime >= (unsigned long)relayDuration * 1000) {
        digitalWrite(relayPin, LOW);
        relayIsOn = false;
        countdownPaused = false;
        loopStartTime = currentTime;
    }
}

String calculateCountdown(unsigned long loopStartTime, int loopInterval, bool relayIsOn, bool countdownPaused, unsigned long pauseStartTime, int relayDuration) {
    unsigned long currentTime = millis();
    long remainingTime = countdownPaused ? (long)relayDuration * 1000 - (currentTime - pauseStartTime) : (long)loopInterval * 60000 - (currentTime - loopStartTime);
    if (remainingTime < 0) remainingTime = 0;
    int minutes = remainingTime / 60000;
    int seconds = (remainingTime % 60000) / 1000;
    return String(minutes) + "m " + String(seconds) + "s";
}

void setupServerRoutes() {
    server.on("/login", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/html", loginPage);
    });

    server.on("/login", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (request->hasParam("username", true) && request->hasParam("password", true)) {
            String username = request->getParam("username", true)->value();
            String password = request->getParam("password", true)->value();
            if (username == adminUsername && password == adminPassword) {
                isAuthenticated = true;
                request->send(200, "text/html", "<script>window.location.href='/';</script>");
            } else {
                request->send(401, "text/html", "<p>Invalid credentials. <a href='/login'>Try again</a></p>");
            }
        } else {
            request->send(400, "text/plain", "Invalid Request");
        }
    });

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!isAuthenticated) {
            request->redirect("/login");
        } else {
            String startHour = preferences.getString("startHour", "00:00");
            String stopHour = preferences.getString("stopHour", "00:00");
            String html = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>ESP32 Relay Control</title><style>body { font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #f0f0f0; }.container { max-width: 600px; margin: auto; padding: 20px; background-color: #fff; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); }h1 { color: #333; }form { margin-bottom: 20px; }label, input, button { display: block; width: 100%; }input, button { padding: 10px; margin-top: 5px; }button { background-color: #0084ff; color: white; border: none; cursor: pointer; }button:hover { background-color: #0056b3; }.countdown { color: #333; margin: 15px 0; }.logout {  background-color: red; color: white; border: none; cursor: pointer; }</style></head><body><div class=\"container\"><h1>Relay Control Loop and Duration.</h1><h2 id=\"dateTime\"></h2><h3><div id='relay1Countdown' class='countdown'></div><div id='relay2Countdown' class='countdown'></div></h3><form action='/updateWorkingHours' method='POST'><label for=\"startHour\">Start Time:</label><input type=\"time\" id=\"startHour\" name=\"startHour\" value=\"" + startHour + "\" required><br><label for=\"stopHour\">Stop Time:</label><input type=\"time\" id=\"stopHour\" name=\"stopHour\" value=\"" + stopHour + "\" required><br><button type='submit'>Update Working Hours</button></form><h3>Relay 1 Settings</h3><div id='countdown1' class='countdown'></div><form action='/updateSettings1' method='POST'><label for=\"interval1\">Interval Loop (min):</label><input type='number' id=\"interval1\" name='interval1' value='" + String(loopInterval1) + "' min='-1'><br><label for=\"duration1\">Duration (sec):</label><input type='number' id=\"duration1\" name='duration1' value='" + String(relayDuration1) + "' min='-1'><br><button type='submit'>Update Relay 1</button></form><h3>Relay 2 Settings</h3><div id='countdown2' class='countdown'></div><form action='/updateSettings2' method='POST'><label for=\"interval2\">Interval Loop (min):</label><input type='number' id=\"interval2\" name='interval2' value='" + String(loopInterval2) + "' min='-1'><br><label for=\"duration2\">Duration (sec):</label><input type='number' id=\"duration2\" name='duration2' value='" + String(relayDuration2) + "' min='-1'><br><button type='submit'>Update Relay 2</button></form><div align='center'><button onclick=\"window.location.href='/logout'\" class='logout'>Logout</button></div></div><script>window.onload = function() {function updateCountdown() {fetch('/countdown1').then(response => response.text()).then(text => {let parts = text.split(',');let countdown = parts[0];let state = parts[1];document.getElementById('countdown1').innerText = 'Relay 1 Countdown: ' + countdown;document.getElementById('countdown1').className = state === 'on' ? 'countdown red' : 'countdown';});fetch('/countdown2').then(response => response.text()).then(text => {let parts = text.split(',');let countdown = parts[0];let state = parts[1];document.getElementById('countdown2').innerText = 'Relay 2 Countdown: ' + countdown;document.getElementById('countdown2').className = state === 'on' ? 'countdown red' : 'countdown';});}updateCountdown();setInterval(updateCountdown, 1000);}</script></body></html>";
            request->send(200, "text/html", html);
        }
    });

    server.on("/updateWorkingHours", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (request->hasParam("startHour", true) && request->hasParam("stopHour", true)) {
            String startHour = request->getParam("startHour", true)->value();
            String stopHour = request->getParam("stopHour", true)->value();
            preferences.putString("startHour", startHour);
            preferences.putString("stopHour", stopHour);
            request->redirect("/");
        } else {
            request->send(400, "text/plain", "Invalid Request");
        }
    });

    server.on("/updateSettings1", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (request->hasParam("interval1", true) && request->hasParam("duration1", true)) {
            loopInterval1 = request->getParam("interval1", true)->value().toInt();
            relayDuration1 = request->getParam("duration1", true)->value().toInt();
            EEPROM.put(0, loopInterval1);
            EEPROM.put(4, relayDuration1);
            EEPROM.commit();
            loopStartTime1 = millis();
            countdownPaused1 = false;
            relayIsOn1 = false;
            request->redirect("/");
        } else {
            request->send(400, "text/plain", "Invalid Request");
        }
    });

    server.on("/updateSettings2", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (request->hasParam("interval2", true) && request->hasParam("duration2", true)) {
            loopInterval2 = request->getParam("interval2", true)->value().toInt();
            relayDuration2 = request->getParam("duration2", true)->value().toInt();
            EEPROM.put(8, loopInterval2);
            EEPROM.put(12, relayDuration2);
            EEPROM.commit();
            loopStartTime2 = millis();
            countdownPaused2 = false;
            relayIsOn2 = false;
            request->redirect("/");
        } else {
            request->send(400, "text/plain", "Invalid Request");
        }
    });

    server.on("/logout", HTTP_GET, [](AsyncWebServerRequest* request) {
        isAuthenticated = false;
        request->redirect("/login");
    });

    server.on("/countdown1", HTTP_GET, [=](AsyncWebServerRequest* request) {
        String countdown = calculateCountdown(loopStartTime1, loopInterval1, relayIsOn1, countdownPaused1, pauseStartTime1, relayDuration1);
        request->send(200, "text/plain", countdown);
    });

    server.on("/countdown2", HTTP_GET, [=](AsyncWebServerRequest* request) {
        String countdown = calculateCountdown(loopStartTime2, loopInterval2, relayIsOn2, countdownPaused2, pauseStartTime2, relayDuration2);
        request->send(200, "text/plain", countdown);
    });
}
