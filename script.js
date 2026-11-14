
const BROKER_URL = "wss://broker.emqx.io:8084/mqtt";
const PREFIX = "home/fire";

const TOPICS = {
    gas: `${PREFIX}/gas`,
    temperature: `${PREFIX}/temperature`,
    modeState: `${PREFIX}/mode/state`,
    fanState: `${PREFIX}/fan/state`,
    pumpState: `${PREFIX}/pump/state`,
    states: `${PREFIX}/states`,
    alarm: `${PREFIX}/alarm`,

    modeSet: `${PREFIX}/mode/set`,
    fanSet: `${PREFIX}/fan/set`,
    pumpSet: `${PREFIX}/pump/set`
};

let currentMode = null;
let currentFan = null;
let currentPump = null;
let mqttClient = null;

const $ = (id) => document.getElementById(id);

// ======================================
// MQTT CONNECTION
// ======================================

function connectMQTT() {
    $("connectionText").textContent = "Đang kết nối...";

    mqttClient = mqtt.connect(BROKER_URL, {
        clientId: "FireGuard_Web_" + Math.random().toString(16).slice(2, 10),
        clean: true,
        connectTimeout: 10000,
        reconnectPeriod: 3000
    });

    mqttClient.on("connect", () => {
        console.log("[MQTT] Connected!");

        $("connectionDot").className = "dot online";
        $("connectionText").textContent = "MQTT đã kết nối";

        mqttClient.subscribe([
            TOPICS.gas,
            TOPICS.temperature,
            TOPICS.modeState,
            TOPICS.fanState,
            TOPICS.pumpState,
            TOPICS.states,
            TOPICS.alarm
        ], (error) => {
            if (error) {
                console.error("[MQTT] Subscribe error:", error);
            } else {
                console.log("[MQTT] Subscribed to FireGuard topics");
            }
        });

        updateControlAvailability();
    });

    mqttClient.on("reconnect", () => {
        $("connectionDot").className = "dot offline";
        $("connectionText").textContent = "Đang kết nối lại...";
        updateControlAvailability();
    });

    mqttClient.on("offline", () => {
        $("connectionDot").className = "dot offline";
        $("connectionText").textContent = "Mất kết nối MQTT";
        updateControlAvailability();
    });

    mqttClient.on("error", (error) => {
        console.error("[MQTT] Error:", error);
    });

    mqttClient.on("message", (topic, message) => {
        const value = message.toString().trim();

        console.log("[MQTT RX]", topic, value);

        switch (topic) {
            case TOPICS.gas:
                updateGas(value);
                break;

            case TOPICS.temperature:
                updateTemperature(value);
                break;

            case TOPICS.modeState:
                updateMode(value);
                break;

            case TOPICS.fanState:
                updateFan(value);
                break;

            case TOPICS.pumpState:
                updatePump(value);
                break;

            case TOPICS.states:
                updateAllStates(value);
                break;

            case TOPICS.alarm:
                updateAlarm(value);
                break;
        }
    });
}

// ======================================
// SENSOR DISPLAY
// ======================================

function updateGas(value) {
    const gas = Number(value);

    if (!Number.isFinite(gas)) return;

    $("gasValue").textContent = gas;
    $("gasBar").style.width =
        Math.min(100, Math.max(0, gas / 4095 * 100)) + "%";

    if (gas >= 2000) {
        $("gasStatus").textContent = "Cảnh báo khí / khói cao";
        $("gasStatus").style.color = "#dc2626";
    } else {
        $("gasStatus").textContent = "Mức khí trong ngưỡng bình thường";
        $("gasStatus").style.color = "#16a36a";
    }
}

function updateTemperature(value) {
    const temperature = Number(value);

    if (!Number.isFinite(temperature)) return;

    $("tempValue").textContent = temperature.toFixed(1);
    $("tempBar").style.width =
        Math.min(100, Math.max(0, temperature / 80 * 100)) + "%";

    if (temperature >= 50) {
        $("tempStatus").textContent = "Cảnh báo nhiệt độ cao";
        $("tempStatus").style.color = "#dc2626";
    } else {
        $("tempStatus").textContent = "Nhiệt độ bình thường";
        $("tempStatus").style.color = "#16a36a";
    }
}

// ======================================
// MODE DISPLAY
// ======================================

function updateMode(value) {
    const mode = value.trim().toUpperCase();

    if (mode !== "AUTO" && mode !== "MANUAL") return;

    currentMode = mode;

    $("modeBadge").textContent = mode;
    $("modeDescription").textContent =
        mode === "AUTO"
            ? "Hệ thống tự động điều khiển theo cảm biến."
            : "Người dùng trực tiếp điều khiển thiết bị.";

    $("autoBtn").classList.toggle("active", mode === "AUTO");
    $("manualBtn").classList.toggle("active", mode === "MANUAL");

    updateControlAvailability();
}

// ======================================
// DEVICE DISPLAY
// ======================================

function updateDeviceState(name, value) {
    const isOn = value.trim().toUpperCase() === "ON";

    const state = $(name + "State");
    const indicator = $(name + "Indicator");

    state.textContent = isOn ? "ON" : "OFF";
    state.classList.toggle("is-on", isOn);
    indicator.classList.toggle("is-on", isOn);
}

