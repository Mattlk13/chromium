// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_chromeos.h"

#include <stddef.h>

#include <algorithm>
#include <cstring>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/third_party/icu/icu_utf.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/ime_engine_handler_interface.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"

namespace {
ui::IMEEngineHandlerInterface* GetEngine() {
  return ui::IMEBridge::Get()->GetCurrentEngineHandler();
}
}  // namespace

namespace ui {

// InputMethodChromeOS implementation -----------------------------------------
InputMethodChromeOS::InputMethodChromeOS(
    internal::InputMethodDelegate* delegate)
    : composing_text_(false),
      composition_changed_(false),
      handling_key_event_(false),
      weak_ptr_factory_(this) {
  SetDelegate(delegate);
  ui::IMEBridge::Get()->SetInputContextHandler(this);

  UpdateContextFocusState();
}

InputMethodChromeOS::~InputMethodChromeOS() {
  ConfirmCompositionText();
  // We are dead, so we need to ask the client to stop relying on us.
  OnInputMethodChanged();

  if (ui::IMEBridge::Get())
    ui::IMEBridge::Get()->SetInputContextHandler(NULL);
}

void InputMethodChromeOS::DispatchKeyEvent(
    ui::KeyEvent* event,
    std::unique_ptr<AckCallback> ack_callback) {
  DCHECK(event->IsKeyEvent());
  DCHECK(!(event->flags() & ui::EF_IS_SYNTHESIZED));

  // For linux_chromeos, the ime keyboard cannot track the caps lock state by
  // itself, so need to call SetCapsLockEnabled() method to reflect the caps
  // lock state by the key event.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    chromeos::input_method::InputMethodManager* manager =
        chromeos::input_method::InputMethodManager::Get();
    if (manager) {
      chromeos::input_method::ImeKeyboard* keyboard = manager->GetImeKeyboard();
      if (keyboard && event->type() == ui::ET_KEY_PRESSED) {
        keyboard->SetCapsLockEnabled((event->key_code() == ui::VKEY_CAPITAL) ?
            !keyboard->CapsLockIsEnabled() : event->IsCapsLockOn());
      }
    }
  }

  // If |context_| is not usable, then we can only dispatch the key event as is.
  // We only dispatch the key event to input method when the |context_| is an
  // normal input field (not a password field).
  // Note: We need to send the key event to ibus even if the |context_| is not
  // enabled, so that ibus can have a chance to enable the |context_|.
  if (!IsNonPasswordInputFieldFocused() || !GetEngine()) {
    if (event->type() == ET_KEY_PRESSED) {
      if (ExecuteCharacterComposer(*event)) {
        // Treating as PostIME event if character composer handles key event and
        // generates some IME event,
        ProcessKeyEventPostIME(event, true);
        if (ack_callback)
          ack_callback->Run(true);
        return;
      }
      ProcessUnfilteredKeyPressEvent(event);
    } else {
      ignore_result(DispatchKeyEventPostIME(event));
    }
    if (ack_callback)
      ack_callback->Run(false);
    return;
  }

  handling_key_event_ = true;
  if (GetEngine()->IsInterestedInKeyEvent()) {
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback = base::Bind(
        &InputMethodChromeOS::ProcessKeyEventDone,
        weak_ptr_factory_.GetWeakPtr(),
        // Pass the ownership of the new copied event.
        base::Owned(new ui::KeyEvent(*event)), Passed(&ack_callback));
    GetEngine()->ProcessKeyEvent(*event, callback);
  } else {
    ProcessKeyEventDone(event, std::move(ack_callback), false);
  }
}

bool InputMethodChromeOS::OnUntranslatedIMEMessage(
    const base::NativeEvent& event,
    NativeEventResult* result) {
  return false;
}

void InputMethodChromeOS::ProcessKeyEventDone(
    ui::KeyEvent* event,
    std::unique_ptr<AckCallback> ack_callback,
    bool is_handled) {
  DCHECK(event);
  if (event->type() == ET_KEY_PRESSED) {
    if (is_handled) {
      // IME event has a priority to be handled, so that character composer
      // should be reset.
      character_composer_.Reset();
    } else {
      // If IME does not handle key event, passes keyevent to character composer
      // to be able to compose complex characters.
      is_handled = ExecuteCharacterComposer(*event);
    }
  }

  if (ack_callback)
    ack_callback->Run(is_handled);

  if (event->type() == ET_KEY_PRESSED || event->type() == ET_KEY_RELEASED)
    ProcessKeyEventPostIME(event, is_handled);

  handling_key_event_ = false;
}

