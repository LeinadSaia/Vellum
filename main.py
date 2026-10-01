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
import tempfile
import time
import shutil
import difflib
from typing import Optional, List, Dict

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
OLLAMA_MODEL = os.getenv("OLLAMA_MODEL", "llama3")
WHISPER_MODEL_NAME = os.getenv("WHISPER_MODEL", "base.en")

log.info(f"Carregando modelo Whisper '{WHISPER_MODEL_NAME}'... (otimizado para leitura em inglês)")
try:
    _modelo_whisper = whisper.load_model(WHISPER_MODEL_NAME)
    log.info(f"Modelo Whisper '{WHISPER_MODEL_NAME}' carregado com sucesso.")
except Exception as e:
    log.warning(f"Falha ao carregar '{WHISPER_MODEL_NAME}', tentando fallback 'base': {e}")
    _modelo_whisper = whisper.load_model("base")
    log.info("Modelo Whisper 'base' carregado.")

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
# Extração de OCR Otimizada (Tesseract)
# ---------------------------------------------------------------------------
def _extrair_texto_imagem(imagem_base64: str) -> str:
    """
    Extrai texto de imagem Base64 com pré-processamento avançado para livros escaneados:
    1. Escala de cinza (L)
    2. Autocontraste dinâmico para eliminar sombras e amarelamento de papel escaneado
    3. Super-resolução / Upscaling 2x com Lanczos para caracteres pequenos (como npn, pnp, Vcc, subscritos)
    4. Tesseract com bloco uniforme (--psm 6) e motor LSTM (--oem 1), com fallback adaptativo
    """
    image_data = base64.b64decode(imagem_base64)
    image = Image.open(io.BytesIO(image_data))
    
    gray = image.convert('L')
    contraste = ImageOps.autocontrast(gray, cutoff=2)

    # Se o recorte tiver fontes pequenas (altura menor que 500px), amplia 2x para o Tesseract reconhecer siglas pequenas
    if contraste.height < 500 or contraste.width < 1000:
        fator = 2
        img_proc = contraste.resize((contraste.width * fator, contraste.height * fator), Image.Resampling.LANCZOS)
    else:
        img_proc = contraste

    config = '--psm 6 --oem 1'
    texto = pytesseract.image_to_string(img_proc, lang='eng', config=config).strip()

    # Se veio quase vazio (ex: tabela ou diagrama), tenta psm 4 ou psm 3
    if len(texto) < 4 and (image.width > 50 and image.height > 25):
        texto = pytesseract.image_to_string(img_proc, lang='eng', config='--psm 4 --oem 1').strip()

    return texto


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
# Funções de Processamento IA (Ollama)
# ---------------------------------------------------------------------------
def _traduzir_sync(texto_ingles: Optional[str] = None, imagem_base64: Optional[str] = None) -> tuple:
    """Tradução técnica direta para Português usando Ollama."""
    if imagem_base64:
        log.info("[Tradução] Extraindo texto da imagem via Tesseract otimizado...")
        try:
            texto_ingles = _extrair_texto_imagem(imagem_base64)
            log.info(f"[Tradução] OCR extraiu {len(texto_ingles)} caracteres.")
            if not texto_ingles:
                return ("", "(Nenhum texto detectado nesta área da página)")
        except Exception as e:
            log.error(f"[Tradução] Erro no OCR: {e}")
            raise RuntimeError(f"Erro ao extrair imagem: {e}")

    if not texto_ingles:
        return ("", "")

    log.info(f"[Tradução] Traduzindo {len(texto_ingles)} caracteres com Ollama ({OLLAMA_MODEL})...")

    prompt = f"""Você é um tradutor técnico sênior especializado em Engenharia Elétrica, Eletrônica, Telecomunicações e Computação.
Traduza o seguinte texto do inglês para o português do Brasil com máxima fidelidade e rigor técnico.

Instruções para textos escaneados e termos de Engenharia:
1. Este texto provém de livros acadêmicos escaneados e pode conter pequenas imperfeições de OCR.
2. Reconheça e deduza pelo contexto termos e siglas clássicos da Engenharia:
   - Semicondutores & Circuitos: transistor npn, pnp, BJT, MOSFET, JFET, diodo zener, LED, CMOS, op-amp.
   - Parâmetros & Polarizações: Vcc, Vee, Vbe, Vce, Ib, Ic, Ie, hfe, beta, ganho, impedância, reatância, malha, nó.
   - Unidades: kΩ, MΩ, mA, µA, pF, nF, µF, V, mV, GHz, MHz, kHz.
3. Preserve integralmente equações, fórmulas matemáticas, nomes de variáveis e notações científicas.
4. Retorne APENAS a tradução em português, sem introduções, aspas extras ou explicações adicionais.

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


def _limpar_ocr_sync(texto_sujo: Optional[str] = None, imagem_base64: Optional[str] = None) -> str:
    """Corrige falhas de OCR mantendo o texto em inglês."""
    if imagem_base64:
        texto_sujo = _extrair_texto_imagem(imagem_base64)

    if not texto_sujo:
        return ""

    prompt = f"""Corrija quaisquer erros pontuais de OCR preservando 100% dos termos técnicos em inglês.
