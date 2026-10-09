#include "crash_catalog.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace {

bool fail(const char *what) {
  std::fprintf(stderr, "CrashCatalog: %s\n", what);
  return false;
}

std::string manifestText(const char *path) {
  std::ifstream manifest(path);
  std::stringstream text;
  text << manifest.rdbuf();
  return text.str();
}

} // namespace

// argv: the two executable.json paths in catalog order.
int main(int argc, char **argv) {
  const crash::CrashCatalog catalog;
  const auto titles = catalog.titles();
  const char *const slugs[] = {"crash1", "crash2"};
  const char *const discKeys[] = {"PSXPORT_CRASH1_DISC", "PSXPORT_CRASH2_DISC"};
  if (argc != 3 || titles.size() != 2) {
    return fail("catalog is not exactly Crash 1 and 2") ? 0 : 1;
  }
  for (std::size_t index = 0; index < titles.size(); ++index) {
    const psx::host::TitleIdentity &identity = titles[index];
    if (identity.slug != slugs[index]) {
      return fail("slug order is not crash1, crash2") ? 0 : 1;
    }
    const std::string json = manifestText(argv[index + 1]);
    const std::string sha = "\"sha256\": \"" + std::string(identity.sha256) + "\"";
    const std::string name = "\"executable\": \"" + std::string(identity.serial) + "\"";
    const std::string size = "\"file_size\": " + std::to_string(identity.fileSize) + ",";
    if (json.find(sha) == std::string::npos || json.find(name) == std::string::npos ||
        json.find(size) == std::string::npos) {
      return fail("identity disagrees with executable.json") ? 0 : 1;
    }
    if (std::string(catalog.runtime(index).discEnvVar()) != discKeys[index]) {
      return fail("runtime does not name its title's disc key") ? 0 : 1;
    }
  }
  std::printf("CrashCatalog: identities match executable.json\n");
  return 0;
}
