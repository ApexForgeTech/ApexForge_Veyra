let webview = document.getElementById('webview');
const urlInput = document.getElementById('url-input');
const goButton = document.getElementById('go-button');
const backButton = document.getElementById('back-button');
const forwardButton = document.getElementById('forward-button');
const reloadButton = document.getElementById('reload-button');
const navButtons = document.querySelectorAll('.nav-btn');
const panels = document.querySelectorAll('.panel');
const webviewContainer = document.getElementById('webview-container');
const securityModeSelector = document.getElementById('security-mode-selector');
const securityIndicator = document.getElementById('security-indicator');
const personaIndicator = document.getElementById('persona-indicator');
const routeIndicator = document.getElementById('route-indicator');
const sessionIndicator = document.getElementById('session-indicator');
const fingerprintIndicator = document.getElementById('fingerprint-indicator');
const modeSummary = document.getElementById('mode-summary');
const personaList = document.getElementById('persona-list');
const personaSummary = document.getElementById('persona-summary');
const matrixCookieStore = document.getElementById('matrix-cookie-store');
const matrixTimezone = document.getElementById('matrix-timezone');
const matrixNetwork = document.getElementById('matrix-network');
const matrixMemory = document.getElementById('matrix-memory');
const aiChatInput = document.getElementById('ai-chat-input');
const aiSendBtn = document.getElementById('ai-send-btn');
const aiChatLog = document.getElementById('ai-chat-log');
const aiAnalyzeBtn = document.getElementById('ai-analyze-btn');
const aiRiskBtn = document.getElementById('ai-risk-btn');
const aiOsintBtn = document.getElementById('ai-osint-btn');
const aiClearBtn = document.getElementById('ai-clear-btn');
const osintTarget = document.getElementById('osint-target');
const osintScanBtn = document.getElementById('osint-scan-btn');
const osintResults = document.getElementById('osint-results');
const osintWhoisBtn = document.getElementById('osint-whois-btn');
const osintDnsBtn = document.getElementById('osint-dns-btn');
const osintMetaBtn = document.getElementById('osint-meta-btn');
const routeControls = document.getElementById('route-controls');
const networkConnections = document.querySelectorAll('.connection');
const routeSummary = document.getElementById('route-summary');
const routeHopOne = document.getElementById('route-hop-1');
const routeHopTwo = document.getElementById('route-hop-2');
const dnsPolicy = document.getElementById('dns-policy');
const webrtcPolicy = document.getElementById('webrtc-policy');
const leakStatus = document.getElementById('leak-status');
const transferPolicy = document.getElementById('transfer-policy');
const toggleButtons = document.querySelectorAll('.toggle-btn');

const PERSONAS = [
  {
    id: 'default',
    name: 'Real Identity',
    route: 'direct',
    partition: 'persist:default',
    timezone: 'Local timezone',
    fingerprint: 'Stable native',
    storage: 'Persistent vault',
    memory: 'Scoped profile memory',
    description: 'Everyday browsing with trusted extensions and local routing.'
  },
  {
    id: 'anonymous',
    name: 'Anonymous',
    route: 'tor',
    partition: 'memory',
    timezone: 'Generic UTC',
    fingerprint: 'Balanced stealth',
    storage: 'RAM-only session',
    memory: 'No retained memory',
    description: 'Disposable research posture with no persistent session artifacts.'
  },
  {
    id: 'work',
    name: 'Work',
    route: 'vpn',
    partition: 'persist:work',
    timezone: 'Office timezone',
    fingerprint: 'Corporate baseline',
    storage: 'Persistent vault',
    memory: 'Team-scoped memory',
    description: 'Productivity profile with clean isolation from personal browsing.'
  },
  {
    id: 'research',
    name: 'Research',
    route: 'residential',
    partition: 'persist:research',
    timezone: 'Target-aligned offset',
    fingerprint: 'Consistency tuned',
    storage: 'Persistent vault',
    memory: 'Research-only memory',
    description: 'Long-form reconnaissance with stable-but-separated browsing identity.'
  },
  {
    id: 'redteam',
    name: 'Red Team',
    route: 'chained',
    partition: 'memory',
    timezone: 'Operator override',
    fingerprint: 'Low-noise obfuscation',
    storage: 'RAM-only session',
    memory: 'No retained memory',
    description: 'High-isolation mode for controlled testing and hostile web exposure.'
  },
  {
    id: 'banking',
    name: 'Banking',
    route: 'direct',
    partition: 'persist:banking',
    timezone: 'Local timezone',
    fingerprint: 'Minimal surface',
    storage: 'Dedicated encrypted vault',
    memory: 'No cross-persona reuse',
    description: 'Locked-down financial browsing with extension minimization.'
  },
  {
    id: 'social',
    name: 'Social Media',
    route: 'vpn',
    partition: 'persist:social',
    timezone: 'Profile timezone',
    fingerprint: 'Platform normality',
    storage: 'Persistent vault',
    memory: 'Feed-scoped memory',
    description: 'Separate social footprint to reduce correlation with work and research.'
  },
  {
    id: 'disposable',
    name: 'Disposable',
    route: 'vpn',
    partition: 'memory',
    timezone: 'Rotating generic',
    fingerprint: 'One-mission profile',
    storage: 'Auto-wipe session',
    memory: 'No retained memory',
    description: 'One-off tasks with automatic cleanup after tab recreation.'
  }
];

