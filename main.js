// Konfigurasi MQTT (ganti broker jika perlu)
const mqttBroker = 'ws://broker.hivemq.com:8000/mqtt';
const clientId = 'WebRecorder_' + Math.random().toString(16).substr(2, 8);
const topicStart = '/record/start';
const topicStop = '/record/stop';

let client;
let isConnected = false;

const micIcon = document.getElementById('micIcon');
const statusDiv = document.getElementById('status');

// Event listeners untuk desktop/mobile
micIcon.addEventListener('mousedown', startRecording);
document.addEventListener('mouseup', stopRecording);
micIcon.addEventListener('touchstart', (e) => {
  e.preventDefault();
  startRecording();
});
document.addEventListener('touchend', (e) => {
  e.preventDefault();
  stopRecording();
});

function connectMQTT() {
  client = mqtt.connect(mqttBroker, {
    clientId: clientId,
    username: '', // Kosong untuk HiveMQ publik
    password: ''
  });

  client.on('connect', () => {
    isConnected = true;
    statusDiv.textContent = 'Status: Terhubung ke MQTT';
    statusDiv.classList.add('connected');
    console.log('MQTT terhubung');
  });

  client.on('error', (err) => {
    console.error('MQTT error:', err);
    statusDiv.textContent = 'Status: Error koneksi';
  });

  client.on('close', () => {
    isConnected = false;
    statusDiv.textContent = 'Status: Koneksi tertutup';
  });
}

function startRecording() {
  if (!isConnected) {
    console.error('MQTT belum terhubung');
    return;
  }
  micIcon.style.color = 'darkred'; // Efek visual saat ditekan
  client.publish(topicStart, 'start');
  console.log('Mengirim: start recording');
}

function stopRecording() {
  if (!isConnected) {
    console.error('MQTT belum terhubung');
    return;
  }
  micIcon.style.color = 'red'; // Kembali ke merah
  client.publish(topicStop, 'stop');
  console.log('Mengirim: stop recording & play');
}

// Mulai koneksi saat halaman load
window.addEventListener('load', connectMQTT);
