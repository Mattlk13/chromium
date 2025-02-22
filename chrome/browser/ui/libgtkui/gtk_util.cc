// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/gtk_util.h"

#include <dlfcn.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/environment.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/size.h"

namespace {

const char kAuraTransientParent[] = "aura-transient-parent";

void CommonInitFromCommandLine(const base::CommandLine& command_line,
                               void (*init_func)(gint*, gchar***)) {
  const std::vector<std::string>& args = command_line.argv();
  int argc = args.size();
  std::unique_ptr<char* []> argv(new char*[argc + 1]);
  for (size_t i = 0; i < args.size(); ++i) {
    // TODO(piman@google.com): can gtk_init modify argv? Just being safe
    // here.
    argv[i] = strdup(args[i].c_str());
  }
  argv[argc] = NULL;
  char** argv_pointer = argv.get();

  {
    // http://crbug.com/423873
    ANNOTATE_SCOPED_MEMORY_LEAK;
    init_func(&argc, &argv_pointer);
  }
  for (size_t i = 0; i < args.size(); ++i) {
    free(argv[i]);
  }
}

#if GTK_MAJOR_VERSION == 3
void* GetGtk3SharedLibrary() {
  static void* gtk3lib = dlopen("libgtk-3.so.0", RTLD_LAZY);
  DCHECK(gtk3lib);
  return gtk3lib;
}
#endif

}  // namespace

