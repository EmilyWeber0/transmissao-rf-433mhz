"""Salva imagens recebidas pelo firmware Projeto_Oficial_433MHz2."""

import argparse
import base64
import hashlib
import secrets
import struct
import time
import zlib
from dataclasses import dataclass, field
from pathlib import Path


PASTA_RECEBIDOS = Path(__file__).resolve().parent / "recebidos"
ASSINATURA_PNG = b"\x89PNG\r\n\x1a\n"
MARCA = "@IMG433"
BLOCO = 165


def extensao(dados):
    if dados.startswith(ASSINATURA_PNG):
        return "png"
    if dados.startswith(b"\xff\xd8\xff"):
        return "jpg"
    raise ValueError("Selecione uma imagem PNG ou JPEG.")


def mensagens_de_imagem(dados):
    """Cada TEXTO cabe no limite de 256 bytes do firmware."""
    formato = extensao(dados)
    identificador = secrets.token_hex(4)
    resumo = hashlib.sha256(dados).hexdigest()
    yield f"{MARCA} I {identificador} {formato} {len(dados)} {resumo}"
    for pos in range(0, len(dados), BLOCO):
        bloco = base64.b64encode(dados[pos:pos + BLOCO]).decode("ascii")
        yield f"{MARCA} D {identificador} {pos} {bloco}"
    yield f"{MARCA} F {identificador}"


def png_do_bitmap(linhas):
    def secao(nome, dados):
        conteudo = nome + dados
        return struct.pack(">I", len(dados)) + conteudo + struct.pack(">I", zlib.crc32(conteudo))

    # Uma linha do Monitor Serial tem duas colunas de caracteres por pixel.
    pixels = bytearray()
    for linha in linhas:
        pixels.append(0)  # Tipo de filtro PNG da linha.
        for pos in range(0, 32, 2):
            pixels.append(0 if linha[pos:pos + 2] == "██" else 255)
    cabecalho = struct.pack(">IIBBBBB", 16, 16, 8, 0, 0, 0, 0)
    return (ASSINATURA_PNG + secao(b"IHDR", cabecalho)
            + secao(b"IDAT", zlib.compress(pixels)) + secao(b"IEND", b""))


@dataclass
class ImagemParcial:
    identificador: str
    formato: str
    tamanho: int
    resumo: str
    dados: bytearray = field(default_factory=bytearray)


class Receptor:
    def __init__(self, pasta):
        self.pasta = pasta
        self.bitmap = None
        self.arquivo = None

    def salvar(self, dados, formato):
        self.pasta.mkdir(parents=True, exist_ok=True)
        nome = f"imagem_recebida_{time.strftime('%Y%m%d_%H%M%S')}_{secrets.token_hex(6)}"
        final = self.pasta / f"{nome}.{formato}"
        parcial = final.with_suffix(".part")
        arquivo = parcial.open("xb")
        try:
            with arquivo:
                arquivo.write(dados)
            parcial.replace(final)
        finally:
            parcial.unlink(missing_ok=True)
        print(f"Imagem salva em {final}", flush=True)

    def receber_bitmap(self, linha):
        if linha == "Imagem monocromatica 16 x 16":
            self.bitmap = []
        elif self.bitmap is not None:
            miolo = linha[1:-1]
            if (not linha.startswith("|") or not linha.endswith("|") or len(miolo) != 32
                    or any(miolo[i:i + 2] not in ("██", "  ") for i in range(0, 32, 2))):
                self.bitmap = None
                print("Desenho incompleto. Envie IMAGEM novamente.", flush=True)
                return
            self.bitmap.append(miolo)
            if len(self.bitmap) == 16:
                self.salvar(png_do_bitmap(self.bitmap), "png")
                self.bitmap = None

    def receber_arquivo(self, mensagem):
        partes = mensagem.split()
        if len(partes) < 3:
            raise ValueError("mensagem de imagem incompleta")

        _, tipo, identificador = partes[:3]
        if tipo == "I" and len(partes) == 6:
            _, _, _, formato, quantidade, resumo = partes
            tamanho = int(quantidade)
            if formato not in ("png", "jpg") or tamanho <= 0 or len(bytes.fromhex(resumo)) != 32:
                raise ValueError("cabeçalho de imagem inválido")
            self.arquivo = ImagemParcial(identificador, formato, tamanho, resumo)
            print(f"Recebendo arquivo de {tamanho} bytes.", flush=True)
            return

        parcial = self.arquivo
        if parcial is None or identificador != parcial.identificador:
            return
        if tipo == "D" and len(partes) == 5:
            deslocamento = int(partes[3])
            bloco = base64.b64decode(partes[4], validate=True)
            restante = parcial.tamanho - len(parcial.dados)
            if deslocamento != len(parcial.dados) or not bloco or len(bloco) != min(BLOCO, restante):
                raise ValueError("bloco faltando ou fora de ordem")
            parcial.dados.extend(bloco)
        elif tipo == "F" and len(partes) == 3:
            dados = parcial.dados
            self.arquivo = None
            if len(dados) != parcial.tamanho or hashlib.sha256(dados).hexdigest() != parcial.resumo:
                raise ValueError("tamanho ou verificação SHA-256 divergente")
            if extensao(dados) != parcial.formato:
                raise ValueError("formato diferente do anunciado")
            self.salvar(dados, parcial.formato)
        else:
            raise ValueError("mensagem de imagem inválida")

    def processar(self, linha):
        prefixo = "Texto recebido: "
        if linha.startswith(prefixo + MARCA + " "):
            try:
                self.receber_arquivo(linha[len(prefixo):])
            except ValueError as erro:
                self.arquivo = None
                print(f"Imagem descartada. Reenvie o arquivo. Motivo: {erro}", flush=True)
        else:
            print(linha, flush=True)
            self.receber_bitmap(linha)