const SECURITY_MODES = {
  casual: {
    label: 'Casual',
    color: 'var(--accent-success)',
    summary: 'Daily browsing with balanced privacy controls.',
    dns: 'System resolver',
    webrtc: 'Allowed with warnings',
    leak: 'No active alert',
    transfer: 'Standard'
  },
  hardened: {
    label: 'Hardened',
    color: 'var(--accent-warning)',
    summary: 'Strict tracking resistance, tighter cookies, and safer defaults.',
    dns: 'Isolated resolver',
    webrtc: 'Constrained',
    leak: 'Leak watch enabled',
    transfer: 'Scanned before release'
  },
  ghost: {
    label: 'Ghost',
    color: 'var(--accent-secondary)',
    summary: 'Ephemeral memory, automatic cleanup, and minimal retention.',
    dns: 'Ephemeral resolver',
    webrtc: 'Disabled',
    leak: 'Ghost session sealed',
    transfer: 'BlackVault staging'
  },
  redteam: {
    label: 'Red Team',
    color: 'var(--accent-danger)',
    summary: 'High-scrutiny route posture with maximum compartmentalization.',
    dns: 'Proxy-bound resolver',
    webrtc: 'Killed',
    leak: 'Continuous leak prevention',
    transfer: 'Quarantined and analyzed'
  },
  airgap: {
    label: 'Airgap Transfer',
    color: 'var(--accent-color)',
    summary: 'Files move through analysis and metadata stripping before host release.',
    dns: 'Analysis-only path',
    webrtc: 'Disabled',
    leak: 'Transfer enclave isolated',
    transfer: 'Vault handoff only'
  }
};

const ROUTES = {
  direct: {
    label: 'Direct ISP',
    button: 'Direct (ISP)',
    hops: ['ISP', 'Target'],
    description: 'Low-latency direct route for trusted sessions.'
  },
  vpn: {
    label: 'VPN Tunnel',
    button: 'VPN Tunnel',
    hops: ['VPN', 'Target'],
    description: 'Single-hop encrypted route for work and separated browsing.'
  },
  tor: {
    label: 'Tor Bridge',
    button: 'Tor Bridge',
    hops: ['Tor Entry', 'Tor Exit'],
    description: 'Multi-hop anonymity route with stronger unlinkability.'
  },
  residential: {
    label: 'Residential Proxy',
    button: 'Residential Proxy',
    hops: ['Residential', 'Target'],
    description: 'Blends into consumer traffic with stable routing.'
  },
  chained: {
    label: 'Chained Route',
    button: 'Chained Route',
    hops: ['VPN', 'Tor'],
    description: 'Stacked multi-hop path for higher-risk red team workflows.'
  }
};

const state = {
  personaId: 'default',
  securityMode: 'casual',
  routeId: 'direct',
  toggles: {
    js: true,
    fingerprint: true,
    networkIsolation: false,
    quarantine: true,
    ramOnly: false
  }
};

function getPersona() {
  return PERSONAS.find((persona) => persona.id === state.personaId) || PERSONAS[0];
}