void InputMethodChromeOS::DispatchKeyEvent(ui::KeyEvent* event) {
  DispatchKeyEvent(event, nullptr);
}

void InputMethodChromeOS::OnTextInputTypeChanged(
    const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;

  UpdateContextFocusState();

  ui::IMEEngineHandlerInterface* engine = GetEngine();
  if (engine) {
    // When focused input client is not changed, a text input type change should
    // cause blur/focus events to engine.
    // The focus in to or out from password field should also notify engine.
    engine->FocusOut();
    ui::IMEEngineHandlerInterface::InputContext context(
        GetTextInputType(), GetTextInputMode(), GetTextInputFlags());
    engine->FocusIn(context);
  }

  InputMethodBase::OnTextInputTypeChanged(client);
}

void InputMethodChromeOS::OnCaretBoundsChanged(const TextInputClient* client) {
  if (!IsInputFieldFocused() || !IsTextInputClientFocused(client))
    return;

  NotifyTextInputCaretBoundsChanged(client);

  if (!IsNonPasswordInputFieldFocused())
    return;

  // The current text input type should not be NONE if |context_| is focused.
  DCHECK(client == GetTextInputClient());
  DCHECK(!IsTextInputTypeNone());

  if (GetEngine())
    GetEngine()->SetCompositionBounds(GetCompositionBounds(client));

  chromeos::IMECandidateWindowHandlerInterface* candidate_window =
      ui::IMEBridge::Get()->GetCandidateWindowHandler();
  if (!candidate_window)
    return;

  const gfx::Rect caret_rect = client->GetCaretBounds();

  gfx::Rect composition_head;
  if (client->HasCompositionText())
    client->GetCompositionCharacterBounds(0, &composition_head);

  // Pepper doesn't support composition bounds, so fall back to caret bounds to
  // avoid a bad user experience (the IME window moved to upper left corner).
  if (composition_head.IsEmpty())
    composition_head = caret_rect;
  candidate_window->SetCursorBounds(caret_rect, composition_head);

  gfx::Range text_range;
  gfx::Range selection_range;
  base::string16 surrounding_text;
  if (!client->GetTextRange(&text_range) ||
      !client->GetTextFromRange(text_range, &surrounding_text) ||
      !client->GetSelectionRange(&selection_range)) {
    previous_surrounding_text_.clear();
    previous_selection_range_ = gfx::Range::InvalidRange();
    return;
  }

  if (previous_selection_range_ == selection_range &&
      previous_surrounding_text_ == surrounding_text)
    return;

  previous_selection_range_ = selection_range;
  previous_surrounding_text_ = surrounding_text;

  if (!selection_range.IsValid()) {
    // TODO(nona): Ideally selection_range should not be invalid.
    // TODO(nona): If javascript changes the focus on page loading, even (0,0)
    //             can not be obtained. Need investigation.
    return;
  }

  // Here SetSurroundingText accepts relative position of |surrounding_text|, so
  // we have to convert |selection_range| from node coordinates to
  // |surrounding_text| coordinates.
  if (!GetEngine())
    return;
  GetEngine()->SetSurroundingText(base::UTF16ToUTF8(surrounding_text),
                                  selection_range.start() - text_range.start(),
                                  selection_range.end() - text_range.start(),
                                  text_range.start());
}

void InputMethodChromeOS::CancelComposition(const TextInputClient* client) {
  if (IsNonPasswordInputFieldFocused() && IsTextInputClientFocused(client))
    ResetContext();
}

bool InputMethodChromeOS::IsCandidatePopupOpen() const {
  // TODO(yukishiino): Implement this method.
  return false;
}

void InputMethodChromeOS::OnWillChangeFocusedClient(
    TextInputClient* focused_before,
    TextInputClient* focused) {
  ConfirmCompositionText();

  if (GetEngine())
    GetEngine()->FocusOut();
}

void InputMethodChromeOS::OnDidChangeFocusedClient(
    TextInputClient* focused_before,
    TextInputClient* focused) {
  // Force to update the input type since client's TextInputStateChanged()
  // function might not be called if text input types before the client loses
  // focus and after it acquires focus again are the same.
  UpdateContextFocusState();

  if (GetEngine()) {
    ui::IMEEngineHandlerInterface::InputContext context(
        GetTextInputType(), GetTextInputMode(), GetTextInputFlags());
    GetEngine()->FocusIn(context);
  }
}

