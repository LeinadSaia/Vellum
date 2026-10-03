"""
Microserviço FastAPI - Motor de IA & Tutor para o Leitor Técnico
================================================================
Roda em background e expõe as funcionalidades de IA como endpoints HTTP:
- OCR Otimizado (Tesseract com PSM 6 e conversão em escala de cinza)
- Tradução Direta e Contextualizada (Ollama local)
- Text-to-Speech com Vozes Neurais (Edge TTS + Miniaudio/SoundDevice)
- Transcrição de Áudio de Alta Performance (OpenAI Whisper)
- Avaliação Fonética e Feedback de Pronúncia para o Tutor

Para iniciar:
    uvicorn main:app --host 0.0.0.0 --port 8000 --reload
"""

import asyncio
import threading
import warnings
import logging
import base64
import io
import os
import sys
import re
import tempfile
import time
import shutil
import difflib
from typing import Optional, List, Dict, Tuple

import numpy as np
import sounddevice as sd
import whisper
import ollama
import edge_tts
import miniaudio
from PIL import Image, ImageOps
import pytesseract
import httpx
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

# ---------------------------------------------------------------------------
# Cliente HTTP persistente (Connection Pooling & Keep-Alive para Google Gemini)
# Elimina overhead de handshake TLS/TCP a cada chamada
# ---------------------------------------------------------------------------
_http_async_client: Optional[httpx.AsyncClient] = None

def _obter_http_client() -> httpx.AsyncClient:
    global _http_async_client
    if _http_async_client is None or _http_async_client.is_closed:
        _http_async_client = httpx.AsyncClient(
            timeout=httpx.Timeout(15.0, connect=5.0),
            limits=httpx.Limits(max_keepalive_connections=20, max_connections=50, keepalive_expiry=60.0)
        )
    return _http_async_client

# ---------------------------------------------------------------------------
# Configuração do executável do Tesseract (detecção local e de sistema)
# ---------------------------------------------------------------------------
base_dir = os.path.dirname(os.path.abspath(__file__))
exec_dir = os.path.dirname(sys.executable) if hasattr(sys, 'executable') else base_dir

candidatos_tesseract = []
if os.name == 'nt':
    candidatos_tesseract.extend([
        os.path.join(base_dir, "tesseract", "tesseract.exe"),
        os.path.join(base_dir, "Tesseract-OCR", "tesseract.exe"),
        os.path.join(base_dir, "bin", "tesseract.exe"),
        os.path.join(exec_dir, "tesseract", "tesseract.exe"),
        os.path.join(exec_dir, "Tesseract-OCR", "tesseract.exe"),
        os.path.join(exec_dir, "bin", "tesseract.exe"),
        r'C:\Program Files\Tesseract-OCR\tesseract.exe',
        r'C:\Program Files (x86)\Tesseract-OCR\tesseract.exe',
        os.path.expandvars(r'%LOCALAPPDATA%\Programs\Tesseract-OCR\tesseract.exe'),
    ])
else:
    candidatos_tesseract.extend([
        os.path.join(base_dir, "bin", "tesseract"),
        os.path.join(exec_dir, "bin", "tesseract"),
    ])

for caminho in candidatos_tesseract:
    if os.path.isfile(caminho):
        pytesseract.pytesseract.tesseract_cmd = caminho
        tess_dir = os.path.dirname(caminho)
        tessdata_local = os.path.join(tess_dir, "tessdata")
        if os.path.isdir(tessdata_local) and "TESSDATA_PREFIX" not in os.environ:
            os.environ["TESSDATA_PREFIX"] = tessdata_local
        break
else:
    cmd_path = shutil.which('tesseract')
    if cmd_path:
        pytesseract.pytesseract.tesseract_cmd = cmd_path

# ---------------------------------------------------------------------------
# Configuração de Logs e Ambiente
# ---------------------------------------------------------------------------
warnings.filterwarnings("ignore", message="FP16 is not supported on CPU")

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s | %(levelname)s | %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("leitor_backend")

# Configuração de Modelos via Variáveis de Ambiente
import functools
import hashlib

OLLAMA_MODEL = os.getenv("OLLAMA_MODEL", "llama3")
WHISPER_MODEL_NAME = os.getenv("WHISPER_MODEL", "base.en")


def _obter_modelos_instalados() -> List[str]:
    """Consulta o Ollama local e retorna lista de nomes de modelos instalados (em minúsculas)."""
    try:
        resp = ollama.list()
        models_list = resp.models if hasattr(resp, "models") else (resp.get("models", []) if isinstance(resp, dict) else [])
        instalados = []
        for m in models_list:
            nome = getattr(m, "model", None) or (m.get("model") if isinstance(m, dict) else str(m))
            if nome:
                instalados.append(nome.lower().strip())
        return instalados
    except Exception:
        return []


def _resolver_modelo_ollama(preferido: Optional[str] = None) -> str:
    """Garante que a requisição use um modelo efetivamente baixado no Ollama.
    Se o preferido não existir, faz fallback inteligente para o melhor modelo instalado."""
    instalados = _obter_modelos_instalados()
    if not instalados:
        return (preferido or OLLAMA_MODEL).strip()

    # 1. Correspondência direta ou por base (ex: 'llama3' casa com 'llama3:latest')
    if preferido:
        pref = preferido.lower().strip()
        pref_base = pref.split(":")[0]
        for inst in instalados:
            if inst == pref or inst == f"{pref}:latest" or inst.split(":")[0] == pref_base:
                return inst

    # 2. Ordem de fallback inteligente entre os instalados
    for cand in ["llama3.2:3b", "llama3.2", "phi3:mini", "phi3", "llama3:latest", "llama3:8b", "llama3"]:
        cand_base = cand.split(":")[0]
        for inst in instalados:
            if inst == cand or inst == f"{cand}:latest" or inst.split(":")[0] == cand_base:
                if preferido:
                    log.info(f"[Ollama] Modelo '{preferido}' não encontrado. Usando instalado '{inst}'.")
                return inst

    # 3. Fallback: primeiro modelo disponível no Ollama
    fallback = instalados[0]
    if preferido:
        log.info(f"[Ollama] Usando primeiro modelo disponível: '{fallback}'.")
    return fallback

# Cache LRU para traducoes identicas (evita reprocessar o mesmo trecho)
# Limitado a 128 entradas — custo de memoria minimo (~512 KB)
@functools.lru_cache(maxsize=128)
def _traducao_cacheada(texto_hash: str, modelo: str) -> tuple:
    """Wrapper de cache: chave = hash SHA1 do texto + modelo."""
    return ()  # placeholder — a logica real esta em _traduzir_sync_interno

_cache_traducao: dict = {}  # hash -> (en_corrigido, pt)
_MAX_CACHE = 128


# Lazy loading do Whisper sob demanda (inicialização ultra rápida do backend e tolerância a falhas)
_modelo_whisper = None
_whisper_lock = threading.Lock()
_whisper_carregado_nome: Optional[str] = None

