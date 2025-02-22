// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/property_converter.h"

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_property.h"

namespace aura {

namespace {

// Get the WindowProperty's value as a byte array. Only supports aura properties
// that point to types with a matching std::vector<uint8_t> mojo::TypeConverter.
template <typename T>
std::unique_ptr<std::vector<uint8_t>> GetArray(Window* window,
                                               const WindowProperty<T>* key) {
  const T value = window->GetProperty(key);
  if (!value)
    return base::MakeUnique<std::vector<uint8_t>>();
  return base::MakeUnique<std::vector<uint8_t>>(
      mojo::ConvertTo<std::vector<uint8_t>>(*value));
}

}  // namespace

PropertyConverter::PropertyConverter() {
  // Add known aura properties with associated mus properties.
  RegisterProperty(client::kAlwaysOnTopKey,
                   ui::mojom::WindowManager::kAlwaysOnTop_Property);
  RegisterProperty(client::kAppIconKey,
                   ui::mojom::WindowManager::kAppIcon_Property);
  RegisterProperty(client::kAppIdKey,
                   ui::mojom::WindowManager::kAppID_Property);
  RegisterProperty(client::kNameKey, ui::mojom::WindowManager::kName_Property);
  RegisterProperty(client::kPreferredSize,
                   ui::mojom::WindowManager::kPreferredSize_Property);
  RegisterProperty(client::kResizeBehaviorKey,
                   ui::mojom::WindowManager::kResizeBehavior_Property);
  RegisterProperty(client::kRestoreBoundsKey,
                   ui::mojom::WindowManager::kRestoreBounds_Property);
  RegisterProperty(client::kShowStateKey,
                   ui::mojom::WindowManager::kShowState_Property);
  RegisterProperty(client::kTitleKey,
                   ui::mojom::WindowManager::kWindowTitle_Property);
  RegisterProperty(client::kWindowIconKey,
                   ui::mojom::WindowManager::kWindowIcon_Property);
}

PropertyConverter::~PropertyConverter() {}

bool PropertyConverter::ConvertPropertyForTransport(
    Window* window,
    const void* key,
    std::string* transport_name,
    std::unique_ptr<std::vector<uint8_t>>* transport_value) {
  *transport_name = GetTransportNameForPropertyKey(key);
  if (transport_name->empty())
    return false;

  auto image_key = static_cast<const WindowProperty<gfx::ImageSkia*>*>(key);
  if (image_properties_.count(image_key) > 0) {
    const gfx::ImageSkia* value = window->GetProperty(image_key);
    if (value) {
      // TODO(crbug.com/667566): Support additional scales or gfx::Image[Skia].
      SkBitmap bitmap = value->GetRepresentation(1.f).sk_bitmap();
      *transport_value = base::MakeUnique<std::vector<uint8_t>>(
          mojo::ConvertTo<std::vector<uint8_t>>(bitmap));
    } else {
      *transport_value = base::MakeUnique<std::vector<uint8_t>>();
    }
    return true;
  }

  auto rect_key = static_cast<const WindowProperty<gfx::Rect*>*>(key);
  if (rect_properties_.count(rect_key) > 0) {
    *transport_value = GetArray(window, rect_key);
    return true;
  }

  auto size_key = static_cast<const WindowProperty<gfx::Size*>*>(key);
  if (size_properties_.count(size_key) > 0) {
    *transport_value = GetArray(window, size_key);
    return true;
  }

  auto string_key = static_cast<const WindowProperty<std::string*>*>(key);
  if (string_properties_.count(string_key) > 0) {
    *transport_value = GetArray(window, string_key);
    return true;
  }

  auto string16_key = static_cast<const WindowProperty<base::string16*>*>(key);
  if (string16_properties_.count(string16_key) > 0) {
    *transport_value = GetArray(window, string16_key);
    return true;
  }

  // Handle primitive property types generically.
  DCHECK_GT(primitive_properties_.count(key), 0u);
  // TODO(msw): Using the int64_t accessor is wasteful for smaller types.
  const PrimitiveType value = window->GetPropertyInternal(key, 0);
  *transport_value = base::MakeUnique<std::vector<uint8_t>>(
      mojo::ConvertTo<std::vector<uint8_t>>(value));
  return true;
}

std::string PropertyConverter::GetTransportNameForPropertyKey(const void* key) {
  if (primitive_properties_.count(key) > 0)
    return primitive_properties_[key].second;

  auto image_key = static_cast<const WindowProperty<gfx::ImageSkia*>*>(key);
  if (image_properties_.count(image_key) > 0)
    return image_properties_[image_key];

  auto rect_key = static_cast<const WindowProperty<gfx::Rect*>*>(key);
  if (rect_properties_.count(rect_key) > 0)
    return rect_properties_[rect_key];

  auto size_key = static_cast<const WindowProperty<gfx::Size*>*>(key);
  if (size_properties_.count(size_key) > 0)
    return size_properties_[size_key];

  auto string_key = static_cast<const WindowProperty<std::string*>*>(key);
  if (string_properties_.count(string_key) > 0)
    return string_properties_[string_key];

  auto string16_key = static_cast<const WindowProperty<base::string16*>*>(key);
  if (string16_properties_.count(string16_key) > 0)
    return string16_properties_[string16_key];

  return std::string();
}

void PropertyConverter::SetPropertyFromTransportValue(
    Window* window,
    const std::string& transport_name,
    const std::vector<uint8_t>* data) {
  for (const auto& primitive_property : primitive_properties_) {
    if (primitive_property.second.second == transport_name) {
      // aura::Window only supports property types that fit in PrimitiveType.
      if (data->size() != 8u) {
        DVLOG(2) << "Property size mismatch (PrimitiveType): "
                 << transport_name;
        return;
      }
      const PrimitiveType value = mojo::ConvertTo<PrimitiveType>(*data);
      // TODO(msw): Should aura::Window just store all properties by name?
      window->SetPropertyInternal(primitive_property.first,
                                  primitive_property.second.first, nullptr,
                                  value, 0);
      return;
    }
  }

  for (const auto& image_property : image_properties_) {
    if (image_property.second == transport_name) {
      // TODO(msw): Validate the data somehow, before trying to convert?
      // TODO(crbug.com/667566): Support additional scales or gfx::Image[Skia].
      const SkBitmap bitmap = mojo::ConvertTo<SkBitmap>(*data);
      const gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
      window->SetProperty(image_property.first, new gfx::ImageSkia(image));
      return;
    }
  }

  for (const auto& rect_property : rect_properties_) {
    if (rect_property.second == transport_name) {
      if (data->size() != 16u) {
        DVLOG(2) << "Property size mismatch (gfx::Rect): " << transport_name;
        return;
      }
      const gfx::Rect value = mojo::ConvertTo<gfx::Rect>(*data);
      window->SetProperty(rect_property.first, new gfx::Rect(value));
      return;
    }
  }

  for (const auto& size_property : size_properties_) {
    if (size_property.second == transport_name) {
      if (data->size() != 8u) {
        DVLOG(2) << "Property size mismatch (gfx::Size): " << transport_name;
        return;
      }
      const gfx::Size value = mojo::ConvertTo<gfx::Size>(*data);
      window->SetProperty(size_property.first, new gfx::Size(value));
      return;
    }
  }

  for (const auto& string_property : string_properties_) {
    if (string_property.second == transport_name) {
      // TODO(msw): Validate the data somehow, before trying to convert?
      const std::string value = mojo::ConvertTo<std::string>(*data);
      window->SetProperty(string_property.first, new std::string(value));
      return;
    }
  }

  for (const auto& string16_property : string16_properties_) {
    if (string16_property.second == transport_name) {
      // TODO(msw): Validate the data somehow, before trying to convert?
      const base::string16 value = mojo::ConvertTo<base::string16>(*data);
      window->SetProperty(string16_property.first, new base::string16(value));
      return;
    }
  }

  DVLOG(2) << "Unknown mus property name: " << transport_name;
}

bool PropertyConverter::GetPropertyValueFromTransportValue(
    const std::string& transport_name,
    const std::vector<uint8_t>& transport_data,
    PrimitiveType* value) {
  // aura::Window only supports property types that fit in PrimitiveType.
  if (transport_data.size() != 8u) {
    DVLOG(2) << "Property size mismatch (PrimitiveType): " << transport_name;
    return false;
  }
  for (const auto& primitive_property : primitive_properties_) {
    if (primitive_property.second.second == transport_name) {
      *value = mojo::ConvertTo<PrimitiveType>(transport_data);
      return true;
    }
  }
  return false;
}

void PropertyConverter::RegisterProperty(
    const WindowProperty<gfx::ImageSkia*>* property,
    const char* transport_name) {
  image_properties_[property] = transport_name;
}

void PropertyConverter::RegisterProperty(
    const WindowProperty<gfx::Rect*>* property,
    const char* transport_name) {
  rect_properties_[property] = transport_name;
}

void PropertyConverter::RegisterProperty(
    const WindowProperty<gfx::Size*>* property,
    const char* transport_name) {
  size_properties_[property] = transport_name;
}

void PropertyConverter::RegisterProperty(
    const WindowProperty<std::string*>* property,
    const char* transport_name) {
  string_properties_[property] = transport_name;
}

void PropertyConverter::RegisterProperty(
    const WindowProperty<base::string16*>* property,
    const char* transport_name) {
  string16_properties_[property] = transport_name;
}

}  // namespace aura
