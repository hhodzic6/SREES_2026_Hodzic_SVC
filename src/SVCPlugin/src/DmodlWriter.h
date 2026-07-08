#pragma once
#include <td/String.h>
#include <td/MutableString.h>
#include <functional>
#include "MatpowerCase.h"
#include "YBusBuilder.h"
#include "SvcConfig.h"

// Generates the textual .dmodl (DAE model, dTwin's own scripting DSL) and the companion
// .vmodl (visualization/plots) content for a MATPOWER case where a subset of buses has
// been replaced by a dynamic SVC Type I / Type II model (Milano, Ch. 19.1).
//
// This class only produces text (td::MutableString buffers); it performs no file or
// archive I/O itself - see Converter for that (keeps the DAE-generation logic testable
// independently of the plugin/archive machinery).
class DmodlWriter
{
protected:
    const MatpowerCase& _case;
    const YBusBuilder& _ybus;
    const SvcConfig& _config;
    const dense::DblMatrix& _theta0;
    const td::String& _caseName;

    void writeHeader(td::MutableString& out) const;
    void writeVars(td::MutableString& out) const;
    void writeParams(td::MutableString& out) const;
    void writeODEs(td::MutableString& out) const;
    void writeNLEs(td::MutableString& out, const std::function<void(double)>& progressCb) const;

public:
    DmodlWriter(const MatpowerCase& mpCase, const YBusBuilder& ybus, const SvcConfig& config,
                const dense::DblMatrix& theta0, const td::String& caseName)
        : _case(mpCase), _ybus(ybus), _config(config), _theta0(theta0), _caseName(caseName)
    {
    }

    // progressCb receives values in [0,1] as NLE generation (the dominant cost for large
    // cases such as case300) advances; called from whatever thread runs the conversion.
    void writeDmodl(td::MutableString& out, const std::function<void(double)>& progressCb) const;
    void writeVmodl(td::MutableString& out) const;
};
