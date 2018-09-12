# Colorpalette

![Screenshot](colorpalette-swatches.png)

The new **Color Palette** User Data Type can be added to any object or tag in
Cinema 4D and read from XPresso or Python. The Color Palette behaves almost
exactly like the Cinema 4D color swatches from the Color Picker UI, only that
the User Data Type allows you to have many of these color palettes attached to
various or a single object.

In the Attributes Manager Mode of the **nr-toolbox**, you can find the
**Swatches** tab which gives you a document-wide, globally available Color
Palette and the **Synchronize Shaders** checkbox.

#### Synchronize Shaders

When this option is enabled, which it is by default, Cinema 4D **Color Shaders**
of which the name starts with `swatch:` will automatically be adjusted to match
the color of the swatch in the global Color Palette given by the name that
follows the `swatch:` string.

![Screenshot](colorpalette-syncshaders.png)

#### Swatch Shader

The **nrSwatch** shader is an alternative to use the **Synchronize Shaders**
feature that allows you to select one of the swatches saved in the *nr-toolbox*
global Color Palette.

![Screenshot](colorpalette-nrswatch.png)