void InputMethodChromeOS::ConfirmCompositionText() {
  TextInputClient* client = GetTextInputClient();
  if (client && client->HasCompositionText())
    client->ConfirmCompositionText();

  ResetContext();
}

void InputMethodChromeOS::ResetContext() {
  if (!IsNonPasswordInputFieldFocused() || !GetTextInputClient())
    return;

  composition_.Clear();
  result_text_.clear();
  composing_text_ = false;
  composition_changed_ = false;

  // This function runs asynchronously.
  // Note: some input method engines may not support reset method, such as
  // ibus-anthy. But as we control all input method engines by ourselves, we can
  // make sure that all of the engines we are using support it correctly.
  if (GetEngine())
    GetEngine()->Reset();

  character_composer_.Reset();
}

void InputMethodChromeOS::UpdateContextFocusState() {
  ResetContext();
  OnInputMethodChanged();

  // Propagate the focus event to the candidate window handler which also
  // manages the input method mode indicator.
  chromeos::IMECandidateWindowHandlerInterface* candidate_window =
      ui::IMEBridge::Get()->GetCandidateWindowHandler();
  if (candidate_window)
    candidate_window->FocusStateChanged(IsNonPasswordInputFieldFocused());

  ui::IMEEngineHandlerInterface::InputContext context(
      GetTextInputType(), GetTextInputMode(), GetTextInputFlags());
  ui::IMEBridge::Get()->SetCurrentInputContext(context);

  if (!IsTextInputTypeNone())
    OnCaretBoundsChanged(GetTextInputClient());
}

void InputMethodChromeOS::ProcessKeyEventPostIME(ui::KeyEvent* event,
                                                 bool handled) {
  TextInputClient* client = GetTextInputClient();
  if (!client) {
    // As ibus works asynchronously, there is a chance that the focused client
    // loses focus before this method gets called.
    ignore_result(DispatchKeyEventPostIME(event));
    return;
  }

  if (event->type() == ET_KEY_PRESSED && handled) {
    ProcessFilteredKeyPressEvent(event);
    if (event->stopped_propagation()) {
      ResetContext();
      return;
    }
  }

  // In case the focus was changed by the key event. The |context_| should have
  // been reset when the focused window changed.
  if (client != GetTextInputClient())
    return;

  if (HasInputMethodResult())
    ProcessInputMethodResult(event, handled);

  // In case the focus was changed when sending input method results to the
  // focused window.
  if (client != GetTextInputClient())
    return;

  if (handled)
    return;  // IME handled the key event. do not forward.

  if (event->type() == ET_KEY_PRESSED)
    ProcessUnfilteredKeyPressEvent(event);
  else if (event->type() == ET_KEY_RELEASED)
    ignore_result(DispatchKeyEventPostIME(event));
}

void InputMethodChromeOS::ProcessFilteredKeyPressEvent(ui::KeyEvent* event) {
  if (NeedInsertChar()) {
    ignore_result(DispatchKeyEventPostIME(event));
    return;
  }
  ui::KeyEvent fabricated_event(ET_KEY_PRESSED,
                                VKEY_PROCESSKEY,
                                event->code(),
                                event->flags(),
                                event->GetDomKey(),
                                event->time_stamp());
  ignore_result(DispatchKeyEventPostIME(&fabricated_event));
  if (fabricated_event.stopped_propagation())
    event->StopPropagation();
}

void InputMethodChromeOS::ProcessUnfilteredKeyPressEvent(
    ui::KeyEvent* event) {
  const TextInputClient* prev_client = GetTextInputClient();
  ignore_result(DispatchKeyEventPostIME(event));
  if (event->stopped_propagation()) {
    ResetContext();
    return;
  }

  // We shouldn't dispatch the character anymore if the key event dispatch
  // caused focus change. For example, in the following scenario,
  // 1. visit a web page which has a <textarea>.
  // 2. click Omnibox.
  // 3. enable Korean IME, press A, then press Tab to move the focus to the web
  //    page.
  // We should return here not to send the Tab key event to RWHV.
  TextInputClient* client = GetTextInputClient();
  if (!client || client != prev_client)
    return;

  // If a key event was not filtered by |context_| and |character_composer_|,
  // then it means the key event didn't generate any result text. So we need
  // to send corresponding character to the focused text input client.
  uint16_t ch = event->GetCharacter();
  if (ch)
    client->InsertChar(*event);
}

