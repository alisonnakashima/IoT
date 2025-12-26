// =======================================================
// SISTEMA INTELIGENTE DE IRRIGAÇÃO - ESP32
// Servo + Umidade do Solo + DHT11 + Sensor de Pressão
// =======================================================

#include <ESP32Servo.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ---------------- CONFIGURAÇÕES ----------------
#define DHTPIN   27
#define DHTTYPE  DHT11

const int SERVO_PIN        = 26;
const int PIN_UMIDADE_SOLO = 35;
const int PIN_PRESSAO_AGUA = 34;

const int ANGULO_FECHADO = 0;
const int ANGULO_ABERTO  = 90;

// Limiares
const int   UMIDADE_SOLO_SECO      = 500;
const float TEMP_MIN_IRRIGACAO     = 10.0;
const float UMIDADE_AR_MAX         = 80.0;
const int   AGUA_MINIMA_RESERVATORIO = 400;

// ---------------- OBJETOS ----------------
Servo servoIrrigacao;
DHT dht(DHTPIN, DHTTYPE);

// ---------------- WIFI / URLs ----------------
const char* ssid = "AH-home";
const char* password = "ah102030405060";

const char* urlPostLeituras = "http://192.168.0.141:1880/leituras";
const char* urlGetComandos  = "http://192.168.0.141:1880/comandos";

// ---------------- VARIÁVEIS ----------------
bool irrigacaoAberta = false;
String modoSistema = "auto"; // auto | manual_on | manual_off

// =======================================================

void setup() {
  Serial.begin(115200);

  servoIrrigacao.setPeriodHertz(50);
  servoIrrigacao.attach(SERVO_PIN, 500, 2400);

  pinMode(PIN_UMIDADE_SOLO, INPUT);
  pinMode(PIN_PRESSAO_AGUA, INPUT);

  dht.begin();
  fecharIrrigacao();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado");
  Serial.println("Sistema de irrigação iniciado");
}

// =======================================================

void loop() {

  // 🔄 Atualiza comando remoto
  buscarModoRemoto();

  // 📊 Leituras
  int umidadeSolo = analogRead(PIN_UMIDADE_SOLO);
  int nivelAgua   = lerNivelAguaReservatorio();
  float temperatura = dht.readTemperature();
  float umidadeAr   = dht.readHumidity();

  if (isnan(temperatura) || isnan(umidadeAr)) {
    Serial.println("Erro DHT11");
    delay(3000);
    return;
  }

  // ---------------- DEBUG ---------------- //
  Serial.println("----- LEITURA DOS SENSORES -----");

  Serial.print("Umidade do solo (ADC): ");
  Serial.println(umidadeSolo);

  Serial.print("Nivel do Reservatório de água: ");
  Serial.println(nivelAgua);

  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.println(" °C");

  Serial.print("Umidade do ar: ");
  Serial.print(umidadeAr);
  Serial.println(" %");

  Serial.print("Irrigação: ");
  Serial.println(irrigacaoAberta ? "ON" : "OFF");

  Serial.println("--------------------------------");
  Serial.println("\n----- STATUS ATUAL -----");
  Serial.println("Modo: " + modoSistema);

  // 🧠 DECISÃO PRINCIPAL
  if (modoSistema == "manual_on") {
    abrirIrrigacao();
  }
  else if (modoSistema == "manual_off") {
    fecharIrrigacao();
  }
  else {
    avaliarIrrigacaoAutomatica(
      umidadeSolo, temperatura, umidadeAr, nivelAgua
    );
  }

  // 📤 Envio das leituras
  enviarLeituras(
    umidadeSolo, temperatura, umidadeAr, nivelAgua
  );

  delay(5000);
}

// =======================================================
// FUNÇÕES
// =======================================================

// --- LEITURA DE NÍVEL DE RESERVATÓRIO (M.A de 10, REDUÇÃO DE RUÍDOS)  ---
int lerNivelAguaReservatorio() {
  int leituraInicial = analogRead(PIN_PRESSAO_AGUA);

  // Condicional de validação
  if (leituraInicial > 0) {

    int soma = 0;
    const int NUM_LEITURAS = 10;

    for (int i = 0; i < NUM_LEITURAS; i++) {
      soma += analogRead(PIN_PRESSAO_AGUA);
      delay(1000); // pequeno atraso para estabilidade
    }

    int media = soma / NUM_LEITURAS;
    return media;

  } else {
    // Falha na leitura
    Serial.println("Erro na leitura do sensor de nivel de agua");
    return 0;
  }
}


void avaliarIrrigacaoAutomatica(
  int umidadeSolo,
  float temperatura,
  float umidadeAr,
  int nivelAgua
) {
  bool soloSeco   = umidadeSolo <= UMIDADE_SOLO_SECO;
  bool tempOk     = temperatura >= TEMP_MIN_IRRIGACAO;
  bool arOk       = umidadeAr <= UMIDADE_AR_MAX;
  bool aguaOk     = nivelAgua >= AGUA_MINIMA_RESERVATORIO;

  if (soloSeco && tempOk && arOk && aguaOk) {
    abrirIrrigacao();
  } else {
    fecharIrrigacao();
  }
}

// --- SERVO MOTOR --- //
void abrirIrrigacao() {
  if (!irrigacaoAberta) {
    servoIrrigacao.write(ANGULO_ABERTO);
    irrigacaoAberta = true;
    Serial.println(">> Irrigação ABERTA");
  }
}

void fecharIrrigacao() {
  if (irrigacaoAberta) {
    servoIrrigacao.write(ANGULO_FECHADO);
    irrigacaoAberta = false;
    Serial.println(">> Irrigação FECHADA");
  }
}

// --- COMANDOS --- //
void buscarModoRemoto() {

  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(urlGetComandos);
  int httpCode = http.GET();
  Serial.print("GET Comandos -> HTTP Code: ");
  Serial.println(httpCode);

  if (httpCode == 200) {
    String payload = http.getString();

    Serial.print("Payload recebido: ");
    Serial.println(payload);

    // 🔵 AUTOMÁTICO → modo = null
    if (payload.indexOf("\"modo\":null") >= 0 || payload.indexOf("\"modo\"") == -1) {
      modoSistema = "auto";
    }
    // 🟢 MANUAL ON → modo = true
    else if (payload.indexOf("\"modo\":true") >= 0) {
      modoSistema = "manual_on";
    }
    // 🔴 MANUAL OFF → modo = false
    else if (payload.indexOf("\"modo\":false") >= 0) {
      modoSistema = "manual_off";
    }

    Serial.print("Modo interpretado: ");
    Serial.println(modoSistema);
  }
  
  http.end();
}

// --- ENVIO --- //
void enviarLeituras(
  int umidadeSolo,
  float temperatura,
  float umidadeAr,
  int nivelAgua
) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(urlPostLeituras);
  http.addHeader("Content-Type", "application/json");

  String payload =
    "{ \"leituras\": { \"ultima\": {"
    "\"umidadeSolo\": " + String(umidadeSolo) + ","
    "\"temperatura\": " + String(temperatura) + ","
    "\"umidadeAr\": " + String(umidadeAr) + ","
    "\"nivelAgua\": " + String(nivelAgua) + ","
    "\"irrigacao\": " + String(irrigacaoAberta ? "true" : "false") +
    "} } }";

  int httpResponseCode = http.POST(payload);
  // --- DEBUG --- //
  Serial.print("POST Leituras -> HTTP Code: ");
  Serial.println(httpResponseCode);
  // --- DEBUG --- //
  http.end();
}
