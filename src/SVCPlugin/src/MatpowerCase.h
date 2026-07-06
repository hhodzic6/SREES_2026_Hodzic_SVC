#pragma once
#include <dense/Matrix.h>
#include <td/String.h>

// MATPOWER case-file column layouts (0-based), see MATPOWER's "caseformat".
// Only the columns actually needed for a power-flow-level SVC conversion are used;
// extra columns present in the .m file (gencost, ramp rates, ...) are read but ignored.
namespace busCol
{
    enum { busI = 0, type, Pd, Qd, Gs, Bs, area, Vm, Va, baseKV, zone, Vmax, Vmin, N };
}
namespace genCol
{
    enum { bus = 0, Pg, Qg, Qmax, Qmin, Vg, mBase, status, Pmax, Pmin, N };
}
namespace branchCol
{
    enum { fbus = 0, tbus, r, x, b, rateA, rateB, rateC, tap, shift, status, angmin, angmax, N };
}

enum class BusType { PQ = 1, PV = 2, Slack = 3, Isolated = 4 };

// Parses a MATPOWER .m case file (mpc.baseMVA / mpc.bus / mpc.gen / mpc.branch blocks)
// into natID dense matrices. This is the only supported input format for the converter.
class MatpowerCase
{
protected:
    double _baseMVA = 100.0;
    dense::DblMatrix _bus;
    dense::DblMatrix _gen;
    dense::DblMatrix _branch;

public:
    bool loadFromFile(const td::String& fileName, td::String& status);

    double getBaseMVA() const { return _baseMVA; }
    const dense::DblMatrix& getBus() const { return _bus; }
    const dense::DblMatrix& getGen() const { return _gen; }
    const dense::DblMatrix& getBranch() const { return _branch; }

    td::UINT4 getNoOfBuses() const { return _bus.getNoOfRows(); }
    td::UINT4 getNoOfGens() const { return _gen.getNoOfRows(); }
    td::UINT4 getNoOfBranches() const { return _branch.getNoOfRows(); }

    // Row index (0-based) in _bus for a given MATPOWER bus number, or -1 if not found.
    int findBusRow(td::UINT4 busNumber) const;
};
