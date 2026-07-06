//
// Converter tab: select a MATPOWER .m case, an SVC XML config, and an output .dmodl file,
// then run the conversion. The actual conversion (MatpowerCase parse -> Ybus -> DAE text
// generation, see Converter.h) runs on a background std::thread so the GUI stays
// responsive; real-time progress is relayed to the main thread via
// gui::thread::asyncExecInMainThread and shown on a gui::ProgressIndicator - this is the
// "conversion on one thread, real-time progress indicator on a second thread" requirement.
//
#pragma once
#include <gui/View.h>
#include <gui/Label.h>
#include <gui/Button.h>
#include <gui/LineEdit.h>
#include <gui/TextEdit.h>
#include <gui/ProgressIndicator.h>
#include <gui/HorizontalLayout.h>
#include <gui/GridLayout.h>
#include <gui/GridComposer.h>
#include <gui/FileDialog.h>
#include <gui/Thread.h>
#include <fo/FileOperations.h>
#include <thread>
#include <memory>
#include "ViewOptions.h"
#include "SVCPlugin.h"

class ViewConv : public gui::View
{
protected:
    sc::IPlugin* _pIPlugin;
    sc::IPlugin::CallBack _onComplete;
    ViewOptions* _pViewOptions = nullptr;

    gui::Label _lblFnIn;
    gui::LineEdit _editFnIn;
    gui::Button _btnSelectInFn;
    gui::Label _lblConfigIn;
    gui::LineEdit _editConfigIn;
    gui::Button _btnSelectConfigFn;
    gui::Label _lblFnOut;
    gui::LineEdit _editFnOut;
    gui::Button _btnSelectOutFn;
    gui::Label _lblStatus;
    gui::LineEdit _editStatus;
    gui::ProgressIndicator _progInd;
    gui::TextEdit _te;
    gui::Button _btnInfo;
    gui::Button _btnConvert;

    gui::HorizontalLayout _hlButtons;
    gui::GridLayout _gl;

    td::UINT4 _wndID;

    std::thread _thread;
    bool _running = false;
    // Shared with the worker thread's queued main-thread callbacks; guards against this
    // ViewConv being closed/destroyed while a conversion is still in flight (the worker
    // thread only ever *reads/copies* plain data, never touches GUI widgets directly - see
    // startConversion() - but the queued callbacks must still avoid touching 'this' once
    // the view is gone).
    std::shared_ptr<bool> _alive = std::make_shared<bool>(true);

protected:
    void updateProgress(double val)
    {
        _progInd.setValue(val); //val expected in [0,1]
    }

    void onConversionFinished(bool ok, const td::String& status, const td::String& outFileName)
    {
        _editStatus = status;
        if (_thread.joinable())
            _thread.join();
        _running = false;

        if (ok)
        {
            if (fo::fileExists(outFileName))
            {
                td::String content;
                if (fo::loadBinaryFile(outFileName, content))
                    _te.setText(content);
            }
            _onComplete(_pIPlugin);
            gui::Window* pWnd = getParentWindow();
            pWnd->close();
            onClosedPluginWindow();
        }
    }

    void startConversion()
    {
        if (_running)
            return;

        td::String inputFileName = _editFnIn.getText();
        td::String configFileName = _editConfigIn.getText();
        td::String outFileName = _editFnOut.getText();

        if (configFileName.isEmpty())
        {
            _editStatus = "ERROR! Select an SVC config XML file!";
            return;
        }
        if (!fo::fileExists(configFileName))
        {
            _editStatus = "ERROR! SVC config XML file doesn't exist!";
            return;
        }
        if (outFileName.isEmpty())
        {
            _editStatus = "ERROR! Empty output file name";
            return;
        }

        SimOverrides overrides = _pViewOptions ? _pViewOptions->getOverrides() : SimOverrides();

        _running = true;
        _progInd.setValue(0.0);
        _editStatus = "Converting...";

        std::shared_ptr<bool> aliveGuard = _alive;
        auto progressCb = [this, aliveGuard](double frac)
        {
            //called from the worker thread - marshal to the GUI thread before touching any widget
            auto fnUpdate = [this, aliveGuard](td::Variant v)
            {
                if (!*aliveGuard)
                    return; //this ViewConv was destroyed while the conversion was running
                updateProgress(v.r8Val());
            };
            gui::thread::asyncExecInMainThread(fnUpdate, td::Variant(frac));
        };

        _thread = std::thread([this, inputFileName, configFileName, outFileName, overrides, progressCb, aliveGuard]()
        {
            td::String status;
            bool ok = convertToSvcModel(configFileName, inputFileName, outFileName, _pIPlugin, progressCb, overrides, status);

            auto fnDone = [this, ok, status, outFileName, aliveGuard]()
            {
                if (!*aliveGuard)
                    return; //this ViewConv was destroyed while the conversion was running
                onConversionFinished(ok, status, outFileName);
            };
            gui::thread::asyncExecInMainThread(fnDone);
        });
    }

