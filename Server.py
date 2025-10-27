import socket
import json
import threading

class JSONSocket:
    @staticmethod
    def send(conn, data):
        try:
            msg = json.dumps(data).encode()
            conn.sendall(msg + b"\n")
        except Exception as e:
            print(f"[ERROR] Envío fallido: {e}")

    @staticmethod
    def recv(conn):
        try:
            data = conn.recv(1024).decode().strip()
            if not data:
                return None
            return json.loads(data)
        except:
            return None

class LedLogic:
    @staticmethod
    def get_led_action(distance):
        if distance <= 10:
            return [
                {"led": "blue", "action": "blink", "interval": 0.2},
                {"led": "green", "action": "blink", "interval": 0.2},
                {"led": "yellow", "action": "blink", "interval": 0.2},
                {"led": "red", "action": "blink", "interval": 0.2},
            ]
        elif distance < 20:
            return [
                {"led": "blue", "action": "blink", "interval": 0.2},
                {"led": "green", "action": "blink", "interval": 0.4},
                {"led": "yellow", "action": "blink", "interval": 0.6},
                {"led": "red", "action": "blink", "interval": 0.8},
            ]
        elif distance < 30:
            return [
                {"led": "blue", "action": "off"},
                {"led": "green", "action": "blink", "interval": 0.4},
                {"led": "yellow", "action": "blink", "interval": 0.6},
                {"led": "red", "action": "blink", "interval": 0.8},
            ]
        elif distance < 40:
            return [
                {"led": "blue", "action": "off"},
                {"led": "green", "action": "off"},
                {"led": "yellow", "action": "blink", "interval": 0.6},
                {"led": "red", "action": "blink", "interval": 0.8},
            ]
        elif 40 <= distance <= 50:
            return [{"led": "red", "action": "blink", "interval": 0.2}]
        elif 50 < distance <= 60:
            return [{"led": "red", "action": "on"}]
        elif 60 < distance <= 70:
            return [{"led": "yellow", "action": "blink", "interval": 0.2}]
        elif 70 < distance <= 80:
            return [{"led": "yellow", "action": "on"}]
        elif 80 < distance <= 90:
            return [{"led": "green", "action": "blink", "interval": 0.2}]
        elif 90 < distance <= 100:
            return [{"led": "green", "action": "on"}]
        elif 100 < distance <= 110:
            return [{"led": "blue", "action": "blink", "interval": 0.2}]
        elif 110 < distance <= 120:
            return [{"led": "blue", "action": "on"}]
        else:
            return [
                {"led": "blue", "action": "off"},
                {"led": "green", "action": "off"},
                {"led": "yellow", "action": "off"},
                {"led": "red", "action": "off"},
            ]

class ClientHandler(threading.Thread):
    def __init__(self, conn, addr, server):
        super().__init__()
        self.conn = conn
        self.addr = addr
        self.server = server
        self.role = None
        self.alive = True

    def run(self):
        print(f"[NUEVA CONEXIÓN] {self.addr}")
        reg = JSONSocket.recv(self.conn)
        if not reg or reg.get("method") != "REGISTER" or "role" not in reg:
            JSONSocket.send(self.conn, {"method": "ERROR", "msg": "Registro inválido"})
            self.conn.close()
            return

        self.role = reg["role"].upper()
        self.server.connections[self.role] = self.conn
        JSONSocket.send(self.conn, {"method": "ACK", "msg": f"REGISTER OK ({self.role})"})
        print(f"[{self.role}] Registrado correctamente.")

        while self.alive:
            data = JSONSocket.recv(self.conn)
            if not data:
                print(f"[{self.role}] Desconectado.")
                self.alive = False
                break
            self.handle_message(data)

        if self.role in self.server.connections:
            del self.server.connections[self.role]
        self.conn.close()

    def handle_message(self, data):
        method = data.get("method")
        if method == "PING":
            JSONSocket.send(self.conn, {"method": "PONG"})
        elif method == "PUT":
            if "data" in data:
                distance = data["data"].get("distance")
                print(f"[SENSOR] Data recibida → {data['data']}")
                if distance is not None and "ACT" in self.server.connections:
                    actions = LedLogic.get_led_action(distance)
                    # Envía todos los LEDs en un solo JSON
                    JSONSocket.send(self.server.connections["ACT"], {"method": "PUT", "leds": actions})
        elif method == "GET":
            JSONSocket.send(self.conn, {"method": "ERROR", "msg": "Variable desconocida"})
        else:
            JSONSocket.send(self.conn, {"method": "ERROR", "msg": "Método desconocido"})

class StayAlertServer:
    def __init__(self, host="0.0.0.0", port=10000):
        self.host = host
        self.port = port
        self.connections = {}

    def start(self):
        print(f"[SERVER] Iniciando en {self.host}:{self.port}...")
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.bind((self.host, self.port))
        server_socket.listen(5)
        print("[SERVER] Esperando conexiones...")

        while True:
            conn, addr = server_socket.accept()
            handler = ClientHandler(conn, addr, self)
            handler.start()

if __name__ == "__main__":
    server = StayAlertServer()
    server.start()
