import requests

# Dirección IP del ESP8266
url = "http://192.168.16.236/set_pin"

# Datos que quieres enviar en la solicitud POST
data = {
    "nombre": "nuevo",
    "pin": "4",
    "estado": "0"
}

# Hacer la solicitud POST
response = requests.post(url, data=data)

# Mostrar la respuesta
print(response.text)
