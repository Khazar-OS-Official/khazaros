#include <kernel/htmlui.h>
#include <drivers/gfx.h>
#include <mm/kheap.h>
#include <libk/string.h>

extern void kprintf(const char *fmt, ...);

// Local string helpers (to avoid linker issues)
static char *html_strcpy(char *dest, const char *src) {
  char *d = dest;
  while ((*d++ = *src++))
    ;
  return dest;
}

static const char *html_strstr(const char *haystack, const char *needle) {
  if (!*needle)
    return haystack;
  for (; *haystack; haystack++) {
    const char *h = haystack;
    const char *n = needle;
    while (*h && *n && (*h == *n)) {
      h++;
      n++;
    }
    if (!*n)
      return haystack;
  }
  return NULL;
}

// Helper: Skip whitespace
static const char *skip_whitespace(const char *str) {
  while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')
    str++;
  return str;
}

// Internal CSS property parser
static void css_parse_property(css_properties_t *style, const char *key,
                               const char *val) {
  val = skip_whitespace(val);

  if (strcmp(key, "background-color") == 0 || strcmp(key, "background") == 0) {
    if (val[0] == '#') {
      uint32_t color = 0;
      for (int i = 1; i < 7 && val[i]; i++) {
        color <<= 4;
        if (val[i] >= '0' && val[i] <= '9')
          color |= (val[i] - '0');
        else if (val[i] >= 'a' && val[i] <= 'f')
          color |= (val[i] - 'a' + 10);
        else if (val[i] >= 'A' && val[i] <= 'F')
          color |= (val[i] - 'A' + 10);
      }
      style->background_color = color;
      style->has_background = true;
      style->is_gradient = false;
    }
  } else if (strcmp(key, "background-gradient") == 0) {
    // Format: background-gradient: #color1 #color2 vertical;
    const char *c1 = html_strstr(val, "#");
    if (c1) {
      uint32_t color = 0;
      for (int i = 1; i < 7 && c1[i]; i++) {
        color <<= 4;
        if (c1[i] >= '0' && c1[i] <= '9')
          color |= (c1[i] - '0');
        else if (c1[i] >= 'a' && c1[i] <= 'f')
          color |= (c1[i] - 'a' + 10);
        else if (c1[i] >= 'A' && c1[i] <= 'F')
          color |= (c1[i] - 'A' + 10);
      }
      style->background_color = color;

      const char *c2 = html_strstr(c1 + 1, "#");
      if (c2) {
        uint32_t color2 = 0;
        for (int i = 1; i < 7 && c2[i]; i++) {
          color2 <<= 4;
          if (c2[i] >= '0' && c2[i] <= '9')
            color2 |= (c2[i] - '0');
          else if (c2[i] >= 'a' && c2[i] <= 'f')
            color2 |= (c2[i] - 'a' + 10);
          else if (c2[i] >= 'A' && c2[i] <= 'F')
            color2 |= (c2[i] - 'A' + 10);
        }
        style->background_color2 = color2;
        style->has_background = true;
        style->is_gradient = true;
        style->is_gradient_vertical = true;

        // Debug & Fallback
        kprintf("CSS Gradient: c1=%x c2=%x\n", color, color2);
        if (color == 0 && color2 == 0) {
          style->background_color = 0x000000FF;  // Blue
          style->background_color2 = 0x00FF0000; // Red
          kprintf("CSS Gradient: Fallback applied (Blue->Red)\n");
        }
      }
    }
  } else if (strcmp(key, "color") == 0) {
    if (val[0] == '#') {
      uint32_t color = 0;
      for (int i = 1; i < 7 && val[i]; i++) {
        color <<= 4;
        if (val[i] >= '0' && val[i] <= '9')
          color |= (val[i] - '0');
        else if (val[i] >= 'a' && val[i] <= 'f')
          color |= (val[i] - 'a' + 10);
        else if (val[i] >= 'A' && val[i] <= 'F')
          color |= (val[i] - 'A' + 10);
      }
      style->text_color = color;
    }
  } else if (strcmp(key, "border-radius") == 0) {
    int r = 0;
    while (*val >= '0' && *val <= '9') {
      r = r * 10 + (*val - '0');
      val++;
    }
    style->border_radius = r;
  } else if (strcmp(key, "padding") == 0) {
    int p = 0;
    while (*val >= '0' && *val <= '9') {
      p = p * 10 + (*val - '0');
      val++;
    }
    style->padding = p;
  } else if (strcmp(key, "margin") == 0) {
    int m = 0;
    while (*val >= '0' && *val <= '9') {
      m = m * 10 + (*val - '0');
      val++;
    }
    style->margin = m;
  } else if (strcmp(key, "hover-background-color") == 0) {
    if (val[0] == '#') {
      uint32_t color = 0;
      for (int i = 1; i < 7 && val[i]; i++) {
        color <<= 4;
        if (val[i] >= '0' && val[i] <= '9')
          color |= (val[i] - '0');
        else if (val[i] >= 'a' && val[i] <= 'f')
          color |= (val[i] - 'a' + 10);
        else if (val[i] >= 'A' && val[i] <= 'F')
          color |= (val[i] - 'A' + 10);
      }
      style->hover_background_color = color;
      style->has_hover_style = true;
    }
  } else if (strcmp(key, "box-shadow") == 0 || strcmp(key, "shadow") == 0) {
    if (html_strstr(val, "true") || html_strstr(val, "1")) {
      style->has_shadow = true;
      style->shadow_offset = 4;
    } else {
      style->has_shadow = false;
    }
  } else if (strcmp(key, "shadow-offset") == 0) {
    int o = 0;
    while (*val >= '0' && *val <= '9') {
      o = o * 10 + (*val - '0');
      val++;
    }
    style->shadow_offset = o;
  }
}

