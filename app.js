const MASTER_SERVICE = "0000bbbb-0000-1000-8000-00805f9b34fb";
const MASTER_CHAR    = "0000bb01-0000-1000-8000-00805f9b34fb";
const TX_SERVICE     = "0000aaaa-0000-1000-8000-00805f9b34fb";
const TX_CHAR        = "0000aa01-0000-1000-8000-00805f9b34fb";
const TX_PREFIX      = "TX-";

const masterMacInput = document.getElementById("masterMac");
const logDiv = document.getElementById("log");

function log(msg) {
    logDiv.textContent += msg + "\n";
    logDiv.scrollTop = logDiv.scrollHeight;
}

async function getGPS() {
    return new Promise((resolve) => {
        navigator.geolocation.getCurrentPosition(
            pos => resolve({lat: pos.coords.latitude, lon: pos.coords.longitude}),
            ()  => resolve(null),
            { enableHighAccuracy:true, timeout:8000 }
        );
    });
}

document.getElementById("btnMaster").onclick = async () => {
    try {
        log("🔍 Procurando MASTER...");
        const device = await navigator.bluetooth.requestDevice({
            filters: [{ services:[MASTER_SERVICE] }]
        });
        log("Conectando...");
        const server = await device.gatt.connect();
        const service = await server.getPrimaryService(MASTER_SERVICE);
        const charac  = await service.getCharacteristic(MASTER_CHAR);
        const value = await charac.readValue();
        const mac = new TextDecoder().decode(value);
        masterMacInput.value = mac;
        log("✔ MASTER conectado: " + mac);
    } catch (e) {
        log("Erro ao conectar MASTER: " + e);
    }
};

document.getElementById("btnTx").onclick = async () => {
    try {
        const masterMac = masterMacInput.value.trim();
        if (!masterMac) return alert("Conecte ao MASTER primeiro.");
        log("🔍 Procurando transmissor...");
        const device = await navigator.bluetooth.requestDevice({
            filters: [{ namePrefix: TX_PREFIX }],
            optionalServices:[TX_SERVICE]
        });
        log("Conectando TX...");
        const server = await device.gatt.connect();
        const service = await server.getPrimaryService(TX_SERVICE);
        const charac  = await service.getCharacteristic(TX_CHAR);
        const name = prompt("Nome do transmissor:", device.name);
        const gps = await getGPS();
        const payload = {
            masterMac: masterMac,
            name: name,
            lat: gps ? gps.lat : null,
            lon: gps ? gps.lon : null
        };
        log("📤 Enviando: " + JSON.stringify(payload));
        await charac.writeValue(new TextEncoder().encode(JSON.stringify(payload)));
        log("✔ Transmissor configurado!");
    } catch (e) {
        log("Erro TX: " + e);
    }
};