class CanalUSB:
    def __init__(self, porta):
        self.porta = porta
        self.parte = bytearray()

    def ler(self):
        bloco = self.porta.read_until(b"\n", size=1024)
        self.parte.extend(bloco)
        if bloco.endswith(b"\n"):
            linha = self.parte.decode("utf-8", errors="replace").rstrip("\r\n")
            self.parte.clear()
            return linha
        if len(self.parte) >= 1024:
            self.parte.clear()
        return None

    def comando(self, texto, resposta):
        self.porta.write((texto + "\n").encode("ascii"))
        limite = time.monotonic() + 10
        while time.monotonic() < limite:
            linha = self.ler()
            if not linha:
                continue
            print(linha, flush=True)
            if linha.startswith(resposta):
                return
            if linha.startswith("FALHA"):
                raise RuntimeError("O rádio não recebeu a confirmação. Confira o PC receptor e tente novamente.")
            if linha.startswith(("TX tipo=", "ACK recebido", "TIMEOUT")):
                limite = time.monotonic() + 10
        raise TimeoutError("O ESP32 não respondeu ao comando. Confira a porta e o firmware gravado.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    comandos = parser.add_subparsers(dest="acao", required=True)
    comandos.add_parser("receber").add_argument("porta", help="Porta do ESP32 B, por exemplo COM4")
    enviar = comandos.add_parser("enviar")
    enviar.add_argument("porta", help="Porta do ESP32 A, por exemplo COM3")
    enviar.add_argument("imagem", type=Path, help="Caminho de um PNG ou JPEG pequeno")
    args = parser.parse_args()
    if args.acao == "enviar":
        dados = args.imagem.read_bytes()
        extensao(dados)

    try:
        import serial
    except ImportError:
        raise RuntimeError("Instale o pyserial com py -m pip install -r .\\requirements.txt.") from None

    porta = serial.Serial(baudrate=115200, timeout=0.3, write_timeout=2)
    porta.port = args.porta
    porta.dtr = porta.rts = False
    with porta:
        time.sleep(2)
        porta.reset_input_buffer()
        canal = CanalUSB(porta)
        canal.comando("MODO CRC", "FCS=CRC-8.")
        if args.acao == "receber":
            receptor = Receptor(PASTA_RECEBIDOS)
            print(f"Receptor pronto. Destino em {PASTA_RECEBIDOS}", flush=True)
            while True:
                linha = canal.ler()
                if linha:
                    receptor.processar(linha)
        else:
            print(f"Enviando {args.imagem.name} ({len(dados)} bytes).", flush=True)
            for mensagem in mensagens_de_imagem(dados):
                canal.comando("TEXTO " + mensagem, "ENVIO CONCLUIDO")
            print("Envio encerrado. Confira a mensagem Imagem salva no PC receptor.")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nPrograma encerrado.")
    except (OSError, ValueError, RuntimeError) as erro:
        raise SystemExit(f"Erro: {erro}")
