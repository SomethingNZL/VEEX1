#pragma once
#include <string>
#include <vector>
#include <memory>
#include "veex/GameInfo.h"
#include "veex/VPK.h"

namespace veex {
    /**
     * @brief Resolves a local asset path (e.g., "textures/wall.png") into an absolute 
     * disk path by checking the search paths defined in gameinfo.txt.
     * 
     * This function checks both VPK archives and regular filesystem paths in order.
     * VPK files are checked first, then regular directories.
     * 
     * @param localPath The relative path to the asset.
     * @param game The GameInfo containing search paths.
     * @return The absolute path to the asset, or empty string if not found.
     */
    std::string ResolveAssetPath(const std::string& localPath, const GameInfo& game);
    
    /**
     * @brief Reads a file from the filesystem or VPK archives.
     * 
     * This function checks both VPK archives and regular filesystem paths in order.
     * VPK files are checked first, then regular directories.
     * 
     * @param localPath The relative path to the file.
     * @param game The GameInfo containing search paths.
     * @param buffer Output buffer to store the file contents.
     * @return True if the file was found and read successfully.
     */
    bool ReadFile(const std::string& localPath, const GameInfo& game, std::vector<char>& buffer);
    
    /**
     * @brief Checks if a file exists in the filesystem or VPK archives.
     * 
     * @param localPath The relative path to the file.
     * @param game The GameInfo containing search paths.
     * @return True if the file exists.
     */
    bool FileExists(const std::string& localPath, const GameInfo& game);
    
    /**
     * @brief VPK Manager class that handles mounting and accessing VPK files.
     * 
     * This class maintains a cache of loaded VPK files and provides
     * efficient access to files within those archives.
     */
    class VPKManager {
    public:
        /**
         * @brief Get the singleton instance of the VPKManager.
         * @return Reference to the VPKManager instance.
         */
        static VPKManager& GetInstance();
        
        /**
         * @brief Mount a VPK file.
         * @param vpkPath Path to the VPK file.
         * @return True if the VPK was successfully mounted.
         */
        bool MountVPK(const std::string& vpkPath);
        
        /**
         * @brief Unmount a VPK file.
         * @param vpkPath Path to the VPK file.
         * @return True if the VPK was successfully unmounted.
         */
        bool UnmountVPK(const std::string& vpkPath);
        
        /**
         * @brief Check if a file exists in any mounted VPK.
         * @param filePath Path to the file (relative to VPK root).
         * @return True if the file exists in a mounted VPK.
         */
        bool FileExists(const std::string& filePath) const;
        
        /**
         * @brief Read a file from a mounted VPK.
         * @param filePath Path to the file (relative to VPK root).
         * @param buffer Output buffer to store the file contents.
         * @return True if the file was successfully read.
         */
        bool ReadFile(const std::string& filePath, std::vector<char>& buffer) const;
        
        /**
         * @brief Get the size of a file in a mounted VPK.
         * @param filePath Path to the file (relative to VPK root).
         * @return File size in bytes, or 0 if not found.
         */
        size_t GetFileSize(const std::string& filePath) const;
        
        /**
         * @brief Check if a VPK is mounted.
         * @param vpkPath Path to the VPK file.
         * @return True if the VPK is mounted.
         */
        bool IsMounted(const std::string& vpkPath) const;
        
        /**
         * @brief Get the list of mounted VPK paths.
         * @return Vector of mounted VPK paths.
         */
        std::vector<std::string> GetMountedVPKs() const {
            std::vector<std::string> paths;
            for (const auto& m : mountedVPKs) {
                paths.push_back(m.path);
            }
            return paths;
        }
        
        /**
         * @brief Clear all mounted VPKs.
         */
        void Clear();
        
    private:
        struct MountedVPK {
            std::string path;
            std::unique_ptr<VPK> vpk;
        };
        
        std::vector<MountedVPK> mountedVPKs;
        
        VPKManager() = default;
        ~VPKManager() { Clear(); }
        
        // Prevent copying
        VPKManager(const VPKManager&) = delete;
        VPKManager& operator=(const VPKManager&) = delete;
    };
    
    /**
     * @brief Initialize the filesystem with a GameInfo configuration.
     * 
     * This function mounts all VPK files specified in the GameInfo.
     * 
     * @param game The GameInfo to use for initialization.
     * @return True if initialization was successful.
     */
    bool InitializeFileSystem(const GameInfo& game);
    
    /**
     * @brief Shutdown the filesystem and unmount all VPKs.
     */
    void ShutdownFileSystem();

} // namespace veex