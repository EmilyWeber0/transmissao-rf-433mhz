# Testes e resultados

[Voltar ao README](../README.md) · [Fotos e registros dos testes](evidencias.md)

Os testes de bancada registraram envio de texto, exibição no OLED, recuperação após injeção de erro, reconstrução do PNG e comunicação a 1,10 m. Um ensaio específico sob ruído permanece pendente.

## Roteiro de demonstração

Configure os dois ESP32 no mesmo modo de FCS. Para seguir o ensaio registrado, use `MODO CRC` e confirme com `STATUS`.

| Ensaio | Como executar | O que registrar |
| --- | --- | --- |
| Texto | Envie `TEXTO TESTE` em A. | ACKs e conclusão em A. Mensagem recebida no Monitor Serial e no OLED de B. |
| Erro proposital | Envie `ERRO TEXTO Olá mundo!` em A. | Seq. 1 corrompida na primeira tentativa, descarte em B, timeout em A, retransmissão com ACK e texto correto no receptor. |
| Bitmap de exemplo | Envie `IMAGEM` em A. | A reconstrução dos 16 × 16 pixels em B. Com o script do PC B em escuta, confira o PNG salvo. |
| Arquivo PNG | Siga os [comandos do README](../README.md#3-transfira-um-arquivo-de-imagem-pelo-powershell) com `teste.png`. | `Imagem salva em` no PowerShell de B e abertura do arquivo gravado na pasta `recebidos`. |
| Ausência de ACK | Com B desligada, envie `TEXTO TESTE` de A. | Seis tentativas do quadro de início e `FALHA: ACK nao recebido apos 6 tentativas (seq=0). Envio interrompido.` em A. |
| Distância | Meça a separação entre as estações e repita o envio de texto. | Distância medida, mensagem em B e confirmação do END em A. |
| Ruído | Compare dois envios com a mesma configuração e distância em condições de bancada identificadas. | Condição do ensaio, timeouts, retransmissões e conteúdo recebido. |

Para o teste de erro, o comando de texto normal é opcional. O comando `ERRO TEXTO` já inicia uma transferência completa. Use os registros de A e B do mesmo envio para relacionar as linhas pela sequência.

## O que foi observado

### Detecção e recuperação

Na estação A, a equipe observou `ERRO SIMULADO seq=1`, timeout, tentativa 2 de 6 e confirmação da retransmissão. Em B, o FCS recebido para o quadro foi `E7` e o calculado foi `F8`. B registrou o descarte sem ACK. Em seguida aceitou a sequência 1, confirmou o quadro e apresentou **Olá mundo!**. A confirmação do END também exigiu uma nova tentativa no registro de A.

### Reconstrução de arquivo

O arquivo de entrada foi `teste.png`, com 134 bytes. O computador B registrou a gravação do PNG na pasta `recebidos`. A mensagem `ENVIO CONCLUIDO` indica o recebimento do ACK de um quadro END pelo ESP32 de origem. O script de transmissão pode emitir essa linha após cada mensagem de transporte. A evidência de que o arquivo inteiro chegou é `Imagem salva em` no computador receptor.

### Alcance

Foi registrado um envio bem-sucedido a **110 cm (1,10 m)**, medidos com trena na montagem de bancada. Registre novamente a distância e as condições caso mude a posição das placas ou a montagem. A injeção de erro no firmware demonstra a recuperação de um quadro conhecido. O teste sob ruído requer um ensaio separado no enlace de rádio.

## Se o teste parar

| Sintoma | Verificação |
| --- | --- |
| `FCS_INVALIDO` em vários quadros | Use `STATUS` nos dois Monitores Seriais para conferir se ambos estão no mesmo modo. |
| `TIMEOUT` seguido de `FALHA` | Confira se B está ligado, se o firmware foi gravado nas duas placas e se o teste funciona a curta distância. |
| Porta serial ocupada | Feche o Monitor Serial antes de iniciar o Python. Encerre o script com `Ctrl+C` antes de reabrir a porta na IDE. |
| `DISPLAY AUSENTE` ou tela vazia | Confira a alimentação, SDA/SCL e a seleção `MOSTRADOR 1` no sketch de B. |
| Imagem sem arquivo na pasta de saída | Confira se o receptor estava em `Receptor pronto` antes do envio e verifique o log de B para identificar um descarte ou transferência incompleta. |

No Windows, `Get-FileHash -Algorithm SHA256` permite comparar manualmente o PNG de origem e o que foi salvo em B. Passe o caminho de `teste.png` em A e o caminho exibido pelo programa em B. Os hashes devem coincidir.
