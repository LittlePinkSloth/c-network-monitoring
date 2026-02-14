/*
 * Project: IoT Monitoring System
 * Author: LittlePinkSloth
 * License: MIT License
 */

const API_URL = '/data.json';
const REFRESH_RATE = 2000;
const MAX_HISTORY = 10;

const clientStates = {}; 

const TYPES_CONFIG = {
    1: { name: "PC Linux", class: "type-pc-linux" },
    2: { name: "PC Windows", class: "type-pc-windows" },
    3: { name: "Raspberry Pi", class: "type-raspberry" },
    4: { name: "Robot Bras", class: "type-bras" },
    5: { name: "Robot Voiture", class: "type-voiture" }
};

const THRESHOLDS = {
    TEMP: 60,
    CPU: 70,
    RAM: 50,
    GPU: 75,
    BAT: 15
};

const METRIC_STYLES = {
    temp:  { label: 'Température (°C)', color: '#ff4d4d', min: 20, max: 90 },
    cpu:   { label: 'Usage CPU (%)', color: '#00adb5', min: 0, max: 100 },
    ram:   { label: 'Usage RAM (%)', color: '#9b59b6', min: 0, max: 100 },
    gpu:   { label: 'Usage GPU (%)', color: '#f1c40f', min: 0, max: 100 },
    proc:  { label: 'Nb Processus', color: '#3498db', min: 0, max: 500 },
    bat:   { label: 'Batterie (%)', color: '#2ecc71', min: 0, max: 100 },
    dist:  { label: 'Distance (cm)', color: '#e67e22', min: 0, max: 400 },
    pince: { label: 'Pince (0=Ouv, 1=Fer)', color: '#95a5a6', min: 0, max: 1 }
};

async function fetchData() {
    try {
        const response = await fetch(API_URL);
        if (!response.ok) throw new Error(`Erreur HTTP: ${response.status}`);
        const data = await response.json();
        updateStatus("En direct", "#00ff00");
        renderDashboard(data);
    } catch (error) {
        console.error("Erreur fetch:", error);
        updateStatus("Erreur : " + error.message, "#ff4d4d");
    }
}

function renderDashboard(clients) {
    const container = document.getElementById('dashboard');
    const activeIds = clients.map(c => `client-${c.id}`);

    document.querySelectorAll('.card').forEach(card => {
        if (!activeIds.includes(card.id)) {
            card.remove();
            delete clientStates[card.id];
        }
    });

    clients.forEach(client => {
        const cardId = `client-${client.id}`;
        let card = document.getElementById(cardId);
        const config = TYPES_CONFIG[client.type] || { name: "Inconnu", class: "" };

        if (!card) {
            card = document.createElement('div');
            card.id = cardId;
            card.className = `card ${config.class}`;
            card.innerHTML = `
                <h3>${client.nom} <small style="font-size:0.7em; opacity:0.6;">${config.name}</small></h3>
                <div class="content"></div>
                <canvas id="chart-${cardId}"></canvas>
            `;
            container.appendChild(card);
            
            clientStates[cardId] = {
                chart: initChart(cardId),
                activeMetric: 'temp',
                history: {}
            };
        }

        const isAlert = client.temp > THRESHOLDS.TEMP || 
                        client.cpu > THRESHOLDS.CPU || 
                        client.ram > THRESHOLDS.RAM ||
                        (client.gpu !== undefined && client.gpu > THRESHOLDS.GPU) ||
                        (client.bat !== undefined && client.bat < THRESHOLDS.BAT);

        card.className = `card ${config.class} ${isAlert ? 'alert-mode' : ''}`;
        
        let html = `
            <div class="data-row clickable" onclick="switchMetric('${cardId}', 'cpu')">
                <span>CPU</span><span class="${client.cpu > THRESHOLDS.CPU ? 'warning-text' : ''}">${client.cpu}%</span>
            </div>
            <div class="data-row clickable" onclick="switchMetric('${cardId}', 'ram')">
                <span>RAM</span><span class="${client.ram > THRESHOLDS.RAM ? 'warning-text' : ''}">${client.ram}%</span>
            </div>
            <div class="data-row clickable" onclick="switchMetric('${cardId}', 'temp')">
                <span>Temp.</span><span class="${client.temp > THRESHOLDS.TEMP ? 'warning-text' : ''}">${client.temp}°C</span>
            </div>
        `;

        if (client.proc !== undefined) {
            html += `
            <div class="data-row clickable" onclick="switchMetric('${cardId}', 'proc')">
                <span>Processus</span><span>${client.proc}</span>
            </div>
            <div class="data-row clickable" onclick="switchMetric('${cardId}', 'gpu')">
                <span>GPU</span><span class="${client.gpu > THRESHOLDS.GPU ? 'warning-text' : ''}">${client.gpu}%</span>
            </div>`;
        } 
        else if (client.bat !== undefined) {
            html += `
            <div class="data-row clickable" onclick="switchMetric('${cardId}', 'bat')">
                <span>Batterie</span><span class="${client.bat < THRESHOLDS.BAT ? 'warning-text' : ''}">${client.bat}%</span>
            </div>
            <div class="data-row clickable" onclick="switchMetric('${cardId}', 'dist')">
                <span>Distance</span><span>${client.dist} cm</span>
            </div>`;
        }
        else if (client.pince !== undefined) {
            html += `
            <div class="data-row clickable" onclick="switchMetric('${cardId}', 'pince')">
                <span>Pince</span><span>${client.pince ? 'Fermée' : 'Ouverte'}</span>
            </div>
            <div class="data-row">
                <span>Servos</span><span style="font-size:0.8em; color:var(--accent-color)">[${client.servos.join(',')}]</span>
            </div>`;
        }

        card.querySelector('.content').innerHTML = html;
        updateData(cardId, client);
    });
}

