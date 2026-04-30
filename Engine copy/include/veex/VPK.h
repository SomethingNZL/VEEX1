#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <fstream>

namespace veex {

/**
 * @brief Represents a file entry in a VPK archive.
 */
struct VPKFileEntry {
    std::string name;           ///< Full path and filename within the VPK
    std::string extension;      ///< File extension (without dot)
    std::string path;           ///< Directory path within the VPK
    std::string archiveName;    ///< Name of the archive file (for multi-part VPKs)
    uint32_t crc32;             ///< CRC32 checksum of the file
    uint16_t preloadBytes;      ///< Number of bytes preloaded at the end of the directory
    uint16_t archiveIndex;      ///< Index of the archive file (0xFFFF for directory)
    uint32_t entryOffset;       ///< Offset within the archive file
    uint32_t entryLength;       ///< Length of the file data
    std::vector<char> preloadData; ///< Preloaded data (if any)
    
    VPKFileEntry() : crc32(0), preloadBytes(0), archiveIndex(0xFFFF), entryOffset(0), entryLength(0) {}
};

/**
 * @brief Represents a directory entry in a VPK archive.
 */
struct VPKDirectoryEntry {
    std::string name;           ///< Directory name
    std::vector<std::shared_ptr<VPKFileEntry>> files; ///< Files in this directory
    std::unordered_map<std::string, std::shared_ptr<VPKDirectoryEntry>> subdirectories; ///< Subdirectories
    
    VPKDirectoryEntry(const std::string& dirName) : name(dirName) {}
};

/**
 * @brief VPK (Valve Pak) archive reader and manager.
 * 
 * Supports VPK version 1 and 2 formats used by Source Engine games.
 * Provides file access abstraction that integrates with the FileSystem.
 * 
 * VPK format documentation:
 * - Version 1: Used by Left 4 Dead, Portal 2, Alien Swarm
 * - Version 2: Used by CS:GO, Dota 2, TF2, and all Source 2 games
 */
class VPK {
public:
    /**
     * @brief Constructor.
     * @param vpkPath Path to the VPK file.
     */
    VPK(const std::string& vpkPath);
    
    /**
     * @brief Destructor.
     */
    ~VPK();
    
    /**
     * @brief Load and parse the VPK file.
     * @return True if loading was successful.
     */
    bool Load();
    
    /**
     * @brief Check if a file exists in the VPK.
     * @param filePath Path to the file within the VPK.
     * @return True if the file exists.
     */
    bool FileExists(const std::string& filePath) const;
    
    /**
     * @brief Get the size of a file in the VPK.
     * @param filePath Path to the file within the VPK.
     * @return File size in bytes, or 0 if file doesn't exist.
     */
    size_t GetFileSize(const std::string& filePath) const;
    
    /**
     * @brief Read a file from the VPK.
     * @param filePath Path to the file within the VPK.
     * @param buffer Output buffer to store the file data.
     * @return True if reading was successful.
     */
    bool ReadFile(const std::string& filePath, std::vector<char>& buffer) const;
    
    /**
     * @brief Get all files in a directory.
     * @param directoryPath Path to the directory.
     * @param files Output vector to store file paths.
     * @param recursive Whether to include subdirectories.
     * @return True if successful.
     */
    bool GetFilesInDirectory(const std::string& directoryPath, std::vector<std::string>& files, bool recursive = false) const;
    
    /**
     * @brief Get the VPK file path.
     * @return Path to the VPK file.
     */
    const std::string& GetPath() const { return vpkPath; }
    
    /**
     * @brief Check if the VPK is loaded and valid.
     * @return True if loaded successfully.
     */
    bool IsValid() const { return valid; }
    
    /**
     * @brief Get the number of files in the VPK.
     * @return Number of files.
     */
    size_t GetFileCount() const;
    
    /**
     * @brief Check if this is a VPK version 2 archive.
     * @return True if version 2.
     */
    bool IsVersion2() const { return isVersion2; }

private:
    // VPK signature constant
    static const uint32_t VPK_SIGNATURE = 0x55aa1234;
    
    // Header structures matching documented format
    struct VPKHeader_v1 {
        uint32_t signature;     ///< Must be 0x55aa1234
        uint32_t version;       ///< Must be 1
        uint32_t treeSize;      ///< Size of directory tree in bytes
    };
    
    struct VPKHeader_v2 {
        uint32_t signature;             ///< Must be 0x55aa1234
        uint32_t version;               ///< Must be 2
        uint32_t treeSize;              ///< Size of directory tree in bytes
        uint32_t fileDataSectionSize;   ///< Size of embedded file data
        uint32_t archiveMD5SectionSize; ///< Size of archive MD5 checksums
        uint32_t otherMD5SectionSize;   ///< Size of other MD5 checksums
        uint32_t signatureSectionSize;   ///< Size of public key + signature
    };
    