function getMode() {
  return SECURITY_MODES[state.securityMode] || SECURITY_MODES.casual;
}

function getRoute() {
  return ROUTES[state.routeId] || ROUTES.direct;
}

function renderPersonas() {
  personaList.innerHTML = PERSONAS.map((persona) => {
    const isActive = persona.id === state.personaId ? ' active' : '';

    return `
      <button class="persona-card${isActive}" data-persona="${persona.id}">
        <h3>${persona.name}</h3>
        <p>${persona.description}</p>
        <div class="persona-meta">
          <span>${persona.route.toUpperCase()}</span>
          <span>${persona.storage}</span>
        </div>
      </button>
    `;
  }).join('');

  personaList.querySelectorAll('.persona-card').forEach((card) => {
    card.addEventListener('click', async () => {
      const personaId = card.getAttribute('data-persona');
      await switchPersona(personaId);
    });
  });
}

function renderRoutes() {
  routeControls.innerHTML = Object.entries(ROUTES)
    .map(([routeId, route]) => {
      const isActive = routeId === state.routeId ? ' active' : '';
      return `<button class="tool-btn route-btn${isActive}" data-route="${routeId}">${route.button}</button>`;
    })
    .join('');

  routeControls.querySelectorAll('.route-btn').forEach((button) => {
    button.addEventListener('click', () => {
      state.routeId = button.getAttribute('data-route');
      syncInterface();
    });
  });
}

function syncInterface() {
  const persona = getPersona();
  const mode = getMode();
  const route = getRoute();
  const useMemoryPartition =
    persona.partition === 'memory' || state.securityMode === 'ghost' || state.securityMode === 'airgap';

  personaIndicator.textContent = persona.name;
  securityIndicator.textContent = mode.label;
  securityIndicator.style.color = mode.color;
  routeIndicator.textContent = route.label;
  sessionIndicator.textContent = useMemoryPartition ? 'RAM-only session' : persona.storage;
  fingerprintIndicator.textContent = persona.fingerprint;
  modeSummary.textContent = mode.summary;

  personaSummary.textContent = `${persona.name} uses ${route.label} with ${persona.storage.toLowerCase()}.`;
  matrixCookieStore.textContent = useMemoryPartition ? 'Ephemeral isolated store' : persona.partition;
  matrixTimezone.textContent = persona.timezone;
  matrixNetwork.textContent = route.label;
  matrixMemory.textContent = persona.memory;

  routeSummary.textContent = route.description;
  routeHopOne.textContent = route.hops[0];
  routeHopTwo.textContent = route.hops[1];
  dnsPolicy.textContent = mode.dns;
  webrtcPolicy.textContent = mode.webrtc;
  leakStatus.textContent = mode.leak;
  transferPolicy.textContent = mode.transfer;

  updateRouteConnections();
  refreshToggleStates();
  highlightActiveButtons();
}

function highlightActiveButtons() {
  document.querySelectorAll('.persona-card').forEach((card) => {
    card.classList.toggle('active', card.getAttribute('data-persona') === state.personaId);
  });

  document.querySelectorAll('.route-btn').forEach((button) => {
    button.classList.toggle('active', button.getAttribute('data-route') === state.routeId);
  });
}

function updateRouteConnections() {
  const activeConnections = state.routeId === 'direct' ? 1 : state.routeId === 'vpn' || state.routeId === 'residential' ? 2 : 3;

  networkConnections.forEach((connection, index) => {
    connection.classList.toggle('active', index < activeConnections);
  });
}

function refreshToggleStates() {
  toggleButtons.forEach((button) => {
    const setting = button.getAttribute('data-setting');
    const isOn = state.toggles[setting];

    button.classList.toggle('active', Boolean(isOn));
    if (setting === 'fingerprint') {
      button.textContent = isOn ? 'MAX' : 'BASE';
      return;
    }

    button.textContent = isOn ? 'ON' : 'OFF';
  });
}

async function switchPersona(personaId) {
  const persona = PERSONAS.find((item) => item.id === personaId);
  if (!persona) {
    return;
  }

  state.personaId = personaId;
  state.routeId = persona.route;

  if (window.sentinelAPI?.changePersona) {
    await window.sentinelAPI.changePersona(personaId);
  }

  syncInterface();
  recreateWebview();
}