function initChart(cardId) {
    const ctx = document.getElementById(`chart-${cardId}`).getContext('2d');
    return new Chart(ctx, {
        type: 'line',
        data: {
            labels: Array(MAX_HISTORY).fill(''),
            datasets: [{ data: [], borderWidth: 2, pointRadius: 2, fill: true, tension: 0.4 }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false, // Permet de suivre la hauteur CSS
            scales: { 
                y: { 
                    display: true, 
                    beginAtZero: false,
                    grid: { color: '#3d3d3d' }, 
                    ticks: { color: '#aaa', font: { size: 10 } } 
                }, 
                x: { display: false } 
            },
            plugins: { 
                legend: { 
                    display: true, 
                    position: 'top',
                    labels: { color: '#eee', font: { size: 11 }, boxWidth: 12, padding: 10 } 
                } 
            }
        }
    });
}

function updateData(cardId, client) {
    const state = clientStates[cardId];
    const metricsToTrack = ['cpu', 'ram', 'temp', 'gpu', 'proc', 'bat', 'dist', 'pince'];
    
    metricsToTrack.forEach(m => {
        if (client[m] !== undefined) {
            if (!state.history[m]) state.history[m] = [];
            state.history[m].push(client[m]);
            if (state.history[m].length > MAX_HISTORY) state.history[m].shift();
        }
    });

    const metric = state.activeMetric;
    const style = METRIC_STYLES[metric];
    
    if (state.history[metric]) {
        state.chart.data.datasets[0].label = style.label;
        state.chart.data.datasets[0].data = state.history[metric];
        state.chart.data.datasets[0].borderColor = style.color;
        state.chart.data.datasets[0].backgroundColor = style.color + '33';
        state.chart.options.scales.y.min = style.min;
        state.chart.options.scales.y.max = style.max;
        state.chart.update('none');
    }
}

function switchMetric(cardId, metric) {
    if (clientStates[cardId]) {
        clientStates[cardId].activeMetric = metric;
    }
}

function updateStatus(text, color) {
    const el = document.getElementById('connection-status');
    if (el) { el.innerText = text; el.style.color = color; }
}

setInterval(fetchData, REFRESH_RATE);
fetchData();