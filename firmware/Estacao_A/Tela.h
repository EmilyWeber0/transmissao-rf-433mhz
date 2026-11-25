#pragma once
#if TEM_DISPLAY
#include <Wire.h>
#if MOSTRADOR == 1
#include <U8g2lib.h>
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, 22, 21);
#endif
bool telaPresente = false;

void lcd(uint8_t valor, bool caractere = false) {
  Wire.beginTransmission(0x3E);
  Wire.write(caractere ? 0x40 : 0);
  Wire.write(valor);
  Wire.endTransmission();
}

void exibir(const String& texto) {
  if (!telaPresente) return;
#if MOSTRADOR == 1
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
  for (int i = 0; i < 6; ++i)
    oled.drawStr(0, 10 + i * 10, texto.substring(i * 21, i * 21 + 21).c_str());
  oled.sendBuffer();
#elif MOSTRADOR == 2
  for (int linha = 0; linha < 2; ++linha) {
    lcd(linha ? 0xC0 : 0x80);
    for (int j = 0; j < 16; ++j) {
      int pos = linha * 16 + j;
      lcd(pos < int(texto.length()) ? texto[pos] : ' ', true);
    }
  }
#endif
}

void iniciarTela() {
  Wire.begin(21, 22);
  Wire.setClock(100000);
#if MOSTRADOR == 1
  for (uint8_t endereco = 0x3C; endereco <= 0x3D; ++endereco) {
    Wire.beginTransmission(endereco);
    if (Wire.endTransmission() != 0) continue;
    oled.setI2CAddress(endereco << 1);
    oled.setBusClock(100000);
    oled.begin(); telaPresente = true; break;
  }
#elif MOSTRADOR == 2
  Wire.beginTransmission(0x3E);
  if (Wire.endTransmission() == 0) {
    delay(45);
    const uint8_t ajustes[] = {0x38, 0x39, 0x14, 0x70, 0x56, 0x6C};
    for (uint8_t c : ajustes) lcd(c);
    delay(220); lcd(0x38); lcd(0x0C); lcd(0x01); delay(3);
    telaPresente = true;
  }
#endif
  Serial.println(telaPresente ? "DISPLAY OK" : "DISPLAY AUSENTE");
  exibir("Aguardando RF");
}
#else
void iniciarTela() {}
void exibir(const String&) {}
#endif
