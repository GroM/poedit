/*
 *  This file is part of Poedit (http://poedit.net)
 *
 *  Copyright (C) 2000-2014 Vaclav Slavik
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 */

#include <memory>

#include <wx/editlbox.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/config.h>
#include <wx/choicdlg.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/fontutil.h>
#include <wx/fontpicker.h>
#include <wx/filename.h>
#include <wx/filedlg.h>
#include <wx/windowptr.h>
#include <wx/sizer.h>
#include <wx/settings.h>
#include <wx/textwrapper.h>
#include <wx/progdlg.h>
#include <wx/xrc/xmlres.h>
#include <wx/numformatter.h>

#include "prefsdlg.h"
#include "edapp.h"
#include "edframe.h"
#include "catalog.h"
#include "tm/transmem.h"
#include "chooselang.h"
#include "errors.h"
#include "extractor.h"
#include "spellchecking.h"
#include "customcontrols.h"

#ifdef __WXMSW__
#include <winsparkle.h>
#endif

#ifdef USE_SPARKLE
#include "osx_helpers.h"
#endif // USE_SPARKLE

#if defined(USE_SPARKLE) || defined(__WXMSW__)
    #define HAS_UPDATES_CHECK
#endif

namespace
{

class PrefsPanel : public wxPanel
{
public:
    PrefsPanel(wxWindow *parent) : wxPanel(parent), m_inTransfer(false) {}
    PrefsPanel() : wxPanel(), m_inTransfer(false) {}

    bool TransferDataToWindow() override
    {
        if (m_inTransfer)
            return false;
        m_inTransfer = true;
        InitValues(*wxConfig::Get());
        m_inTransfer = false;

        // This is a "bit" of a hack: we take advantage of being in the last point before
        // showing the window and re-layout it on the off chance that some data transfered
        // into the window affected its size. And, currently more importantly, to reflect
        // ExplanationLabel instances' rewrapping.
        Fit();

        return true;
    }

    bool TransferDataFromWindow() override
    {
        if (m_inTransfer)
            return false;
        m_inTransfer = true;
        SaveValues(*wxConfig::Get());
        m_inTransfer = false;
        return true;
    }

protected:
    virtual void InitValues(const wxConfigBase& cfg) = 0;
    virtual void SaveValues(wxConfigBase& cfg) = 0;

private:
    bool m_inTransfer;
};

class GeneralPageWindow : public PrefsPanel
{
public:
    GeneralPageWindow(wxWindow *parent)
    {
        wxXmlResource::Get()->LoadPanel(this, parent, "prefs_general");

#ifdef __WXMSW__
        if (!IsSpellcheckingAvailable())
        {
            auto spellcheck = XRCCTRL(*this, "enable_spellchecking", wxCheckBox);
            spellcheck->Disable();
            spellcheck->SetValue(false);
            // TRANSLATORS: This is a note appended to "Check spelling" when running on older Windows versions
            spellcheck->SetLabel(spellcheck->GetLabel() + " " + _("(requires Windows 8 or newer)"));
        }
#endif

        if (wxPreferencesEditor::ShouldApplyChangesImmediately())
        {
            Bind(wxEVT_CHECKBOX, [=](wxCommandEvent&){ TransferDataFromWindow(); });
            Bind(wxEVT_CHOICE, [=](wxCommandEvent&){ TransferDataFromWindow(); });
            Bind(wxEVT_TEXT, [=](wxCommandEvent&){ TransferDataFromWindow(); });

            // Some settings directly affect the UI, so need a more expensive handler:
            Bind(wxEVT_CHECKBOX, &GeneralPageWindow::TransferDataFromWindowAndUpdateUI, this, XRCID("use_font_list"));
            Bind(wxEVT_CHECKBOX, &GeneralPageWindow::TransferDataFromWindowAndUpdateUI, this, XRCID("use_font_text"));
            Bind(wxEVT_FONTPICKER_CHANGED, &GeneralPageWindow::TransferDataFromWindowAndUpdateUI, this);
            Bind(wxEVT_CHECKBOX, &GeneralPageWindow::TransferDataFromWindowAndUpdateUI, this, XRCID("focus_to_text"));
            Bind(wxEVT_CHECKBOX, &GeneralPageWindow::TransferDataFromWindowAndUpdateUI, this, XRCID("comment_window_editable"));
            Bind(wxEVT_CHECKBOX, &GeneralPageWindow::TransferDataFromWindowAndUpdateUI, this, XRCID("enable_spellchecking"));
        }

        // handle UI updates:
        Bind(wxEVT_UPDATE_UI, [=](wxUpdateUIEvent& e){
            e.Enable(XRCCTRL(*this, "use_font_list", wxCheckBox)->GetValue());
            }, XRCID("font_list"));
        Bind(wxEVT_UPDATE_UI, [=](wxUpdateUIEvent& e){
            e.Enable(XRCCTRL(*this, "use_font_text", wxCheckBox)->GetValue());
            }, XRCID("font_text"));

#if NEED_CHOOSELANG_UI
        Bind(wxEVT_BUTTON, [=](wxCommandEvent&){ ChangeUILanguage(); }, XRCID("ui_language"));
#endif
    }