static void apply_to_tree_internal(html_element_t *node, const char *cls,
                                   const char *k, const char *v) {
  if (node->class_name && strcmp(node->class_name, cls) == 0) {
    css_parse_property(&node->style, k, v);
  }
  html_element_t *child = node->first_child;
  while (child) {
    apply_to_tree_internal(child, cls, k, v);
    child = child->next_sibling;
  }
}

// Helper: Create new element
static html_element_t *create_element(html_element_type_t type) {
  html_element_t *elem = (html_element_t *)kmalloc(sizeof(html_element_t));
  if (!elem)
    return NULL;

  // Clear memory to avoid garbage values
  memset(elem, 0, sizeof(html_element_t));

  elem->type = type;
  elem->style.background_color = 0; // Default transparent
  elem->style.background_color2 = 0;
  elem->style.has_background = false; // No background until explicitly set
  elem->style.is_gradient = false;
  elem->style.is_gradient_vertical = true;
  elem->style.text_color = COLOR_WHITE; // Default white text
  elem->style.hover_background_color = 0x00FFFFFF;
  elem->style.has_hover_style = false;
  elem->style.border_radius = 0;
  elem->style.padding = 0;
  elem->style.margin = 0;
  elem->style.width = -1;  // Auto
  elem->style.height = -1; // Auto
  elem->style.has_shadow = false;
  elem->style.shadow_offset = 0;
  elem->style.shadow_color = 0x00000000;

  return elem;
}

