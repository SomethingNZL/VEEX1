# MDL (Valve Source Engine Model) Integration Guide

## Overview

This engine now supports loading and rendering Valve Source Engine (SDK2013) MDL model files. MDL models can be used with both static props (`prop_static`) and animated dynamic props (`prop_dynamic`).

## Features

### Core MDL Support
- ✅ Binary MDL file parsing (SDK2013 format, versions 44-49)
- ✅ Skeletal animation system
- ✅ Bone hierarchy and transformations
- ✅ Animation sequence playback
- ✅ Material/texture reference extraction
- ✅ VPK archive support (models in VPK files)
- ✅ Model caching for performance

### Entity Integration
- ✅ `prop_dynamic` - Animated MDL models with sequence control
- ✅ `prop_static` - Static MDL models (no animation)
- ✅ Source Engine-style I/O system for animation control
- ✅ Skin selection support
- ✅ Model scaling
- ✅ Body group support

## Usage

### 1. Loading MDL Models in Entities

#### Prop Dynamic (Animated)
```cpp
// In a BSP map entity file or through entity spawning:
{
    "classname" "prop_dynamic"
    "model" "models/props_c17/furnitureCouch001.mdl"
    "origin" "0 0 0"
    "angles" "0 45 0"
    "sequence" "0"  // Animation sequence index
    "skin" "0"
    "scale" "1.0"
    "playbackrate" "1.0"
}
```

#### Prop Static (Non-animated)
```cpp
{
    "classname" "prop_static"
    "model" "models/props_c17/furnitureCouch001.mdl"
    "origin" "0 0 0"
    "angles" "0 45 0"
    "skin" "0"
}
```

### 2. Animation Control via I/O

Prop_dynamic entities support Source Engine-style inputs:

```cpp
// Change animation sequence
"SetSequence" "sequence_name"    // By name
"SetSequence" "2"                // By index

// Control playback
"SetPlaybackRate" "2.0"          // Double speed
"SetPlaybackRate" "0.5"          // Half speed

// Change skin
"SetSkin" "1"

// Set animation (alias for SetSequence)
"SetAnimation" "idle"
```

### 3. Programmatic Control

```cpp
// Get the entity
auto prop = std::static_pointer_cast<PropDynamicEntity>(entity);

// Change sequence
prop->SetSequence("walk");
prop->SetSequence(1);  // By index

// Control animation
prop->SetPlaybackRate(2.0f);
prop->SetSkin(1);
prop->SetScale(1.5f);

// Get current state
int currentSeq = prop->GetSequence();
float animTime = prop->GetAnimationTime();
int skin = prop->GetSkin();
```

### 4. Direct MDL Loading

```cpp
#include "veex/MDL.h"
#include "veex/FileSystem.h"

// Load an MDL model directly
auto& mdlCache = MDLCache::Get();
auto mdlModel = mdlCache.LoadModel("models/my_model.mdl", gameInfo);

if (mdlModel) {
    // Access model data
    int numBones = mdlModel->GetNumBones();
    int numSequences = mdlModel->GetNumSequences();
    
    // Find sequence by name
    int seqIndex = mdlModel->FindSequence("idle");
    
    // Get sequence info
    if (seqIndex >= 0) {
        const auto& seq = mdlModel->GetSequences()[seqIndex];
        float duration = seq.numFrames / seq.frameRate;
    }
}
```

## File Structure

### New Files Added
```
Engine/include/veex/
├── MDL.h                    # MDL format definitions and loader
├── PropDynamicEntity.h      # Animated entity class

Engine/src/
├── MDL.cpp                  # MDL parsing implementation
├── PropDynamicEntity.cpp    # Dynamic entity implementation
└── Model.cpp                # Extended for MDL support
```

### MDL Format Structures
The `MDL.h` file contains:
- `MDLHeader` - File header with model metadata
- `MDLBone` - Bone definition
- `MDLTexture` - Material reference
- `MDLBodyPart` - Body part grouping
- `MDLModel` - Model within body part
- `MDLMesh` - Mesh data
- `MDLSeqDesc` - Animation sequence
- `Bone` - Runtime bone data
- `MDLVertex` - Vertex data
- `AnimationSequence` - Runtime animation data
- `MDLModel` class - Main loader
- `MDLCache` class - Caching system