    void InitValues(const wxConfigBase& cfg) override
    {
        XRCCTRL(*this, "user_name", wxTextCtrl)->SetValue(cfg.Read("translator_name", wxEmptyString));
        XRCCTRL(*this, "user_email", wxTextCtrl)->SetValue(cfg.Read("translator_email", wxEmptyString));
        XRCCTRL(*this, "compile_mo", wxCheckBox)->SetValue(cfg.ReadBool("compile_mo", true));
        XRCCTRL(*this, "show_summary", wxCheckBox)->SetValue(cfg.ReadBool("show_summary", false));
        XRCCTRL(*this, "focus_to_text", wxCheckBox)->SetValue(cfg.ReadBool("focus_to_text", false));
        XRCCTRL(*this, "comment_window_editable", wxCheckBox)->SetValue(cfg.ReadBool("comment_window_editable", false));
        XRCCTRL(*this, "keep_crlf", wxCheckBox)->SetValue(cfg.ReadBool("keep_crlf", true));

        if (IsSpellcheckingAvailable())
        {
            XRCCTRL(*this, "enable_spellchecking", wxCheckBox)->SetValue(cfg.ReadBool("enable_spellchecking", true));
        }

        XRCCTRL(*this, "use_font_list", wxCheckBox)->SetValue(cfg.ReadBool("custom_font_list_use", false));
        XRCCTRL(*this, "use_font_text", wxCheckBox)->SetValue(cfg.ReadBool("custom_font_text_use", false));
        XRCCTRL(*this, "font_list", wxFontPickerCtrl)->SetSelectedFont(wxFont(cfg.Read("custom_font_list_name", wxEmptyString)));
        XRCCTRL(*this, "font_text", wxFontPickerCtrl)->SetSelectedFont(wxFont(cfg.Read("custom_font_text_name", wxEmptyString)));

        wxString format = cfg.Read("crlf_format", "unix");
        int sel;
        if (format == "win") sel = 1;
        else /* "unix" or obsolete settings */ sel = 0;

        XRCCTRL(*this, "crlf_format", wxChoice)->SetSelection(sel);
    }

    void SaveValues(wxConfigBase& cfg) override
    {
        cfg.Write("translator_name", XRCCTRL(*this, "user_name", wxTextCtrl)->GetValue());
        cfg.Write("translator_email", XRCCTRL(*this, "user_email", wxTextCtrl)->GetValue());
        cfg.Write("compile_mo", XRCCTRL(*this, "compile_mo", wxCheckBox)->GetValue());
        cfg.Write("show_summary", XRCCTRL(*this, "show_summary", wxCheckBox)->GetValue());
        cfg.Write("focus_to_text", XRCCTRL(*this, "focus_to_text", wxCheckBox)->GetValue());
        cfg.Write("comment_window_editable", XRCCTRL(*this, "comment_window_editable", wxCheckBox)->GetValue());
        cfg.Write("keep_crlf", XRCCTRL(*this, "keep_crlf", wxCheckBox)->GetValue());

        if (IsSpellcheckingAvailable())
        {
            cfg.Write("enable_spellchecking", XRCCTRL(*this, "enable_spellchecking", wxCheckBox)->GetValue());
        }
       
        wxFont listFont = XRCCTRL(*this, "font_list", wxFontPickerCtrl)->GetSelectedFont();
        wxFont textFont = XRCCTRL(*this, "font_text", wxFontPickerCtrl)->GetSelectedFont();

        cfg.Write("custom_font_list_use", listFont.IsOk() && XRCCTRL(*this, "use_font_list", wxCheckBox)->GetValue());
        cfg.Write("custom_font_text_use", textFont.IsOk() && XRCCTRL(*this, "use_font_text", wxCheckBox)->GetValue());
        if ( listFont.IsOk() )
            cfg.Write("custom_font_list_name", listFont.GetNativeFontInfoDesc());
        if ( textFont.IsOk() )
            cfg.Write("custom_font_text_name", textFont.GetNativeFontInfoDesc());

        static const char *formats[] = { "unix", "win" };
        cfg.Write("crlf_format", formats[XRCCTRL(*this, "crlf_format", wxChoice)->GetSelection()]);

        // On Windows, we must update the UI here; on other platforms, it was done
        // via TransferDataFromWindowAndUpdateUI immediately:
        if (!wxPreferencesEditor::ShouldApplyChangesImmediately())
        {
            PoeditFrame::UpdateAllAfterPreferencesChange();
        }
    }