namespace libgtkui {

// Theme colors returned by GetSystemColor().
const SkColor kInvalidColorIdColor = SkColorSetRGB(255, 0, 128);
const SkColor kURLTextColor = SkColorSetRGB(0x0b, 0x80, 0x43);

SkColor NormalURLColor(SkColor foreground) {
  color_utils::HSL fg_hsl, hue_hsl;
  color_utils::SkColorToHSL(foreground, &fg_hsl);
  color_utils::SkColorToHSL(kURLTextColor, &hue_hsl);

  // Only allow colors that have a fair amount of saturation in them (color vs
  // white). This means that our output color will always be fairly green.
  double s = std::max(0.5, fg_hsl.s);

  // Make sure the luminance is at least as bright as the |kURLTextColor| green
  // would be if we were to use that.
  double l;
  if (fg_hsl.l < hue_hsl.l)
    l = hue_hsl.l;
  else
    l = (fg_hsl.l + hue_hsl.l) / 2;

  color_utils::HSL output = {hue_hsl.h, s, l};
  return color_utils::HSLToSkColor(output, 255);
}

SkColor SelectedURLColor(SkColor foreground, SkColor background) {
  color_utils::HSL fg_hsl, bg_hsl, hue_hsl;
  color_utils::SkColorToHSL(foreground, &fg_hsl);
  color_utils::SkColorToHSL(background, &bg_hsl);
  color_utils::SkColorToHSL(kURLTextColor, &hue_hsl);

  // The saturation of the text should be opposite of the background, clamped
  // to 0.2-0.8. We make sure it's greater than 0.2 so there's some color, but
  // less than 0.8 so it's not the oversaturated neon-color.
  double opposite_s = 1 - bg_hsl.s;
  double s = std::max(0.2, std::min(0.8, opposite_s));

  // The luminance should match the luminance of the foreground text.  Again,
  // we clamp so as to have at some amount of color (green) in the text.
  double opposite_l = fg_hsl.l;
  double l = std::max(0.1, std::min(0.9, opposite_l));

  color_utils::HSL output = {hue_hsl.h, s, l};
  return color_utils::HSLToSkColor(output, 255);
}

void GtkInitFromCommandLine(const base::CommandLine& command_line) {
  CommonInitFromCommandLine(command_line, gtk_init);
}

// TODO(erg): This method was copied out of shell_integration_linux.cc. Because
// of how this library is structured as a stand alone .so, we can't call code
// from browser and above.
std::string GetDesktopName(base::Environment* env) {
#if defined(GOOGLE_CHROME_BUILD)
  return "google-chrome.desktop";
#else  // CHROMIUM_BUILD
  // Allow $CHROME_DESKTOP to override the built-in value, so that development
  // versions can set themselves as the default without interfering with
  // non-official, packaged versions using the built-in value.
  std::string name;
  if (env->GetVar("CHROME_DESKTOP", &name) && !name.empty())
    return name;
  return "chromium-browser.desktop";
#endif
}

guint GetGdkKeyCodeForAccelerator(const ui::Accelerator& accelerator) {
  // The second parameter is false because accelerator keys are expressed in
  // terms of the non-shift-modified key.
  return XKeysymForWindowsKeyCode(accelerator.key_code(), false);
}

GdkModifierType GetGdkModifierForAccelerator(
    const ui::Accelerator& accelerator) {
  int event_flag = accelerator.modifiers();
  int modifier = 0;
  if (event_flag & ui::EF_SHIFT_DOWN)
    modifier |= GDK_SHIFT_MASK;
  if (event_flag & ui::EF_CONTROL_DOWN)
    modifier |= GDK_CONTROL_MASK;
  if (event_flag & ui::EF_ALT_DOWN)
    modifier |= GDK_MOD1_MASK;
  return static_cast<GdkModifierType>(modifier);
}

int EventFlagsFromGdkState(guint state) {
  int flags = ui::EF_NONE;
  flags |= (state & GDK_SHIFT_MASK) ? ui::EF_SHIFT_DOWN : ui::EF_NONE;
  flags |= (state & GDK_LOCK_MASK) ? ui::EF_CAPS_LOCK_ON : ui::EF_NONE;
  flags |= (state & GDK_CONTROL_MASK) ? ui::EF_CONTROL_DOWN : ui::EF_NONE;
  flags |= (state & GDK_MOD1_MASK) ? ui::EF_ALT_DOWN : ui::EF_NONE;
  flags |= (state & GDK_BUTTON1_MASK) ? ui::EF_LEFT_MOUSE_BUTTON : ui::EF_NONE;
  flags |=
      (state & GDK_BUTTON2_MASK) ? ui::EF_MIDDLE_MOUSE_BUTTON : ui::EF_NONE;
  flags |= (state & GDK_BUTTON3_MASK) ? ui::EF_RIGHT_MOUSE_BUTTON : ui::EF_NONE;
  return flags;
}

void TurnButtonBlue(GtkWidget* button) {
#if GTK_MAJOR_VERSION == 2
  gtk_widget_set_can_default(button, true);
#else
  gtk_style_context_add_class(gtk_widget_get_style_context(button),
                              "suggested-action");
#endif
}

void SetGtkTransientForAura(GtkWidget* dialog, aura::Window* parent) {
  if (!parent || !parent->GetHost())
    return;

  gtk_widget_realize(dialog);
  GdkWindow* gdk_window = gtk_widget_get_window(dialog);

  // TODO(erg): Check to make sure we're using X11 if wayland or some other
  // display server ever happens. Otherwise, this will crash.
  XSetTransientForHint(GDK_WINDOW_XDISPLAY(gdk_window),
                       GDK_WINDOW_XID(gdk_window),
                       parent->GetHost()->GetAcceleratedWidget());

  // We also set the |parent| as a property of |dialog|, so that we can unlink
  // the two later.
  g_object_set_data(G_OBJECT(dialog), kAuraTransientParent, parent);
}

aura::Window* GetAuraTransientParent(GtkWidget* dialog) {
  return reinterpret_cast<aura::Window*>(
      g_object_get_data(G_OBJECT(dialog), kAuraTransientParent));
}

void ClearAuraTransientParent(GtkWidget* dialog) {
  g_object_set_data(G_OBJECT(dialog), kAuraTransientParent, NULL);
}

#if GTK_MAJOR_VERSION > 2
ScopedStyleContext AppendNode(GtkStyleContext* context,
                              const std::string& css_node) {
  GtkWidgetPath* path =
      context ? gtk_widget_path_copy(gtk_style_context_get_path(context))
              : gtk_widget_path_new();

  enum {
    CSS_TYPE,
    CSS_NAME,
    CSS_CLASS,
    CSS_PSEUDOCLASS,
  } part_type = CSS_TYPE;
  static const struct {
    const char* name;
    GtkStateFlags state_flag;
  } pseudo_classes[] = {
      {"active", GTK_STATE_FLAG_ACTIVE},
      {"hover", GTK_STATE_FLAG_PRELIGHT},
      {"selected", GTK_STATE_FLAG_SELECTED},
      {"disabled", GTK_STATE_FLAG_INSENSITIVE},
      {"indeterminate", GTK_STATE_FLAG_INCONSISTENT},
      {"focus", GTK_STATE_FLAG_FOCUSED},
      {"backdrop", GTK_STATE_FLAG_BACKDROP},
      // TODO(thomasanderson): These state flags are only available in
      // GTK 3.10 or later, which is unavailable in the wheezy
      // sysroot.  Add them once the sysroot is updated to jessie.
      // { "link",          GTK_STATE_FLAG_LINK },
      // { "visited",       GTK_STATE_FLAG_VISITED },
      // { "checked",       GTK_STATE_FLAG_CHECKED },
  };
  GtkStateFlags state =
      context ? gtk_style_context_get_state(context) : GTK_STATE_FLAG_NORMAL;
  base::StringTokenizer t(css_node, ".:#");
  t.set_options(base::StringTokenizer::RETURN_DELIMS);
  while (t.GetNext()) {
    if (t.token_is_delim()) {
      if (t.token_begin() == css_node.begin()) {
        // Special case for the first token.
        gtk_widget_path_append_type(path, G_TYPE_NONE);
      }
      switch (*t.token_begin()) {
        case '#':
          part_type = CSS_NAME;
          break;
        case '.':
          part_type = CSS_CLASS;
          break;
        case ':':
          part_type = CSS_PSEUDOCLASS;
          break;
        default:
          NOTREACHED();
      }
    } else {
      switch (part_type) {
        case CSS_NAME: {
          if (gtk_get_major_version() > 3 ||
              (gtk_get_major_version() == 3 && gtk_get_minor_version() >= 20)) {
            static auto* _gtk_widget_path_iter_set_object_name =
                reinterpret_cast<void (*)(GtkWidgetPath*, gint, const char*)>(
                    dlsym(GetGtk3SharedLibrary(),
                          "gtk_widget_path_iter_set_object_name"));
            DCHECK(_gtk_widget_path_iter_set_object_name);
            _gtk_widget_path_iter_set_object_name(path, -1, t.token().c_str());
          } else {
            gtk_widget_path_iter_add_class(path, -1, t.token().c_str());
          }
          break;
        }
        case CSS_TYPE: {
          GType type = g_type_from_name(t.token().c_str());
          DCHECK(type);
          gtk_widget_path_append_type(path, type);
          break;
        }
        case CSS_CLASS: {
          gtk_widget_path_iter_add_class(path, -1, t.token().c_str());
          break;
        }
        case CSS_PSEUDOCLASS: {
          GtkStateFlags state_flag = GTK_STATE_FLAG_NORMAL;
          for (const auto& pseudo_class_entry : pseudo_classes) {
            if (strcmp(pseudo_class_entry.name, t.token().c_str()) == 0) {
              state_flag = pseudo_class_entry.state_flag;
              break;
            }
          }
          state = static_cast<GtkStateFlags>(state | state_flag);
          break;
        }
      }
    }
  }
  auto child_context = ScopedStyleContext(gtk_style_context_new());
  gtk_style_context_set_path(child_context, path);
  gtk_style_context_set_state(child_context, state);
  gtk_style_context_set_parent(child_context, context);
  gtk_widget_path_unref(path);
  return child_context;
}

ScopedStyleContext GetStyleContextFromCss(const char* css_selector) {
  // Prepend "GtkWindow.background" to the selector since all widgets must live
  // in a window, but we don't want to specify that every time.
  auto context = AppendNode(nullptr, "GtkWindow.background");

  for (const auto& widget_type :
       base::SplitString(css_selector, base::kWhitespaceASCII,
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    context = AppendNode(context, widget_type);
  }
  return context;
}

SkColor GdkRgbaToSkColor(const GdkRGBA& color) {
  return SkColorSetARGB(color.alpha * 255, color.red * 255, color.green * 255,
                        color.blue * 255);
}

SkColor GetFgColor(const char* css_selector) {
  auto context = GetStyleContextFromCss(css_selector);
  GdkRGBA color;
  gtk_style_context_get_color(context, gtk_style_context_get_state(context),
                              &color);
  return GdkRgbaToSkColor(color);
}

GtkCssProvider* GetCssProvider(const char* css) {
  GtkCssProvider* provider = gtk_css_provider_new();
  GError* error = nullptr;
  gtk_css_provider_load_from_data(provider, css, -1, &error);
  DCHECK(!error);
  return provider;
}

void ApplyCssToContext(GtkStyleContext* context, GtkCssProvider* provider) {
  while (context) {
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider),
                                   G_MAXUINT);
    context = gtk_style_context_get_parent(context);
  }
}

