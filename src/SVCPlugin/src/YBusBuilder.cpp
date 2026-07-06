#include "YBusBuilder.h"
#include <sparse/ISolver.h>
#include <complex>
#include <cmath>

namespace
{
    constexpr double kPi = 3.14159265358979323846;
}

bool YBusBuilder::build(bool solveDCAngles, dense::DblMatrix& thetaOut, td::String& status)
{
    td::UINT4 nBus = _case.getNoOfBuses();
    td::UINT4 nBranch = _case.getNoOfBranches();

    dense::DblMatrix accG(nBus, nBus, nullptr, true);
    dense::DblMatrix accB(nBus, nBus, nullptr, true);
    auto mG = accG.getManipulator();
    auto mB = accB.getManipulator();

    auto branchM = _case.getBranch().getManipulator();
    for (td::UINT4 k = 0; k < nBranch; ++k)
    {
        if (branchM(k, branchCol::status) == 0.0)
            continue;

        td::UINT4 fBusNum = td::UINT4(branchM(k, branchCol::fbus));
        td::UINT4 tBusNum = td::UINT4(branchM(k, branchCol::tbus));
        int fRow = _case.findBusRow(fBusNum);
        int tRow = _case.findBusRow(tBusNum);
        if (fRow < 0 || tRow < 0)
        {
            status.format("ERROR! Branch references unknown bus (%u or %u)!", fBusNum, tBusNum);
            return false;
        }

        double r = branchM(k, branchCol::r);
        double x = branchM(k, branchCol::x);
        double b = branchM(k, branchCol::b);
        double tap = branchM(k, branchCol::tap);
        double shiftDeg = branchM(k, branchCol::shift);
        if (tap == 0.0)
            tap = 1.0;

        std::complex<double> z(r, x);
        std::complex<double> ySeries = (std::abs(z) > 1e-12) ? (1.0 / z) : std::complex<double>(0.0, 0.0);
        std::complex<double> yShunt(0.0, b / 2.0);

        double shiftRad = shiftDeg * kPi / 180.0;
        std::complex<double> t = std::polar(tap, shiftRad);

        std::complex<double> Yff = (ySeries + yShunt) / std::norm(t); // norm(t) == |t|^2
        std::complex<double> Ytt = (ySeries + yShunt);
        std::complex<double> Yft = -ySeries / std::conj(t);
        std::complex<double> Ytf = -ySeries / t;

        mG(fRow, fRow) += Yff.real(); mB(fRow, fRow) += Yff.imag();
        mG(tRow, tRow) += Ytt.real(); mB(tRow, tRow) += Ytt.imag();
        mG(fRow, tRow) += Yft.real(); mB(fRow, tRow) += Yft.imag();
        mG(tRow, fRow) += Ytf.real(); mB(tRow, fRow) += Ytf.imag();
    }

    double baseMVA = _case.getBaseMVA();
    auto busM = _case.getBus().getManipulator();
    for (td::UINT4 i = 0; i < nBus; ++i)
    {
        double gs = busM(i, busCol::Gs);
        double bs = busM(i, busCol::Bs);
        if (gs != 0.0 || bs != 0.0)
        {
            mG(i, i) += gs / baseMVA;
            mB(i, i) += bs / baseMVA;
        }
    }

    _entries.reserve(nBus * 6);
    for (td::UINT4 i = 0; i < nBus; ++i)
    {
        for (td::UINT4 j = 0; j < nBus; ++j)
        {
            double g = mG(i, j);
            double bVal = mB(i, j);
            if (g != 0.0 || bVal != 0.0)
            {
                YBusEntry& e = _entries.push_back();
                e.row = i;
                e.col = j;
                e.G = g;
                e.B = bVal;
            }
        }
    }

    if (!solveDCAngles)
    {
        thetaOut.reserve(nBus, 1, nullptr, true);
        return true;
    }

    // ---- DC power-flow angle estimate: B'*theta = P, via natID's sparse solver ----
    // Only used to seed better initial e_i/f_i values in the generated Vars section;
    // any failure here falls back to a flat start (theta=0) and is not a hard error.
    int slackRow = -1;
    for (td::UINT4 i = 0; i < nBus; ++i)
    {
        if (int(busM(i, busCol::type)) == int(BusType::Slack))
        {
            slackRow = int(i);
            break;
        }
    }
    if (slackRow < 0)
        slackRow = 0;

    dense::DblMatrix pInj(nBus, 1, nullptr, true);
    auto mP = pInj.getManipulator();
    for (td::UINT4 i = 0; i < nBus; ++i)
        mP(i, 0) = -busM(i, busCol::Pd) / baseMVA;

    auto genM = _case.getGen().getManipulator();
    td::UINT4 nGen = _case.getNoOfGens();
    for (td::UINT4 g = 0; g < nGen; ++g)
    {
        if (genM(g, genCol::status) <= 0.0)
            continue;
        int row = _case.findBusRow(td::UINT4(genM(g, genCol::bus)));
        if (row >= 0)
            mP(row, 0) += genM(g, genCol::Pg) / baseMVA;
    }

    thetaOut.reserve(nBus, 1, nullptr, true);

    int n = int(nBus);
    int nzEstimate = int(_entries.size()) + n + 1;
    sparse::DblSolver* pRawSolver = sparse::createDblSolver(n, nzEstimate);
    if (!pRawSolver)
    {
        status = "WARNING! Sparse solver could not be created; SVC vref/initial angles use a flat start.";
        return true;
    }
    sparse::DblSolverReleaser pSolver(pRawSolver);

    auto nEntries = _entries.size();
    for (td::UINT4 idx = 0; idx < nEntries; ++idx)
    {
        const YBusEntry& e = _entries[idx];
        if (int(e.row) == slackRow)
            continue; // slack row replaced by the identity row below
        pSolver->addTriple(int(e.row), int(e.col), e.B);
    }
    pSolver->addTriple(slackRow, slackRow, 1.0);

    for (td::UINT4 i = 0; i < nBus; ++i)
        pSolver->setRHS(int(i), (int(i) == slackRow) ? 0.0 : mP(i, 0));

    if (!pSolver->factorize() || !pSolver->solve())
    {
        status = "WARNING! Sparse DC angle solve did not converge; using a flat start for initial angles.";
        return true;
    }

    auto mTheta = thetaOut.getManipulator();
    for (td::UINT4 i = 0; i < nBus; ++i)
        mTheta(i, 0) = pSolver->x(int(i));

    return true;
}

void YBusBuilder::getRowEntries(td::UINT4 busRow, cnt::PushBackVector<const YBusEntry*>& out) const
{
    auto n = _entries.size();
    for (td::UINT4 i = 0; i < n; ++i)
    {
        if (_entries[i].row == busRow)
            out.push_back() = &_entries[i];
    }
}
