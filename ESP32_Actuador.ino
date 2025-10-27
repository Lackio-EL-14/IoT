#include <WiFi.h>

class Led {
private:
  int pin;
  bool blinking;
  bool state;
  unsigned long lastToggle;
  int interval;

public:
  Led() : pin(-1), blinking(false), state(false), lastToggle(0), interval(0) {}

  void setPin(int p) {
    pin = p;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  int getPin() { return pin; }

  void turnOn() {
    blinking = false;
    state = true;
    digitalWrite(pin, HIGH);
  }

  void turnOff() {
    blinking = false;
    state = false;
    digitalWrite(pin, LOW);
  }

  void setBlink(int ms) {
    blinking = true;
    interval = ms;
    lastToggle = millis();
    state = true;
    digitalWrite(pin, HIGH);
  }

  void update() {
    if (blinking && millis() - lastToggle >= interval) {
      state = !state;
      digitalWrite(pin, state ? HIGH : LOW);
      lastToggle = millis();
    }
  }
};


class TcpClient {
private:
  WiFiClient client;
  const char* serverIP;
  int port;
  unsigned long lastPingTime;
  bool pongReceived;

public:
  TcpClient() : serverIP(nullptr), port(0), lastPingTime(0), pongReceived(true) {}

  void setServer(const char* ip, int p) {
    serverIP = ip;
    port = p;
  }

  bool connectServer() {
    Serial.println("[TCP] Connecting to server...");
    if (client.connect(serverIP, port)) {
      Serial.println("[TCP] Connected to server!");
      client.print("{\"method\":\"REGISTER\",\"role\":\"ACT\"}\n");
      lastPingTime = millis();
      pongReceived = true;
      return true;
    } else {
      Serial.println("[TCP] Connection failed.");
      return false;
    }
  }

  bool isConnected() { return client.connected(); }
  bool available() { return client.available(); }
  String readLine() { return client.readStringUntil('\n'); }

  void sendPing() {
    unsigned long now = millis();
    if (now - lastPingTime > 5000) {
      client.print("{\"method\":\"PING\"}\n");
      pongReceived = false;
      lastPingTime = now;
      Serial.println("[PING] Sent to server.");
    }
  }

  void handlePong(const String& msg) {
    if (msg.indexOf("\"PONG\"") >= 0) {
      pongReceived = true;
      Serial.println("[PONG] Received ");
    }
  }

  bool pingTimeout() {
    return !pongReceived && (millis() - lastPingTime > 10000);
  }

  void reconnect() {
    Serial.println("[TCP] Attempting reconnection...");
    client.stop();
    delay(1500);
    connectServer();
  }
};


class ActuatorSystem {
private:
  Led& redLed;
  Led& yellowLed;
  Led& greenLed;
  Led& blueLed;
  TcpClient& tcp;

public:
  ActuatorSystem(Led& r, Led& y, Led& g, Led& b, TcpClient& t)
    : redLed(r), yellowLed(y), greenLed(g), blueLed(b), tcp(t) {}

  void turnOffLeds() {
    redLed.turnOff();
    yellowLed.turnOff();
    greenLed.turnOff();
    blueLed.turnOff();
  }

  void start() {
    Serial.println("[SYSTEM] Actuator ready.");
    turnOffLeds();
  }

  void setLedAction(String ledColor, String action, float interval = 0) {
    Led* led = nullptr;

    if (ledColor == "red") led = &redLed;
    else if (ledColor == "yellow") led = &yellowLed;
    else if (ledColor == "green") led = &greenLed;
    else if (ledColor == "blue") led = &blueLed;

    if (!led) return;

    if (action == "on") {
      led->turnOn();
      Serial.printf("[LED] %s → ON\n", ledColor.c_str());
    } 
    else if (action == "off") {
      led->turnOff();
      Serial.printf("[LED] %s → OFF\n", ledColor.c_str());
    } 
    else if (action == "blink") {
      led->setBlink((int)(interval * 1000));
      Serial.printf("[LED] %s → BLINK (%.1fs)\n", ledColor.c_str(), interval);
    }
  }

  void processMessage(const String& msg) {
    if (msg.indexOf("\"leds\"") >= 0) {
      int idx = msg.indexOf("[");
      int end = msg.indexOf("]");
      if (idx == -1 || end == -1) return;

      String ledsArray = msg.substring(idx + 1, end);
      int pos = 0;
      while (pos < ledsArray.length()) {
        int s = ledsArray.indexOf("{", pos);
        int e = ledsArray.indexOf("}", s);
        if (s == -1 || e == -1) break;

        String ledJson = ledsArray.substring(s, e + 1);
        String led, action;
        float interval = 0.0;

        int iLed = ledJson.indexOf("\"led\"");
        int iAction = ledJson.indexOf("\"action\"");
        int iInterval = ledJson.indexOf("\"interval\"");

        if (iLed >= 0) {
          int start = ledJson.indexOf("\"", iLed + 5);
          int end = ledJson.indexOf("\"", start + 1);
          led = ledJson.substring(start + 1, end);
        }
        if (iAction >= 0) {
          int start = ledJson.indexOf("\"", iAction + 8);
          int end = ledJson.indexOf("\"", start + 1);
          action = ledJson.substring(start + 1, end);
        }
        if (iInterval >= 0) {
          int start = ledJson.indexOf(":", iInterval) + 1;
          int end = ledJson.indexOf(",", start);
          if (end == -1) end = ledJson.length();
          interval = ledJson.substring(start, end).toFloat();
        }

        setLedAction(led, action, interval);
        pos = e + 1;
      }
    } 
    else if (msg.indexOf("\"PONG\"") >= 0) {
      tcp.handlePong(msg);
    }
  }

  void update() {
    if (!tcp.isConnected()) return;

    while (tcp.available()) {
      String msg = tcp.readLine();
      msg.trim();
      if (msg.isEmpty()) return;

      Serial.print("[TCP RX] "); Serial.println(msg);
      processMessage(msg);
    }

   
    redLed.update();
    yellowLed.update();
    greenLed.update();
    blueLed.update();
  }
};

const char* ssid = "Redmi-Kf14";
const char* password = "vivaellol";
const char* server_ip = "10.105.87.110";
const int server_port = 10000;

Led redLed, yellowLed, greenLed, blueLed;
TcpClient tcp;
ActuatorSystem* actuatorSystem;


void setup() {
  Serial.begin(115200);

  redLed.setPin(32);
  yellowLed.setPin(25);
  greenLed.setPin(26);
  blueLed.setPin(27);

  Serial.println("[WIFI] Connecting...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WIFI] Connected!");

  tcp.setServer(server_ip, server_port);
  tcp.connectServer();

  actuatorSystem = new ActuatorSystem(redLed, yellowLed, greenLed, blueLed, tcp);
  actuatorSystem->start();
}


void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Lost connection. Reconnecting...");
    WiFi.disconnect();
    delay(2000);
    WiFi.begin(ssid, password);
    return;
  }

  if (!tcp.isConnected() || tcp.pingTimeout()) {
    Serial.println("[TCP] Lost connection. Reconnecting...");
    tcp.reconnect();
    delay(1000);
    return;
  }

  tcp.sendPing();
  actuatorSystem->update();
  delay(10);
}

