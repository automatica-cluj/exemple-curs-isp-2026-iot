// ─── IoT Dashboard — Deep Sleep ─────────────────────────────────
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

async function sendIntervalCommand(deviceId, seconds) {
    const response = await fetch(API_BASE + '/devices/' + deviceId + '/interval', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ seconds: seconds })
    });
    if (!response.ok) {
        throw new Error('Failed to set interval');
    }
}

// ─── Rendering ─────────────────────────────────────────────────

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

function formatInterval(seconds) {
    if (seconds == null) return '—';
    if (seconds >= 60) {
        var m = Math.floor(seconds / 60);
        var s = seconds % 60;
        return s > 0 ? m + 'm ' + s + 's' : m + 'm';
    }
    return seconds + 's';
}

function isOnline(lastSeen, sleepInterval) {
    if (!lastSeen) return false;
    var lastSeenDate = new Date(lastSeen);
    var now = new Date();
    // Device is "online" if last seen within 2x its sleep interval (+ 30s buffer)
    var interval = (sleepInterval || 300);
    var timeoutMs = (interval * 2 + 30) * 1000;
    return (now - lastSeenDate) < timeoutMs;
}

function renderDeviceCard(device) {
    var macDisplay = device.macFormatted || device.macAddress;
    var online = isOnline(device.lastSeen, device.sleepInterval);
    var currentInterval = device.sleepInterval || 300;

    var html = ''
        + '<div class="device-card">'
        + '  <div class="card-header">'
        + '    <div>'
        + '      <div class="mac-address">'
        + '        <span class="online-dot ' + (online ? 'online' : 'offline') + '"></span>'
        + '        ' + macDisplay
        + '      </div>'
        + '      <div class="ip-address">' + (device.ipAddress || '—') + '</div>'
        + '      <div class="ip-address">AP: ' + (device.ssid || '—') + '</div>'
        + '    </div>'
        + '    <div class="header-right">'
        + '      <div class="last-seen">Last seen: ' + formatTime(device.lastSeen) + '</div>'
        + '      <div class="boot-count">Boots: ' + (device.bootCount != null ? device.bootCount : '—') + '</div>'
        + '    </div>'
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
        + '      <div class="label">WiFi Channel</div>'
        + '      <div class="value">' + (device.wifiChannel != null ? device.wifiChannel : '&mdash;') + '</div>'
        + '    </div>'
        + '  </div>'
        + '  <div class="interval-controls">'
        + '    <span class="interval-label">'
        + '      <span class="sleep-icon">&#x1F4A4;</span>'
        + '      Sleep interval: ' + formatInterval(currentInterval)
        + '      | FW: ' + (device.firmwareVersion || 'unknown')
        + '    </span>'
        + '    <div class="interval-input">'
        + '      <select id="interval-select-' + device.id + '">'
        + '        <option value="10"' + (currentInterval === 10 ? ' selected' : '') + '>10s</option>'
        + '        <option value="30"' + (currentInterval === 30 ? ' selected' : '') + '>30s</option>'
        + '        <option value="60"' + (currentInterval === 60 ? ' selected' : '') + '>1m</option>'
        + '        <option value="300"' + (currentInterval === 300 ? ' selected' : '') + '>5m</option>'
        + '        <option value="600"' + (currentInterval === 600 ? ' selected' : '') + '>10m</option>'
        + '        <option value="1800"' + (currentInterval === 1800 ? ' selected' : '') + '>30m</option>'
        + '        <option value="3600"' + (currentInterval === 3600 ? ' selected' : '') + '>1h</option>'
        + '      </select>'
        + '      <button class="interval-btn" onclick="setInterval_(' + device.id + ')">Set</button>'
        + '    </div>'
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
        grid.innerHTML = '<p class="placeholder">Waiting for devices to report...</p>';
        return;
    }

    var html = '';
    for (var i = 0; i < devices.length; i++) {
        html += renderDeviceCard(devices[i]);
    }
    grid.innerHTML = html;
}

// ─── Actions ───────────────────────────────────────────────────

async function setInterval_(deviceId) {
    var select = document.getElementById('interval-select-' + deviceId);
    var seconds = parseInt(select.value);
    try {
        await sendIntervalCommand(deviceId, seconds);
    } catch (error) {
        alert('Failed to set interval: ' + error.message);
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

// Load immediately, then refresh every 5 seconds
loadDevices();
setInterval(loadDevices, 5000);
