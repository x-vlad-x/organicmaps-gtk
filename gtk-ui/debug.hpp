#pragma once

#include <gio/gio.h>

namespace omgtk
{
class MainWindow;
void RegisterDebugActions(MainWindow & window, GApplication * app);
}  // namespace omgtk
