// ─── IoT Dashboard ─────────────────────────────────────────────
// Polls the REST API every 5 seconds and renders device cards.

const API_BASE = '/api';

// ─── API Calls ─────────────────────────────────────────────────

async function fetchDevices() {
    const response = await fetch(API_BASE + '/devices');
    if (!response.ok) {
        throw new Error('Failed to fetch devices');
    }
    return response.json();
}

async function fetchFirmwareInfo() {
    const response = await fetch(API_BASE + '/firmware/info');
    if (!response.ok) {
        throw new Error('Failed to fetch firmware info');
    }
    return response.json();
}

async function uploadFirmware() {
    var fileInput = document.getElementById('firmware-file');
    if (!fileInput.files.length) {
        alert('Select a .bin file first');
        return;
    }
    var password = prompt('Enter admin password:');
    if (!password) return;
    var formData = new FormData();
    formData.append('file', fileInput.files[0]);
    try {
        var response = await fetch(API_BASE + '/firmware/upload', {
            method: 'POST',
            headers: { 'X-Admin-Password': password },
            body: formData
        });
        if (response.status === 401) {
            alert('Wrong password');
            return;
        }
        if (!response.ok) {
            throw new Error('Upload failed');
        }
        alert('Firmware uploaded');
        loadFirmwareInfo();
    } catch (error) {
        alert('Failed to upload firmware: ' + error.message);
    }
}

async function triggerOta(deviceId) {
    if (!confirm('Deploy firmware to this device?')) return;
    var password = prompt('Enter admin password:');
    if (!password) return;
    try {
        var response = await fetch(API_BASE + '/firmware/deploy/' + deviceId, {
            method: 'POST',
            headers: { 'X-Admin-Password': password }
        });
        if (response.status === 401) {
            alert('Wrong password');
            return;
        }
        if (!response.ok) {
            throw new Error('Deploy failed');
        }
        alert('OTA command sent');
    } catch (error) {
        alert('Failed to send OTA command: ' + error.message);
    }
}

async function sendLedCommand(deviceId, state) {
    const response = await fetch(API_BASE + '/devices/' + deviceId + '/led', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ state: state })
    });
    if (!response.ok) {
        throw new Error('Failed to send LED command');
    }
}

// ─── Rendering ─────────────────────────────────────────────────

function formatUptime(seconds) {
    if (seconds == null) return '—';
    var h = Math.floor(seconds / 3600);
    var m = Math.floor((seconds % 3600) / 60);
    var s = seconds % 60;
    if (h > 0) return h + 'h ' + m + 'm ' + s + 's';
    if (m > 0) return m + 'm ' + s + 's';
    return s + 's';
}

function formatHeap(bytes) {
    if (bytes == null) return '—';
    return Math.round(bytes / 1024) + ' KB';
}

function rssiClass(rssi) {
    if (rssi == null) return '';
    if (rssi >= -50) return 'rssi-good';
    if (rssi >= -70) return 'rssi-medium';
    return 'rssi-weak';
}

function formatTime(isoString) {
    if (!isoString) return 'never';
    var date = new Date(isoString);
    return date.toLocaleTimeString();
}

function renderDeviceCard(device) {
    var macDisplay = device.macFormatted || device.macAddress;
    var ledOn = device.ledState === 'on';

    var html = ''
        + '<div class="device-card">'
        + '  <div class="card-header">'
        + '    <div>'
        + '      <div class="mac-address">' + macDisplay + '</div>'
        + '      <div class="ip-address">' + (device.ipAddress || '—') + '</div>'
        + '      <div class="ip-address">AP: ' + (device.ssid || '—') + '</div>'
        + '    </div>'
        + '    <div class="last-seen">Last seen: ' + formatTime(device.lastSeen) + '</div>'
        + '  </div>'
        + '  <div class="readings">'
        + '    <div class="reading">'
        + '      <div class="label">Temperature</div>'
        + '      <div class="value">' + (device.temperature != null ? device.temperature + ' &deg;C' : '&mdash;') + '</div>'
        + '    </div>'
        + '    <div class="reading">'
        + '      <div class="label">WiFi RSSI</div>'
        + '      <div class="value ' + rssiClass(device.rssi) + '">' + (device.rssi != null ? device.rssi + ' dBm' : '&mdash;') + '</div>'
        + '    </div>'
        + '    <div class="reading">'
        + '      <div class="label">Free Heap</div>'
        + '      <div class="value">' + formatHeap(device.freeHeap) + '</div>'
        + '    </div>'
        + '    <div class="reading">'
        + '      <div class="label">Uptime</div>'
        + '      <div class="value">' + formatUptime(device.uptime) + '</div>'
        + '    </div>'
        + '    <div class="reading">'
        + '      <div class="label">WiFi Channel</div>'
        + '      <div class="value">' + (device.wifiChannel != null ? device.wifiChannel : '&mdash;') + '</div>'
        + '    </div>'
        + '  </div>'
        + '  <div class="led-controls">'
        + '    <span class="led-label">'
        + '      <span class="led-status ' + (ledOn ? 'on' : 'off') + '"></span>'
        + '      LED: ' + (device.ledState || 'unknown')
        + '    </span>'
        + '    <button class="led-btn on-btn" onclick="toggleLed(' + device.id + ', \'on\')">ON</button>'
        + '    <button class="led-btn off-btn" onclick="toggleLed(' + device.id + ', \'off\')">OFF</button>'
        + '  </div>'
        + '  <div class="ota-controls">'
        + '    <span class="ota-label">'
        + '      FW: ' + (device.firmwareVersion || 'unknown')
        + '      | OTA: ' + (device.otaStatus || 'idle')
        + '    </span>'
        + '    <button class="ota-btn" onclick="triggerOta(' + device.id + ')">Update FW</button>'
        + '  </div>'
        + '</div>';

    return html;
}

function renderDashboard(devices) {
    var grid = document.getElementById('devices-grid');
    var countEl = document.getElementById('device-count');
    var refreshEl = document.getElementById('last-refresh');

    countEl.textContent = devices.length + ' device' + (devices.length !== 1 ? 's' : '');
    refreshEl.textContent = 'Updated: ' + new Date().toLocaleTimeString();

    if (devices.length === 0) {
        grid.innerHTML = '<p class="placeholder">Waiting for devices to connect...</p>';
        return;
    }

    var html = '';
    for (var i = 0; i < devices.length; i++) {
        html += renderDeviceCard(devices[i]);
    }
    grid.innerHTML = html;
}

// ─── Actions ───────────────────────────────────────────────────

async function toggleLed(deviceId, state) {
    try {
        await sendLedCommand(deviceId, state);
    } catch (error) {
        alert('Failed to send LED command: ' + error.message);
    }
}

// ─── Polling Loop ──────────────────────────────────────────────

async function loadDevices() {
    try {
        var devices = await fetchDevices();
        renderDashboard(devices);
    } catch (error) {
        console.error('Error loading devices:', error);
    }
}

async function loadFirmwareInfo() {
    try {
        var info = await fetchFirmwareInfo();
        var el = document.getElementById('firmware-info');
        if (info.available) {
            el.textContent = info.filename + ' (' + info.size + ' bytes, uploaded ' + info.uploadedAt + ')';
        } else {
            el.textContent = 'No firmware uploaded';
        }
    } catch (error) {
        console.error('Error loading firmware info:', error);
    }
}

// Load immediately, then refresh every 5 seconds
loadDevices();
loadFirmwareInfo();
setInterval(loadDevices, 5000);
