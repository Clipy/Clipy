//
//  CPYSnippetTableView.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/28.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

// MARK: - CPYSnippetTableView Protocol
@objc protocol CPYSnippetTableViewDelegate {
    optional func selectSnippet(row: Int, folder: CPYFolder?)
}

// MARK: - CPYSnippetTableView
class CPYSnippetTableView: NSTableView {

    // MARK: - Properties
    internal weak var tableDelegate: CPYSnippetTableViewDelegate?
    private var snippetFolder: CPYFolder?
    private var fileIcon: NSImage?
    
    // MARK - Init
    required init?(coder: NSCoder) {
        super.init(coder: coder)
        self.setDelegate(self)
        self.setDataSource(self)
        
        self.registerForDraggedTypes([kDraggedDataType])
        self.setDraggingSourceOperationMask(NSDragOperation.Move, forLocal: true)
    }
    
    override func drawRect(dirtyRect: NSRect) {
        super.drawRect(dirtyRect)
    }
    
    // MARK: - Self Methods
    internal func setFolder(folder: CPYFolder?) {
        self.snippetFolder = folder
        self.reloadData()
    }
    
}

// MARK: - NSTableView DataSource
extension CPYSnippetTableView: NSTableViewDataSource {
    func numberOfRowsInTableView(tableView: NSTableView) -> Int {
        if self.snippetFolder != nil {
            return Int(self.snippetFolder!.snippets.count)
        } else {
            return 0
        }
    }
    
    func tableView(tableView: NSTableView, objectValueForTableColumn tableColumn: NSTableColumn?, row: Int) -> AnyObject? {
        
        if snippetFolder != nil {
            let snippet = snippetFolder!.snippets.sortedResultsUsingProperty("index", ascending: true).objectAtIndex(UInt(row)) as! CPYSnippet
            
            if let dataCell = tableColumn?.dataCellForRow(row) as? CPYImageAndTextCell {
                if snippet.enable {
                    dataCell.textColor = NSColor.blackColor()
                } else {
                    dataCell.textColor = NSColor.lightGrayColor()
                }
            }
            
            return snippet.title
        }
        
        return ""
    }
    
    func tableView(tableView: NSTableView, shouldSelectRow row: Int) -> Bool {
        self.tableDelegate?.selectSnippet?(row, folder: self.snippetFolder)
        return true
    }
}

// MARk: - NSTableView Delegate
extension CPYSnippetTableView: NSTableViewDelegate {
    func tableView(tableView: NSTableView, willDisplayCell cell: AnyObject, forTableColumn tableColumn: NSTableColumn?, row: Int) {
        (cell as! CPYImageAndTextCell).cellImageType = .File
    }
    
    func control(control: NSControl, textShouldEndEditing fieldEditor: NSText) -> Bool {
        if let text = fieldEditor.string {
            if count(text.utf16) != 0 {
                if let tableView = control as? NSTableView {
                    let snippet = self.snippetFolder!.snippets.sortedResultsUsingProperty("index", ascending: true).objectAtIndex(UInt(tableView.selectedRow)) as! CPYSnippet
                    let realm = RLMRealm.defaultRealm()
                    realm.transactionWithBlock({ () -> Void in
                        snippet.title = text
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
                
                CPYSnippetManager.sharedManager.updateSnippetIndex(row, selectIndexes: rowIndexes, folder: self.snippetFolder)
                self.reloadData()
                if row > rowIndexes.firstIndex {
                    self.selectRowIndexes(NSIndexSet(index: row - 1), byExtendingSelection: false)
                    self.tableDelegate?.selectSnippet?(row - 1, folder: self.snippetFolder)
                } else {
                    self.selectRowIndexes(NSIndexSet(index: row), byExtendingSelection: false)
                    self.tableDelegate?.selectSnippet?(row, folder: self.snippetFolder)
                }
            }
        }
        return true
    }
}
