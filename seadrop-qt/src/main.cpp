/**
 * @file main.cpp
 * @brief SeaDrop Qt Application Entry Point
 */

#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  // Application metadata
  app.setApplicationName("SeaDrop");
  app.setApplicationVersion("1.0.0");
  app.setOrganizationName("SeaDrop");
  app.setOrganizationDomain("seadrop.io");

  // Use Fusion style for consistent cross-platform look
  app.setStyle(QStyleFactory::create("Fusion"));

  // Create and show main window
  MainWindow window;
  window.show();

  return app.exec();
}
