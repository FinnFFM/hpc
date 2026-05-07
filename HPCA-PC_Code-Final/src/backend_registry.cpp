#include "backend_registry.h"

#include <algorithm>

BackendRegistry& BackendRegistry::Instance() {
  static BackendRegistry instance;
  return instance;
}

void BackendRegistry::Register(const std::string& name, BackendFn fn) {
  backends_[name] = std::move(fn);
}

BackendFn BackendRegistry::Find(const std::string& name) const {
  auto it = backends_.find(name);
  if (it == backends_.end()) {
    return BackendFn{};
  }
  return it->second;
}

std::vector<std::string> BackendRegistry::Names() const {
  std::vector<std::string> names;
  names.reserve(backends_.size());
  for (const auto& entry : backends_) {
    names.push_back(entry.first);
  }
  return names;
}

BackendRegister::BackendRegister(const std::string& name, BackendFn fn) {
  BackendRegistry::Instance().Register(name, std::move(fn));
}