def _obter_modelo_whisper(nome_modelo: Optional[str] = None):
    """Carrega o modelo Whisper apenas quando for solicitado pelo Tutor de pronúncia."""
    global _modelo_whisper, _whisper_carregado_nome
    nome_alvo = nome_modelo or os.getenv("WHISPER_MODEL", WHISPER_MODEL_NAME)
    with _whisper_lock:
        if _modelo_whisper is not None and _whisper_carregado_nome == nome_alvo:
            return _modelo_whisper

        log.info(f"[Whisper] Carregando modelo '{nome_alvo}' sob demanda...")
        try:
            _modelo_whisper = whisper.load_model(nome_alvo)
            _whisper_carregado_nome = nome_alvo
            log.info(f"[Whisper] Modelo '{nome_alvo}' carregado com sucesso.")
            return _modelo_whisper
        except Exception as e1:
            log.warning(f"[Whisper] Falha ao carregar '{nome_alvo}': {e1}. Tentando fallback 'tiny.en' ou 'base'...")
            for fallback in ["tiny.en", "base"]:
                if fallback == nome_alvo:
                    continue
                try:
                    _modelo_whisper = whisper.load_model(fallback)
                    _whisper_carregado_nome = fallback
                    log.info(f"[Whisper] Modelo fallback '{fallback}' carregado.")
                    return _modelo_whisper
                except Exception as e2:
                    log.warning(f"[Whisper] Falha no fallback '{fallback}': {e2}")
            log.error("[Whisper] Não foi possível carregar nenhum modelo do Whisper.")
            return None

# ---------------------------------------------------------------------------
# Catálogo de Vozes Neurais (Edge TTS)
# ---------------------------------------------------------------------------
VOZES_DISPONIVEIS: Dict[str, str] = {
    "en-US-JennyNeural": "Inglês (EUA) - Jenny (Feminina)",
    "en-US-GuyNeural": "Inglês (EUA) - Guy (Masculina)",
    "en-GB-SoniaNeural": "Inglês (UK) - Sonia (Feminina)",
    "en-GB-RyanNeural": "Inglês (UK) - Ryan (Masculina)",
    "en-AU-NatashaNeural": "Inglês (AU) - Natasha (Feminina)",
}

# ---------------------------------------------------------------------------
# Estado global de Gravação do Microfone
# ---------------------------------------------------------------------------
_stop_event = threading.Event()
_audio_buffer: list = []
_gravacao_thread: Optional[threading.Thread] = None
_gravando = False

# Trava para reprodução de áudio
_playback_lock = threading.Lock()

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
                bloco_audio, _ = stream.read(tamanho_bloco)
                _audio_buffer.append(bloco_audio.copy())
    except Exception as exc:
        log.error(f"[Microfone] Erro durante gravação: {exc}")
    finally:
        _gravando = False


# ---------------------------------------------------------------------------
# Otimizacao 1: Pre-processador avancado de texto e OCR para engenharia
# Unifica linhas quebradas por hifen, normaliza espacamento e corrige termos
# tecnicos sem consumo de LLM de forma instantanea (~1ms).
# ---------------------------------------------------------------------------
_CORRECOES_ENG: List[Tuple[re.Pattern, str]] = [
    # Ligaturas tipograficas comuns de escaneamento
    (re.compile(r'ﬁ'), 'fi'),
    (re.compile(r'ﬂ'), 'fl'),
    (re.compile(r'ﬀ'), 'ff'),
    (re.compile(r'ﬃ'), 'ffi'),
    (re.compile(r'ﬄ'), 'ffl'),

    # Numeros confundidos com letras em contexto numerico
    (re.compile(r'(?<=[\d\s])l(?=[\d\s.,])', re.IGNORECASE), '1'),   # l → 1 entre numeros
    (re.compile(r'(?<=[\d\s])O(?=[\d\s.,])', re.IGNORECASE), '0'),   # O → 0 entre numeros
    (re.compile(r'(?<=[\d\s])I(?=[\d\s.,])', re.IGNORECASE), '1'),   # I → 1 entre numeros

    # Siglas classicas de semicondutores e eletronica
    (re.compile(r'\b[Nn][Pp][Nn]\b'), 'npn'),
    (re.compile(r'\b[Pp][Nn][Pp]\b'), 'pnp'),
    (re.compile(r'\bBJ[lI|1]\b'), 'BJT'),
    (re.compile(r'\bMOSFE[lI|1]\b'), 'MOSFET'),
    (re.compile(r'\bJFE[lI|1]\b'), 'JFET'),
    (re.compile(r'\bCM[0O]S\b'), 'CMOS'),
    (re.compile(r'\bop[-. ]?amp\b', re.IGNORECASE), 'op-amp'),
    (re.compile(r'\bcoliector\b', re.IGNORECASE), 'collector'),
    (re.compile(r'\bemiiter\b', re.IGNORECASE), 'emitter'),

    # Parametros de polarizacao e transistores
    (re.compile(r'\bV[cC][cC]\b'), 'Vcc'),
    (re.compile(r'\bV[eE][eE]\b'), 'Vee'),
    (re.compile(r'\bV[bB][eE]\b'), 'Vbe'),
    (re.compile(r'\bV[cC][eE]\b'), 'Vce'),
    (re.compile(r'\bV[cC][bB]\b'), 'Vcb'),
    (re.compile(r'\bh[Ff][Ee]\b'), 'hFE'),

    # Unidades de medidas tecnicas
    (re.compile(r'(\d+)\s*[kK][Oo0]hm\b', re.IGNORECASE), r'\1 kΩ'),
    (re.compile(r'(\d+)\s*[mM][Oo0]hm\b', re.IGNORECASE), r'\1 MΩ'),
    (re.compile(r'(\d+)\s*[Oo0]hm\b', re.IGNORECASE), r'\1 Ω'),
    (re.compile(r'(\d+)\s*k[Oo0](?=[\s,;\.]|$)'), r'\1 kΩ'),
    (re.compile(r'(\d+)\s*M[Oo0](?=[\s,;\.]|$)'), r'\1 MΩ'),
    (re.compile(r'(\d+)\s*[µu]A\b'), r'\1 μA'),
    (re.compile(r'(\d+)\s*[µu]F\b'), r'\1 μF'),
    (re.compile(r'(\d+)\s*p[Ff]\b'), r'\1 pF'),
    (re.compile(r'(\d+)\s*n[Ff]\b'), r'\1 nF'),

    # Ruido residual de borda de escaner
    (re.compile(r'[|](?![\w])'), ' '),
]

