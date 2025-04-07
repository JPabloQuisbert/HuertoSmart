import requests

# Dirección IP del ESP8266
url = "http://192.168.16.236/update"

# Datos que quieres enviar en la solicitud POST
data = {
    "nombre": "255",
    "pin": "2",
    "estado": "1"
}

# Hacer la solicitud POST
response = requests.post(url, data=data)

# Mostrar la respuesta
print(response.text)
