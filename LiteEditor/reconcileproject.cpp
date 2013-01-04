#include "reconcileproject.h"
#include "windowattrmanager.h"
#include "VirtualDirectorySelectorDlg.h"
#include "workspace.h"
#include "manager.h"
#include "frame.h"
#include "tree_node.h"
#include "globals.h"
#include "event_notifier.h"
#include <wx/dirdlg.h>
#include <wx/dir.h>
#include <wx/tokenzr.h>
#include <wx/log.h>
#include <wx/regex.h>
#include <wx/busyinfo.h>
#include <algorithm>

// ---------------------------------------------------------

bool IsSourceVD(const wxString& name)
{
    return (name == "src" || name == "source" || name == "cpp" || name == "c" || name == "cc");
}

bool IsHeaderVD(const wxString& name)
{
    return (name == "include" || name == "includes" || name == "header" || name == "headers" || name == "hpp" || name == "h");
}

bool IsResourceVD(const wxString& name)
{
    return (name == "rc" || name == "resource" || name == "resources");
}

// ---------------------------------------------------------

class ReconcileFileItemData : public wxClientData
{
    wxString m_filename;
    wxString m_virtualFolder;

public:
    ReconcileFileItemData() {}
    ReconcileFileItemData(const wxString &filename, const wxString &vd) : m_filename(filename), m_virtualFolder(vd) {}
    virtual ~ReconcileFileItemData() {}
    void SetFilename(const wxString& filename) {
        this->m_filename = filename;
    }
    void SetVirtualFolder(const wxString& virtualFolder) {
        this->m_virtualFolder = virtualFolder;
    }
    const wxString& GetFilename() const {
        return m_filename;
    }
    const wxString& GetVirtualFolder() const {
        return m_virtualFolder;
    }
};

// ---------------------------------------------------------

class FindFilesTraverser : public wxDirTraverser
{
public:
    FindFilesTraverser(const wxString types, const wxArrayString& ignorefiles, const wxArrayString& excludes, const wxString& projFP)
                                        : m_ignorefiles(ignorefiles), m_excludes(excludes), m_projFP(projFP) {
        m_types = wxStringTokenize(types, ";,|"); // The tooltip says use ';' but cover all bases
    }

    virtual wxDirTraverseResult OnFile(const wxString& filename) {
        wxFileName fn(filename);

        // First check for a matching file-ignore
        for (size_t n = 0; n < m_ignorefiles.GetCount(); ++n){
            if (wxMatchWild(m_ignorefiles.Item(n), fn.GetFullName())) {
                return wxDIR_CONTINUE;
            }
        }

        if (m_types.empty()) {
            m_results.Add(filename); // No types presumably means everything
        } else {
            for (size_t n = 0; n < m_types.GetCount(); ++n) {
                if (m_types.Item(n) == fn.GetExt() || m_types.Item(n) == "*" || m_types.Item(n) == "*.*") { // Other ways to say "Be greedy"
                    m_results.Add(fn.GetFullPath());
                    break;
                }
            }
        }

        return wxDIR_CONTINUE;
    }

    virtual wxDirTraverseResult OnDir(const wxString& dirname) {
        // Skip this dir if it's found in the list of excludes
        wxFileName fn = wxFileName::DirName(dirname);
        if (fn.IsAbsolute()) {
            fn.MakeRelativeTo(m_projFP);
        }
        return (m_excludes.Index(fn.GetFullPath()) == wxNOT_FOUND) ? wxDIR_CONTINUE : wxDIR_IGNORE;
    }

    const wxArrayString& GetResults() const {
        return m_results;
    }

private:
    wxArrayString m_types;
    wxArrayString m_results;
    const wxArrayString m_ignorefiles;
    const wxArrayString m_excludes;
    const wxString m_projFP;
};

// ---------------------------------------------------------

ReconcileProjectDlg::ReconcileProjectDlg(wxWindow* parent, const wxString& projname)
    : ReconcileProjectDlgBaseClass(parent)
    , m_projname(projname)
    , m_projectModified(false)
{
    BitmapLoader bl;
    m_bitmaps = bl.MakeStandardMimeMap();
    WindowAttrManager::Load(this, wxT("ReconcileProjectDlg"), NULL);
}