    void TransferDataFromWindowAndUpdateUI(wxCommandEvent&)
    {
        TransferDataFromWindow();
        PoeditFrame::UpdateAllAfterPreferencesChange();
    }
};

class GeneralPage : public wxPreferencesPage
{
public:
    wxString GetName() const override { return _("General"); }
    wxBitmap GetLargeIcon() const override { return wxArtProvider::GetBitmap("Prefs-General"); }
    wxWindow *CreateWindow(wxWindow *parent) override { return new GeneralPageWindow(parent); }
};



class TMPageWindow : public PrefsPanel
{
public:
    TMPageWindow(wxWindow *parent) : PrefsPanel(parent)
    {
        wxSizer *topsizer = new wxBoxSizer(wxVERTICAL);
#ifdef __WXOSX__
        topsizer->SetMinSize(410, -1); // for OS X look
#endif

        wxSizer *sizer = new wxBoxSizer(wxVERTICAL);
        topsizer->Add(sizer, wxSizerFlags().Expand().Border());
        SetSizer(topsizer);

        sizer->AddSpacer(5);
        m_useTM = new wxCheckBox(this, wxID_ANY, _("Use translation memory"));
        sizer->Add(m_useTM, wxSizerFlags().Expand().Border(wxALL));

        m_stats = new wxStaticText(this, wxID_ANY, "--\n--", wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE);
        sizer->AddSpacer(10);
        sizer->Add(m_stats, wxSizerFlags().Expand().Border(wxLEFT|wxRIGHT, 25));
        sizer->AddSpacer(10);

        auto import = new wxButton(this, wxID_ANY, _("Learn From Files..."));
        sizer->Add(import, wxSizerFlags().Border(wxLEFT|wxRIGHT, 25));
        sizer->AddSpacer(10);

        m_useTMWhenUpdating = new wxCheckBox(this, wxID_ANY, _("Consult TM when updating from sources"));
        sizer->Add(m_useTMWhenUpdating, wxSizerFlags().Expand().Border(wxALL));

        auto explainTxt = _("If enabled, Poedit will try to fill in new entries using your previous\n"
                            "translations stored in the translation memory. If the TM is\n"
                            "near-empty, it will not be very effective. The more translations\n"
                            "you edit and the larger the TM grows, the better it gets.");
        auto explain = new ExplanationLabel(this, explainTxt);
        sizer->Add(explain, wxSizerFlags().Expand().Border(wxLEFT|wxRIGHT, 25));

        auto learnMore = new LearnMoreLink(this, "http://poedit.net/trac/wiki/Doc/TranslationMemory");
        sizer->AddSpacer(5);
        sizer->Add(learnMore, wxSizerFlags().Border(wxLEFT|wxRIGHT, 25 + LearnMoreLink::EXTRA_INDENT));
        sizer->AddSpacer(10);

#ifdef __WXOSX__
        m_stats->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
        import->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
#endif

        m_useTMWhenUpdating->Bind(wxEVT_UPDATE_UI, &TMPageWindow::OnUpdateUI, this);
        m_stats->Bind(wxEVT_UPDATE_UI, &TMPageWindow::OnUpdateUI, this);
        import->Bind(wxEVT_UPDATE_UI, &TMPageWindow::OnUpdateUI, this);

        import->Bind(wxEVT_BUTTON, &TMPageWindow::OnImportIntoTM, this);

        UpdateStats();

        if (wxPreferencesEditor::ShouldApplyChangesImmediately())
        {
            m_useTM->Bind(wxEVT_CHECKBOX, [=](wxCommandEvent&){ TransferDataFromWindow(); });
            m_useTMWhenUpdating->Bind(wxEVT_CHECKBOX, [=](wxCommandEvent&){ TransferDataFromWindow(); });
        }
    }

