# Spiny pufferfish — Batyskaf

Original static low-poly model inspired by the supplied porcupinefish photograph,
with an ochre/ivory palette chosen to match the project's LowPolyFish assets.

- Unreal asset: `/Game/Batyscaphe/Models/Pufferfish/SM_Pufferfish_Spiny`
- Editable source: `Pufferfish_Spiny.blend`
- Interchange: `SM_Pufferfish_Spiny.fbx`, with embedded palette texture
- 2,534 triangles, 79 modeled spines, one material, 256 × 256 palette texture
- Forward: +X; up: +Z; pivot: body center
- Total dimensions: approximately 148 × 152 × 130 cm; body diameter: about 100 cm
- Imported into Unreal with one simple convex collision hull
- Static mesh only: no skeleton, swimming animation, or gameplay behavior

The Blender file includes a separate Preview_Studio collection. The FBX contains
only the fish. Palette UVs intentionally sample solid color cells; they are not
a unique paint UV layout. No external texture dependencies are needed in Blender.

Rebuild using Blender 2.93 or compatible:

```text
blender -b --python build_pufferfish.py
```

The script regenerates the mesh, palette, FBX, Blender source, statistics and two
transparent-background preview renders. Rebuilding overwrites those generated
files; preserve any hand-edited Blender source separately first.