ReconcileProjectDlg::~ReconcileProjectDlg()
{
    WindowAttrManager::Save(this, wxT("ReconcileProjectDlg"), NULL);
}

bool ReconcileProjectDlg::LoadData()
{
    ReconcileProjectFiletypesDlg dlg(this, m_projname);
    dlg.SetData();
    if (dlg.ShowModal() != wxID_OK) {
        return false;
    }
    wxString toplevelDir, types;
    wxArrayString ignorefiles, excludes, regexes;
    dlg.GetData(toplevelDir, types, ignorefiles, excludes, regexes);
    m_regexes = regexes;

    wxDir dir(toplevelDir);
    if (!dir.IsOpened()) {
        return false;
    }

    m_toplevelDir = toplevelDir;

    m_allfiles.clear();
    {
        wxBusyInfo wait("Searching for files...", this);
        wxSafeYield();

        FindFilesTraverser traverser(types, ignorefiles, excludes, toplevelDir);
        dir.Traverse(traverser);
        m_allfiles.insert(traverser.GetResults().begin(), traverser.GetResults().end());
        DoFindFiles();
    }

    if (m_newfiles.empty() && m_stalefiles.empty()) {
        wxMessageBox(_("No new or stale files found. The project is up-to-date"), _("CodeLite"), wxICON_INFORMATION|wxOK, this);
        return false;
    }

    DistributeFiles(false);
    return true;
}

void ReconcileProjectDlg::DistributeFiles(bool usingAutoallocate)
{
    VirtualDirectoryTree vdTree;
    if (usingAutoallocate) {
        vdTree.BuildTree(m_projname);
    }
    
    //---------------------------------------------------------
    // populate the 'new files' tab
    //---------------------------------------------------------

    m_dataviewAssignedModel->Clear();
    m_dvListCtrl1Unassigned->DeleteAllItems();

    wxStringSet_t::const_iterator iter = m_newfiles.begin();
    for (; iter != m_newfiles.end(); ++iter) {
        wxString filename = *iter;
        wxFileName fn(filename);
        fn.MakeRelativeTo(m_toplevelDir);

        // Even without auto-allocation, apply any regex as that'll be most likely to reflect the user's choice
        bool bFileAllocated = false;
        for (size_t i = 0; i < m_regexes.GetCount() ; ++i) {
            wxString virtualFolder(m_regexes.Item(i).BeforeFirst('|'));
            wxRegEx regex         (m_regexes.Item(i).AfterFirst('|'));
            if ( regex.IsValid() && regex.Matches( filename ) ) {
                wxVector<wxVariant> cols;
                cols.push_back( ::MakeIconText(fn.GetFullPath(), GetBitmap(filename)) );
                cols.push_back( virtualFolder );
                ReconcileFileItemData *data = new ReconcileFileItemData(filename, virtualFolder);
                m_dataviewAssignedModel->AppendItem(wxDataViewItem(0), cols, data);
                bFileAllocated = true;
                break;
            }
        }

        if (usingAutoallocate) {
            wxString virtualFolder = vdTree.FindBestMatchVDir(fn.GetPath(), fn.GetExt());
            if (!virtualFolder.empty()) {
                wxVector<wxVariant> cols;
                cols.push_back( ::MakeIconText(fn.GetFullPath(), GetBitmap(filename)) );
                cols.push_back( virtualFolder );
                ReconcileFileItemData* data = new ReconcileFileItemData(filename, virtualFolder);
                m_dataviewAssignedModel->AppendItem(wxDataViewItem(0), cols, data);
                bFileAllocated = true;
            }
        }

        if ( !bFileAllocated ) {
            wxVector<wxVariant> cols;
            cols.push_back( ::MakeIconText(fn.GetFullPath(), GetBitmap(filename) ) );
            m_dvListCtrl1Unassigned->AppendItem(cols, (wxUIntPtr)NULL);
        }
    }

    //---------------------------------------------------------
    // populate the 'stale files' tab
    //---------------------------------------------------------
    m_dataviewStaleFilesModel->Clear();
    Project::FileInfoList_t::const_iterator staleIter = m_stalefiles.begin();
    for(; staleIter != m_stalefiles.end(); ++staleIter) {

        wxVector<wxVariant> cols;
        cols.push_back( ::MakeIconText(staleIter->GetFilename(), GetBitmap( staleIter->GetFilename() )) );
        m_dataviewStaleFilesModel->AppendItem( wxDataViewItem(0), cols, new ReconcileFileItemData(staleIter->GetFilename(), staleIter->GetVirtualFolder() ) );
    }
}

