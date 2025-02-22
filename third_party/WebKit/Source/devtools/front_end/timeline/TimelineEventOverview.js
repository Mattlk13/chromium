/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @unrestricted
 */
Timeline.TimelineEventOverview = class extends UI.TimelineOverviewBase {
  /**
   * @param {string} id
   * @param {?string} title
   * @param {!TimelineModel.TimelineModel} model
   */
  constructor(id, title, model) {
    super();
    this.element.id = 'timeline-overview-' + id;
    this.element.classList.add('overview-strip');
    if (title)
      this.element.createChild('div', 'timeline-overview-strip-title').textContent = title;
    this._model = model;
  }

  /**
   * @param {number} begin
   * @param {number} end
   * @param {number} position
   * @param {number} height
   * @param {string} color
   */
  _renderBar(begin, end, position, height, color) {
    var x = begin;
    var width = end - begin;
    var ctx = this.context();
    ctx.fillStyle = color;
    ctx.fillRect(x, position, width, height);
  }

  /**
   * @override
   * @param {number} windowLeft
   * @param {number} windowRight
   * @return {!{startTime: number, endTime: number}}
   */
  windowTimes(windowLeft, windowRight) {
    var absoluteMin = this._model.minimumRecordTime();
    var timeSpan = this._model.maximumRecordTime() - absoluteMin;
    return {startTime: absoluteMin + timeSpan * windowLeft, endTime: absoluteMin + timeSpan * windowRight};
  }

  /**
   * @override
   * @param {number} startTime
   * @param {number} endTime
   * @return {!{left: number, right: number}}
   */
  windowBoundaries(startTime, endTime) {
    var absoluteMin = this._model.minimumRecordTime();
    var timeSpan = this._model.maximumRecordTime() - absoluteMin;
    var haveRecords = absoluteMin > 0;
    return {
      left: haveRecords && startTime ? Math.min((startTime - absoluteMin) / timeSpan, 1) : 0,
      right: haveRecords && endTime < Infinity ? (endTime - absoluteMin) / timeSpan : 1
    };
  }
};

/**
 * @unrestricted
 */
Timeline.TimelineEventOverviewInput = class extends Timeline.TimelineEventOverview {
  /**
   * @param {!TimelineModel.TimelineModel} model
   */
  constructor(model) {
    super('input', null, model);
  }

  /**
   * @override
   */
  update() {
    super.update();
    var events = this._model.mainThreadEvents();
    var height = this.height();
    var descriptors = Timeline.TimelineUIUtils.eventDispatchDesciptors();
    /** @type {!Map.<string,!Timeline.TimelineUIUtils.EventDispatchTypeDescriptor>} */
    var descriptorsByType = new Map();
    var maxPriority = -1;
    for (var descriptor of descriptors) {
      for (var type of descriptor.eventTypes)
        descriptorsByType.set(type, descriptor);
      maxPriority = Math.max(maxPriority, descriptor.priority);
    }

    var /** @const */ minWidth = 2 * window.devicePixelRatio;
    var timeOffset = this._model.minimumRecordTime();
    var timeSpan = this._model.maximumRecordTime() - timeOffset;
    var canvasWidth = this.width();
    var scale = canvasWidth / timeSpan;

    for (var priority = 0; priority <= maxPriority; ++priority) {
      for (var i = 0; i < events.length; ++i) {
        var event = events[i];
        if (event.name !== TimelineModel.TimelineModel.RecordType.EventDispatch)
          continue;
        var descriptor = descriptorsByType.get(event.args['data']['type']);
        if (!descriptor || descriptor.priority !== priority)
          continue;
        var start = Number.constrain(Math.floor((event.startTime - timeOffset) * scale), 0, canvasWidth);
        var end = Number.constrain(Math.ceil((event.endTime - timeOffset) * scale), 0, canvasWidth);
        var width = Math.max(end - start, minWidth);
        this._renderBar(start, start + width, 0, height, descriptor.color);
      }
    }
  }
};

/**
 * @unrestricted
 */
