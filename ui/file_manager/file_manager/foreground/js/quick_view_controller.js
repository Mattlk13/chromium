// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Controller for QuickView.
 * This should be initialized with |init_| method.
 *
 * @param {!MetadataModel} metadataModel File system metadata.
 * @param {!FileSelectionHandler} selectionHandler
 * @param {!ListContainer} listContainer
 * @param {!QuickViewModel} quickViewModel
 * @param {!TaskController} taskController
 * @param {!cr.ui.ListSelectionModel} fileListSelectionModel
 * @param {!QuickViewUma} quickViewUma
 * @param {!MetadataBoxController} metadataBoxController
 *
 * @constructor
 */
function QuickViewController(
    metadataModel, selectionHandler, listContainer, quickViewModel,
    taskController, fileListSelectionModel, quickViewUma,
    metadataBoxController) {
  /**
   * @type {FilesQuickView}
   * @private
   */
  this.quickView_ = null;

  /**
   * @type {!FileSelectionHandler}
   * @private
   */
  this.selectionHandler_ = selectionHandler;

  /**
   * @type {!ListContainer}
   * @private
   */
  this.listContainer_ = listContainer;

  /**
   * @type{!QuickViewModel}
   * @private
   */
  this.quickViewModel_ = quickViewModel;

  /**
   * @type {!QuickViewUma}
   * @private
   */
  this.quickViewUma_ = quickViewUma;

  /**
   * @type {!MetadataModel}
   * @private
   */
  this.metadataModel_ = metadataModel;

  /**
   * @type {!TaskController}
   * @private
   */
  this.taskController_ = taskController;

  /**
   * @type {!cr.ui.ListSelectionModel}
   * @private
   */
  this.fileListSelectionModel_ = fileListSelectionModel;

  /**
   * @type {!MetadataBoxController}
   * @private
   */
  this.metadataBoxController_ = metadataBoxController;

  /**
   * Current selection of selectionHandler.
   *
   * @type {!Array<!FileEntry>}
   * @private
   */
  this.entries_ = [];

  this.selectionHandler_.addEventListener(
      FileSelectionHandler.EventType.CHANGE,
      this.onFileSelectionChanged_.bind(this));
  this.listContainer_.element.addEventListener(
      'keydown', this.onKeyDownToOpen_.bind(this));
  this.listContainer_.element.addEventListener('command', function(event) {
    if(event.command.id === 'get-info')
      this.display_();
  }.bind(this));
}

/**
 * Initialize the controller with quick view which will be lazily loaded.
 * @param {!FilesQuickView} quickView
 * @private
 */
QuickViewController.prototype.init_ = function(quickView) {
  this.quickView_ = quickView;
  this.metadataBoxController_.init(quickView);
  quickView.addEventListener('keydown', this.onQuickViewKeyDown_.bind(this));
  quickView.addEventListener('iron-overlay-closed', function() {
    this.listContainer_.focus();
  }.bind(this));
  quickView.onOpenInNewButtonTap = this.onOpenInNewButtonTap_.bind(this);

  var toolTip = this.quickView_.$$('files-tooltip');
  var elems = this.quickView_.$.buttons.querySelectorAll('[has-tooltip]');
  toolTip.addTargets(elems);
};

/**
 * Craete quick view element.
 * @return Promise<!FilesQuickView>
 * @private
 */
QuickViewController.prototype.createQuickView_ = function() {
  return new Promise(function(resolve, reject) {
    Polymer.Base.importHref(
        'foreground/elements/files_quick_view.html', function() {
          var quickView = document.querySelector('#quick-view');
          i18nTemplate.process(quickView, loadTimeData);
          resolve(quickView);
        }, reject);
  });
};

/**
 * Handles open-in-new button tap.
 *
 * @param {!Event} event A button click event.
 * @private
 */
QuickViewController.prototype.onOpenInNewButtonTap_ = function(event) {
  this.taskController_.executeDefaultTask();
  this.quickView_.close();
};

/**
 * Handles key event on listContainer if it's relevent to quick view.
 *
 * @param {!Event} event A keyboard event.
 * @private
 */
QuickViewController.prototype.onKeyDownToOpen_ = function(event) {
  if (this.entries_.length == 0)
    return;
  if (event.key === ' ') {
    event.preventDefault();
    this.display_();
  }
};

/**
 * Handles key event on quick view.
 *
 * @param {!Event} event A keyboard event.
 * @private
 */
QuickViewController.prototype.onQuickViewKeyDown_ = function(event) {
  switch (event.key) {
    case ' ':
    case 'Escape':
      event.preventDefault();
      // Prevent the open dialog from closing.
      event.stopImmediatePropagation();
      this.quickView_.close();
      break;
    case 'ArrowRight':
      var index = this.fileListSelectionModel_.selectedIndex + 1;
      if (index >= this.fileListSelectionModel_.length)
        index = 0;
      this.fileListSelectionModel_.selectedIndex = index;
      break;
    case 'ArrowLeft':
      var index = this.fileListSelectionModel_.selectedIndex - 1;
      if (index < 0)
        index = this.fileListSelectionModel_.length - 1;
      this.fileListSelectionModel_.selectedIndex = index;
      break;
  }
};

/**
 * Display quick view.
 *
 * @private
 */
QuickViewController.prototype.display_ = function() {
  this.updateQuickView_().then(function() {
    if (!this.quickView_.isOpened()) {
      this.quickView_.open();
      this.quickViewUma_.onOpened(this.entries_[0]);
    }
  }.bind(this));
};

/**
 * Update quick view on file selection change.
 *
 * @param {!Event} event an Event whose target is FileSelectionHandler.
 * @private
 */
