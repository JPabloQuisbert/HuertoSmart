import requests

# Dirección IP del ESP8266
url = "http://192.168.16.236/delete"

# Datos que quieres enviar en la solicitud DELETE (si es necesario)
data = {
    "nombre": "255",
    "pin": "2",
    "estado": "1"
}

# Hacer la solicitud DELETE
response = requests.delete(url, data=data)

# Mostrar la respuesta
print(response.text)