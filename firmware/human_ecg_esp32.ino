#include <WiFi.h>
#include <WebSocketsClient.h>
#include <Adafruit_NeoPixel.h>

/* ==================== REŽIM ==================== */
#define MODE_LOW_POWER 0
#define MODE_COMPLETE 1
int deviceMode = MODE_COMPLETE;

/* ==================== HARDWARE ==================== */
#define ECG_PIN 0
#define LO_PLUS 4
#define LO_MINUS 5
#define POWER_PIN 11
#define RGB_LED_PIN 8
#define SAMPLE_RATE 250
#define REFRACTORY_PERIOD 250

/* ==================== WIFI ==================== */
const char* ssid = "ART3";
const char* password = "M@theo@rte23418";
const char* ws_host = "192.168.1.100";
const uint16_t ws_port = 8080;

WebSocketsClient webSocket;
Adafruit_NeoPixel pixel(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

/* ==================== DSP STAV ==================== */
float hp_prev_input=0, hp_prev_output=0;
float lp_prev_output=0;

float prevSample=0;
float mwiBuffer[30];
int mwiIndex=0;

/* ==================== PAN TOMPKINS ==================== */
float signalPeak=0;
float noisePeak=0;
float threshold=0.02;

unsigned long lastPeakTime=0;
float bpm=0;

/* ==================== RR ==================== */
float rrBuffer[30];
int rrIndex=0;

/* ==================== TIMING ==================== */
unsigned long lastSampleMicros=0;

/* ==================== FILTRY ==================== */

// High-pass (~0.5 Hz)
float highpass(float input){
  float output = 0.995*(hp_prev_output + input - hp_prev_input);
  hp_prev_input = input;
  hp_prev_output = output;
  return output;
}

// Low-pass (~40 Hz)
float lowpass(float input){
  lp_prev_output += 0.1*(input - lp_prev_output);
  return lp_prev_output;
}

/* ==================== HRV ==================== */

float computeRMSSD(){
  if(rrIndex<2) return 0;
  float sum=0;
  for(int i=1;i<rrIndex;i++){
    float d = rrBuffer[i]-rrBuffer[i-1];
    sum += d*d;
  }
  return sqrt(sum/(rrIndex-1));
}

float computeSDNN(){
  if(rrIndex<2) return 0;
  float mean=0;
  for(int i=0;i<rrIndex;i++) mean+=rrBuffer[i];
  mean/=rrIndex;

  float sum=0;
  for(int i=0;i<rrIndex;i++)
    sum+=pow(rrBuffer[i]-mean,2);

  return sqrt(sum/rrIndex);
}

float computePNN50(){
  if(rrIndex<2) return 0;
  int count=0;
  for(int i=1;i<rrIndex;i++)
    if(abs(rrBuffer[i]-rrBuffer[i-1])>50) count++;
  return (float)count/rrIndex*100.0;
}

float computeQuality(float val){
  static float noise=0;
  noise = 0.99*noise + 0.01*abs(val);
  return max(0.0, 100.0 - noise*400);
}

/* ==================== WEBSOCKET EVENT ==================== */

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

  if(type == WStype_TEXT){
    String msg = String((char*)payload);

    if(msg.indexOf("\"cmd\":\"set_mode\"")>=0){

      if(msg.indexOf("\"mode\":\"easy\"")>=0)
        deviceMode = MODE_LOW_POWER;

      if(msg.indexOf("\"mode\":\"complete\"")>=0)
        deviceMode = MODE_COMPLETE;
    }
  }
}

/* ==================== SETUP ==================== */

void setup(){

  Serial.begin(115200);

  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);

  pinMode(LO_PLUS, INPUT);
  pinMode(LO_MINUS, INPUT);

  pixel.begin();
  pixel.setBrightness(20);

  WiFi.begin(ssid,password);
  while(WiFi.status()!=WL_CONNECTED) delay(500);

  webSocket.begin(ws_host, ws_port, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

/* ==================== LOOP ==================== */

void loop(){

  webSocket.loop();

  if(micros() - lastSampleMicros >= 1000000/SAMPLE_RATE){

    lastSampleMicros = micros();

    bool leadsAttached =
      (digitalRead(LO_PLUS)==LOW &&
       digitalRead(LO_MINUS)==LOW);

    if(!leadsAttached){
      if(webSocket.isConnected())
        webSocket.sendTXT("{\"status\":\"leads-off\"}");
      return;
    }

    int raw = analogRead(ECG_PIN);
    float voltage = (float)raw / 4095.0;

    float filtered = highpass(voltage);
    filtered = lowpass(filtered);

    float derivative = filtered - prevSample;
    prevSample = filtered;

    float squared = derivative * derivative;

    // Moving Window Integration
    mwiBuffer[mwiIndex++] = squared;
    if(mwiIndex>=30) mwiIndex=0;

    float integrated=0;
    for(int i=0;i<30;i++)
      integrated += mwiBuffer[i];
    integrated/=30;

    unsigned long now = millis();
    bool beat=false;

    if(integrated > threshold &&
       (now - lastPeakTime) > REFRACTORY_PERIOD){

      float interval = now - lastPeakTime;
      lastPeakTime = now;

      if(interval>300 && interval<2000){

        bpm = 60000.0 / interval;
        beat=true;

        if(rrIndex<30)
          rrBuffer[rrIndex++] = interval;
        else{
          for(int i=1;i<30;i++)
            rrBuffer[i-1]=rrBuffer[i];
          rrBuffer[29]=interval;
        }

        signalPeak = 0.125*integrated + 0.875*signalPeak;
      }
    } else {
      noisePeak = 0.125*integrated + 0.875*noisePeak;
    }

    threshold = noisePeak + 0.25*(signalPeak - noisePeak);

    float rmssd = computeRMSSD();
    float sdnn  = computeSDNN();
    float pnn50 = computePNN50();
    float quality = computeQuality(filtered);

    if(deviceMode == MODE_LOW_POWER){

      if(beat && webSocket.isConnected()){
        String json="{";
        json+="\"bpm\":"+String(bpm,1)+",";
        json+="\"quality\":"+String(quality,0);
        json+="}";
        webSocket.sendTXT(json);
      }
    }

    if(deviceMode == MODE_COMPLETE){

      if(webSocket.isConnected()){
        String json="{";
        json+="\"ecg\":"+String(filtered,4)+",";
        json+="\"bpm\":"+String(bpm,1)+",";
        json+="\"rr\":"+String(rrIndex>0?rrBuffer[rrIndex-1]:0)+",";
        json+="\"rmssd\":"+String(rmssd,1)+",";
        json+="\"sdnn\":"+String(sdnn,1)+",";
        json+="\"pnn50\":"+String(pnn50,1)+",";
        json+="\"quality\":"+String(quality,0);
        json+="}";
        webSocket.sendTXT(json);
      }
    }

    if(beat){
      pixel.setPixelColor(0, pixel.Color(0,255,0));
      pixel.show();
    }
  }
}
