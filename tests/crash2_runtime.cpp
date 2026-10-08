#include "crash2_runtime.h"
#include "title_runtime_contract.h"

int main() {
  return verifyTitleRuntimeContract<crash2::Crash2Runtime>("Crash2Runtime", true, crash::NativeFrameLoopState::Missing);
}
