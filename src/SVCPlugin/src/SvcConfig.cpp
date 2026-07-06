#include "SvcConfig.h"
#include <xml/DOMParser.h>

namespace
{
    bool parseTypeStr(const td::String& s, SvcNodeType& type)
    {
        if (s.compareCI("SVCTypeI"))
        {
            type = SvcNodeType::SvcTypeI;
            return true;
        }
        if (s.compareCI("SVCTypeII"))
        {
            type = SvcNodeType::SvcTypeII;
            return true;
        }
        return false;
    }
}

bool SvcConfig::loadFromFile(const td::String& fileName, td::String& status)
{
    xml::FileParser parser;
    if (!parser.parseFile(fileName))
    {
        status = "ERROR! Cannot parse the SVC config XML file!";
        return false;
    }

    auto root = parser.getRootNode();
    if (!root.isOk() || root->getName().cCompare("SVCConverterConfig") != 0)
    {
        status = "ERROR! Root node of the config file must be <SVCConverterConfig>!";
        return false;
    }

    // Files
    auto files = root.getChildNode("Files");
    if (files.isOk())
    {
        auto inputFile = files.getChildNode("InputFile");
        if (inputFile.isOk())
            inputFile.getAttribValue("path", _inputFilePath);

        auto outputFile = files.getChildNode("OutputFile");
        if (outputFile.isOk())
        {
            outputFile.getAttribValue("name", _outputName);
            outputFile.getAttribValue("directory", _outputDirectory);
        }
    }

    // Simulation (-> Header section of the generated .dmodl)
    auto sim = root.getChildNode("Simulation");
    if (sim.isOk())
    {
        auto n = sim.getChildNode("maxIter");
        if (n.isOk()) _simOptions.maxIter = n->getValue().toINT4();
        n = sim.getChildNode("report");
        if (n.isOk()) _simOptions.report = n->getValue();
        n = sim.getChildNode("maxReps");
        if (n.isOk()) _simOptions.maxReps = n->getValue().toINT4();
        n = sim.getChildNode("startTime");
        if (n.isOk()) _simOptions.startTime = n->getValue().toDouble();
        n = sim.getChildNode("dTime");
        if (n.isOk()) _simOptions.dTime = n->getValue().toDouble();
        n = sim.getChildNode("endTime");
        if (n.isOk()) _simOptions.endTime = n->getValue().toDouble();
        n = sim.getChildNode("method");
        if (n.isOk()) _simOptions.method = n->getValue();
    }

    // Options
    auto opts = root.getChildNode("Options");
    if (opts.isOk())
    {
        auto n = opts.getChildNode("EnableNLEComments");
        if (n.isOk()) _options.enableNLEComments = n->getValue().toBoolean();
        n = opts.getChildNode("GeneratorLimits");
        if (n.isOk()) _options.generatorLimits = n->getValue().toBoolean();
        n = opts.getChildNode("UseDCAngleInit");
        if (n.isOk()) _options.useDCAngleInit = n->getValue().toBoolean();
    }

    // Defaults (must be parsed before SVCNodes, since node configs start from these)
    auto defI = root.getChildNode("DefaultsSVCTypeI");
    if (defI.isOk())
    {
        defI.getAttribValue("K", _defaultsTypeI.K);
        defI.getAttribValue("KD", _defaultsTypeI.KD);
        defI.getAttribValue("KM", _defaultsTypeI.KM);
        defI.getAttribValue("T1", _defaultsTypeI.T1);
        defI.getAttribValue("T2", _defaultsTypeI.T2);
        defI.getAttribValue("TM", _defaultsTypeI.TM);
        defI.getAttribValue("xL", _defaultsTypeI.xL);
        defI.getAttribValue("xC", _defaultsTypeI.xC);
        defI.getAttribValue("sigmaMax", _defaultsTypeI.sigmaMax);
        defI.getAttribValue("sigmaMin", _defaultsTypeI.sigmaMin);
    }

    auto defII = root.getChildNode("DefaultsSVCTypeII");
    if (defII.isOk())
    {
        defII.getAttribValue("Kr", _defaultsTypeII.Kr);
        defII.getAttribValue("Tr", _defaultsTypeII.Tr);
        defII.getAttribValue("bMax", _defaultsTypeII.bMax);
        defII.getAttribValue("bMin", _defaultsTypeII.bMin);
    }

    // SVC node assignment
    auto svcNodesParent = root.getChildNode("SVCNodes");
    if (svcNodesParent.isOk())
    {
        auto node = svcNodesParent.getChildNode("Node");
        while (node.isOk())
        {
            SvcNodeConfig cfg;
            td::UINT4 busNum = 0;
            if (!node.getAttribValue("bus", busNum))
            {
                status = "ERROR! <Node> is missing the required 'bus' attribute!";
                return false;
            }
            cfg.busNumber = busNum;

            td::String typeStr;
            node.getAttribValue("type", typeStr);
            if (!parseTypeStr(typeStr, cfg.type))
            {
                status.format("ERROR! Unknown SVC node type '%s' for bus %u! Use SVCTypeI or SVCTypeII.", typeStr.c_str(), busNum);
                return false;
            }

            cfg.typeI = _defaultsTypeI;
            cfg.typeII = _defaultsTypeII;

            double vrefVal = 1.0;
            if (node.getAttribValue("vref", vrefVal))
            {
                cfg.hasVref = true;
                cfg.vref = vrefVal;
            }

            if (cfg.type == SvcNodeType::SvcTypeI)
            {
                node.getAttribValue("K", cfg.typeI.K);
                node.getAttribValue("KD", cfg.typeI.KD);
                node.getAttribValue("KM", cfg.typeI.KM);
                node.getAttribValue("T1", cfg.typeI.T1);
                node.getAttribValue("T2", cfg.typeI.T2);
                node.getAttribValue("TM", cfg.typeI.TM);
                node.getAttribValue("xL", cfg.typeI.xL);
                node.getAttribValue("xC", cfg.typeI.xC);
                node.getAttribValue("sigmaMax", cfg.typeI.sigmaMax);
                node.getAttribValue("sigmaMin", cfg.typeI.sigmaMin);
            }
            else
            {
                node.getAttribValue("Kr", cfg.typeII.Kr);
                node.getAttribValue("Tr", cfg.typeII.Tr);
                node.getAttribValue("bMax", cfg.typeII.bMax);
                node.getAttribValue("bMin", cfg.typeII.bMin);
            }

            _nodes.push_back() = cfg;
            ++node;
        }
    }

    return true;
}

const SvcNodeConfig* SvcConfig::findNode(td::UINT4 busNumber) const
{
    auto n = _nodes.size();
    for (td::UINT4 i = 0; i < n; ++i)
    {
        if (_nodes[i].busNumber == busNumber)
            return &_nodes[i];
    }
    return nullptr;
}
