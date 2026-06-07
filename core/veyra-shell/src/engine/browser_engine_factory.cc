#include "veyra/engine/browser_engine.h"
#include "veyra/engine/webkitgtk_browser_engine.h"

namespace veyra {

std::unique_ptr<BrowserEngine> CreateBrowserEngine(std::string* /*error*/) {
  return std::make_unique<WebKitGtkBrowserEngine>();
}

}  // namespace veyra
