let tempChart, humChart;
let currentTempRange = 'day';
let currentHumRange = 'day';

function initCharts() {
    const commonOptions = {
        responsive: true,
        maintainAspectRatio: false,
        scales: {
            x: {
                ticks: {
                    maxRotation: 45,
                    minRotation: 45
                }
            }
        },
        plugins: {
            tooltip: {
                callbacks: {
                    title: function(context) {
                        return context[0].label;
                    }
                }
            }
        }
    };

    const tempCtx = document.getElementById('tempChart').getContext('2d');
    tempChart = new Chart(tempCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Temperature (°C)',
                data: [],
                borderColor: 'rgb(255, 99, 132)',
                tension: 0.1
            }]
        },
        options: commonOptions
    });

    const humCtx = document.getElementById('humChart').getContext('2d');
    humChart = new Chart(humCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Humidity (%)',
                data: [],
                borderColor: 'rgb(54, 162, 235)',
                tension: 0.1
            }]
        },
        options: commonOptions
    });
}

function updateCharts() {
    fetch('/history?range=' + currentTempRange + '&type=temp')
        .then(response => response.json())
        .then(data => {
            tempChart.data.labels = data.labels;
            tempChart.data.datasets[0].data = data.values;
            tempChart.update();
        });

    fetch('/history?range=' + currentHumRange + '&type=hum')
        .then(response => response.json())
        .then(data => {
            humChart.data.labels = data.labels;
            humChart.data.datasets[0].data = data.values;
            humChart.update();
        });
}

function changeTimeRange(chartId, range) {
    if (chartId === 'tempChart') {
        currentTempRange = range;
    } else {
        currentHumRange = range;
    }
    updateCharts();
}

function updateFan(action) {
    fetch('/fan-control?action=' + action, { method: 'POST' })
        .then(response => {
            if (response.ok) {
                updateStatus();
            }
        });
}

function updateStatus() {
    fetch('/data')
        .then(response => response.json())
        .then(data => {
            document.getElementById('temperature').textContent = data.temperature.toFixed(1);
            document.getElementById('humidity').textContent = data.humidity.toFixed(1);
            document.getElementById('fogStatus').textContent = data.fogState ? 'ON' : 'OFF';
            document.getElementById('fanPower').textContent = data.fanState ? 'ON' : 'OFF';
            document.getElementById('fanMode').textContent = data.fanManual ? 'Manual' : 'Auto';
            
            document.querySelector('input[name="setpoint"]').value = data.setpoint.toFixed(1);
            
            const fanBtn = document.getElementById('fanPowerBtn');
            if (data.fanState) {
                fanBtn.classList.add('active');
            } else {
                fanBtn.classList.remove('active');
            }
        });
}

function confirmDelete() {
    if (confirm('Are you sure you want to delete all historical data? This action cannot be undone.')) {
        fetch('/delete-data', { method: 'POST' })
            .then(response => {
                if (response.ok) {
                    alert('Data deleted successfully');
                    updateCharts();
                } else {
                    alert('Error deleting data');
                }
            });
    }
}

// Initialize when document is ready
document.addEventListener('DOMContentLoaded', function() {
    initCharts();
    updateCharts();
    
    // Update current values every 5 seconds
    setInterval(updateStatus, 5000);

    // Update charts every minute
    setInterval(updateCharts, 60000);

    // Update current time every second
    setInterval(function() {
        const now = new Date();
        document.getElementById('currentTime').textContent = 
            now.toLocaleString('es-CO', { 
                year: 'numeric', 
                month: '2-digit', 
                day: '2-digit',
                hour: '2-digit', 
                minute: '2-digit',
                second: '2-digit'
            });
    }, 1000);
}); 