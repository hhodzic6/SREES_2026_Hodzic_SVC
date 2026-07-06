#include "SVCPlugin.h"
#include <sc/IPlugin.h>
#include "WindowPlugin.h"
#include <td/StringUtils.h>
#include <mu/ScopedCLocale.h>
#include <fo/FileOperations.h>
#include <arch/MemoryOut.h>

class Plugin : public sc::IPlugin
{
    MemoryArchiveContainer _outArchives;
    WindowPlugin* _pWnd = nullptr;
public:
    Plugin()
    {
        //dont change this
        for (size_t i=0; i< size_t(ArchType::NA); ++i)
            _outArchives[i] = nullptr;
    }

    void show(gui::Window* parentWnd, MemoryArchiveContainer& archives, td::UINT4 wndID, const sc::IPlugin::Cleaner& cleaner, const sc::IPlugin::CallBack& onComplete) override final
    {
        //dont change this
        for (size_t i=0; i< size_t(ArchType::NA); ++i)
            _outArchives[i] = archives[i];

        if (_pWnd)
            _pWnd->setFocus();
        else
        {
            _pWnd = new WindowPlugin(parentWnd, this, onComplete, cleaner, wndID);
            _pWnd->open();
        }
    }

    td::String getMenuName() const override final
    {
        return "SVC Type I/II Converter";
    }

    arch::MemoryOut* getArchive(sc::IPlugin::ArchType type) override final
    {
        //dont change this
        auto iType = size_t(type);
        if (iType >= getMaxSupportedArchiveParts())
            return nullptr;

        return _outArchives[size_t(type)];
    }

    MemoryArchiveContainer& getArchives() override final
    {
        //dont change this
        return _outArchives;
    }

    td::String getOutFileName() const override final
    {
        //dont change this
        assert(_pWnd);
        return _pWnd->getOutFileName();
    }

    size_t getMaxSupportedArchiveParts() const override final
    {
        return size_t(ArchType::NA); //don't change this
    }

    ModelType getModelType() const override final
    {
        //this converter always produces a Differential-Algebraic-Equation model
        //(algebraic power-flow NLEs for standard buses + ODEs for SVC Type I/II buses)
        return ModelType::DAE;
    }

    void onClosedPluginWindow()
    {
        //dont change this
        _pWnd = nullptr;
    }
};

static Plugin s_plugin;

void onClosedPluginWindow()
{
    s_plugin.onClosedPluginWindow();
}

//Plugin requires extern C
extern "C"
{

PLUGIN_API sc::IPlugin* getPluginInterface()
{
    return &s_plugin;
}
}

bool convertToSvcModel(const td::String& svcConfigFileName, const td::String& caseFileOverride,
                        const td::String& outFileName, sc::IPlugin* pIPlugin,
                        const std::function<void(double)>& progressCb,
                        const SimOverrides& overrides, td::String& status)
{
    mu::ScopedCLocale scopedLocale; //set numerics locale to "C" for the scope of this function

    if (outFileName.isEmpty())
    {
        status = "ERROR! Empty output file name";
        return false;
    }

    td::MutableString dmodlText;
    dmodlText.reserve(1 << 16);
    td::MutableString vmodlText;
    vmodlText.reserve(1 << 12);

    if (!Converter::run(svcConfigFileName, caseFileOverride, dmodlText, vmodlText, progressCb, status, &overrides))
        return false;

    auto pDigitModel = pIPlugin->getArchive(sc::IPlugin::ArchType::DigitalModel);
    if (!pDigitModel)
    {
        status = "ERROR! Plugin archive (DigitalModel) not available!";
        return false;
    }
    auto& memDigitalOut = *pDigitModel;
    memDigitalOut.put(dmodlText.c_str(), dmodlText.length());

    std::ofstream fDigital;
    if (!fo::createTextFile(fDigital, outFileName))
    {
        status = "ERROR! Cannot create output .dmodl file!";
        return false;
    }
    memDigitalOut.writeToFile(fDigital);
    fDigital.close();

    auto pVisualModel = pIPlugin->getArchive(sc::IPlugin::ArchType::VisualModel);
    if (pVisualModel)
    {
        auto& memVisualOut = *pVisualModel;
        memVisualOut.put(vmodlText.c_str(), vmodlText.length());

        td::String vmodlFileName = fo::replaceFileExtension<false>(outFileName, ".vmodl");
        std::ofstream fVisual;
        if (fo::createTextFile(fVisual, vmodlFileName))
        {
            memVisualOut.writeToFile(fVisual);
            fVisual.close();
        }
    }

    return true;
}
