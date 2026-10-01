import sounddevice as sd
import whisper
import ollama
import numpy as np
import warnings
import time

# Suprimir o aviso de FP16 do Whisper quando roda na CPU
warnings.filterwarnings("ignore", message="FP16 is not supported on CPU")

def gravar_audio(duracao_maxima=15, taxa_amostragem=16000, limite_silencio=2.0, limite_volume=0.01):
    """
    Grava o áudio do microfone nativamente com SoundDevice.
    Implementa um detector de voz (VAD) inteligente:
    Espera até 2 segundos de silêncio (pause_threshold) para cortar a gravação.
    """
    print("\n[Microfone] Inicializando microfone... Aguarde.")
    time.sleep(1) # Simula um ajuste de ambiente
    
    print("\n>>> FALE AGORA a sua tradução em português... <<<")
    
    frames_gravados = []
    tempo_silencio = 0.0
    tamanho_bloco = int(taxa_amostragem * 0.1) # Lendo em pedacinhos de 100ms (0.1s)
    falou_algo = False
    
    try:
        # Abrimos um stream de entrada de áudio
        # dtype='float32' já nos entrega o áudio no formato [-1.0, 1.0] que o Whisper gosta!
        with sd.InputStream(samplerate=taxa_amostragem, channels=1, dtype='float32', blocksize=tamanho_bloco) as stream:
            
            # Vamos ler blocos até o limite da duracao_maxima
            limite_iteracoes = int(duracao_maxima / 0.1)
            
            for _ in range(limite_iteracoes):
                bloco_audio, overflow = stream.read(tamanho_bloco)
                frames_gravados.append(bloco_audio)
                
                # Calcula o volume atual (Root Mean Square)
                volume_atual = np.sqrt(np.mean(bloco_audio**2))
                
                if volume_atual > limite_volume:
                    # O usuário está falando! Zera o contador de silêncio.
                    tempo_silencio = 0.0
                    falou_algo = True
                else:
                    # O volume está baixo (silêncio)
                    if falou_algo:
                        tempo_silencio += 0.1 # Passou-se 100ms de silêncio
                        
                        # Se ficou em silêncio por 2.0 segundos, encerramos!
                        if tempo_silencio >= limite_silencio:
                            print(f"[Microfone] Silêncio de {limite_silencio}s detectado. Encerrando gravação.")
                            break
                            
        print("[Microfone] Gravação concluída.")
        
        # Junta todos os blocos em um único array 1D
        audio_array = np.concatenate(frames_gravados).flatten()
        return audio_array
        
    except Exception as e:
        print(f"Erro ao gravar áudio: {e}")
        return None

def transcrever(audio_array):
    """
    Utiliza o modelo Whisper (small) para transcrever o áudio na memória.
    """
    print("[Whisper] Carregando o modelo 'small' e transcrevendo o áudio (pode demorar um pouquinho)...")
    
    modelo = whisper.load_model("small")
    
    resultado = modelo.transcribe(audio_array, language="pt")
    
    texto_transcrito = resultado["text"].strip()
    return texto_transcrito

def avaliar(texto_ingles, texto_portugues):
    """
    Chama a LLM local via Ollama para validar a tradução comparando as frases.
    """
    print("[Ollama] Avaliando a similaridade semântica...")
    
    prompt = f"""
Você é um avaliador técnico e preciso. Avalie a similaridade de sentido entre a frase original em inglês e a tradução em português.
Inglês: "{texto_ingles}"
Português: "{texto_portugues}"

Regras Críticas:
1. Se a tradução mantiver 100% do significado técnico (corrente, capacitor, proporcionalidade, taxa de variação, tensão), a nota DEVE SER EXATAMENTE 100. Não desconte pontos por escolhas de estilo ou sinônimos corretos.
2. Só penalize (abaixo de 100) se houver erro conceitual físico, omissão de informações cruciais ou adição de informações falsas.
3. Responda EXATAMENTE E APENAS com um número inteiro de 0 a 100. Não adicione o símbolo de %, texto extra, nem explique nada.
"""
    resposta = ollama.generate(model='llama3', prompt=prompt.strip())
    
    return resposta['response'].strip()

def main():
    frase_ingles = "The current flowing through a capacitor is directly proportional to the rate of change of voltage across it."
    print("="*60)
    print(f"FRASE ALVO (INGLÊS):\n{frase_ingles}")
    print("="*60)
    
    # 2. Gravar o áudio direto na memória
    audio_array = gravar_audio(duracao_maxima=15)
    
    if audio_array is not None:
        # 3. Transcrever o áudio
        texto_reconhecido = transcrever(audio_array)
        
        # Imprimir o texto transcrito
        print(f"\n---> [Texto Reconhecido pelo Whisper]: {texto_reconhecido}")
        
        if texto_reconhecido:
            # 4 & 5. Avaliar e imprimir o resultado
            nota = avaliar(frase_ingles, texto_reconhecido)
            print(f"---> [Nota do Ollama (0-100)]: {nota}\n")
        else:
            print("\n[Aviso] O áudio foi gravado, mas nenhuma palavra foi reconhecida.")

if __name__ == "__main__":
    main()