def _pre_processar_ocr_engenharia(texto: str) -> str:
    """Aplica correcoes estruturais e tecnicas no texto extraido do escaneamento:
    1. Reconecta palavras divididas por hifen no final de linha (ex: tran-\\nsistor -> transistor)
    2. Transforma quebras de linha isoladas em espacos para restaurar oracoes continuas
    3. Normaliza siglas e unidades de engenharia eletrica
    """
    if not texto:
        return ""

    # Unir palavras quebradas por hífen no final de linha de livro acadêmico
    texto = re.sub(r'(\b[a-zA-Z]+)[-\u2010\u2013]\s*\n\s*([a-zA-Z]+\b)', r'\1\2', texto)

    # Unir quebras de linha simples dentro de paragrafos (mantem quebra dupla de paragrafo)
    texto = re.sub(r'(?<!\n)\n(?!\n)', ' ', texto)

    # Aplicar correções de vocabulário técnico de engenharia
    for padrao, substituto in _CORRECOES_ENG:
        texto = padrao.sub(substituto, texto)

    # Normalizar múltiplos espaços em branco
    texto = re.sub(r'[ \t]{2,}', ' ', texto)
    return texto.strip()

def _extrair_texto_imagem(imagem_base64: str) -> str:
    """
    Extrai texto de imagem com velocidade e fidelidade aprimoradas para livros escaneados:
    - Escala de cinza e autocontraste calibrado (elimina sombras e tons amarelados)
    - Upscaling suave Bicubic 1.8x quando a altura do recorte for < 400px (4x mais rapido que Lanczos)
    - Tesseract LSTM otimizado com flag --dpi 300 e -c tessedit_do_invert=0 (elimina overhead de deteccao de resolucao)
    - Reconstrucao sintatica com des-hifenizacao automatica
    """
    image_data = base64.b64decode(imagem_base64)
    image = Image.open(io.BytesIO(image_data))

    gray = image.convert('L')
    contraste = ImageOps.autocontrast(gray, cutoff=2)

    # Redimensionamento inteligente: bicubic e 4x mais rapido que Lanczos e ideal para LSTM
    if contraste.height < 400 or contraste.width < 800:
        fator = 1.8
        novo_tam = (int(contraste.width * fator), int(contraste.height * fator))
        img_proc = contraste.resize(novo_tam, Image.Resampling.BICUBIC)
    else:
        img_proc = contraste

    # --dpi 300 e -c tessedit_do_invert=0 removem o atraso de estimativa de resolucao do Tesseract
    config_otimizada = '--psm 6 --oem 1 --dpi 300 -c tessedit_do_invert=0'
    try:
        texto = pytesseract.image_to_string(img_proc, lang='eng', config=config_otimizada).strip()

        # Fallback se vier quase vazio (ex: tabela ou diagrama isolado)
        if len(texto) < 4 and (image.width > 50 and image.height > 25):
            texto = pytesseract.image_to_string(img_proc, lang='eng', config='--psm 4 --oem 1 --dpi 300 -c tessedit_do_invert=0').strip()
    except pytesseract.TesseractNotFoundError:
        log.warning("[OCR] Tesseract não encontrado no sistema.")
        return "[Aviso: Tesseract OCR não foi detectado no sistema. Instale o Tesseract ou verifique o instalador para OCR de imagem.]"
    except Exception as e:
        log.error(f"[OCR] Erro durante processamento com Tesseract: {e}")
        return ""

    return _pre_processar_ocr_engenharia(texto)


# ---------------------------------------------------------------------------
# Síntese e Reprodução de Voz (Edge TTS + Miniaudio + SoundDevice)
# ---------------------------------------------------------------------------
def _tocar_audio_arquivo(caminho_mp3: str):
    """Decodifica o MP3 usando miniaudio e reproduz via sounddevice."""
    with _playback_lock:
        try:
            sd.stop()
            decoded = miniaudio.mp3_read_file_f32(caminho_mp3)
            samples = np.frombuffer(decoded.samples, dtype=np.float32)
            if decoded.nchannels > 1:
                samples = samples.reshape(-1, decoded.nchannels)
            sd.play(samples, samplerate=decoded.sample_rate)
            sd.wait()
        except Exception as e:
            log.error(f"[TTS Playback] Erro na reprodução: {e}")
        finally:
            if os.path.exists(caminho_mp3):
                try:
                    os.unlink(caminho_mp3)
                except Exception:
                    pass


async def _sintetizar_e_tocar_voz_async(texto: str, voz: str = "en-US-JennyNeural", velocidade: str = "+0%") -> bool:
    if voz not in VOZES_DISPONIVEIS:
        voz = "en-US-JennyNeural"

    with tempfile.NamedTemporaryFile(suffix=".mp3", delete=False) as tmp:
        temp_path = tmp.name

    try:
        comm = edge_tts.Communicate(texto, voz, rate=velocidade)
        await comm.save(temp_path)
        threading.Thread(target=_tocar_audio_arquivo, args=(temp_path,), daemon=True).start()
        return True
    except Exception as e:
        log.error(f"[TTS] Erro ao sintetizar áudio: {e}")
        if os.path.exists(temp_path):
            try:
                os.unlink(temp_path)
            except Exception:
                pass
        return False


# ---------------------------------------------------------------------------
# Otimizacao 2: Chamada unica Ollama — traduz + retorna ingles corrigido
# Otimizacao 3: Roteamento pela API Gemini quando chave disponivel
# ---------------------------------------------------------------------------
async def _traduzir_via_gemini_async(texto_ingles: str, api_key: str, modelo_preferido: Optional[str] = None) -> Tuple[str, str]:
    """Traduz via Gemini API (cloud) com latência ultrabaixa e failover instantâneo.
    Prioriza modelos com altíssimo throughput (gemini-3.5-flash-lite) para eliminar erros de alta demanda."""
    modelos_candidatos = []
    if modelo_preferido and modelo_preferido.startswith("gemini-"):
        modelos_candidatos.append(modelo_preferido)
    padrao = os.environ.get("GEMINI_TRANSLATE_MODEL", "gemini-3.5-flash-lite")
    if padrao not in modelos_candidatos:
        modelos_candidatos.append(padrao)
    for reserva in ["gemini-3.5-flash-lite", "gemini-3.5-flash", "gemini-flash-lite-latest", "gemini-3.8-flash"]:
        if reserva not in modelos_candidatos:
            modelos_candidatos.append(reserva)

    prompt = (
        "You are a senior technical translator specialized in Electrical Engineering, Electronics and Telecommunications.\n"
        "The text below comes from a scanned academic textbook and may have minor OCR errors.\n"
        "Your task:\n"
        "1. Fix any OCR errors in the original English text (preserve technical terms like npn, pnp, BJT, MOSFET, Vcc, hFE).\n"
        "2. Translate the corrected text to Brazilian Portuguese with maximum technical fidelity.\n"
        "Respond ONLY with valid JSON in this exact format (no markdown, no extra text):\n"
        '{"en_corrigido": "<corrected english>", "pt": "<portuguese translation>"}\n\n'
        f"Text:\n{texto_ingles}"
    )
    payload = {
        "contents": [{"parts": [{"text": prompt}]}],
        "generationConfig": {
            "temperature": 0.1,
            "maxOutputTokens": max(512, len(texto_ingles) * 3)
        }
    }

    client = _obter_http_client()
    ultimo_erro = None

    for mod in modelos_candidatos:
        url = f"https://generativelanguage.googleapis.com/v1beta/models/{mod}:generateContent?key={api_key}"
        # Timeout curto por tentativa (7s) para não prender o usuário se o cluster estiver congestionado
        try:
            resp = await client.post(url, json=payload, timeout=7.0)
            if resp.status_code == 200:
                cand = resp.json().get("candidates", [{}])[0]
                parts = cand.get("content", {}).get("parts", [])
                raw = "\n".join(p.get("text", "") for p in parts if "text" in p).strip()
                raw = re.sub(r'^```[a-z]*\n?', '', raw).rstrip('`').strip()
                import json as _json
                data = _json.loads(raw)
                return data.get("en_corrigido", texto_ingles), data.get("pt", "")
            elif resp.status_code in (503, 429):
                ultimo_erro = f"Gemini {mod} em alta demanda ({resp.status_code})"
                log.warning(f"[Gemini Traducao] {mod} com alta demanda ({resp.status_code}). Alternando imediatamente...")
                continue
            else:
                ultimo_erro = f"Gemini {mod} {resp.status_code}: {resp.text[:150]}"
                log.warning(f"[Gemini Traducao] {mod} retornou {resp.status_code}: {resp.text[:100]}")
                continue
        except Exception as exc:
            ultimo_erro = str(exc)
            log.warning(f"[Gemini Traducao] {mod} falhou/timeout ({exc}). Tentando próximo modelo...")
            continue

    log.warning(f"[Gemini Traducao] Falha em todos os modelos ({ultimo_erro}), fazendo fallback para Ollama")
    raise RuntimeError(ultimo_erro or "Falha na API Gemini")