void RemoveBorders(GtkStyleContext* context) {
  static GtkCssProvider* provider = GetCssProvider(
      "* {"
      "border-style: none;"
      "border-radius: 0px;"
      "border-width: 0px;"
      "border-image-width: 0px;"
      "padding: 0px;"
      "margin: 0px;"
      "}");
  ApplyCssToContext(context, provider);
}

void AddBorders(GtkStyleContext* context) {
  static GtkCssProvider* provider = GetCssProvider(
      "* {"
      "border-style: solid;"
      "border-radius: 0px;"
      "border-width: 1px;"
      "padding: 0px;"
      "margin: 0px;"
      "}");
  ApplyCssToContext(context, provider);
}

// A 1x1 cairo surface that GTK can render into.
class PixelSurface {
 public:
  PixelSurface()
      : surface_(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1)),
        cairo_(cairo_create(surface_)) {}

  ~PixelSurface() {
    cairo_destroy(cairo_);
    cairo_surface_destroy(surface_);
  }

  // Get the drawing context for GTK to use.
  cairo_t* cairo() { return cairo_; }

  // Get the color value of the single pixel.
  SkColor GetPixelValue() {
    return *reinterpret_cast<SkColor*>(cairo_image_surface_get_data(surface_));
  }

 private:
  cairo_surface_t* surface_;
  cairo_t* cairo_;
};

