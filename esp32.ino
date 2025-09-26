#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASS";

AsyncWebServer server(80);

#define DAC_PIN 25            // ESP32 DAC channel 1 (GPIO25)
#define AUDIO_PATH "/audio.wav"

File audioFile;
volatile bool playing = false;
volatile uint32_t sampleRate = 16000;
volatile uint32_t dataPos = 0;
volatile uint32_t dataLen = 0;
volatile uint32_t playFileOffset = 0;
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// simple buffer for streaming
#define BUF_SAMPLES 512
uint8_t buf[BUF_SAMPLES];

void IRAM_ATTR onTimer() {
  // This ISR runs at sampleRate frequency. We'll output one sample per call.
  portENTER_CRITICAL_ISR(&timerMux);
  if (!playing) {
    portEXIT_CRITICAL_ISR(&timerMux);
    return;
  }
  if (playFileOffset >= dataLen) {
    // stop
    playing = false;
    dacWrite(DAC_PIN, 128); // mid
    portEXIT_CRITICAL_ISR(&timerMux);
    return;
  }
  // read one sample (assume 16-bit PCM little endian mono)
  uint16_t s = 0;
  audioFile.seek(dataPos + playFileOffset);
  int bytes = audioFile.read((uint8_t*)&s, 2);
  if (bytes < 2) {
    playing = false;
    dacWrite(DAC_PIN, 128);
    portEXIT_CRITICAL_ISR(&timerMux);
    return;
  }
  playFileOffset += 2;

  // convert 16-bit signed to 8-bit unsigned for DAC (0..255)
  int16_t sample16 = (int16_t)s; // wav PCM is signed 16-bit
  uint8_t out8 = (uint8_t)((sample16 + 32768) >> 8); // downscale
  dacWrite(DAC_PIN, out8);
  portEXIT_CRITICAL_ISR(&timerMux);
}

void setup() {
  Serial.begin(115200);
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // serve index.html from SPIFFS
  server.serveStatic("/", SPIFFS, "/index.html");

  // upload handler
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "Uploaded");
  }, handleUpload);

  server.begin();

  // prepare timer but start only when playing
  timer = timerBegin(0, 80, true); // 80 prescaler -> 1us tick on 80MHz APB
  timerAttachInterrupt(timer, &onTimer, true);
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  // called repeatedly while upload in progress
  if (index == 0) {
    // first chunk: create/overwrite
    File f = SPIFFS.open(AUDIO_PATH, FILE_WRITE);
    if (!f) {
      Serial.println("Failed open file for write");
      return;
    }
    f.close();
  }
  File f = SPIFFS.open(AUDIO_PATH, FILE_APPEND);
  if (f) {
    f.write(data, len);
    f.close();
  }
  if (final) {
    Serial.println("Upload complete");
    // parse WAV header to set sampleRate & dataPos/dataLen
    audioFile = SPIFFS.open(AUDIO_PATH, FILE_READ);
    if (!audioFile) {
      Serial.println("cannot open saved audio");
      return;
    }
    if (!parseWavHeader(audioFile)) {
      Serial.println("invalid wav");
      audioFile.close();
      return;
    }
    // start playback
    startPlayback();
  }
}

bool parseWavHeader(File &f) {
  // minimal WAV header parsing for PCM 16-bit mono
  f.seek(0);
  char riff[4];
  f.read((uint8_t*)riff, 4);
  if (strncmp(riff, "RIFF", 4) != 0) return false;
  f.seek(22); // numChannels
  uint16_t channels = readLE16(f);
  f.seek(24); // sampleRate
  uint32_t sr = readLE32(f);
  f.seek(34);
  uint16_t bits = readLE16(f);

  // find "data" subchunk
  f.seek(12);
  bool found = false;
  while (f.position() < f.size()) {
    char id[4];
    f.read((uint8_t*)id, 4);
    uint32_t chunkSize = readLE32(f);
    if (strncmp(id, "data", 4) == 0) {
      dataPos = f.position();
      dataLen = chunkSize;
      found = true;
      break;
    } else {
      f.seek(f.position() + chunkSize);
    }
  }
  if (!found) return false;
  Serial.printf("WAV parsed: sr=%u bits=%u ch=%u datapos=%u datalen=%u\n", sr, bits, channels, dataPos, dataLen);

  sampleRate = sr;
  // only support mono 16-bit for now
  if (channels != 1) Serial.println("Warning: WAV not mono; will play only first channel (if present).");
  return true;
}

uint16_t readLE16(File &f) {
  uint8_t b[2];
  f.read(b,2);
  return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

uint32_t readLE32(File &f) {
  uint8_t b[4];
  f.read(b,4);
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

void startPlayback() {
  if (playing) return;
  audioFile.seek(0); // keep file open globally
  playFileOffset = 0;
  playing = true;
  // configure timer to fire at sampleRate microseconds
  // timer alarm counts in microseconds because prescaler set to 80 (1us tick)
  uint64_t alarm_us = 1000000 / sampleRate;
  timerAlarmWrite(timer, alarm_us, true);
  timerAlarmEnable(timer);
  Serial.println("Playback started");
}
