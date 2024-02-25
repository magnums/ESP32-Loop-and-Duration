# ESP32-Relay Control Loop-and-Duration

This repository contains firmware for controlling relays using an ESP32 microcontroller. The firmware allows users to set up intervals and durations for the relays, manage working hours, and monitor relay statuses through a web interface. Additionally, it includes basic authentication for security.

Features
Control relays based on specified intervals and durations.
Set working hours to automate relay operation.
Secure web interface with basic authentication.
Real-time monitoring of relay statuses.
Requirements
ESP32 microcontroller board.
Arduino IDE or PlatformIO for development.
Libraries: WiFiManager, AsyncTCP, ESPAsyncWebServer, Preferences, NTPClient.
Installation
Clone the repository: git clone https://github.com/yourusername/esp32-relay-control.git.
Open the project in Arduino IDE or PlatformIO.
Install required libraries listed in the requirements section.
Configure your Wi-Fi credentials in the setup() function.
Upload the firmware to your ESP32 board.
Usage
Connect the relays to the appropriate GPIO pins on the ESP32 board.
Power on the ESP32 board.
Access the web interface by navigating to the ESP32's IP address in a web browser.
Log in using the default username and password.
Configure relay settings, working hours, and monitor relay statuses through the web interface.
Configuration
Wi-Fi: Modify Wi-Fi credentials in the setup() function to connect the ESP32 to your local network.
Relay Pins: Adjust relayPin1 and relayPin2 constants to match the GPIO pins connected to your relays.
Working Hours: Set the start and stop times for working hours in the web interface.
Authentication: Change the default username and password in the code for enhanced security.
Contributing
Contributions to this project are welcome. Feel free to open issues for bug reports, feature requests, or submit pull requests with improvements.

Acknowledgements
Special thanks to the developers of libraries used in this project: WiFiManager, AsyncTCP, ESPAsyncWebServer, Preferences, NTPClient.
