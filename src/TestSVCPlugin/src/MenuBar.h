#pragma once
#include <gui/MenuBar.h>

class MenuBar : public gui::MenuBar
{
private:
    gui::SubMenu subApp;
    gui::SubMenu subFile;
protected:
    void populateSubAppMenu()
    {
        auto& items = subApp.getItems();
        items[0].initAsQuitAppActionItem(tr("Quit"), "q");
    }

    void populateSubFileMenu()
    {
        auto& items = subFile.getItems();
        items[0].initAsActionItem(tr("open"), 10, "o");
    }
public:
    MenuBar()
    : gui::MenuBar(2)
    , subApp(10, "App", 1)
    , subFile(20, "Model", 1)
    {
        populateSubAppMenu();
        populateSubFileMenu();
        setMenu(0, &subApp);
        setMenu(1, &subFile);
    }

};
