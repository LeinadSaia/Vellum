"""
Microserviço FastAPI - Motor de IA para o Ensinador de Inglês
=============================================================
Roda invisível em background e expõe as funcionalidades de IA
como endpoints HTTP para ser consumido por qualquer cliente
(interface Python, app C++, etc.)

Para iniciar:
    uvicorn main:app --host 0.0.0.0 --port 8000 --reload
"""

import asyncio
import threading
import warnings
import logging

import numpy as np
import sounddevice as sd
import whisper
import ollama
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import Optional
import base64
import io
import os
import tempfile
import re
from PIL import Image
import pytesseract
import shutil

# ---------------------------------------------------------------------------
# Configuração do executável do Tesseract (com detecção automática no Windows)
# ---------------------------------------------------------------------------
if os.name == 'nt':
    candidatos_tesseract = [
        r'C:\Program Files\Tesseract-OCR\tesseract.exe',
        r'C:\Program Files (x86)\Tesseract-OCR\tesseract.exe',
        os.path.expandvars(r'%LOCALAPPDATA%\Programs\Tesseract-OCR\tesseract.exe'),
    ]
    for caminho in candidatos_tesseract:
        if os.path.isfile(caminho):
            pytesseract.pytesseract.tesseract_cmd = caminho
            break
    else:
        cmd_path = shutil.which('tesseract')
        if cmd_path:
            pytesseract.pytesseract.tesseract_cmd = cmd_path

# ---------------------------------------------------------------------------
# Configuração de ambiente
# ---------------------------------------------------------------------------
warnings.filterwarnings("ignore", message="FP16 is not supported on CPU")

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s | %(levelname)s | %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger(__name__)

# Modelo Ollama configurável via variável de ambiente (padrão: llama3)
OLLAMA_MODEL = os.getenv("OLLAMA_MODEL", "llama3")

# Carrega o modelo Whisper UMA ÚNICA VEZ no startup — evita recarregar a cada request
log.info("Carregando modelo Whisper 'small'... (aguarde)")
_modelo_whisper = whisper.load_model("small")
log.info("Modelo Whisper carregado com sucesso.")

# ---------------------------------------------------------------------------
# Estado global de gravação (controlado por /iniciar_gravacao + /parar_gravacao)
# ---------------------------------------------------------------------------
_stop_event   = threading.Event()   # setado = sinal para parar o stream
_audio_buffer: list = []             # frames capturados
_gravacao_thread: Optional[threading.Thread] = None
_gravando = False


def _gravar_background(taxa_amostragem: int = 16000) -> None:
    """Grava continuamente do microfone até que _stop_event seja setado."""
    global _audio_buffer, _gravando
    tamanho_bloco = int(taxa_amostragem * 0.1)
    _audio_buffer = []
    try:
        with sd.InputStream(
            samplerate=taxa_amostragem,
            channels=1,
            dtype="float32",
            blocksize=tamanho_bloco,
        ) as stream:
            while not _stop_event.is_set():
                bloco, _ = stream.read(tamanho_bloco)
                _audio_buffer.append(bloco)
    except Exception as exc:
        log.error(f"[Gravação BG] Erro: {exc}")
    finally:
        _gravando = False
        log.info("[Gravação BG] Thread encerrada.")

# ---------------------------------------------------------------------------
# Configuração do App FastAPI
# ---------------------------------------------------------------------------
app = FastAPI(
    title="Ensinador de Inglês — IA Engine",
    description="Motor de processamento de IA local (Whisper + Ollama) exposto via REST.",
    version="1.0.0",
)

# CORS: libera conexões de qualquer origem local para o C++ conseguir chamar
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],          # Em produção, restringir para o endereço do cliente C++
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# ---------------------------------------------------------------------------
# Schemas (Pydantic) — contratos das requisições e respostas
# ---------------------------------------------------------------------------
class TextoSujoRequest(BaseModel):
    texto_sujo: Optional[str] = None
    imagem_base64: Optional[str] = None

class AvaliarRequest(BaseModel):
    texto_ingles: str
    texto_portugues: str

class TextoResponse(BaseModel):
    resultado: str

class NotaResponse(BaseModel):
    nota: int

class TranscricaoResponse(BaseModel):
    texto_transcrito: str

class TraduzirRequest(BaseModel):
    texto_ingles: Optional[str] = None
    imagem_base64: Optional[str] = None

class TraducaoResponse(BaseModel):
    texto_ingles: str
    traducao_portugues: str

# ---------------------------------------------------------------------------
# Funções de Processamento (Síncronas — rodam em thread pool)
# ---------------------------------------------------------------------------

