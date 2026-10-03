#!/usr/bin/env python3
"""
Benchmark Tiers — Ensinador de Inglês
======================================
Testa os 4 perfis (Nuvem, Básico, Médio, Avançado) nos três endpoints principais:
  1. /traduzir  — Tradução técnica EN → PT com correção de OCR
  2. /limpar_ocr — Limpeza de OCR
  3. /avaliar_pronuncia — Avaliação de pronúncia

Saída: tabela no terminal + arquivo benchmark_results.json

Uso:
    ./venv/bin/python3 benchmark_tiers.py
"""

import asyncio
import json
import os
import statistics
import sys
import time
import configparser
import pathlib
from typing import Optional

import httpx

# ---------------------------------------------------------------------------
# Configuração
# ---------------------------------------------------------------------------
BASE_URL   = "http://localhost:8000"
OLLAMA_URL = "http://localhost:11434"
N_RUNS     = 3   # repetições por teste (P50 / P95)

# Ler chave Gemini do QSettings
def _ler_api_key() -> Optional[str]:
    cfg_base = pathlib.Path(os.environ.get("XDG_CONFIG_HOME", pathlib.Path.home() / ".config"))
    ini = cfg_base / "EnsinadorDeIngles" / "LeitorTecnico.conf"
    if ini.exists():
        cfg = configparser.ConfigParser()
        cfg.read(str(ini))
        return cfg.get("General", "iaApiKey", fallback=None)
    return None

GEMINI_KEY = _ler_api_key()

# ---------------------------------------------------------------------------
# Corpus de teste — textos reais do domínio do app
# ---------------------------------------------------------------------------
TEXTOS = {
    "curto": "The npn transistor is biased in the active region.",
    "medio": (
        "In a common-emitter amplifier, the BJT is biased such that the base-emitter "
        "junction is forward biased and the collector-base junction is reverse biased. "
        "The voltage gain Av = -gm * Rc."
    ),
    "longo": (
        "The operational amplifier (op-amp) is a high-gain electronic voltage amplifier "
        "with a differential input and, usually, a single-ended output. In the inverting "
        "configuration, the closed-loop gain is determined by the ratio -Rf/Rin. "
        "The virtual ground concept applies when the op-amp operates in the linear region "
        "with negative feedback. The CMRR (Common Mode Rejection Ratio) is a key parameter "
        "that quantifies the ability of the amplifier to reject common-mode signals. "
        "Typical values for precision op-amps range from 80 dB to 130 dB. "
        "The Thevenin equivalent circuit at the output terminal shows the output impedance "
        "decreasing with increased loop gain, approaching zero in ideal conditions."
    ),
    "ocr_sujo": (
        "Tbe npn tr4nsist0r base curr3nt IB flows fr0m emiiter t0 base. "
        "Th3 coliector current IC = hFE * IB wh3re hFE is the DC current g4in. "
        "Vcc = 12V, Rc = 2.2kOhm, Re = 470 Ohm."
    ),
}

PRONUNCIA_TESTS = [
    {
        "esperado":  "The transistor operates in the active region when the base-emitter junction is forward biased.",
        "falado":    "The transistor operates in the active region when the base emitter junction is forward biased.",
        "nivel":     "intermediario",
    },
    {
        "esperado":  "Kirchhoff's voltage law states that the sum of all voltages around a closed loop equals zero.",
        "falado":    "Kirchhoff voltage law states that the sum of voltages around a closed loop is zero.",
        "nivel":     "avancado",
    },
]

# ---------------------------------------------------------------------------
# Definição dos tiers
# ---------------------------------------------------------------------------
TIERS = [
    {
        "id":     0,
        "nome":   "Nuvem (Gemini API)",
        "ollama": None,
        "whisper": "base.en",
        "usa_gemini": True,
    },
    {
        "id":     1,
        "nome":   "Básico (phi3:mini)",
        "ollama": "phi3:mini",
        "whisper": "tiny.en",
        "usa_gemini": False,
    },
    {
        "id":     2,
        "nome":   "Médio (llama3.2:3b)",
        "ollama": "llama3.2:3b",
        "whisper": "base.en",
        "usa_gemini": False,
    },
    {
        "id":     3,
        "nome":   "Avançado (llama3)",
        "ollama": "llama3",
        "whisper": "base.en",
        "usa_gemini": False,
    },
]

