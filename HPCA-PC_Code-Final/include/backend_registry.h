#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

using BackendFn = std::function<int(int argc, char** argv)>;

class BackendRegistry {
 public:
  static BackendRegistry& Instance();

  void Register(const std::string& name, BackendFn fn);
  BackendFn Find(const std::string& name) const;
  std::vector<std::string> Names() const;

 private:
  std::map<std::string, BackendFn> backends_;
};

class BackendRegister {
 public:
  BackendRegister(const std::string& name, BackendFn fn);
};
