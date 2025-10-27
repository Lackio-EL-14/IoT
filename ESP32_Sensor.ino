#include <WiFi.h>

class SensorDistancia {
private:
  int pinTrig;
  int pinEcho;

public:
  SensorDistancia() {}

  void setPins(int trig, int echo) {
    pinTrig = trig;
    pinEcho = echo;
    pinMode(pinTrig, OUTPUT);
    pinMode(pinEcho, INPUT);
  }

  float leerDistanciaCM() {
    digitalWrite(pinTrig, LOW);
    delayMicroseconds(2);
    digitalWrite(pinTrig, HIGH);
    delayMicroseconds(10);
    digitalWrite(pinTrig, LOW);


    long duracion = pulseIn(pinEcho, HIGH);

    float distancia = duracion * 0.0343 / 2.0;
    return distancia;
  }
};

class TcpClient {
private:
  WiFiClient client;
  const char* serverIP;
  int port;
  unsigned long lastPing;
  unsigned long lastPong;

public:
  TcpClient() {
    lastPing = 0;
    lastPong = millis();
  }

  void setServer(const char* ip, int p) {
    serverIP = ip;
    port = p;
  }

  void disconnect() {
    if (client.connected()) {
      client.stop();
      Serial.println("[TCP] Connection closed.");
    }
  }

  bool connectServer() {
    Serial.println("[TCP] Connecting to server...");
    disconnect();

    if (client.connect(serverIP, port)) {
      Serial.println("[TCP] Connected to server!");
      client.print("{\"method\":\"REGISTER\",\"role\":\"SENSOR\"}\n");
      lastPing = millis();
      lastPong = millis();
      return true;
    } else {
      Serial.println("[TCP] Connection failed.");
      return false;
    }
  }

  bool isConnected() {
    return client.connected();
  }

  bool available() {
    return client.available();
  }

  String readLine() {
    return client.readStringUntil('\n');
  }

  void sendJson(const String& json) {
    if (isConnected()) {
      client.print(json + "\n");
    }
  }

  void sendPing() {
    unsigned long now = millis();
    if (now - lastPing > 5000) {
      if (isConnected()) {
        client.print("{\"method\":\"PING\"}\n");
        Serial.println("[PING] Sent to server.");
        lastPing = now;
      }
    }
  }

  void handlePong() {
    lastPong = millis();
  }

  bool connectionTimeout() {
    return (millis() - lastPong) > 10000;
  }
};

class SensorSystem {
private:
  SensorDistancia& sensor;
  TcpClient& tcp;
  unsigned long lastSend;

public:
  SensorSystem(SensorDistancia& s, TcpClient& c)
    : sensor(s), tcp(c) {
    lastSend = 0;
  }

  void start() {
    Serial.begin(115200);
    Serial.println("Starting distance sensor system...");
  }

  void processMessage(const String& msg) {
    if (msg.indexOf("\"method\":\"PONG\"") >= 0) {
      Serial.println("[PONG] Received from server.");
      tcp.handlePong();
    }
  }

  void update() {
   
    if (tcp.available()) {
      String msg = tcp.readLine();
      msg.trim();
      if (msg.length() > 0) {
        Serial.print("[TCP RX] "); Serial.println(msg);
        processMessage(msg);
      }
    }

    // Enviar PING cada 5s
    tcp.sendPing();

    // Enviar distancia cada 2s
    unsigned long now = millis();
    if (now - lastSend > 2000) {
      float distancia = sensor.leerDistanciaCM();

      String json = "{\"method\":\"PUT\",\"data\":{\"distance\":";
      json += String(distancia, 2);
      json += "}}\n";

      tcp.sendJson(json);
      Serial.println("[DATA] Sent: " + json);
      lastSend = now;
    }

    
    if (tcp.connectionTimeout()) {
      Serial.println("[WARN] No PONG detected, reconnecting TCP...");
      tcp.disconnect();
      delay(2000);
      tcp.connectServer();
    }
  }
};


const char* ssid = "Redmi-Kf14";
const char* password = "vivaellol";
const char* server_ip = "10.105.87.110";
const int server_port = 10000;

SensorDistancia sensorDistancia;
TcpClient tcp;
SensorSystem* sistema;


void setup() {

  sensorDistancia.setPins(27, 25); 

  Serial.begin(115200);
  Serial.println("[WIFI] Connecting...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WIFI] Connected!");

  tcp.setServer(server_ip, server_port);
  tcp.connectServer();

  sistema = new SensorSystem(sensorDistancia, tcp);
  sistema->start();
}

void loop() {
  // Reconección WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Lost connection. Reconnecting...");
    WiFi.disconnect();
    delay(2000);
    WiFi.begin(ssid, password);
    return;
  }

  // Reconección TCP
  if (!tcp.isConnected()) {
    Serial.println("[TCP] Lost connection, retrying...");
    tcp.disconnect();
    delay(1000);
    tcp.connectServer();
    return;
  }


  sistema->update();
  delay(200);
}

