# Montagem das estações

[Voltar ao README](../README.md)

Cada protoboard recebe um ESP32, um transmissor e um receptor de 433 MHz. A estação B também recebe o display OLED. Alimente as placas por USB, uma em cada computador, e use o GND da própria placa como referência para os componentes conectados a ela.

## Materiais

| Item | Quantidade | Uso |
| --- | ---: | --- |
| Computador com USB | 2 | Monitor Serial e programa de envio ou recepção de arquivos. |
| ESP32 DevKit | 2 | Geração e leitura dos pulsos, protocolo e comunicação serial. |
| Transmissor de 433 MHz | 2 | Um para cada estação. Modelo de referência FS1000A. |
| Receptor de 433 MHz | 2 | Um para cada estação. Modelo de referência MX-RM-5V. |
| OLED SSD1306 I²C de 128 × 64 | 1 | Exibição de texto e status do bitmap em B. |
| Resistor de 10 kΩ | 4 | Dois em cada divisor da saída DATA do receptor. |
| Protoboard e cabo USB de dados | 2 de cada | Montagem e ligação local ao computador. |
| Jumpers macho-macho | 18 na montagem documentada | Interligação dos componentes. |

## Ligações elétricas

Repita as ligações de rádio em A e B. Confira a identificação de **DATA, VCC e GND** impressa no seu módulo, pois a ordem física dos pinos pode variar entre modelos.

| Componente | Pino | Ligação na estação |
| --- | --- | --- |
| Transmissor RF | DATA | GPIO25 do ESP32. |
| Transmissor RF | VCC | VIN da placa na montagem documentada. |
| Receptor RF | DATA | R1 do divisor, com a junção R1/R2 no GPIO34. |
| Receptor RF | VCC | 5 V no esquema de referência do MX-RM-5V. |
| Módulos RF | GND | GND do ESP32 local. |
| OLED de B | VCC / GND | 3V3 / GND do ESP32 B. |
| OLED de B | SDA / SCL | GPIO21 / GPIO22 do ESP32 B. |

Ligue R1 entre **DATA do receptor** e **GPIO34**. Ligue R2 entre **GPIO34** e **GND**. Com R1 e R2 de 10 kΩ, o divisor fornece metade de VDATA no GPIO34. Uma saída de 5,0 V produz **2,5 V** no cálculo ideal. O firmware mede essa tensão com o ADC e adapta o limiar de leitura durante a busca pelo início do quadro.

Mantenha as antenas dos módulos desobstruídas. Para repetir uma medida de alcance, marque a distância entre as estações e mantenha a orientação das antenas ao comparar tentativas.

## Gravação e verificação inicial

1. Instale o pacote **esp32 by Espressif Systems** e a biblioteca **U8g2** na Arduino IDE.
2. Abra o sketch [Estacao_A.ino](../firmware/Estacao_A/Estacao_A.ino) e selecione a placa e a porta de A. Use o perfil **ESP32 Dev Module** para o DevKit da montagem.
3. Abra e grave [Estacao_B.ino](../firmware/Estacao_B/Estacao_B.ino) na placa B. A configuração `TEM_DISPLAY 1` habilita o mostrador e `MOSTRADOR 1` escolhe o OLED.
4. Abra cada Monitor Serial em 115200 baud e selecione **Nova linha**. B deve registrar `DISPLAY OK` se o módulo responder por I²C. Se aparecer `DISPLAY AUSENTE`, confira alimentação e SDA/SCL.
5. Envie `MODO CRC` nos dois lados e valide a comunicação a curta distância com `TEXTO TESTE`.

O driver procura o SSD1306 nos endereços `0x3C` e `0x3D`. O firmware também contém uma rotina para LCD ST7032i no endereço `0x3E`, selecionada com `MOSTRADOR 2`. As evidências de bancada deste projeto foram registradas com o OLED.
