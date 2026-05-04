#!/usr/bin/env python3
"""
Dashboard Streamlit para visualizar métricas dos agents
"""

import socket
import time
import threading
from collections import defaultdict, deque
from dataclasses import dataclass
from typing import Dict, List, Optional
import streamlit as st
import pandas as pd
import plotly.graph_objects as go
from datetime import datetime


@dataclass
class MetricsData:
    """Estrutura para armazenar dados de métricas"""
    timestamp: datetime
    cpu_usage: Optional[float] = None
    memory_usage: Optional[float] = None
    uptime_seconds: Optional[float] = None
    tx_rate: Optional[float] = None  # bytes per second
    rx_rate: Optional[float] = None  # bytes per second


class AgentClient:
    """Cliente para se conectar a um agent e coletar métricas"""
    
    def __init__(self, host: str, port: int, max_history: int = 300):
        self.host = host
        self.port = port
        self.socket: Optional[socket.socket] = None
        self.metrics_history: deque = deque(maxlen=max_history)
        self.max_history = max_history
        self.connected = False
        self.last_error: Optional[str] = None
        
    def connect(self) -> bool:
        """Conecta ao agent"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(2.5)
            self.socket.connect((self.host, self.port))
            self.connected = True
            self.last_error = None
            return True
        except Exception as e:
            self.last_error = str(e)
            self.connected = False
            return False
    
    def disconnect(self):
        """Desconecta do agent"""
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
        self.connected = False
    
    def request_metrics(self) -> Optional[MetricsData]:
        """Requisita métricas do agent"""
        if not self.connected:
            if not self.connect():
                return None
        
        try:
            # Envia requisição
            self.socket.sendall(b"GET .")
            
            # Recebe resposta
            buffer = bytearray(1024)
            bytes_received = self.socket.recv_into(buffer)
            
            if bytes_received == 0:
                self.disconnect()
                return None
            
            response = buffer[:bytes_received].decode('utf-8')
            
            # Parse da resposta
            metrics = MetricsData(timestamp=datetime.now())
            
            for part in response.split(';'):
                if not part.strip():
                    continue
                
                try:
                    key, value = part.split('=')
                    value = float(value)
                    
                    if key == ".1.1":
                        metrics.cpu_usage = value
                    elif key == ".1.2":
                        metrics.memory_usage = value
                    elif key == ".1.3":
                        metrics.uptime_seconds = value
                    elif key == ".2.1":
                        metrics.tx_rate = value
                    elif key == ".2.2":
                        metrics.rx_rate = value
                except ValueError:
                    continue
            
            self.metrics_history.append(metrics)
            self.last_error = None
            return metrics
            
        except socket.timeout:
            self.last_error = "Connection timeout"
            self.disconnect()
            return None
        except Exception as e:
            self.last_error = str(e)
            self.disconnect()
            return None
    
    def get_history(self) -> List[MetricsData]:
        """Retorna o histórico de métricas"""
        return list(self.metrics_history)


class MetricsCollector:
    """Coletor de métricas de múltiplos agents"""
    
    def __init__(self):
        self.agents: Dict[str, AgentClient] = {}
        self.collection_thread: Optional[threading.Thread] = None
        self.running = False
        self.lock = threading.Lock()
    
    def add_agent(self, name: str, host: str, port: int):
        """Adiciona um novo agent para monitorar"""
        with self.lock:
            self.agents[name] = AgentClient(host, port)
    
    def start_collection(self, interval: float = 1.0):
        """Inicia a coleta de métricas em background"""
        if self.running:
            return
        
        self.running = True
        self.collection_thread = threading.Thread(
            target=self._collect_loop,
            args=(interval,),
            daemon=True
        )
        self.collection_thread.start()
    
    def stop_collection(self):
        """Para a coleta de métricas"""
        self.running = False
        if self.collection_thread:
            self.collection_thread.join(timeout=5)
    
    def _collect_loop(self, interval: float):
        """Loop de coleta de métricas"""
        while self.running:
            with self.lock:
                for agent in self.agents.values():
                    agent.request_metrics()
            
            time.sleep(interval)
    
    def get_agent_data(self, agent_name: str) -> Optional[List[MetricsData]]:
        """Retorna dados de um agent específico"""
        with self.lock:
            if agent_name in self.agents:
                return self.agents[agent_name].get_history()
        return None
    
    def get_agent_status(self, agent_name: str) -> Dict:
        """Retorna status de um agent"""
        with self.lock:
            if agent_name not in self.agents:
                return {"status": "unknown"}
            
            agent = self.agents[agent_name]
            return {
                "status": "connected" if agent.connected else "disconnected",
                "error": agent.last_error,
                "metrics_collected": len(agent.metrics_history)
            }
    
    def get_all_agents(self) -> List[str]:
        """Retorna lista de todos os agents"""
        with self.lock:
            return list(self.agents.keys())


def create_cpu_chart(agent_name: str, data: List[MetricsData]) -> go.Figure:
    """Cria gráfico de CPU"""
    timestamps = [m.timestamp for m in data if m.cpu_usage is not None]
    values = [m.cpu_usage for m in data if m.cpu_usage is not None]
    
    fig = go.Figure()
    fig.add_trace(go.Scatter(
        x=timestamps, y=values,
        mode='lines+markers',
        name='CPU Usage',
        line=dict(color='#FF6B6B', width=2),
        fill='tozeroy'
    ))
    
    fig.update_layout(
        title=f"CPU Usage - {agent_name}",
        xaxis_title="Time",
        yaxis_title="Usage (%)",
        hovermode='x unified',
        template='plotly_dark',
        height=400
    )
    
    return fig


def create_memory_chart(agent_name: str, data: List[MetricsData]) -> go.Figure:
    """Cria gráfico de memória"""
    timestamps = [m.timestamp for m in data if m.memory_usage is not None]
    values = [m.memory_usage for m in data if m.memory_usage is not None]
    
    fig = go.Figure()
    fig.add_trace(go.Scatter(
        x=timestamps, y=values,
        mode='lines+markers',
        name='Memory Usage',
        line=dict(color='#4ECDC4', width=2),
        fill='tozeroy'
    ))
    
    fig.update_layout(
        title=f"Memory Usage - {agent_name}",
        xaxis_title="Time",
        yaxis_title="Usage (%)",
        hovermode='x unified',
        template='plotly_dark',
        height=400
    )
    
    return fig


def create_network_chart(agent_name: str, data: List[MetricsData]) -> go.Figure:
    """Cria gráfico de tráfego de rede"""
    timestamps = [m.timestamp for m in data if m.tx_rate is not None or m.rx_rate is not None]
    tx_values = [m.tx_rate / 1024 if m.tx_rate is not None else 0 for m in data]
    rx_values = [m.rx_rate / 1024 if m.rx_rate is not None else 0 for m in data]
    
    fig = go.Figure()
    fig.add_trace(go.Scatter(
        x=timestamps, y=tx_values,
        mode='lines',
        name='TX Rate',
        line=dict(color='#45B7D1', width=2)
    ))
    fig.add_trace(go.Scatter(
        x=timestamps, y=rx_values,
        mode='lines',
        name='RX Rate',
        line=dict(color='#FFA07A', width=2)
    ))
    
    fig.update_layout(
        title=f"Network Traffic - {agent_name}",
        xaxis_title="Time",
        yaxis_title="Rate (KB/s)",
        hovermode='x unified',
        template='plotly_dark',
        height=400
    )
    
    return fig


def main():
    st.set_page_config(
        page_title="System Metrics Dashboard",
        page_icon="📊",
        layout="wide"
    )
    
    st.title("📊 System Metrics Dashboard")
    
    # Inicializa o collector na session
    if 'collector' not in st.session_state:
        st.session_state.collector = MetricsCollector()
    
    collector = st.session_state.collector
    
    # Sidebar para configuração
    st.sidebar.header("Configuration")
    
    # Seção para adicionar agents
    st.sidebar.subheader("Add Agent")
    agent_name = st.sidebar.text_input("Agent Name", key="agent_name_input")
    agent_host = st.sidebar.text_input("Host", value="127.0.0.1", key="agent_host_input")
    agent_port = st.sidebar.number_input("Port", value=54001, min_value=1, max_value=65535, key="agent_port_input")
    
    if st.sidebar.button("Add Agent"):
        if agent_name and agent_host and agent_port:
            collector.add_agent(agent_name, agent_host, int(agent_port))
            st.sidebar.success(f"Agent '{agent_name}' added!")
        else:
            st.sidebar.error("Please fill in all fields")
    
    # Inicia coleta se não estiver rodando
    if not collector.running:
        collector.start_collection(interval=1.0)
    
    # Lista de agents
    agents = collector.get_all_agents()
    
    if not agents:
        st.info("No agents added yet. Use the sidebar to add agents.")
        return
    
    # Seleção de agent
    selected_agent = st.selectbox("Select Agent", agents)
    
    # Mostra status do agent
    status = collector.get_agent_status(selected_agent)
    col1, col2, col3 = st.columns(3)
    
    with col1:
        status_color = "🟢" if status["status"] == "connected" else "🔴"
        st.metric("Status", f"{status_color} {status['status'].capitalize()}")
    
    with col2:
        st.metric("Metrics Collected", status["metrics_collected"])
    
    with col3:
        if status["error"]:
            st.metric("Last Error", "✓ No error" if not status["error"] else f"✗ {status['error'][:20]}...")
    
    # Obtém dados do agent
    agent_data = collector.get_agent_data(selected_agent)
    
    if not agent_data:
        st.warning("No data collected yet. Waiting for metrics...")
        return
    
    # Mostra métricas atuais
    st.subheader("Current Metrics")
    
    latest = agent_data[-1] if agent_data else None
    
    if latest:
        col1, col2, col3, col4 = st.columns(4)
        
        with col1:
            if latest.cpu_usage is not None:
                st.metric("CPU Usage", f"{latest.cpu_usage:.1f}%")
        
        with col2:
            if latest.memory_usage is not None:
                st.metric("Memory Usage", f"{latest.memory_usage:.1f}%")
        
        with col3:
            if latest.uptime_seconds is not None:
                hours = int(latest.uptime_seconds) // 3600
                minutes = (int(latest.uptime_seconds) % 3600) // 60
                st.metric("Uptime", f"{hours}h {minutes}m")
        
        with col4:
            if latest.tx_rate is not None and latest.rx_rate is not None:
                tx_kb = latest.tx_rate / 1024
                rx_kb = latest.rx_rate / 1024
                st.metric("Network", f"↑{tx_kb:.1f} ↓{rx_kb:.1f} KB/s")
    
    # Gráficos
    st.subheader("Historical Data")
    
    tab1, tab2, tab3 = st.tabs(["CPU", "Memory", "Network"])
    
    with tab1:
        if any(m.cpu_usage is not None for m in agent_data):
            fig = create_cpu_chart(selected_agent, agent_data)
            st.plotly_chart(fig, use_container_width=True)
        else:
            st.info("No CPU data available yet")
    
    with tab2:
        if any(m.memory_usage is not None for m in agent_data):
            fig = create_memory_chart(selected_agent, agent_data)
            st.plotly_chart(fig, use_container_width=True)
        else:
            st.info("No memory data available yet")
    
    with tab3:
        if any(m.tx_rate is not None or m.rx_rate is not None for m in agent_data):
            fig = create_network_chart(selected_agent, agent_data)
            st.plotly_chart(fig, use_container_width=True)
        else:
            st.info("No network data available yet")
    
    # Tabela de dados
    st.subheader("Data Table")
    
    df_data = []
    for m in agent_data:
        df_data.append({
            "Timestamp": m.timestamp.strftime("%H:%M:%S"),
            "CPU (%)": f"{m.cpu_usage:.1f}" if m.cpu_usage is not None else "N/A",
            "Memory (%)": f"{m.memory_usage:.1f}" if m.memory_usage is not None else "N/A",
            "TX (KB/s)": f"{m.tx_rate/1024:.1f}" if m.tx_rate is not None else "N/A",
            "RX (KB/s)": f"{m.rx_rate/1024:.1f}" if m.rx_rate is not None else "N/A",
        })
    
    if df_data:
        df = pd.DataFrame(df_data)
        st.dataframe(df, use_container_width=True, hide_index=True)


if __name__ == "__main__":
    main()