def _limpar_ocr_sync(texto_sujo: Optional[str] = None, imagem_base64: Optional[str] = None) -> str:
    """
    Se uma imagem base64 for recebida, usa o Tesseract para extrair o texto.
    Chama o Ollama para corrigir erros de OCR em texto técnico de engenharia.
    Função síncrona/bloqueante — sempre chamar via asyncio.to_thread().
    """
    if imagem_base64:
        log.info("[OCR] Processando imagem Base64 recebida...")
        try:
            image_data = base64.b64decode(imagem_base64)
            image = Image.open(io.BytesIO(image_data))
            texto_sujo = pytesseract.image_to_string(image, lang='eng').strip()
            log.info(f"[OCR] Texto bruto extraído da imagem: {texto_sujo}")
            if not texto_sujo:
                return "(Nenhum texto detectado nesta área da imagem)"
        except Exception as e:
            log.error(f"[OCR] Erro ao decodificar/extrair imagem: {e}")
            raise RuntimeError(f"Erro ao extrair OCR da imagem: {e}")
            
    if not texto_sujo:
        return ""

    log.info(f"[OCR] Iniciando limpeza com IA de {len(texto_sujo)} caracteres...")

    prompt = f"""Você é um especialista em língua inglesa e correção de erros de OCR \
(Reconhecimento Ótico de Caracteres) de livros técnicos de Engenharia Elétrica.
Abaixo está um texto extraído de uma página escaneada com baixa qualidade.
Sua tarefa:
1. Corrija palavras quebradas, hifenizações erradas e caracteres trocados pelo OCR.
2. Preserve 100% do significado técnico: fórmulas, símbolos, termos de engenharia.
3. Mantenha o idioma original (inglês).
4. NÃO adicione introduções, comentários ou notas. Retorne APENAS o texto corrigido.

Texto com falhas de OCR:
{texto_sujo}
"""
    resposta = ollama.generate(
        model=OLLAMA_MODEL,
        prompt=prompt.strip(),
        options={
            "temperature": 0.1,
            "num_predict": 512,
        },
    )
    texto_limpo = resposta["response"].strip()
    log.info("[OCR] Limpeza concluída.")
    return texto_limpo


def _traduzir_sync(texto_ingles: Optional[str] = None, imagem_base64: Optional[str] = None) -> tuple:
    """
    Tradução técnica direta para Português usando o Ollama.
    Se imagem for enviada, usa OCR primeiro.
    Retorna tupla: (texto_ingles, traducao_portugues).
    """
    if imagem_base64:
        log.info("[Tradução] Extraindo texto da imagem via Tesseract...")
        try:
            image_data = base64.b64decode(imagem_base64)
            image = Image.open(io.BytesIO(image_data))
            texto_ingles = pytesseract.image_to_string(image, lang='eng').strip()
            log.info(f"[Tradução] OCR extraiu {len(texto_ingles)} caracteres.")
            if not texto_ingles:
                return ("", "(Nenhum texto detectado nesta área da imagem)")
        except Exception as e:
            log.error(f"[Tradução] Erro no OCR: {e}")
            raise RuntimeError(f"Erro ao extrair imagem: {e}")

    if not texto_ingles:
        return ("", "")

    log.info(f"[Tradução] Traduzindo {len(texto_ingles)} caracteres com Ollama ({OLLAMA_MODEL})...")

    prompt = f"""Você é um tradutor técnico especializado em Engenharia Elétrica, Computação e Ciências Exatas.
Traduza o seguinte texto do inglês para o português do Brasil com máxima fidelidade e precisão técnica.
Se houver pequenas falhas de OCR (como hífens quebrados ou letras trocadas), deduza o termo correto pelo contexto.
Preserve integralmente fórmulas, variáveis, símbolos e nomes próprios.
Retorne APENAS a tradução em português, sem introduções, aspas extras ou explicações adicionais.

Texto em inglês:
{texto_ingles}
"""
    resposta = ollama.generate(
        model=OLLAMA_MODEL,
        prompt=prompt.strip(),
        options={
            "temperature": 0.1,
            "num_predict": max(256, int(len(texto_ingles) * 2)),
        },
    )
    traducao = resposta["response"].strip()
    log.info("[Tradução] Tradução concluída.")
    return texto_ingles, traducao


