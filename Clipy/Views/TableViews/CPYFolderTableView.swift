//
//  CPYFolderTableView.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/28.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

// MARK: - CPYFolderTableView Protocol
@objc protocol CPYFolderTableViewDelegate {
    optional func selectFolder(row: Int)
}

// MARK: - CPYFolderTableView
class CPYFolderTableView: NSTableView {
    
    // MARK: - Properties
    weak var tableDelegate: CPYFolderTableViewDelegate?
    private var folderIcon: NSImage?
    
    // MARK - Init
    required init?(coder: NSCoder) {
        super.init(coder: coder)
        self.setDelegate(self)
        self.setDataSource(self)
        
        self.registerForDraggedTypes([kDraggedDataType])
        self.setDraggingSourceOperationMask(NSDragOperation.Move, forLocal: true)
    }
    
}

// MARK: - NSTableView DataSource
extension CPYFolderTableView: NSTableViewDataSource {
    func numberOfRowsInTableView(tableView: NSTableView) -> Int {
        return Int(CPYSnippetManager.sharedManager.loadFolders().count)
    }
    
    func tableView(tableView: NSTableView, objectValueForTableColumn tableColumn: NSTableColumn?, row: Int) -> AnyObject? {
        
        let folders = CPYSnippetManager.sharedManager.loadSortedFolders()
        let folder = folders.objectAtIndex(UInt(row)) as! CPYFolder
        
        if let dataCell = tableColumn?.dataCellForRow(row) as? CPYImageAndTextCell {
            if folder.enable {
                dataCell.textColor = NSColor.blackColor()
            } else {
                dataCell.textColor = NSColor.lightGrayColor()
            }
        }
        
        return folder.title
    }
    
    func tableView(tableView: NSTableView, shouldSelectRow row: Int) -> Bool {
        self.tableDelegate?.selectFolder?(row)
        return true
    }
}

// MARK: - NSTableView Delegate
extension CPYFolderTableView: NSTableViewDelegate {
    func tableView(tableView: NSTableView, willDisplayCell cell: AnyObject, forTableColumn tableColumn: NSTableColumn?, row: Int) {
        (cell as! CPYImageAndTextCell).cellImageType = .Folder
    }
    
    func control(control: NSControl, textShouldEndEditing fieldEditor: NSText) -> Bool {
        if let text = fieldEditor.string {
            if count(text.utf16) != 0 {
                if let tableView = control as? NSTableView {
                    let folder = CPYSnippetManager.sharedManager.loadSortedFolders().objectAtIndex(UInt(tableView.selectedRow)) as! CPYFolder
                    let realm = RLMRealm.defaultRealm()
                    realm.transactionWithBlock({ () -> Void in
                        folder.title = text
                    })
                }
                return true
            }
        }
        return false
    }
    
    func tableView(tableView: NSTableView, writeRowsWithIndexes rowIndexes: NSIndexSet, toPasteboard pboard: NSPasteboard) -> Bool {
        let draggedTypes = [kDraggedDataType]
        pboard.declareTypes(draggedTypes, owner: self)
        
        let data = NSKeyedArchiver.archivedDataWithRootObject(rowIndexes)
        pboard.setData(data, forType: kDraggedDataType)
        
        return true
    }
    
    func tableView(tableView: NSTableView, validateDrop info: NSDraggingInfo, proposedRow row: Int, proposedDropOperation dropOperation: NSTableViewDropOperation) -> NSDragOperation {
        let pboard = info.draggingPasteboard()
        let draggedTypes = [kDraggedDataType]
        let draggingSource: AnyObject? = info.draggingSource()
        
        if pboard.availableTypeFromArray(draggedTypes) != nil {
            if draggingSource is NSTableView {
                if dropOperation == NSTableViewDropOperation.Above {
                    return NSDragOperation.Move
                }
            }
        }
        
        return NSDragOperation.None
    }
    
    
    func tableView(tableView: NSTableView, acceptDrop info: NSDraggingInfo, row: Int, dropOperation: NSTableViewDropOperation) -> Bool {
        
        let pboard = info.draggingPasteboard()
        if let data = pboard.dataForType(kDraggedDataType) {
            if let rowIndexes = NSKeyedUnarchiver.unarchiveObjectWithData(data) as? NSIndexSet {
                if row == rowIndexes.firstIndex {
                    return false
                }
                if rowIndexes.count > 1 {
                    return false
                }
                
                CPYSnippetManager.sharedManager.updateFolderIndex(row, selectIndexes: rowIndexes)
                self.reloadData()
                if row > rowIndexes.firstIndex {
                    self.selectRowIndexes(NSIndexSet(index: row - 1), byExtendingSelection: false)
                    self.tableDelegate?.selectFolder?(row - 1)
                } else {
                    self.selectRowIndexes(NSIndexSet(index: row), byExtendingSelection: false)
                    self.tableDelegate?.selectFolder?(row)
                }
                
                
            }
        }
        return true
    }
}
