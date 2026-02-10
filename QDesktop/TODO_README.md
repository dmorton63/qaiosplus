# QDesktop - Desktop Environment TODO

## Overview

QDesktop will be a JSON-driven desktop environment with Windows Vista/Aero-style visual effects including:
- Glass/transparency effects
- Glow highlights on focused elements
- Drop shadows
- Smooth gradients and blur effects
- Animated transitions

The desktop layout, themes, and styling will be fully configurable via JSON files.

---

## Phase 1: JSON Parser & File Infrastructure

### 1.1 JSON Parser (`QDesktop/QDJson.h/.cpp`)
- [ ] Implement lightweight JSON tokenizer
- [ ] JSON value types: null, bool, number, string, array, object
- [ ] Parse from string buffer
- [ ] Error handling with line/column info
- [ ] Memory-efficient design (no STL)

### 1.2 JSON File Loading
- [ ] Integrate with QFileSystem VFS to read `.json` files
- [ ] Support for file watching (detect changes)
- [ ] Cached parsing for performance

---

## Phase 2: Theme System

### 2.1 Theme Definition (`QDesktop/QDTheme.h/.cpp`)
- [ ] `QDTheme` class to hold all visual settings
- [ ] Color palette (primary, secondary, accent, background, text, etc.)
- [ ] Font settings (when we have fonts)
- [ ] Border styles (width, radius, color)
- [ ] Shadow settings (offset, blur, color, spread)
- [ ] Glow settings (color, intensity, radius)
- [ ] Transparency/opacity levels
- [ ] Animation timings

### 2.2 Theme JSON Schema
```json
{
  "name": "Vista Glass",
  "colors": {
    "windowBackground": "#80FFFFFF",
    "titleBarGradientStart": "#CC3A6EA5",
    "titleBarGradientEnd": "#CC1E4A73",
    "buttonNormal": "#40FFFFFF",
    "buttonHover": "#60FFFFFF",
    "buttonPressed": "#30FFFFFF",
    "buttonGlow": "#8052B4E5",
    "textPrimary": "#FFFFFFFF",
    "textSecondary": "#CCFFFFFF",
    "border": "#40000000",
    "shadow": "#40000000"
  },
  "effects": {
    "glassBlur": 20,
    "shadowOffset": { "x": 4, "y": 4 },
    "shadowBlur": 10,
    "glowRadius": 8,
    "borderRadius": 6,
    "borderWidth": 1
  },
  "animations": {
    "hoverDuration": 150,
    "pressDuration": 50,
    "windowOpenDuration": 200
  }
}
```

---

## Phase 3: Enhanced Renderer

### 3.1 Alpha Blending (`QWRenderer` extensions)
- [ ] Full ARGB color support (already partial)
- [ ] Porter-Duff compositing modes
- [ ] Pre-multiplied alpha optimization

### 3.2 Gradient Rendering
- [ ] Linear gradients (horizontal, vertical, diagonal)
- [ ] Radial gradients
- [ ] Multi-stop gradient support

### 3.3 Blur Effects
- [ ] Box blur (fast, for real-time)
- [ ] Gaussian blur (higher quality)
- [ ] Cached blur textures for glass effect

### 3.4 Shadow Rendering
- [ ] Drop shadow with configurable offset/blur
- [ ] Inner shadow for pressed states
- [ ] Shadow caching for performance

### 3.5 Glow Effects
- [ ] Outer glow for focused elements
- [ ] Inner glow for buttons
- [ ] Bloom effect for highlights

### 3.6 Rounded Corners
- [ ] Rounded rectangle primitives
- [ ] Anti-aliased edges

---

## Phase 4: Desktop Manager

### 4.1 Core Desktop Manager (`QDesktop/QDManager.h/.cpp`)
- [ ] Singleton pattern
- [ ] Theme loading and switching
- [ ] Desktop layout management
- [ ] Wallpaper handling
- [ ] Icon grid management
- [ ] System tray / notification area

### 4.2 Desktop Layout JSON
```json
{
  "wallpaper": "/system/wallpapers/default.bmp",
  "theme": "/system/themes/vista-glass.json",
  "taskbar": {
    "position": "bottom",
    "height": 48,
    "autoHide": false
  },
  "icons": {
    "gridSize": 80,
    "spacing": 10,
    "labelPosition": "bottom"
  },
  "shortcuts": [
    {
      "name": "Terminal",
      "icon": "/system/icons/terminal.bmp",
      "command": "terminal",
      "position": { "row": 0, "col": 0 }
    }
  ]
}
```

### 4.3 Layout Switching
- [ ] Save current layout to JSON
- [ ] Load layout from JSON
- [ ] Hot-reload support
- [ ] Layout profiles (work, gaming, etc.)

---

## Phase 5: Enhanced Controls

### 5.1 Base Control Enhancements
- [ ] `QWCtrlBase` - Common control base class
  - Themed rendering
  - Animation state machine
  - Event handling
  - Layout constraints

### 5.2 Vista-Style Button (`QWCtrlButton` upgrade)
- [ ] Gradient background with glass effect
- [ ] Glow on hover (animated fade-in)
- [ ] Press animation (darken + slight shrink)
- [ ] Disabled state (desaturated)
- [ ] Focus ring with glow

### 5.3 Text Box (`QWCtrlTextBox` upgrade)
- [ ] Glass-style background
- [ ] Glowing border on focus
- [ ] Selection highlighting
- [ ] Cursor blinking animation
- [ ] Placeholder text with alpha

