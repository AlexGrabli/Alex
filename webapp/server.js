const express = require('express');
const mqtt = require('mqtt');
const Database = require('better-sqlite3');
const WebSocket = require('ws');
const cors = require('cors');
const path = require('path');
const fs = require('fs');

const app = express();
const PORT = process.env.PORT || 3000;

// Middleware
app.use(cors());
app.use(express.json());
app.use(express.static('public'));

// Инициализация базы данных
const dbDir = path.join(__dirname, 'data');
if (!fs.existsSync(dbDir)) {
    fs.mkdirSync(dbDir, { recursive: true });
}

const db = new Database(path.join(dbDir, 'sensor_data.db'));

// Создание таблиц
db.exec(`
    CREATE TABLE IF NOT EXISTS sensor_data (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        timestamp INTEGER NOT NULL,
        tair REAL,
        rh REAL,
        pressure REAL,
        par REAL,
        ndvi REAL,
        msavi REAL,
        distance REAL,
        tsoil1 REAL,
        tsoil2 REAL,
        tsoil3 REAL,
        soilwc1 REAL,
        soilwc2 REAL,
        soilwc3 REAL
    );
    CREATE INDEX IF NOT EXISTS idx_timestamp ON sensor_data(timestamp);
`);

// Подготовка SQL запросов
const insertData = db.prepare(`
    INSERT INTO sensor_data
    (timestamp, tair, rh, pressure, par, ndvi, msavi, distance,
     tsoil1, tsoil2, tsoil3, soilwc1, soilwc2, soilwc3)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
`);

// Хранилище последних данных
let latestData = {
    timestamp: Date.now(),
    tair: null,
    rh: null,
    pressure: null,
    par: null,
    ndvi: null,
    msavi: null,
    distance: null,
    tsoil1: null,
    tsoil2: null,
    tsoil3: null,
    soilwc1: null,
    soilwc2: null,
    soilwc3: null
};

// Настройка MQTT клиента
const MQTT_BROKER = process.env.MQTT_BROKER || 'mqtt://94.154.11.74:1884';
const mqttClient = mqtt.connect(MQTT_BROKER);

mqttClient.on('connect', () => {
    console.log('✓ Подключено к MQTT брокеру');

    // Подписка на все топики датчиков
    const topics = [
        'CropTalkerData/Tair',
        'CropTalkerData/RH',
        'CropTalkerData/Pressure',
        'CropTalkerData/PAR',
        'CropTalkerData/NDVI',
        'CropTalkerData/MSAVI',
        'CropTalkerData/Distance',
        'CropTalkerData/Tsoil1',
        'CropTalkerData/Tsoil2',
        'CropTalkerData/Tsoil3',
        'CropTalkerData/SoilWC1',
        'CropTalkerData/SoilWC2',
        'CropTalkerData/SoilWC3'
    ];

    topics.forEach(topic => {
        mqttClient.subscribe(topic, (err) => {
            if (!err) {
                console.log(`  Подписка на ${topic}`);
            } else {
                console.error(`  Ошибка подписки на ${topic}:`, err);
            }
        });
    });
});

mqttClient.on('error', (error) => {
    console.error('Ошибка MQTT:', error);
});

// Обработка входящих MQTT сообщений
mqttClient.on('message', (topic, message) => {
    const value = parseFloat(message.toString());
    const timestamp = Date.now();

    // Обновление последних данных
    switch(topic) {
        case 'CropTalkerData/Tair':
            latestData.tair = value;
            break;
        case 'CropTalkerData/RH':
            latestData.rh = value;
            break;
        case 'CropTalkerData/Pressure':
            latestData.pressure = value;
            break;
        case 'CropTalkerData/PAR':
            latestData.par = value;
            break;
        case 'CropTalkerData/NDVI':
            latestData.ndvi = value;
            break;
        case 'CropTalkerData/MSAVI':
            latestData.msavi = value;
            break;
        case 'CropTalkerData/Distance':
            latestData.distance = value;
            break;
        case 'CropTalkerData/Tsoil1':
            latestData.tsoil1 = value;
            break;
        case 'CropTalkerData/Tsoil2':
            latestData.tsoil2 = value;
            break;
        case 'CropTalkerData/Tsoil3':
            latestData.tsoil3 = value;
            break;
        case 'CropTalkerData/SoilWC1':
            latestData.soilwc1 = value;
            break;
        case 'CropTalkerData/SoilWC2':
            latestData.soilwc2 = value;
            break;
        case 'CropTalkerData/SoilWC3':
            latestData.soilwc3 = value;
            break;
    }

    latestData.timestamp = timestamp;

    // Отправка данных всем подключенным WebSocket клиентам
    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(JSON.stringify({
                type: 'update',
                data: latestData
            }));
        }
    });
});