Timeline.TimelineEventOverviewNetwork = class extends Timeline.TimelineEventOverview {
  /**
   * @param {!TimelineModel.TimelineModel} model
   */
  constructor(model) {
    super('network', Common.UIString('NET'), model);
  }

  /**
   * @override
   */
  update() {
    super.update();
    var height = this.height();
    var numBands = categoryBand(Timeline.TimelineUIUtils.NetworkCategory.Other) + 1;
    var bandHeight = Math.floor(height / numBands);
    var devicePixelRatio = window.devicePixelRatio;
    var timeOffset = this._model.minimumRecordTime();
    var timeSpan = this._model.maximumRecordTime() - timeOffset;
    var canvasWidth = this.width();
    var scale = canvasWidth / timeSpan;
    var ctx = this.context();
    var requests = this._model.networkRequests();
    /** @type {!Map<string,!{waiting:!Path2D,transfer:!Path2D}>} */
    var paths = new Map();
    requests.forEach(drawRequest);
    for (var path of paths) {
      ctx.fillStyle = path[0];
      ctx.globalAlpha = 0.3;
      ctx.fill(path[1]['waiting']);
      ctx.globalAlpha = 1;
      ctx.fill(path[1]['transfer']);
    }

    /**
     * @param {!Timeline.TimelineUIUtils.NetworkCategory} category
     * @return {number}
     */
    function categoryBand(category) {
      var categories = Timeline.TimelineUIUtils.NetworkCategory;
      switch (category) {
        case categories.HTML:
          return 0;
        case categories.Script:
          return 1;
        case categories.Style:
          return 2;
        case categories.Media:
          return 3;
        default:
          return 4;
      }
    }

    /**
     * @param {!TimelineModel.TimelineModel.NetworkRequest} request
     */
    function drawRequest(request) {
      var tickWidth = 2 * devicePixelRatio;
      var category = Timeline.TimelineUIUtils.networkRequestCategory(request);
      var style = Timeline.TimelineUIUtils.networkCategoryColor(category);
      var band = categoryBand(category);
      var y = band * bandHeight;
      var path = paths.get(style);
      if (!path) {
        path = {waiting: new Path2D(), transfer: new Path2D()};
        paths.set(style, path);
      }
      var s = Math.max(Math.floor((request.startTime - timeOffset) * scale), 0);
      var e = Math.min(Math.ceil((request.endTime - timeOffset) * scale), canvasWidth);
      path['waiting'].rect(s, y, e - s, bandHeight - 1);
      path['transfer'].rect(e - tickWidth / 2, y, tickWidth, bandHeight - 1);
      if (!request.responseTime)
        return;
      var r = Math.ceil((request.responseTime - timeOffset) * scale);
      path['transfer'].rect(r - tickWidth / 2, y, tickWidth, bandHeight - 1);
    }
  }
};

/**
 * @unrestricted
 */
