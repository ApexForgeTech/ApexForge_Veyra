const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('sentinelAPI', {
  changePersona: (personaId) => ipcRenderer.invoke('change-persona', personaId),
  onPreload: () => console.log('Veyra Guard Preload Active')
});

window.addEventListener('DOMContentLoaded', () => {
  console.log('ApexForge Veyra preload initialized');
});
