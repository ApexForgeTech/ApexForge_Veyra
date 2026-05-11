// ApexForge Veyra Fingerprint Shield
// This script is injected into the webview to protect user privacy.

(function () {
  const originalCanvasToDataURL = HTMLCanvasElement.prototype.toDataURL;

  HTMLCanvasElement.prototype.toDataURL = function (type) {
    if (type === 'image/png') {
      console.log('Veyra: Canvas fingerprinting attempt observed.');
    }

    return originalCanvasToDataURL.apply(this, arguments);
  };

  Object.defineProperty(navigator, 'webdriver', { get: () => undefined });
  Object.defineProperty(navigator, 'languages', { get: () => ['en-US', 'en'] });

  console.log('Veyra Guard: Fingerprint shield active.');
})();
