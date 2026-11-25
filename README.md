# Transmissão confiável de dados por rádio de 433 MHz

Projeto final de **Transmissão e Comunicação de Dados** do curso de Engenharia da Computação do IFMT. Duas estações ESP32 trocam texto e imagens por módulos de 433 MHz. O firmware verifica os quadros com **CRC-8** ou **checksum de 8 bits** e retransmite perdas com **Stop-and-Wait ARQ**. A estação B exibe texto no OLED e pode salvar imagens no computador.

| Guia | Conteúdo |
| --- | --- |
| [Montagem](docs/montagem.md) | Componentes, pinagem, divisor de tensão e gravação dos ESP32. |
| [Arquitetura e código](docs/arquitetura.md) | Formato dos quadros, Manchester, cálculo do CRC e funções do firmware e do Python. |
| [Testes e resultados](docs/testes.md) | Roteiro da demonstração, leitura dos logs e resultados de bancada. |

## Como as estações se comunicam

O computador A envia o comando ao ESP32 A por USB. A transmite os quadros por rádio, B valida os dados e responde com um ACK pelo mesmo enlace. B remonta a mensagem e a entrega ao seu computador pela USB local. Cada estação tem um transmissor e um receptor de 433 MHz. Envie em um sentido por vez.

## 1. Prepare os ESP32

1. [Monte as duas estações](docs/montagem.md) e conecte cada placa ao respectivo computador por USB.
2. Na Arduino IDE, instale **esp32 by Espressif Systems** pelo Gerenciador de Placas e **U8g2** pelo Gerenciador de Bibliotecas para compilar o sketch de B com OLED.
3. Abra [Estacao_A.ino](firmware/Estacao_A/Estacao_A.ino) para gravar a placa A e [Estacao_B.ino](firmware/Estacao_B/Estacao_B.ino) para gravar a placa B. Mantenha os arquivos `.h` junto ao sketch. Selecione **ESP32 Dev Module** para a placa usada no ensaio e a porta correspondente.
4. Abra os dois Monitores Seriais em **115200 baud**, com final de linha **Nova linha**. Envie `MODO CRC` e depois `STATUS` em **cada** estação. Confirme `FCS=CRC-8` nos dois lados. O firmware inicializa em checksum após reiniciar.

## 2. Envie texto e simule um erro

Digite um comando por vez no Monitor Serial de A. Deixe o Monitor Serial de B aberto para acompanhar a recepção.

| Comando | Resultado esperado |
| --- | --- |
| `TEXTO TESTE` | B mostra `Texto recebido: TESTE` e apresenta o texto no OLED. |
| `ERRO TEXTO Olá mundo!` | B detecta a divergência de FCS na sequência 1. A retransmite o quadro e B entrega o texto completo. |
| `IMAGEM` | B reconstrói o desenho de 16 × 16 pixels no Monitor Serial. O OLED mostra `Imagem recebida`. |
| `ERRO IMAGEM` | Executa a mesma injeção de erro no primeiro quadro de conteúdo do bitmap. |
| `MODO CHECKSUM` | Ativa a soma de 8 bits. Envie o comando também na outra estação. |
| `MODO CRC` / `STATUS` | Ativa o CRC-8 / mostra o modo de FCS e o estado da recepção. |

O texto tem limite de **256 bytes**, incluindo os bytes adicionais de caracteres UTF-8. Para uma demonstração legível no OLED, use uma mensagem ASCII curta, como `TESTE`. Para confirmar a recuperação do erro, compare `FCS_INVALIDO` em B com `TIMEOUT`, nova tentativa e `ACK recebido` em A. Os eventos devem ter a mesma sequência.

## 3. Transfira um arquivo PNG ou JPEG

Use Python 3.8 ou superior. Em ambos os computadores, abra o PowerShell na pasta raiz do repositório. Feche os Monitores Seriais para liberar as portas e instale a dependência.

```powershell
cd .\computadores
py -m pip install -r .\requirements.txt
```

**Computador B** — inicie a recepção e aguarde `Receptor pronto`:

```powershell
py .\imagem_rf.py receber COM4
```

**Computador A** — a partir da mesma pasta, envie o [PNG de teste](computadores/teste.png), de 134 bytes:

```powershell
py .\imagem_rf.py enviar COM3 ".\teste.png"
```

Substitua `COM3` e `COM4` pelas portas atribuídas às placas. O programa coloca cada estação em CRC-8 ao abrir sua porta. Abra o PNG em **`computadores/recebidos/`** no PC B após a mensagem `Imagem salva em`. Use `Ctrl+C` para encerrar o receptor e liberar a porta. Para enviar outra imagem pequena, passe o caminho de um PNG ou JPEG entre aspas.

O comando `IMAGEM` também pode gerar um PNG. Deixe o programa Python rodando em B no modo `receber`, configure A em CRC-8 e envie `IMAGEM` pelo Monitor Serial de A. O script converte as linhas do bitmap recebidas pela serial em arquivo.

## Resultados registrados

A equipe registrou a recepção de texto, a exibição da mensagem no OLED, a detecção e recuperação de um quadro corrompido e a gravação do PNG no computador B. Um envio de bancada foi concluído com as estações a **1,10 m**. O ensaio sob ruído permanece pendente. A [página de testes](docs/testes.md) descreve as evidências e como repetir cada cenário.