// Периодическое сохранение данных в БД (каждые 5 минут)
setInterval(() => {
    try {
        insertData.run(
            latestData.timestamp,
            latestData.tair,
            latestData.rh,
            latestData.pressure,
            latestData.par,
            latestData.ndvi,
            latestData.msavi,
            latestData.distance,
            latestData.tsoil1,
            latestData.tsoil2,
            latestData.tsoil3,
            latestData.soilwc1,
            latestData.soilwc2,
            latestData.soilwc3
        );
        console.log('✓ Данные сохранены в БД');
    } catch (err) {
        console.error('Ошибка сохранения в БД:', err);
    }
}, 5 * 60 * 1000); // 5 минут

// API эндпоинты

// Получение последних данных
app.get('/api/latest', (req, res) => {
    res.json(latestData);
});

// Получение исторических данных
app.get('/api/history', (req, res) => {
    const { period = 'day' } = req.query;
    let startTime;
    const now = Date.now();

    switch(period) {
        case 'hour':
            startTime = now - (60 * 60 * 1000);
            break;
        case 'day':
            startTime = now - (24 * 60 * 60 * 1000);
            break;
        case 'week':
            startTime = now - (7 * 24 * 60 * 60 * 1000);
            break;
        case 'month':
            startTime = now - (30 * 24 * 60 * 60 * 1000);
            break;
        case 'all':
            startTime = 0;
            break;
        default:
            startTime = now - (24 * 60 * 60 * 1000);
    }

    try {
        const stmt = db.prepare(`
            SELECT * FROM sensor_data
            WHERE timestamp >= ?
            ORDER BY timestamp ASC
        `);
        const data = stmt.all(startTime);
        res.json(data);
    } catch (err) {
        console.error('Ошибка получения данных:', err);
        res.status(500).json({ error: 'Ошибка получения данных' });
    }
});