// Simple HTML parser (very basic, just for our UI)
html_element_t *html_parse(const char *html_string) {
  const char *p = html_string;
  // Create a temporary wrapper to hold the tree during parsing
  html_element_t wrapper;
  wrapper.first_child = NULL;
  wrapper.parent = NULL;
  wrapper.next_sibling = NULL;
  html_element_t *current = &wrapper;

  while (*p) {
    p = skip_whitespace(p);
    if (*p == '\0')
      break;

    if (*p == '<') {
      p++; // Skip <

      // Check for closing tag
      if (*p == '/') {
        // Move up to parent
        if (current->parent) {
          current = current->parent;
        }
        // Skip to >
        while (*p && *p != '>')
          p++;
        if (*p == '>')
          p++;
        continue;
      }

      // Parse tag name
      char tag_name[32] = {0};
      int i = 0;
      while (*p && *p != ' ' && *p != '>' && i < 31) {
        tag_name[i++] = *p++;
      }
      tag_name[i] = '\0';

      // Create element based on tag
      html_element_t *new_elem = NULL;
      if (strcmp(tag_name, "div") == 0) {
        new_elem = create_element(HTML_ELEMENT_DIV);
      } else if (strcmp(tag_name, "span") == 0) {
        new_elem = create_element(HTML_ELEMENT_SPAN);
      } else if (strcmp(tag_name, "button") == 0) {
        new_elem = create_element(HTML_ELEMENT_BUTTON);
      }

      if (new_elem) {
        // Parse attributes
        while (*p && *p != '>') {
          p = skip_whitespace(p);
          if (*p == '>')
            break;

          // Get attribute name
          char attr_name[32] = {0};
          i = 0;
          while (*p && *p != '=' && *p != ' ' && *p != '>' && i < 31) {
            attr_name[i++] = *p++;
          }
          attr_name[i] = '\0';

          if (*p == '=') {
            p++; // Skip =
            if (*p == '"')
              p++; // Skip opening quote

            // Get attribute value
            char attr_value[128] = {0};
            i = 0;
            while (*p && *p != '"' && *p != '>' && i < 127) {
              attr_value[i++] = *p++;
            }
            attr_value[i] = '\0';

            if (*p == '"')
              p++; // Skip closing quote

            // Apply attribute
            if (strcmp(attr_name, "class") == 0) {
              new_elem->class_name = (char *)kmalloc(strlen(attr_value) + 1);
              if (new_elem->class_name)
                html_strcpy(new_elem->class_name, attr_value);
            } else if (strcmp(attr_name, "id") == 0) {
              new_elem->id = (char *)kmalloc(strlen(attr_value) + 1);
              if (new_elem->id)
                html_strcpy(new_elem->id, attr_value);
            }
          }
        }

        if (*p == '>')
          p++; // Skip >

        // Add to tree
        new_elem->parent = current;
        if (!current->first_child) {
          current->first_child = new_elem;
        } else {
          html_element_t *sibling = current->first_child;
          while (sibling->next_sibling) {
            sibling = sibling->next_sibling;
          }
          sibling->next_sibling = new_elem;
        }

        current = new_elem;
      }
    } else {
      // Text content
      char text[256] = {0};
      int i = 0;
      while (*p && *p != '<' && i < 255) {
        text[i++] = *p++;
      }
      text[i] = '\0';

      // Trim whitespace
      const char *trimmed = skip_whitespace(text);
      if (*trimmed) {
        html_element_t *text_elem = create_element(HTML_ELEMENT_TEXT);
        if (text_elem) {
          text_elem->text = (char *)kmalloc(strlen(trimmed) + 1);
          if (text_elem->text)
            html_strcpy(text_elem->text, trimmed);

          text_elem->parent = current;
          if (!current->first_child) {
            current->first_child = text_elem;
          } else {
            html_element_t *sibling = current->first_child;
            while (sibling->next_sibling) {
              sibling = sibling->next_sibling;
            }
            sibling->next_sibling = text_elem;
          }
        }
      }
    }
  }

  // Return the first actual parsed element (skip the temporary wrapper)
  html_element_t *result = wrapper.first_child;
  if (result) {
    result->parent = NULL; // Detach from stack wrapper
  }
  return result;
}

// Free HTML tree
void html_free(html_element_t *root) {
  if (!root)
    return;

  // Free children
  html_element_t *child = root->first_child;
  while (child) {
    html_element_t *next = child->next_sibling;
    html_free(child);
    child = next;
  }

  // Free strings
  if (root->text)
    kfree(root->text);
  if (root->class_name)
    kfree(root->class_name);
  if (root->id)
    kfree(root->id);

  kfree(root);
}

// Parse simple CSS property (e.g., "background: #ff0000")
void css_parse_inline_style(html_element_t *element, const char *style_string) {
  // Very simple parser for demonstration
  if (html_strstr(style_string, "background:")) {
    // Parse hex color
    const char *color_start = html_strstr(style_string, "#");
    if (color_start) {
      // Simple hex parsing (assumes #RRGGBB format)
      element->style.background_color = 0x00CCCCCC; // Placeholder
    }
  }
}

