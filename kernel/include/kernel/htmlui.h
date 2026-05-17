#ifndef HTMLUI_H
#define HTMLUI_H

#include <libk/types.h>

// HTML Element Types
typedef enum {
  HTML_ELEMENT_DIV,
  HTML_ELEMENT_SPAN,
  HTML_ELEMENT_BUTTON,
  HTML_ELEMENT_TEXT
} html_element_type_t;

// CSS Properties
typedef struct {
  uint32_t background_color;
  uint32_t background_color2; // For gradients
  bool has_background;        // True if background was explicitly set
  bool is_gradient;
  bool is_gradient_vertical;
  uint32_t text_color;
  uint32_t hover_background_color;
  bool has_hover_style;
  int border_radius;
  int padding;
  int margin;
  int width;
  int height;
  bool has_shadow;
  int shadow_offset;
  uint32_t shadow_color;
} css_properties_t;

// HTML Element (DOM Node)
typedef struct html_element {
  html_element_type_t type;
  char *text;
  char *class_name;
  char *id;
  css_properties_t style;

  struct html_element *parent;
  struct html_element *first_child;
  struct html_element *next_sibling;

  // Computed layout
  int x, y;
  int computed_width, computed_height;
  bool is_hovered;
} html_element_t;

// Parser functions
html_element_t *html_parse(const char *html_string);
void html_free(html_element_t *root);

// CSS functions
void css_parse_inline_style(html_element_t *element, const char *style_string);
void css_apply_class_styles(html_element_t *element, const char *css_string);
void css_apply_styles(html_element_t *root, const char *css_string);

// Rendering functions
void html_render(html_element_t *root, int x, int y);
void html_layout(html_element_t *root, int parent_width);
void html_handle_mouse(html_element_t *root, int mouse_x, int mouse_y);

#endif
