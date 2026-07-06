#pragma once
#include <compiler/Definitions.h>
#include <sc/IPlugin.h>
#include <gui/LineEdit.h>
#include <functional>
#include "Converter.h"

#ifdef MU_WINDOWS
	#ifdef PLUGIN_EXPORTS
	#define PLUGIN_API __declspec(dllexport)
	#else
	#define PLUGIN_API __declspec(dllimport)
	#endif
#else
	#ifdef PLUGIN_EXPORTS
    #define PLUGIN_API __attribute__((visibility("default")))
	#else
    #define PLUGIN_API
	#endif
#endif

void onClosedPluginWindow();

// Runs the SVC conversion (MatpowerCase -> Ybus -> DAE .dmodl/.vmodl, see Converter) and
// writes the result both into the plugin's archives (for dTwin) and to outFileName/.vmodl
// on disk (so it can be inspected directly, same as EQPlugin's createModel).
//
// NOTE ON THREADING: unlike the EQPlugin example this converter may run on a worker
// thread (see ViewConv.h), so - deliberately, unlike EQPlugin's createModel - this
// function reports status through a plain td::String (safe from any thread) instead of a
// gui::LineEdit&; only the caller, back on the GUI thread, copies that text into a widget.
bool convertToSvcModel(const td::String& svcConfigFileName, const td::String& caseFileOverride,
                        const td::String& outFileName, sc::IPlugin* pIPlugin,
                        const std::function<void(double)>& progressCb,
                        const SimOverrides& overrides, td::String& status); //in SVCPlugin.cpp
