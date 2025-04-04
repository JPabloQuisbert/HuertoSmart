#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include "DHTesp.h"
#include <EEPROM.h>
#include <string.h>  

//Crea un AP para acceder a la configuracion de red del ESP
const char *ssid_ap = "MyHuerto";  // SSID del punto de acceso
const char *password_ap = "MyHuerto";  // Contraseña del AP

char ssid_eeprom[50];
char pass_eeprom[50];
int direccionesEEPROM[7]={100,100+52,100+(2*52),100+(3*52),100+(4*52),100+(5*52),100+(6*52)}; //Los pines D1 al D7 como salida

//52 bytes de tamanio
struct Salida {
  char nombre[50];  // Nombre de la salida de 50 caracteres
  u_int8_t gpio;        // Número de pin asociado
  bool estado;    // Estado de la salida (true = ON, false = OFF)
};


String gpioToPin(int gpio){
  switch (gpio){
  case 5:  return "D1";
  case 4:  return "D2";
  case 0:  return "D3";
  case 2:  return "D4";
  case 14:  return "D5";
  case 12:  return "D6";
  case 13:  return "D7";
  default: return "Desconocido";
  }
}

u_int8_t pinToDpin(int pin){
  switch (pin){
  case 1:  return D1;
  case 2:  return D2;
  case 3:  return D3;
  case 4:  return D4;
  case 5:  return D5;
  case 6:  return D6;
  case 7:  return D7;
  default: return NULL;
  }
}
// Definimos los pines que queremos controlar
const u_int8_t pines[] = {D1, D2, D3, D4, D5, D6, D7};  // Pines digitales del ESP8266
const u_int8_t numPines = sizeof(pines) / sizeof(pines[0]);  // Número de pines en el arreglo

ESP8266WebServer server(80);  // Servidor en el puerto 80
WebSocketsServer webSocket = WebSocketsServer(81); // El puerto de WebSocket será el 81

//Variables del Sensor DHT
DHTesp dht;
float humidity;
float temperature;

unsigned long previousMillis = 0;  // Almacena el tiempo en milisegundos de la última lectura
const long interval = 2000;        //tiempo que tarda en leer el DHT22 


void borrarEERPOM(){
  // write a 0 to all 512 bytes of the EEPROM
  for (int i = 0; i < 512; i++) { EEPROM.write(i, 0); }
  EEPROM.commit();
}
//Guardar la SSID y PWD en los primeros 100 espacios de la EEPROM
void saveSSIDyPwdEEPROM(char ssid_eeprom[50],char pass_eeprom[50]){
  Serial.println("Guardando en EEPROM");
  for (int i = 0; i < 50; i++) {
    EEPROM.write(i, ssid_eeprom[i]);    
  }
  EEPROM.commit();
  Serial.println(ssid_eeprom);
  for (int j = 50; j < 100; j++) {
    EEPROM.write(j, pass_eeprom[j-50]);
  }
  Serial.println(pass_eeprom);
  EEPROM.commit();
}

// Función para guardar una salida en la EEPROM
void guardarSalidaEnEEPROM(int direccion, Salida salida) {  
  // Guardar cada carácter del nombre
  for (int i = 0; i < sizeof(salida.nombre); i++) {
    EEPROM.write(direccion + i, salida.nombre[i]);
  }
  direccion += sizeof(salida.nombre);  // Avanzar la dirección después de guardar el nombre
  
  // Guardar el pin (1 byte)
  EEPROM.write(direccion, salida.gpio);         
  direccion++;
  
  // Guardar el estado (1 byte)
  EEPROM.write(direccion, salida.estado);
  
  // Guardar los cambios en la EEPROM
  EEPROM.commit();
}
// Función para leer el SSID y Password de la EEPROM
void leerSSIDyPwdEEPROM(){
  Serial.println("Leyendo EEPROM SSID y PWD");
  for (int i = 0; i < 50; i++) {
    ssid_eeprom[i] = EEPROM.read(i);
  }
  Serial.println(ssid_eeprom);
  for (int j = 50; j < 100; j++) {
    pass_eeprom[j-50] = EEPROM.read(j);
  }
  Serial.println(pass_eeprom);
  Serial.println("-----------FIN---------");
}
// Función para leer una salida de la EEPROM
Salida leerSalidaDeEEPROM(int direccion) {
  Salida salida;
  
  // Leer el nombre
  char nombre[50];  
  for (int i = 0; i < sizeof(nombre); i++) {
    nombre[i] = EEPROM.read(direccion + i);
  }
  // Copiar el nombre leído a la estructura salida
  strcpy(salida.nombre, nombre);  // Copia la cadena leída en la estructura salida
  direccion += sizeof(nombre);  // Avanzar después del nombre
  
  // Leer el pin
  salida.gpio = EEPROM.read(direccion);
  direccion += 1;
  
  // Leer el estado
  salida.estado = EEPROM.read(direccion);
  
  return salida;
}


