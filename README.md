# EP2 — Monitoramento de Métricas (MAC0352)

Descrição
-------
Este repositório contém uma implementação didática de um sistema distribuído para coleta de métricas. O sistema possui:
- agentes (`agent`) que coletam métricas locais (CPU, memória, uptime, tráfego de rede) e expõem uma MIB simplificada;
- um gerenciador (`manager`) que conecta-se aos agentes, consulta métricas e imprime um resumo;
- utilitários para iniciar múltiplos agentes localmente (`run.py`).

Observação: a aplicação de visualização desenvolvida localmente não faz parte desta entrega e não é mencionada aqui.

Pré-requisitos
-------
- GNU Make / CMake (>= 3.16)
- compilador C++ com suporte a C++17 (g++/clang++)
- (Opcional) Python 3 (para o script `run.py`)

Compilação (recomendado: CMake)
-------
A forma recomendada de compilar é usando CMake em um diretório de build:

```bash
# a partir da raiz do projeto
cmake -S . -B build
cmake --build build -- -j
```

Após a compilação os executáveis estarão em `build/agent` e `build/manager`.

Compilação manual (alternativa)
-------
Se preferir compilar manualmente sem CMake, use os comandos abaixo (exemplo usando `g++`):

```bash
# Compilar agent
g++ -std=c++17 -O2 -Wall -Wextra -pthread \
  src/agents.cpp src/server.cpp src/metrics.cpp src/mib.cpp \
  -Iinclude -o build/agent

# Compilar manager
g++ -std=c++17 -O2 -Wall -Wextra \
  src/manager.cpp src/utils.cpp \
  -Iinclude -o build/manager
```

Certifique-se de que o diretório `build/` exista antes de direcionar os binários para lá:

```bash
mkdir -p build
```

Execução
-------
Executando um único `agent` (exemplo):

```bash
# executável e parâmetros: [porta] [interface]
./build/agent 54001 wlan0
```

Executando o `manager` apontando para um ou mais agents:

```bash
# cada agente é passado como IP:PORT
./build/manager 127.0.0.1:54001 127.0.0.1:54002
```

Script auxiliar `run.py`
-------
O projeto inclui `run.py`, um script que inicia N agentes (em portas consecutivas a partir de 54001) e em seguida inicia o `manager` apontando para todos eles. O script espera encontrar os executáveis em `build/agent` e `build/manager`.

Opção A — usando `uv` (recomendado se quiser ambiente isolado):

```bash
# instala dependências do pyproject.toml no ambiente gerenciado por uv
uv sync

# inicia 2 agents + manager (altere o número conforme necessário)
uv run python run.py 2
```

Opção B — manual, sem uv:

```bash
# execute em um terminal para iniciar N agents e o manager via script
python3 run.py 2
```

Antes de executar manualmente, instale as dependências Python listadas em `requirements.txt` (crie um ambiente virtual se desejar):

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
# então execute:
python3 run.py 2
```

Observações: o script abre cada processo (agents e manager) em background e imprime suas saídas prefixadas. Use Ctrl+C no terminal que executa `run.py` para encerrar os processos iniciados pelo script.

Limpeza
-------
Se você usou CMake, é possível limpar os artefatos removendo o diretório `build/`:

```bash
rm -rf build
```

Alternativamente, o alvo CMake customizado `clean-all` está disponível se estiver usando `make` gerado pelo CMake (execute em `build/`):

```bash
cmake --build build --target clean-all
```

Notas finais
-------
- O código está organizado em `src/` (fontes) e `include/` (headers).
- Se houver problemas de permissão ao executar os binários, verifique as permissões e o caminho corretos.
- Para experimentos em grande escala, considerar ajustar timeouts e usar versão do manager com consultas paralelas.
