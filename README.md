# HUMAN ECG

Research-grade ECG wearable architecture.

## Structure

- firmware/ → ESP32 DSP pipeline
- backend/ → WebSocket gateway + HRV + AI
- webapp/ → Dashboard

## Run backend

cd backend
npm install
npm start

## ESP32

Flash firmware from firmware/ directory.

## Web

Open webapp/index.html
