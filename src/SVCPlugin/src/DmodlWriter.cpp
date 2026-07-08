#include "DmodlWriter.h"
#include <cnt/PushBackVector.h>
#include <cmath>

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    inline td::UINT4 busNumberOfRow(const MatpowerCase& mpCase, td::UINT4 row)
    {
        return td::UINT4(mpCase.getBus().getManipulator()(row, busCol::busI));
    }

    // Voltage magnitude to use as both the initial guess and (for Slack/PV) the regulated
    // set-point: the connected generator's Vg if one exists at this bus (status>0), since
    // that is the actual operating voltage MATPOWER assumes for Slack/PV buses; otherwise
    // falls back to the case file's own bus.Vm column (typically the flat-start 1.0).
    double effectiveVm(const MatpowerCase& mpCase, td::UINT4 row)
    {
        td::UINT4 num = busNumberOfRow(mpCase, row);
        auto genM = mpCase.getGen().getManipulator();
        auto nGen = mpCase.getNoOfGens();
        for (td::UINT4 g = 0; g < nGen; ++g)
        {
            if (genM(g, genCol::status) <= 0.0)
                continue;
            if (td::UINT4(genM(g, genCol::bus)) == num)
                return genM(g, genCol::Vg);
        }
        double vm = mpCase.getBus().getManipulator()(row, busCol::Vm);
        return (vm > 0.0) ? vm : 1.0;
    }

    // Appends one summed injection expression (P-part if isP, Q-part otherwise) for a bus,
    // over all of its YBusEntry neighbors (including its own diagonal entry), in polar form:
    //   P_i = sum_k v_i*v_k*absY_ik*cos(phi_i-phi_k-angleY_ik)
    //   Q_i = sum_k v_i*v_k*absY_ik*sin(phi_i-phi_k-angleY_ik)
    // The diagonal (k==i) term is written without a unary minus inside the trig argument
    // (cos(angleY_ii) instead of cos(-angleY_ii), with the sign for Q pulled out front as
    // "-" between terms) to match the exact syntax style of dTwin's own bundled polar
    // power-flow examples - see PROGRESS_NOTES.txt for why this matters.
    void appendInjectionSum(td::MutableString& out, const MatpowerCase& mpCase,
                             const cnt::PushBackVector<const YBusEntry*>& rowEntries, bool isP)
    {
        bool first = true;
        auto n = rowEntries.size();
        for (td::UINT4 k = 0; k < n; ++k)
        {
            const YBusEntry* pEntry = rowEntries[k];
            td::UINT4 i = busNumberOfRow(mpCase, pEntry->row);
            td::UINT4 j = busNumberOfRow(mpCase, pEntry->col);
            bool diag = (i == j);
            bool negative = (!isP && diag);

            if (!first)
                out.append(negative ? " - " : " + ");
            else if (negative)
                out.append("-");
            first = false;

            const char* fn = isP ? "cos" : "sin";
            if (diag)
                out.appendFormat("v_%u*v_%u*absY_%u_%u*%s(angleY_%u_%u)", i, i, i, i, fn, i, i);
            else
                out.appendFormat("v_%u*v_%u*absY_%u_%u*%s(phi_%u-phi_%u-angleY_%u_%u)", i, j, i, j, fn, i, j, i, j);
        }
        if (first)
            out.append("0");
    }

    // B_SVC(sigma) = B_C - B_TCR(sigma), Milano eq. 19.1 (see PROGRESS_NOTES.txt for the
    // full re-derivation): the fixed capacitor's positive/capacitive susceptance 1/xC minus
    // the TCR branch's inductive susceptance (2*pi-2*sigma+sin(2*sigma))/(pi*xL), so that
    // sigma->pi (TCR blocked) gives the fully-capacitive +1/xC, and sigma->pi/2 (TCR fully
    // conducting) gives 1/xC-1/xL. Positive result = capacitive = voltage-supporting.
    void appendSvcTypeIBSuscExpr(td::MutableString& out, td::UINT4 busNum)
    {
        out.appendFormat("((%.14f*xL_%u/xC_%u - 2*%.14f + 2*sigma_%u - sin(2*sigma_%u)) / (%.14f*xL_%u))",
            kPi, busNum, busNum, kPi, busNum, busNum, kPi, busNum);
    }
}

