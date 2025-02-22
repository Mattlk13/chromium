/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/layout/BidiRunForLine.h"

#include "core/layout/line/InlineIterator.h"

namespace blink {

using namespace WTF::Unicode;

static LineLayoutItem firstLayoutObjectForDirectionalityDetermination(
    LineLayoutItem root,
    LineLayoutItem current = nullptr) {
  LineLayoutItem next = current;
  while (current) {
    if (treatAsIsolated(current.styleRef()) &&
        (current.isLayoutInline() || current.isLayoutBlock())) {
      if (current != root)
        current = nullptr;
      else
        current = next;
      break;
    }
    current = current.parent();
  }

  if (!current)
    current = root.slowFirstChild();

  while (current) {
    next = nullptr;
    if (isIteratorTarget(current) &&
        !(current.isText() &&
          LineLayoutText(current).isAllCollapsibleWhitespace()))
      break;

    if (!isIteratorTarget(LineLayoutItem(current)) &&
        !treatAsIsolated(current.styleRef()))
      next = current.slowFirstChild();

    if (!next) {
      while (current && current != root) {
        next = current.nextSibling();
        if (next)
          break;
        current = current.parent();
      }
    }

    if (!next)
      break;

    current = next;
  }

  return current;
}

TextDirection determinePlaintextDirectionality(LineLayoutItem root,
                                               LineLayoutItem current,
                                               unsigned pos) {
  LineLayoutItem firstLayoutObject =
      firstLayoutObjectForDirectionalityDetermination(root, current);
  InlineIterator iter(LineLayoutItem(root), firstLayoutObject,
                      firstLayoutObject == current ? pos : 0);
  InlineBidiResolver observer;
  observer.setStatus(BidiStatus(root.style()->direction(),
                                isOverride(root.style()->unicodeBidi())));
  observer.setPositionIgnoringNestedIsolates(iter);
  return observer.determineParagraphDirectionality();
}

static inline void setupResolverToResumeInIsolate(InlineBidiResolver& resolver,
                                                  LineLayoutItem root,
                                                  LineLayoutItem startObject) {
  if (root != startObject) {
    LineLayoutItem parent = startObject.parent();
    setupResolverToResumeInIsolate(resolver, root, parent);
    notifyObserverEnteredObject(&resolver, LineLayoutItem(startObject));
  }
}

void constructBidiRunsForLine(InlineBidiResolver& topResolver,
                              BidiRunList<BidiRun>& bidiRuns,
                              const InlineIterator& endOfLine,
                              VisualDirectionOverride override,
                              bool previousLineBrokeCleanly,
                              bool isNewUBAParagraph) {
  // FIXME: We should pass a BidiRunList into createBidiRunsForLine instead
  // of the resolver owning the runs.
  ASSERT(&topResolver.runs() == &bidiRuns);
  ASSERT(topResolver.position() != endOfLine);
  LineLayoutItem currentRoot = topResolver.position().root();
  topResolver.createBidiRunsForLine(endOfLine, override,
                                    previousLineBrokeCleanly);

  while (!topResolver.isolatedRuns().isEmpty()) {
    // It does not matter which order we resolve the runs as long as we
    // resolve them all.
    BidiIsolatedRun isolatedRun = topResolver.isolatedRuns().back();
    topResolver.isolatedRuns().pop_back();
    currentRoot = isolatedRun.root;

    LineLayoutItem startObj = isolatedRun.object;

    // Only inlines make sense with unicode-bidi: isolate (blocks are
    // already isolated).
    // FIXME: Because enterIsolate is not passed a LayoutObject, we have to
    // crawl up the tree to see which parent inline is the isolate. We could
    // change enterIsolate to take a LayoutObject and do this logic there,
    // but that would be a layering violation for BidiResolver (which knows
    // nothing about LayoutObject).
    LineLayoutItem isolatedInline =
        highestContainingIsolateWithinRoot(startObj, currentRoot);
    ASSERT(isolatedInline);

    InlineBidiResolver isolatedResolver;
    LineMidpointState& isolatedLineMidpointState =
        isolatedResolver.midpointState();
    isolatedLineMidpointState =
        topResolver.midpointStateForIsolatedRun(isolatedRun.runToReplace);
    EUnicodeBidi unicodeBidi = isolatedInline.style()->unicodeBidi();
    TextDirection direction;
    if (unicodeBidi == Plaintext) {
      direction = determinePlaintextDirectionality(
          isolatedInline, isNewUBAParagraph ? startObj : 0);
    } else {
      ASSERT(unicodeBidi == Isolate || unicodeBidi == IsolateOverride);
      direction = isolatedInline.style()->direction();
    }
    isolatedResolver.setStatus(BidiStatus::createForIsolate(
        direction, isOverride(unicodeBidi), isolatedRun.level));

    setupResolverToResumeInIsolate(isolatedResolver, isolatedInline, startObj);

    // The starting position is the beginning of the first run within the
    // isolate that was identified during the earlier call to
    // createBidiRunsForLine. This can be but is not necessarily the first
    // run within the isolate.
    InlineIterator iter =
        InlineIterator(LineLayoutItem(isolatedInline), LineLayoutItem(startObj),
                       isolatedRun.position);
    isolatedResolver.setPositionIgnoringNestedIsolates(iter);
    // We stop at the next end of line; we may re-enter this isolate in the
    // next call to constructBidiRuns().
    // FIXME: What should end and previousLineBrokeCleanly be?
    // rniwa says previousLineBrokeCleanly is just a WinIE hack and could
    // always be false here?
    isolatedResolver.createBidiRunsForLine(endOfLine, NoVisualOverride,
                                           previousLineBrokeCleanly);

    ASSERT(isolatedResolver.runs().runCount());
    if (isolatedResolver.runs().runCount())
      bidiRuns.replaceRunWithRuns(&isolatedRun.runToReplace,
                                  isolatedResolver.runs());

    // If we encountered any nested isolate runs, save them for later
    // processing.
    while (!isolatedResolver.isolatedRuns().isEmpty()) {
      BidiIsolatedRun runWithContext = isolatedResolver.isolatedRuns().back();
      isolatedResolver.isolatedRuns().pop_back();
      topResolver.setMidpointStateForIsolatedRun(
          runWithContext.runToReplace,
          isolatedResolver.midpointStateForIsolatedRun(
              runWithContext.runToReplace));
      topResolver.isolatedRuns().push_back(runWithContext);
    }
  }
}

}  // namespace blink