function buildPartition() {
  const persona = getPersona();
  const useMemoryPartition =
    persona.partition === 'memory' || state.securityMode === 'ghost' || state.securityMode === 'airgap';

  return useMemoryPartition ? `veyra-temp-${state.personaId}-${Date.now()}` : persona.partition;
}

function recreateWebview() {
  const currentUrl = webview?.getURL?.() || webview?.src || 'https://www.google.com';
  const newWebview = document.createElement('webview');

  newWebview.id = 'webview';
  newWebview.src = currentUrl;
  newWebview.partition = buildPartition();
  newWebview.style.width = '100%';
  newWebview.style.height = '100%';

  webviewContainer.innerHTML = '';
  webviewContainer.appendChild(newWebview);
  webview = newWebview;

  attachWebviewListeners();
}

function attachWebviewListeners() {
  webview.addEventListener('did-stop-loading', () => {
    urlInput.value = webview.getURL();
  });
}

function navigatePanel(panelName) {
  navButtons.forEach((button) => {
    button.classList.toggle('active', button.getAttribute('data-panel') === panelName);
  });

  panels.forEach((panel) => {
    panel.classList.toggle('active', panel.id === `${panelName}-panel`);
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

function addMessage(text, sender) {
  const msgDiv = document.createElement('div');
  msgDiv.classList.add('message', sender);
  msgDiv.textContent = text;
  aiChatLog.appendChild(msgDiv);
  aiChatLog.scrollTop = aiChatLog.scrollHeight;
}

function currentPage() {
  return webview?.getURL?.() || webview?.src || 'unknown';
}

function generateAIResponse(text) {
  const query = text.toLowerCase();
  const persona = getPersona();
  const mode = getMode();
  const route = getRoute();

  if (query.includes('whoami') || query.includes('persona')) {
    return `Active persona is ${persona.name}. Route is ${route.label}, timezone profile is ${persona.timezone}, and storage mode is ${persona.storage}.`;
  }

  if (query.includes('security') || query.includes('mode')) {
    return `Security mode is ${mode.label}. DNS policy is ${mode.dns}, WebRTC policy is ${mode.webrtc}, and transfer policy is ${mode.transfer}.`;
  }

  if (query.includes('route') || query.includes('proxy') || query.includes('tor')) {
    return `Current route is ${route.label}. This posture is tuned for ${persona.name.toLowerCase()} and keeps persona memory ${persona.memory.toLowerCase()}.`;
  }

  if (query.includes('osint') || query.includes('domain')) {
    return `ForgeAI can prepare a quick OSINT brief, correlate DNS and WHOIS records, and summarize page indicators without leaving the persona boundary.`;
  }

  return 'ForgeAI is ready to analyze pages, inspect routing posture, summarize research targets, and keep findings scoped to the active persona.';
}

function handleAISend() {
  const text = aiChatInput.value.trim();
  if (!text) {
    return;
  }

  addMessage(text, 'user');
  aiChatInput.value = '';

  setTimeout(() => {
    addMessage(generateAIResponse(text), 'ai');
  }, 600);
}

function renderPageAnalysis() {
  const url = currentPage();
  const persona = getPersona();
  const mode = getMode();
  const route = getRoute();

  addMessage(`Analyze page: ${url}`, 'user');

  setTimeout(() => {
    addMessage(
      `[ForgeAI Report]
URL: ${url}
Persona: ${persona.name}
Security Mode: ${mode.label}
Route: ${route.label}
Threat Score: Low to moderate
Privacy Signal: Third-party analytics observed
Recommended Action: Use Hardened or Ghost mode before login or download events.`,
      'ai'
    );
  }, 900);
}

function renderPhishingCheck() {
  const url = currentPage();
  const persona = getPersona();

  addMessage(`Run phishing check for ${url}`, 'user');

  setTimeout(() => {
    addMessage(
      `[Sentinel Guard]
Target: ${url}
Persona Boundary: ${persona.name}
Brand Spoofing: No direct match
Form Risk: Medium if credentials are entered
Action: Keep password manager disabled outside Banking persona.`,
      'ai'
    );
  }, 900);
}

function renderOSINTBrief() {
  const target = osintTarget.value.trim() || currentPage();
  const route = getRoute();

  addMessage(`Generate OSINT brief for ${target}`, 'user');

  setTimeout(() => {
    addMessage(
      `[OSINT Brief]
Target: ${target}
Route Posture: ${route.label}
Likely Assets: Main domain, MX infrastructure, CDN edge, social profiles
Next Steps: WHOIS, DNS expansion, archive snapshots, metadata sweep, timeline build.`,
      'ai'
    );
  }, 900);
}

function renderOSINTScan() {
  const target = osintTarget.value.trim();
  if (!target) {
    return;
  }

  const persona = getPersona();
  const route = getRoute();

  osintResults.innerHTML = `<div class="scanning">Initializing scoped scan for ${target} via ${route.label}...</div>`;

  setTimeout(() => {
    osintResults.innerHTML = `
      <div>[+] Target: ${target}</div>
      <div>[+] Persona: ${persona.name}</div>
      <div>[+] Route: ${route.label}</div>
      <div>[+] Status: ACTIVE</div>
      <div>[+] Correlated assets: 18</div>
      <br>
      <div>[SCAN LOG]</div>
      <div>Historical DNS located and normalized.</div>
      <div>Archive snapshots queued for timeline build.</div>
      <div>Social footprint hints detected across 3 likely profiles.</div>
      <div>Metadata workflow ready for BlackVault transfer.</div>
      <br>
      <div style="color: var(--accent-warning);">[!] Note: this is a legal research mockup, not an active collection engine.</div>
    `;
  }, 1100);
}

function renderWhois() {
  const target = osintTarget.value.trim();
  if (!target) {
    return;
  }

  osintResults.innerHTML =
    `[WHOIS INFO FOR ${target}]
Registrar: SafeNames Ltd.
Creation Date: 2010-05-15
Expiry Date: 2028-05-15
Name Servers: ns1.sentinel.guard, ns2.sentinel.guard
Privacy Posture: Registry privacy enabled`.replace(/\n/g, '<br>');
}

function renderDnsMap() {
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
    <div>ASN clue: edge network observed</div>
  `;
}

function renderMetadataFlow() {
  osintResults.innerHTML = '<div style="color: var(--accent-warning);">[SYSTEM] SpecterScan staging artifact for metadata stripping and entropy review...</div>';
  setTimeout(() => {
    osintResults.innerHTML =
      "<div style=\"color: var(--accent-success);\">[SUCCESS] Metadata stripped from 'evidence.jpg'. Artifact moved to BlackVault quarantine and marked ready for controlled release.</div>";
  }, 900);
}

navButtons.forEach((btn) => {
  btn.addEventListener('click', () => {
    navigatePanel(btn.getAttribute('data-panel'));
  });
});

securityModeSelector.addEventListener('change', () => {
  state.securityMode = securityModeSelector.value;
  state.toggles.ramOnly = ['ghost', 'airgap', 'redteam'].includes(state.securityMode);
  state.toggles.networkIsolation = ['hardened', 'ghost', 'redteam', 'airgap'].includes(state.securityMode);
  syncInterface();
  recreateWebview();
});

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

aiSendBtn.addEventListener('click', handleAISend);
aiChatInput.addEventListener('keydown', (event) => {
  if (event.key === 'Enter') {
    handleAISend();
  }
});

aiAnalyzeBtn.addEventListener('click', renderPageAnalysis);
aiRiskBtn.addEventListener('click', renderPhishingCheck);
aiOsintBtn.addEventListener('click', renderOSINTBrief);
aiClearBtn.addEventListener('click', () => {
  aiChatLog.innerHTML =
    '<div class="message ai">Logs cleared. ForgeAI memory remains scoped to the active persona.</div>';
});

osintScanBtn.addEventListener('click', renderOSINTScan);
osintWhoisBtn.addEventListener('click', renderWhois);
osintDnsBtn.addEventListener('click', renderDnsMap);
osintMetaBtn.addEventListener('click', renderMetadataFlow);

toggleButtons.forEach((button) => {
  button.addEventListener('click', () => {
    const setting = button.getAttribute('data-setting');
    state.toggles[setting] = !state.toggles[setting];
    refreshToggleStates();
  });
});

renderPersonas();
renderRoutes();
syncInterface();
attachWebviewListeners();