def _limpar_traducao_rambling(texto_pt: str) -> str:
    """Remove comentários, repetições e invenções de modelos menores em frases curtas."""
    if not texto_pt:
        return ""
    t = texto_pt.strip()
    if "Tradução:" in t or "Tradução técnica:" in t:
        partes = re.split(r"\n+|(?:,\s*Tradução)", t)
        for p in partes:
            p_limpo = re.sub(r"^(?:Tradução.*?:\s*)?", "", p.strip(), flags=re.IGNORECASE).strip()
            if p_limpo and not p_limpo.lower().startswith("tradução"):
                t = p_limpo
                break
    linhas = [l.strip() for l in t.splitlines() if l.strip()]
    if linhas:
        t = linhas[0]
    t = re.sub(r"^(?:Tradução(?:\s+técnica)?(?:\s*\(.*?\))?|Translation|PT-BR|Português)\s*:\s*", "", t, flags=re.IGNORECASE)
    return t.strip(" \"'\n\r")


def _traduzir_via_ollama_local(texto_ingles: str, modelo_ollama: str) -> tuple:
    """Traduz via Ollama com instruções estritas, sem alucinações ou invenções para textos curtos."""
    log.info(f"[Traducao] Processando {len(texto_ingles)} chars com Ollama ({modelo_ollama})...")

    n_chars = len(texto_ingles)
    n_palavras = len(texto_ingles.split())

    if n_palavras <= 6:
        num_predict = min(90, max(50, n_palavras * 10))
    elif n_palavras <= 25:
        num_predict = min(200, max(100, n_palavras * 6))
    else:
        num_predict = min(1024, max(256, int(n_chars * 2.2)))

    system_prompt = (
        "Você é um tradutor técnico sênior e estrito especializado em Engenharia, Ciência da Computação e Exatas.\n"
        "REGRAS OBRIGATÓRIAS:\n"
        "1. Traduza EXATAMENTE o texto fornecido pelo usuário para o Português do Brasil com máximo rigor técnico.\n"
        "2. NUNCA invente continuações, explicações, contexto adicional, notas ou variações de tradução.\n"
        "3. Se o texto for uma palavra isolada, sigla ou frase curta, retorne APENAS a tradução direta exata e PARE imediatamente.\n"
        "4. Preserve termos padrão e siglas de engenharia.\n"
        "5. Responda ESTRITAMENTE em formato JSON: {\"en_corrigido\": \"<ingles corrigido>\", \"pt\": \"<traducao em portugues>\"}"
    )

    messages = [
        {"role": "system", "content": system_prompt},
        {"role": "user", "content": f'Texto a traduzir:\n"{texto_ingles}"'}
    ]

    try:
        resposta = ollama.chat(
            model=modelo_ollama,
            messages=messages,
            format="json",
            options={
                "temperature": 0.0,
                "num_predict": num_predict,
                "stop": ["\n\n\n", "Tradução técnica:", "Tradução:", "Nota:", "Explicação:"]
            },
        )
        import json as _json
        raw = resposta["message"]["content"].strip()
        raw = re.sub(r"^```[a-z]*\n?", "", raw).rstrip("`").strip()

        try:
            data = _json.loads(raw)
        except Exception:
            m_en = re.search(r'"en_corrigido"\s*:\s*"([^"\\]*(?:\\.[^"\\]*)*)"', raw)
            m_pt = re.search(r'"pt"\s*:\s*"([^"\\]*(?:\\.[^"\\]*)*)"', raw)
            data = {
                "en_corrigido": m_en.group(1) if m_en else texto_ingles,
                "pt": m_pt.group(1) if m_pt else ""
            }

        en_corrigido = data.get("en_corrigido", texto_ingles)
        pt = _limpar_traducao_rambling(data.get("pt", ""))
        if not pt:
            raise ValueError("Campo 'pt' vazio no JSON")

        log.info(f"[Traducao] Concluida via Ollama ({modelo_ollama}).")
        return (en_corrigido, pt, f"{modelo_ollama} (Local)")

    except Exception as exc:
        log.warning(f"[Traducao] JSON falhou ({exc}), usando prompt simples e estrito de fallback.")
        system_fallback = (
            "Você é um tradutor técnico estrito. Traduza o texto do usuário para Português do Brasil. "
            "Retorne SOMENTE a tradução direta exata, sem introdução, sem explicações, sem alternativas e sem inventar texto."
        )
        messages_fallback = [
            {"role": "system", "content": system_fallback},
            {"role": "user", "content": f"Traduza diretamente:\n{texto_ingles}"}
        ]
        resposta = ollama.chat(
            model=modelo_ollama,
            messages=messages_fallback,
            options={
                "temperature": 0.0,
                "num_predict": min(80, max(25, n_palavras * 4)),
                "stop": ["\n", "\n\n", "Tradução:", "Tradução técnica:", "Original:", "Inglês:", "Nota:"]
            },
        )
        pt_limpo = _limpar_traducao_rambling(resposta["message"]["content"].strip())
        return (texto_ingles, pt_limpo, f"{modelo_ollama} (Local)")


