#pragma once

// Always include glad before GLFW. Include this header instead of
// including glad/gl.h or GLFW/glfw3.h directly in engine source files.

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <string>
#include <cstring>

// OpenGL DXT compressed texture format constants for OpenGL 3.3 Core
// These are typically defined in GL_ARB_texture_compression_bptc or GL_EXT_texture_compression_s3tc
// For OpenGL 3.3+, we use the ARB versions which are part of core functionality
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#endif

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#endif

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#endif

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif

// Helper function to check if DXT compression is fully supported
// Must be called after OpenGL context is created
// Note: macOS supports creating compressed textures but NOT glCompressedTexSubImage2D
inline bool IsDXTSupported() {
    // For OpenGL 3.3+ Core Profile, use glGetStringi instead of glGetString(GL_EXTENSIONS)
    GLint numExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    
    bool found = false;
    for (int i = 0; i < numExtensions; i++) {
        const char* extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
        if (extension) {
            // Simple string comparison without std::string to avoid namespace issues
            if (strstr(extension, "GL_EXT_texture_compression_s3tc") ||
                strstr(extension, "GL_ARB_texture_compression_s3tc") ||
                strstr(extension, "GL_S3_s3tc") ||
                strstr(extension, "GL_EXT_texture_compression_dxt1") ||
                strstr(extension, "GL_EXT_texture_compression_dxt3") ||
                strstr(extension, "GL_EXT_texture_compression_dxt5")) {
                found = true;
                break;
            }
        }
    }
    
    if (!found) {
        // Fallback: Check renderer string for known DXT-supporting GPUs
        const char* renderer = (const char*)glGetString(GL_RENDERER);
        if (renderer) {
            if (strstr(renderer, "NVIDIA") ||
                strstr(renderer, "AMD") ||
                strstr(renderer, "Radeon") ||
                strstr(renderer, "GeForce") ||
                strstr(renderer, "Quadro")) {
                found = true;
            }
        }
    }
    
    if (!found) {
        return false;
    }
    
    // Even if DXT is "supported", check if we're on macOS
    // because macOS doesn't support glCompressedTexSubImage2D
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    if (renderer) {
        // Check for macOS GPU identifiers - these indicate macOS even when running native
        // The GeForce GT 650M is a macBook Pro GPU
        // We also check vendor to be safe
        const char* vendor = (const char*)glGetString(GL_VENDOR);
        if (vendor && strstr(vendor, "NVIDIA")) {
            // On macOS with NVIDIA, DXT subimage upload doesn't work
            // Check if we're on macOS by looking for Apple in renderer
            if (strstr(renderer, "Apple") || strstr(renderer, "Mac")) {
                return false;
            }
            // Also disable for mobile/embedded which may have similar limitations
            if (strstr(renderer, "AGX") || strstr(renderer, "Apple M")) {
                return false;
            }
        }
    }
    
    return true;
}