# ---------------------------------------------------------------------------
# Cores ANSI
# ---------------------------------------------------------------------------
RESET  = "\033[0m"
BOLD   = "\033[1m"
GREEN  = "\033[92m"
YELLOW = "\033[93m"
RED    = "\033[91m"
CYAN   = "\033[96m"
BLUE   = "\033[94m"

def cor_latencia(ms: float) -> str:
    if ms < 3000:
        return GREEN
    elif ms < 8000:
        return YELLOW
    return RED

def cor_qualidade(score: float) -> str:
    if score >= 0.8:
        return GREEN
    elif score >= 0.5:
        return YELLOW
    return RED

def _avaliar_qualidade_traducao(pt: str, en: str) -> float:
    if not pt or len(pt) < 10:
        return 0.0
    if pt.lower() == en.lower():
        return 0.0
    termos_pt = ["transistor", "amplificador", "tensão", "corrente", "ganho", "região", "base", "coletor", "emissor", "polarizado", "malha"]
    score = sum(1 for t in termos_pt if t in pt.lower()) / len(termos_pt)
    if len(pt) < len(en) * 0.4:
        score *= 0.6
    return min(1.0, score)

async def _post(client: httpx.AsyncClient, endpoint: str, payload: dict, timeout: float = 90.0):
    t0 = time.perf_counter()
    try:
        resp = await client.post(f"{BASE_URL}{endpoint}", json=payload, timeout=timeout)
        elapsed = (time.perf_counter() - t0) * 1000
        if resp.status_code == 200:
            return resp.json(), elapsed
        else:
            return {"erro": f"HTTP {resp.status_code}: {resp.text[:200]}"}, elapsed
    except Exception as exc:
        elapsed = (time.perf_counter() - t0) * 1000
        return {"erro": str(exc)}, elapsed

# ---------------------------------------------------------------------------
# Testes por endpoint
# ---------------------------------------------------------------------------
async def testar_traducao(client, tier, texto_id, texto):
    api_key = GEMINI_KEY if tier["usa_gemini"] else None
    payload = {"texto_ingles": texto, "api_key": api_key}
    latencias = []
    qualidades = []
    erros = []
    ultima_saida = {}
    for _ in range(N_RUNS):
        resp, ms = await _post(client, "/traduzir", payload)
        latencias.append(ms)
        if "erro" in resp:
            erros.append(resp["erro"])
            qualidades.append(0.0)
        else:
            pt = resp.get("traducao_portugues", "")
            qualidades.append(_avaliar_qualidade_traducao(pt, texto))
            ultima_saida = resp
    return {
        "teste": f"traducao_{texto_id}",
        "tier_id": tier["id"],
        "tier_nome": tier["nome"],
        "latencia_ms": latencias,
        "p50_ms": statistics.median(latencias),
        "p95_ms": max(latencias),
        "qualidade": statistics.mean(qualidades),
        "erros": erros,
        "saida": ultima_saida,
    }

async def testar_ocr(client, tier):
    api_key = GEMINI_KEY if tier["usa_gemini"] else None
    payload = {"texto_sujo": TEXTOS["ocr_sujo"], "api_key": api_key}
    latencias = []
    erros = []
    ultima_saida = {}
    for _ in range(N_RUNS):
        resp, ms = await _post(client, "/limpar_ocr", payload)
        latencias.append(ms)
        if "erro" in resp:
            erros.append(resp["erro"])
        else:
            ultima_saida = resp
    corrigido = ultima_saida.get("resultado", "")
    correto = sum(1 for t in ["transistor", "emitter", "base", "current", "collector", "hFE"] if t.lower() in corrigido.lower())
    return {
        "teste": "limpeza_ocr",
        "tier_id": tier["id"],
        "tier_nome": tier["nome"],
        "latencia_ms": latencias,
        "p50_ms": statistics.median(latencias),
        "p95_ms": max(latencias),
        "qualidade": correto / 6.0,
        "erros": erros,
        "saida": ultima_saida,
    }

