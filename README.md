# Smart Token-Based Power Distribution System  
This project is a smart electricity distribution system using an ESP8266 (NodeMCU), designed for managing two independent clients. Each client can generate a token via a web interface, and power is distributed based on token validation.  
## Project Overview  
-Built using ESP8266 (NodeMCU)  
-Web-based token generation via local hotspot  
-Secure client validation with pre-registered phone numbers  
-Each client has an independent I2C LCD for status display  
-Power control via relays  
-Notification via buzzers  
-Power status indicators via LEDs  

---

## Features

- Two pre-registered clients can generate power tokens.
- ESP8266 verifies the client and generates tokens on-device.
- LCDs display dynamic information for each client:
  - Phone (masked for privacy)
  - Token units remaining
  - Status (active/expired)
- Buzzer alerts when token expires or power is cut.
- External LEDs indicate system power status.
- Built-in LED is used for internal system status.

---

## Hardware Connections

| Component         | GPIO Pin (NodeMCU) | Description                 |
|------------------|--------------------|-----------------------------|
| **Relay 1**      | D1 (GPIO5)         | Controls Client 1 power     |
| **Relay 2**      | D2 (GPIO4)         | Controls Client 2 power     |
| **LCD 1 (I2C)**  | SDA: D3 (GPIO0), SCL: D4 (GPIO2) | For Client 1 |
| **LCD 2 (I2C)**  | SDA: D5 (GPIO14), SCL: D6 (GPIO12) | For Client 2 |
| **Buzzer 1**     | D7 (GPIO13)        | Client 1 notification       |
| **Buzzer 2**     | D8 (GPIO15)        | Client 2 notification       |
| **LED 1**        | GPIO16             | Client 1 power status       |
| **LED 2**        | GPIO10             | Client 2 power status       |

> Note: All GPIOs are configurable in code if you need to change them.

---

## How It Works

1. ESP8266 serves a Wi-Fi Access Point (AP).
2. User connects and opens `http://192.168.4.1/generate.html`.
3. User enters their **phone number** and **amount**.
4. ESP validates the number and generates a **unique token**.
5. Token is mapped to units (kWh/time).
6. If valid, relay is activated and LCD shows user details.
7. When token expires, power is cut, buzzer is triggered, and LCD updates.

---

## Pre-Registered Clients

You must define allowed clients in the firmware (`main.ino`):

```cpp
Client clients[] = {
  {"0711111111", 0.0, false, CLIENT1_RELAY},
  {"0722222222", 0.0, false, CLIENT2_RELAY}
};











