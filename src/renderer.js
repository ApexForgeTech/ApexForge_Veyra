let webview = document.getElementById('webview');
const urlInput = document.getElementById('url-input');
const goButton = document.getElementById('go-button');
const backButton = document.getElementById('back-button');
const forwardButton = document.getElementById('forward-button');
const reloadButton = document.getElementById('reload-button');
const navButtons = document.querySelectorAll('.nav-btn');
const panels = document.querySelectorAll('.panel');
const personaCards = document.querySelectorAll('.persona-card');
const personaIndicator = document.getElementById('persona-indicator');
const webviewContainer = document.getElementById('webview-container');
const securityModeSelector = document.getElementById('security-mode-selector');
const securityIndicator = document.getElementById('security-indicator');
const aiChatInput = document.getElementById('ai-chat-input');
const aiSendBtn = document.getElementById('ai-send-btn');
const aiChatLog = document.getElementById('ai-chat-log');
const aiAnalyzeBtn = document.getElementById('ai-analyze-btn');
const aiClearBtn = document.getElementById('ai-clear-btn');
const osintTarget = document.getElementById('osint-target');
const osintScanBtn = document.getElementById('osint-scan-btn');
const osintResults = document.getElementById('osint-results');
const osintWhoisBtn = document.getElementById('osint-whois-btn');
const osintDnsBtn = document.getElementById('osint-dns-btn');
const osintMetaBtn = document.getElementById('osint-meta-btn');
const networkBtns = document.querySelectorAll('#network-panel .tool-btn');
const networkConnections = document.querySelectorAll('.connection');
const toggleButtons = document.querySelectorAll('.toggle-btn');

navButtons.forEach((btn) => {
  btn.addEventListener('click', () => {
    const targetPanel = btn.getAttribute('data-panel');

    navButtons.forEach((button) => button.classList.remove('active'));
    btn.classList.add('active');

    panels.forEach((panel) => {
      panel.classList.toggle('active', panel.id === `${targetPanel}-panel`);
    });
  });
});

personaCards.forEach((card) => {
  card.addEventListener('click', async () => {
    const personaId = card.getAttribute('data-persona');
    const personaName = card.querySelector('h3').innerText;

    personaCards.forEach((personaCard) => personaCard.classList.remove('active'));
    card.classList.add('active');

    if (window.sentinelAPI?.changePersona) {
      await window.sentinelAPI.changePersona(personaId);
    }

    recreateWebview(personaId);
    personaIndicator.innerText = `\u{1F464} ${personaName}`;
    console.log(`Switched to ${personaName} persona`);
  });
});

function recreateWebview(personaId = null, mode = null) {
  const currentUrl = webview?.src;
  const currentPersonaId =
    document.querySelector('.persona-card.active')?.getAttribute('data-persona') || 'default';
  const activeMode = mode || securityModeSelector.value;
  const activePersonaId = personaId || currentPersonaId;

  const newWebview = document.createElement('webview');
  newWebview.id = 'webview';
  newWebview.src = currentUrl || 'https://www.google.com';
  newWebview.partition =
    activeMode === 'ghost'
      ? 'temp'
      : activePersonaId === 'anon'
        ? 'temp'
        : `persist:${activePersonaId}`;

  newWebview.style.width = '100%';
  newWebview.style.height = '100%';

  webviewContainer.innerHTML = '';
  webviewContainer.appendChild(newWebview);
  webview = newWebview;

  attachWebviewListeners();
}

securityModeSelector.addEventListener('change', () => {
  const mode = securityModeSelector.value;

  if (mode === 'casual') {
    securityIndicator.innerText = '\u{1F7E2} Normal';
    securityIndicator.style.color = '#00ff9d';
  } else if (mode === 'hardened') {
    securityIndicator.innerText = '\u{1F7E1} Hardened';
    securityIndicator.style.color = '#ffcc00';
  } else if (mode === 'ghost') {
    securityIndicator.innerText = '\u{1F47B} Ghost';
    securityIndicator.style.color = '#ffffff';
  }

  recreateWebview();
});

function attachWebviewListeners() {
  webview.addEventListener('did-stop-loading', () => {
    urlInput.value = webview.getURL();
  });
}

function loadURL() {
  let url = urlInput.value.trim();
  if (!url) {
    return;
  }

  if (!url.startsWith('http://') && !url.startsWith('https://')) {
    if (url.includes('.') && !url.includes(' ')) {
      url = `https://${url}`;
    } else {
      url = `https://www.google.com/search?q=${encodeURIComponent(url)}`;
    }
  }

  webview.src = url;
}

goButton.addEventListener('click', loadURL);
urlInput.addEventListener('keydown', (event) => {
  if (event.key === 'Enter') {
    loadURL();
  }
});

backButton.addEventListener('click', () => {
  if (webview.canGoBack()) {
    webview.goBack();
  }
});

