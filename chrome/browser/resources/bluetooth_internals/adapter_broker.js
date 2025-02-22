// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for AdapterBroker, served from
 *     chrome://bluetooth-internals/.
 */
cr.define('adapter_broker', function() {
  /** @typedef {interfaces.BluetoothAdapter.Adapter.ptrClass} */
  var AdapterPtr;
  /** @typedef {interfaces.BluetoothDevice.Device.ptrClass} */
  var DevicePtr;
  /** @typedef {interfaces.BluetoothAdapter.DiscoverySession.ptrClass} */
  var DiscoverySessionPtr;

  /**
   * Enum of adapter property names. Used for adapterchanged events.
   * @enum {string}
   */
  var AdapterProperty = {
    DISCOVERING: 'discovering',
  };

  /**
   * The proxy class of an adapter and router of adapter events.
   * Exposes an EventTarget interface that allows other object to subscribe to
   * to specific AdapterClient events.
   * Provides proxy access to Adapter functions. Converts parameters to Mojo
   * handles and back when necessary.
   * @constructor
   * @extends {cr.EventTarget}
   * @param {!AdapterPtr} adapter
   */
  var AdapterBroker = function(adapter) {
    this.adapter_ = adapter;
    this.adapterClient_ = new AdapterClient(this);
    this.setClient(this.adapterClient_);
  };

  AdapterBroker.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * Creates a GATT connection to the device with |address|.
     * @param {string} address
     * @return {!Promise<!DevicePtr>}
     */
    connectToDevice: function(address) {
      return this.adapter_.connectToDevice(address).then(function(response) {
        if (response.result !=
            interfaces.BluetoothAdapter.ConnectResult.SUCCESS) {
          // TODO(crbug.com/663394): Replace with more descriptive error
          // messages.
          var ConnectResult = interfaces.BluetoothAdapter.ConnectResult;
          var errorString = Object.keys(ConnectResult).find(function(key) {
            return ConnectResult[key] === response.result;
          });

          throw new Error(errorString);
        }

        return response.device;
      });
    },

    /**
     * Gets an array of currently detectable devices from the Adapter service.
     * @return {!Array<!interfaces.BluetoothDevice.DeviceInfo>}
     */
    getDevices: function() {
      return this.adapter_.getDevices();
    },

    /**
     * Gets the current state of the Adapter.
     * @return {!interfaces.BluetoothAdapter.AdapterInfo}
     */
    getInfo: function() {
      return this.adapter_.getInfo();
    },

    /**
     * Sets client of Adapter service.
     * @param {!interfaces.BluetoothAdapter.AdapterClient} adapterClient
     */
    setClient: function(adapterClient) {
      adapterClient.binding = new interfaces.Bindings.Binding(
          interfaces.BluetoothAdapter.AdapterClient,
          adapterClient);

      this.adapter_.setClient(
          adapterClient.binding.createInterfacePtrAndBind());
    },

    /**
     * Requests the adapter to start a new discovery session.
     * @return {!Promise<!DiscoverySessionPtr>}
     */
    startDiscoverySession: function() {
      return this.adapter_.startDiscoverySession().then(function(response) {
        if (!response.session.ptr.isBound()) {
          throw new Error('Discovery session failed to start');
        }

        return response.session;
      });
    },
  };

  /**
   * The implementation of AdapterClient in
   * device/bluetooth/public/interfaces/adapter.mojom. Dispatches events
   * through AdapterBroker to notify client objects of changes to the Adapter
   * service.
   * @constructor
   * @param {!AdapterBroker} adapterBroker Broker to dispatch events through.
   */
  var AdapterClient = function(adapterBroker) {
    this.adapterBroker_ = adapterBroker;
  };

  AdapterClient.prototype = {
    /**
     * Fires adapterchanged event.
     * @param {boolean} discovering
     */
    discoveringChanged: function(discovering) {
      var event = new CustomEvent('adapterchanged', {
        detail: {
          property: AdapterProperty.DISCOVERING,
          value: discovering,
        }
      });
      this.adapterBroker_.dispatchEvent(event);
    },

    /**
     * Fires deviceadded event.
     * @param {!interfaces.BluetoothDevice.DeviceInfo} deviceInfo
     */
    deviceAdded: function(deviceInfo) {
      var event = new CustomEvent('deviceadded', {
        detail: {
          deviceInfo: deviceInfo
        }
      });
      this.adapterBroker_.dispatchEvent(event);
    },

    /**
     * Fires devicechanged event.
     * @param {!interfaces.BluetoothDevice.DeviceInfo} deviceInfo
     */
    deviceChanged: function(deviceInfo) {
      var event = new CustomEvent('devicechanged', {
        detail: {
          deviceInfo: deviceInfo
        }
      });
      this.adapterBroker_.dispatchEvent(event);
    },

    /**
     * Fires deviceremoved event.
     * @param {!interfaces.BluetoothDevice.DeviceInfo} deviceInfo
     */
    deviceRemoved: function(deviceInfo) {
      var event = new CustomEvent('deviceremoved', {
        detail: {
          deviceInfo: deviceInfo
        }
      });
      this.adapterBroker_.dispatchEvent(event);
    },
  };

  var adapterBroker = null;

  /**
   * Initializes an AdapterBroker if one doesn't exist.
   * @return {!Promise<!AdapterBroker>} resolves with AdapterBroker,
   *     rejects if Bluetooth is not supported.
   */
  function getAdapterBroker() {
    if (adapterBroker) return Promise.resolve(adapterBroker);

    return interfaces.setupInterfaces().then(function(adapter) {
      var adapterFactory = new interfaces.BluetoothAdapter.AdapterFactoryPtr(
          interfaces.FrameInterfaces.getInterface(
              interfaces.BluetoothAdapter.AdapterFactory.name));

      // Get an Adapter service.
      return adapterFactory.getAdapter();
    }).then(function(response) {
      if (!response.adapter.ptr.isBound()) {
        throw new Error('Bluetooth Not Supported on this platform.');
      }

      adapterBroker = new AdapterBroker(response.adapter);
      return adapterBroker;
    });
  }

  return {
    AdapterProperty: AdapterProperty,
    getAdapterBroker: getAdapterBroker,
  };
});
