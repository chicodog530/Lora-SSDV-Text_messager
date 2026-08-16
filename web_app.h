#pragma once

const char* WEB_APP_HTML = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>LoRa Transceiver</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;800&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg: #0f172a;
            --surface: #1e293b;
            --primary: #3b82f6;
            --secondary: #10b981;
            --text: #f8fafc;
            --text-muted: #94a3b8;
            --danger: #ef4444;
        }
        body {
            margin: 0;
            background-color: var(--bg);
            color: var(--text);
            font-family: 'Inter', sans-serif;
            display: flex;
            flex-direction: column;
            align-items: center;
            min-height: 100vh;
        }
        .header {
            width: 100%;
            padding: 15px 0;
            text-align: center;
            background: rgba(30, 41, 59, 0.9);
            backdrop-filter: blur(10px);
            border-bottom: 1px solid rgba(255, 255, 255, 0.1);
            position: sticky;
            top: 0;
            z-index: 100;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .header h1 {
            margin: 0 0 0 20px;
            font-size: 20px;
            font-weight: 800;
            background: linear-gradient(135deg, #60a5fa, #3b82f6);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }
        .status-badge {
            margin-right: 20px;
            padding: 4px 12px;
            border-radius: 20px;
            font-size: 12px;
            font-weight: 600;
            background: #334155;
        }
        .status-badge.rx { background: rgba(16, 185, 129, 0.2); color: var(--secondary); }
        .status-badge.tx { background: rgba(59, 130, 246, 0.2); color: var(--primary); }
        
        .container {
            width: 90%;
            max-width: 500px;
            display: flex;
            flex-direction: column;
            gap: 20px;
            padding: 20px 0;
        }
        .card {
            background: var(--surface);
            border-radius: 16px;
            padding: 20px;
            box-shadow: 0 4px 20px rgba(0, 0, 0, 0.2);
            border: 1px solid rgba(255,255,255,0.05);
        }
        .card-title {
            font-size: 16px;
            font-weight: 600;
            margin-top: 0;
            margin-bottom: 15px;
            color: var(--text-muted);
            border-bottom: 1px solid rgba(255,255,255,0.1);
            padding-bottom: 10px;
        }
        .screen {
            background: #000;
            border-radius: 8px;
            min-height: 200px;
            display: flex;
            justify-content: center;
            align-items: center;
            overflow: hidden;
            margin-bottom: 15px;
            position: relative;
        }
        .screen img {
            width: 100%;
            image-rendering: pixelated;
        }
        .placeholder {
            color: #475569;
            font-size: 14px;
        }
        input[type="file"] { display: none; }
        .btn {
            background: var(--primary);
            color: white;
            border: none;
            padding: 14px 20px;
            border-radius: 10px;
            font-size: 15px;
            font-weight: 600;
            cursor: pointer;
            width: 100%;
            text-align: center;
            box-sizing: border-box;
            display: inline-block;
            transition: all 0.2s ease;
        }
        .btn:active { transform: scale(0.98); }
        .btn-outline {
            background: transparent;
            border: 2px solid var(--primary);
            color: var(--primary);
        }
        .progress-bar {
            width: 100%;
            height: 6px;
            background: rgba(255,255,255,0.1);
            border-radius: 3px;
            margin-top: 15px;
            overflow: hidden;
            display: none;
        }
        .progress-fill {
            height: 100%;
            background: var(--primary);
            width: 0%;
            transition: width 0.3s ease;
        }
        .progress-fill.rx { background: var(--secondary); }
        
        .form-group {
            display: flex;
            gap: 10px;
        }
        input[type="text"] {
            flex: 1;
            background: #0f172a;
            border: 1px solid #334155;
            color: white;
            padding: 12px;
            border-radius: 8px;
            font-size: 16px;
        }
    </style>
</head>
<body>

<div class="header">
    <h1>LoRa Chat</h1>
    <div class="status-badge" id="mainStatus">IDLE</div>
</div>

<div class="container">
    <!-- INCOMING -->
    <div class="card">
        <h2 class="card-title">Incoming Photo</h2>
        <div class="screen">
            <span class="placeholder" id="rx-placeholder">No photo received yet</span>
            <img id="rx-img" style="display:none;">
        </div>
        <div class="progress-bar" id="rx-prog-container">
            <div class="progress-fill rx" id="rx-prog"></div>
        </div>
    </div>

    <!-- OUTGOING -->
    <div class="card">
        <h2 class="card-title">Send Photo</h2>
        <div class="screen" style="min-height: 150px;">
            <span class="placeholder" id="tx-placeholder">Preview</span>
            <img id="tx-img" style="display:none;">
        </div>
        
        <label class="btn btn-outline" style="margin-bottom: 15px;">
            Choose Photo
            <input type="file" id="file-input" accept="image/*">
        </label>
        
        <div style="display: flex; justify-content: center; gap: 20px; margin-bottom: 15px;">
            <label style="display: flex; align-items: center; gap: 6px; cursor: pointer;">
                <input type="checkbox" id="grayscale-toggle" style="width: 16px; height: 16px;">
                <span style="font-size: 13px; color: var(--text-muted);">2-Bit Grayscale</span>
            </label>
            <label style="display: flex; align-items: center; gap: 6px; cursor: pointer;">
                <input type="checkbox" id="fast-mode-toggle" style="width: 16px; height: 16px;">
                <span style="font-size: 13px; color: var(--text-muted);">Fast Mode (400x300)</span>
            </label>
        </div>
        
        <button class="btn" id="send-btn" style="display:none;">Send over LoRa</button>

        <div class="progress-bar" id="tx-prog-container">
            <div class="progress-fill" id="tx-prog"></div>
        </div>
    </div>

    <!-- TEXT CHAT -->
    <div class="card">
        <h2 class="card-title">Text Chat</h2>
        <div id="chat-history" style="height: 150px; overflow-y: auto; background: #000; border-radius: 8px; padding: 10px; margin-bottom: 15px; display: flex; flex-direction: column; gap: 8px; font-size: 14px;">
        </div>
        <div class="form-group">
            <input type="text" id="chat-input" placeholder="Type a message..." maxlength="1000">
            <button class="btn" id="send-text-btn" style="width: auto; padding: 12px 20px;">Send</button>
        </div>
    </div>

    <!-- SETTINGS -->
    <div class="card">
        <h2 class="card-title">Device Settings</h2>
        <div style="font-size: 13px; color: var(--text-muted); margin-bottom: 15px;">
            Rename this board's Wi-Fi network. The board will restart immediately after saving.
        </div>
        <div class="form-group">
            <input type="text" id="wifi-name" placeholder="e.g. Node_A">
            <button class="btn" style="width: auto;" onclick="renameWifi()">Save</button>
        </div>
    </div>
</div>

<script>
    const mainStatus = document.getElementById('mainStatus');
    
    // RX Elements
    const rxImg = document.getElementById('rx-img');
    const rxPlaceholder = document.getElementById('rx-placeholder');
    const rxProgContainer = document.getElementById('rx-prog-container');
    const rxProg = document.getElementById('rx-prog');
    
    // TX Elements
    const fileInput = document.getElementById('file-input');
    const grayscaleToggle = document.getElementById('grayscale-toggle');
    const fastModeToggle = document.getElementById('fast-mode-toggle');
    const txImg = document.getElementById('tx-img');
    const txPlaceholder = document.getElementById('tx-placeholder');
    const sendBtn = document.getElementById('send-btn');
    const txProgContainer = document.getElementById('tx-prog-container');
    const txProg = document.getElementById('tx-prog');
    
    // Chat Elements
    const chatInput = document.getElementById('chat-input');
    const sendTextBtn = document.getElementById('send-text-btn');
    const chatHistory = document.getElementById('chat-history');
    
    let compressedBlob = null;
    let finalWidth = 0;
    let finalHeight = 0;
    let lastRxImageId = 0;
    let lastRxTextId = 0;
    let currentImg = null;

    function appendMessage(text, isMe) {
        const div = document.createElement('div');
        div.style.padding = '8px 12px';
        div.style.borderRadius = '12px';
        div.style.maxWidth = '80%';
        div.style.wordBreak = 'break-word';
        if (isMe) {
            div.style.alignSelf = 'flex-end';
            div.style.background = 'var(--primary)';
            div.style.color = '#fff';
        } else {
            div.style.alignSelf = 'flex-start';
            div.style.background = '#334155';
            div.style.color = '#fff';
        }
        div.innerText = text;
        chatHistory.appendChild(div);
        chatHistory.scrollTop = chatHistory.scrollHeight;
    }

    function processImage(img) {
        let width = img.width;
        let height = img.height;
        const isFastMode = fastModeToggle.checked;
        const MAX_W = isFastMode ? 400 : 800;
        const MAX_H = isFastMode ? 300 : 600;
        
        if (width > height) {
            if (width > MAX_W) { height = Math.round(height * (MAX_W / width)); width = MAX_W; }
        } else {
            if (height > MAX_H) { width = Math.round(width * (MAX_H / height)); height = MAX_H; }
        }
        finalWidth = width; finalHeight = height;

        const canvas = document.createElement('canvas');
        canvas.width = width; canvas.height = height;
        const ctx = canvas.getContext('2d');
        ctx.drawImage(img, 0, 0, width, height);

        if (grayscaleToggle.checked) {
            const imgData = ctx.getImageData(0, 0, width, height);
            const data = imgData.data;
            for (let i = 0; i < data.length; i += 4) {
                const lum = 0.299 * data[i] + 0.587 * data[i+1] + 0.114 * data[i+2];
                // 2-bit quantization (4 levels: 0, 85, 170, 255)
                const val = Math.round((lum / 255) * 3) * 85;
                data[i] = data[i+1] = data[i+2] = val;
            }
            ctx.putImageData(imgData, 0, 0);
        }

        let quality = 0.85;
        const compressAndCheck = () => {
            canvas.toBlob(blob => {
                if (blob.size > 60000 && quality > 0.2) {
                    quality -= 0.1;
                    compressAndCheck();
                } else {
                    compressedBlob = blob;
                    txImg.src = URL.createObjectURL(blob);
                    txImg.style.display = 'block';
                    txPlaceholder.style.display = 'none';
                    sendBtn.style.display = 'block';
                    sendBtn.innerText = `Send over LoRa (${(blob.size/1024).toFixed(1)} KB)`;
                }
            }, 'image/jpeg', quality);
        };
        compressAndCheck();
    }

    grayscaleToggle.addEventListener('change', () => {
        if (currentImg) processImage(currentImg);
    });

    fastModeToggle.addEventListener('change', () => {
        if (currentImg) processImage(currentImg);
    });

    fileInput.addEventListener('change', function(e) {
        const file = e.target.files[0];
        if (!file) return;

        const reader = new FileReader();
        reader.onload = function(event) {
            const img = new Image();
            img.onload = function() {
                currentImg = img;
                processImage(img);
            };
            img.src = event.target.result;
        };
        reader.readAsDataURL(file);
    });

    sendBtn.addEventListener('click', async function() {
        if (!compressedBlob) return;
        sendBtn.disabled = true; fileInput.disabled = true;
        
        try {
            const formData = new FormData();
            formData.append("image", compressedBlob, "image.jpg");

            const res = await fetch('/upload', {
                method: 'POST',
                headers: { 'X-Width': finalWidth, 'X-Height': finalHeight },
                body: formData
            });
            if (!res.ok) {
                const errText = await res.text();
                throw new Error("Upload failed: " + errText);
            }
            sendBtn.innerText = "Transmitting...";
        } catch (e) {
            alert(e.message);
            sendBtn.disabled = false; fileInput.disabled = false;
        }
    });

    sendTextBtn.addEventListener('click', async () => {
        const text = chatInput.value.trim();
        if (!text) return;
        
        chatInput.value = '';
        sendTextBtn.disabled = true;
        
        appendMessage(text, true);
        
        const MAX_PAYLOAD = 96;
        for (let i = 0; i < text.length; i += MAX_PAYLOAD) {
            const chunk = text.substring(i, i + MAX_PAYLOAD);
            try {
                const formData = new FormData();
                formData.append('text', chunk);
                
                const res = await fetch('/send_text', {
                    method: 'POST',
                    body: formData
                });
                if (!res.ok) throw new Error();
                await new Promise(r => setTimeout(r, 600)); 
            } catch (e) {
                alert("Failed to send part of the message.");
                break;
            }
        }
        sendTextBtn.disabled = false;
    });

    function renameWifi() {
        const name = document.getElementById('wifi-name').value;
        if (!name) return;
        fetch('/rename?name=' + encodeURIComponent(name), { method: 'POST' })
            .then(() => alert("Wi-Fi renamed! Please reconnect to the new network."))
            .catch(e => alert("Error renaming: " + e.message));
    }

    // Polling Loop
    setInterval(async () => {
        try {
            const res = await fetch('/status');
            const data = await res.json();
            
            // Set Status Badge
            if (data.mode === 'tx') {
                mainStatus.innerText = "TRANSMITTING";
                mainStatus.className = "status-badge tx";
                txProgContainer.style.display = 'block';
                rxProgContainer.style.display = 'none';
                txProg.style.width = Math.round((data.sent / data.total) * 100) + '%';
            } else if (data.mode === 'rx') {
                mainStatus.innerText = "RECEIVING";
                mainStatus.className = "status-badge rx";
                rxProgContainer.style.display = 'block';
                txProgContainer.style.display = 'none';
                rxProg.style.width = Math.round((data.sent / data.total) * 100) + '%';
            } else {
                mainStatus.innerText = "IDLE";
                mainStatus.className = "status-badge";
                rxProgContainer.style.display = 'none';
                txProgContainer.style.display = 'none';
                sendBtn.disabled = false;
                fileInput.disabled = false;
                if (compressedBlob) sendBtn.innerText = "Send over LoRa";
            }

            // Check for new RX image
            if (data.rxImageId === 0) {
                lastRxImageId = 0;
                rxImg.style.display = 'none';
                rxPlaceholder.style.display = 'block';
            } else if (data.rxImageId !== lastRxImageId && data.mode !== 'rx') {
                lastRxImageId = data.rxImageId;
                rxImg.src = '/rx_image.bin?t=' + new Date().getTime();
                rxImg.style.display = 'block';
                rxPlaceholder.style.display = 'none';
            }

            // Check for new RX text
            if (data.rxTextId && data.rxTextId !== lastRxTextId) {
                lastRxTextId = data.rxTextId;
                if (data.rxText) {
                    appendMessage(data.rxText, false);
                }
            }
        } catch (e) {}
    }, 1000);
</script>
</body>
</html>
)rawliteral";
