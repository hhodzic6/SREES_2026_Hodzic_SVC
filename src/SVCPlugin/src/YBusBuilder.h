#pragma once
#include <dense/Matrix.h>
#include <cnt/PushBackVector.h>
#include "MatpowerCase.h"

// One nonzero (row,col) entry of the bus admittance matrix Ybus = G + jB (0-based bus
// row indices, matching MatpowerCase::getBus() row order). Both (i,j) and (j,i) are
// stored as separate entries so that emitting one NLE per bus is a straight iteration.
struct YBusEntry
{
    td::UINT4 row = 0;
    td::UINT4 col = 0;
    double G = 0.0;
    double B = 0.0;
};

// Builds Ybus from a MatpowerCase's branch + bus shunt data.
//
// Matrix strategy (see PROGRESS_NOTES.txt for the rationale):
//  - Assembly/accumulation uses a dense::DblMatrix scratch pair (G,B), since per-branch
//    contributions land on only 4 (or 1, for shunts) cells and dense element accumulation
//    via operator() is the natural fit; case9..case300 (<=300 buses) are trivially small
//    for a dense NxN scratch.
//  - The susceptance part is additionally assembled into a genuine natID
//    sparse::IDblMatrix (triplet insertion) and solved with natID's sparse direct solver
//    (sparse::createDblSolver) to obtain a real DC-power-flow angle estimate, used only to
//    seed better initial conditions in the generated model's Vars section instead of a
//    flat start. This is the "sparse matrix" usage requested for the Ybus-related part of
//    the pipeline; it does not replace or duplicate the dense scratch above.
class YBusBuilder
{
protected:
    const MatpowerCase& _case;
    cnt::PushBackVector<YBusEntry> _entries;

public:
    explicit YBusBuilder(const MatpowerCase& mpCase) : _case(mpCase) {}

    // Builds Ybus. If solveDCAngles is true, also runs the sparse DC angle estimate and
    // fills thetaOut (Nx1, radians, one row per bus, in MatpowerCase::getBus() row order).
    // If the sparse solve fails for any reason, thetaOut is filled with zeros (safe
    // fallback - only affects the quality of the initial guess, never correctness of the
    // generated NLEs/ODEs).
    bool build(bool solveDCAngles, dense::DblMatrix& thetaOut, td::String& status);

    const cnt::PushBackVector<YBusEntry>& getEntries() const { return _entries; }

    // All entries whose row == busRow, in ascending column order.
    void getRowEntries(td::UINT4 busRow, cnt::PushBackVector<const YBusEntry*>& out) const;
};
