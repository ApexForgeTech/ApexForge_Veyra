const { app, BrowserWindow, ipcMain } = require('electron');
const fs = require('fs');
const path = require('path');

const fingerprintShieldPath = path.join(__dirname, 'fingerprint-shield.js');
const fingerprintShieldSource = fs.readFileSync(fingerprintShieldPath, 'utf8');

console.warn('[Veyra Prototype] Electron track is prototype-only and non-production.');

function createWindow() {
  const win = new BrowserWindow({
    width: 1400,
    height: 900,
    title: 'ApexForge Veyra',
    backgroundColor: '#0a0a0a',
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      webviewTag: true,
      preload: path.join(__dirname, 'preload.js')
    }
  });

  win.loadFile(path.join(__dirname, 'index.html'));

  // Inject the fingerprint shield into every attached webview once it loads.
  win.webContents.on('did-attach-webview', (_event, guestContents) => {
    guestContents.on('dom-ready', () => {
      guestContents.executeJavaScript(fingerprintShieldSource).catch((error) => {
        console.error('Failed to inject fingerprint shield:', error);
      });
    });
  });

  win.webContents.session.setPermissionRequestHandler((_webContents, permission, callback) => {
    const allowedPermissions = ['notifications', 'fullscreen'];
    callback(allowedPermissions.includes(permission));
  });
}

ipcMain.handle('change-persona', async (_event, personaId) => {
  console.log(`Switching to persona: ${personaId}`);
  return `persist:${personaId}`;
});

app.whenReady().then(() => {
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});
