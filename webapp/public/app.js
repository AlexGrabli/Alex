// Глобальные переменные
let ws;
let charts = {};
let currentPeriod = 'day';

// WebSocket подключение
function connectWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}`;

    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
        console.log('WebSocket подключен');
        updateConnectionStatus('online');
    };

    ws.onmessage = (event) => {
        const message = JSON.parse(event.data);

        if (message.type === 'init' || message.type === 'update') {
            updateCurrentValues(message.data);
            updateLastUpdateTime();
        }
    };

    ws.onerror = (error) => {
        console.error('WebSocket ошибка:', error);
        updateConnectionStatus('offline');
    };

    ws.onclose = () => {
        console.log('WebSocket отключен');
        updateConnectionStatus('offline');

        // Переподключение через 5 секунд
        setTimeout(connectWebSocket, 5000);
    };
}

// Обновление статуса подключения
function updateConnectionStatus(status) {
    const statusElement = document.getElementById('connectionStatus');

    if (status === 'online') {
        statusElement.textContent = 'Онлайн';
        statusElement.className = 'status-value status-online';
    } else if (status === 'offline') {
        statusElement.textContent = 'Отключено';
        statusElement.className = 'status-value status-offline';
    } else {
        statusElement.textContent = 'Подключение...';
        statusElement.className = 'status-value status-connecting';
    }
}

// Обновление времени последнего обновления
function updateLastUpdateTime() {
    const lastUpdateElement = document.getElementById('lastUpdate');
    const now = new Date();
    lastUpdateElement.textContent = now.toLocaleString('ru-RU');
}

// Обновление текущих значений
function updateCurrentValues(data) {
    const fields = {
        'currentTair': data.tair,
        'currentRH': data.rh,
        'currentPressure': data.pressure,
        'currentPAR': data.par,
        'currentNDVI': data.ndvi,
        'currentMSAVI': data.msavi,
        'currentDistance': data.distance
    };

    for (const [id, value] of Object.entries(fields)) {
        const element = document.getElementById(id);
        if (element) {
            if (value !== null && value !== undefined) {
                if (id === 'currentDistance') {
                    element.textContent = value.toFixed(0);
                } else if (id === 'currentNDVI' || id === 'currentMSAVI') {
                    element.textContent = value.toFixed(3);
                } else {
                    element.textContent = value.toFixed(2);
                }
            } else {
                element.textContent = '-';
            }
        }
    }
}

// Инициализация графиков
function initCharts() {
    const commonOptions = {
        responsive: true,
        maintainAspectRatio: true,
        plugins: {
            legend: {
                display: true,
                position: 'top'
            }
        },
        scales: {
            x: {
                type: 'time',
                time: {
                    unit: 'hour',
                    displayFormats: {
                        hour: 'HH:mm',
                        day: 'dd.MM'
                    }
                },
                title: {
                    display: true,
                    text: 'Время'
                }
            }
        }
    };

    // График температуры и влажности воздуха
    charts.air = new Chart(document.getElementById('airChart'), {
        type: 'line',
        data: {
            datasets: [
                {
                    label: 'Температура воздуха (°C)',
                    borderColor: 'rgb(255, 99, 132)',
                    backgroundColor: 'rgba(255, 99, 132, 0.1)',
                    yAxisID: 'y',
                    data: []
                },
                {
                    label: 'Влажность воздуха (%)',
                    borderColor: 'rgb(54, 162, 235)',
                    backgroundColor: 'rgba(54, 162, 235, 0.1)',
                    yAxisID: 'y1',
                    data: []
                }
            ]
        },
        options: {
            ...commonOptions,
            scales: {
                ...commonOptions.scales,
                y: {
                    type: 'linear',
                    display: true,
                    position: 'left',
                    title: {
                        display: true,
                        text: 'Температура (°C)'
                    }
                },
                y1: {
                    type: 'linear',
                    display: true,
                    position: 'right',
                    title: {
                        display: true,
                        text: 'Влажность (%)'
                    },
                    grid: {
                        drawOnChartArea: false
                    }
                }
            }
        }
    });

    // График давления
    charts.pressure = new Chart(document.getElementById('pressureChart'), {
        type: 'line',
        data: {
            datasets: [{
                label: 'Давление (кПа)',
                borderColor: 'rgb(75, 192, 192)',
                backgroundColor: 'rgba(75, 192, 192, 0.1)',
                data: []
            }]
        },
        options: {
            ...commonOptions,
            scales: {
                ...commonOptions.scales,
                y: {
                    title: {
                        display: true,
                        text: 'Давление (кПа)'
                    }
                }
            }
        }
    });

    // График ФАР
    charts.par = new Chart(document.getElementById('parChart'), {
        type: 'line',
        data: {
            datasets: [{
                label: 'ФАР (мкмоль/м²/с)',
                borderColor: 'rgb(255, 206, 86)',
                backgroundColor: 'rgba(255, 206, 86, 0.1)',
                data: []
            }]
        },
        options: {
            ...commonOptions,
            scales: {
                ...commonOptions.scales,
                y: {
                    title: {
                        display: true,
                        text: 'ФАР (мкмоль/м²/с)'
                    }
                }
            }
        }
    });

    // График вегетационных индексов
    charts.vegetation = new Chart(document.getElementById('vegetationChart'), {
        type: 'line',
        data: {
            datasets: [
                {
                    label: 'NDVI',
                    borderColor: 'rgb(46, 204, 113)',
                    backgroundColor: 'rgba(46, 204, 113, 0.1)',
                    data: []
                },
                {
                    label: 'MSAVI',
                    borderColor: 'rgb(155, 89, 182)',
                    backgroundColor: 'rgba(155, 89, 182, 0.1)',
                    data: []
                }
            ]
        },
        options: {
            ...commonOptions,
            scales: {
                ...commonOptions.scales,
                y: {
                    title: {
                        display: true,
                        text: 'Индекс'
                    },
                    min: -1,
                    max: 1
                }
            }
        }
    });

    // График температуры почвы
    charts.soilTemp = new Chart(document.getElementById('soilTempChart'), {
        type: 'line',
        data: {
            datasets: [
                {
                    label: 'Датчик 1 (°C)',
                    borderColor: 'rgb(231, 76, 60)',
                    backgroundColor: 'rgba(231, 76, 60, 0.1)',
                    data: []
                },
                {
                    label: 'Датчик 2 (°C)',
                    borderColor: 'rgb(230, 126, 34)',
                    backgroundColor: 'rgba(230, 126, 34, 0.1)',
                    data: []
                },
                {
                    label: 'Датчик 3 (°C)',
                    borderColor: 'rgb(241, 196, 15)',
                    backgroundColor: 'rgba(241, 196, 15, 0.1)',
                    data: []
                }
            ]
        },
        options: {
            ...commonOptions,
            scales: {
                ...commonOptions.scales,
                y: {
                    title: {
                        display: true,
                        text: 'Температура почвы (°C)'
                    }
                }
            }
        }
    });

    // График влажности почвы
    charts.soilMoisture = new Chart(document.getElementById('soilMoistureChart'), {
        type: 'line',
        data: {
            datasets: [
                {
                    label: 'Датчик 1 (%)',
                    borderColor: 'rgb(52, 152, 219)',
                    backgroundColor: 'rgba(52, 152, 219, 0.1)',
                    data: []
                },
                {
                    label: 'Датчик 2 (%)',
                    borderColor: 'rgb(41, 128, 185)',
                    backgroundColor: 'rgba(41, 128, 185, 0.1)',
                    data: []
                },
                {
                    label: 'Датчик 3 (%)',
                    borderColor: 'rgb(26, 188, 156)',
                    backgroundColor: 'rgba(26, 188, 156, 0.1)',
                    data: []
                }
            ]
        },
        options: {
            ...commonOptions,
            scales: {
                ...commonOptions.scales,
                y: {
                    title: {
                        display: true,
                        text: 'Влажность почвы (%)'
                    }
                }
            }
        }
    });

    // График высоты растений
    charts.plantHeight = new Chart(document.getElementById('plantHeightChart'), {
        type: 'line',
        data: {
            datasets: [{
                label: 'Высота растений (мм)',
                borderColor: 'rgb(46, 204, 113)',
                backgroundColor: 'rgba(46, 204, 113, 0.1)',
                data: []
            }]
        },
        options: {
            ...commonOptions,
            scales: {
                ...commonOptions.scales,
                y: {
                    title: {
                        display: true,
                        text: 'Высота (мм)'
                    }
                }
            }
        }
    });
}

// Загрузка исторических данных
async function loadHistoryData(period) {
    try {
        const response = await fetch(`/api/history?period=${period}`);
        const data = await response.json();

        updateCharts(data);
    } catch (error) {
        console.error('Ошибка загрузки данных:', error);
    }
}

// Обновление графиков
function updateCharts(data) {
    if (!data || data.length === 0) return;

    // Температура и влажность воздуха
    charts.air.data.datasets[0].data = data.map(d => ({
        x: d.timestamp,
        y: d.tair
    }));
    charts.air.data.datasets[1].data = data.map(d => ({
        x: d.timestamp,
        y: d.rh
    }));
    charts.air.update();

    // Давление
    charts.pressure.data.datasets[0].data = data.map(d => ({
        x: d.timestamp,
        y: d.pressure
    }));
    charts.pressure.update();

    // ФАР
    charts.par.data.datasets[0].data = data.map(d => ({
        x: d.timestamp,
        y: d.par
    }));
    charts.par.update();

    // Вегетационные индексы
    charts.vegetation.data.datasets[0].data = data.map(d => ({
        x: d.timestamp,
        y: d.ndvi
    }));
    charts.vegetation.data.datasets[1].data = data.map(d => ({
        x: d.timestamp,
        y: d.msavi
    }));
    charts.vegetation.update();

    // Температура почвы
    charts.soilTemp.data.datasets[0].data = data.map(d => ({
        x: d.timestamp,
        y: d.tsoil1
    }));
    charts.soilTemp.data.datasets[1].data = data.map(d => ({
        x: d.timestamp,
        y: d.tsoil2
    }));
    charts.soilTemp.data.datasets[2].data = data.map(d => ({
        x: d.timestamp,
        y: d.tsoil3
    }));
    charts.soilTemp.update();

    // Влажность почвы
    charts.soilMoisture.data.datasets[0].data = data.map(d => ({
        x: d.timestamp,
        y: d.soilwc1
    }));
    charts.soilMoisture.data.datasets[1].data = data.map(d => ({
        x: d.timestamp,
        y: d.soilwc2
    }));
    charts.soilMoisture.data.datasets[2].data = data.map(d => ({
        x: d.timestamp,
        y: d.soilwc3
    }));
    charts.soilMoisture.update();

    // Высота растений
    charts.plantHeight.data.datasets[0].data = data.map(d => ({
        x: d.timestamp,
        y: d.distance
    }));
    charts.plantHeight.update();
}

// Экспорт данных
function exportData(format) {
    const period = document.getElementById('periodSelect').value;
    window.location.href = `/api/export?period=${period}&format=${format}`;
}

// Инициализация при загрузке страницы
document.addEventListener('DOMContentLoaded', () => {
    // Подключение WebSocket
    connectWebSocket();

    // Инициализация графиков
    initCharts();

    // Загрузка исторических данных
    loadHistoryData(currentPeriod);

    // Обработчик изменения периода
    document.getElementById('periodSelect').addEventListener('change', (e) => {
        currentPeriod = e.target.value;
        loadHistoryData(currentPeriod);
    });

    // Обработчики экспорта
    document.getElementById('exportCSV').addEventListener('click', () => {
        exportData('csv');
    });

    document.getElementById('exportTXT').addEventListener('click', () => {
        exportData('txt');
    });

    // Автоматическое обновление графиков каждые 5 минут
    setInterval(() => {
        loadHistoryData(currentPeriod);
    }, 5 * 60 * 1000);

    console.log('CropTalker V2.0 инициализирован');
});