// Maneja la solicitud GET para la página principal
void handleRoot() {
  String html = "<html><meta charset='UTF-8'><body>";
  html += "<h1>Conectar a WiFi</h1>";

  // Verificar el estado de la conexión Wi-Fi
  if (WiFi.status() == WL_CONNECTED) {
    // Si está conectado a una red, muestra el SSID y la IP
    html += "<p><strong>Conectado a la red Wi-Fi: </strong>" + WiFi.SSID() + "</p>";
    html += "<p><strong>Dirección IP: </strong>" + WiFi.localIP().toString() + "</p>";
  } else {
    // Si no está conectado a ninguna red
    html += "<p><strong>No está conectado a ninguna red Wi-Fi.</strong></p>";
  }

  // Escanea las redes Wi-Fi disponibles
  int n = WiFi.scanNetworks();
  if (n == 0) {
    html += "<p>No se encontraron redes Wi-Fi.</p>";
  } else {
    html += "<form action='/connect' method='POST'>";
    html += "<label for='ssid'>Selecciona una red Wi-Fi:</label><br>";
    html += "<select name='ssid' id='ssid'>";
    for (int i = 0; i < n; i++) {
      html += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + "</option>";
    }
    html += "</select><br>";

    // Agregar campo para ingresar la contraseña
    html += "<label for='password'>Contraseña de la red Wi-Fi (si es necesario):</label><br>";
    html += "<input type='password' name='password' id='password'><br>";
    html += "<input type='submit' value='Conectar'>";
    html += "</form>";
    html += "<h2>Control de Pines</h2>";
    html += "<label for='pin1'>D1</label><input type='checkbox' id='pin1' onchange='controlPin(5, this.checked)'><br>";
    html += "<label for='pin2'>D2</label><input type='checkbox' id='pin2' onchange='controlPin(4, this.checked)'><br>";
    html += "<label for='pin3'>D3</label><input type='checkbox' id='pin3' onchange='controlPin(0, this.checked)'><br>";
    html += "<label for='pin4'>D4</label><input type='checkbox' id='pin4' onchange='controlPin(2, this.checked)'><br>";
    html += "<label for='pin5'>D5</label><input type='checkbox' id='pin5' onchange='controlPin(14, this.checked)'><br>";
    html += "<label for='pin4'>D6</label><input type='checkbox' id='pin6' onchange='controlPin(12, this.checked)'><br>";
    html += "<label for='pin5'>D7</label><input type='checkbox' id='pin7' onchange='controlPin(13, this.checked)'><br>";
  }
  html += "<script>";
  html += "function controlPin(pin, state) {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  var stateStr = state ? 'ON' : 'OFF';";  // 'state' será true o false
  html += "  xhr.open('GET', '/control?pin=' + pin + '&state=' + stateStr, true);";
  html += "  xhr.send();";
  html += "}";
  html += "</script>";

  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// Maneja la solicitud POST para conectar a la red seleccionada
void handleConnect() {
  String ssid = server.arg("ssid");  // Obtenemos el SSID seleccionado
  String password = server.arg("password");  // Obtenemos la contraseña de la red

  // Imprime los valores para depuración
  Serial.print("Intentando conectar a la red: ");
  Serial.println(ssid);
  Serial.print("Contraseña: ");
  Serial.println(password);

  // Desconectamos cualquier red previamente conectada
  WiFi.disconnect();  

  // Intentamos conectar a la red
  WiFi.begin(ssid.c_str(), password.c_str());

  // Esperamos a que se conecte
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 10) {
    delay(1000);
    timeout++;
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Enviamos una página de éxito
    server.send(200, "text/html", "<html><body><h1>Conexión exitosa!</h1><p>" +WiFi.SSID() +"</p><p>IP del dispositivo: " + WiFi.localIP().toString() + "</p></body></html>");
    Serial.println("Conexión exitosa.");
    Serial.print("SSID del dispositivo: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP del dispositivo: ");
    Serial.println(WiFi.localIP());
    strcpy(ssid_eeprom, ssid.c_str());
    strcpy(pass_eeprom, password.c_str());
    saveSSIDyPwdEEPROM(ssid_eeprom,pass_eeprom);
    // // Cambiamos el modo del ESP a STA (Cliente) y apagamos el AP
    // WiFi.softAPdisconnect(true);  // Desconecta el punto de acceso
    // Serial.println("Modo AP apagado, ahora en modo Cliente.");
  } else {
    // Si no se conecta, mostramos un mensaje de error
    server.send(200, "text/html", "<html><body><h1>Error al conectar a la red Wi-Fi.</h1><p>Por favor, intenta nuevamente.</p></body></html>");
    Serial.println("No se pudo conectar.");
  }
}