wxArrayString ReconcileProjectDlg::RemoveStaleFiles(const wxArrayString& StaleFiles) const
{
    wxArrayString removals;

    ProjectPtr proj = ManagerST::Get()->GetProject(m_projname);
    wxCHECK_MSG(proj, removals, "Can't find a Project with the supplied name");

    for (size_t n = 0; n < StaleFiles.GetCount(); ++n) {
        // Reconstruct the VD path in projectname:foo:bar format
        int index = StaleFiles[n].Find(": ");
        wxCHECK_MSG(index != wxNOT_FOUND, removals, "Badly-formed stalefile string");
        wxString vdPath = StaleFiles[n].Left(index);
        wxString filepath = StaleFiles[n].Mid(index+2);

        if (proj->RemoveFile(filepath, vdPath)) {
            removals.Add(StaleFiles[n]);
        }
    }

    return removals;
}

wxArrayString ReconcileProjectDlg::AddMissingFiles(const wxArrayString& files, const wxString& vdPath)
{
    wxArrayString additions;

    ProjectPtr proj = ManagerST::Get()->GetProject(m_projname);
    wxCHECK_MSG(proj, additions, "Can't find a Project with the supplied name");

    wxString VD = vdPath;
    if (VD.empty()) {
        // If we were called from the root panel (so the user is trying to add unallocated files, or all files at once) we need to know which VD to use
        VirtualDirectorySelectorDlg selector(this, WorkspaceST::Get(), "", m_projname);
        selector.SetText("Please choose the Virtual Directory to which to add the files");
        if (selector.ShowModal() == wxID_OK) {
            VD = selector.GetVirtualDirectoryPath();
        } else {
            return additions;
        }
    }

    VD = VD.AfterFirst(':'); // Remove the projectname

    for (size_t n = 0; n < files.GetCount(); ++n) {
        if (proj->FastAddFile(files[n], VD)) {
            additions.Add(files[n]);
        }
    }

    return additions;
}

void ReconcileProjectDlg::DoFindFiles()
{
    m_stalefiles.clear();
    m_newfiles.clear();

    ProjectPtr proj = ManagerST::Get()->GetProject(m_projname);
    wxCHECK_RET(proj, "Can't find a Project with the supplied name");

    // get list of files from the project
    Project::FileInfoList_t projectfiles;
    proj->GetFilesMetadata(projectfiles);
    wxStringSet_t projectfilesSet;
    
    Project::FileInfoList_t::const_iterator it = projectfiles.begin();
    for( ; it != projectfiles.end(); ++it ) {
        projectfilesSet.insert( it->GetFilename() );
    }
    
    std::vector<wxString> result;
    std::set_difference(m_allfiles.begin(), m_allfiles.end(), projectfilesSet.begin(), projectfilesSet.end(), std::back_inserter(result));
    m_newfiles.insert(result.begin(), result.end());

    // now run the diff reverse to get list of stale files
    m_stalefiles.clear();
    Project::FileInfoList_t::const_iterator iter = projectfiles.begin();
    for(; iter != projectfiles.end(); ++iter ) {
        if ( !wxFileName::FileExists( iter->GetFilename() ) ) {
            m_stalefiles.push_back( *iter );
        }
    }
}

wxBitmap ReconcileProjectDlg::GetBitmap(const wxString& filename) const
{
    FileExtManager::FileType type = FileExtManager::GetType(filename);
    if ( !m_bitmaps.count( type ) )
        return m_bitmaps.find(FileExtManager::TypeText)->second;;
    return m_bitmaps.find(type)->second;
}