    void InitValues(const wxConfigBase& cfg) override
    {
        m_useTM->SetValue(cfg.ReadBool("use_tm", true));
        m_useTMWhenUpdating->SetValue(cfg.ReadBool("use_tm_when_updating", false));
    }

    void SaveValues(wxConfigBase& cfg) override
    {
        cfg.Write("use_tm", m_useTM->GetValue());
        cfg.Write("use_tm_when_updating", m_useTMWhenUpdating->GetValue());
    }

private:
    void UpdateStats()
    {
        wxString sDocs("--");
        wxString sFileSize("--");
        if (wxConfig::Get()->ReadBool("use_tm", true))
        {
            try
            {
                long docs, fileSize;
                TranslationMemory::Get().GetStats(docs, fileSize);
                sDocs.Printf("<b>%s</b>", wxNumberFormatter::ToString(docs));
                sFileSize.Printf("<b>%s</b>", wxFileName::GetHumanReadableSize(fileSize, "--", 1, wxSIZE_CONV_SI));
            }
            catch (Exception&)
            {
                // ignore Lucene errors -- if the index doesn't exist yet, just show --
            }
        }

        m_stats->SetLabelMarkup(wxString::Format(
            "%s %s\n%s %s",
            _("Stored translations:"),      sDocs,
            _("Database size on disk:"),    sFileSize
        ));
    }

    void OnImportIntoTM(wxCommandEvent&)
    {
        wxWindowPtr<wxFileDialog> dlg(new wxFileDialog(
            this,
            _("Select translation files to import"),
            wxEmptyString,
            wxEmptyString,
			wxString::Format("%s (*.po)|*.po|%s (*.*)|*.*",
                _("PO Translation Files"), _("All Files")),
			wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE));

        dlg->ShowWindowModalThenDo([=](int retcode){
            if (retcode != wxID_OK)
                return;

            wxArrayString paths;
            dlg->GetPaths(paths);

            wxProgressDialog progress(_("Translation Memory"),
                                      _("Importing translations..."),
                                      (int)paths.size() * 2 + 1,
                                      this,
                                      wxPD_APP_MODAL|wxPD_AUTO_HIDE|wxPD_CAN_ABORT);
            auto tm = TranslationMemory::Get().CreateWriter();
            int step = 0;
            for (size_t i = 0; i < paths.size(); i++)
            {
                std::unique_ptr<Catalog> cat(new Catalog(paths[i]));
                if (!progress.Update(++step))
                    break;
                if (cat->IsOk())
                    tm->Insert(*cat);
                if (!progress.Update(++step))
                    break;
            }
            progress.Pulse(_("Finalizing..."));
            tm->Commit();
            UpdateStats();
        });
    }

    void OnUpdateUI(wxUpdateUIEvent& e)
    {
        e.Enable(m_useTM->GetValue());
    }

    wxCheckBox *m_useTM, *m_useTMWhenUpdating;
    wxStaticText *m_stats;
};

class TMPage : public wxPreferencesPage
{
public:
    wxString GetName() const override
    {
#ifdef __WXOSX__
        // TRANSLATORS: This is abbreviation of "Translation Memory" used in Preferences on OS X.
        // Long text looks weird there, too short (like TM) too, but less so. "General" is about ideal
        // length there.
        return _("TM");
#else
        return _("Translation Memory");
#endif
    }
    wxBitmap GetLargeIcon() const override { return wxArtProvider::GetBitmap("Prefs-TM"); }
    wxWindow *CreateWindow(wxWindow *parent) override { return new TMPageWindow(parent); }
};



class ExtractorsPageWindow : public PrefsPanel
{
public:
    ExtractorsPageWindow(wxWindow *parent) : PrefsPanel(parent)
    {
        wxSizer *topsizer = new wxBoxSizer(wxVERTICAL);

        wxSizer *sizer = new wxBoxSizer(wxVERTICAL);
        topsizer->Add(sizer, wxSizerFlags(1).Expand().DoubleBorder());
        SetSizer(topsizer);

        auto horizontal = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(horizontal, wxSizerFlags(1).Expand());

        m_list = new wxListBox(this, wxID_ANY);
        m_list->SetMinSize(wxSize(250,300));
        horizontal->Add(m_list, wxSizerFlags(1).Expand().Border(wxRIGHT));

        auto buttons = new wxBoxSizer(wxVERTICAL);
        horizontal->Add(buttons, wxSizerFlags().Expand());

        m_new = new wxButton(this, wxID_ANY, _("New"));
        m_edit = new wxButton(this, wxID_ANY, _("Edit"));
        m_delete = new wxButton(this, wxID_ANY, _("Delete"));
        buttons->Add(m_new, wxSizerFlags().Border(wxBOTTOM));
        buttons->Add(m_edit, wxSizerFlags().Border(wxBOTTOM));
        buttons->Add(m_delete, wxSizerFlags().Border(wxBOTTOM));

        m_new->Bind(wxEVT_BUTTON, &ExtractorsPageWindow::OnNewExtractor, this);
        m_edit->Bind(wxEVT_BUTTON, &ExtractorsPageWindow::OnEditExtractor, this);
        m_delete->Bind(wxEVT_BUTTON, &ExtractorsPageWindow::OnDeleteExtractor, this);
    }