// Ruta POST para agregar salida
void handlePost() {
  if (server.hasArg("nombre") && server.hasArg("pin") && server.hasArg("estado")) {
    Salida datoRecibido;
    server.arg("nombre").toCharArray(datoRecibido.nombre, sizeof(datoRecibido.nombre));
    datoRecibido.gpio=server.arg("pin").toInt();
    datoRecibido.estado=server.arg("estado").toInt();

    guardarSalidaEnEEPROM(direccionesEEPROM[datoRecibido.gpio-1], datoRecibido);

    server.send(200, "text/plain", "Datos actualizados exitosamente");
  } else {
    server.send(400, "text/plain", "Faltan parámetros");
  }
}

void primeraConexion(){
  // Imprime los valores para depuración
  Serial.print("Intentando conectar a la red de eeprom: ");
  Serial.println(ssid_eeprom);
  Serial.print("Contraseña: ");
  Serial.println(pass_eeprom);

  // Desconectamos cualquier red previamente conectada
  WiFi.disconnect();  

  // Intentamos conectar a la red
  WiFi.begin(ssid_eeprom, pass_eeprom);

  // Esperamos a que se conecte
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 10) {
    delay(1000);
    timeout++;
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Enviamos una página de éxito
    server.send(200, "text/html", "<html><body><h1>Conexión exitosa desde eeprom!</h1><p>" +WiFi.SSID() +"</p><p>IP del dispositivo: " + WiFi.localIP().toString() + "</p></body></html>");
    Serial.println("Conexión exitosa desde EEPROM.");
    Serial.print("SSID del dispositivo: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP del dispositivo: ");
    Serial.println(WiFi.localIP());
  } else {
    // Si no se conecta, mostramos un mensaje de error
    server.send(200, "text/html", "<html><body><h1>Error al conectar a la red Wi-Fi de EEPROM.</h1><p>Por favor, intenta nuevamente.</p></body></html>");
    Serial.println("No se pudo conectar de eeprom.");
  }
}
String leer(int addr) {
  byte lectura;
  String strlectura;
  for (int i = addr; i < addr+50; i++) {
     lectura = EEPROM.read(i);
     if (lectura != 255) {
       strlectura += (char)lectura;
     }
  }
  return strlectura;
}
void buscarDatosEEPROM(){
  leer(0).toCharArray(ssid_eeprom, 50);
  leer(50).toCharArray(pass_eeprom, 50);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("Cliente %u desconectado\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("Nuevo cliente conectado desde %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
      }
      break;
    case WStype_TEXT:
      // Puedes agregar lógica aquí si deseas recibir datos desde el cliente
      String mensaje = String((char *)payload);  // Convertir el mensaje a String
        Serial.println("Mensaje recibido: " + mensaje);
        
        if (mensaje == "ON") {
          digitalWrite(pines[3], HIGH);  // Enciende el pin
          Serial.println("Foco Encendido");
        }
        else if (mensaje == "OFF") {
          digitalWrite(pines[3], LOW);  // Apaga el pin
          Serial.println("Foco Apagado");
        }
        else {
          Serial.println("Comando desconocido");
        }
      break;
  }
}