void DmodlWriter::writeHeader(td::MutableString& out) const
{
    const auto& sim = _config.getSimulationOptions();
    out.appendFormat(
        "Header:\n"
        "\tmaxIter = %d\t//Maximum number of iterations for NL, WLS, and DAE problems\n"
        "\treport = %s\t//Solved/unsolved cases with iterations and debug information\n"
        "\tmaxReps = %d\t//Used to limit repetitions (-1: no limit)\n"
        "\toutToTxt = false\n"
        "\ttxtFile = \"\"\n"
        "\tstartTime = %.6f\n"
        "\tdTime = %.6f\n"
        "\tendTime = %.6f\n"
        "end\n"
        "//Model created by the SVC (Static Var Compensator Type I/II) dTwin plugin converter\n",
        sim.maxIter, sim.report.c_str(), sim.maxReps, sim.startTime, sim.dTime, sim.endTime);

    td::String modelName;
    modelName.format("SVC dynamic model for %s", _caseName.c_str());
    out.appendFormat("\nModel [type=DAE domain=real method=%s name=\"%s\"]:\n", sim.method.c_str(), modelName.c_str());
}

void DmodlWriter::writeVars(td::MutableString& out) const
{
    out.append("\nVars [out=true]:\n");

    auto nBus = _case.getNoOfBuses();
    for (td::UINT4 row = 0; row < nBus; ++row)
    {
        td::UINT4 num = busNumberOfRow(_case, row);
        double vm0 = effectiveVm(_case, row);
        double phi0 = _theta0.getNoOfRows() > row ? _theta0.getManipulator()(row, 0) : 0.0;
        out.appendFormat("\tv_%u = %.8f; phi_%u = %.8f;\n", num, vm0, num, phi0);

        const SvcNodeConfig* pSvc = _config.findNode(num);
        if (pSvc)
        {
            if (pSvc->type == SvcNodeType::SvcTypeI)
                out.appendFormat("\tvM_%u = %.8f; sigma_%u = 0.0;\n", num, vm0, num);
            else
                out.appendFormat("\tbSvc_%u = 0.0;\n", num);

            // vh/bEff/q used to be PostProc-derived, but dTwin's solver crashes when a
            // PostProc section is present on models above a certain variable-count
            // threshold (reproduced even with a single trivial PostProc line on a plain,
            // SVC-free network - see PROGRESS_NOTES.txt). Defining them as ordinary
            // algebraic Vars/NLEs instead avoids PostProc entirely and is unaffected by
            // that bug, since plain Vars+NLEs at this scale are confirmed to work.
            out.appendFormat("\tvh_%u = %.8f;\n", num, vm0);
            if (pSvc->type == SvcNodeType::SvcTypeI)
                out.appendFormat("\tbEff_%u = 0.0; q_%u = 0.0;\n", num, num);
            else
                out.appendFormat("\tq_%u = 0.0;\n", num);
        }
    }
}

