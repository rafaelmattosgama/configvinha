const MASTER_SERVICE = "0000bbbb-0000-1000-8000-00805f9b34fb";
const MASTER_CHAR = "0000bb01-0000-1000-8000-00805f9b34fb";
const TX_SERVICE = "0000aaaa-0000-1000-8000-00805f9b34fb";
const TX_CHAR = "0000aa01-0000-1000-8000-00805f9b34fb";
const TX_PREFIX = "TX-";

const masterMacInput = document.getElementById("masterMac");
const logDiv = document.getElementById("log");
const btnMaster = document.getElementById("btnMaster");
const btnTx = document.getElementById("btnTx");

const formatNow = () => new Date().toLocaleTimeString();

const log = (msg, isError = false) => {
  const prefix = isError ? "[ERRO]" : "";
  logDiv.textContent += `[${formatNow()}] ${prefix} ${msg}\n`;
  logDiv.scrollTop = logDiv.scrollHeight;
};

const setLoading = (button, loading) => {
  if (!button) return;
  button.disabled = loading;
  button.textContent = loading ? "Aguarde..." : button.dataset.label;
};

const ensureBluetooth = () => {
  if (!navigator.bluetooth) {
    const message = "Web Bluetooth nao e suportado neste navegador/dispositivo.";
    alert(message);
    log(message, true);
    btnMaster.disabled = true;
    btnTx.disabled = true;
    return false;
  }
  return true;
};

const ensureGeolocation = () => {
  if (!navigator.geolocation) {
    const message = "Geolocalizacao nao suportada neste navegador.";
    log(message, true);
    return false;
  }
  return true;
};

async function getGPS() {
  if (!ensureGeolocation()) return null;

  return new Promise((resolve) => {
    navigator.geolocation.getCurrentPosition(
      (pos) => resolve({ lat: pos.coords.latitude, lon: pos.coords.longitude }),
      (err) => {
        const message = err?.message || "Nao foi possivel obter a localizacao (permita o acesso ou tente novamente).";
        log(message, true);
        resolve(null);
      },
      { enableHighAccuracy: true, timeout: 8000 }
    );
  });
}

btnMaster.onclick = async () => {
  if (!ensureBluetooth()) return;
  setLoading(btnMaster, true);
  try {
    log("Procurando MASTER...");
    const device = await navigator.bluetooth.requestDevice({
      filters: [{ services: [MASTER_SERVICE] }],
    });
    log("Conectando...");
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(MASTER_SERVICE);
    const charac = await service.getCharacteristic(MASTER_CHAR);
    const value = await charac.readValue();
    const mac = new TextDecoder().decode(value);
    masterMacInput.value = mac;
    log("MASTER conectado: " + mac);
  } catch (e) {
    log("Erro ao conectar MASTER: " + e, true);
  } finally {
    setLoading(btnMaster, false);
  }
};

btnTx.onclick = async () => {
  if (!ensureBluetooth()) return;
  setLoading(btnTx, true);
  try {
    const masterMac = masterMacInput.value.trim();
    if (!masterMac) {
      log("[ERRO] Conecte ao MASTER primeiro.", true);
      return;
    }

    // Validate MAC address format
    const macRegex = /^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$/;
    if (!macRegex.test(masterMac)) {
      log("[ERRO] MAC inválido. Certifique-se de usar o formato XX:XX:XX:XX:XX:XX.", true);
      return;
    }

    log("[APP] Procurando transmissor...");
    const device = await navigator.bluetooth.requestDevice({
      filters: [{ namePrefix: TX_PREFIX }],
      optionalServices: [TX_SERVICE],
    });

    log(`[APP] Dispositivo encontrado: ${device.name}`);

    log("[APP] Conectando ao transmissor...");
    const server = await device.gatt.connect();
    log("[APP] Conexão BLE estabelecida.");

    const service = await server.getPrimaryService(TX_SERVICE);
    log(`[APP] Serviço BLE obtido: ${TX_SERVICE}`);

    const charac = await service.getCharacteristic(TX_CHAR);
    log(`[APP] Característica BLE obtida: ${TX_CHAR}`);

    const promptName = prompt("Nome do transmissor:", device.name) || device.name || "TX";
    const name = promptName.trim() || "TX";
    const gps = await getGPS();

    // O Transmissor espera a string MAC;NOME;LAT;LON
    const latStr = gps && gps.lat != null ? gps.lat : "";
    const lonStr = gps && gps.lon != null ? gps.lon : "";
    const payload = `${masterMac};${name};${latStr};${lonStr}`;

    log(`[APP] Enviando dados: ${payload}`);
    await charac.writeValue(new TextEncoder().encode(payload));
    log("[APP] Dados enviados com sucesso!");
  } catch (e) {
    log(`[ERRO] Falha na comunicação BLE: ${e}`, true);
  } finally {
    setLoading(btnTx, false);
  }
};

// Define labels para restaurar texto original apos loading
[btnMaster, btnTx].forEach((btn) => {
  if (btn) {
    btn.dataset.label = btn.textContent;
  }
});

ensureBluetooth();