function updateFan(value) {
    if (!["ON", "OFF"].includes(value.trim().toUpperCase())) return;

    currentFan = value.trim().toUpperCase();
    updateDeviceState("fan", currentFan);
}

function updatePump(value) {
    if (!["ON", "OFF"].includes(value.trim().toUpperCase())) return;

    currentPump = value.trim().toUpperCase();
    updateDeviceState("pump", currentPump);
}

// ======================================
// RECEIVE JSON STATES
// ======================================

function updateAllStates(value) {
    try {
        const data = JSON.parse(value);

        if (data.mode) updateMode(data.mode);
        if (data.fan) updateFan(data.fan);
        if (data.pump) updatePump(data.pump);

    } catch (error) {
        console.error("[MQTT] Invalid states JSON:", error);
    }
}

// ======================================
// ALARM DISPLAY
// ======================================

function updateAlarm(value) {
    const alarm = value.trim().toUpperCase();

    const panel = $("alarmPanel");
    const title = $("alarmTitle");
    const description = $("alarmDescription");
    const symbol = panel.querySelector(".alarm-symbol");
    const status = $("systemStatus");

    panel.className = "panel alarm-panel";
    status.style.color = "#16a36a";

    if (alarm === "NORMAL") {
        title.textContent = "Hệ thống an toàn";
        description.textContent =
            "Không phát hiện cảnh báo khí hoặc nhiệt độ.";
        symbol.textContent = "✓";
        status.querySelector("#alarmText").textContent = "NORMAL";
        status.querySelector(".status-icon").textContent = "●";

    } else if (alarm === "GAS_WARNING") {
        panel.classList.add("warning");
        title.textContent = "Cảnh báo khí / khói";
        description.textContent =
            "MQ-2 phát hiện giá trị khí vượt ngưỡng.";
        symbol.textContent = "!";
        status.style.color = "#b45309";
        status.querySelector("#alarmText").textContent = "GAS WARNING";
        status.querySelector(".status-icon").textContent = "●";

    } else if (alarm === "TEMP_WARNING") {
        panel.classList.add("warning");
        title.textContent = "Cảnh báo nhiệt độ";
        description.textContent =
            "Nhiệt độ vượt ngưỡng cảnh báo.";
        symbol.textContent = "!";
        status.style.color = "#b45309";
        status.querySelector("#alarmText").textContent = "TEMP WARNING";
        status.querySelector(".status-icon").textContent = "●";

    } else if (alarm === "FIRE_WARNING") {
        panel.classList.add("danger");
        title.textContent = "CẢNH BÁO CHÁY!";
        description.textContent =
            "Phát hiện đồng thời khí / khói và nhiệt độ cao.";
        symbol.textContent = "!";
        status.style.color = "#dc2626";
        status.querySelector("#alarmText").textContent = "FIRE WARNING";
        status.querySelector(".status-icon").textContent = "●";

    } else {
        title.textContent = alarm;
        description.textContent = "Trạng thái cảnh báo nhận từ ESP32.";
        symbol.textContent = "?";
        status.querySelector("#alarmText").textContent = alarm;
        status.style.color = "#64748b";
    }
}

// ======================================
// CONTROL AVAILABILITY
// ======================================

function updateControlAvailability() {
    const connected = mqttClient && mqttClient.connected;
    const manual = currentMode === "MANUAL";

    $("autoBtn").disabled = !connected;
    $("manualBtn").disabled = !connected;

    const canControlDevices = connected && manual;

    $("fanOnBtn").disabled = !canControlDevices;
    $("fanOffBtn").disabled = !canControlDevices;
    $("pumpOnBtn").disabled = !canControlDevices;
    $("pumpOffBtn").disabled = !canControlDevices;

    $("controlNotice").textContent = !connected
        ? "Chưa kết nối MQTT."
        : manual
            ? "Chế độ MANUAL: Có thể điều khiển quạt và bơm."
            : currentMode === "AUTO"
                ? "Chế độ AUTO: Thiết bị được điều khiển tự động theo cảm biến."
                : "Đang chờ ESP32 gửi trạng thái chế độ.";
}

// ======================================
// SEND MQTT COMMAND
// ======================================

function sendCommand(topic, value) {
    if (!mqttClient || !mqttClient.connected) {
        alert("Chưa kết nối MQTT!");
        return;
    }

    mqttClient.publish(topic, value, { qos: 0, retain: false });

    console.log("[MQTT TX]", topic, value);
}

$("autoBtn").addEventListener("click", () => {
    sendCommand(TOPICS.modeSet, "AUTO");
});

$("manualBtn").addEventListener("click", () => {
    sendCommand(TOPICS.modeSet, "MANUAL");
});

$("fanOnBtn").addEventListener("click", () => {
    sendCommand(TOPICS.fanSet, "ON");
});

$("fanOffBtn").addEventListener("click", () => {
    sendCommand(TOPICS.fanSet, "OFF");
});

$("pumpOnBtn").addEventListener("click", () => {
    sendCommand(TOPICS.pumpSet, "ON");
});

$("pumpOffBtn").addEventListener("click", () => {
    sendCommand(TOPICS.pumpSet, "OFF");
});

// ======================================
// START
// ======================================

connectMQTT();
