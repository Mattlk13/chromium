// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextDetector_h
#define TextDetector_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "modules/canvas2d/CanvasRenderingContext2D.h"
#include "modules/shapedetection/ShapeDetector.h"
#include "public/platform/modules/shapedetection/textdetection.mojom-blink.h"

namespace blink {

class LocalFrame;

class MODULES_EXPORT TextDetector final : public ShapeDetector,
                                          public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static TextDetector* create(Document&);

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit TextDetector(LocalFrame&);
  ~TextDetector() override = default;

  ScriptPromise doDetect(ScriptPromiseResolver*,
                         mojo::ScopedSharedBufferHandle,
                         int imageWidth,
                         int imageHeight) override;
  void onDetectText(ScriptPromiseResolver*,
                    Vector<mojom::blink::TextDetectionResultPtr>);
  void onTextServiceConnectionError();

  mojom::blink::TextDetectionPtr m_textService;

  HeapHashSet<Member<ScriptPromiseResolver>> m_textServiceRequests;
};

}  // namespace blink

#endif  // TextDetector_h