Timeline.TimelineEventOverviewCPUActivity = class extends Timeline.TimelineEventOverview {
  /**
   * @param {!TimelineModel.TimelineModel} model
   */
  constructor(model) {
    super('cpu-activity', Common.UIString('CPU'), model);
    this._backgroundCanvas = this.element.createChild('canvas', 'fill background');
  }

  /**
   * @override
   */
  resetCanvas() {
    super.resetCanvas();
    this._backgroundCanvas.width = this.element.clientWidth * window.devicePixelRatio;
    this._backgroundCanvas.height = this.element.clientHeight * window.devicePixelRatio;
  }

  /**
   * @override
   */
  update() {
    super.update();
    var /** @const */ quantSizePx = 4 * window.devicePixelRatio;
    var width = this.width();
    var height = this.height();
    var baseLine = height;
    var timeOffset = this._model.minimumRecordTime();
    var timeSpan = this._model.maximumRecordTime() - timeOffset;
    var scale = width / timeSpan;
    var quantTime = quantSizePx / scale;
    var categories = Timeline.TimelineUIUtils.categories();
    var categoryOrder = ['idle', 'loading', 'painting', 'rendering', 'scripting', 'other'];
    var otherIndex = categoryOrder.indexOf('other');
    var idleIndex = 0;
    console.assert(idleIndex === categoryOrder.indexOf('idle'));
    for (var i = idleIndex + 1; i < categoryOrder.length; ++i)
      categories[categoryOrder[i]]._overviewIndex = i;

    var backgroundContext = this._backgroundCanvas.getContext('2d');
    for (var thread of this._model.virtualThreads())
      drawThreadEvents(backgroundContext, thread.events);
    applyPattern(backgroundContext);
    drawThreadEvents(this.context(), this._model.mainThreadEvents());

    /**
     * @param {!CanvasRenderingContext2D} ctx
     * @param {!Array<!SDK.TracingModel.Event>} events
     */
    function drawThreadEvents(ctx, events) {
      var quantizer = new Timeline.Quantizer(timeOffset, quantTime, drawSample);
      var x = 0;
      var categoryIndexStack = [];
      var paths = [];
      var lastY = [];
      for (var i = 0; i < categoryOrder.length; ++i) {
        paths[i] = new Path2D();
        paths[i].moveTo(0, height);
        lastY[i] = height;
      }

      /**
       * @param {!Array<number>} counters
       */
      function drawSample(counters) {
        var y = baseLine;
        for (var i = idleIndex + 1; i < categoryOrder.length; ++i) {
          var h = (counters[i] || 0) / quantTime * height;
          y -= h;
          paths[i].bezierCurveTo(x, lastY[i], x, y, x + quantSizePx / 2, y);
          lastY[i] = y;
        }
        x += quantSizePx;
      }

      /**
       * @param {!SDK.TracingModel.Event} e
       */
      function onEventStart(e) {
        var index = categoryIndexStack.length ? categoryIndexStack.peekLast() : idleIndex;
        quantizer.appendInterval(e.startTime, index);
        categoryIndexStack.push(Timeline.TimelineUIUtils.eventStyle(e).category._overviewIndex || otherIndex);
      }

      /**
       * @param {!SDK.TracingModel.Event} e
       */
      function onEventEnd(e) {
        quantizer.appendInterval(e.endTime, categoryIndexStack.pop());
      }

      TimelineModel.TimelineModel.forEachEvent(events, onEventStart, onEventEnd);
      quantizer.appendInterval(timeOffset + timeSpan + quantTime, idleIndex);  // Kick drawing the last bucket.
      for (var i = categoryOrder.length - 1; i > 0; --i) {
        paths[i].lineTo(width, height);
        ctx.fillStyle = categories[categoryOrder[i]].color;
        ctx.fill(paths[i]);
      }
    }

    /**
     * @param {!CanvasRenderingContext2D} ctx
     */
    function applyPattern(ctx) {
      var step = 4 * window.devicePixelRatio;
      ctx.save();
      ctx.lineWidth = step / Math.sqrt(8);
      for (var x = 0.5; x < width + height; x += step) {
        ctx.moveTo(x, 0);
        ctx.lineTo(x - height, height);
      }
      ctx.globalCompositeOperation = 'destination-out';
      ctx.stroke();
      ctx.restore();
    }
  }
};

/**
 * @unrestricted
 */
Timeline.TimelineEventOverviewResponsiveness = class extends Timeline.TimelineEventOverview {
  /**
   * @param {!TimelineModel.TimelineModel} model
   * @param {!TimelineModel.TimelineFrameModel} frameModel
   */
  constructor(model, frameModel) {
    super('responsiveness', null, model);
    this._frameModel = frameModel;
  }

  /**
   * @override
   */
  update() {
    super.update();
    var height = this.height();
    var timeOffset = this._model.minimumRecordTime();
    var timeSpan = this._model.maximumRecordTime() - timeOffset;
    var scale = this.width() / timeSpan;
    var frames = this._frameModel.frames();
    // This is due to usage of new signatures of fill() and storke() that closure compiler does not recognize.
    var ctx = /** @type {!Object} */ (this.context());
    var fillPath = new Path2D();
    var markersPath = new Path2D();
    for (var i = 0; i < frames.length; ++i) {
      var frame = frames[i];
      if (!frame.hasWarnings())
        continue;
      paintWarningDecoration(frame.startTime, frame.duration);
    }

    var events = this._model.mainThreadEvents();
    for (var i = 0; i < events.length; ++i) {
      if (!TimelineModel.TimelineData.forEvent(events[i]).warning)
        continue;
      paintWarningDecoration(events[i].startTime, events[i].duration);
    }

    ctx.fillStyle = 'hsl(0, 80%, 90%)';
    ctx.strokeStyle = 'red';
    ctx.lineWidth = 2 * window.devicePixelRatio;
    ctx.fill(fillPath);
    ctx.stroke(markersPath);

    /**
     * @param {number} time
     * @param {number} duration
     */
    function paintWarningDecoration(time, duration) {
      var x = Math.round(scale * (time - timeOffset));
      var w = Math.round(scale * duration);
      fillPath.rect(x, 0, w, height);
      markersPath.moveTo(x + w, 0);
      markersPath.lineTo(x + w, height);
    }
  }
};

