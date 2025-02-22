// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @unrestricted
 */
Network.EventSourceMessagesView = class extends UI.VBox {
  /**
   * @param {!SDK.NetworkRequest} request
   */
  constructor(request) {
    super();
    this.registerRequiredCSS('network/eventSourceMessagesView.css');
    this.element.classList.add('event-source-messages-view');
    this._request = request;

    var columns = /** @type {!Array<!UI.DataGrid.ColumnDescriptor>} */ ([
      {id: 'id', title: Common.UIString('Id'), sortable: true, weight: 8},
      {id: 'type', title: Common.UIString('Type'), sortable: true, weight: 8},
      {id: 'data', title: Common.UIString('Data'), sortable: false, weight: 88},
      {id: 'time', title: Common.UIString('Time'), sortable: true, weight: 8}
    ]);

    this._dataGrid = new UI.SortableDataGrid(columns);
    this._dataGrid.setStickToBottom(true);
    this._dataGrid.markColumnAsSortedBy('time', UI.DataGrid.Order.Ascending);
    this._sortItems();
    this._dataGrid.addEventListener(UI.DataGrid.Events.SortingChanged, this._sortItems, this);

    this._dataGrid.setName('EventSourceMessagesView');
    this._dataGrid.asWidget().show(this.element);
  }

  /**
   * @override
   */
  wasShown() {
    this._dataGrid.rootNode().removeChildren();
    var messages = this._request.eventSourceMessages();
    for (var i = 0; i < messages.length; ++i)
      this._dataGrid.insertChild(new Network.EventSourceMessageNode(messages[i]));

    this._request.addEventListener(SDK.NetworkRequest.Events.EventSourceMessageAdded, this._messageAdded, this);
  }

  /**
   * @override
   */
  willHide() {
    this._request.removeEventListener(SDK.NetworkRequest.Events.EventSourceMessageAdded, this._messageAdded, this);
  }

  /**
   * @param {!Common.Event} event
   */
  _messageAdded(event) {
    var message = /** @type {!SDK.NetworkRequest.EventSourceMessage} */ (event.data);
    this._dataGrid.insertChild(new Network.EventSourceMessageNode(message));
  }

  _sortItems() {
    var sortColumnId = this._dataGrid.sortColumnId();
    if (!sortColumnId)
      return;
    var comparator = Network.EventSourceMessageNode.Comparators[sortColumnId];
    if (!comparator)
      return;
    this._dataGrid.sortNodes(comparator, !this._dataGrid.isSortOrderAscending());
  }
};

/**
 * @unrestricted
 */
Network.EventSourceMessageNode = class extends UI.SortableDataGridNode {
  /**
   * @param {!SDK.NetworkRequest.EventSourceMessage} message
   */
  constructor(message) {
    var time = new Date(message.time * 1000);
    var timeText = ('0' + time.getHours()).substr(-2) + ':' + ('0' + time.getMinutes()).substr(-2) + ':' +
        ('0' + time.getSeconds()).substr(-2) + '.' + ('00' + time.getMilliseconds()).substr(-3);
    var timeNode = createElement('div');
    timeNode.createTextChild(timeText);
    timeNode.title = time.toLocaleString();
    super({id: message.eventId, type: message.eventName, data: message.data, time: timeNode});
    this._message = message;
  }
};

/**
 * @param {string} field
 * @param {!Network.EventSourceMessageNode} a
 * @param {!Network.EventSourceMessageNode} b
 * @return {number}
 */
Network.EventSourceMessageNodeComparator = function(field, a, b) {
  var aValue = a._message[field];
  var bValue = b._message[field];
  return aValue < bValue ? -1 : aValue > bValue ? 1 : 0;
};

/** @type {!Object.<string, function(!Network.EventSourceMessageNode, !Network.EventSourceMessageNode):number>} */
Network.EventSourceMessageNode.Comparators = {
  'id': Network.EventSourceMessageNodeComparator.bind(null, 'eventId'),
  'type': Network.EventSourceMessageNodeComparator.bind(null, 'eventName'),
  'time': Network.EventSourceMessageNodeComparator.bind(null, 'time')
};
