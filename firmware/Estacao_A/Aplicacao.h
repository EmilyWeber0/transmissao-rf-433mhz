#pragma once
#include "Radio.h"
#include "Tela.h"
#include "Imagem.h"
#include <esp_random.h>

uint8_t conteudo[256], esperado = 0;
uint16_t total = 0, recebidos = 0;
char formato = 'T';
bool emRecepcao = false, temUltimo = false;
Quadro ultimo;

// Exibe os pixels do buffer recebido, sem depender de uma imagem preinstalada.
void desenhar(const uint8_t* pixels) {
  Serial.println("Imagem monocromatica 16 x 16");
  for (int y = 0; y < 16; ++y) {
    Serial.print("|");
    for (int x = 0; x < 16; ++x) {
      bool aceso = pixels[y * 2 + x / 8] & (0x80 >> (x % 8));
      Serial.print(aceso ? "██" : "  ");
    }
    Serial.println("|");
  }
}

void confirmar(uint8_t seq) {
  delay(30);
  Quadro ack; ack.tipo = ACK; ack.seq = seq;
  transmitir(ack);
  Serial.printf("ACK enviado seq=%u\n", seq);
}

void processar(const Quadro& q) {
  if (q.tipo == ACK) return;
  bool repetido = temUltimo && q.tipo == ultimo.tipo && q.seq == ultimo.seq &&
                  q.len == ultimo.len && memcmp(q.dados, ultimo.dados, q.len) == 0;
  if (repetido) {
    Serial.printf("DUPLICADO seq=%u. Quadro ja aceito. Reenviando ACK.\n", q.seq);
    confirmar(q.seq); return;
  }
  bool inicio = q.tipo == DATA && q.seq == 0 && q.len == 8 && q.dados[0] == 1 &&
                (q.dados[1] == 'T' || q.dados[1] == 'I');
  if (!inicio && (!emRecepcao || q.seq != esperado)) {
    Serial.printf("Quadro descartado seq=%u. %s ACK nao enviado.\n", q.seq,
                  emRecepcao ? "Sequencia fora de ordem." : "Sem recepcao iniciada.");
    return;
  }
  bool final = q.tipo == END && q.len == 0;
  if (inicio) {
    uint16_t tamanho = (uint16_t(q.dados[6]) << 8) | q.dados[7];
    if (tamanho > sizeof(conteudo) || (q.dados[1] == 'I' && tamanho != 32)) {
      Serial.printf("INICIO descartado seq=%u. Tamanho invalido (%u bytes). ACK nao enviado.\n",
                    q.seq, tamanho);
      return;
    }
    total = tamanho; recebidos = 0; formato = q.dados[1]; emRecepcao = true;
    Serial.printf("INICIO formato=%c bytes=%u seq=%u\n", formato, total, q.seq);
  } else if (q.tipo == DATA && q.len >= 2 && q.dados[0] == 2 &&
             uint16_t(q.len - 1) <= total - recebidos) {
    memcpy(conteudo + recebidos, q.dados + 1, q.len - 1);
    recebidos += q.len - 1;
    Serial.printf("DATA recebido seq=%u acumulado=%u/%u\n", q.seq, recebidos, total);
  } else if (final) {
    if (recebidos != total) {
      Serial.printf("END descartado seq=%u. Recepcao incompleta (%u/%u bytes). ACK nao enviado.\n",
                    q.seq, recebidos, total);
      return;
    }
    emRecepcao = false;
  } else {
    Serial.printf("Quadro descartado seq=%u. Tipo ou dados invalidos. ACK nao enviado.\n", q.seq);
    return;
  }
  ultimo = q; temUltimo = true; esperado = uint8_t(q.seq + 1);
  confirmar(q.seq);
  if (!final) return;
  Serial.printf("END recebido. Transferencia concluida. seq=%u bytes=%u/%u\n",
                q.seq, recebidos, total);
  if (formato == 'I') {
    desenhar(conteudo);
    exibir("Imagem recebida");
  } else {
    String texto;
    for (uint16_t i = 0; i < total; ++i) texto += char(conteudo[i]);
    Serial.print("Texto recebido: "); Serial.println(texto);
    exibir(texto);
  }
}