### 5.4 New Controls Needed
- [ ] `QWCtrlScrollBar` - Themed scrollbar with glass track
- [ ] `QWCtrlCheckBox` - Check mark with glow animation
- [ ] `QWCtrlRadioButton` - Radio with glow
- [ ] `QWCtrlSlider` - Glass track with glowing thumb
- [ ] `QWCtrlProgressBar` - Glass with animated glow pulse
- [ ] `QWCtrlDropDown` - Combo box with glass dropdown
- [ ] `QWCtrlTabControl` - Tabbed container
- [ ] `QWCtrlTreeView` - Hierarchical view with expand animation
- [ ] `QWCtrlToolbar` - Glass toolbar with button groups
- [ ] `QWCtrlMenu` - Popup menus with glass effect
- [ ] `QWCtrlTooltip` - Fade-in glass tooltip
- [ ] `QWCtrlStatusBar` - Glass status bar

---

## Phase 6: Window Chrome

### 6.1 Vista-Style Window Frame
- [ ] Glass title bar with gradient
- [ ] Glowing window border on focus
- [ ] Drop shadow surrounding window
- [ ] Rounded corners (top only or all)
- [ ] Resize handles with cursor changes

### 6.2 Title Bar Buttons
- [ ] Close button (red glow on hover)
- [ ] Minimize button
- [ ] Maximize/Restore button
- [ ] Custom title bar buttons support

### 6.3 Window Animations
- [ ] Open animation (fade + scale)
- [ ] Close animation (fade out)
- [ ] Minimize animation (shrink to taskbar)
- [ ] Maximize animation (expand to screen)

---

## Phase 7: Taskbar

### 7.1 Taskbar Implementation (`QDesktop/QDTaskbar.h/.cpp`)
- [ ] Glass background with blur
- [ ] Start button with orb glow
- [ ] Window buttons with previews (stretch goal)
- [ ] Quick launch area
- [ ] System tray
- [ ] Clock display

### 7.2 Start Menu
- [ ] Glass panel
- [ ] Search box
- [ ] Pinned items
- [ ] Recent items
- [ ] All programs flyout

---

## Phase 8: Compositor Enhancements

### 8.1 Glass/Blur Compositor (`QWCompositor` upgrade)
- [ ] Per-window blur regions
- [ ] Background sampling for glass effect
- [ ] Blur cache management
- [ ] Composition order handling

### 8.2 Animation System
- [ ] Frame-based animation timer
- [ ] Easing functions (ease-in, ease-out, ease-in-out)
- [ ] Property animation (position, size, opacity, color)
- [ ] Animation chaining

---

## Implementation Order (Suggested)

### Sprint 1: Foundation
1. JSON Parser
2. Theme loading
3. Gradient rendering
4. Alpha blending improvements

### Sprint 2: Visual Effects
1. Drop shadows
2. Glow effects  
3. Rounded corners
4. Blur (basic)

### Sprint 3: Controls
1. Button upgrade with Vista style
2. TextBox upgrade
3. ScrollBar
4. CheckBox, RadioButton

### Sprint 4: Windows
1. Vista window chrome
2. Title bar effects
3. Window shadow
4. Basic animations

### Sprint 5: Desktop
1. Desktop Manager
2. Taskbar
3. Layout loading
4. Theme switching

### Sprint 6: Polish
1. Start Menu
2. Remaining controls
3. Animation refinement
4. Performance optimization

---

## Technical Notes

### Memory Considerations
- Pre-allocate blur buffers
- Cache shadow textures
- Pool animation objects
- Lazy theme loading

### Performance
- Dirty rectangle tracking for partial redraws
- Hardware acceleration hooks (future)
- LOD for blur (lower quality during animations)
- Skip effects on slow systems

### Color Format
- All colors as ARGB (0xAARRGGBB)
- Alpha: 0x00 = transparent, 0xFF = opaque
- Pre-multiplied alpha for faster blending

---

## File Structure

```
QDesktop/
├── CMakeLists.txt
├── include/
│   ├── QDManager.h         # Desktop manager
│   ├── QDTheme.h           # Theme definitions
│   ├── QDTaskbar.h         # Taskbar
│   ├── QDStartMenu.h       # Start menu
│   ├── QDDesktopIcons.h    # Icon grid
│   ├── QDJson.h            # JSON parser
│   └── QDAnimation.h       # Animation system
├── src/
│   ├── QDManager.cpp
│   ├── QDTheme.cpp
│   ├── QDTaskbar.cpp
│   ├── QDStartMenu.cpp
│   ├── QDDesktopIcons.cpp
│   ├── QDJson.cpp
│   └── QDAnimation.cpp
└── themes/
    └── (JSON theme files for testing)
```

---

## Dependencies

- `QFileSystem` - Reading JSON files
- `QWindowing` - Window management
- `QWControls` - Control widgets
- `QEvent` - Input handling
- `QKMemory` - Memory allocation

---

## Questions to Resolve

1. **Blur implementation** - True Gaussian blur is expensive. Box blur cascade? Pre-blurred wallpaper?
2. **Font rendering** - Need TrueType or bitmap fonts for proper text
3. **Image loading** - BMP loader exists? PNG support needed?
4. **Timer system** - Need millisecond-precision timer for animations
5. **Multi-monitor** - Support multiple displays eventually?

---

*Last Updated: February 9, 2026*