void DmodlWriter::writeParams(td::MutableString& out) const
{
    out.append("\nParams:\n");

    auto nBus = _case.getNoOfBuses();
    auto busM = _case.getBus().getManipulator();
    auto genM = _case.getGen().getManipulator();
    auto nGen = _case.getNoOfGens();
    double baseMVA = _case.getBaseMVA();

    for (td::UINT4 row = 0; row < nBus; ++row)
    {
        td::UINT4 num = busNumberOfRow(_case, row);
        const SvcNodeConfig* pSvc = _config.findNode(num);
        double vm0 = effectiveVm(_case, row);
        double pd = busM(row, busCol::Pd) / baseMVA;
        double qd = busM(row, busCol::Qd) / baseMVA;

        if (pSvc)
        {
            double vref = pSvc->hasVref ? pSvc->vref : vm0;
            out.appendFormat("\t// Node %u - SVC %s\n", num, pSvc->type == SvcNodeType::SvcTypeI ? "Type I" : "Type II");
            out.appendFormat("\tP_%u_load = %.8f; Q_%u_load = %.8f;\n", num, pd, num, qd);
            out.appendFormat("\tP_%u_inj_spec = %.8f;\n", num, -pd);
            out.appendFormat("\tvref_%u = %.8f [out=true];\n", num, vref);

            if (pSvc->type == SvcNodeType::SvcTypeI)
            {
                const auto& p = pSvc->typeI;
                out.appendFormat(
                    "\tK_%u = %.8f; KD_%u = %.8f; KM_%u = %.8f;\n"
                    "\tT1_%u = %.8f; T2_%u = %.8f; TM_%u = %.8f;\n"
                    "\txL_%u = %.8f; xC_%u = %.8f;\n"
                    "\tsigmaMax_%u = %.8f; sigmaMin_%u = %.8f;\n",
                    num, p.K, num, p.KD, num, p.KM,
                    num, p.T1, num, p.T2, num, p.TM,
                    num, p.xL, num, p.xC,
                    num, p.sigmaMax, num, p.sigmaMin);
            }
            else
            {
                const auto& p = pSvc->typeII;
                out.appendFormat(
                    "\tKr_%u = %.8f; Tr_%u = %.8f;\n"
                    "\tbMax_%u = %.8f; bMin_%u = %.8f;\n",
                    num, p.Kr, num, p.Tr, num, p.bMax, num, p.bMin);
            }
        }
        else
        {
            BusType type = BusType(int(busM(row, busCol::type)));
            if (type == BusType::Slack)
            {
                double va0 = busM(row, busCol::Va) * kPi / 180.0;
                out.appendFormat("\t// Node %u - Slack\n", num);
                out.appendFormat("\tv_%u_spec = %.8f; phi_%u_spec = %.8f;\n", num, vm0, num, va0);
            }
            else if (type == BusType::PV)
            {
                double pGen = 0.0, vg = vm0;
                for (td::UINT4 g = 0; g < nGen; ++g)
                {
                    if (genM(g, genCol::status) <= 0.0)
                        continue;
                    if (td::UINT4(genM(g, genCol::bus)) != num)
                        continue;
                    pGen += genM(g, genCol::Pg);
                    vg = genM(g, genCol::Vg);
                }
                out.appendFormat("\t// Node %u - PV\n", num);
                out.appendFormat("\tP_%u_inj_spec = %.8f; v_%u_spec = %.8f;\n", num, (pGen - busM(row, busCol::Pd)) / baseMVA, num, vg);
            }
            else // PQ (and isolated buses handled the same way; MATPOWER cases used here have none)
            {
                double pGen = 0.0, qGen = 0.0;
                for (td::UINT4 g = 0; g < nGen; ++g)
                {
                    if (genM(g, genCol::status) <= 0.0)
                        continue;
                    if (td::UINT4(genM(g, genCol::bus)) != num)
                        continue;
                    pGen += genM(g, genCol::Pg);
                    qGen += genM(g, genCol::Qg);
                }
                out.appendFormat("\t// Node %u - PQ\n", num);
                out.appendFormat("\tP_%u_inj_spec = %.8f; Q_%u_inj_spec = %.8f;\n",
                    num, (pGen - busM(row, busCol::Pd)) / baseMVA, num, (qGen - busM(row, busCol::Qd)) / baseMVA);
            }
        }
    }

    out.append("\n\t// Y-bus matrix in polar form (|Y|, angle[Y]), built with natID's sparse assembly (see YBusBuilder)\n");
    const auto& entries = _ybus.getEntries();
    auto nEntries = entries.size();
    for (td::UINT4 k = 0; k < nEntries; ++k)
    {
        const YBusEntry& e = entries[k];
        td::UINT4 i = busNumberOfRow(_case, e.row);
        td::UINT4 j = busNumberOfRow(_case, e.col);
        double absY = std::sqrt(e.G * e.G + e.B * e.B);
        double angleY = std::atan2(e.B, e.G);
        out.appendFormat("\tabsY_%u_%u=%.10f; angleY_%u_%u=%.10f;\n", i, j, absY, i, j, angleY);
    }
}