def _traduzir_sync(texto_ingles: Optional[str] = None, imagem_base64: Optional[str] = None,
                  gemini_api_key: Optional[str] = None, modelo_especifico: Optional[str] = None) -> tuple:
    """Traducao tecnica EN→PT com correcao de OCR embutida.

    Fluxo:
      1. OCR (Tesseract) se vier imagem com pre-processamento otimizado
      2. Pre-processador de regras instantaneo (de-hifenizacao e siglas de eng. eletrica)
      3. Cache LRU: se o mesmo texto ja foi traduzido, retorna instantaneamente
      4. Roteamento transparente:
         - Se gemini_api_key presente → API Gemini na nuvem
         - Senao → Ollama local com o modelo EXATO solicitado (phi3:mini, llama3.2:3b, llama3)
    Retorna: (texto_en_corrigido, texto_pt, modelo_usado)
    """
    if imagem_base64:
        log.info("[Traducao] Extraindo texto da imagem via Tesseract otimizado...")
        try:
            texto_ingles = _extrair_texto_imagem(imagem_base64)
            log.info(f"[Traducao] OCR extraiu {len(texto_ingles)} caracteres.")
            if not texto_ingles:
                return ("", "(Nenhum texto detectado nesta area da pagina)", "Tesseract OCR")
        except Exception as e:
            log.error(f"[Traducao] Erro no OCR: {e}")
            raise RuntimeError(f"Erro ao extrair imagem: {e}")

    if not texto_ingles:
        return ("", "", "")

    # Pre-processador de regras (instantaneo, sem LLM)
    texto_ingles = _pre_processar_ocr_engenharia(texto_ingles)

    # Identifica o modelo exato que executara (com fallback inteligente se nao estiver baixado)
    modelo_ollama = _resolver_modelo_ollama(modelo_especifico)
    motor_id = "gemini" if gemini_api_key else modelo_ollama

    # Cache LRU: evita rechamar Ollama/Gemini para o mesmo texto
    cache_key = hashlib.sha1(f"{motor_id}:{texto_ingles}".encode()).hexdigest()
    if cache_key in _cache_traducao:
        log.info(f"[Traducao] Cache hit ({motor_id}) — retornando resultado anterior.")
        return _cache_traducao[cache_key]

    # Gemini disponivel → nuvem, zero CPU/GPU local
    if gemini_api_key:
        try:
            loop = asyncio.new_event_loop()
            en_corrigido, pt = loop.run_until_complete(
                _traduzir_via_gemini_async(texto_ingles, gemini_api_key, modelo_especifico)
            )
            loop.close()
            log.info("[Traducao] Concluida via Gemini (nuvem).")
            resultado = (en_corrigido, pt, "Gemini (Nuvem)")
            _guardar_cache(cache_key, resultado)
            return resultado
        except Exception:
            pass  # fallback para Ollama local abaixo

    # Ollama local: processamento com modelo especificado
    resultado = _traduzir_via_ollama_local(texto_ingles, modelo_ollama)
    _guardar_cache(cache_key, resultado)
    return resultado


def _guardar_cache(key: str, valor: tuple) -> None:
    """Guarda no cache LRU; descarta o mais antigo se ultrapassar _MAX_CACHE."""
    if len(_cache_traducao) >= _MAX_CACHE:
        _cache_traducao.pop(next(iter(_cache_traducao)))
    _cache_traducao[key] = valor



def _limpar_ocr_sync(texto_sujo: Optional[str] = None, imagem_base64: Optional[str] = None,
                    gemini_api_key: Optional[str] = None, modelo_especifico: Optional[str] = None) -> str:
    """Retorna ingles corrigido reutilizando o prompt duplo de _traduzir_sync.
    Nao faz chamada extra ao Ollama — aproveita o en_corrigido do prompt de traducao.
    """
    if imagem_base64:
        texto_sujo = _extrair_texto_imagem(imagem_base64)
    if not texto_sujo:
        return ""
    texto_sujo = _pre_processar_ocr_engenharia(texto_sujo)
    try:
        en_corrigido, _, _ = _traduzir_sync(texto_sujo, gemini_api_key=gemini_api_key, modelo_especifico=modelo_especifico)
        return en_corrigido if en_corrigido else texto_sujo
    except Exception:
        return texto_sujo



def _avaliar_pronuncia_sync(texto_esperado: str, texto_falado: str, nivel: str = "intermediario",
                            modelo: Optional[str] = None, api_key: Optional[str] = None) -> dict:
    """
    Compara o texto esperado do documento com a transcrição do Whisper.
    Calcula acurácia objetiva e gera feedback de pronúncia via LLM considerando o nível de exigência
    e o modelo configurado no perfil ativo (Gemini na nuvem ou modelo local do Ollama).
    """
    palavras_esperadas = [w.lower().strip(".,!?;:\"'()[]") for w in texto_esperado.split() if w]
    palavras_faladas   = [w.lower().strip(".,!?;:\"'()[]") for w in texto_falado.split() if w]

    matcher = difflib.SequenceMatcher(None, palavras_esperadas, palavras_faladas)
    ratio = matcher.ratio()

    # Ajuste de rigor por nível
    if nivel == "iniciante":
        # Tolerância generosa para iniciantes
        similaridade = min(100, int(ratio * 125))
        cobranca_prompt = "Nível do aluno: INICIANTE. Seja muito encorajador e acolhedor, parabenize os acertos e releve pequenos desvios de sotaque."
    elif nivel == "avancado":
        # Rigor alto: exige correspondência precisa
        similaridade = int(ratio * 90)
        cobranca_prompt = "Nível do aluno: AVANÇADO / EXIGENTE. Seja rigoroso, pontue com precisão trocas de fonemas, omissões de terminações verbais e ritmo."
    else:
        similaridade = int(ratio * 100)
        cobranca_prompt = "Nível do aluno: INTERMEDIÁRIO. Seja equilibrado, aponte palavras com pronúncia truncada e elogie a fluidez."

    set_esperadas = set(palavras_esperadas)
    set_faladas = set(palavras_faladas)
    ausentes = list(set_esperadas - set_faladas)[:6]

    prompt = f"""Você é um tutor de pronúncia em inglês para estudantes universitários de Engenharia.
{cobranca_prompt}

Texto original esperado do livro: "{texto_esperado}"
O que o reconhecimento de voz captou da fala do aluno: "{texto_falado}"

Forneça um feedback curto (2 a 3 frases) em português:
1. Diga claramente como foi a clareza geral e o que o aluno acertou.
2. Dê uma dica fonética sobre as palavras mais difíceis ou que saíram distorcidas.
Retorne APENAS o feedback, sem notas numéricas no texto.
"""
    feedback = ""
    # Se tiver chave do Gemini informada (perfil Nuvem)
    if api_key:
        for mod in ["gemini-3.5-flash-lite", "gemini-3.5-flash"]:
            try:
                url = f"https://generativelanguage.googleapis.com/v1beta/models/{mod}:generateContent?key={api_key}"
                payload = {
                    "contents": [{"parts": [{"text": prompt}]}],
                    "generationConfig": {"temperature": 0.2, "maxOutputTokens": 200}
                }
                with httpx.Client(timeout=8.0) as client:
                    resp = client.post(url, json=payload)
                    if resp.status_code == 200:
                        parts = resp.json().get("candidates", [{}])[0].get("content", {}).get("parts", [])
                        feedback = "\n".join(p.get("text", "") for p in parts if "text" in p).strip()
                        if feedback:
                            log.info(f"[Tutor Pronúncia] Feedback gerado via Gemini ({mod}).")
                            break
                    elif resp.status_code in (503, 429):
                        log.warning(f"[Tutor Pronúncia] {mod} com alta demanda ({resp.status_code}), tentando modelo reserva...")
                        continue
            except Exception as e:
                log.warning(f"[Tutor Pronúncia] Falha no Gemini {mod} ({e}), tentando próximo...")

    # Se não usou Gemini ou falhou, usa o modelo local do Ollama correspondente ao perfil
    if not feedback:
        modelo_ollama = _resolver_modelo_ollama(modelo)
        try:
            resp = ollama.generate(
                model=modelo_ollama,
                prompt=prompt.strip(),
                options={"temperature": 0.2, "num_predict": 180},
            )
            feedback = resp["response"].strip()
            log.info(f"[Tutor Pronúncia] Feedback gerado via Ollama ({modelo_ollama}).")
        except Exception as e:
            log.error(f"[Tutor Pronúncia] Erro no Ollama ({modelo_ollama}): {e}")
            feedback = f"Boa tentativa! Acurácia de correspondência de {similaridade}%."

    return {
        "nota": similaridade,
        "feedback": feedback,
        "texto_esperado": texto_esperado,
        "texto_falado": texto_falado,
        "palavras_ausentes": ausentes,
        "palavras_faladas": palavras_faladas,
    }


