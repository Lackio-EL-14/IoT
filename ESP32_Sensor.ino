#include <WiFi.h>

// ========================================
// ========== CLASE SENSOR DISTANCIA ======
// ========================================
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
    // Envía pulso ultrasónico
    digitalWrite(pinTrig, LOW);
    delayMicroseconds(2);
    digitalWrite(pinTrig, HIGH);
    delayMicroseconds(10);
    digitalWrite(pinTrig, LOW);

    // Mide duración del eco
    long duracion = pulseIn(pinEcho, HIGH);
    // Calcular distancia en cm (343 m/s → 0.0343 cm/µs)
    float distancia = duracion * 0.0343 / 2.0;
    return distancia;
  }
};

// ========================================
// ========== CLASE TCP CLIENT ============
// ========================================
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
    return (millis() - lastPong) > 15000;
  }
};

// ========================================
// ======== CLASE SENSOR SYSTEM ===========
// ========================================
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
    // Procesar mensajes del servidor
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

    // Enviar distancia cada 5s
    unsigned long now = millis();
    if (now - lastSend > 5000) {
      float distancia = sensor.leerDistanciaCM();

      String json = "{\"method\":\"PUT\",\"data\":{\"distance\":";
      json += String(distancia, 2);
      json += "}}\n";

      tcp.sendJson(json);
      Serial.println("[DATA] Sent: " + json);
      lastSend = now;
    }

    // Timeout sin PONG
    if (tcp.connectionTimeout()) {
      Serial.println("[WARN] No PONG detected, reconnecting TCP...");
      tcp.disconnect();
      delay(2000);
      tcp.connectServer();
    }
  }
};

// ========================================
// =========== CONFIGURACIÓN ==============
// ========================================
const char* ssid = "Flia LAMAS";
const char* password = "kf142004";
const char* server_ip = "192.168.0.10";
const int server_port = 10000;

SensorDistancia sensorDistancia;
TcpClient tcp;
SensorSystem* sistema;

// ========================================
// ============= SETUP ====================
// ========================================
void setup() {
  // Configura los pines del sensor ultrasónico
  sensorDistancia.setPins(27, 25); // Trig, Echo (ajústalos según tu conexión)

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

// ========================================
// =============== LOOP ===================
// ========================================
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
    delay(2000);
    tcp.connectServer();
    return;
  }

  // Actualización del sistema
  sistema->update();
  delay(1000);
}