    void InitValues(const wxConfigBase& cfg) override
    {
        m_extractors.Read(const_cast<wxConfigBase*>(&cfg));
        
        for (const auto& item: m_extractors.Data)
            m_list->Append(item.Name);
        
        if (m_extractors.Data.empty())
        {
            m_edit->Enable(false);
            m_delete->Enable(false);
        }
        else
            m_list->SetSelection(0);
    }

    void SaveValues(wxConfigBase& cfg) override
    {
        m_extractors.Write(&cfg);
    }

private:
    /// Called to launch dialog for editting parser properties.
    template<typename TFunctor>
    void EditExtractor(int num, TFunctor completionHandler)
    {
        wxWindowPtr<wxDialog> dlg(wxXmlResource::Get()->LoadDialog(this, "edit_extractor"));
        dlg->Centre();

        auto extractor_language = XRCCTRL(*dlg, "extractor_language", wxTextCtrl);
        auto extractor_extensions = XRCCTRL(*dlg, "extractor_extensions", wxTextCtrl);
        auto extractor_command = XRCCTRL(*dlg, "extractor_command", wxTextCtrl);
        auto extractor_keywords = XRCCTRL(*dlg, "extractor_keywords", wxTextCtrl);
        auto extractor_files = XRCCTRL(*dlg, "extractor_files", wxTextCtrl);
        auto extractor_charset = XRCCTRL(*dlg, "extractor_charset", wxTextCtrl);

        {
            const Extractor& nfo = m_extractors.Data[num];
            extractor_language->SetValue(nfo.Name);
            extractor_extensions->SetValue(nfo.Extensions);
            extractor_command->SetValue(nfo.Command);
            extractor_keywords->SetValue(nfo.KeywordItem);
            extractor_files->SetValue(nfo.FileItem);
            extractor_charset->SetValue(nfo.CharsetItem);
        }

        dlg->Bind
        (
            wxEVT_UPDATE_UI,
            [=](wxUpdateUIEvent& e){
                e.Enable(!extractor_language->IsEmpty() &&
                         !extractor_extensions->IsEmpty() &&
                         !extractor_command->IsEmpty() &&
                         !extractor_files->IsEmpty());
                // charset, keywords could in theory be empty if unsupported by the parser tool
            },
            wxID_OK
        );

        dlg->ShowWindowModalThenDo([=](int retcode){
            (void)dlg; // force use
            if (retcode == wxID_OK)
            {
                Extractor& nfo = m_extractors.Data[num];
                nfo.Name = extractor_language->GetValue();
                nfo.Extensions = extractor_extensions->GetValue();
                nfo.Command = extractor_command->GetValue();
                nfo.KeywordItem = extractor_keywords->GetValue();
                nfo.FileItem = extractor_files->GetValue();
                nfo.CharsetItem = extractor_charset->GetValue();
                m_list->SetString(num, nfo.Name);
            }
            completionHandler(retcode == wxID_OK);
        });
    }

    void OnNewExtractor(wxCommandEvent&)
    {
        Extractor info;
        m_extractors.Data.push_back(info);
        m_list->Append(wxEmptyString);
        int index = (int)m_extractors.Data.size()-1;
        EditExtractor(index, [=](bool added){
            if (added)
            {
                m_edit->Enable(true);
                m_delete->Enable(true);
            }
            else
            {
                m_list->Delete(index);
                m_extractors.Data.erase(m_extractors.Data.begin() + index);
            }

            if (wxPreferencesEditor::ShouldApplyChangesImmediately())
                TransferDataFromWindow();
        });
    }

    void OnEditExtractor(wxCommandEvent&)
    {
        EditExtractor(m_list->GetSelection(), [=](bool changed){
            if (changed && wxPreferencesEditor::ShouldApplyChangesImmediately())
                TransferDataFromWindow();
        });
    }