void ReconcileProjectDlg::OnAddFile(wxCommandEvent& event)
{
    VirtualDirectorySelectorDlg selector(this, WorkspaceST::Get(), "", m_projname);
    if ( selector.ShowModal()  == wxID_OK ) {
        wxString vd = selector.GetVirtualDirectoryPath();
        wxDataViewItemArray items;
        m_dvListCtrl1Unassigned->GetSelections(items);

        for(size_t i=0; i<items.GetCount(); ++i) {
            wxVariant v;
            m_dvListCtrl1Unassigned->GetValue( v, m_dvListCtrl1Unassigned->GetStore()->GetRow(items.Item(i)), 0 );

            wxString path;
            wxDataViewIconText iv;
            if ( !v.IsNull() ) {
                iv << v;
                path = iv.GetText();
            }


            wxFileName fn(path);
            fn.MakeAbsolute(m_toplevelDir);

            wxVector<wxVariant> cols;
            cols.push_back( ::MakeIconText(path, GetBitmap(path) ) );
            cols.push_back( vd );
            m_dataviewAssignedModel->AppendItem( wxDataViewItem(0), cols, new ReconcileFileItemData(fn.GetFullPath(), vd) );
            m_dvListCtrl1Unassigned->DeleteItem(  m_dvListCtrl1Unassigned->GetStore()->GetRow(items.Item(i)) );
        }
    }
}

void ReconcileProjectDlg::OnAddFileUI(wxUpdateUIEvent& event)
{
    event.Enable(m_dvListCtrl1Unassigned->GetSelectedItemsCount());
}

void ReconcileProjectDlg::OnAutoAssignUI(wxUpdateUIEvent& event)
{
    event.Enable(m_dvListCtrl1Unassigned->GetItemCount());
}

void ReconcileProjectDlg::OnAutoSuggest(wxCommandEvent& event)
{
    DistributeFiles(true);
}

void ReconcileProjectDlg::OnUndoSelectedFiles(wxCommandEvent& event)
{
    wxDataViewItemArray items;
    m_dataviewAssigned->GetSelections(items);

    for(size_t i=0; i<items.GetCount(); ++i) {
        wxVariant v;
        ReconcileFileItemData* data = dynamic_cast<ReconcileFileItemData*>(m_dataviewAssignedModel->GetClientObject( items.Item(i) ));
        if ( data ) {
            wxFileName fn(data->GetFilename());
            fn.MakeRelativeTo(m_toplevelDir);

            wxVector<wxVariant> cols;
            cols.push_back(::MakeIconText(fn.GetFullPath(), GetBitmap(fn.GetFullName())));
            m_dvListCtrl1Unassigned->AppendItem( cols, (wxUIntPtr)NULL );

        }
    }

    // get the list of items
    wxArrayString allfiles;
    for(int i=0 ; i<m_dvListCtrl1Unassigned->GetItemCount(); ++i) {
        wxVariant v;
        m_dvListCtrl1Unassigned->GetValue(v, i, 0);
        wxDataViewIconText it;
        it << v;
        allfiles.Add(it.GetText());
    }

    m_dataviewAssignedModel->DeleteItems(wxDataViewItem(0), items);

    // Could not find a nicer way of doing this, but
    // we want the files to be sorted again
    m_dvListCtrl1Unassigned->DeleteAllItems();

    std::sort(allfiles.begin(), allfiles.end());
    for(size_t i=0; i<allfiles.GetCount(); ++i) {
        wxVector<wxVariant> cols;
        cols.push_back( ::MakeIconText(allfiles.Item(i), GetBitmap(allfiles.Item(i)) ) );
        m_dvListCtrl1Unassigned->AppendItem( cols, (wxUIntPtr)NULL);
    }
}

void ReconcileProjectDlg::OnUndoSelectedFilesUI(wxUpdateUIEvent& event)
{
    event.Enable(m_dataviewAssigned->GetSelectedItemsCount());
}

