#ifndef CLTREECTRLPANEL_H
#define CLTREECTRLPANEL_H

#include "wxcrafter_plugin.h"
#include "bitmap_loader.h"
#include "cl_command_event.h"
#include "clFileViwerTreeCtrl.h"
#include <imanager.h>
#include "cl_config.h"

class clTreeCtrlPanelDefaultPage;
class WXDLLIMPEXP_SDK clTreeCtrlPanel : public clTreeCtrlPanelBase
{
protected:
    BitmapLoader m_bmpLoader;
    clConfig* m_config;
    wxString m_viewName;
    clTreeCtrlPanelDefaultPage* m_defaultView;
    wxString m_newfileTemplate;
    size_t m_newfileTemplateHighlightLen;
    size_t m_options;

public:
    enum {
        kShowHiddenFiles = (1 << 0),
        kShowHiddenFolders = (1 << 1),
    };

protected:
    void ToggleView();

public:
    clTreeCtrlPanel(wxWindow* parent);
    virtual ~clTreeCtrlPanel();

    /**
     * @brief set the tree options
     */
    void SetOptions(size_t options) { m_options = options; }

    /**
     * @brief set the new file template (default is "Untitled.txt")
     */
    void SetNewFileTemplate(const wxString& newfile, size_t charsToHighlight);

    void SetViewName(const wxString& viewName) { this->m_viewName = viewName; }
    const wxString& GetViewName() const { return m_viewName; }

    clConfig* GetConfig() { return m_config; }

    /**
     * @brief clear the view (i.e. close all top level folders)
     */
    void Clear();

    /**
     * @brief return the configuration tool used for storing information about
     * this tree. Override it to provide a custom configuration tool
     */
    void SetConfig(clConfig* config) { this->m_config = config; }

    /**
     * @brief add top level folder
     */
    void AddFolder(const wxString& path);

    /**
     * @brief return an info about the selected items in the tree
     * @return
     */
    TreeItemInfo GetSelectedItemInfo();

    /**
     * @brief return 2 arrays of the selected items
     * @param folders [output]
     * @param files [output]
     */
    void GetSelections(wxArrayString& folders, wxArrayString& files);

    /**
     * @brief select a given filename in the tree. Expand the tree if needed
     */
    bool ExpandToFile(const wxFileName& filename);

protected:
    void UpdateItemDeleted(const wxTreeItemId& item);
    void GetTopLevelFolders(wxArrayString& paths, wxArrayTreeItemIds& items);

    /**
     * @brief ensure that item is selected (single selection)
     */
    void SelectItem(const wxTreeItemId& item);
    /**
     * @brief return list of selected files and folders. In addition return the
     * tree ctrl items. You can always assume that the folders and the folderItems are of the same
     * size. Same for the file arrays
     */
    void GetSelections(wxArrayString& folders,
                       wxArrayTreeItemIds& folderItems,
                       wxArrayString& files,
                       wxArrayTreeItemIds& fileItems);

    // Make the event handler functions virtual
    // so any subclass could override them
    virtual void OnActiveEditorChanged(wxCommandEvent& event);
    virtual void OnInitDone(wxCommandEvent& event);
    virtual void OnContextMenu(wxTreeEvent& event);
    virtual void OnItemActivated(wxTreeEvent& event);
    virtual void OnItemExpanding(wxTreeEvent& event);
    virtual void OnCloseFolder(wxCommandEvent& event);
    virtual void OnNewFolder(wxCommandEvent& event);
    virtual void OnNewFile(wxCommandEvent& event);
    virtual void OnOpenFile(wxCommandEvent& event);
    virtual void OnOpenWithDefaultApplication(wxCommandEvent& event);
    virtual void OnRenameFile(wxCommandEvent& event);
    virtual void OnDeleteSelections(wxCommandEvent& event);
    virtual void OnFindInFilesFolder(wxCommandEvent& event);
    virtual void OnOpenContainingFolder(wxCommandEvent& event);
    virtual void OnOpenShellFolder(wxCommandEvent& event);
    virtual void OnFolderDropped(clCommandEvent& event);
    virtual void OnRefresh(wxCommandEvent& event);
    void OnOpenFolder(wxCommandEvent& event);
    // Helpers
    void DoExpandItem(const wxTreeItemId& parent, bool expand);
    void DoRenameItem(const wxTreeItemId& item, const wxString& oldname, const wxString& newname);

    bool IsTopLevelFolder(const wxTreeItemId& item);

    clTreeCtrlData* GetItemData(const wxTreeItemId& item);
    wxTreeItemId DoAddFolder(const wxTreeItemId& parent, const wxString& path);
    wxTreeItemId DoAddFile(const wxTreeItemId& parent, const wxString& path);
    void DoCloseFolder(const wxTreeItemId& item);
};
#endif // CLTREECTRLPANEL_H
