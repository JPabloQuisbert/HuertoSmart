#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include "DHTesp.h"
#include <EEPROM.h>
#include <string.h>  

//Crea un AP para acceder a la configuracion de red del ESP
const char *ssid_ap = "MyHuerto";  // SSID del punto de acceso
const char *password_ap = "MyHuerto";  // Contraseña del AP

// Dirección IP, máscara de subred y puerta de enlace para asignar al AP
IPAddress local_ip(192, 168, 1, 1);        // IP estática para el AP
IPAddress gateway(192, 168, 1, 1);         // Puerta de enlace
IPAddress subnet(255, 255, 255, 0);        // Máscara de subred

char ssid_eeprom[50];
char pass_eeprom[50];
int direccionesEEPROM[7]={100,100+52,100+(2*52),100+(3*52),100+(4*52),100+(5*52),100+(6*52)}; //Los pines D1 al D7 como salida
String pinLabels[7]={"pin1","pin2","pin3","pin4","pin5","pin6","pin7"};
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

// =================Funciones de manejo de EERPOM
void borrarEERPOM(int direccion){
  // write a 0 to all 512 bytes of the EEPROM
  for (int i = direccion; i < direccion+10; i++) { EEPROM.write(i, 255); }
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
// =================FIN Funciones de manejo de EERPOM


// ================= Manejos Solicitudes al servidor http:
// Maneja la solicitud GET para la página principal
void handleRoot() {
  String html = "<html lang='es'><meta charset='UTF-8'><body>";
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

    //lectura de todas las salidas en la memoria EEPROM
    for (int i = 0; i < numPines; i++){
      Salida b1=leerSalidaDeEEPROM(direccionesEEPROM[i]);
      // html += "<label for='"+pinLabels[i]+"'>"+pinLabels[i]+": "+String(b1.nombre[0]!=255 ? b1.nombre:"")+"</label><input type='checkbox' id='"+pinLabels[i]+"' onchange='controlPin("+String(b1.gpio)+", this.checked)' " + String(b1.estado ? "checked" : "") + "><br>";     
      if (b1.nombre[0]!=255){
        html += "<label for='"+pinLabels[i]+"'>"+pinLabels[i]+": "+String(b1.nombre)+"</label><input type='checkbox' id='"+pinLabels[i]+"' onchange='controlPin("+String(b1.gpio)+", this.checked)' " + String(b1.estado ? "checked" : "") + "><br>";
        digitalWrite(pinToDpin(b1.gpio),b1.estado);
      }
      else{
        html += "<label for='"+pinLabels[i]+"'>"+pinLabels[i]+": sin registro</label><input type='checkbox' id='"+pinLabels[i]+"' onchange='controlPin("+String(b1.gpio)+", this.checked)'><br>";     
        digitalWrite(pinToDpin(b1.gpio),LOW);
      }
    }
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
    
    digitalWrite(pines[datoRecibido.gpio-1],datoRecibido.estado);
    Serial.println(datoRecibido.gpio);
    Serial.println(datoRecibido.estado);
    server.send(200, "text/plain", "Datos actualizados exitosamente");
  } else {
    server.send(400, "text/plain", "Faltan parámetros");
  }
}

void handleControl() {
  String pin = server.arg("pin");
  String state = server.arg("state");

  int pinNumber = pin.toInt();
  int pinState = (state == "ON") ? HIGH : LOW;

  // Activa o desactiva el pin correspondiente
  digitalWrite(pinToDpin(pinNumber), pinState);
  Serial.print("pinNumber ");
  Serial.println(pinNumber);
  Serial.print("pinState ");
  Serial.println(pinState);
  // Responder al cliente
  server.send(200, "text/plain", "Pin " + pin + " set to " + state);
}