void ReconcileProjectDlg::OnDeleteStaleFiles(wxCommandEvent& event)
{
    ProjectPtr proj = ManagerST::Get()->GetProject(m_projname);
    wxCHECK_RET(proj, "Can't find a Project with the supplied name");

    wxDataViewItemArray items;
    if (event.GetId() == wxID_DELETE) {
        m_dataviewStaleFiles->GetSelections( items );
    } else {
        m_dataviewStaleFilesModel->GetChildren(wxDataViewItem(0), items);
    }
    
    proj->BeginTranscation();
    for(size_t i=0; i<items.GetCount(); ++i) {
        ReconcileFileItemData* data =  dynamic_cast<ReconcileFileItemData*>(m_dataviewStaleFilesModel->GetClientObject(items.Item(i)));
        if ( data ) {
            proj->RemoveFile( data->GetFilename(), data->GetVirtualFolder() );
        }
        m_projectModified = true;
    }
    proj->CommitTranscation();
    m_dataviewStaleFilesModel->DeleteItems(wxDataViewItem(0), items);
}

void ReconcileProjectDlg::OnDeleteStaleFilesUI(wxUpdateUIEvent& event)
{
    event.Enable( m_dataviewStaleFiles->GetSelectedItemsCount() );
}

void ReconcileProjectDlg::OnDeleteAllStaleFilesUI(wxUpdateUIEvent& event)
{
    wxDataViewItemArray items;
    event.Enable( m_dataviewStaleFilesModel->GetChildren(wxDataViewItem(0), items) > 0 );
}

void ReconcileProjectDlg::OnClose(wxCommandEvent& event)
{
    // reload the workspace
    if ( m_projectModified ) {
        wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED, XRCID("reload_workspace"));
        evt.SetEventObject( clMainFrame::Get() );
        clMainFrame::Get()->GetEventHandler()->AddPendingEvent( evt );
    }
    EndModal(wxID_CLOSE);
}

void ReconcileProjectDlg::OnApply(wxCommandEvent& event)
{
    // get the list of files to add to the project
    wxDataViewItemArray items;
    if (event.GetId() == wxID_APPLY) {
        m_dataviewAssigned->GetSelections( items );
    } else {
        m_dataviewAssignedModel->GetChildren(wxDataViewItem(0), items);
    }

    // virtual folder to file name
    wxStringSet_t vds;
    StringMultimap_t filesToAdd;
    for(size_t i=0; i<items.GetCount(); ++i) {
        ReconcileFileItemData* data =  dynamic_cast<ReconcileFileItemData*>(m_dataviewAssignedModel->GetClientObject(items.Item(i)));
        if ( data ) {
            filesToAdd.insert(std::make_pair(data->GetVirtualFolder(), data->GetFilename()));
            vds.insert( data->GetVirtualFolder() );
        }
    }

    wxStringSet_t::const_iterator iter = vds.begin();
    for(; iter != vds.end(); ++iter) {
        std::pair<StringMultimap_t::iterator, StringMultimap_t::iterator> range = filesToAdd.equal_range( *iter );
        StringMultimap_t::iterator from = range.first;
        wxArrayString vdFiles;
        for( ; from != range.second; ++from ) {
            vdFiles.Add( from->second );
        }
        AddMissingFiles(vdFiles, *iter);
        m_projectModified = true;
    }
    m_dataviewAssignedModel->DeleteItems( wxDataViewItem(0), items );
}

void ReconcileProjectDlg::OnApplyUI(wxUpdateUIEvent& event)
{
    event.Enable( m_dataviewAssigned->GetSelectedItemsCount() );
}

void ReconcileProjectDlg::OnApplyAllUI(wxUpdateUIEvent& event)
{
    wxDataViewItemArray items;
    event.Enable( m_dataviewAssignedModel->GetChildren(wxDataViewItem(0), items) > 0 );
}