/**
 * @unrestricted
 */
Timeline.TimelineFilmStripOverview = class extends Timeline.TimelineEventOverview {
  /**
   * @param {!TimelineModel.TimelineModel} model
   * @param {!Components.FilmStripModel} filmStripModel
   */
  constructor(model, filmStripModel) {
    super('filmstrip', null, model);
    this._filmStripModel = filmStripModel;
    this.reset();
  }

  /**
   * @override
   */
  update() {
    super.update();
    var frames = this._filmStripModel.frames();
    if (!frames.length)
      return;

    var drawGeneration = Symbol('drawGeneration');
    this._drawGeneration = drawGeneration;
    this._imageByFrame(frames[0]).then(image => {
      if (this._drawGeneration !== drawGeneration)
        return;
      if (!image.naturalWidth || !image.naturalHeight)
        return;
      var imageHeight = this.height() - 2 * Timeline.TimelineFilmStripOverview.Padding;
      var imageWidth = Math.ceil(imageHeight * image.naturalWidth / image.naturalHeight);
      var popoverScale = Math.min(200 / image.naturalWidth, 1);
      this._emptyImage = new Image(image.naturalWidth * popoverScale, image.naturalHeight * popoverScale);
      this._drawFrames(imageWidth, imageHeight);
    });
  }

  /**
   * @param {!Components.FilmStripModel.Frame} frame
   * @return {!Promise<!HTMLImageElement>}
   */
  _imageByFrame(frame) {
    var imagePromise = this._frameToImagePromise.get(frame);
    if (!imagePromise) {
      imagePromise = frame.imageDataPromise().then(createImage);
      this._frameToImagePromise.set(frame, imagePromise);
    }
    return imagePromise;

    /**
     * @param {?string} data
     * @return {!Promise<!HTMLImageElement>}
     */
    function createImage(data) {
      var fulfill;
      var promise = new Promise(f => fulfill = f);

      var image = /** @type {!HTMLImageElement} */ (createElement('img'));
      if (data) {
        image.src = 'data:image/jpg;base64,' + data;
        image.addEventListener('load', () => fulfill(image));
        image.addEventListener('error', () => fulfill(image));
      } else {
        fulfill(image);
      }
      return promise;
    }
  }

  /**
   * @param {number} imageWidth
   * @param {number} imageHeight
   */
  _drawFrames(imageWidth, imageHeight) {
    if (!imageWidth)
      return;
    if (!this._filmStripModel.frames().length)
      return;
    var padding = Timeline.TimelineFilmStripOverview.Padding;
    var width = this.width();
    var zeroTime = this._filmStripModel.zeroTime();
    var spanTime = this._filmStripModel.spanTime();
    var scale = spanTime / width;
    var context = this.context();
    var drawGeneration = this._drawGeneration;

    context.beginPath();
    for (var x = padding; x < width; x += imageWidth + 2 * padding) {
      var time = zeroTime + (x + imageWidth / 2) * scale;
      var frame = this._filmStripModel.frameByTimestamp(time);
      if (!frame)
        continue;
      context.rect(x - 0.5, 0.5, imageWidth + 1, imageHeight + 1);
      this._imageByFrame(frame).then(drawFrameImage.bind(this, x));
    }
    context.strokeStyle = '#ddd';
    context.stroke();

    /**
     * @param {number} x
     * @param {!HTMLImageElement} image
     * @this {Timeline.TimelineFilmStripOverview}
     */
    function drawFrameImage(x, image) {
      // Ignore draws deferred from a previous update call.
      if (this._drawGeneration !== drawGeneration)
        return;
      context.drawImage(image, x, 1, imageWidth, imageHeight);
    }
  }

  /**
   * @override
   * @param {number} x
   * @return {!Promise<?Element>}
   */
  popoverElementPromise(x) {
    if (!this._filmStripModel.frames().length)
      return Promise.resolve(/** @type {?Element} */ (null));

    var time = this.calculator().positionToTime(x);
    var frame = this._filmStripModel.frameByTimestamp(time);
    if (frame === this._lastFrame)
      return Promise.resolve(this._lastElement);
    var imagePromise = frame ? this._imageByFrame(frame) : Promise.resolve(this._emptyImage);
    return imagePromise.then(createFrameElement.bind(this));

    /**
     * @this {Timeline.TimelineFilmStripOverview}
     * @param {!HTMLImageElement} image
     * @return {?Element}
     */
    function createFrameElement(image) {
      var element = createElementWithClass('div', 'frame');
      element.createChild('div', 'thumbnail').appendChild(image);
      UI.appendStyle(element, 'timeline/timelinePanel.css');
      this._lastFrame = frame;
      this._lastElement = element;
      return element;
    }
  }

  /**
   * @override
   */
  reset() {
    this._lastFrame = undefined;
    this._lastElement = null;
    /** @type {!Map<!Components.FilmStripModel.Frame,!Promise<!HTMLImageElement>>} */
    this._frameToImagePromise = new Map();
    this._imageWidth = 0;
  }
};