Retorne APENAS o texto corrigido, sem comentários.

Texto com OCR:
{texto_sujo}
"""
    resposta = ollama.generate(
        model=OLLAMA_MODEL,
        prompt=prompt.strip(),
        options={"temperature": 0.1, "num_predict": 512},
    )
    return resposta["response"].strip()


def _avaliar_pronuncia_sync(texto_esperado: str, texto_falado: str, nivel: str = "intermediario") -> dict:
    """
    Compara o texto esperado do documento com a transcrição do Whisper.
    Calcula acurácia objetiva e gera feedback de pronúncia via LLM considerando o nível de exigência.
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
    try:
        resp = ollama.generate(
            model=OLLAMA_MODEL,
            prompt=prompt.strip(),
            options={"temperature": 0.2, "num_predict": 180},
        )
        feedback = resp["response"].strip()
    except Exception as e:
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

class TextoResponse(BaseModel):
    resultado: str

class TraduzirRequest(BaseModel):
    texto_ingles: Optional[str] = None
    imagem_base64: Optional[str] = None

class TraducaoResponse(BaseModel):
    texto_ingles: str
    traducao_portugues: str

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


@app.post("/traduzir", response_model=TraducaoResponse, tags=["Tradução"])
async def traduzir(body: TraduzirRequest):
    """Tradução técnica direta para Português usando Ollama."""
    if not body.texto_ingles and not body.imagem_base64:
        raise HTTPException(status_code=422, detail="É necessário enviar 'texto_ingles' ou 'imagem_base64'.")

    try:
        texto_en, traducao_pt = await asyncio.to_thread(_traduzir_sync, body.texto_ingles, body.imagem_base64)
    except Exception as exc:
        log.error(f"[/traduzir] Erro: {exc}")
        raise HTTPException(status_code=500, detail=f"Erro na tradução: {exc}")

    return TraducaoResponse(texto_ingles=texto_en, traducao_portugues=traducao_pt)


@app.post("/limpar_ocr", response_model=TextoResponse, tags=["OCR"])
async def limpar_ocr(body: TextoSujoRequest):
    """Recebe texto com OCR e retorna texto limpo em inglês."""
    if not body.texto_sujo and not body.imagem_base64:
        raise HTTPException(status_code=422, detail="É necessário enviar 'texto_sujo' ou 'imagem_base64'.")

    try:
        resultado = await asyncio.to_thread(_limpar_ocr_sync, body.texto_sujo, body.imagem_base64)
    except Exception as exc:
        raise HTTPException(status_code=500, detail=f"Erro no OCR/IA: {exc}")

    return TextoResponse(resultado=resultado)