async def testar_pronuncia(client, tier, test_case, idx):
    payload = {
        "texto_esperado": test_case["esperado"],
        "texto_falado":   test_case["falado"],
        "nivel":          test_case["nivel"],
    }
    latencias = []
    erros = []
    ultima_saida = {}
    for _ in range(N_RUNS):
        resp, ms = await _post(client, "/avaliar_pronuncia", payload)
        latencias.append(ms)
        if "erro" in resp:
            erros.append(resp["erro"])
        else:
            ultima_saida = resp
    nota = ultima_saida.get("nota", 0) / 100.0 if ultima_saida else 0.0
    return {
        "teste": f"pronuncia_case{idx}",
        "tier_id": tier["id"],
        "tier_nome": tier["nome"],
        "latencia_ms": latencias,
        "p50_ms": statistics.median(latencias),
        "p95_ms": max(latencias),
        "qualidade": nota,
        "erros": erros,
        "saida": ultima_saida,
    }

def _verificar_backend() -> bool:
    import urllib.request
    for endpoint in ["/saude", "/docs"]:
        try:
            r = urllib.request.urlopen(f"{BASE_URL}{endpoint}", timeout=3)
            if r.status == 200:
                return True
        except Exception:
            pass
    return False

def _print_resultado_linha(res):
    p50 = res["p50_ms"]
    q   = res["qualidade"]
    cor_l = cor_latencia(p50)
    cor_q = cor_qualidade(q)
    erros_str = f"  {RED}[{len(res['erros'])} ERROS]{RESET}" if res["erros"] else ""
    print(f"{cor_l}{p50/1000:6.1f}s{RESET}  qualidade={cor_q}{q:.0%}{RESET}{erros_str}")

def _imprimir_relatorio(resultados):
    print(f"\n\n{BOLD}{CYAN}{'='*70}{RESET}")
    print(f"{BOLD}{CYAN}  RELATÓRIO CONSOLIDADO{RESET}")
    print(f"{BOLD}{CYAN}{'='*70}{RESET}")
    por_tier = {}
    for r in resultados:
        tid = r["tier_id"]
        if tid not in por_tier:
            por_tier[tid] = {"nome": r["tier_nome"], "resultados": []}
        por_tier[tid]["resultados"].append(r)

    print(f"\n{BOLD}  {'Tier':<28} {'Teste':<22} {'P50':>7} {'P95':>7} {'Qualidade':>10} {'Status':>8}{RESET}")
    print("  " + "-" * 86)
    for tid, dados in sorted(por_tier.items()):
        for r in dados["resultados"]:
            p50 = r["p50_ms"]
            p95 = r["p95_ms"]
            q   = r["qualidade"]
            status = f"{RED}FALHA{RESET}" if r["erros"] else f"{GREEN}OK{RESET}"
            cor_l = cor_latencia(p50)
            cor_q = cor_qualidade(q)
            print(
                f"  {dados['nome']:<28} {r['teste']:<22} "
                f"{cor_l}{p50/1000:>6.1f}s{RESET} {p95/1000:>6.1f}s "
                f"{cor_q}{q:>9.0%}{RESET}   {status}"
            )
        print()

    print(f"\n{BOLD}  Sumário geral por tier:{RESET}")
    print("  " + "-" * 55)
    for tid, dados in sorted(por_tier.items()):
        rs = dados["resultados"]
        p50_medio = statistics.mean(r["p50_ms"] for r in rs)
        q_medio   = statistics.mean(r["qualidade"] for r in rs)
        n_erros   = sum(len(r["erros"]) for r in rs)
        cor_l = cor_latencia(p50_medio)
        cor_q = cor_qualidade(q_medio)
        erros_str = f"  {RED}{n_erros} erros{RESET}" if n_erros else f"  {GREEN}sem erros{RESET}"
        print(
            f"  Tier {tid} {dados['nome']:<26} "
            f"latência méd={cor_l}{p50_medio/1000:.1f}s{RESET}  "
            f"qualidade={cor_q}{q_medio:.0%}{RESET}{erros_str}"
        )