// Ruta DELETE para eliminar o desactivar una salida
void handleDelete() {
  if (server.hasArg("nombre") && server.hasArg("pin")) {
    // Salida datoRecibido;
    // server.arg("nombre").toCharArray(datoRecibido.nombre, sizeof(datoRecibido.nombre));
    // datoRecibido.gpio = server.arg("pin").toInt();
    
    // Desactivar el pin en la placa
    digitalWrite(pinToDpin(server.arg("pin").toInt()), LOW);  // Apagar el pin
    
    // Opcional: Eliminar la salida de la EEPROM (dependiendo de tu lógica de almacenamiento)
    //borrarEERPOM(direccionesEEPROM[datoRecibido.gpio - 1]);
    borrarEERPOM(direccionesEEPROM[server.arg("pin").toInt() - 1]);

    Serial.println("Pin desactivado: " + String(server.arg("pin").toInt()));
    server.send(200, "text/plain", "Pin desactivado correctamente");
  } else {
    server.send(400, "text/plain", "Faltan parámetros");
  }
}
// Ruta GET para leer datos del sensor
void handleGet(){
  // Leer los valores de temperatura y humedad
  float temperatura = dht.getTemperature();   // Temperatura en grados Celsius
  float humedad = dht.getHumidity();          // Humedad relativa

  // Verificar si las lecturas son válidas
  if (isnan(temperatura) || isnan(humedad)) {
    server.send(500, "text/plain", "Error al leer los datos del sensor");
  } else {
    // Crear una respuesta en formato JSON
    String jsonResponse = "{";
    jsonResponse += "\"temperatura\": " + String(temperatura) + ",";
    jsonResponse += "\"humedad\": " + String(humedad);
    jsonResponse += "}";

    // Enviar la respuesta con los datos
    server.send(200, "application/json", jsonResponse);
  }
}

void handlePines() {
  String response = "{ \"pines\": [";
  // Crear el JSON con los nombres de los pines y sus estados de todas las salidas en la memoria EEPROM
  for (int i = 0; i < numPines; i++){
    Salida b1=leerSalidaDeEEPROM(direccionesEEPROM[i]);    
    if (b1.nombre[0]!=255){                                     // Aseguramos que sea una salida valida
      response += "{";
      response += "\"pin\": \"" + String(b1.gpio) + "\", ";
      response += "\"nombre\": \"" + String(b1.nombre) + "\", ";
      response += "\"estado\": " + String(b1.estado);
      response += "}";
    }
    else{
      response += "{";
      response += "\"pin\": \"" + String(i+1) + "\", ";
      response += "\"nombre\": \"" + String("Sin registro") + "\", ";
      response += "\"estado\": " + String("0");
      response += "}";
    }
    if (i < numPines-1) response += ", ";  // Para no agregar coma al final
  }
  response += "] }";
  // Enviar la respuesta al cliente
  server.send(200, "application/json", response);
}
// ================= Fin Manejos Solicitudes al servidor http

// ================= WEB socket =================
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
// ================= FIN WEB socket =================

// ================= Otras Funciones =================
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
// ================= FIN Otras Funciones =================
void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);

  // Configurar los pines como salidas
  for (int i = 0; i < numPines; i++) {
    pinMode(pines[i], OUTPUT);
  }

  // Configura el AP con la IP, puerta de enlace y máscara de subred deseadas
  WiFi.softAPConfig(local_ip, gateway, subnet);

  // Inicia el ESP8266 como punto de acceso
  WiFi.softAP(ssid_ap, password_ap);
  Serial.println("Configurando el ESP8266 en modo AP...");
  Serial.print("Dirección IP del AP: ");
  Serial.println(WiFi.softAPIP());
  
  // Enlazamos las rutas que manejarán las peticiones
  server.on("/", HTTP_GET, handleRoot);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/control", HTTP_GET, handleControl);
  server.on("/update", HTTP_POST, handlePost);      // Ruta POST
  server.on("/delete", HTTP_DELETE, handleDelete);  // Ruta DELETE
  server.on("/get_sensor", HTTP_GET, handleGet);      // Ruta GET
  server.on("/get_pines", HTTP_GET, handlePines);      // Ruta GET

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
    if (b1.nombre[0]!=255){                                     // Aseguramos que sea una salida valida
      digitalWrite(pinToDpin(b1.gpio),b1.estado);               // Escribimos el estado de la salida
      Serial.println("Salida guardada en direccion: ");
      Serial.println(direccionesEEPROM[i]);
      Serial.println(b1.nombre);
      Serial.print("Pin de la tarjeta: ");
      Serial.println(b1.gpio);
      Serial.print("Estado: ");
      Serial.println(b1.estado);
    }
    else{
      digitalWrite(pines[i],false);               // Escribimos el estado de la salida
    }
  }
}

void loop() {
  server.handleClient();  // Maneja las peticiones HTTP
  webSocket.loop();  // Mantiene la comunicación WebSocket

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
    // Serial.print(temperature);
    // Serial.println(" Se leyo en el tiempo ");  // Enviamos el valor leído al monitor serial
  }
  delay(50);
}