ReconcileProjectFiletypesDlg::ReconcileProjectFiletypesDlg(wxWindow* parent, const wxString& projname)
    : ReconcileProjectFiletypesDlgBaseClass(parent)
    , m_projname(projname)
{
    m_listCtrlRegexes->AppendColumn("Regex");
    m_listCtrlRegexes->AppendColumn("Virtual Directory");

    WindowAttrManager::Load(this, wxT("ReconcileProjectFiletypesDlg"), NULL);
}

ReconcileProjectFiletypesDlg::~ReconcileProjectFiletypesDlg()
{
    WindowAttrManager::Save(this, wxT("ReconcileProjectFiletypesDlg"), NULL);
}

void ReconcileProjectFiletypesDlg::SetData()
{
    ProjectPtr proj = ManagerST::Get()->GetProject(m_projname);
    wxCHECK_RET(proj, "Can't find a Project with the supplied name");

    wxString topleveldir, types;
    wxArrayString ignorefiles, excludes, regexes;
    proj->GetReconciliationData(topleveldir, types, ignorefiles, excludes, regexes);

    if (topleveldir.empty()) {
        topleveldir = proj->GetFileName().GetPath();
    }
    wxFileName tld(topleveldir);
    if ( tld.IsRelative() ) {
        tld.MakeAbsolute( proj->GetFileName().GetPath() );
    }
    m_dirPickerToplevel->SetPath(tld.GetFullPath());

    if (types.empty()) {
        types << "cpp;c;h;hpp;xrc;wxcp;fbp";
    }
    m_textExtensions->ChangeValue(types);

    m_listIgnoreFiles->Clear();
    m_listIgnoreFiles->Append(ignorefiles);

    m_listExclude->Clear();
    m_listExclude->Append(excludes);

    m_listCtrlRegexes->DeleteAllItems();
    for (size_t n = 0; n < regexes.GetCount(); ++n) {
        SetRegex(regexes[n]);
    }
}

void ReconcileProjectFiletypesDlg::GetData(wxString& toplevelDir, wxString& types, wxArrayString& ignoreFiles, wxArrayString& excludePaths, wxArrayString& regexes) const
{
    toplevelDir = m_dirPickerToplevel->GetPath();
    types = m_textExtensions->GetValue();
    ignoreFiles = m_listIgnoreFiles->GetStrings();
    excludePaths = m_listExclude->GetStrings();
    regexes = GetRegexes();

    // While we're here, save the current data
    ProjectPtr proj = ManagerST::Get()->GetProject(m_projname);
    wxCHECK_RET(proj, "Can't find a Project with the supplied name");
    
    wxFileName relTopLevelDir(toplevelDir);
    if( relTopLevelDir.IsAbsolute() ) {
        relTopLevelDir.MakeRelativeTo( proj->GetFileName().GetPath() );
    }
    
    proj->SetReconciliationData(relTopLevelDir.GetFullPath(wxPATH_UNIX), types, ignoreFiles, excludePaths, regexes);
}

void ReconcileProjectFiletypesDlg::SetRegex(const wxString& regex)
{
    int n = m_listCtrlRegexes->GetItemCount();
    AppendListCtrlRow(m_listCtrlRegexes);
    SetColumnText(m_listCtrlRegexes, n, 0, regex.AfterFirst('|'));
    SetColumnText(m_listCtrlRegexes, n, 1, regex.BeforeFirst('|'));
}

wxArrayString ReconcileProjectFiletypesDlg::GetRegexes() const
{
    wxArrayString array;
    for (int n = 0; n < m_listCtrlRegexes->GetItemCount(); ++n) {
        wxString regex = GetColumnText(m_listCtrlRegexes, n, 0);
        wxString VD = GetColumnText(m_listCtrlRegexes, n, 1);
        array.Add(VD + '|' + regex); // Store the data as a VD|regex string, as the regex might contain a '|' but the VD won't
    }
    return array;
}

