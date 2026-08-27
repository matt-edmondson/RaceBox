// Unit tests for menu navigation and persisted settings semantics.
#include "TestMain.hpp"

#include "../../main/config/Settings.hpp"
#include "../../main/ui/Menu.hpp"

using ktsu::racebox::config::Settings;
using ktsu::racebox::config::SpeedUnits;
using ktsu::racebox::ui::Menu;
using ktsu::racebox::ui::MenuItem;
using ktsu::racebox::ui::MenuNavigator;

namespace {

// A small root -> settings -> units tree mirroring the real menu shape.
struct Tree {
  MenuItem unitsItems[2];
  MenuItem settingsItems[2];
  MenuItem rootItems[3];
  Menu units;
  Menu settings;
  Menu root;

  Tree() {
    unitsItems[0].label = "km/h";
    unitsItems[1].label = "mph";
    units.title = "Units";
    units.items = unitsItems;
    units.itemCount = 2;

    settingsItems[0].label = "Brightness";
    settingsItems[1].label = "Units";
    settingsItems[1].submenu = &units;
    settings.title = "Settings";
    settings.items = settingsItems;
    settings.itemCount = 2;

    rootItems[0].label = "Start";
    rootItems[1].label = "Lap";
    rootItems[2].label = "Settings";
    rootItems[2].submenu = &settings;
    root.title = "Main";
    root.items = rootItems;
    root.itemCount = 3;
  }
};

} // namespace

TEST(MenuRotationWrapsBothWays) {
  Tree t;
  MenuNavigator nav(&t.root);
  CHECK_EQ(nav.currentIndex(), 0);
  nav.rotate(1);
  CHECK_EQ(nav.currentIndex(), 1);
  nav.rotate(2);                 // 1 + 2 == 3 -> wraps to 0
  CHECK_EQ(nav.currentIndex(), 0);
  nav.rotate(-1);                // wraps backwards to the last item
  CHECK_EQ(nav.currentIndex(), 2);
}

TEST(MenuRotationHandlesLargeDeltas) {
  Tree t;
  MenuNavigator nav(&t.root);
  nav.rotate(31);                // 31 % 3 == 1
  CHECK_EQ(nav.currentIndex(), 1);
  nav.rotate(-30);               // back by a whole number of cycles
  CHECK_EQ(nav.currentIndex(), 1);
}

TEST(ConfirmDescendsAndBackReturns) {
  Tree t;
  MenuNavigator nav(&t.root);
  CHECK_EQ(nav.depth(), size_t{1});
  CHECK(!nav.canGoBack());

  nav.rotate(2);                 // "Settings"
  nav.confirm();
  CHECK_EQ(nav.depth(), size_t{2});
  CHECK(nav.canGoBack());
  CHECK(nav.currentMenu() == &t.settings);
  CHECK_EQ(nav.currentIndex(), 0); // submenu starts at the top

  nav.back();
  CHECK_EQ(nav.depth(), size_t{1});
  CHECK(nav.currentMenu() == &t.root);
  CHECK_EQ(nav.currentIndex(), 2); // parent cursor is preserved
}

TEST(BackAtRootIsANoOp) {
  Tree t;
  MenuNavigator nav(&t.root);
  nav.back();
  nav.back();
  CHECK_EQ(nav.depth(), size_t{1});
  CHECK(nav.currentMenu() == &t.root);
}

TEST(ConfirmInvokesTheItemAction) {
  Tree t;
  int fired = 0;
  t.rootItems[0].action = [&fired]() { ++fired; };
  MenuNavigator nav(&t.root);
  nav.confirm();
  CHECK_EQ(fired, 1);
  CHECK_EQ(nav.depth(), size_t{1}); // an action must not change depth
}

TEST(CurrentItemTracksTheCursor) {
  Tree t;
  MenuNavigator nav(&t.root);
  CHECK(nav.currentItem() == &t.rootItems[0]);
  nav.rotate(1);
  CHECK(nav.currentItem() == &t.rootItems[1]);
  nav.rotate(1);
  nav.confirm();                 // into Settings
  CHECK(nav.currentItem() == &t.settingsItems[0]);
}

TEST(NavigatorToleratesEmptyAndNullMenus) {
  MenuNavigator nav;
  CHECK(nav.currentMenu() == nullptr);
  CHECK(nav.currentItem() == nullptr);
  nav.rotate(1);
  nav.confirm();
  nav.back();                    // must not crash

  Menu empty;
  empty.title = "Empty";
  MenuNavigator nav2(&empty);
  CHECK(nav2.currentItem() == nullptr);
  nav2.rotate(3);
  nav2.confirm();
  CHECK_EQ(nav2.currentIndex(), 0);
}

TEST(SetRootResetsNavigationState) {
  Tree t;
  MenuNavigator nav(&t.root);
  nav.rotate(2);
  nav.confirm();
  CHECK_EQ(nav.depth(), size_t{2});
  nav.setRoot(&t.root);
  CHECK_EQ(nav.depth(), size_t{1});
  CHECK_EQ(nav.currentIndex(), 0);
}

TEST(SpeedUnitConversion) {
  Settings s;
  CHECK(s.speedUnits() == SpeedUnits::Kmh);
  CHECK_NEAR(s.displaySpeed(100.0f), 100.0, 1e-3);
  CHECK(std::string(s.speedUnitLabel()) == "km/h");

  s.toggleSpeedUnits();
  CHECK(s.speedUnits() == SpeedUnits::Mph);
  CHECK_NEAR(s.displaySpeed(160.9344f), 100.0, 1e-3);
  CHECK(std::string(s.speedUnitLabel()) == "mph");

  s.toggleSpeedUnits();
  CHECK(s.speedUnits() == SpeedUnits::Kmh);
}

TEST(BrightnessIsClampedToUsableRange) {
  Settings s;
  s.setBrightness(0);
  CHECK_EQ(int(s.brightness()), int(Settings::kMinBrightness)); // never fully dark
  s.setBrightness(255);
  CHECK_EQ(int(s.brightness()), int(Settings::kMaxBrightness));
  s.setBrightness(55);
  CHECK_EQ(int(s.brightness()), 55);
}

TEST(BrightnessSteppingSaturates) {
  Settings s;
  s.setBrightness(100);
  s.adjustBrightness(+1);
  CHECK_EQ(int(s.brightness()), 100);
  for (int i = 0; i < 50; ++i) s.adjustBrightness(-1);
  CHECK_EQ(int(s.brightness()), int(Settings::kMinBrightness));
  s.adjustBrightness(+2);
  CHECK_EQ(int(s.brightness()), int(Settings::kMinBrightness + 2 * Settings::kBrightnessStep));
}