// Apply CSS class styles
void css_apply_class_styles(html_element_t *element, const char *css_string) {
  if (!element->class_name)
    return;

  // Hardcoded styles to bypass CSS parser fragility
  if (strcmp(element->class_name, "menu-item") == 0) {
    element->style.background_color = 0x00333333;
    element->style.has_background = true;
    element->style.text_color = COLOR_WHITE;
    element->style.padding = 8;
    element->style.border_radius = 4;
  } else if (strcmp(element->class_name, "start-menu") == 0) {
    element->style.background_color = 0x0016213e;
    element->style.has_background = true;
    element->style.padding = 10;
    element->style.border_radius = 8;
    element->style.has_shadow = true;
    element->style.shadow_offset = 4;
    element->style.shadow_color = 0x00000000;
  } else if (strcmp(element->class_name, "menu-container") == 0) {
    // Start Menu Container
    element->style.background_color = 0x0016213e;
    element->style.background_color2 = 0x000f3460;
    element->style.is_gradient = true;
    element->style.is_gradient_vertical = true;
    element->style.has_background = true;
    element->style.padding = 10;
    element->style.border_radius = 8;
    element->style.has_shadow = true;
    element->style.shadow_offset = 6;
  } else if (strcmp(element->class_name, "header") == 0) {
    // Start Menu Header
    element->style.text_color = 0x0087CEEB;
    element->style.padding = 5;
    element->style.margin = 2;
  } else if (strcmp(element->class_name, "item") == 0) {
    // Start Menu Item
    element->style.background_color = 0x001a1a2e;
    element->style.has_background = true;
    element->style.text_color = COLOR_WHITE;
    element->style.padding = 8;
    element->style.margin = 2;
    element->style.border_radius = 4;
    element->style.hover_background_color = 0x000f3460;
    element->style.has_hover_style = true;
  } else if (strcmp(element->class_name, "ctx-menu") == 0) {
    // Desktop Context Menu
    element->style.background_color = 0x00FFFFFF; // White
    element->style.has_background = true;
    element->style.padding = 5;
    element->style.border_radius = 4;
    element->style.has_shadow = true;
    element->style.shadow_offset = 4;
  } else if (strcmp(element->class_name, "mi") == 0) {
    // Context Menu Item
    element->style.text_color = 0x00333333; // Dark Grey text
    element->style.padding = 5;
    element->style.margin = 1;
    element->style.border_radius = 2;
    element->style.hover_background_color = 0x00e0e0e0; // Light Grey hover
    element->style.has_hover_style = true;
  }

  // Apply to children
  html_element_t *child = element->first_child;
  while (child) {
    css_apply_class_styles(child, css_string);
    child = child->next_sibling;
  }
}

// Full CSS String Parser (e.g. ".class { prop: val; }")
void css_apply_styles(html_element_t *root, const char *css_string) {
  if (!root || !css_string)
    return;

  const char *p = css_string;
  while (*p) {
    p = skip_whitespace(p);
    if (*p == '\0')
      break;

    if (*p != '.') { // Only class selectors for now
      while (*p && *p != '{')
        p++;
      if (*p == '{') {
        int depth = 1;
        p++;
        while (*p && depth > 0) {
          if (*p == '{')
            depth++;
          else if (*p == '}')
            depth--;
          p++;
        }
      } else {
        if (*p)
          p++;
      }
      continue;
    }
    p++; // skip .

    char class_name[64] = {0};
    int i = 0;
    while (*p && *p != ' ' && *p != '{' && i < 63)
      class_name[i++] = *p++;
    class_name[i] = '\0';

    p = skip_whitespace(p);
    if (*p == '{') {
      p++;
      while (*p && *p != '}') {
        p = skip_whitespace(p);
        if (*p == '}' || *p == '\0')
          break;

        char key[64] = {0};
        i = 0;
        while (*p && *p != ':' && i < 63)
          key[i++] = *p++;
        key[i] = '\0';

        // Trim end spaces of key
        int klen = strlen(key);
        while (klen > 0 && (key[klen - 1] == ' ' || key[klen - 1] == '\t' ||
                            key[klen - 1] == '\r' || key[klen - 1] == '\n')) {
          key[--klen] = '\0';
        }

        if (*p == ':') {
          p++;
          p = skip_whitespace(p);
          char val[64] = {0};
          i = 0;
          while (*p && *p != ';' && *p != '}' && i < 63)
            val[i++] = *p++;
          val[i] = '\0';

          // Trim end spaces of val
          int vlen = strlen(val);
          while (vlen > 0 && (val[vlen - 1] == ' ' || val[vlen - 1] == '\t' ||
                              val[vlen - 1] == '\r' || val[vlen - 1] == '\n')) {
            val[--vlen] = '\0';
          }

          if (*p == ';')
            p++;

          kprintf("CSS Apply: Cls='%s' Key='%s' Val='%s'\n", class_name, key,
                  val);
          apply_to_tree_internal(root, class_name, key, val);
        } else {
          while (*p && *p != ';' && *p != '}')
            p++;
          if (*p == ';')
            p++;
        }
      }
      if (*p == '}')
        p++;
    }
  }
}

