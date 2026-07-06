#pragma once
#include <td/String.h>
#include <cnt/PushBackVector.h>

enum class SvcNodeType { Standard, SvcTypeI, SvcTypeII };

// Milano, "Power System Modelling and Scripting", Table 19.1 / Table D.10
struct SvcTypeIParams
{
    double K = 10.0;    // regulator gain [rad/pu]
    double KD = 0.0;    // integral deviation [-]
    double KM = 1.0;    // measure gain [pu/pu]
    double T1 = 0.05;   // transient regulator time constant [s]
    double T2 = 0.01;   // regulator time constant [s]
    double TM = 0.01;   // measure time delay [s]
    double xL = 0.1;    // reactance, inductive [pu]
    double xC = 0.1;    // reactance, capacitive [pu]
    double sigmaMax = 3.14159265358979;  // maximum firing angle [rad]
    double sigmaMin = -3.14159265358979; // minimum firing angle [rad]
};

// Milano, Table 19.2 / Table D.11
struct SvcTypeIIParams
{
    double Kr = 10.0;   // regulator gain [pu/pu]
    double Tr = 0.05;   // regulator time constant [s]
    double bMax = 0.20; // maximum susceptance [pu]
    double bMin = -0.05;// minimum susceptance [pu]
};

struct SvcNodeConfig
{
    td::UINT4 busNumber = 0;
    SvcNodeType type = SvcNodeType::Standard;
    bool hasVref = false;
    double vref = 1.0; // used only if hasVref==true; otherwise the case file's bus Vm is used
    SvcTypeIParams typeI;
    SvcTypeIIParams typeII;
};

struct SimulationOptions
{
    int maxIter = 100;
    td::String report = "AllDetails";
    int maxReps = -1;
    double startTime = 0.0;
    double dTime = 0.01;
    double endTime = 10.0;
    td::String method = "RK2";
};

struct ConverterOptions
{
    bool enableNLEComments = true;
    bool generatorLimits = true;
    bool useDCAngleInit = true;
};

// Parses the SVC converter's XML configuration file: which MATPOWER bus numbers are
// converted to a dynamic SVC Type I / Type II model, and simulation/solver options.
// Any bus NOT listed keeps its original static (Slack/PV/PQ) power-flow model.
class SvcConfig
{
protected:
    td::String _inputFilePath;
    td::String _outputName;
    td::String _outputDirectory;
    SimulationOptions _simOptions;
    ConverterOptions _options;
    SvcTypeIParams _defaultsTypeI;
    SvcTypeIIParams _defaultsTypeII;
    cnt::PushBackVector<SvcNodeConfig> _nodes;

public:
    bool loadFromFile(const td::String& fileName, td::String& status);

    const td::String& getInputFilePath() const { return _inputFilePath; }
    const td::String& getOutputName() const { return _outputName; }
    const td::String& getOutputDirectory() const { return _outputDirectory; }
    const SimulationOptions& getSimulationOptions() const { return _simOptions; }
    const ConverterOptions& getOptions() const { return _options; }

    // Lets the GUI's Options tab override the Header/solver settings loaded from the XML
    // config without having to edit the file by hand (the SVC bus/type assignment itself
    // always comes from the XML - only these simulation-level knobs are overridable here).
    void applySimulationOverrides(int maxIter, double dTime, double endTime, bool useDCAngleInit)
    {
        _simOptions.maxIter = maxIter;
        _simOptions.dTime = dTime;
        _simOptions.endTime = endTime;
        _options.useDCAngleInit = useDCAngleInit;
    }

    td::UINT4 getNoOfSvcNodes() const { return _nodes.size(); }
    const SvcNodeConfig& getSvcNode(td::UINT4 i) const { return _nodes[i]; }

    // nullptr => bus keeps its standard (Slack/PV/PQ) model
    const SvcNodeConfig* findNode(td::UINT4 busNumber) const;
};