void DmodlWriter::writeODEs(td::MutableString& out) const
{
    out.append("\nODEs:\n");

    auto nBus = _case.getNoOfBuses();
    for (td::UINT4 row = 0; row < nBus; ++row)
    {
        td::UINT4 num = busNumberOfRow(_case, row);
        const SvcNodeConfig* pSvc = _config.findNode(num);
        if (!pSvc)
            continue;

        td::String vh;
        vh.format("v_%u", num);

        if (pSvc->type == SvcNodeType::SvcTypeI)
        {
            out.appendFormat("\t// SVC Type I dynamics on bus %u (Milano eq. 19.3)\n", num);
            out.appendFormat("\tvM_%u' = (KM_%u*%s - vM_%u)/TM_%u\n", num, num, vh.c_str(), num, num);
            out.appendFormat("\tsigma_%u' = -KD_%u + K_%u*T1_%u/(T2_%u*TM_%u)*(vM_%u - KM_%u*%s) + K_%u/T2_%u*(vref_%u - vM_%u)\n",
                num, num, num, num, num, num, num, num, vh.c_str(), num, num, num, num);
        }
        else
        {
            out.appendFormat("\t// SVC Type II dynamics on bus %u (Milano eq. 19.4)\n", num);
            out.appendFormat("\tbSvc_%u' = (Kr_%u*(vref_%u - %s) - bSvc_%u)/Tr_%u\n", num, num, num, vh.c_str(), num, num);
        }
    }
}

void DmodlWriter::writeNLEs(td::MutableString& out, const std::function<void(double)>& progressCb) const
{
    out.append("\nNLEs:\n");

    auto nBus = _case.getNoOfBuses();
    auto busM = _case.getBus().getManipulator();
    bool comments = _config.getOptions().enableNLEComments;

    for (td::UINT4 row = 0; row < nBus; ++row)
    {
        td::UINT4 num = busNumberOfRow(_case, row);
        const SvcNodeConfig* pSvc = _config.findNode(num);

        cnt::PushBackVector<const YBusEntry*> rowEntries;
        rowEntries.reserve(8);
        _ybus.getRowEntries(row, rowEntries);

        if (pSvc)
        {
            if (comments)
                out.appendFormat("\t// Node %u - SVC %s\n", num, pSvc->type == SvcNodeType::SvcTypeI ? "Type I" : "Type II");

            out.append("\t");
            appendInjectionSum(out, _case, rowEntries, true);
            out.appendFormat(" - P_%u_inj_spec = 0\n", num);

            out.append("\t");
            appendInjectionSum(out, _case, rowEntries, false);
            out.append(" - ");
            if (pSvc->type == SvcNodeType::SvcTypeI)
                appendSvcTypeIBSuscExpr(out, num);
            else
                out.appendFormat("bSvc_%u", num);
            out.appendFormat("*(v_%u*v_%u) + Q_%u_load = 0\n", num, num, num);

            out.appendFormat("\tvh_%u - v_%u = 0\n", num, num);
            if (pSvc->type == SvcNodeType::SvcTypeI)
            {
                out.appendFormat("\tbEff_%u - ", num);
                appendSvcTypeIBSuscExpr(out, num);
                out.append(" = 0\n");
                out.appendFormat("\tq_%u - bEff_%u*(v_%u*v_%u) = 0\n", num, num, num, num);
            }
            else
            {
                out.appendFormat("\tq_%u - bSvc_%u*(v_%u*v_%u) = 0\n", num, num, num, num);
            }
        }
        else
        {
            BusType type = BusType(int(busM(row, busCol::type)));
            if (comments)
            {
                const char* typeName = (type == BusType::Slack) ? "Slack" : (type == BusType::PV) ? "PV" : "PQ";
                out.appendFormat("\t// Node %u - %s\n", num, typeName);
            }

            if (type == BusType::Slack)
            {
                out.appendFormat("\tphi_%u - phi_%u_spec = 0\n", num, num);
                out.appendFormat("\tv_%u - v_%u_spec = 0\n", num, num);
            }
            else if (type == BusType::PV)
            {
                out.append("\t");
                appendInjectionSum(out, _case, rowEntries, true);
                out.appendFormat(" - P_%u_inj_spec = 0\n", num);
                out.appendFormat("\tv_%u - v_%u_spec = 0\n", num, num);
            }
            else // PQ
            {
                out.append("\t");
                appendInjectionSum(out, _case, rowEntries, true);
                out.appendFormat(" - P_%u_inj_spec = 0\n", num);

                out.append("\t");
                appendInjectionSum(out, _case, rowEntries, false);
                out.appendFormat(" - Q_%u_inj_spec = 0\n", num);
            }
        }

        if (progressCb && (nBus > 0))
            progressCb(double(row + 1) / double(nBus));
    }
}