    void OnDeleteExtractor(wxCommandEvent&)
    {
        int index = m_list->GetSelection();
        m_extractors.Data.erase(m_extractors.Data.begin() + index);
        m_list->Delete(index);
        if (m_extractors.Data.empty())
        {
            m_list->Enable(false);
            m_list->Enable(false);
        }

        if (wxPreferencesEditor::ShouldApplyChangesImmediately())
            TransferDataFromWindow();
    }

    ExtractorsDB m_extractors;

    wxListBox *m_list;
    wxButton *m_new, *m_edit, *m_delete;
};

class ExtractorsPage : public wxPreferencesPage
{
public:
    wxString GetName() const override { return _("Extractors"); }
    wxBitmap GetLargeIcon() const override { return wxArtProvider::GetBitmap("Prefs-Extractors"); }
    wxWindow *CreateWindow(wxWindow *parent) override { return new ExtractorsPageWindow(parent); }
};




#ifdef HAS_UPDATES_CHECK
class UpdatesPageWindow : public PrefsPanel
{
public:
    UpdatesPageWindow(wxWindow *parent) : PrefsPanel(parent)
    {
        wxSizer *topsizer = new wxBoxSizer(wxVERTICAL);
        topsizer->SetMinSize(350, -1); // for OS X look, wouldn't fit the toolbar otherwise

        wxSizer *sizer = new wxBoxSizer(wxVERTICAL);
        topsizer->Add(sizer, wxSizerFlags().Expand().DoubleBorder());
        SetSizer(topsizer);

        m_updates = new wxCheckBox(this, wxID_ANY, _("Automatically check for updates"));
        sizer->Add(m_updates, wxSizerFlags().Expand().Border(wxTOP|wxBOTTOM));

        m_beta = new wxCheckBox(this, wxID_ANY, _("Include beta versions"));
        sizer->Add(m_beta, wxSizerFlags().Expand().Border(wxBOTTOM));
        
        sizer->Add(new ExplanationLabel(this, _("Beta versions contain the latest new features and improvements, but may be a bit less stable.")),
                   wxSizerFlags().Expand().Border(wxLEFT, 20));
        sizer->AddSpacer(5);

        if (wxPreferencesEditor::ShouldApplyChangesImmediately())
            Bind(wxEVT_CHECKBOX, [=](wxCommandEvent&){ TransferDataFromWindow(); });
    }

    void InitValues(const wxConfigBase&) override
    {
#ifdef USE_SPARKLE
        m_updates->SetValue((bool)UserDefaults_GetBoolValue("SUEnableAutomaticChecks"));
#endif
#ifdef __WXMSW__
        m_updates->SetValue(win_sparkle_get_automatic_check_for_updates() != 0);
#endif
        m_beta->SetValue(wxGetApp().CheckForBetaUpdates());
        if (wxGetApp().IsBetaVersion())
            m_beta->Disable();
    }

    void SaveValues(wxConfigBase& cfg) override
    {
#ifdef __WXMSW__
        win_sparkle_set_automatic_check_for_updates(m_updates->GetValue());
#endif
        if (!wxGetApp().IsBetaVersion())
        {
            cfg.Write("check_for_beta_updates", m_beta->GetValue());
        }
#ifdef USE_SPARKLE
        UserDefaults_SetBoolValue("SUEnableAutomaticChecks", m_updates->GetValue());
        Sparkle_Initialize(wxGetApp().CheckForBetaUpdates());
#endif
    }

private:
    wxCheckBox *m_updates, *m_beta;
};

class UpdatesPage : public wxPreferencesPage
{
public:
    wxString GetName() const override { return _("Updates"); }
    wxBitmap GetLargeIcon() const override { return wxArtProvider::GetBitmap("Prefs-Updates"); }
    wxWindow *CreateWindow(wxWindow *parent) override { return new UpdatesPageWindow(parent); }
};
#endif // HAS_UPDATES_CHECK


} // anonymous namespace



std::unique_ptr<PoeditPreferencesEditor> PoeditPreferencesEditor::Create()
{
    std::unique_ptr<PoeditPreferencesEditor> p(new PoeditPreferencesEditor);
    p->AddPage(new GeneralPage);
    p->AddPage(new TMPage);
    p->AddPage(new ExtractorsPage);
#ifdef HAS_UPDATES_CHECK
    p->AddPage(new UpdatesPage);
#endif
    return p;
}