// Экспорт данных в CSV
app.get('/api/export', (req, res) => {
    const { period = 'all', format = 'csv' } = req.query;
    let startTime;
    const now = Date.now();

    switch(period) {
        case 'hour':
            startTime = now - (60 * 60 * 1000);
            break;
        case 'day':
            startTime = now - (24 * 60 * 60 * 1000);
            break;
        case 'week':
            startTime = now - (7 * 24 * 60 * 60 * 1000);
            break;
        case 'month':
            startTime = now - (30 * 24 * 60 * 60 * 1000);
            break;
        case 'all':
            startTime = 0;
            break;
        default:
            startTime = 0;
    }

    try {
        const stmt = db.prepare(`
            SELECT * FROM sensor_data
            WHERE timestamp >= ?
            ORDER BY timestamp ASC
        `);
        const data = stmt.all(startTime);

        if (format === 'csv') {
            // Формирование CSV
            const headers = [
                'Дата и время', 'Температура воздуха (°C)', 'Влажность воздуха (%)',
                'Давление (кПа)', 'ФАР (мкмоль/м²/с)', 'NDVI', 'MSAVI',
                'Высота растений (мм)', 'Т почвы 1 (°C)', 'Т почвы 2 (°C)',
                'Т почвы 3 (°C)', 'Влажность почвы 1 (%)', 'Влажность почвы 2 (%)',
                'Влажность почвы 3 (%)'
            ];

            let csv = headers.join(';') + '\n';

            data.forEach(row => {
                const date = new Date(row.timestamp).toLocaleString('ru-RU');
                csv += [
                    date,
                    row.tair !== null ? row.tair.toFixed(2) : '',
                    row.rh !== null ? row.rh.toFixed(2) : '',
                    row.pressure !== null ? row.pressure.toFixed(2) : '',
                    row.par !== null ? row.par.toFixed(2) : '',
                    row.ndvi !== null ? row.ndvi.toFixed(3) : '',
                    row.msavi !== null ? row.msavi.toFixed(3) : '',
                    row.distance !== null ? row.distance.toFixed(0) : '',
                    row.tsoil1 !== null ? row.tsoil1.toFixed(2) : '',
                    row.tsoil2 !== null ? row.tsoil2.toFixed(2) : '',
                    row.tsoil3 !== null ? row.tsoil3.toFixed(2) : '',
                    row.soilwc1 !== null ? row.soilwc1.toFixed(2) : '',
                    row.soilwc2 !== null ? row.soilwc2.toFixed(2) : '',
                    row.soilwc3 !== null ? row.soilwc3.toFixed(2) : ''
                ].join(';') + '\n';
            });

            res.setHeader('Content-Type', 'text/csv; charset=utf-8');
            res.setHeader('Content-Disposition', `attachment; filename=croptalker_data_${period}.csv`);
            res.send('\uFEFF' + csv); // BOM для корректной кодировки в Excel
        } else {
            // TXT формат
            let txt = 'Данные мониторинга CropTalker V2.0\n';
            txt += '='.repeat(80) + '\n\n';

            data.forEach(row => {
                const date = new Date(row.timestamp).toLocaleString('ru-RU');
                txt += `Дата и время: ${date}\n`;
                txt += `  Температура воздуха: ${row.tair !== null ? row.tair.toFixed(2) : 'н/д'} °C\n`;
                txt += `  Влажность воздуха: ${row.rh !== null ? row.rh.toFixed(2) : 'н/д'} %\n`;
                txt += `  Давление: ${row.pressure !== null ? row.pressure.toFixed(2) : 'н/д'} кПа\n`;
                txt += `  ФАР: ${row.par !== null ? row.par.toFixed(2) : 'н/д'} мкмоль/м²/с\n`;
                txt += `  NDVI: ${row.ndvi !== null ? row.ndvi.toFixed(3) : 'н/д'}\n`;
                txt += `  MSAVI: ${row.msavi !== null ? row.msavi.toFixed(3) : 'н/д'}\n`;
                txt += `  Высота растений: ${row.distance !== null ? row.distance.toFixed(0) : 'н/д'} мм\n`;
                txt += `  Температура почвы: ${row.tsoil1?.toFixed(2) || 'н/д'} / ${row.tsoil2?.toFixed(2) || 'н/д'} / ${row.tsoil3?.toFixed(2) || 'н/д'} °C\n`;
                txt += `  Влажность почвы: ${row.soilwc1?.toFixed(2) || 'н/д'} / ${row.soilwc2?.toFixed(2) || 'н/д'} / ${row.soilwc3?.toFixed(2) || 'н/д'} %\n`;
                txt += '-'.repeat(80) + '\n';
            });

            res.setHeader('Content-Type', 'text/plain; charset=utf-8');
            res.setHeader('Content-Disposition', `attachment; filename=croptalker_data_${period}.txt`);
            res.send(txt);
        }
    } catch (err) {
        console.error('Ошибка экспорта данных:', err);
        res.status(500).json({ error: 'Ошибка экспорта данных' });
    }
});

// Запуск HTTP сервера
const server = app.listen(PORT, () => {
    console.log(`\n╔════════════════════════════════════════════════════╗`);
    console.log(`║   CropTalker V2.0 - Система мониторинга датчиков  ║`);
    console.log(`╠════════════════════════════════════════════════════╣`);
    console.log(`║  Сервер запущен: http://localhost:${PORT}         ║`);
    console.log(`║  MQTT брокер: ${MQTT_BROKER.padEnd(32)} ║`);
    console.log(`╚════════════════════════════════════════════════════╝\n`);
});

// Настройка WebSocket сервера
const wss = new WebSocket.Server({ server });

wss.on('connection', (ws) => {
    console.log('✓ Новое WebSocket подключение');

    // Отправка последних данных при подключении
    ws.send(JSON.stringify({
        type: 'init',
        data: latestData
    }));

    ws.on('close', () => {
        console.log('✗ WebSocket отключение');
    });
});

// Graceful shutdown
process.on('SIGINT', () => {
    console.log('\nЗакрытие приложения...');
    mqttClient.end();
    db.close();
    server.close(() => {
        console.log('Сервер остановлен');
        process.exit(0);
    });
});
