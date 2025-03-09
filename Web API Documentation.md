# ESP32 Relay Control System - Web API Documentation

This document explains how to interact with the ESP32 Relay Control System through its web interface and API endpoints.

## Overview

The ESP32 Relay Control System provides a web interface and REST API to remotely monitor and control relay states and schedules. You can access these features from any web browser or mobile app that can make HTTP requests.

## Connection

1. Connect to the same WiFi network as the ESP32 device
2. Access the device using its IP address in your browser: http://[ESP32-IP-ADDRESS]

The IP address is displayed on the OLED screen during startup.

## Web Interface

The web interface provides a user-friendly way to:
- View the current time
- See all relay states and schedules
- Modify relay ON/OFF times
- Enable/disable scheduled operation for each relay
- Toggle relay states manually
- Save settings to EEPROM

## REST API Endpoints

You can also interact with the system programmatically using the following REST API endpoints:

### 1. Get All Relay Information

```
GET /api/relays
```

**Response:** JSON array with information about all relays
```json
[
  {
    "relay": 1,
    "hourOn": 23,
    "minuteOn": 4,
    "hourOff": 19,
    "minuteOff": 0,
    "useSchedule": 1,
    "currentState": 0
  },
  {
    "relay": 2,
    "hourOn": 23,
    "minuteOn": 3,
    "hourOff": 19,
    "minuteOff": 0,
    "useSchedule": 0,
    "currentState": 0
  },
  ...
]
```

### 2. Update Relay Settings

```
GET /api/update?relay=[RELAY_ID]&field=[FIELD_ID]&value=[NEW_VALUE]
```

**Parameters:**
- `relay`: Relay ID (1-8)
- `field`: Field to update
  - 1: Hour ON
  - 2: Minute ON
  - 3: Hour OFF
  - 4: Minute OFF
  - 5: Use Schedule (1=yes, 0=no)
  - 6: Current State (1=on, 0=off)
- `value`: New value for the field

**Response:** Text confirmation message

**Example:** Turn on relay 3
```
GET /api/update?relay=3&field=6&value=1
```

### 3. Save to EEPROM

```
GET /api/save
```

**Response:** Text confirmation message

## Mobile App Integration

You can integrate with mobile apps using the REST API. Here's how to create a simple app with frameworks like React Native or Flutter:

1. Obtain the ESP32's IP address
2. Use the `/api/relays` endpoint to fetch current states
3. Use the `/api/update` endpoint to modify settings
4. Use the `/api/save` endpoint to save changes to EEPROM

### Example Code for HTTP Requests in a Mobile App

Here's how you might fetch relay information in JavaScript for a React Native app:

```javascript
fetch('http://[ESP32-IP-ADDRESS]/api/relays')
  .then(response => response.json())
  .then(data => {
    // Process relay data
    console.log(data);
  })
  .catch(error => {
    console.error('Error fetching relay data:', error);
  });
```

To update a relay setting:

```javascript
function updateRelay(relayId, fieldId, value) {
  fetch(`http://[ESP32-IP-ADDRESS]/api/update?relay=${relayId}&field=${fieldId}&value=${value}`)
    .then(response => response.text())
    .then(message => {
      console.log('Update result:', message);
    })
    .catch(error => {
      console.error('Error updating relay:', error);
    });
}

// Example: Turn on relay 2
updateRelay(2, 6, 1);

// Example: Change ON time for relay 1 to 8:30
updateRelay(1, 1, 8); // Hour ON
updateRelay(1, 2, 30); // Minute ON
```

## Troubleshooting

If you cannot connect to the web interface:

1. Verify the ESP32 is powered on and the OLED display shows an IP address
2. Ensure you're connected to the same WiFi network
3. Try rebooting the ESP32
4. Check if there are multiple access points with the same name and try connecting to each one

If the time is incorrect:

1. Make sure the ESP32 has internet access for NTP time synchronization
2. The device automatically tries to sync time every hour
3. Restart the device to force a time sync
