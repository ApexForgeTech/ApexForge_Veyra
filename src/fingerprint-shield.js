// ApexForge Veyra Advanced Anti-Fingerprinting Enclave Shield
// Injected at document-start to prevent tracker correlation across personas.

(function () {
  'use strict';

  console.log('Veyra Sentinel Guard: Initializing advanced fingerprint shield...');

  // 1. Canvas Fingerprint Protection (Pixel Noise Injection)
  const originalToDataURL = HTMLCanvasElement.prototype.toDataURL;
  const originalGetImageData = CanvasRenderingContext2D.prototype.getImageData;

  HTMLCanvasElement.prototype.toDataURL = function (type, ...args) {
    // Inject subtle noise if it looks like a canvas measurement
    if (this.width > 10 && this.height > 10) {
      const ctx = this.getContext('2d');
      if (ctx) {
        try {
          const imgData = originalGetImageData.call(ctx, 0, 0, 1, 1);
          // Subtle manipulation of the first pixel
          imgData.data[0] = (imgData.data[0] + 1) % 256; 
          ctx.putImageData(imgData, 0, 0);
        } catch (e) {
          // Cross-origin canvases will throw
        }
      }
      console.log('Veyra Guard: Intercepted Canvas.toDataURL() fingerprinting vector.');
    }
    return originalToDataURL.apply(this, [type, ...args]);
  };

  CanvasRenderingContext2D.prototype.getImageData = function (sx, sy, sw, sh) {
    const imgData = originalGetImageData.call(this, sx, sy, sw, sh);
    // Inject imperceptible noise into pixel data to break deterministic hashing
    if (sw > 5 && sh > 5) {
      for (let i = 0; i < imgData.data.length; i += 40) {
        imgData.data[i] = (imgData.data[i] + (Math.random() > 0.5 ? 1 : -1)) & 0xFF;
      }
      console.log('Veyra Guard: Injected noise into Canvas.getImageData() buffer.');
    }
    return imgData;
  };

  // 2. WebGL Hardware Info Spoofing
  const originalGetParameter = WebGLRenderingContext.prototype.getParameter;
  const originalGetParameter2 = typeof WebGL2RenderingContext !== 'undefined' ? WebGL2RenderingContext.prototype.getParameter : null;

  const webGLOverrides = {
    37445: 'Google Inc. (Apple)', // UNMASKED_VENDOR_WEBGL
    37446: 'ANGLE (Apple, Apple M1 Max, OpenGL 4.1)', // UNMASKED_RENDERER_WEBGL
    7936: 'WebKit', // VENDOR
    7937: 'WebKit WebGL' // RENDERER
  };

  const spoofWebGL = function (parameter) {
    if (webGLOverrides[parameter]) {
      console.log(`Veyra Guard: Spoofed WebGL Parameter ${parameter} -> ${webGLOverrides[parameter]}`);
      return webGLOverrides[parameter];
    }
    return originalGetParameter.apply(this, arguments);
  };

  WebGLRenderingContext.prototype.getParameter = spoofWebGL;
  if (originalGetParameter2) {
    WebGL2RenderingContext.prototype.getParameter = spoofWebGL;
  }

  // 3. Audio Context & Frequency Noise Injection
  const originalGetChannelData = AudioBuffer.prototype.getChannelData;
  AudioBuffer.prototype.getChannelData = function (channel) {
    const data = originalGetChannelData.apply(this, arguments);
    // Add micro-noise to frequency buffer
    for (let i = 0; i < data.length; i += 100) {
      data[i] += Math.random() * 0.0000001;
    }
    console.log('Veyra Guard: Scrambled AudioContext getChannelData() fingerprint.');
    return data;
  };

  // 4. Hardware and Platform Metrics Obfuscation
  Object.defineProperty(navigator, 'webdriver', { get: () => undefined, configurable: true });
  Object.defineProperty(navigator, 'deviceMemory', { get: () => 8, configurable: true });
  Object.defineProperty(navigator, 'hardwareConcurrency', { get: () => 8, configurable: true });
  Object.defineProperty(navigator, 'platform', { get: () => 'Win32', configurable: true });
  Object.defineProperty(navigator, 'languages', { get: () => ['en-US', 'en'], configurable: true });

  // 5. Screen Metrics Normalization
  Object.defineProperty(screen, 'width', { get: () => 1920, configurable: true });
  Object.defineProperty(screen, 'height', { get: () => 1080, configurable: true });
  Object.defineProperty(screen, 'availWidth', { get: () => 1920, configurable: true });
  Object.defineProperty(screen, 'availHeight', { get: () => 1040, configurable: true });
  Object.defineProperty(screen, 'colorDepth', { get: () => 24, configurable: true });
  Object.defineProperty(screen, 'pixelDepth', { get: () => 24, configurable: true });

  console.log('Veyra Sentinel Guard: Enclave fingerprint protection successfully sealed.');
})();
