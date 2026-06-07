#include "veyra/runtime/extension_engine.h"

#include <cctype>
#include <string>

namespace veyra {
namespace {

std::string ToLowerAscii(std::string value) {
  for (char& ch : value) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return value;
}

bool HostMatchesDomain(const std::string& host, const std::string& domain) {
  const std::string lower_host = ToLowerAscii(host);
  const std::string lower_domain = ToLowerAscii(domain);

  // Exact match.
  if (lower_host == lower_domain) {
    return true;
  }

  // Subdomain match: host ends with ".<domain>".
  if (lower_host.size() > lower_domain.size() + 1) {
    const std::size_t suffix_pos = lower_host.size() - lower_domain.size();
    if (lower_host[suffix_pos - 1] == '.' &&
        lower_host.compare(suffix_pos, lower_domain.size(), lower_domain) == 0) {
      return true;
    }
  }

  return false;
}

}  // namespace

ExtensionPolicy BuildExtensionPolicy(const ExtensionPolicyDefinition& definition) {
  ExtensionPolicy policy;
  policy.policy_id = definition.id;
  policy.policy_display_name = definition.display_name;
  policy.allow_eval = definition.allow_eval;
  policy.allow_third_party_frames = definition.allow_third_party_frames;
  policy.block_mixed_content = definition.block_mixed_content;
  policy.allow_external_fonts = definition.allow_external_fonts;
  policy.blocked_domains = definition.blocked_domains;

  if (!definition.allow_eval) {
    policy.eval_block_script = BuildEvalBlockScript();
  }

  return policy;
}

std::string BuildEvalBlockScript() {
  // Override eval, Function constructor, and string-argument forms of
  // setTimeout/setInterval to prevent dynamic code execution.
  // Idempotent: each override is wrapped in try/catch so repeated injections
  // (e.g., across iframes or hot-route reconfigures) silently no-op after the
  // first successful defineProperty call.
  return
    "(function(){\n"
    "'use strict';\n"
    "var _deny=function(name){\n"
    "  return function(){\n"
    "    throw new EvalError('[Veyra] ' + name + ' is not permitted under this persona policy.');\n"
    "  };\n"
    "};\n"
    "try{\n"
    "  Object.defineProperty(window,'eval',{value:_deny('eval'),writable:false,configurable:false});\n"
    "}catch(e){}\n"
    "try{\n"
    "  var _OrigFunction=Function;\n"
    "  var _blockedFunction=function(){\n"
    "    if(arguments.length>0&&typeof arguments[arguments.length-1]==='string'){\n"
    "      throw new EvalError('[Veyra] Function constructor with string body is not permitted.');\n"
    "    }\n"
    "    return _OrigFunction.apply(this,arguments);\n"
    "  };\n"
    "  _blockedFunction.prototype=_OrigFunction.prototype;\n"
    "  Object.defineProperty(window,'Function',{value:_blockedFunction,writable:false,configurable:false});\n"
    "}catch(e){}\n"
    "try{\n"
    "  var _origST=window.setTimeout;\n"
    "  Object.defineProperty(window,'setTimeout',{\n"
    "    value:function(fn,delay){\n"
    "      if(typeof fn==='string')throw new EvalError('[Veyra] setTimeout with string is not permitted.');\n"
    "      return _origST.apply(this,arguments);\n"
    "    },writable:false,configurable:false\n"
    "  });\n"
    "  var _origSI=window.setInterval;\n"
    "  Object.defineProperty(window,'setInterval',{\n"
    "    value:function(fn,delay){\n"
    "      if(typeof fn==='string')throw new EvalError('[Veyra] setInterval with string is not permitted.');\n"
    "      return _origSI.apply(this,arguments);\n"
    "    },writable:false,configurable:false\n"
    "  });\n"
    "}catch(e){}\n"
    "})();\n";
}

bool IsBlockedDomain(const std::vector<std::string>& blocked_domains,
                     const std::string& host) {
  for (const std::string& domain : blocked_domains) {
    if (HostMatchesDomain(host, domain)) {
      return true;
    }
  }
  return false;
}

}  // namespace veyra