bool enviarQuadro(const Quadro& q, bool alterar) {
  constexpr int maxTentativas = 6;
  constexpr uint32_t tempoAckMs = 1200;
  for (int tentativa = 1; tentativa <= maxTentativas; ++tentativa) {
    bool corromper = alterar && tentativa == 1;
    Serial.printf("TX tipo=%u seq=%u len=%u tentativa=%d/%d fcs=%s [%s]\n",
                  q.tipo, q.seq, q.len, tentativa, maxTentativas, nomeFCS(),
                  q.tipo == END ? "END" : "DATA");
    if (corromper) {
      Serial.printf("ERRO SIMULADO seq=%u. Alterando um bit apos calcular o FCS.\n", q.seq);
    }
    transmitir(q, corromper);
    uint32_t inicio = millis();
    while (millis() - inicio < tempoAckMs) {
      Quadro resposta;
      if (receber(resposta, 15) && resposta.tipo == ACK &&
          resposta.seq == q.seq && resposta.len == 0) {
        Serial.printf("ACK recebido seq=%u%s\n", q.seq,
                      tentativa > 1 ? " apos retransmissao." : "");
        delay(30); return true;
      }
    }
    Serial.printf("TIMEOUT seq=%u. ACK nao recebido (tentativa %d/%d).%s\n",
                  q.seq, tentativa, maxTentativas,
                  tentativa < maxTentativas ? " Retransmissao automatica." : "");
  }
  Serial.printf("FALHA: ACK nao recebido apos %d tentativas (seq=%u). Envio interrompido.\n",
                maxTentativas, q.seq);
  return false;
}

void enviarArquivo(char tipo, const uint8_t* dados, uint16_t tamanho, bool erro) {
  Quadro q; q.len = 8;
  q.dados[0] = 1; q.dados[1] = tipo;
  uint32_t id = esp_random();
  for (int i = 0; i < 4; ++i) q.dados[2 + i] = id >> (24 - 8 * i);
  q.dados[6] = tamanho >> 8; q.dados[7] = tamanho & 0xFF;
  if (!enviarQuadro(q, false)) return;
  uint8_t seq = 1;
  for (uint16_t pos = 0; pos < tamanho; pos += q.len - 1) {
    q = Quadro(); q.seq = seq++; q.dados[0] = 2;
    q.len = 1 + min(uint16_t(15), uint16_t(tamanho - pos));
    memcpy(q.dados + 1, dados + pos, q.len - 1);
    if (!enviarQuadro(q, erro && pos == 0)) return;
  }
  q = Quadro(); q.tipo = END; q.seq = seq;
  if (!enviarQuadro(q, false)) return;
  Serial.println("ENVIO CONCLUIDO");
  if (tipo == 'I') desenhar(dados);
}

void comandoSerial() {
  String comando = Serial.readStringUntil('\n'); comando.trim();
  if (comando == "MODO CHECKSUM" || comando == "MODO CRC" || comando == "MODO CRC8") {
    usarCRC = comando != "MODO CHECKSUM";
    if (emRecepcao) Serial.println("Recepcao parcial cancelada para trocar o FCS.");
    emRecepcao = temUltimo = false;
    recebidos = total = 0;
    Serial.printf("FCS=%s. Configure o mesmo modo no outro ESP32.\n", nomeFCS());
    return;
  }
  if (comando == "DIAG") { diagnosticarRF(); return; }
  if (comando == "SINAL") { sinalRF(); return; }
  if (comando == "STATUS") {
    Serial.printf("FCS=%s | ARQ=STOP-AND-WAIT | RX=%u/%u bytes | PHY=MANCHESTER | limiar=%u mV\n", nomeFCS(), recebidos, total, limiarMV);
    return;
  }
  bool erro = comando.startsWith("ERRO ");
  if (erro) comando.remove(0, 5);
  if (comando == "IMAGEM") enviarArquivo('I', IMAGEM, sizeof(IMAGEM), erro);
  else if (comando == "TEXTO" || comando.startsWith("TEXTO ")) {
    if (comando.length() <= 6 || comando.length() - 6 > sizeof(conteudo)) {
      Serial.println("Texto invalido. Digite TEXTO seguido de 1 a 256 bytes."); return;
    }
    String texto = comando.substring(6);
    enviarArquivo('T', reinterpret_cast<const uint8_t*>(texto.c_str()), texto.length(), erro);
  } else {
    Serial.println("Comando invalido. Use MODO CRC, MODO CHECKSUM, STATUS, TEXTO <mensagem>, "
                   "IMAGEM, ERRO TEXTO <mensagem>, ERRO IMAGEM, SINAL ou DIAG.");
  }
}