    // Directory entry structure (on-disk format)
    struct VPKDirectoryEntryRaw {
        uint32_t crc32;
        uint16_t preloadBytes;
        uint16_t archiveIndex;  ///< 0xFFFF = data follows directory
        uint32_t entryOffset;
        uint32_t entryLength;
        uint16_t terminator;    ///< Must be 0xffff
    };
    
    std::string vpkPath;                    ///< Path to the VPK file
    mutable std::ifstream file;             ///< File stream for reading (mutable for const methods)
    bool isVersion2;                        ///< True if VPK version 2
    uint32_t treeSize;                      ///< Size of directory tree
    uint32_t fileDataSectionSize;           ///< VPK2: embedded file data size
    uint32_t archiveMD5SectionSize;         ///< VPK2: archive MD5 section size
    uint32_t otherMD5SectionSize;           ///< VPK2: other MD5 section size
    uint32_t signatureSectionSize;          ///< VPK2: signature section size
    std::shared_ptr<VPKDirectoryEntry> rootDirectory; ///< Root directory of the VPK
    std::unordered_map<std::string, std::shared_ptr<VPKFileEntry>> fileIndex; ///< Fast lookup index
    mutable std::vector<std::vector<char>> archiveFiles; ///< Cached archive file data
    bool valid;                             ///< Whether the VPK is valid and loaded
    
    /**
     * @brief Parse the VPK directory structure.
     * @return True if parsing was successful.
     */
    bool ParseDirectory();
    
    /**
     * @brief Read a file entry from the directory.
     * @param directory Directory to add the file to.
     * @param extension File extension.
     * @param path File path.
     * @return True if successful.
     */
    bool ReadFileEntry(VPKDirectoryEntry* directory, const std::string& extension, const std::string& path);
    
    /**
     * @brief Read file data from archive (version 1).
     * @param entry File entry containing archive information.
     * @param buffer Output buffer.
     * @return True if successful.
     */
    bool ReadFileData_V1(const VPKFileEntry* entry, std::vector<char>& buffer) const;
    
    /**
     * @brief Read file data from archive (version 2).
     * @param entry File entry containing archive information.
     * @param buffer Output buffer.
     * @return True if successful.
     */
    bool ReadFileData_V2(const VPKFileEntry* entry, std::vector<char>& buffer) const;
    
    /**
     * @brief Read file data from an archive.
     * @param entry File entry containing archive information.
     * @param buffer Output buffer.
     * @return True if successful.
     */
    bool ReadFileData(const VPKFileEntry* entry, std::vector<char>& buffer) const;
    
    /**
     * @brief Read and cache an archive file.
     * @param archiveIndex Index of the archive file.
     * @param buffer Output buffer for archive data.
     * @return True if successful.
     */
    bool ReadArchiveFile(uint32_t archiveIndex, std::vector<char>& buffer) const;
    
    /**
     * @brief Get the path to an archive file.
     * @param archiveIndex Index of the archive file.
     * @return Path to the archive file.
     */
    std::string GetArchivePath(uint32_t archiveIndex) const;
    
    /**
     * @brief Validate the VPK signature (VPK2 only).
     * @return True if signature is valid.
     */
    bool ValidateSignature() const;
    
    /**
     * @brief Validate MD5 checksums (VPK2 only).
     * @return True if checksums are valid.
     */
    bool ValidateMD5Checksums() const;
    
    /**
     * @brief Validate tree checksum (VPK2 only).
     * @return True if checksum is valid.
     */
    bool ValidateTreeChecksum() const;
    
    /**
     * @brief Validate all checksums and signatures.
     * @return True if validation passes.
     */
    bool Validate();
    
    /**
     * @brief Get a directory entry by path.
     * @param path Path to the directory.
     * @return Pointer to the directory entry, or nullptr if not found.
     */
    VPKDirectoryEntry* GetDirectory(const std::string& path) const;
    
    /**
     * @brief Helper function to collect all files recursively.
     * @param directory Directory to collect files from.
     * @param prefix Path prefix to prepend to file names.
     * @param files Output vector.
     */
    void CollectFiles(const VPKDirectoryEntry* directory, const std::string& prefix, std::vector<std::string>& files) const;
    
    /**
     * @brief Helper function to collect files in a specific directory.
     * @param directory Directory to collect files from.
     * @param files Output vector.
     * @param recursive Whether to include subdirectories.
     */
    void CollectFilesInDirectory(const VPKDirectoryEntry* directory, std::vector<std::string>& files, bool recursive) const;
    
    /**
     * @brief Get all file paths in the VPK.
     * @param files Output vector to store all file paths.
     */
    void GetAllFiles(std::vector<std::string>& files) const;
};

} // namespace veex