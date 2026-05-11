# IoT Multifunctional Digital Clock 🕒🌡️

A comprehensive hardware and software project featuring an IoT-enabled multifunctional digital clock. Built with Arduino Mega 2560 and a Python/Flask backend. 

This project was created as a high school graduation project (Maturitní práce) in Electrical Engineering.

## Features
* **Real-time Clock & Date**: Synchronized and fetched from a local backend.
* **Weather Integration**: Displays current temperature fetched via Open-Meteo API.
* **Alarm System**: Supports weekly scheduled alarms and one-time quick alarms.
* **Smart Notifications**: Can receive and display custom text notifications (e.g., triggered by iOS Shortcuts/Instagram).
* **Power Management**: Auto-sleep function turns off the LED matrices after 10 minutes of server unreachability.

## Hardware Stack 🛠️
* **Microcontroller**: Arduino Mega 2560
* **Network**: Ethernet Shield W5100 R3
* **Display**: 8x MAX7219 8x8 LED Matrices (configured as two 32x8 displays)
* **Audio**: LM386 Amplifier + Speaker for alarms
* **Housing**: Custom 3D printed/designed chassis (SolidWorks)

## Software Stack 💻
* **C++ (Arduino)**: Custom firmware handling SPI communication with displays, Ethernet networking, and state management.
* **Python (Flask)**: A lightweight REST API server handling data aggregation (time, weather APIs) and serving payloads to the Arduino.

## Project Structure
* `/arduino` - Contains the main `.ino` sketch and various hardware diagnostic tests (SPI, positioning, raw bit-banging).
* `/server` - The Python Flask backend application and `requirements.txt`.
* `/docs` - Complete project documentation and thesis (in Czech).

## Quick Start (Server)
1. Clone the repository and navigate to the server directory.
2. Install dependencies:
   ```bash
   pip install -r requirements.txt
Run the Flask server:

Bash
python3 app.py
Note: Ensure the server runs on a static IP address accessible by the Arduino.

Quick Start (Arduino)
Open arduino/arduino_hodiny/arduino_hodiny.ino in the Arduino IDE.

Install required libraries: Ethernet, MD_Parola, MD_MAX72xx.

Update the SERVER_IP constant in the code to match your Flask server's IP.

Upload to the Arduino Mega 2560.

License
Open-source. Feel free to use and modify for your own IoT hardware projects.
