#pragma once
#include <gui/View.h>
#include <gui/Label.h>
#include <gui/NumericEdit.h>
#include <gui/CheckBox.h>
#include <gui/TextEdit.h>
#include <gui/GridLayout.h>
#include <gui/GridComposer.h>
#include "Converter.h"

// Simulation-level (Header/solver) options. The SVC bus/type assignment itself always
// comes from the XML config selected in the Converter tab; this tab only lets the user
// optionally override the few solver knobs on top of it, without having to edit the XML.
class ViewOptions : public gui::View
{
protected:
    gui::CheckBox _cbApplyOverrides;
    gui::Label _lblMaxIter;
    gui::NumericEdit _neMaxIter;
    gui::Label _lblDeltaTime;
    gui::NumericEdit _neDeltaTime;
    gui::Label _lblEndTime;
    gui::NumericEdit _neEndTime;
    gui::CheckBox _cbUseDCAngleInit;
    gui::TextEdit _te;
    gui::GridLayout _gl;

public:
    ViewOptions()
        : _cbApplyOverrides(tr("Override Header/solver options below (otherwise use the XML config values):"))
        , _lblMaxIter(tr("Max iterations:"))
        , _neMaxIter(td::int4)
        , _lblDeltaTime(tr("dTime [s]:"))
        , _neDeltaTime(td::real4, gui::LineEdit::Messages::DoNotSend, false, tr("Integration time step"), 4)
        , _lblEndTime(tr("End time [s]:"))
        , _neEndTime(td::real4, gui::LineEdit::Messages::DoNotSend, true, tr("Simulation end time"), 3)
        , _cbUseDCAngleInit(tr("Seed initial angles from a sparse DC power-flow solve (natID sparse solver)"))
        , _gl(6, 2)
    {
        _cbApplyOverrides.setChecked(false);
        _neMaxIter.setValue(td::INT4(100));
        _neDeltaTime.setValue(0.01f);
        _neEndTime.setValue(10.f);
        _cbUseDCAngleInit.setChecked(true);
        _te.setAsReadOnly();
        _te.setText(tr("SVC_Options_Help"));

        gui::GridComposer gc(_gl);
        gc.appendRow(_cbApplyOverrides, 0);
        gc.appendRow(_lblMaxIter) << _neMaxIter;
        gc.appendRow(_lblDeltaTime) << _neDeltaTime;
        gc.appendRow(_lblEndTime) << _neEndTime;
        gc.appendRow(_cbUseDCAngleInit, 0);
        gc.appendRow(_te, 0); //0:span to end

        setLayout(&_gl);
    }

    SimOverrides getOverrides() const
    {
        SimOverrides ov;
        ov.apply = _cbApplyOverrides.isChecked();
        ov.maxIter = _neMaxIter.getValue().i4Val();
        ov.dTime = double(_neDeltaTime.getValue().r4Val());
        ov.endTime = double(_neEndTime.getValue().r4Val());
        ov.useDCAngleInit = _cbUseDCAngleInit.isChecked();
        return ov;
    }
};
