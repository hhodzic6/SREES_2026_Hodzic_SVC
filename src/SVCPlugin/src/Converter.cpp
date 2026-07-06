#include "Converter.h"
#include "SvcConfig.h"
#include "MatpowerCase.h"
#include "YBusBuilder.h"
#include "DmodlWriter.h"
#include <fo/FileOperations.h>

bool Converter::run(const td::String& svcConfigFileName, const td::String& caseFileOverride,
                     td::MutableString& dmodlText, td::MutableString& vmodlText,
                     const std::function<void(double)>& progressCb, td::String& status,
                     const SimOverrides* pOverrides)
{
    if (progressCb) progressCb(0.0);

    SvcConfig config;
    if (!config.loadFromFile(svcConfigFileName, status))
        return false;
    if (pOverrides && pOverrides->apply)
        config.applySimulationOverrides(pOverrides->maxIter, pOverrides->dTime, pOverrides->endTime, pOverrides->useDCAngleInit);
    if (progressCb) progressCb(0.05);

    td::String caseFileName = !caseFileOverride.isEmpty() ? caseFileOverride : config.getInputFilePath();
    if (caseFileName.isEmpty())
    {
        status = "ERROR! No MATPOWER input file specified (neither in the config nor in the GUI)!";
        return false;
    }
    if (!fo::fileExists(caseFileName))
    {
        status.format("ERROR! MATPOWER input file not found: %s", caseFileName.c_str());
        return false;
    }

    MatpowerCase mpCase;
    if (!mpCase.loadFromFile(caseFileName, status))
        return false;
    if (progressCb) progressCb(0.25);

    YBusBuilder ybus(mpCase);
    dense::DblMatrix theta0;
    if (!ybus.build(config.getOptions().useDCAngleInit, theta0, status))
        return false;
    if (progressCb) progressCb(0.4);

    td::String caseName = fo::getFilename(caseFileName);
    DmodlWriter writer(mpCase, ybus, config, theta0, caseName);

    auto nleProgress = [&progressCb](double frac)
    {
        if (progressCb)
            progressCb(0.4 + 0.5 * frac);
    };
    writer.writeDmodl(dmodlText, nleProgress);
    if (progressCb) progressCb(0.95);

    writer.writeVmodl(vmodlText);
    if (progressCb) progressCb(1.0);

    status = "OK! Conversion completed successfully.";
    return true;
}
