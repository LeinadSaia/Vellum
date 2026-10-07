import pytest
from fastapi.testclient import TestClient
import sys
import os

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from main import app

client = TestClient(app)

def test_health_check():
    response = client.get("/health")
    assert response.status_code == 200
    assert response.json() == {"status": "ok"}

def test_listar_vozes():
    response = client.get("/vozes")
    assert response.status_code == 200
    assert isinstance(response.json(), list)
    assert len(response.json()) > 0

def test_modelos_status():
    response = client.get("/modelos_status")
    assert response.status_code == 200
    assert "ollama_online" in response.json()

def test_traduzir_sem_texto():
    response = client.post("/traduzir", json={})
    assert response.status_code == 400

# def test_limpar_ocr():
#    response = client.post("/limpar_ocr", json={"texto_sujo": "Hello-\nworld\n123"})
#    assert response.status_code == 200
#    assert "Hello world" in response.json()["texto_limpo"]