# ---------------------------------------------------------------------------
# Aplicação FastAPI
# ---------------------------------------------------------------------------
app = FastAPI(
    title="Motor de IA & Tutor de Inglês",
    description="Microserviço local para Tradução, OCR e Tutor de Pronúncia",
    version="2.0.0",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# ---------------------------------------------------------------------------
# Schemas (Pydantic)
# ---------------------------------------------------------------------------
class TextoSujoRequest(BaseModel):
    texto_sujo: Optional[str] = None
    imagem_base64: Optional[str] = None
    api_key: Optional[str] = None  # Gemini key para roteamento em nuvem
    modelo: Optional[str] = None

class TextoResponse(BaseModel):
    resultado: str

class TraduzirRequest(BaseModel):
    texto_ingles: Optional[str] = None
    imagem_base64: Optional[str] = None
    api_key: Optional[str] = None  # Gemini key para roteamento em nuvem
    modelo: Optional[str] = None   # Modelo específico Ollama (ex: "phi3:mini", "llama3.2:3b", "llama3")

class TraducaoResponse(BaseModel):
    texto_ingles: str
    traducao_portugues: str
    modelo_usado: Optional[str] = None

class TranscricaoResponse(BaseModel):
    texto_transcrito: str

class FalarRequest(BaseModel):
    texto: str
    voz: Optional[str] = "en-US-JennyNeural"
    velocidade: Optional[str] = "+0%"

class FalarResponse(BaseModel):
    sucesso: bool
    mensagem: str
    voz: str

class AvaliarPronunciaRequest(BaseModel):
    texto_esperado: str
    texto_falado: str
    nivel: Optional[str] = "intermediario"
    modelo: Optional[str] = None
    api_key: Optional[str] = None

class AvaliarPronunciaResponse(BaseModel):
    nota: int
    feedback: str
    texto_esperado: str
    texto_falado: str
    palavras_ausentes: List[str]
    palavras_faladas: List[str]

class ChatMensagem(BaseModel):
    role: str  # "user" ou "assistant"
    content: str

class ChatIARequest(BaseModel):
    mensagem: str = ""
    imagem_base64: Optional[str] = None
    historico: List[ChatMensagem] = []
    provedor: Optional[str] = "gemini"  # "gemini" | "ollama"
    api_key: Optional[str] = None
    modelo: Optional[str] = None

class ChatIAResponse(BaseModel):
    resposta: str
    provedor_usado: str
    modelo_usado: str



# ---------------------------------------------------------------------------
# Endpoints
# ---------------------------------------------------------------------------
@app.get("/", tags=["Health"])
async def health_check():
    return {
        "status": "online",
        "whisper_model": WHISPER_MODEL_NAME,
        "ollama_model": OLLAMA_MODEL,
        "vozes_disponiveis": len(VOZES_DISPONIVEIS),
    }


@app.get("/vozes", tags=["TTS"])
async def listar_vozes():
    """Retorna as vozes neurais disponíveis para o tutor."""
    return [{"id": k, "nome": v} for k, v in VOZES_DISPONIVEIS.items()]


@app.get("/modelos_status", tags=["Health"])
async def modelos_status():
    """Retorna o status do Ollama local e disponibilidade dos modelos de cada tier."""
    instalados = _obter_modelos_instalados()
    ollama_ok = len(instalados) > 0
    if not ollama_ok:
        try:
            ollama.list()
            ollama_ok = True
        except Exception:
            ollama_ok = False

    tem_phi3 = any("phi3" in inst for inst in instalados)
    tem_llama32 = any("llama3.2" in inst for inst in instalados)
    tem_llama3 = any(inst.startswith("llama3:") or inst == "llama3" for inst in instalados if not inst.startswith("llama3.2"))

    return {
        "ollama_online": ollama_ok,
        "modelos_instalados": instalados,
        "tem_phi3": tem_phi3,
        "tem_llama32": tem_llama32,
        "tem_llama3": tem_llama3,
    }


@app.post("/falar", response_model=FalarResponse, tags=["TTS"])
async def falar_texto(body: FalarRequest):
    """
    Sintetiza o texto em inglês com voz neural e reproduz no dispositivo de áudio local.
    """
    if not body.texto.strip():
        raise HTTPException(status_code=422, detail="O campo 'texto' não pode ser vazio.")

    voz = body.voz if body.voz in VOZES_DISPONIVEIS else "en-US-JennyNeural"
    sucesso = await _sintetizar_e_tocar_voz_async(body.texto, voz=voz, velocidade=body.velocidade or "+0%")
    if not sucesso:
        raise HTTPException(status_code=500, detail="Erro ao gerar ou reproduzir áudio da fala.")

    return FalarResponse(sucesso=True, mensagem="Reprodução iniciada", voz=voz)


@app.post("/parar_audio", tags=["TTS"])
async def parar_audio():
    """Interrompe qualquer reprodução de áudio em andamento."""
    sd.stop()
    return {"status": "parado"}


@app.post("/traduzir", response_model=TraducaoResponse, tags=["Traducao"])
async def traduzir(body: TraduzirRequest):
    """Traducao tecnica EN→PT com correcao de OCR embutida.
    Usa Gemini (nuvem, leve) se api_key disponivel, senao Ollama (local) com modelo especifico."""
    if not body.texto_ingles and not body.imagem_base64:
        raise HTTPException(status_code=422, detail="Envie 'texto_ingles' ou 'imagem_base64'.")

    gemini_key = (body.api_key or os.environ.get("GEMINI_API_KEY", "")).strip() or None
    try:
        texto_en, traducao_pt, modelo_usado = await asyncio.to_thread(
            _traduzir_sync, body.texto_ingles, body.imagem_base64, gemini_key, body.modelo
        )
    except Exception as exc:
        log.error(f"[/traduzir] Erro: {exc}")
        raise HTTPException(status_code=500, detail=f"Erro na traducao: {exc}")

    return TraducaoResponse(texto_ingles=texto_en, traducao_portugues=traducao_pt, modelo_usado=modelo_usado)


@app.post("/limpar_ocr", response_model=TextoResponse, tags=["OCR"])
async def limpar_ocr(body: TextoSujoRequest):
    """Recebe texto com OCR e retorna texto limpo em ingles.
    Aplica pre-processador de regras instantaneo + prompt duplo Ollama (ou Gemini)."""
    if not body.texto_sujo and not body.imagem_base64:
        raise HTTPException(status_code=422, detail="Envie 'texto_sujo' ou 'imagem_base64'.")

    gemini_key = (body.api_key or os.environ.get("GEMINI_API_KEY", "")).strip() or None
    try:
        resultado = await asyncio.to_thread(
            _limpar_ocr_sync, body.texto_sujo, body.imagem_base64, gemini_key, body.modelo
        )
    except Exception as exc:
        raise HTTPException(status_code=500, detail=f"Erro no OCR/IA: {exc}")

    return TextoResponse(resultado=resultado)


@app.post("/iniciar_gravacao", tags=["Áudio"])
async def iniciar_gravacao():
    """Inicia gravação contínua do microfone em background."""
    global _gravacao_thread, _gravando

    if _gravando:
        raise HTTPException(status_code=409, detail="Já há uma gravação em andamento.")

    try:
        sd.check_input_settings()
    except Exception as exc:
        log.warning(f"[Microfone] Nenhum dispositivo de gravação disponível: {exc}")
        raise HTTPException(status_code=503, detail="Nenhum dispositivo de microfone disponível ou permissão de áudio negada.")

    _stop_event.clear()
    _gravando = True
    _gravacao_thread = threading.Thread(target=_gravar_background, daemon=True)
    _gravacao_thread.start()
    log.info("[Microfone] Gravação iniciada.")
    return {"status": "gravando"}


@app.post("/parar_gravacao", response_model=TranscricaoResponse, tags=["Áudio"])
async def parar_gravacao(idioma: str = "en"):
    """
    Para a gravação e transcreve o áudio com o Whisper.
    Padrão para treino de pronúncia: idioma='en'.
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
    log.info(f"[Whisper] Transcrevendo {len(audio_array)/16000:.1f}s de áudio em inglês...")

    def _transcrever():
        mod = _obter_modelo_whisper()
        if mod is None:
            raise RuntimeError("Módulo de reconhecimento de voz (Whisper) indisponível.")
        return mod.transcribe(audio_array, language=idioma)["text"].strip()

    try:
        texto = await asyncio.to_thread(_transcrever)
    except Exception as exc:
        log.error(f"[Whisper] Erro ao transcrever áudio: {exc}")
        raise HTTPException(status_code=503, detail=f"Erro no Whisper: {exc}")
    log.info(f"[Whisper] Transcrição: '{texto}'")

    if not texto:
        raise HTTPException(status_code=204, detail="Nenhuma fala detectada.")

    return TranscricaoResponse(texto_transcrito=texto)


@app.post("/avaliar_pronuncia", response_model=AvaliarPronunciaResponse, tags=["Tutor"])
async def avaliar_pronuncia(body: AvaliarPronunciaRequest):
    """
    Avalia a pronúncia do aluno comparando o texto esperado com o falado.
    Retorna pontuação de similaridade e feedback fonético construtivo do tutor.
    """
    if not body.texto_esperado.strip() or not body.texto_falado.strip():
        raise HTTPException(status_code=422, detail="Ambos os campos são obrigatórios.")

    try:
        resultado = await asyncio.to_thread(
            _avaliar_pronuncia_sync, 
            body.texto_esperado, 
            body.texto_falado, 
            body.nivel or "intermediario",
            body.modelo,
            body.api_key
        )
    except Exception as exc:
        log.error(f"[/avaliar_pronuncia] Erro: {exc}")
        raise HTTPException(status_code=500, detail=f"Erro na avaliação de pronúncia: {exc}")

    return AvaliarPronunciaResponse(**resultado)


# ---------------------------------------------------------------------------
# Assistente Técnico / Chat IA Multimodal (Gemini & Ollama)
# ---------------------------------------------------------------------------
async def _chamar_gemini_chat_async(mensagem: str, imagem_base64: Optional[str], historico: List[ChatMensagem], api_key: str, modelo: Optional[str]) -> str:
    mod = (modelo or "").lower().strip()
    candidatos = []
    if mod and mod.startswith("gemini-"):
        candidatos.append(mod)
    env_mod = os.environ.get("GEMINI_CHAT_MODEL", "gemini-3.5-flash-lite")
    if env_mod not in candidatos:
        candidatos.append(env_mod)
    for reserva in ["gemini-3.5-flash-lite", "gemini-3.5-flash", "gemini-flash-lite-latest", "gemini-3.8-flash"]:
        if reserva not in candidatos:
            candidatos.append(reserva)
    
    system_instruction = (
        "Você é um engenheiro sênior e tutor especialista em Engenharia Elétrica, Eletrônica, Telecomunicações e Computação. "
        "Auxilie o estudante a entender livros e papers em inglês, tirando dúvidas conceituais, explicando esquemáticos e circuitos "
        "(como NPN, PNP, MOSFETs, polarização DC, pequenas sinais, Leis de Kirchhoff, Thévenin, Miller, filtros, amplificadores operacionais). "
        "Quando uma imagem de circuito for enviada, analise detalhadamente a topologia, componentes e equações de malha/nó. "
        "Responda sempre em Português do Brasil com máxima clareza técnica, objetividade, equações bem estruturadas e formatação didática."
    )
    
    contents = []
    for h in historico:
        r = "user" if h.role == "user" else "model"
        contents.append({"role": r, "parts": [{"text": h.content}]})
        
    current_parts = []
    if imagem_base64:
        raw_b64 = imagem_base64
        if "base64," in raw_b64:
            raw_b64 = raw_b64.split("base64,")[-1]
        elif raw_b64.startswith("BASE64:"):
            raw_b64 = raw_b64[7:]
        raw_b64 = raw_b64.strip()
        current_parts.append({
            "inlineData": {
                "mimeType": "image/png",
                "data": raw_b64
            }
        })
    texto_usuario = mensagem.strip() if mensagem else "Por favor, analise o circuito/imagem anexada e explique seus principais aspectos técnicos."
    current_parts.append({"text": texto_usuario})
    contents.append({"role": "user", "parts": current_parts})
    
    payload = {
        "contents": contents,
        "systemInstruction": {
            "parts": [{"text": system_instruction}]
        },
        "generationConfig": {
            "temperature": 0.3,
            "maxOutputTokens": 2048
        }
    }

    client = _obter_http_client()
    ultimo_erro = ""

    for modelo_escolhido in candidatos:
        url = f"https://generativelanguage.googleapis.com/v1beta/models/{modelo_escolhido}:generateContent?key={api_key}"
        try:
            resp = await client.post(url, json=payload, timeout=25.0)
            if resp.status_code == 200:
                data = resp.json()
                cand = data.get("candidates", [{}])[0]
                parts = cand.get("content", {}).get("parts", [])
                resposta_texto = "\n".join(p.get("text", "") for p in parts if "text" in p).strip()
                if resposta_texto:
                    return resposta_texto
            elif resp.status_code in (503, 429):
                log.warning(f"[Gemini Chat] {modelo_escolhido} em alta demanda ({resp.status_code}). Alternando...")
                ultimo_erro = f"{modelo_escolhido} em alta demanda"
                continue
            else:
                err_text = resp.text
                log.error(f"[Gemini API] Erro {resp.status_code} em {modelo_escolhido}: {err_text}")
                try:
                    err_json = resp.json()
                    msg_google = err_json.get("error", {}).get("message", err_text)
                    status_google = err_json.get("error", {}).get("status", "")
                    if "API_KEY_INVALID" in msg_google or "API key not valid" in msg_google:
                        return "⚠️ **Chave de API do Gemini inválida.**\n\nPor favor, verifique se copiou a chave corretamente no [Google AI Studio](https://aistudio.google.com/app/apikey) e configure-a novamente no botão **⚙️ Chaves/API**."
                    elif "NOT_FOUND" in status_google or "not found" in msg_google.lower() or "no longer available" in msg_google.lower():
                        continue
                except Exception:
                    pass
                ultimo_erro = err_text
        except httpx.ConnectError:
            return "⚠️ **Erro de conexão com a Internet.**\n\nNão foi possível alcançar os servidores do Google Gemini. Verifique sua conexão de rede."
        except Exception as exc:
            log.warning(f"[Gemini Chat] Exceção em {modelo_escolhido}: {exc}. Tentando próximo modelo...")
            ultimo_erro = str(exc)

    return f"⚠️ **Não foi possível obter resposta do Gemini ({ultimo_erro}).**\n\nRecomendamos usar o modelo estável **gemini-3.5-flash-lite** em Configurar API."


async def _chamar_ollama_chat_async(mensagem: str, imagem_base64: Optional[str], historico: List[ChatMensagem], modelo: Optional[str]) -> str:
    modelo_preferido = modelo or ("llama3.2-vision" if imagem_base64 else None)
    modelo_escolhido = _resolver_modelo_ollama(modelo_preferido)
    url = "http://localhost:11434/api/chat"
    
    system_instruction = (
        "Você é um engenheiro sênior e tutor especialista em Engenharia Elétrica, Eletrônica e Computação. "
        "Auxilie o estudante a entender livros em inglês, explicando conceitos, circuitos e equações. "
        "Responda em Português do Brasil com clareza e precisão técnica."
    )
    
    messages = [{"role": "system", "content": system_instruction}]
    for h in historico:
        messages.append({"role": h.role, "content": h.content})
        
    texto_usuario = mensagem.strip() if mensagem else "Analise o circuito anexado."
    curr_msg = {"role": "user", "content": texto_usuario}
    if imagem_base64:
        raw_b64 = imagem_base64
        if "base64," in raw_b64:
            raw_b64 = raw_b64.split("base64,")[-1]
        elif raw_b64.startswith("BASE64:"):
            raw_b64 = raw_b64[7:]
        curr_msg["images"] = [raw_b64.strip()]
        
    messages.append(curr_msg)
    
    payload = {
        "model": modelo_escolhido,
        "messages": messages,
        "stream": False
    }
    
    try:
        async with httpx.AsyncClient(timeout=90.0) as client:
            resp = await client.post(url, json=payload)
            if resp.status_code != 200:
                return f"⚠️ **Erro no Ollama ({resp.status_code}):** {resp.text}"
            data = resp.json()
            return data.get("message", {}).get("content", "").strip()
    except Exception as exc:
        log.error(f"[Ollama] Erro de conexão: {exc}")
        return (
            "⚠️ **Não foi possível conectar ao servidor Ollama local (localhost:11434).**\n\n"
            "Verifique se o Ollama está rodando no terminal (`ollama serve` ou `ollama run llama3`), "
            "ou configure uma chave gratuita do Google Gemini no botão **⚙️ Chaves/API** acima."
        )


@app.post("/chat_ia", response_model=ChatIAResponse, tags=["Chat"])
async def chat_ia(body: ChatIARequest):
    """
    Assistente técnico com IA multimodal (Gemini ou Ollama) para tirar dúvidas
    sobre excertos do livro, circuitos e diagramas esquemáticos com imagens.
    """
    if not body.mensagem.strip() and not body.imagem_base64:
        raise HTTPException(status_code=422, detail="Envie uma mensagem ou uma imagem de circuito.")

    provedor = (body.provedor or "gemini").lower().strip()
    api_key = (body.api_key or os.environ.get("GEMINI_API_KEY", "")).strip()

    if provedor == "gemini":
        if not api_key:
            # Tenta fallback para Ollama se disponível
            try:
                resposta = await _chamar_ollama_chat_async(body.mensagem, body.imagem_base64, body.historico, body.modelo)
                if "Não foi possível conectar ao servidor Ollama" not in resposta:
                    return ChatIAResponse(resposta=resposta, provedor_usado="ollama (fallback)", modelo_usado=body.modelo or "llama3")
            except Exception:
                pass

            return ChatIAResponse(
                resposta=(
                    "⚠️ **Chave de API do Google Gemini não configurada.**\n\n"
                    "Para conversar com o Assistente IA e analisar circuitos com visão computacional:\n"
                    "1. Clique no botão **⚙️ Chaves/API** no topo da aba (ou no menu *Configurações* > *Chaves de API e IA*).\n"
                    "2. Cole sua chave gratuita obtida no [Google AI Studio](https://aistudio.google.com/app/apikey).\n"
                    "3. Clique em **Salvar Configurações**.\n\n"
                    "*(Se preferir usar IA 100% offline, selecione o provedor **Ollama** após iniciar `ollama run llama3` no terminal)*."
                ),
                provedor_usado="aviso",
                modelo_usado="nenhum"
            )

        resposta = await _chamar_gemini_chat_async(body.mensagem, body.imagem_base64, body.historico, api_key, body.modelo)
        return ChatIAResponse(resposta=resposta, provedor_usado="gemini", modelo_usado=body.modelo or "gemini-1.5-flash")

    else:
        resposta = await _chamar_ollama_chat_async(body.mensagem, body.imagem_base64, body.historico, body.modelo)
        return ChatIAResponse(resposta=resposta, provedor_usado="ollama", modelo_usado=body.modelo or "llama3")


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="127.0.0.1", port=8000, log_level="info")