void InputMethodChromeOS::ProcessInputMethodResult(ui::KeyEvent* event,
                                                   bool handled) {
  TextInputClient* client = GetTextInputClient();
  DCHECK(client);

  if (result_text_.length()) {
    if (handled && NeedInsertChar()) {
      for (base::string16::const_iterator i = result_text_.begin();
           i != result_text_.end(); ++i) {
        ui::KeyEvent ch_event(*event);
        ch_event.set_character(*i);
        client->InsertChar(ch_event);
      }
    } else {
      client->InsertText(result_text_);
      composing_text_ = false;
    }
  }

  if (composition_changed_ && !IsTextInputTypeNone()) {
    if (composition_.text.length()) {
      composing_text_ = true;
      client->SetCompositionText(composition_);
    } else if (result_text_.empty()) {
      client->ClearCompositionText();
    }
  }

  // We should not clear composition text here, as it may belong to the next
  // composition session.
  result_text_.clear();
  composition_changed_ = false;
}

bool InputMethodChromeOS::NeedInsertChar() const {
  return GetTextInputClient() &&
      (IsTextInputTypeNone() ||
       (!composing_text_ && result_text_.length() == 1));
}

bool InputMethodChromeOS::HasInputMethodResult() const {
  return result_text_.length() || composition_changed_;
}

void InputMethodChromeOS::CommitText(const std::string& text) {
  if (text.empty())
    return;

  // We need to receive input method result even if the text input type is
  // TEXT_INPUT_TYPE_NONE, to make sure we can always send correct
  // character for each key event to the focused text input client.
  if (!GetTextInputClient())
    return;

  const base::string16 utf16_text = base::UTF8ToUTF16(text);
  if (utf16_text.empty())
    return;

  if (!CanComposeInline()) {
    // Hides the candidate window for preedit text.
    UpdateCompositionText(CompositionText(), 0, false);
  }

  // Append the text to the buffer, because commit signal might be fired
  // multiple times when processing a key event.
  result_text_.append(utf16_text);

  // If we are not handling key event, do not bother sending text result if the
  // focused text input client does not support text input.
  if (!handling_key_event_ && !IsTextInputTypeNone()) {
    if (!SendFakeProcessKeyEvent(true))
      GetTextInputClient()->InsertText(utf16_text);
    SendFakeProcessKeyEvent(false);
    result_text_.clear();
  }
}

void InputMethodChromeOS::UpdateCompositionText(const CompositionText& text,
                                                uint32_t cursor_pos,
                                                bool visible) {
  if (IsTextInputTypeNone())
    return;

  if (!CanComposeInline()) {
    chromeos::IMECandidateWindowHandlerInterface* candidate_window =
        ui::IMEBridge::Get()->GetCandidateWindowHandler();
    if (candidate_window)
      candidate_window->UpdatePreeditText(text.text, cursor_pos, visible);
  }

  // |visible| argument is very confusing. For example, what's the correct
  // behavior when:
  // 1. OnUpdatePreeditText() is called with a text and visible == false, then
  // 2. OnShowPreeditText() is called afterwards.
  //
  // If it's only for clearing the current preedit text, then why not just use
  // OnHidePreeditText()?
  if (!visible) {
    HidePreeditText();
    return;
  }

  ExtractCompositionText(text, cursor_pos, &composition_);

  composition_changed_ = true;

  // In case OnShowPreeditText() is not called.
  if (composition_.text.length())
    composing_text_ = true;

  if (!handling_key_event_) {
    // If we receive a composition text without pending key event, then we need
    // to send it to the focused text input client directly.
    if (!SendFakeProcessKeyEvent(true))
      GetTextInputClient()->SetCompositionText(composition_);
    SendFakeProcessKeyEvent(false);
    composition_changed_ = false;
    composition_.Clear();
  }
}

void InputMethodChromeOS::HidePreeditText() {
  if (composition_.text.empty() || IsTextInputTypeNone())
    return;

  // Intentionally leaves |composing_text_| unchanged.
  composition_changed_ = true;
  composition_.Clear();

  if (!handling_key_event_) {
    TextInputClient* client = GetTextInputClient();
    if (client && client->HasCompositionText()) {
      if (!SendFakeProcessKeyEvent(true))
        client->ClearCompositionText();
      SendFakeProcessKeyEvent(false);
    }
    composition_changed_ = false;
  }
}