void RenderBackground(cairo_t* cr, GtkStyleContext* context) {
  if (!context)
    return;
  RenderBackground(cr, gtk_style_context_get_parent(context));
  gtk_render_background(context, cr, 0, 0, 1, 1);
}

SkColor GetBgColor(const char* css_selector) {
  // Backgrounds are more general than solid colors (eg. gradients),
  // but chromium requires us to boil this down to one color.  We
  // cannot use the background-color here because some themes leave it
  // set to a garbage color because a background-image will cover it
  // anyway.  So we instead render the background into a single pixel,
  // removing any borders, and hope that we get a good color.
  auto context = GetStyleContextFromCss(css_selector);
  RemoveBorders(context);
  PixelSurface surface;
  RenderBackground(surface.cairo(), context);
  return surface.GetPixelValue();
}

SkColor GetBorderColor(const char* css_selector) {
  // Borders have the same issue as backgrounds, due to the
  // border-image property.
  auto context = GetStyleContextFromCss(css_selector);
  GtkStateFlags state = gtk_style_context_get_state(context);
  GtkBorderStyle border_style = GTK_BORDER_STYLE_NONE;
  gtk_style_context_get(context, state, GTK_STYLE_PROPERTY_BORDER_STYLE,
                        &border_style, nullptr);
  GtkBorder border;
  gtk_style_context_get_border(context, state, &border);
  if ((border_style == GTK_BORDER_STYLE_NONE ||
       border_style == GTK_BORDER_STYLE_HIDDEN) ||
      (!border.left && !border.right && !border.top && !border.bottom)) {
    return SK_ColorTRANSPARENT;
  }

  AddBorders(context);
  PixelSurface surface;
  RenderBackground(surface.cairo(), context);
  gtk_render_frame(context, surface.cairo(), 0, 0, 1, 1);
  return surface.GetPixelValue();
}
#endif

}  // namespace libgtkui