@app.post("/iniciar_gravacao", tags=["Áudio"])
async def iniciar_gravacao():
    """Inicia gravação contínua do microfone em background."""
    global _gravacao_thread, _gravando

    if _gravando:
        raise HTTPException(status_code=409, detail="Já há uma gravação em andamento.")

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
        # Transcrição em inglês para avaliação de pronúncia
        return _modelo_whisper.transcribe(audio_array, language=idioma)["text"].strip()

    texto = await asyncio.to_thread(_transcrever)
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
            body.nivel or "intermediario"
        )
    except Exception as exc:
        log.error(f"[/avaliar_pronuncia] Erro: {exc}")
        raise HTTPException(status_code=500, detail=f"Erro na avaliação de pronúncia: {exc}")

    return AvaliarPronunciaResponse(**resultado)


# ---------------------------------------------------------------------------
# Assistente Técnico / Chat IA Multimodal (Gemini & Ollama)
# ---------------------------------------------------------------------------
async def _chamar_gemini_chat_async(mensagem: str, imagem_base64: Optional[str], historico: List[ChatMensagem], api_key: str, modelo: Optional[str]) -> str:
    # Mapeamento e fallback para modelos válidos no Google AI Studio
    mod = (modelo or "").lower().strip()
    if not mod or "2.5" in mod or mod == "gemini-flash":
        modelo_escolhido = "gemini-1.5-flash"
    elif "2.0" in mod:
        modelo_escolhido = "gemini-2.0-flash"
    elif "pro" in mod:
        modelo_escolhido = "gemini-1.5-pro"
    else:
        modelo_escolhido = "gemini-1.5-flash"

    url = f"https://generativelanguage.googleapis.com/v1beta/models/{modelo_escolhido}:generateContent?key={api_key}"
    
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
            "inline_data": {
                "mime_type": "image/png",
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
    
    try:
        async with httpx.AsyncClient(timeout=60.0) as client:
            resp = await client.post(url, json=payload)
            if resp.status_code != 200:
                err_text = resp.text
                log.error(f"[Gemini API] Erro {resp.status_code}: {err_text}")
                try:
                    err_json = resp.json()
                    msg_google = err_json.get("error", {}).get("message", err_text)
                    status_google = err_json.get("error", {}).get("status", "")
                    if "API_KEY_INVALID" in msg_google or "API key not valid" in msg_google:
                        return "⚠️ **Chave de API do Gemini inválida.**\n\nPor favor, verifique se copiou a chave corretamente no [Google AI Studio](https://aistudio.google.com/app/apikey) e configure-a novamente no botão **⚙️ Chaves/API**."
                    elif "RESOURCE_EXHAUSTED" in status_google or "quota" in msg_google.lower():
                        return "⚠️ **Limite temporário de requisições excedido.**\n\nA cota gratuita por minuto do Gemini foi atingida. Aguarde cerca de 20 a 30 segundos e envie sua pergunta novamente."
                    elif "NOT_FOUND" in status_google or "not found" in msg_google.lower():
                        return f"⚠️ **Modelo Gemini não encontrado ({modelo_escolhido}).**\n\nErro: {msg_google}\n\nTente selecionar o modelo **gemini-1.5-flash** em Configurações."
                    return f"⚠️ **Erro na API do Google Gemini ({resp.status_code}):**\n\n{msg_google}"
                except Exception:
                    return f"⚠️ **Erro na API do Google Gemini ({resp.status_code}):**\n\n{err_text}"

            data = resp.json()
            return data["candidates"][0]["content"]["parts"][0]["text"].strip()
    except httpx.ConnectError:
        return "⚠️ **Erro de conexão com a Internet.**\n\nNão foi possível alcançar os servidores do Google Gemini. Verifique sua conexão de rede."
    except Exception as exc:
        log.error(f"[Gemini API] Exceção inesperada: {exc}")
        return f"⚠️ **Erro ao consultar Gemini:** {exc}"


async def _chamar_ollama_chat_async(mensagem: str, imagem_base64: Optional[str], historico: List[ChatMensagem], modelo: Optional[str]) -> str:
    modelo_escolhido = modelo or ("llama3.2-vision" if imagem_base64 else "llama3")
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


