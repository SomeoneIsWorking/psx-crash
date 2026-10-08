#include "crash1_catalog.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace {

bool fail(const char *what) {
  std::fprintf(stderr, "Crash1Catalog: %s\n", what);
  return false;
}

} // namespace

int main(int, char **argv) {
  const crash1::Crash1Catalog catalog;
  const auto titles = catalog.titles();
  if (titles.size() != 1) {
    return fail("catalog is not exactly Crash 1") ? 0 : 1;
  }
  const psx::host::TitleIdentity &identity = titles.front();
  if (identity.slug != "crash1") {
    return fail("slug is not crash1") ? 0 : 1;
  }
  std::ifstream manifest(argv[1]);
  std::stringstream text;
  text << manifest.rdbuf();
  const std::string json = text.str();
  const std::string sha = "\"sha256\": \"" + std::string(identity.sha256) + "\"";
  const std::string name = "\"executable\": \"" + std::string(identity.serial) + "\"";
  const std::string size = "\"file_size\": " + std::to_string(identity.fileSize) + ",";
  if (json.find(sha) == std::string::npos || json.find(name) == std::string::npos ||
      json.find(size) == std::string::npos) {
    return fail("identity disagrees with executable.json") ? 0 : 1;
  }
  if (std::string(catalog.runtime(0).discEnvVar()) != "PSXPORT_CRASH1_DISC") {
    return fail("runtime does not name Crash 1's disc key") ? 0 : 1;
  }
  std::printf("Crash1Catalog: identity matches executable.json\n");
  return 0;
}
