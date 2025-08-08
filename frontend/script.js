function appendMessage(message, isUser) {
    const chatBox = document.getElementById('chatBox');
    const messageDiv = document.createElement('div');
    messageDiv.className = `message ${isUser ? 'user-message' : 'bot-message'}`;
    if (isUser) {
        messageDiv.textContent = message;
    } else {
        // Basic markdown: newlines and code blocks
        const html = message
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/```([\s\S]*?)```/g, '<pre><code>$1</code></pre>')
            .replace(/\n/g, '<br>');
        messageDiv.innerHTML = html;
    }

    chatBox.appendChild(messageDiv);
    chatBox.scrollTop = chatBox.scrollHeight;
}

async function sendMessage() {
    const input = document.getElementById('userInput');
    const message = input.value.trim();
    
    if (message === '') return;
    
    appendMessage(message, true);
    input.value = '';

    try {
        const response = await fetch('http://localhost:8080/chat', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ message: message })
        });
        
        const data = await response.json();
        const aiText = (data.response || '').replace(/^Assistant:\s*/g, '').trim();
        appendMessage(aiText, false);
    } catch (error) {
        appendMessage('Error: Could not connect to server', false);
        console.error('Error:', error);
    }
}

document.getElementById('userInput').addEventListener('keypress', function(e) {
    if (e.key === 'Enter') {
        sendMessage();
    }
});

// Settings modal logic
const settingsModal = document.getElementById('settingsModal');
document.getElementById('openSettings').addEventListener('click', async () => {
    await loadConfigIntoForm();
    settingsModal.style.display = 'block';
});
document.getElementById('closeSettings').addEventListener('click', () => {
    settingsModal.style.display = 'none';
});
document.getElementById('saveSettings').addEventListener('click', async () => {
    const cfg = {
        model: document.getElementById('model').value.trim(),
        temperature: parseFloat(document.getElementById('temperature').value),
        top_p: parseFloat(document.getElementById('top_p').value),
        top_k: parseInt(document.getElementById('top_k').value, 10),
        max_output_tokens: parseInt(document.getElementById('max_output_tokens').value, 10),
        system_prompt: document.getElementById('system_prompt').value
    };
    // Remove NaNs/empties
    Object.keys(cfg).forEach(k => {
        if (cfg[k] === '' || Number.isNaN(cfg[k])) delete cfg[k];
    });
    try {
        await fetch('http://localhost:8080/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(cfg)
        });
        settingsModal.style.display = 'none';
    } catch (e) {
        console.error('Failed to save config', e);
    }
});

document.getElementById('clearHistory').addEventListener('click', async () => {
    try {
        await fetch('http://localhost:8080/clear', { method: 'POST' });
        const chatBox = document.getElementById('chatBox');
        chatBox.innerHTML = '';
    } catch (e) {
        console.error('Failed to clear history', e);
    }
});

async function loadConfigIntoForm() {
    try {
        const res = await fetch('http://localhost:8080/config');
        const cfg = await res.json();
        document.getElementById('model').value = cfg.model || 'gemini-1.5-pro';
        document.getElementById('temperature').value = cfg.temperature ?? 0.7;
        document.getElementById('top_p').value = cfg.top_p ?? 1.0;
        document.getElementById('top_k').value = cfg.top_k ?? 64;
        document.getElementById('max_output_tokens').value = cfg.max_output_tokens ?? 2048;
        document.getElementById('system_prompt').value = cfg.system_prompt || '';
    } catch (e) {
        console.error('Failed to load config', e);
    }
}
