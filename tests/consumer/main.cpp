#include <holonight/config/config.h>

int main() {
  const HoloNight::Config::Appearance appearance = HoloNight::Config::defaults();
  return HoloNight::Config::validate(appearance).empty() ? 0 : 1;
}
