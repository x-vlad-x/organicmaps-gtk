#pragma once

#include <gio/gio.h>

namespace lichen
{
class MainWindow;
void RegisterDebugActions(MainWindow & window, GApplication * app);
}  // namespace lichen