void ReconcileProjectFiletypesDlg::OnIgnoreBrowse(wxCommandEvent& WXUNUSED(event))
{
    ProjectPtr proj = ManagerST::Get()->GetProject(m_projname);
    wxCHECK_RET(proj, "Can't find a Project with the supplied name");

    wxString topleveldir, types;
    wxArrayString ignorefiles, excludes, regexes;
    proj->GetReconciliationData(topleveldir, types, ignorefiles, excludes, regexes);

    if (topleveldir.empty()) {
        topleveldir = proj->GetFileName().GetPath();
    }
    
    wxFileName tld(topleveldir);
    if ( tld.IsRelative() ) {
        tld.MakeAbsolute( proj->GetFileName().GetPath() );
    }
    wxString new_exclude = wxDirSelector(_("Select a directory to ignore:"), tld.GetFullPath(), wxDD_DEFAULT_STYLE, wxDefaultPosition, this);

    if (!new_exclude.empty()) {
        wxFileName fn = wxFileName::DirName(new_exclude);
        fn.MakeRelativeTo(topleveldir);
        new_exclude = fn.GetFullPath();

        if (m_listExclude->FindString(new_exclude) == wxNOT_FOUND) {
            m_listExclude->Append(new_exclude);
        }
    }
}

void ReconcileProjectFiletypesDlg::OnIgnoreRemove(wxCommandEvent& WXUNUSED(event))
{
    int sel = m_listExclude->GetSelection();
    if (sel != wxNOT_FOUND) {
        m_listExclude->Delete(sel);
    }
}

void ReconcileProjectFiletypesDlg::OnIgnoreRemoveUpdateUI(wxUpdateUIEvent& event)
{
    event.Enable(m_listExclude->GetSelection() != wxNOT_FOUND);
}

void ReconcileProjectFiletypesDlg::OnIgnoreFileBrowse(wxCommandEvent& WXUNUSED(event))
{
    wxString name = wxGetTextFromUser("Enter the filename to ignore e.g. foo*.cpp", _("CodeLite"), "", this);
    if (!name.empty()) {
        if (m_listIgnoreFiles->FindString(name) == wxNOT_FOUND) {
            m_listIgnoreFiles->Append(name);
        }
    }
}

void ReconcileProjectFiletypesDlg::OnIgnoreFileRemove(wxCommandEvent& WXUNUSED(event))
{
    int sel = m_listIgnoreFiles->GetSelection();
    if (sel != wxNOT_FOUND) {
        m_listIgnoreFiles->Delete(sel);
    }
}

void ReconcileProjectFiletypesDlg::OnIgnoreFileRemoveUpdateUI(wxUpdateUIEvent& event)
{
    event.Enable(m_listIgnoreFiles->GetSelection() != wxNOT_FOUND);
}

void ReconcileProjectFiletypesDlg::OnAddRegex(wxCommandEvent& event)
{
    ReconcileByRegexDlg dlg(this, m_projname);
    if (dlg.ShowModal() == wxID_OK) {
        SetRegex(dlg.GetRegex());
    }
}

void ReconcileProjectFiletypesDlg::OnRemoveRegex(wxCommandEvent& event)
{
    wxUnusedVar(event);

    long selecteditem = m_listCtrlRegexes->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (selecteditem != wxNOT_FOUND) {
        m_listCtrlRegexes->DeleteItem(selecteditem);
    }
}

void ReconcileProjectFiletypesDlg::OnRemoveRegexUpdateUI(wxUpdateUIEvent& event)
{
    long selecteditem = m_listCtrlRegexes->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    event.Enable(selecteditem != wxNOT_FOUND);
}



ReconcileByRegexDlg::ReconcileByRegexDlg(wxWindow* parent, const wxString& projname)
    : ReconcileByRegexDlgBaseClass(parent)
    , m_projname(projname)
{
    WindowAttrManager::Load(this, wxT("ReconcileByRegexDlg"), NULL);
}

ReconcileByRegexDlg::~ReconcileByRegexDlg()
{
    WindowAttrManager::Save(this, wxT("ReconcileByRegexDlg"), NULL);
}

void ReconcileByRegexDlg::OnTextEnter(wxCommandEvent& event)
{
    if (m_buttonOK->IsEnabled()) {
        EndModal(wxID_OK);
    }
}

