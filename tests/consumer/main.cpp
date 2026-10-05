#include <holonight/config/config.h>

int main() {
  using namespace HoloNight::Config;
  const auto snapshot = parseDocument("version = 2\n[theme]\naccent = 'red'\n");
  if (!snapshot) {
    return 1;
  }
  const auto decoded = decodeAppearanceDocument(*snapshot.value);
  if (!decoded || decoded.value->appearance.theme.accent != "red") {
    return 2;
  }
  const auto staged = stageAppearanceDocument({}, {});
  if (staged.result.status != SaveStatus::StorageFailure || staged.previous.has_value()) {
    return 4;
  }
  const auto patched = patchDocument(
      *snapshot.value,
      {{.key = {"theme", "accent"}, .baseline = snapshot.value->value({"theme", "accent"}), .pending = std::nullopt}},
      appearanceDocumentSchema());
  return patched.status == SaveStatus::Success &&
                 decodeAppearanceDocument(*patched.snapshot).value->appearance == defaults()
             ? 0
             : 3;
}
