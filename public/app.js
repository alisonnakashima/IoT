// CONFIGURAÇÃO DO FIREBASE
const firebaseConfig = {
  apiKey: "AIzaSyB6IydU8r4uH8-fOPR2MwKBv6HVW5BBh-M",
  authDomain: "irrigacao-iot-9a204.firebaseapp.com",
  databaseURL: "https://irrigacao-iot-9a204-default-rtdb.firebaseio.com",
  projectId: "irrigacao-iot-9a204",
  storageBucket: "irrigacao-iot-9a204.appspot.com",
  messagingSenderId: "645652723736",
  appId: "1:645652723736:web:bd3f76b9e77f1567306b6d"
};

firebase.initializeApp(firebaseConfig);
const db = firebase.database();

// ESTADOS
let modoAtual = "auto"; // auto | manual_on | manual_off

// REFERÊNCIAS
const leituraRef = db.ref("leituras/ultima");
const comandoRef = db.ref("comandos/irrigacao");

// LEITURAS DOS SENSORES
leituraRef.on("value", (snapshot) => {
  const d = snapshot.val();
  if (!d) return;

  // Transforma SOLO em %
  let auxSolo = Math.trunc(d.umidadeSolo/30);
  auxSolo = Math.max(0, Math.min(auxSolo, 100));
  
  document.getElementById("solo").innerText = auxSolo;
  document.getElementById("temp").innerText = d.temperatura;
  document.getElementById("ar").innerText = d.umidadeAr;
  document.getElementById("agua").innerText = d.nivelAgua;
  document.getElementById("status").innerText =
    d.irrigacao ? "Ligada" : "Desligada";
});

// LEITURA DO COMANDO ATUAL
comandoRef.on("value", (snapshot) => {
  const cmd = snapshot.val();

  if (!cmd || cmd.modo === null) {
    modoAtual = "auto";
  } else if (cmd.modo === true) {
    modoAtual = "manual_on";
  } else {
    modoAtual = "manual_off";
  }

  atualizarBotao();
});

// BOTÃO DE COMANDO
document.getElementById("btnIrrigar").addEventListener("click", () => {
  if (modoAtual === "auto") {
    comandoRef.set({ modo: true });
  } else if (modoAtual === "manual_on") {
    comandoRef.set({ modo: false });
  } else {
    comandoRef.set({ modo: null });
  }
});

// ATUALIZA BOTÃO
function atualizarBotao() {
  const btn = document.getElementById("btnIrrigar");

  if (modoAtual === "auto") {
    btn.innerText = "🔵 MODO AUTOMÁTICO";
    btn.style.background = "#3498db";
  }

  if (modoAtual === "manual_on") {
    btn.innerText = "🟢 IRRIGAÇÃO LIGADA (MANUAL)";
    btn.style.background = "#2ecc71";
  }

  if (modoAtual === "manual_off") {
    btn.innerText = "🔴 IRRIGAÇÃO DESLIGADA (MANUAL)";
    btn.style.background = "#e74c3c";
  }
}
