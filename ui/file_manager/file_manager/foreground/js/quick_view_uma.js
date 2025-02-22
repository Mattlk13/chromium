// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * UMA exporter for Quick View.
 *
 * @param {!VolumeManagerWrapper} volumeManager
 * @param {!DialogType} dialogType
 *
 * @constructor
 */
function QuickViewUma(volumeManager, dialogType) {

  /**
   * @type {!VolumeManagerWrapper}
   * @private
   */
  this.volumeManager_ = volumeManager;
  /**
   * @type {DialogType}
   * @private
   */
  this.dialogType_ = dialogType;
}

/**
 * Keep the order of this in sync with FileManagerVolumeType in
 * tools/metrics/histograms/histograms.xml.
 *
 * @type {!Array<VolumeManagerCommon.VolumeType>}
 * @const
 */
QuickViewUma.VolumeType = [
  VolumeManagerCommon.VolumeType.DRIVE,
  VolumeManagerCommon.VolumeType.DOWNLOADS,
  VolumeManagerCommon.VolumeType.REMOVABLE,
  VolumeManagerCommon.VolumeType.ARCHIVE,
  VolumeManagerCommon.VolumeType.PROVIDED,
  VolumeManagerCommon.VolumeType.MTP,
];

/**
 * Exports file type metric with the given |name|.
 *
 * @param {!FileEntry} entry
 * @param {string} name The histogram name.
 *
 * @private
 */
QuickViewUma.prototype.exportFileType_ = function(entry, name) {
  var extension = FileType.getExtension(entry).toLowerCase();
  if (FileTasks.UMA_INDEX_KNOWN_EXTENSIONS.indexOf(extension) < 0) {
    extension = 'other';
  }
  metrics.recordEnum(name, extension, FileTasks.UMA_INDEX_KNOWN_EXTENSIONS);
};

/**
 * Exports UMA based on the entry shown in Quick View.
 *
 * @param {!FileEntry} entry
 */
QuickViewUma.prototype.onEntryChanged = function(entry) {
  this.exportFileType_(entry, 'QuickView.FileType');
};

/**
 * Exports UMA based on the entry selected when Quick View is opened.
 *
 * @param {!FileEntry} entry
 */
QuickViewUma.prototype.onOpened = function(entry) {
  this.exportFileType_(entry, 'QuickView.FileTypeOnLaunch');
  var volumeType = this.volumeManager_.getVolumeInfo(entry).volumeType;
  if (QuickViewUma.VolumeType.includes(volumeType)) {
    metrics.recordEnum(
        'QuickView.VolumeType', volumeType, QuickViewUma.VolumeType);
  } else {
    console.error('Unknown volume type: ' + volumeType);
  }
  // Record stats of dialog types. It must be in sync with
  // FileDialogType enum in tools/metrics/histograms/histogram.xml.
  metrics.recordEnum('QuickView.DialogType', this.dialogType_,
      [DialogType.SELECT_FOLDER,
       DialogType.SELECT_UPLOAD_FOLDER,
       DialogType.SELECT_SAVEAS_FILE,
       DialogType.SELECT_OPEN_FILE,
       DialogType.SELECT_OPEN_MULTI_FILE,
       DialogType.FULL_PAGE]);
};
