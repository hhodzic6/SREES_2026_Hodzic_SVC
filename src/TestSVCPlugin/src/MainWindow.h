#pragma once
#include <gui/Window.h>
#include "MenuBar.h"
#include "ToolBar.h"
#include "MainView.h"

class MainWindow : public gui::Window
{
protected:
    MenuBar _mainMenuBar;
    ToolBar _toolBar;
    MainView _mainView;
public:
    MainWindow()
    : gui::Window(gui::Size(1000, 800))
    {
        setTitle(tr("appTitle"));
        _mainMenuBar.setAsMain(this);
        setToolBar(_toolBar);
        setCentralView(&_mainView);
        forwardMessagesTo(&_mainView); //forward menu and Toolbar messages to the main view
    }

};
