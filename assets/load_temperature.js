let timestamps = [];
let temperatures = [];
let temperatureChart;

function downsampleData(timestamps, temperatures, maxPoints = 300) {
    if (timestamps.length <= maxPoints) {
        return { timestamps, temperatures };
    }
    
    const result = {
        timestamps: [],
        temperatures: []
    };
    
    const skipFactor = Math.ceil(timestamps.length / maxPoints);
    
    result.timestamps.push(timestamps[0]);
    result.temperatures.push(temperatures[0]);
    
    for (let i = skipFactor; i < timestamps.length - skipFactor; i += skipFactor) {
        let minTemp = temperatures[i];
        let maxTemp = temperatures[i];
        let minIndex = i;
        let maxIndex = i;
        
        for (let j = i; j < i + skipFactor && j < timestamps.length; j++) {
            if (temperatures[j] < minTemp) {
                minTemp = temperatures[j];
                minIndex = j;
            }
            if (temperatures[j] > maxTemp) {
                maxTemp = temperatures[j];
                maxIndex = j;
            }
        }
        
        if (minIndex !== maxIndex) {
            if (minIndex < maxIndex) {
                result.timestamps.push(timestamps[minIndex], timestamps[maxIndex]);
                result.temperatures.push(temperatures[minIndex], temperatures[maxIndex]);
            } else {
                result.timestamps.push(timestamps[maxIndex], timestamps[minIndex]);
                result.temperatures.push(temperatures[maxIndex], temperatures[minIndex]);
            }
        } else {
            result.timestamps.push(timestamps[minIndex]);
            result.temperatures.push(temperatures[minIndex]);
        }
    }
    
    if (result.timestamps[result.timestamps.length - 1] !== timestamps[timestamps.length - 1]) {
        result.timestamps.push(timestamps[timestamps.length - 1]);
        result.temperatures.push(temperatures[timestamps.length - 1]);
    }
    
    console.log(`Downsampled from ${timestamps.length} to ${result.timestamps.length} points`);
    return result;
}

function createChart() {
    const ctx = document.getElementById('temperatureChart').getContext('2d');
    let chartData = downsampleData([...timestamps].reverse(), [...temperatures].reverse());
    
    temperatureChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: chartData.timestamps,
            datasets: [{
                label: 'Temperature (°C)',
                data: chartData.temperatures,
                borderColor: '#007bff',
                backgroundColor: 'rgba(0, 123, 255, 0.1)',
                borderWidth: 2,
                tension: 0.1,
                fill: true,
                pointRadius: 0,
                pointHoverRadius: 0
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                y: {
                    beginAtZero: false,
                    title: {
                        display: true,
                        text: 'Temperature (°C)'
                    }
                },
                x: {
                    title: {
                        display: true,
                        text: 'Time'
                    },
                    ticks: {
                        maxRotation: 45,
                        minRotation: 45
                    }
                }
            },
            plugins: {
                title: {
                    display: true,
                    text: chartPeriod + ' Temperature Readings'
                },
                tooltip: {
                    callbacks: {
                        label: function(context) {
                            return `Temperature: ${context.parsed.y.toFixed(2)}°C`;
                        }
                    },
                    intersect: false,
                    mode: 'index'
                }
            }
        }
    });
}

let updateInterval = 0;
let updateTimer = null;
const updateStatusElement = document.getElementById('update-status');
const updateIntervalSelect = document.getElementById('update-interval');

function setupUpdateInterval(interval) {
    updateInterval = interval;
    
    if (updateTimer) {
        clearInterval(updateTimer);
        updateTimer = null;
    }
    
    if (updateInterval > 0) {
        updateStatusElement.textContent = `Auto-update: ${updateInterval}s`;
        updateStatusElement.classList.add('active');
        updateTimer = setInterval(fetchLatestData, updateInterval * 1000);
    } else {
        updateStatusElement.textContent = 'Auto-update: OFF';
        updateStatusElement.classList.remove('active');
    }
}

if (localStorage.getItem('temperatureUpdateInterval')) {
    updateInterval = parseInt(localStorage.getItem('temperatureUpdateInterval'));
    if (updateIntervalSelect) {
        updateIntervalSelect.value = updateInterval;
    }
    setupUpdateInterval(updateInterval);
}

if (updateIntervalSelect) {
    updateIntervalSelect.addEventListener('change', function() {
        const interval = parseInt(this.value);
        localStorage.setItem('temperatureUpdateInterval', interval);
        setupUpdateInterval(interval);
    });
}

function updateTable(readings) {
    const tbody = document.getElementById('readings-body');
    tbody.innerHTML = '';
    
    readings.forEach(reading => {
        const row = document.createElement('tr');
        
        const timestampCell = document.createElement('td');
        timestampCell.textContent = reading.timestamp;
        row.appendChild(timestampCell);
        
        const tempCell = document.createElement('td');
        tempCell.className = 'temperature';
        tempCell.textContent = reading.temperature.toFixed(2) + '°C';
        row.appendChild(tempCell);
        
        tbody.appendChild(row);
    });
}

function fetchLatestData() {
    const currentPath = window.location.pathname;
    const apiUrl = `${serviceEndpoint}${currentPath}`;
    fetch(apiUrl, {
        headers: {
            'Accept': 'application/json'
        }
    })
    .then(response => response.json())
    .then(data => {
        if (data.readings && data.readings.length > 0) {
            const newTimestamps = [];
            const newTemperatures = [];
            
            data.readings.forEach(reading => {
                newTimestamps.push(reading.timestamp);
                newTemperatures.push(reading.temperature);
            });
            
            timestamps = newTimestamps;
            temperatures = newTemperatures;
            
            if (!temperatureChart) {
                createChart();
            } else {
                const newChartData = downsampleData([...timestamps].reverse(), [...temperatures].reverse());
                temperatureChart.data.labels = newChartData.timestamps;
                temperatureChart.data.datasets[0].data = newChartData.temperatures;
                temperatureChart.update();
            }
            
            updateTable(data.readings);
            
            const countInfo = document.querySelector('.count-info');
            countInfo.textContent = `Showing ${data.count} temperature readings.`;
        }
    })
    .catch(error => {
        console.error('Error fetching updated data:', error);
    });
}

document.addEventListener('DOMContentLoaded', function() {
    fetchLatestData();
});

fetchLatestData();