Timeline.TimelineFilmStripOverview.Padding = 2;

/**
 * @unrestricted
 */
Timeline.TimelineEventOverviewFrames = class extends Timeline.TimelineEventOverview {
  /**
   * @param {!TimelineModel.TimelineModel} model
   * @param {!TimelineModel.TimelineFrameModel} frameModel
   */
  constructor(model, frameModel) {
    super('framerate', Common.UIString('FPS'), model);
    this._frameModel = frameModel;
  }

  /**
   * @override
   */
  update() {
    super.update();
    var height = this.height();
    var /** @const */ padding = 1 * window.devicePixelRatio;
    var /** @const */ baseFrameDurationMs = 1e3 / 60;
    var visualHeight = height - 2 * padding;
    var timeOffset = this._model.minimumRecordTime();
    var timeSpan = this._model.maximumRecordTime() - timeOffset;
    var scale = this.width() / timeSpan;
    var frames = this._frameModel.frames();
    var baseY = height - padding;
    var ctx = this.context();
    var bottomY = baseY + 10 * window.devicePixelRatio;
    var x = 0;
    var y = bottomY;
    if (!frames.length)
      return;

    var lineWidth = window.devicePixelRatio;
    var offset = lineWidth & 1 ? 0.5 : 0;
    var tickDepth = 1.5 * window.devicePixelRatio;
    ctx.beginPath();
    ctx.moveTo(0, y);
    for (var i = 0; i < frames.length; ++i) {
      var frame = frames[i];
      x = Math.round((frame.startTime - timeOffset) * scale) + offset;
      ctx.lineTo(x, y);
      ctx.lineTo(x, y + tickDepth);
      y = frame.idle ? bottomY :
                       Math.round(baseY - visualHeight * Math.min(baseFrameDurationMs / frame.duration, 1)) - offset;
      ctx.lineTo(x, y + tickDepth);
      ctx.lineTo(x, y);
    }
    if (frames.length) {
      var lastFrame = frames.peekLast();
      x = Math.round((lastFrame.startTime + lastFrame.duration - timeOffset) * scale) + offset;
      ctx.lineTo(x, y);
    }
    ctx.lineTo(x, bottomY);
    ctx.fillStyle = 'hsl(110, 50%, 88%)';
    ctx.strokeStyle = 'hsl(110, 50%, 60%)';
    ctx.lineWidth = lineWidth;
    ctx.fill();
    ctx.stroke();
  }
};

/**
 * @unrestricted
 */
