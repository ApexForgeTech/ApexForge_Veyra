#include "veyra/runtime/fingerprint_engine.h"

#include <functional>
#include <sstream>
#include <string>

namespace veyra {
namespace {

unsigned int SessionSeed(const std::string& session_id, const std::string& profile_id) {
  const std::size_t h = std::hash<std::string>{}(session_id + "|" + profile_id);
  // Fold to 32 bits safely on both 32-bit and 64-bit platforms.
  // On 64-bit: shifts right by 32; on 32-bit: shifts right by 16.
  const unsigned int lo = static_cast<unsigned int>(h & 0xFFFFFFFFu);
  const unsigned int hi = static_cast<unsigned int>((h >> (sizeof(h) * 4)) & 0xFFFFFFFFu);
  return lo ^ (hi * 2654435761u);
}

std::string JsEscape(const std::string& value) {
  std::string result;
  result.reserve(value.size());
  for (const char ch : value) {
    if (ch == '\'') {
      result += "\\'";
    } else if (ch == '\\') {
      result += "\\\\";
    } else if (ch == '\n') {
      result += "\\n";
    } else if (ch == '\r') {
      result += "\\r";
    } else {
      result.push_back(ch);
    }
  }
  return result;
}

std::string BoolLiteral(bool value) {
  return value ? "true" : "false";
}

}  // namespace

FingerprintPolicy BuildFingerprintPolicy(const FingerprintProfileDefinition& profile,
                                          const std::string& session_id) {
  FingerprintPolicy policy;
  policy.profile_id = profile.id;
  policy.profile_display_name = profile.display_name;
  policy.canvas_seed = SessionSeed(session_id, profile.id);

  if (profile.canvas_noise == "subtle" || profile.canvas_noise == "aggressive") {
    policy.inject_canvas_noise = true;
    policy.canvas_noise_level = profile.canvas_noise;
  }

  if (profile.webgl_vendor != "native") {
    policy.override_webgl = true;
    policy.webgl_vendor = profile.webgl_vendor;
    policy.webgl_renderer = profile.webgl_renderer;
  }

  if (profile.audio_noise == "subtle") {
    policy.inject_audio_noise = true;
    policy.audio_noise_magnitude = 0.0001;
  }

  if (profile.hardware_concurrency > 0) {
    policy.override_hardware_concurrency = true;
    policy.hardware_concurrency = profile.hardware_concurrency;
  }

  if (profile.device_memory > 0) {
    policy.override_device_memory = true;
    policy.device_memory = profile.device_memory;
  }

  if (profile.platform != "native") {
    policy.override_platform = true;
    policy.platform = profile.platform;
  }

  if (profile.screen_width > 0 && profile.screen_height > 0) {
    policy.override_screen = true;
    policy.screen_width = profile.screen_width;
    policy.screen_height = profile.screen_height;
    policy.color_depth = profile.color_depth;
    policy.device_pixel_ratio = profile.device_pixel_ratio > 0.0 ? profile.device_pixel_ratio : 1.0;
  }

  return policy;
}

std::string BuildFingerprintScript(const FingerprintPolicy& policy) {
  if (!policy.inject_canvas_noise && !policy.override_webgl && !policy.inject_audio_noise &&
      !policy.override_hardware_concurrency && !policy.override_device_memory &&
      !policy.override_platform && !policy.override_screen) {
    return {};
  }

  const bool aggressive = policy.canvas_noise_level == "aggressive";
  const double noise_probability = aggressive ? 0.03 : 0.01;
  const double audio_mag = policy.audio_noise_magnitude;

  std::ostringstream js;
  js << "(function(){\n"
     << "'use strict';\n"
     << "var SEED=" << policy.canvas_seed << ">>>0;\n"
     << "var NOISE_PROB=" << noise_probability << ";\n"
     << "var AUDIO_MAG=" << audio_mag << ";\n"

     // Seeded xorshift32 PRNG — deterministic per session, different per session
     << "function makeRng(s){\n"
     << "  var st=s>>>0||1;\n"
     << "  return function(){\n"
     << "    st^=st<<13;st^=st>>>17;st^=st<<5;\n"
     << "    return (st>>>0)/4294967296;\n"
     << "  };\n"
     << "}\n"

     // Canvas noise: proxy getContext('2d') to intercept getImageData.
     // WeakSet tracks already-patched context objects so that calling
     // getContext('2d') more than once on the same element (which the spec
     // requires to return the same object) does not attempt to redefine the
     // non-configurable getImageData property and throw TypeError.
     << "if(" << BoolLiteral(policy.inject_canvas_noise) << "){\n"
     << "  var _patchedCtx=new WeakSet();\n"
     << "  var _gc=HTMLCanvasElement.prototype.getContext;\n"
     << "  Object.defineProperty(HTMLCanvasElement.prototype,'getContext',{\n"
     << "    value:function(t,a){\n"
     << "      var ctx=_gc.call(this,t,a);\n"
     << "      if(ctx&&t==='2d'&&!_patchedCtx.has(ctx)){\n"
     << "        _patchedCtx.add(ctx);\n"
     << "        var _gid=ctx.getImageData.bind(ctx);\n"
     << "        Object.defineProperty(ctx,'getImageData',{\n"
     << "          value:function(sx,sy,sw,sh){\n"
     << "            var d=_gid(sx,sy,sw,sh);\n"
     << "            var rng=makeRng(SEED^((sx*1000003)^(sy*999983)));\n"
     << "            for(var i=0;i<d.data.length;i+=4){\n"
     << "              if(rng()<NOISE_PROB){\n"
     << "                var delta=rng()<0.5?1:-1;\n"
     << "                d.data[i]=(d.data[i]+delta)&0xff;\n"
     << "              }\n"
     << "            }\n"
     << "            return d;\n"
     << "          },\n"
     << "          writable:false,configurable:false\n"
     << "        });\n"
     << "      }\n"
     << "      return ctx;\n"
     << "    },\n"
     << "    writable:false,configurable:false\n"
     << "  });\n"
     << "}\n"

     // WebGL vendor/renderer override
     << "if(" << BoolLiteral(policy.override_webgl) << "){\n"
     << "  var patchWebGL=function(proto){\n"
     << "    if(!proto)return;\n"
     << "    var _gp=proto.getParameter;\n"
     << "    Object.defineProperty(proto,'getParameter',{\n"
     << "      value:function(p){\n"
     << "        if(p===0x1F00||p===0x9245)return '" << JsEscape(policy.webgl_vendor) << "';\n"
     << "        if(p===0x1F01||p===0x9246)return '" << JsEscape(policy.webgl_renderer) << "';\n"
     << "        return _gp.call(this,p);\n"
     << "      },writable:false,configurable:false\n"
     << "    });\n"
     << "  };\n"
     << "  if(typeof WebGLRenderingContext!=='undefined')patchWebGL(WebGLRenderingContext.prototype);\n"
     << "  if(typeof WebGL2RenderingContext!=='undefined')patchWebGL(WebGL2RenderingContext.prototype);\n"
     << "}\n"

     // AudioContext noise: inject into getFloatFrequencyData / getByteFrequencyData
     << "if(" << BoolLiteral(policy.inject_audio_noise) << "){\n"
     << "  var patchAudio=function(AC){\n"
     << "    if(!AC||!AC.prototype)return;\n"
     << "    var _ca=AC.prototype.createAnalyser;\n"
     << "    if(!_ca)return;\n"
     << "    AC.prototype.createAnalyser=function(){\n"
     << "      var a=_ca.call(this);\n"
     << "      var _gffd=a.getFloatFrequencyData.bind(a);\n"
     << "      var _gbfd=a.getByteFrequencyData.bind(a);\n"
     << "      var rng=makeRng(SEED);\n"
     << "      a.getFloatFrequencyData=function(arr){\n"
     << "        _gffd(arr);\n"
     << "        for(var i=0;i<arr.length;i++)arr[i]+=(rng()-0.5)*AUDIO_MAG;\n"
     << "      };\n"
     << "      a.getByteFrequencyData=function(arr){\n"
     << "        _gbfd(arr);\n"
     << "        for(var i=0;i<arr.length;i++){\n"
     << "          arr[i]=Math.max(0,Math.min(255,arr[i]+Math.round((rng()-0.5)*0.5)));\n"
     << "        }\n"
     << "      };\n"
     << "      return a;\n"
     << "    };\n"
     << "  };\n"
     << "  patchAudio(typeof AudioContext!=='undefined'?AudioContext:undefined);\n"
     << "  patchAudio(typeof OfflineAudioContext!=='undefined'?OfflineAudioContext:undefined);\n"
     << "}\n"

     // Navigator property overrides
     << "try{\n"
     << "  var navProps={};\n";

  if (policy.override_hardware_concurrency) {
    js << "  navProps.hardwareConcurrency={value:" << policy.hardware_concurrency
       << ",writable:false,configurable:false};\n";
  }
  if (policy.override_device_memory) {
    js << "  navProps.deviceMemory={value:" << policy.device_memory
       << ",writable:false,configurable:false};\n";
  }
  if (policy.override_platform) {
    js << "  navProps.platform={value:'" << JsEscape(policy.platform)
       << "',writable:false,configurable:false};\n";
  }

  js << "  if(Object.keys(navProps).length>0)Object.defineProperties(navigator,navProps);\n"
     << "}catch(e){}\n"

     // Screen and window metrics
     << "if(" << BoolLiteral(policy.override_screen) << "){\n"
     << "  try{\n"
     << "    Object.defineProperties(screen,{\n"
     << "      width:{value:" << policy.screen_width << ",writable:false,configurable:false},\n"
     << "      height:{value:" << policy.screen_height << ",writable:false,configurable:false},\n"
     << "      availWidth:{value:" << policy.screen_width << ",writable:false,configurable:false},\n"
     << "      availHeight:{value:" << (policy.screen_height - 40)
                                  << ",writable:false,configurable:false},\n"
     << "      colorDepth:{value:" << policy.color_depth << ",writable:false,configurable:false},\n"
     << "      pixelDepth:{value:" << policy.color_depth << ",writable:false,configurable:false}\n"
     << "    });\n"
     << "    Object.defineProperty(window,'devicePixelRatio',{value:" << policy.device_pixel_ratio
     << ",writable:false,configurable:false});\n"
     << "  }catch(e){}\n"
     << "}\n"

     << "})();\n";

  return js.str();
}

}  // namespace veyra
