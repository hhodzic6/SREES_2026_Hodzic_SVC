#pragma once
#include <td/String.h>
#include <td/MutableString.h>
#include <functional>

// Orchestrates one full conversion: SVC XML config -> MatpowerCase -> Ybus (natID sparse
// assembly + solve) -> .dmodl/.vmodl text (DmodlWriter). Pure algorithm, no GUI or archive
// dependency, so it is safe to run entirely on a background std::thread - see ViewConv.h,
// which spawns the thread and relays progressCb calls to gui::ProgressIndicator on the
// main thread via gui::thread::asyncExecInMainThread.
// Simulation-level knobs the GUI's Options tab may override on top of the XML config
// (the SVC bus/type assignment itself always comes from the XML config, never from here).
struct SimOverrides
{
    bool apply = false;
    int maxIter = 100;
    double dTime = 0.01;
    double endTime = 10.0;
    bool useDCAngleInit = true;
};

class Converter
{
public:
    // caseFileOverride: if non-empty, used instead of the config's own <InputFile> (lets
    // the GUI's "input file" field take precedence over the config file's default).
    static bool run(const td::String& svcConfigFileName,
                     const td::String& caseFileOverride,
                     td::MutableString& dmodlText,
                     td::MutableString& vmodlText,
                     const std::function<void(double)>& progressCb,
                     td::String& status,
                     const SimOverrides* pOverrides = nullptr);
};
