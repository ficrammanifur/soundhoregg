// Konfigurasi MQTT - Ganti ke EMQX WSS (secure untuk HTTPS)
const mqttBroker = 'wss://broker.emqx.io:8084/mqtt';
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
    username: '', // Kosong untuk EMQX publik
    password: '',
    // Opsi tambahan untuk WSS
    protocolVersion: 4,
    clean: true
  });

  client.on('connect', () => {
    isConnected = true;
    statusDiv.textContent = 'Status: Terhubung ke MQTT';
    statusDiv.classList.add('connected');
    console.log('MQTT terhubung ke EMQX');
  });

  client.on('error', (err) => {
    console.error('MQTT error:', err);
    statusDiv.textContent = 'Status: Error koneksi - Cek console';
  });

  client.on('close', () => {
    isConnected = false;
    statusDiv.textContent = 'Status: Koneksi tertutup - Mencoba reconnect...';
    setTimeout(connectMQTT, 5000); // Auto-reconnect
  });

  client.on('message', (topic, message) => {
    console.log('Pesan diterima:', topic.toString(), message.toString());
  });
}

function startRecording() {
  if (!isConnected) {
    console.error('MQTT belum terhubung');
    statusDiv.textContent = 'Status: MQTT belum siap - Tunggu...';
    return;
  }
  micIcon.style.color = 'darkred'; // Efek visual
  client.publish(topicStart, 'start');
  console.log('Mengirim: start recording');
}

function stopRecording() {
  if (!isConnected) {
    console.error('MQTT belum terhubung');
    return;
  }
  micIcon.style.color = 'red'; // Kembali normal
  client.publish(topicStop, 'stop');
  console.log('Mengirim: stop recording & play');
}

// Mulai koneksi saat halaman load, dengan retry
function init() {
  connectMQTT();
}
window.addEventListener('load', init);