def _analisar_e_sugerir_otimizacoes(resultados):
    print(f"\n\n{BOLD}{CYAN}{'='*70}{RESET}")
    print(f"{BOLD}{CYAN}  ANÁLISE DE OTIMIZAÇÕES{RESET}")
    print(f"{BOLD}{CYAN}{'='*70}{RESET}\n")

    criticos = [r for r in resultados if r["p50_ms"] > 10_000]
    lentos   = [r for r in resultados if 5_000 < r["p50_ms"] <= 10_000]
    baixa_q  = [r for r in resultados if r["qualidade"] < 0.4 and not r["erros"]]
    com_erro = [r for r in resultados if r["erros"]]

    if not criticos and not lentos and not baixa_q and not com_erro:
        print(f"  {GREEN}Todos os tiers dentro dos parâmetros aceitáveis. Nenhuma otimização crítica necessária.{RESET}\n")
        return

    sugestoes = set()

    if criticos:
        print(f"  {RED}CRÍTICO — Tiers com P50 > 10s:{RESET}")
        for r in criticos:
            print(f"    • {r['tier_nome']} / {r['teste']}: {r['p50_ms']/1000:.1f}s")
        sugestoes.add("num_predict")
        print()

    if lentos:
        print(f"  {YELLOW}LENTO — Tiers com P50 entre 5s e 10s:{RESET}")
        for r in lentos:
            print(f"    • {r['tier_nome']} / {r['teste']}: {r['p50_ms']/1000:.1f}s")
        sugestoes.add("context_window")
        print()

    if baixa_q:
        print(f"  {YELLOW}QUALIDADE BAIXA — Tiers com score < 40%:{RESET}")
        for r in baixa_q:
            print(f"    • {r['tier_nome']} / {r['teste']}: {r['qualidade']:.0%}")
        sugestoes.add("prompt_reforco")
        print()

    if com_erro:
        print(f"  {RED}ERROS DE EXECUÇÃO:{RESET}")
        for r in com_erro:
            for e in r["erros"]:
                print(f"    • [{r['tier_nome']} / {r['teste']}] {e[:120]}")
        print()

    print(f"  {BOLD}Sugestões de otimização:{RESET}")
    n = 1
    if "num_predict" in sugestoes:
        print(f"""
  {n}. Reduzir num_predict nos modelos lentos:
     - texto curto (<100 chars): num_predict = 200
     - texto médio (<300 chars): num_predict = 350
     - texto longo: manter teto em 1024
     → main.py: trocar escalares n_chars * 2.5 por n_chars * 1.8 para Tiers 1 e 2""")
        n += 1

    if "context_window" in sugestoes:
        print(f"""
  {n}. Limitar janela de contexto (num_ctx) nos tiers leves:
     - phi3:mini e llama3.2:3b: adicionar "num_ctx": 2048 no options do ollama.generate
     → Reduz tempo de inicialização e uso de RAM sem perda perceptível de qualidade em textos curtos""")
        n += 1

    if "prompt_reforco" in sugestoes:
        print(f"""
  {n}. Reforço de prompt para modelos pequenos (phi3:mini):
     - Adicionar no topo: "Responda SOMENTE com JSON válido. Nenhum texto antes ou depois."
     → phi3:mini tende a adicionar texto explicativo fora do JSON""")
        n += 1

    if com_erro:
        print(f"""
  {n}. Erros de execução detectados:
     - Verifique: ollama serve && ollama list
     - Reinicie o backend: ./iniciar_backend.sh""")
    print()