Timeline.TimelineEventOverviewMemory = class extends Timeline.TimelineEventOverview {
  /**
   * @param {!TimelineModel.TimelineModel} model
   */
  constructor(model) {
    super('memory', Common.UIString('HEAP'), model);
    this._heapSizeLabel = this.element.createChild('div', 'memory-graph-label');
  }

  resetHeapSizeLabels() {
    this._heapSizeLabel.textContent = '';
  }

  /**
   * @override
   */
  update() {
    super.update();
    var ratio = window.devicePixelRatio;

    var events = this._model.mainThreadEvents();
    if (!events.length) {
      this.resetHeapSizeLabels();
      return;
    }

    var lowerOffset = 3 * ratio;
    var maxUsedHeapSize = 0;
    var minUsedHeapSize = 100000000000;
    var minTime = this._model.minimumRecordTime();
    var maxTime = this._model.maximumRecordTime();
    /**
     * @param {!SDK.TracingModel.Event} event
     * @return {boolean}
     */
    function isUpdateCountersEvent(event) {
      return event.name === TimelineModel.TimelineModel.RecordType.UpdateCounters;
    }
    events = events.filter(isUpdateCountersEvent);
    /**
     * @param {!SDK.TracingModel.Event} event
     */
    function calculateMinMaxSizes(event) {
      var counters = event.args.data;
      if (!counters || !counters.jsHeapSizeUsed)
        return;
      maxUsedHeapSize = Math.max(maxUsedHeapSize, counters.jsHeapSizeUsed);
      minUsedHeapSize = Math.min(minUsedHeapSize, counters.jsHeapSizeUsed);
    }
    events.forEach(calculateMinMaxSizes);
    minUsedHeapSize = Math.min(minUsedHeapSize, maxUsedHeapSize);

    var lineWidth = 1;
    var width = this.width();
    var height = this.height() - lowerOffset;
    var xFactor = width / (maxTime - minTime);
    var yFactor = (height - lineWidth) / Math.max(maxUsedHeapSize - minUsedHeapSize, 1);

    var histogram = new Array(width);

    /**
     * @param {!SDK.TracingModel.Event} event
     */
    function buildHistogram(event) {
      var counters = event.args.data;
      if (!counters || !counters.jsHeapSizeUsed)
        return;
      var x = Math.round((event.startTime - minTime) * xFactor);
      var y = Math.round((counters.jsHeapSizeUsed - minUsedHeapSize) * yFactor);
      histogram[x] = Math.max(histogram[x] || 0, y);
    }
    events.forEach(buildHistogram);

    var ctx = this.context();
    var heightBeyondView = height + lowerOffset + lineWidth;

    ctx.translate(0.5, 0.5);
    ctx.beginPath();
    ctx.moveTo(-lineWidth, heightBeyondView);
    var y = 0;
    var isFirstPoint = true;
    var lastX = 0;
    for (var x = 0; x < histogram.length; x++) {
      if (typeof histogram[x] === 'undefined')
        continue;
      if (isFirstPoint) {
        isFirstPoint = false;
        y = histogram[x];
        ctx.lineTo(-lineWidth, height - y);
      }
      var nextY = histogram[x];
      if (Math.abs(nextY - y) > 2 && Math.abs(x - lastX) > 1)
        ctx.lineTo(x, height - y);
      y = nextY;
      ctx.lineTo(x, height - y);
      lastX = x;
    }
    ctx.lineTo(width + lineWidth, height - y);
    ctx.lineTo(width + lineWidth, heightBeyondView);
    ctx.closePath();

    ctx.fillStyle = 'hsla(220, 90%, 70%, 0.2)';
    ctx.fill();
    ctx.lineWidth = lineWidth;
    ctx.strokeStyle = 'hsl(220, 90%, 70%)';
    ctx.stroke();

    this._heapSizeLabel.textContent =
        Common.UIString('%s \u2013 %s', Number.bytesToString(minUsedHeapSize), Number.bytesToString(maxUsedHeapSize));
  }
};

/**
 * @unrestricted
 */
Timeline.Quantizer = class {
  /**
   * @param {number} startTime
   * @param {number} quantDuration
   * @param {function(!Array<number>)} callback
   */
  constructor(startTime, quantDuration, callback) {
    this._lastTime = startTime;
    this._quantDuration = quantDuration;
    this._callback = callback;
    this._counters = [];
    this._remainder = quantDuration;
  }

  /**
   * @param {number} time
   * @param {number} group
   */
  appendInterval(time, group) {
    var interval = time - this._lastTime;
    if (interval <= this._remainder) {
      this._counters[group] = (this._counters[group] || 0) + interval;
      this._remainder -= interval;
      this._lastTime = time;
      return;
    }
    this._counters[group] = (this._counters[group] || 0) + this._remainder;
    this._callback(this._counters);
    interval -= this._remainder;
    while (interval >= this._quantDuration) {
      var counters = [];
      counters[group] = this._quantDuration;
      this._callback(counters);
      interval -= this._quantDuration;
    }
    this._counters = [];
    this._counters[group] = interval;
    this._lastTime = time;
    this._remainder = this._quantDuration - interval;
  }
};
