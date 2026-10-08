#include "crash3_runtime.h"
#include "title_runtime_contract.h"

int main() {
  return verifyTitleRuntimeContract<crash3::Crash3Runtime>("Crash3Runtime", true, crash::NativeFrameLoopState::Missing);
}