void InputMethodChromeOS::DeleteSurroundingText(int32_t offset,
                                                uint32_t length) {
  if (!composition_.text.empty())
    return;  // do nothing if there is ongoing composition.

  if (GetTextInputClient()) {
    uint32_t before = offset >= 0 ? 0U : static_cast<uint32_t>(-1 * offset);
    GetTextInputClient()->ExtendSelectionAndDelete(before, length - before);
  }
}

bool InputMethodChromeOS::ExecuteCharacterComposer(const ui::KeyEvent& event) {
  if (!character_composer_.FilterKeyPress(event))
    return false;

  // CharacterComposer consumed the key event.  Update the composition text.
  CompositionText preedit;
  preedit.text = character_composer_.preedit_string();
  UpdateCompositionText(preedit, preedit.text.size(), !preedit.text.empty());
  std::string commit_text =
      base::UTF16ToUTF8(character_composer_.composed_character());
  if (!commit_text.empty()) {
    CommitText(commit_text);
  }
  return true;
}

void InputMethodChromeOS::ExtractCompositionText(
    const CompositionText& text,
    uint32_t cursor_position,
    CompositionText* out_composition) const {
  out_composition->Clear();
  out_composition->text = text.text;

  if (out_composition->text.empty())
    return;

  // ibus uses character index for cursor position and attribute range, but we
  // use char16 offset for them. So we need to do conversion here.
  std::vector<size_t> char16_offsets;
  size_t length = out_composition->text.length();
  base::i18n::UTF16CharIterator char_iterator(&out_composition->text);
  do {
    char16_offsets.push_back(char_iterator.array_pos());
  } while (char_iterator.Advance());

  // The text length in Unicode characters.
  uint32_t char_length = static_cast<uint32_t>(char16_offsets.size());
  // Make sure we can convert the value of |char_length| as well.
  char16_offsets.push_back(length);

  size_t cursor_offset =
      char16_offsets[std::min(char_length, cursor_position)];

  out_composition->selection = gfx::Range(cursor_offset);

  const CompositionUnderlines text_underlines = text.underlines;
  if (!text_underlines.empty()) {
    for (size_t i = 0; i < text_underlines.size(); ++i) {
      const uint32_t start = text_underlines[i].start_offset;
      const uint32_t end = text_underlines[i].end_offset;
      if (start >= end)
        continue;
      CompositionUnderline underline(
          char16_offsets[start], char16_offsets[end], text_underlines[i].color,
          text_underlines[i].thick, text_underlines[i].background_color);
      out_composition->underlines.push_back(underline);
    }
  }

  DCHECK(text.selection.start() <= text.selection.end());
  if (text.selection.start() < text.selection.end()) {
    const uint32_t start = text.selection.start();
    const uint32_t end = text.selection.end();
    CompositionUnderline underline(char16_offsets[start],
                                   char16_offsets[end],
                                   SK_ColorBLACK,
                                   true /* thick */,
                                   SK_ColorTRANSPARENT);
    out_composition->underlines.push_back(underline);

    // If the cursor is at start or end of this underline, then we treat
    // it as the selection range as well, but make sure to set the cursor
    // position to the selection end.
    if (underline.start_offset == cursor_offset) {
      out_composition->selection.set_start(underline.end_offset);
      out_composition->selection.set_end(cursor_offset);
    } else if (underline.end_offset == cursor_offset) {
      out_composition->selection.set_start(underline.start_offset);
      out_composition->selection.set_end(cursor_offset);
    }
  }

  // Use a black thin underline by default.
  if (out_composition->underlines.empty()) {
    out_composition->underlines.push_back(CompositionUnderline(
        0, length, SK_ColorBLACK, false /* thick */, SK_ColorTRANSPARENT));
  }
}

bool InputMethodChromeOS::IsNonPasswordInputFieldFocused() {
  TextInputType type = GetTextInputType();
  return (type != TEXT_INPUT_TYPE_NONE) && (type != TEXT_INPUT_TYPE_PASSWORD);
}

bool InputMethodChromeOS::IsInputFieldFocused() {
  return GetTextInputType() != TEXT_INPUT_TYPE_NONE;
}

}  // namespace ui