forwardButton.addEventListener('click', () => {
  if (webview.canGoForward()) {
    webview.goForward();
  }
});

reloadButton.addEventListener('click', () => {
  webview.reload();
});

function addMessage(text, sender) {
  const msgDiv = document.createElement('div');
  msgDiv.classList.add('message', sender);
  msgDiv.innerText = text;
  aiChatLog.appendChild(msgDiv);
  aiChatLog.scrollTop = aiChatLog.scrollHeight;
}

function handleAISend() {
  const text = aiChatInput.value.trim();
  if (!text) {
    return;
  }

  addMessage(text, 'user');
  aiChatInput.value = '';

  setTimeout(() => {
    let response = "I'm analyzing the requested data.";
    if (text.toLowerCase().includes('whoami')) {
      response =
        'You are currently operating under the ' +
        (document.querySelector('.persona-card.active')?.querySelector('h3').innerText || 'Default') +
        ' persona.';
    } else if (text.toLowerCase().includes('security')) {
      response = `Current security mode is set to ${securityModeSelector.value.toUpperCase()}. All systems guarded.`;
    } else {
      response += ' Veyra AI stands ready to assist with OSINT, security, and developer tasks.';
    }
    addMessage(response, 'ai');
  }, 1000);
}

aiSendBtn.addEventListener('click', handleAISend);
aiChatInput.addEventListener('keydown', (event) => {
  if (event.key === 'Enter') {
    handleAISend();
  }
});

aiAnalyzeBtn.addEventListener('click', () => {
  const currentUrl = webview.src || 'unknown';
  addMessage(`Analyzing current page: ${currentUrl}...`, 'user');

  setTimeout(() => {
    const report = `[Veyra Analysis Report]
- URL: ${currentUrl}
- Threat Level: LOW
- Detected Scripts: 12
- Privacy Risks: Google Analytics detected.
- Recommendation: Use Hardened mode for this domain.`;
    addMessage(report, 'ai');
  }, 1500);
});

aiClearBtn.addEventListener('click', () => {
  aiChatLog.innerHTML = '<div class="message ai">Logs cleared. Systems operational.</div>';
});

osintScanBtn.addEventListener('click', () => {
  const target = osintTarget.value.trim();
  if (!target) {
    return;
  }

  osintResults.innerHTML = `<div class="scanning">Initializing deep scan for: ${target}...</div>`;

  setTimeout(() => {
    osintResults.innerHTML = `
      <div>[+] Target: ${target}</div>
      <div>[+] Status: ACTIVE</div>
      <div>[+] Data points found: 42</div>
      <br>
      <div>[SCAN LOG]</div>
      <div>Searching historical DNS... Found.</div>
      <div>Checking leaked databases... No matches.</div>
      <div>Analyzing social footprint... 3 profiles detected.</div>
      <br>
      <div style="color: #f00;">[!] Warning: High entropy detected in network traffic.</div>
    `;
  }, 2000);
});

osintWhoisBtn.addEventListener('click', () => {
  const target = osintTarget.value.trim();
  if (!target) {
    return;
  }

  osintResults.innerHTML =
    `[WHOIS INFO FOR ${target}]
Registrar: SafeNames Ltd.
Creation Date: 2010-05-15
Expiry Date: 2026-05-15
NS: ns1.sentinel.guard`.replace(/\n/g, '<br>');
});

osintDnsBtn.addEventListener('click', () => {
  const target = osintTarget.value.trim();
  if (!target) {
    return;
  }

  osintResults.innerHTML = `
    <div>[DNS MAP FOR ${target}]</div>
    <br>
    <div>A record: 203.0.113.12</div>
    <div>MX record: mail.${target}</div>
    <div>TXT record: v=spf1 include:sentinel.guard ~all</div>
    <div>CNAME: cdn.${target}</div>
  `;
});

osintMetaBtn.addEventListener('click', () => {
  osintResults.innerHTML = '<div style="color: yellow;">[SYSTEM] Select file for metadata stripping...</div>';
  setTimeout(() => {
    osintResults.innerHTML =
      "<div style=\"color: green;\">[SUCCESS] Metadata stripped from 'evidence.jpg'. Entropy reduced. File moved to BlackVault.</div>";
  }, 1500);
});

networkBtns.forEach((btn, index) => {
  btn.addEventListener('click', () => {
    networkBtns.forEach((button) => button.classList.remove('active'));
    btn.classList.add('active');

    networkConnections.forEach((connection, connectionIndex) => {
      connection.classList.toggle('active', connectionIndex < index);
    });

    console.log(`Routing changed to: ${btn.innerText}`);
  });
});

toggleButtons.forEach((button) => {
  button.addEventListener('click', () => {
    button.classList.toggle('active');
  });
});

attachWebviewListeners();
