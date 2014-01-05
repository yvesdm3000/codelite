//////////////////////////////////////////////////////////////////////
// This file was auto-generated by codelite's wxCrafter Plugin
// Do not modify this file by hand!
//////////////////////////////////////////////////////////////////////

#ifndef CODEFORMATTERDLG_BASE_CLASSES_H
#define CODEFORMATTERDLG_BASE_CLASSES_H

#include <wx/settings.h>
#include <wx/xrc/xmlres.h>
#include <wx/xrc/xh_bmp.h>
#include <wx/dialog.h>
#include <wx/iconbndl.h>
#include <wx/artprov.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/panel.h>
#include <wx/notebook.h>
#include <wx/imaglist.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/radiobox.h>
#include <wx/checkbox.h>
#include <wx/textctrl.h>
#include <wx/stc/stc.h>
#include <wx/button.h>

class CodeFormatterBaseDlg : public wxDialog
{
protected:
    wxSplitterWindow* m_splitterSettingsPreview;
    wxPanel* m_panelSettings;
    wxNotebook* m_notebookSettings;
    wxPanel* m_panelPredefinedStyles;
    wxStaticText* m_staticText2;
    wxRadioBox* m_radioBoxPredefinedStyle;
    wxStaticText* m_staticTextPredefineHelp;
    wxPanel* m_panelBrackets;
    wxRadioBox* m_radioBoxBrackets;
    wxPanel* m_panelIndentation;
    wxCheckBox* m_checkBoxIndetClass;
    wxCheckBox* m_checkBoxIndentBrackets;
    wxCheckBox* m_checkBoxIndetSwitch;
    wxCheckBox* m_checkBoxIndentNamespaces;
    wxCheckBox* m_checkBoxIndetCase;
    wxCheckBox* m_checkBoxIndentLabels;
    wxCheckBox* m_checkBoxIndetBlocks;
    wxCheckBox* m_checkBoxIndentPreprocessors;
    wxCheckBox* m_checkBoxIndentMaxInst;
    wxCheckBox* m_checkBoxIndentMinCond;
    wxPanel* m_panelFormatting;
    wxCheckBox* m_checkBoxFormatBreakBlocks;
    wxCheckBox* m_checkBoxFormatPadParenth;
    wxCheckBox* m_checkBoxFormatBreakBlocksAll;
    wxCheckBox* m_checkBoxFormatPadParentOut;
    wxCheckBox* m_checkBoxFormatBreakElseif;
    wxCheckBox* m_checkBoxFormatPadParentIn;
    wxCheckBox* m_checkBoxFormatPadOperators;
    wxCheckBox* m_checkBoxFormatUnPadParent;
    wxCheckBox* m_checkBoxFormatOneLineKeepStmnt;
    wxCheckBox* m_checkBoxFormatFillEmptyLines;
    wxCheckBox* m_checkBoxFormatOneLineKeepBlocks;
    wxPanel* m_panelCustomSettings;
    wxStaticText* m_staticText3;
    wxTextCtrl* m_textCtrlUserFlags;
    wxPanel* m_panelPreview;
    wxStyledTextCtrl* m_textCtrlPreview;
    wxButton* m_buttonOK;
    wxButton* m_buttonClose;
    wxButton* m_buttonHelp;

protected:
    virtual void OnRadioBoxPredefinedStyle(wxCommandEvent& event) { event.Skip(); }
    virtual void OnRadioBoxBrackets(wxCommandEvent& event) { event.Skip(); }
    virtual void OnCheckBox(wxCommandEvent& event) { event.Skip(); }

public:
    CodeFormatterBaseDlg(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Formatter Options"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(800,600), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER);
    virtual ~CodeFormatterBaseDlg();
};

#endif