# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'background',
      'variables': {
        'depends': [
          '../../../../../third_party/jstemplate/compiled_resources.gyp:jstemplate',
          '../../../../webui/resources/js/load_time_data.js',
          '../../../../webui/resources/js/cr.js',
          '../../../../webui/resources/js/cr/ui.js',
          '../../../../webui/resources/js/promise_resolver.js',
          '../../../../webui/resources/js/util.js',
          '../../../../webui/resources/js/cr/event_target.js',
          '../../../../webui/resources/js/cr/ui/array_data_model.js',
          '../../common/js/lru_cache.js',
          '../../../image_loader/image_loader_client.js',
          '../../common/js/error_util.js',
          '../../common/js/async_util.js',
          '../../common/js/file_type.js',
          '../../common/js/metrics_base.js',
          '../../common/js/metrics_events.js',
          '../../common/js/metrics.js',
          '../../common/js/progress_center_common.js',
          '../../common/js/util.js',
          '../../common/js/volume_manager_common.js',
          '../../common/js/importer_common.js',
          '../../foreground/js/ui/progress_center_panel.js',
          '../../foreground/js/progress_center_item_group.js',
          '../../foreground/js/metadata/byte_reader.js',
          'app_window_wrapper.js',
          'device_handler.js',
          'drive_sync_handler.js',
          'duplicate_finder.js',
          'entry_location_impl.js',
          'file_operation_handler.js',
          'file_operation_manager.js',
          'file_operation_util.js',
          'import_history.js',
          'launcher_search.js',
          'task_queue.js',
          'media_import_handler.js',
          'media_scanner.js',
          'progress_center.js',
          'volume_info_impl.js',
          'volume_info_list_impl.js',
          'volume_manager_factory.js',
          'volume_manager_impl.js',
          'volume_manager_util.js',
          'background_base.js',
        ],
        'externs': [
          '<(EXTERNS_DIR)/chrome_extensions.js',
          '<(EXTERNS_DIR)/chrome_send.js',
          '<(EXTERNS_DIR)/command_line_private.js',
          '<(EXTERNS_DIR)/file_manager_private.js',
          '<(EXTERNS_DIR)/metrics_private.js',
          '../../../../../third_party/analytics/externs.js',
          '../../../externs/chrome_test.js',
          '../../../externs/connection.js',
          '../../../externs/css_rule.js',
          '../../../externs/entry_location.js',
          '../../../externs/es6_workaround.js',
          '../../../externs/file_operation_progress_event.js',
          '../../../externs/launcher_search_provider.js',
          '../../../externs/webview_tag.js',
          '../../../externs/platform.js',
          '../../../externs/volume_info.js',
          '../../../externs/volume_info_list.js',
          '../../../externs/volume_manager.js',
        ],
      },
      'includes': [
        '../../../compile_js.gypi',
      ],
    }
  ],
}