def _avaliar_traducao_sync(texto_ingles: str, texto_portugues: str) -> int:
    """
    Avalia a precisão técnica de uma tradução usando o Ollama.
    Retorna um inteiro de 0 a 100.
    Função síncrona/bloqueante — sempre chamar via asyncio.to_thread().
    """
    log.info("[Avaliação] Enviando par de frases para o Ollama...")

    prompt = f"""Você é um avaliador técnico e preciso de traduções de Engenharia.
Avalie a similaridade de sentido entre a frase original em inglês e a tradução em português.

Inglês: "{texto_ingles}"
Português: "{texto_portugues}"

Regras Críticas:
1. Se a tradução mantiver 100% do significado técnico e científico, a nota DEVE SER EXATAMENTE 100.
2. Não desconte pontos por escolhas de estilo, ordem de palavras ou uso de sinônimos tecnicamente corretos.
3. Penalize (abaixo de 100) APENAS se houver: erro conceitual físico, omissão de informação crucial, \
ou adição de informação falsa.
4. Responda EXATAMENTE E APENAS com um número inteiro de 0 a 100. Nenhum texto adicional. \
Nenhum símbolo. Apenas o número.
"""
    resposta = ollama.generate(
        model=OLLAMA_MODEL,
        prompt=prompt.strip(),
        options={
            "temperature": 0.0,
            "num_predict": 5,
        },
    )
    texto_nota = resposta["response"].strip()
    log.info(f"[Avaliação] Ollama retornou: '{texto_nota}'")

    # Extrai apenas dígitos da resposta para garantir robustez
    digitos = "".join(filter(str.isdigit, texto_nota.split()[0] if texto_nota else "0"))
    nota = min(100, max(0, int(digitos))) if digitos else 0
    return nota


def _gravar_e_transcrever_sync(
    duracao_maxima: int = 15,
    taxa_amostragem: int = 16000,
    limite_silencio: float = 2.0,
    limite_volume: float = 0.01,
) -> str:
    """
    Grava o microfone via SoundDevice (VAD por silêncio de 2s),
    passa o array diretamente ao Whisper e retorna o texto.
    Função síncrona/bloqueante — sempre chamar via asyncio.to_thread().
    """
    log.info("[Microfone] Iniciando captura de áudio com VAD...")

    frames_gravados = []
    tempo_silencio = 0.0
    tamanho_bloco = int(taxa_amostragem * 0.1)  # blocos de 100ms
    falou_algo = False

    try:
        with sd.InputStream(
            samplerate=taxa_amostragem,
            channels=1,
            dtype="float32",
            blocksize=tamanho_bloco,
        ) as stream:
            limite_iteracoes = int(duracao_maxima / 0.1)
            for _ in range(limite_iteracoes):
                bloco_audio, _ = stream.read(tamanho_bloco)
                frames_gravados.append(bloco_audio)

                volume_atual = float(np.sqrt(np.mean(bloco_audio ** 2)))

                if volume_atual > limite_volume:
                    tempo_silencio = 0.0
                    falou_algo = True
                elif falou_algo:
                    tempo_silencio += 0.1
                    if tempo_silencio >= limite_silencio:
                        log.info(f"[Microfone] {limite_silencio}s de silêncio detectado. Encerrando.")
                        break

        log.info("[Microfone] Gravação finalizada.")

        audio_array = np.concatenate(frames_gravados).flatten()

        log.info("[Whisper] Transcrevendo áudio em memória...")
        resultado = _modelo_whisper.transcribe(audio_array, language="pt")
        texto = resultado["text"].strip()
        log.info(f"[Whisper] Transcrição: '{texto}'")
        return texto

    except Exception as exc:
        log.error(f"[Microfone] Erro durante captura/transcrição: {exc}")
        raise RuntimeError(str(exc))


# ---------------------------------------------------------------------------
# Endpoints da API
# ---------------------------------------------------------------------------

@app.get("/", tags=["Health"])
async def health_check():
    """Verifica se o servidor está rodando."""
    return {"status": "online", "motor": "Whisper small + Ollama llama3"}


@app.post("/limpar_ocr", response_model=TextoResponse, tags=["OCR"])
async def limpar_ocr(body: TextoSujoRequest):
    """
    Recebe um texto com erros de OCR e retorna a versão limpa e corrigida
    usando o LLM local (Ollama). O processamento ocorre em thread pool
    para não bloquear o event loop.
    """
    if not body.texto_sujo and not body.imagem_base64:
        raise HTTPException(status_code=422, detail="É necessário enviar 'texto_sujo' ou 'imagem_base64'.")

    try:
        resultado = await asyncio.to_thread(_limpar_ocr_sync, body.texto_sujo, body.imagem_base64)
    except Exception as exc:
        log.error(f"[/limpar_ocr] Erro: {exc}")
        raise HTTPException(status_code=500, detail=f"Erro no processamento IA: {exc}")

    return TextoResponse(resultado=resultado)