void ReconcileByRegexDlg::OnVDBrowse(wxCommandEvent& WXUNUSED(event))
{
    VirtualDirectorySelectorDlg selector(this, WorkspaceST::Get(), m_textCtrlVirtualFolder->GetValue(), m_projname);
    if (selector.ShowModal() == wxID_OK) {
        m_textCtrlVirtualFolder->ChangeValue(selector.GetVirtualDirectoryPath());
    }
}

void ReconcileByRegexDlg::OnRegexOKCancelUpdateUI(wxUpdateUIEvent& event)
{
    event.Enable(!m_textCtrlRegex->IsEmpty() && !m_textCtrlVirtualFolder->IsEmpty());
}


void VirtualDirectoryTree::BuildTree(const wxString& projName)
{
    ProjectPtr proj = ManagerST::Get()->GetProject(projName);
    wxCHECK_RET(proj, "Can't find a Project with the supplied name");

    ProjectTreePtr tree = proj->AsTree();
    TreeWalker<wxString, ProjectItem> walker(tree->GetRoot());

    for ( ; !walker.End(); walker++ ) {
        ProjectTreeNode* node = walker.GetNode();
        wxString displayname(node->GetData().GetDisplayName());
        if (node->GetData().GetKind() == ProjectItem::TypeVirtualDirectory) {
            wxString vdPath = displayname;
            ProjectTreeNode* tempnode = node->GetParent();
            while (tempnode) {
                vdPath = tempnode->GetData().GetDisplayName() + ':' + vdPath;
                tempnode = tempnode->GetParent();
            }

            VirtualDirectoryTree* parent = FindParent(vdPath.BeforeLast(':'));
            if (parent) {
                parent->StoreChild(displayname, vdPath);
            } else {
                // Any orphans must be root's top-level children, and we're root
                StoreChild(displayname, vdPath);
            }
        }
    }
}

VirtualDirectoryTree* VirtualDirectoryTree::FindParent(const wxString& vdChildPath)
{
    if (!vdChildPath.empty()) {
        if (m_vdPath == vdChildPath) {
            return this;
        }
        for (size_t n = 0; n < m_children.size(); ++n) {
            VirtualDirectoryTree* item = m_children[n]->FindParent(vdChildPath);
            if (item) {
                return item;
            }
        }
    }

    return NULL;
}

void VirtualDirectoryTree::StoreChild(const wxString& displayname, const wxString& vdPath)
{
    VirtualDirectoryTree* child = new VirtualDirectoryTree(this, displayname, vdPath);
    if (IsSourceVD(displayname.Lower()) || IsHeaderVD(displayname.Lower()) || IsResourceVD(displayname.Lower())) {
        m_children.push_back(child); // We want these processed last, so push_back
    } else {
        m_children.push_front(child);
    }
}

wxString VirtualDirectoryTree::FindBestMatchVDir(const wxString& path, const wxString& ext) const
{
    // Try all children first
    for (size_t n = 0; n < m_children.size(); ++n) {
        wxString vdir = m_children[n]->FindBestMatchVDir(path, ext);
        if (!vdir.empty()) {
            return vdir;
        }
    }

    // Now try here. If there's an exact match, we're the correct one _unless_ there's a src/header/resource immediate child
    wxString vdpath(m_vdPath.AfterFirst(':')); // We need to compare without the projectname
    vdpath.Replace(":", wxString(wxFILE_SEP_PATH));
    if (vdpath == path) {
        // Try for a src/header/etc immediate child. If there is one, it's presumably where files with a matching ext should go
        for (size_t c = 0; c < m_children.size(); ++c) {
            wxString childname = m_children[c]->GetDisplayname();
            if (IsSourceVD(childname.Lower())) {
                if (ext == "cpp" || ext == "c" || ext == "cc") {
                    return m_children[c]->GetVPath();
                }
            }

            if (IsHeaderVD(childname.Lower())) {
                if (ext == "h" || ext == "hpp" || ext == "hh") {
                    return m_children[c]->GetVPath();
                }
            }

            if (IsResourceVD(childname.Lower())) {
                if (ext == "rc") {
                    return m_children[c]->GetVPath();
                }
            }
        }

        // None found so return us
        return m_vdPath;
    }
    
    return "";
}