    void handleUserActions()
    {
        _btnInfo.onClick([this]{
            td::String fileName = _editFnIn.getText();
            if (!fo::fileExists(fileName))
            {
                _editStatus = "ERROR! File doesn't exist!";
                return;
            }
            td::String content;
            if (!fo::loadBinaryFile(fileName, content))
            {
                _editStatus = "ERROR! Cannot load the file content!";
                return;
            }
            _editStatus = "INFO! Content ok!";
            _te.setText(content);
        });

        _btnSelectInFn.onClick([this]{
            gui::OpenFileDialog::show(this, tr("openMatpowerCase"), "*.m", _wndID + 1000, [this](gui::FileDialog* pDlg)
            {
                  auto status = pDlg->getStatus();
                  if (status == gui::FileDialog::Status::OK)
                  {
                      td::String fileName = pDlg->getFileName();
                      if (fileName.isEmpty())
                          return;
                      _editFnIn = fileName;
                      _editFnIn.setFocus();
                  }
            });
        });

        _btnSelectConfigFn.onClick([this]{
            gui::OpenFileDialog::show(this, tr("openSvcConfig"), "*.xml", _wndID + 1500, [this](gui::FileDialog* pDlg)
            {
                  auto status = pDlg->getStatus();
                  if (status == gui::FileDialog::Status::OK)
                  {
                      td::String fileName = pDlg->getFileName();
                      if (fileName.isEmpty())
                          return;
                      _editConfigIn = fileName;
                      _editConfigIn.setFocus();
                  }
            });
        });

        _btnSelectOutFn.onClick([this]{
            gui::SaveFileDialog::show(this, tr("createDmodl"), "*.dmodl", _wndID + 2000, [this](gui::FileDialog* pDlg)
            {
                  auto status = pDlg->getStatus();
                  if (status == gui::FileDialog::Status::OK)
                  {
                      td::String fileName = pDlg->getFileName();
                      if (fileName.isEmpty())
                          return;
                      _editFnOut = fileName;
                      _editFnOut.setFocus();
                  }
            });
        });

        _btnConvert.onClick([this]{
            startConversion();
        });
    }
    ViewConv() = delete;
public:
    ViewConv(sc::IPlugin* pIPlugin, const sc::IPlugin::CallBack& onComplete)
    : _pIPlugin(pIPlugin)
    , _onComplete(onComplete)
    , _lblFnIn(tr("MATPOWER case (.m):"))
    , _lblConfigIn(tr("SVC config (.xml):"))
    , _lblFnOut(tr("Out File Name:"))
    , _lblStatus(tr("Status:"))
    , _btnSelectInFn("…")
    , _btnSelectConfigFn("…")
    , _btnSelectOutFn("…")
    , _btnInfo(tr("Info"))
    , _btnConvert(tr("Convert"))
    , _hlButtons(3)
    , _gl(7, 3)
    {
        assert(_pIPlugin);
        _editStatus.setAsReadOnly();
        _te.setAsReadOnly();
        _editFnIn.setToolTip(tr("PLSVC_EnterFN"));

        gui::GridComposer gc(_gl);
        gc.appendRow(_lblFnIn) << _editFnIn << _btnSelectInFn;
        gc.appendRow(_lblConfigIn) << _editConfigIn << _btnSelectConfigFn;
        gc.appendRow(_lblFnOut) << _editFnOut << _btnSelectOutFn;
        gc.appendRow(_lblStatus); gc.appendCol(_editStatus, 0);
        gc.appendRow(_progInd, 0); //0:span to end - real-time progress from the worker thread
        gc.appendRow(_te, 0); //0:span to end
        _hlButtons.appendSpacer() << _btnInfo << _btnConvert;
        gc.appendRow(_hlButtons, 0); //0:span to end

        setLayout(&_gl);

        handleUserActions();
    }

    ~ViewConv()
    {
        *_alive = false; //tell any in-flight worker-thread callback to no-op
        if (_thread.joinable())
            _thread.join();
    }

    void setOptions(ViewOptions* pViewOptions)
    {
        _pViewOptions = pViewOptions;
    }

    td::String getOutFileName() const
    {
        td::String strOutFn = _editFnOut.getText();
        return strOutFn;
    }

};