## Technical Details

### Supported MDL Features
- ✅ Studio model format (IDSTUDIO magic)
- ✅ Version 44-49 (Source SDK 2013)
- ✅ Bone hierarchies
- ✅ Skeletal animation sequences
- ✅ Multiple meshes and body parts
- ✅ Material references
- ✅ Vertex data (position, normal, UV, tangent)
- ✅ Basic index buffer data

### Limitations & TODO
- ⚠️ Index buffer parsing (currently generates sequential indices)
- ⚠️ Animation data interpolation (basic playback only)
- ⚠️ Flex animations (facial animation)
- ⚠️ IK chains and rules
- ⚠️ Hitbox sets
- ⚠️ Attachment points
- ⚠️ LOD (Level of Detail) switching
- ⚠️ Advanced vertex animation

### Performance Considerations
- MDL models are cached after first load
- Skeletal animation is calculated per-frame
- Bone matrix calculations use parent hierarchy
- Consider using `prop_static` for non-animated models

## Integration with Existing Systems

### Material System
MDL models reference materials by name. The existing VMT material system will attempt to load corresponding `.vmt` files:

```
models/props_c17/furnitureCouch001.mdl
  → material: "models/props_c17/furnitureCouch001"
  → looks for: materials/models/props_c17/furnitureCouch001.vmt
```

### File System
MDL files can be loaded from:
1. Regular filesystem paths
2. VPK archives (via existing VPK system)
3. Game search paths (via GameInfo)

### Rendering
Models are rendered using the existing OpenGL pipeline. Skeletal animation would require shader modifications to support bone matrices (not yet implemented).

## Example: Complete Setup

### 1. Add to CMakeLists.txt (already done)
```cmake
src/PropDynamicEntity.cpp
src/MDL.cpp
```

### 2. Include Headers
```cpp
#include "veex/PropDynamicEntity.h"
#include "veex/MDL.h"
#include "veex/Model.h"
```

### 3. Create Entity in Map
Place a `prop_dynamic` entity in your BSP map with an MDL model.

### 4. Control via I/O
Connect entity outputs to control animation:
```
logic_relay → prop_dynamic (SetSequence: "walk")
```

## Troubleshooting

### Model Not Loading
1. Check file path is correct
2. Verify MDL file is valid (IDSTUDIO header)
3. Ensure GameInfo search paths are configured
4. Check VPK mounting if model is in archive

### Animation Not Playing
1. Verify model has animation sequences
2. Check sequence index/name is valid
3. Ensure playback rate is > 0
4. Confirm entity is a `prop_dynamic`, not `prop_static`

### Performance Issues
1. Use `prop_static` for non-animated models
2. Reduce number of animated entities
3. Check bone count (high bone counts are expensive)
4. Consider LOD models for distant objects

## Future Enhancements

### Planned Features
- [ ] Full index buffer parsing from strip data
- [ ] Advanced animation interpolation
- [ ] Skeletal animation shaders
- [ ] IK system support
- [ ] Flex animation (facial animation)
- [ ] Attachment point rendering
- [ ] LOD switching
- [ ] Model decaling
- [ ] Physics mesh extraction
- [ ] Studio model QC file support

### Optimization Opportunities
- [ ] GPU skinning implementation
- [ ] Animation blending
- [ ] Compressed vertex formats
- [ ] Instanced rendering for static props
- [ ] Frustum culling per bone
- [ ] Animation LOD

## References

### MDL Format Documentation
- Valve Source Engine SDK documentation
- Studio model format specification
- Source SDK 2013 source code

### Related Systems
- VPK archive format
- VMT material format
- VTF texture format
- BSP entity system

## Support

For issues or questions:
1. Check existing engine documentation
2. Review Source SDK 2013 documentation
3. Examine MDL.cpp implementation details
4. Test with known-good MDL files

---

**Note**: This implementation provides basic MDL support. For production use, consider implementing the TODO items and optimizations listed above.