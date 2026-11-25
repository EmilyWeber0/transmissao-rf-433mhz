#define TEM_DISPLAY 0
#define MOSTRADOR 1   // 1 = OLED do teste; 2 = LCD ST7032i do guia.
#include "Aplicacao.h"

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(200);
  iniciarRadio();
  iniciarTela();
  Serial.println("Estacao A pronta. Monitor Serial em 115200 e Nova linha.");
  Serial.println("RF 433 MHz | PHY=MANCHESTER | ARQ=STOP-AND-WAIT | Diagnostico: SINAL ou DIAG");
  Serial.println("FCS inicial: CHECKSUM. Comandos de modo: MODO CHECKSUM | MODO CRC | STATUS");
  Serial.println("Comandos: TEXTO sua mensagem | IMAGEM | ERRO TEXTO teste | ERRO IMAGEM");
}

void loop() {
  Quadro q;
  if (receber(q, 15)) processar(q);
  if (Serial.available()) comandoSerial();
  yield();
}
