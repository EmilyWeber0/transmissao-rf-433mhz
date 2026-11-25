#pragma once
#include <Arduino.h>

// RF bruto com Manchester e limiar analogico ajustado no preambulo.
constexpr uint8_t PINO_TX = 25, PINO_RX = 34;
constexpr unsigned CHIP_US = 1000;
unsigned limiarMV = 1050;
constexpr uint8_t FLAG = 0x7E, DATA = 1, ACK = 2, END = 3;
bool usarCRC = false;
const char* nomeFCS() { return usarCRC ? "CRC-8" : "CHECKSUM"; }
struct Quadro {
  uint8_t tipo = DATA, seq = 0, len = 0;
  uint8_t dados[16] = {};
};

void iniciarRadio() {
  pinMode(PINO_TX, OUTPUT);
  digitalWrite(PINO_TX, LOW);
  pinMode(PINO_RX, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(PINO_RX, ADC_11db);
  analogReadMilliVolts(PINO_RX);
}

uint8_t checksum(const uint8_t* bytes, unsigned quantidade) {
  uint8_t soma = 0;
  for (unsigned i = 0; i < quantidade; ++i) soma += bytes[i];
  return soma;
}

// CRC-8: polinomio 0x07, inicial 0, sem reflexao e sem XOR final.
uint8_t calcularFCS(const uint8_t* bytes, unsigned quantidade) {
  if (!usarCRC) return checksum(bytes, quantidade);
  uint8_t crc = 0;
  for (unsigned i = 0; i < quantidade; ++i) {
    crc ^= bytes[i];
    for (int bit = 0; bit < 8; ++bit) {
      if (crc & 0x80) crc = uint8_t((crc << 1) ^ 0x07);
      else crc = uint8_t(crc << 1);
    }
  }
  return crc;
}

void pulso(unsigned alto, unsigned baixo) {
  digitalWrite(PINO_TX, HIGH);
  delayMicroseconds(alto);
  digitalWrite(PINO_TX, LOW);
  delayMicroseconds(baixo);
}

// 0 = alto/baixo e 1 = baixo/alto. Cada bit tem 50% de portadora.
void transmitir(const Quadro& q, bool corromper = false) {
  uint8_t bytes[21] = {FLAG, q.tipo, q.seq, q.len};
  memcpy(bytes + 4, q.dados, q.len);
  bytes[4 + q.len] = calcularFCS(bytes + 1, 3 + q.len);
  if (corromper && q.len >= 2) bytes[5] ^= 1;
  for (int i = 0; i < 48; ++i) pulso(CHIP_US, CHIP_US);
  pulso(6 * CHIP_US, 2 * CHIP_US);
  bool ultimoNivel = false;
  for (unsigned i = 0; i < 5u + q.len; ++i) {
    for (uint8_t bit = 0x80; bit; bit >>= 1) {
      bool um = bytes[i] & bit;
      digitalWrite(PINO_TX, !um); delayMicroseconds(CHIP_US);
      digitalWrite(PINO_TX, um); delayMicroseconds(CHIP_US);
      ultimoNivel = um;
    }
  }
  // A borda final permite ler a ultima metade do ultimo bit.
  digitalWrite(PINO_TX, !ultimoNivel); delayMicroseconds(CHIP_US);
  digitalWrite(PINO_TX, LOW);
}

bool lerNivel(bool ajustar) {
  static unsigned minimo = 3300, maximo = 0;
  static uint32_t inicio = 0;
  static bool alto = false;
  unsigned mv = analogReadMilliVolts(PINO_RX);
  if (ajustar) {
    if (mv < minimo) minimo = mv;
    if (mv > maximo) maximo = mv;
    if (millis() - inicio >= 30) {
      if (maximo > minimo + 150) limiarMV = (minimo + maximo) / 2;
      minimo = 3300; maximo = 0; inicio = millis();
    }
  }
  if (mv > limiarMV + 35) alto = true;
  else if (mv + 35 < limiarMV) alto = false;
  return alto;
}

bool receber(Quadro& q, unsigned janelaMs) {
  static bool antes = false, aguarda = false, emDados = false, metade = false, primeira = false;
  static uint32_t borda = 0;
  static uint8_t bytes[21], usados = 0, bits = 0, acumulador = 0;
  uint32_t inicio = millis();
  do {
    bool alto = lerNivel(!emDados && !aguarda);
    uint32_t agora = micros();
    if (alto == antes) {
      if ((emDados || aguarda) && agora - borda > 4 * CHIP_US) emDados = aguarda = false;
      continue;
    }
    bool nivelAnterior = antes;
    uint32_t largura = agora - borda;
    antes = alto; borda = agora;
    if (nivelAnterior && largura >= 5 * CHIP_US && largura <= 8 * CHIP_US) {
      aguarda = true; emDados = false;
      usados = bits = acumulador = 0; metade = false;
      continue;
    }
    if (aguarda) {
      aguarda = false;
      emDados = alto && largura >= CHIP_US && largura <= 3 * CHIP_US;
      continue;
    }
    if (!emDados) continue;
    if (largura < CHIP_US / 2 || largura > 5 * CHIP_US / 2) { emDados = false; continue; }
    unsigned chips = 1;
    if (largura > 3 * CHIP_US / 2) chips = 2;
    for (unsigned c = 0; c < chips; ++c) {
      if (!metade) { primeira = nivelAnterior; metade = true; continue; }
      metade = false;
      if (primeira == nivelAnterior) { emDados = false; break; }
      acumulador = (acumulador << 1) | nivelAnterior;
      if (++bits < 8) continue;
      bytes[usados++] = acumulador;
      bits = acumulador = 0;
      if ((usados == 1 && bytes[0] != FLAG) || (usados == 4 && bytes[3] > 16)) {
        emDados = false; break;
      }
      if (usados < 5 || usados != 5 + bytes[3]) continue;
      emDados = false;
      uint8_t calculado = calcularFCS(bytes + 1, 3 + bytes[3]);
      if (calculado != bytes[usados - 1]) {
        Serial.printf("LOG FCS_INVALIDO seq=%u recebido=%02X calculado=%02X fcs=%s. "
                      "Quadro descartado.%s\n",
                      bytes[2], bytes[usados - 1], calculado, nomeFCS(),
                      (bytes[1] == DATA || bytes[1] == END) ? " ACK nao enviado." : "");
        break;
      }
      q.tipo = bytes[1]; q.seq = bytes[2]; q.len = bytes[3];
      memcpy(q.dados, bytes + 4, q.len);
      return true;
    }
  } while (millis() - inicio < janelaMs);
  return false;
}

// Diagnostico local da entrada do RX. Nao representa uma medicao de RSSI.
void diagnosticarRF() {
  unsigned minimo = 3300, maximo = 0, bordas = 0;
  bool antes = lerNivel(true);
  uint32_t inicio = millis();
  while (millis() - inicio < 1000) {
    unsigned mv = analogReadMilliVolts(PINO_RX);
    if (mv < minimo) minimo = mv;
    if (mv > maximo) maximo = mv;
    bool alto = lerNivel(true);
    if (alto != antes) ++bordas;
    antes = alto;
    yield();
  }
  Serial.printf("DIAG RX: minimo=%u mV maximo=%u mV limiar=%u mV bordas=%u\n",
                minimo, maximo, limiarMV, bordas);
}

void sinalRF() {
  Serial.println("SINAL RF por 10 segundos. Execute DIAG no outro lado.");
  for (int i = 0; i < 5000; ++i) {
    pulso(CHIP_US, CHIP_US);
    if (i % 50 == 0) delay(1);
  }
  Serial.println("SINAL finalizado.");
}