async def _benchmark_direto():
    """Benchmark direto via ollama (sem servidor FastAPI)."""
    import ollama as _ollama

    print(f"\n{BOLD}Testando modelos Ollama diretamente (sem servidor FastAPI){RESET}\n")
    PROMPT_TESTE = (
        'Translate to Brazilian Portuguese and fix OCR errors. '
        'Respond ONLY with valid JSON: {"en_corrigido": "...", "pt": "..."}\n\n'
        "Text: The npn transistor base current IB flows from emitter to base. "
        "The collector current IC = hFE * IB where hFE is the DC current gain. Vcc = 12V."
    )

    resultados_diretos = []
    for tier in TIERS[1:]:
        modelo = tier["ollama"]
        print(f"  Testando {tier['nome']:30s} ... ", end="", flush=True)
        latencias = []
        for _ in range(N_RUNS):
            t0 = time.perf_counter()
            try:
                _ollama.generate(
                    model=modelo,
                    prompt=PROMPT_TESTE,
                    options={"temperature": 0.05, "num_predict": 400},
                )
                latencias.append((time.perf_counter() - t0) * 1000)
            except Exception as exc:
                latencias.append(99999)
                print(f"\n    {RED}ERRO: {exc}{RESET}", end="")
        p50 = statistics.median(latencias)
        p95 = max(latencias)
        cor = cor_latencia(p50)
        print(f"{cor}{p50/1000:.1f}s{RESET} (P95={p95/1000:.1f}s)")
        resultados_diretos.append({"tier": tier["nome"], "p50_ms": p50, "p95_ms": p95})

    if GEMINI_KEY:
        print(f"\n  Testando Tier 0 Nuvem (Gemini API) ... ", end="", flush=True)
        t0 = time.perf_counter()
        try:
            import urllib.request, json as _json
            mod = "gemini-3.8-flash"
            url = f"https://generativelanguage.googleapis.com/v1beta/models/{mod}:generateContent?key={GEMINI_KEY}"
            payload = json.dumps({"contents": [{"parts": [{"text": "Translate: The transistor is active."}]}]}).encode()
            req = urllib.request.Request(url, data=payload, headers={"Content-Type": "application/json"}, method="POST")
            resp = urllib.request.urlopen(req, timeout=30)
            ms = (time.perf_counter() - t0) * 1000
            cor = cor_latencia(ms)
            print(f"{cor}{ms/1000:.1f}s{RESET} (API remota, GPU do Google)")
        except Exception as exc:
            ms = (time.perf_counter() - t0) * 1000
            print(f"{RED}{ms/1000:.1f}s — ERRO: {exc}{RESET}")

    print(f"\n{BOLD}Sumário Direto:{RESET}")
    for r in resultados_diretos:
        cor = cor_latencia(r["p50_ms"])
        print(f"  {r['tier']:30s} P50={cor}{r['p50_ms']/1000:.1f}s{RESET}  P95={r['p95_ms']/1000:.1f}s")

async def rodar_benchmark():
    print(f"\n{BOLD}{CYAN}{'='*70}{RESET}")
    print(f"{BOLD}{CYAN}  BENCHMARK DE TIERS — Ensinador de Inglês{RESET}")
    print(f"{BOLD}{CYAN}{'='*70}{RESET}")

    if not GEMINI_KEY:
        print(f"\n{YELLOW}[AVISO] Chave Gemini não encontrada. Tier 0 (Nuvem) usará chave vazia.{RESET}")

    backend_ok = _verificar_backend()
    if not backend_ok:
        print(f"\n{YELLOW}Backend FastAPI não detectado em {BASE_URL}.{RESET}")
        print(f"{YELLOW}Rodando benchmark DIRETO (sem servidor)...{RESET}")
        await _benchmark_direto()
        return

    print(f"\n{GREEN}Backend detectado em {BASE_URL}. {N_RUNS} rodadas por teste.{RESET}\n")

    todos_resultados = []

    async with httpx.AsyncClient() as client:
        for tier in TIERS:
            if tier["usa_gemini"] and not GEMINI_KEY:
                print(f"\n{YELLOW}[SKIP] Tier {tier['id']} — {tier['nome']} (sem chave Gemini){RESET}")
                continue

            print(f"\n{BOLD}{BLUE}━━━ Tier {tier['id']}: {tier['nome']} ━━━{RESET}")

            for texto_id in ["curto", "medio", "longo"]:
                print(f"  ↳ tradução [{texto_id:5s}]   ", end="", flush=True)
                res = await testar_traducao(client, tier, texto_id, TEXTOS[texto_id])
                todos_resultados.append(res)
                _print_resultado_linha(res)

            print(f"  ↳ limpeza OCR       ", end="", flush=True)
            res = await testar_ocr(client, tier)
            todos_resultados.append(res)
            _print_resultado_linha(res)

            for i, caso in enumerate(PRONUNCIA_TESTS, 1):
                print(f"  ↳ pronúncia #{i}      ", end="", flush=True)
                res = await testar_pronuncia(client, tier, caso, i)
                todos_resultados.append(res)
                _print_resultado_linha(res)

    _imprimir_relatorio(todos_resultados)

    output_path = pathlib.Path(__file__).parent / "benchmark_results.json"
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(todos_resultados, f, ensure_ascii=False, indent=2)
    print(f"\n{CYAN}Resultados salvos em: {output_path}{RESET}")

    _analisar_e_sugerir_otimizacoes(todos_resultados)

if __name__ == "__main__":
    asyncio.run(rodar_benchmark())