@app.post("/traduzir", response_model=TraducaoResponse, tags=["Tradução"])
async def traduzir(body: TraduzirRequest):
    """
    Tradução técnica direta para Português.
    Aceita texto em inglês puro ou imagem Base64.
    Retorna o texto em inglês identificado e a tradução técnica em português.
    """
    if not body.texto_ingles and not body.imagem_base64:
        raise HTTPException(status_code=422, detail="É necessário enviar 'texto_ingles' ou 'imagem_base64'.")

    try:
        texto_en, traducao_pt = await asyncio.to_thread(_traduzir_sync, body.texto_ingles, body.imagem_base64)
    except Exception as exc:
        log.error(f"[/traduzir] Erro: {exc}")
        raise HTTPException(status_code=500, detail=f"Erro na tradução: {exc}")

    return TraducaoResponse(texto_ingles=texto_en, traducao_portugues=traducao_pt)


@app.post("/avaliar_traducao", response_model=NotaResponse, tags=["Avaliação"])
async def avaliar_traducao(body: AvaliarRequest):
    """
    Recebe um par (inglês / português) e retorna uma nota inteira
    de 0 a 100 representando a precisão técnica da tradução.
    """
    if not body.texto_ingles.strip() or not body.texto_portugues.strip():
        raise HTTPException(status_code=422, detail="Os campos 'texto_ingles' e 'texto_portugues' são obrigatórios.")

    try:
        nota = await asyncio.to_thread(_avaliar_traducao_sync, body.texto_ingles, body.texto_portugues)
    except Exception as exc:
        log.error(f"[/avaliar_traducao] Erro: {exc}")
        raise HTTPException(status_code=500, detail=f"Erro no processamento IA: {exc}")

    return NotaResponse(nota=nota)


@app.post("/gravar_e_transcrever", response_model=TranscricaoResponse, tags=["Áudio"])
async def gravar_e_transcrever():
    """
    (legado — VAD automático) Aciona gravação com detecção de silêncio de 2s.
    Prefira /iniciar_gravacao + /parar_gravacao para controle manual.
    """
    try:
        texto = await asyncio.to_thread(_gravar_e_transcrever_sync)
    except RuntimeError as exc:
        raise HTTPException(status_code=500, detail=f"Falha na captura de áudio: {exc}")

    if not texto:
        raise HTTPException(status_code=204, detail="Nenhuma fala detectada pelo Whisper.")

    return TranscricaoResponse(texto_transcrito=texto)


# ---------------------------------------------------------------------------
# Novos endpoints: gravação manual com botão parar
# ---------------------------------------------------------------------------

@app.post("/iniciar_gravacao", tags=["Áudio"])
async def iniciar_gravacao():
    """
    Inicia a gravação do microfone em background.
    Retorna imediatamente — o microfone fica aberto até /parar_gravacao ser chamado.
    """
    global _gravacao_thread, _gravando

    if _gravando:
        raise HTTPException(status_code=409, detail="Já há uma gravação em andamento.")

    _stop_event.clear()
    _gravando = True
    _gravacao_thread = threading.Thread(target=_gravar_background, daemon=True)
    _gravacao_thread.start()
    log.info("[Gravação] Iniciada pelo usuário (modo botão).")
    return {"status": "gravando"}


@app.post("/parar_gravacao", response_model=TranscricaoResponse, tags=["Áudio"])
async def parar_gravacao():
    """
    Para a gravação iniciada por /iniciar_gravacao,
    passa o áudio ao Whisper e retorna o texto transcrito.
    """
    global _gravacao_thread, _gravando

    if not _gravando:
        raise HTTPException(status_code=400, detail="Nenhuma gravação em andamento.")

    _stop_event.set()
    if _gravacao_thread:
        await asyncio.to_thread(_gravacao_thread.join, 3.0)
    _gravacao_thread = None

    if not _audio_buffer:
        raise HTTPException(status_code=422, detail="Nenhum áudio capturado.")

    audio_array = np.concatenate(_audio_buffer).flatten()
    log.info(f"[Whisper] Transcrevendo {len(audio_array)/16000:.1f}s de áudio...")

    def _transcrever():
        return _modelo_whisper.transcribe(audio_array, language="pt")["text"].strip()

    texto = await asyncio.to_thread(_transcrever)
    log.info(f"[Whisper] Transcrição: '{texto}'")

    if not texto:
        raise HTTPException(status_code=204, detail="Nenhuma fala detectada pelo Whisper.")

    return TranscricaoResponse(texto_transcrito=texto)