void handleControl() {
  String pin = server.arg("pin");
  String state = server.arg("state");

  int pinNumber = pin.toInt();
  int pinState = (state == "ON") ? HIGH : LOW;

  // Activa o desactiva el pin correspondiente
  digitalWrite(pinNumber, pinState);
  Serial.println(pinState);
  // Responder al cliente
  server.send(200, "text/plain", "Pin " + pin + " set to " + state);
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);

  // Configurar los pines como salidas
  for (int i = 0; i < numPines; i++) {
    pinMode(pines[i], OUTPUT);
  }

  // Serial.println("Guardando salida a1...");
  // Salida a1={"led prueba",pines[3],1};
  // guardarSalidaEnEEPROM(direccionesEEPROM[0],a1);
  // Serial.println("done...!");

  // Inicia el ESP8266 como punto de acceso
  WiFi.softAP(ssid_ap, password_ap);
  Serial.println("Configurando el ESP8266 en modo AP...");
  Serial.print("Dirección IP del AP: ");
  Serial.println(WiFi.softAPIP());
  
  // Enlazamos las rutas que manejarán las peticiones
  server.on("/", HTTP_GET, handleRoot);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/control", HTTP_GET, handleControl);
  server.on("/update", HTTP_POST, handlePost);   // Ruta POST

  // Iniciamos el servidor
  server.begin();
  Serial.println("Servidor iniciado.");

  //Iniciamos el Web Socket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  leerSSIDyPwdEEPROM();
  primeraConexion();
  
  //Iniciamos el sensor
  dht.setup(D0, DHTesp::DHT22); // Conectar al GPIO 16, se usa el pin D0

  //lectura de todas las salidas en la memoria EEPROM
  for (int i = 0; i < numPines; i++){
    Salida b1=leerSalidaDeEEPROM(direccionesEEPROM[i]);
    if (!gpioToPin(b1.gpio).equals("Desconocido")){
      Serial.println("Salida guardada en direccion: ");
      Serial.println(direccionesEEPROM[i]);
      Serial.println(b1.nombre);
      Serial.println(pines[b1.gpio-1]);
      Serial.println(b1.gpio);
      Serial.println(b1.estado);
    }
  }
}

void loop() {
  server.handleClient();  // Maneja las peticiones HTTP
  webSocket.loop();  // Mantiene la comunicación WebSocket

  // Salida b1=leerSalidaDeEEPROM(direccionesEEPROM[0]);

  // Serial.println(b1.nombre);
  // Serial.println(b1.estado);
  // Serial.println(gpioToPin(b1.pin));
  // delay(500);

  // Leer el valor analógico del pin A0 (0-1023)
  int sensorValue = analogRead(A0);
  String sensorValueStr = String(temperature);
  // Enviar el valor de la lectura analógica a todos los clientes conectados
  webSocket.broadcastTXT(sensorValueStr);
  // Escuchar por solicitudes entrantes
  server.handleClient();
  unsigned long currentMillis = millis();  // Obtenemos el tiempo actual en milisegundos
  // Verificamos si ha pasado el intervalo definido
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;  // Actualizamos el tiempo de la última lectura
    humidity = dht.getHumidity();
    temperature = dht.getTemperature();
    // Realizamos la lectura del sensor
    Serial.print(temperature);
    Serial.println(" Se leyo en el tiempo ");  // Enviamos el valor leído al monitor serial
  }
  delay(50);
}