// Layout calculation
void html_layout(html_element_t *root, int parent_width) {
  if (!root)
    return;

  // Calculate dimensions
  if (root->style.width > 0) {
    root->computed_width = root->style.width;
  } else {
    root->computed_width = parent_width - root->style.margin * 2;
  }

  if (root->style.height > 0) {
    root->computed_height = root->style.height;
  } else {
    // Auto height based on content
    if (root->type == HTML_ELEMENT_TEXT && root->text) {
      root->computed_height = 20; // Single line of text
    } else {
      root->computed_height = 30; // Default
    }
  }

  // Add padding
  root->computed_width += root->style.padding * 2;
  root->computed_height += root->style.padding * 2;

  // Layout children
  int current_child_y = root->style.padding;
  html_element_t *child = root->first_child;
  while (child) {
    child->x = root->style.padding;
    child->y = current_child_y;
    html_layout(child, root->computed_width - root->style.padding * 2);
    current_child_y += child->computed_height + child->style.margin;
    child = child->next_sibling;
  }

  // Adjust parent height if needed
  if (current_child_y > root->computed_height) {
    root->computed_height = current_child_y + root->style.padding;
  }
}

// Render HTML element
void html_render(html_element_t *root, int x, int y) {
  if (!root)
    return;

  root->x = x;
  root->y = y;

  // Render shadow first (if any)
  if (root->style.has_shadow) {
    int shadow_x = x + root->style.shadow_offset;
    int shadow_y = y + root->style.shadow_offset;
    if (root->style.border_radius > 0) {
      gfx_draw_rounded_rect(shadow_x, shadow_y, root->computed_width,
                            root->computed_height, root->style.border_radius,
                            0x00404040); // Shadow color
    } else {
      gfx_draw_rect(shadow_x, shadow_y, root->computed_width,
                    root->computed_height, 0x00404040); // Shadow color
    }
  }

  // Choose background color (hover vs normal)
  uint32_t bg_color = root->style.background_color;
  bool draw_bg = root->style.has_background || root->style.is_gradient;
  if (root->is_hovered && root->style.has_hover_style) {
    bg_color = root->style.hover_background_color;
    draw_bg = true;
  }

  // Render background (support gradients and rounded corners)
  // Only draw if a background was explicitly set via CSS
  if (draw_bg) {
    if (root->style.is_gradient &&
        !(root->is_hovered && root->style.has_hover_style)) {
      gfx_draw_gradient_rect(x, y, root->computed_width, root->computed_height,
                             root->style.background_color,
                             root->style.background_color2,
                             root->style.is_gradient_vertical);
    } else if (root->style.border_radius > 0) {
      gfx_draw_rounded_rect(x, y, root->computed_width, root->computed_height,
                            root->style.border_radius, bg_color);
    } else {
      gfx_draw_rect(x, y, root->computed_width, root->computed_height,
                    bg_color);
    }
  }

  // Render text content
  if (root->type == HTML_ELEMENT_TEXT && root->text) {
    // Text elements already have padding offset included in their x,y from
    // html_layout – just render the text itself.
    gfx_puts(x, y, root->text, root->style.text_color);
  }

  // Render children (inherit text color from parent)
  html_element_t *child = root->first_child;
  while (child) {
    // Text elements inherit parent's text_color
    if (child->type == HTML_ELEMENT_TEXT &&
        root->style.text_color != COLOR_WHITE) {
      child->style.text_color = root->style.text_color;
    }
    html_render(child, x + child->x, y + child->y);
    child = child->next_sibling;
  }
}

// Mouse event handling
void html_handle_mouse(html_element_t *root, int mouse_x, int mouse_y) {
  if (!root)
    return;

  // Check if mouse is within bounds
  if (mouse_x >= root->x && mouse_x < root->x + root->computed_width &&
      mouse_y >= root->y && mouse_y < root->y + root->computed_height) {
    root->is_hovered = true;
  } else {
    root->is_hovered = false;
  }

  // Recursively check children
  html_element_t *child = root->first_child;
  while (child) {
    html_handle_mouse(child, mouse_x, mouse_y);
    child = child->next_sibling;
  }
}