QuickViewController.prototype.onFileSelectionChanged_ = function(event) {
  this.entries_ = event.target.selection.entries;
  if (this.quickView_ && this.quickView_.isOpened()) {
    assert(this.entries_.length > 0);
    var entry = this.entries_[0];
    this.quickViewModel_.setSelectedEntry(entry);
    this.display_();
  }
};

/**
 * Update quick view using current entries.
 *
 * @return {!Promise} Promise fulfilled after quick view is updated.
 * @private
 */
QuickViewController.prototype.updateQuickView_ = function() {
  if (!this.quickView_) {
    return this.createQuickView_()
        .then(this.init_.bind(this))
        .then(this.updateQuickView_.bind(this))
        .catch(console.error);
  }
  assert(this.entries_.length > 0);
  // TODO(oka): Support multi-selection.
  this.quickViewModel_.setSelectedEntry(this.entries_[0]);

  var entry =
      (/** @type {!FileEntry} */ (this.quickViewModel_.getSelectedEntry()));
  assert(entry);
  this.quickViewUma_.onEntryChanged(entry);
  return this.metadataModel_.get([entry], ['thumbnailUrl', 'externalFileUrl'])
      .then(this.onMetadataLoaded_.bind(this, entry));
};

/**
 * Update quick view using file entry and loaded metadata.
 *
 * @param {!FileEntry} entry
 * @param {Array<MetadataItem>} items
 * @private
 */
QuickViewController.prototype.onMetadataLoaded_ = function(entry, items) {
  return this.getQuickViewParameters_(entry, items).then(function(params) {
    this.quickView_.type = params.type || '';
    this.quickView_.filePath = params.filePath || '';
    this.quickView_.contentUrl = params.contentUrl || '';
    this.quickView_.videoPoster = params.videoPoster || '';
    this.quickView_.audioArtwork = params.audioArtwork || '';
    this.quickView_.autoplay = params.autoplay || false;
    this.quickView_.browsable = params.browsable || false;
  }.bind(this));
};

/**
 * @typedef {{
 *   type: string,
 *   filePath: string,
 *   contentUrl: (string|undefined),
 *   videoPoster: (string|undefined),
 *   audioArtwork: (string|undefined),
 *   autoplay: (boolean|undefined),
 *   browsable: (boolean|undefined),
 * }}
 */
var QuickViewParams;

/**
 * @param {!FileEntry} entry
 * @param {Array<MetadataItem>} items
 * @return !Promise<!QuickViewParams>
 *
 * @private
 */
QuickViewController.prototype.getQuickViewParameters_ = function(entry, items) {
  var item = items[0];
  var type = FileType.getType(entry).type;

  /** @type {!QuickViewParams} */
  var params = {
    type: type,
    filePath: entry.name,
  };

  /**
   * @type function(!FileEntry): !Promise<!File>
   */
  var getFile = function(entry) {
    return new Promise(function(resolve, reject) {
      entry.file(resolve, reject);
    });
  };

  if (type === 'image') {
    if (item.externalFileUrl) {
      if (item.thumbnailUrl) {
        return this.loadThumbnailFromDrive_(item.thumbnailUrl)
            .then(function(result) {
              if (result.status === 'success')
                params.contentUrl = result.data;
              return params;
            }.bind(this));
      }
    } else {
      return getFile(entry).then(function(file) {
        params.contentUrl = URL.createObjectURL(file);
        return params;
      });
    }
  } else if (type === 'video') {
    if (item.externalFileUrl) {
      if (item.thumbnailUrl) {
        return this.loadThumbnailFromDrive_(item.thumbnailUrl)
            .then(function(result) {
              if (result.status === 'success') {
                params.videoPoster = result.data;
              }
              return params;
            });
      }
    } else {
      params.autoplay = true;
      if (item.thumbnailUrl) {
        params.videoPoster = item.thumbnailUrl;
      }
      return getFile(entry).then(function(file) {
        params.contentUrl = URL.createObjectURL(file);
        return params;
      });
    }
  } else if (type === 'audio') {
    if (item.externalFileUrl) {
      // If the file is in Drive, we ask user to open it with external app.
    } else {
      params.autoplay = true;
      return Promise
          .all([
            this.metadataModel_.get([entry], ['contentThumbnailUrl']),
            getFile(entry)
          ])
          .then(function(values) {
            /** @type {!Array<!MetadataItem>} */
            var items = values[0];
            /** @type {!File} */
            var file = values[1];
            var item = items[0];
            if (item.contentThumbnailUrl) {
              params.audioArtwork = item.contentThumbnailUrl;
            }
            params.contentUrl = URL.createObjectURL(file);
            return params;
          });
    }
  }
  if (item.externalFileUrl || type === '.folder') {
    return Promise.resolve(params);
  }
  return Promise
      .all([
        getFile(entry),
        new Promise(function(resolve) {
          chrome.fileManagerPrivate.getFileTasks([entry], resolve);
        })
      ])
      .then(function(values) {
        var file = values[0];
        var tasks = values[1];
        var browsable = tasks.some(function(task) {
          return ['view-in-browser', 'view-pdf'].includes(
              task.taskId.split('|')[2]);
        });
        params.browsable = browsable;
        params.contentUrl = browsable && URL.createObjectURL(file);
        return params;
      });
};

/**
 * Loads a thumbnail from Drive.
 *
 * @param {string} url Thumbnail url
 * @return Promise<{{status: string, data:string, width:number, height:number}}>
 * @private
 */
QuickViewController.prototype.loadThumbnailFromDrive_ = function(url) {
  return new Promise(function(resolve) {
    ImageLoaderClient.getInstance().load(url, resolve)
  });
};