void DmodlWriter::writeDmodl(td::MutableString& out, const std::function<void(double)>& progressCb) const
{
    writeHeader(out);
    writeVars(out);
    writeParams(out);
    writeODEs(out);
    writeNLEs(out, progressCb);
    out.append("\nend\n");
}

void DmodlWriter::writeVmodl(td::MutableString& out) const
{
    out.append("Header:\n\tnewTab = false\n\tdrawPlots = true\nend\n");
    out.appendFormat("\nModel [name=\"SVC visualization for %s\"]:\n", _caseName.c_str());
    out.append("\nPlots [backColor=auto]:\n");

    auto nBus = _case.getNoOfBuses();
    for (td::UINT4 row = 0; row < nBus; ++row)
    {
        td::UINT4 num = busNumberOfRow(_case, row);
        const SvcNodeConfig* pSvc = _config.findNode(num);
        if (!pSvc)
            continue;

        bool isTypeI = (pSvc->type == SvcNodeType::SvcTypeI);
        const char* typeLabel = isTypeI ? "SVC Type I" : "SVC Type II";

        out.appendFormat("\t//Voltage magnitude at bus %u (%s)\n", num, typeLabel);
        out.appendFormat("\tlinePlot [xLabel=\"Time [s]\" yLabel=\"V [pu]\" name=\"Bus %u voltage\" anchor=TR legend=true nCols=1 anchorX=130 anchorY=35]:\n", num);
        out.append("\t\t@x << t\n");
        out.appendFormat("\t\t@y << vh_%u [colorL=black colorD=red width=2 name=\"vh_%u\"]\n", num, num);
        out.appendFormat("\t\t@y << vref_%u [colorL=darkGreen colorD=green width=2 name=\"vref_%u\"]\n", num, num);
        out.append("\tend\n");

        if (isTypeI)
        {
            out.appendFormat("\t//SVC Type I state variables at bus %u\n", num);
            out.appendFormat("\tlinePlot [xLabel=\"Time [s]\" yLabel=\"sigma [rad], vM [pu]\" name=\"Bus %u SVC-I states\" anchor=TR legend=true nCols=1 anchorX=130 anchorY=35]:\n", num);
            out.append("\t\t@x << t\n");
            out.appendFormat("\t\t@y << sigma_%u [colorL=darkBlue colorD=cyan width=2 name=\"sigma_%u\"]\n", num, num);
            out.appendFormat("\t\t@y << vM_%u [colorL=darkYellow colorD=lightBlue width=2 name=\"vM_%u\"]\n", num, num);
            out.append("\tend\n");
        }
        else
        {
            out.appendFormat("\t//SVC Type II susceptance state at bus %u\n", num);
            out.appendFormat("\tlinePlot [xLabel=\"Time [s]\" yLabel=\"bSvc [pu]\" name=\"Bus %u SVC-II state\" anchor=TR legend=true nCols=1 anchorX=130 anchorY=35]:\n", num);
            out.append("\t\t@x << t\n");
            out.appendFormat("\t\t@y << bSvc_%u [colorL=darkBlue colorD=cyan width=2 name=\"bSvc_%u\"]\n", num, num);
            out.append("\tend\n");
        }

        out.appendFormat("\t//Reactive power injected by the SVC at bus %u\n", num);
        out.appendFormat("\tlinePlot [xLabel=\"Time [s]\" yLabel=\"Q [pu]\" name=\"Bus %u SVC Q\" anchor=TR legend=true nCols=1 anchorX=130 anchorY=35]:\n", num);
        out.append("\t\t@x << t\n");
        out.appendFormat("\t\t@y << q_%u [colorL=darkRed colorD=magenta width=2 name=\"q_%u\"]\n", num, num);
        out.append("\tend\n");
    }

    out.append("end //end of the visual model\n");
}
