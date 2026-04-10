#include "veex/VPK.h"
#include "veex/Logger.h"
#include <algorithm>
#include <cstring>

namespace veex {

// VPK signature constant
static const uint32_t VPK_SIGNATURE = 0x55aa1234;

VPK::VPK(const std::string& vpkPath) 
    : vpkPath(vpkPath), valid(false) {
}

VPK::~VPK() {
    if (file.is_open()) {
        file.close();
    }
}

bool VPK::Load() {
    // Open the VPK file
    file.open(vpkPath, std::ios::binary);
    if (!file.is_open()) {
        Logger::Error("VPK: Could not open file: " + vpkPath);
        return false;
    }

    // Read and validate header
    file.read(reinterpret_cast<char*>(&header), sizeof(VPKHeader));
    if (file.gcount() != sizeof(VPKHeader)) {
        Logger::Error("VPK: Could not read header from: " + vpkPath);
        return false;
    }

    // Validate signature
    if (header.signature != VPK_SIGNATURE) {
        Logger::Error("VPK: Invalid signature in: " + vpkPath);
        return false;
    }

    // Check version (we support version 1 and 2)
    if (header.version != 1 && header.version != 2) {
        Logger::Error("VPK: Unsupported version " + std::to_string(header.version) + " in: " + vpkPath);
        return false;
    }

    Logger::Info("VPK: Loading version " + std::to_string(header.version) + " archive: " + vpkPath);

    // Parse directory structure
    if (!ParseDirectory()) {
        Logger::Error("VPK: Failed to parse directory in: " + vpkPath);
        return false;
    }

    valid = true;
    Logger::Info("VPK: Successfully loaded " + std::to_string(GetFileCount()) + " files from: " + vpkPath);
    return true;
}

bool VPK::ParseDirectory() {
    // Seek to the beginning of the directory
    file.seekg(-static_cast<int>(header.directorySize), std::ios::end);

    // Create root directory
    rootDirectory = std::make_shared<VPKDirectoryEntry>("");
    
    // Read directory entries
    std::string extension;
    while (true) {
        // Read extension
        std::getline(file, extension, '\0');
        if (extension.empty() && file.eof()) {
            break;
        }
        
        // Read path
        std::string path;
        while (true) {
            std::getline(file, path, '\0');
            if (path.empty() && file.eof()) {
                break;
            }
            
            if (!ReadFileEntry(rootDirectory.get(), extension, path)) {
                return false;
            }
        }
    }

    return true;
}

bool VPK::ReadFileEntry(VPKDirectoryEntry* directory, const std::string& extension, const std::string& path) {
    // Read file name
    std::string fileName;
    std::getline(file, fileName, '\0');
    
    if (fileName.empty() && file.eof()) {
        return true; // End of directory
    }

    // Create full file path
    std::string fullPath = path;
    if (!path.empty()) {
        fullPath += "/" + fileName;
    } else {
        fullPath = fileName;
    }
    
    if (!extension.empty()) {
        fullPath += "." + extension;
    }

    // Create file entry
    auto fileEntry = std::make_shared<VPKFileEntry>();
    fileEntry->name = fullPath;
    fileEntry->extension = extension;
    fileEntry->path = path;
    
    // Read file metadata
    file.read(reinterpret_cast<char*>(&fileEntry->crc32), sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(&fileEntry->preloadBytes), sizeof(uint16_t));
    file.read(reinterpret_cast<char*>(&fileEntry->archiveIndex), sizeof(uint16_t));
    file.read(reinterpret_cast<char*>(&fileEntry->entryOffset), sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(&fileEntry->entryLength), sizeof(uint32_t));

    // Read preload data if present
    if (fileEntry->preloadBytes > 0) {
        fileEntry->preloadData.resize(fileEntry->preloadBytes);
        file.read(fileEntry->preloadData.data(), fileEntry->preloadBytes);
    }

    // Add to directory
    directory->files.push_back(fileEntry);
    
    // Add to fast lookup index
    fileIndex[fullPath] = fileEntry;

    return true;
}

bool VPK::FileExists(const std::string& filePath) const {
    return fileIndex.find(filePath) != fileIndex.end();
}

size_t VPK::GetFileSize(const std::string& filePath) const {
    auto it = fileIndex.find(filePath);
    if (it == fileIndex.end()) {
        return 0;
    }
    
    const auto& entry = it->second;
    return entry->preloadBytes + entry->entryLength;
}

bool VPK::ReadFile(const std::string& filePath, std::vector<char>& buffer) const {
    auto it = fileIndex.find(filePath);
    if (it == fileIndex.end()) {
        Logger::Error("VPK: File not found: " + filePath);
        return false;
    }

    const auto& entry = it->second;
    size_t totalSize = entry->preloadBytes + entry->entryLength;
    buffer.resize(totalSize);

    size_t offset = 0;

    // Copy preload data
    if (entry->preloadBytes > 0) {
        std::memcpy(buffer.data(), entry->preloadData.data(), entry->preloadBytes);
        offset += entry->preloadBytes;
    }

    // Read file data from archive
    if (entry->entryLength > 0) {
        if (!ReadFileData(entry.get(), buffer)) {
            Logger::Error("VPK: Failed to read file data: " + filePath);
            return false;
        }
    }

    return true;
}

bool VPK::ReadFileData(const VPKFileEntry* entry, std::vector<char>& buffer) const {
    // For VPK version 1, files are stored in separate .vpk files
    // For VPK version 2, files are stored in chunks
    if (header.version == 1) {
        // Version 1: files are in separate archive files
        std::string archivePath = vpkPath;
        if (entry->archiveIndex != 0xFFFF) {
            // Replace the extension with the archive index
            size_t pos = archivePath.find_last_of('.');
            if (pos != std::string::npos) {
                archivePath = archivePath.substr(0, pos) + "_" + std::to_string(entry->archiveIndex) + ".vpk";
            }
        }

        std::ifstream archiveFile(archivePath, std::ios::binary);
        if (!archiveFile.is_open()) {
            Logger::Error("VPK: Could not open archive file: " + archivePath);
            return false;
        }

        // Seek to the file position
        archiveFile.seekg(entry->entryOffset);
        
        // Read the file data
        archiveFile.read(buffer.data(), entry->entryLength);
        if (archiveFile.gcount() != static_cast<std::streamsize>(entry->entryLength)) {
            Logger::Error("VPK: Failed to read complete file data: " + entry->name);
            return false;
        }
    } else if (header.version == 2) {
        // Version 2: files are stored in chunks
        // For now, we'll treat it similarly to version 1
        // In a full implementation, we would handle chunking properly
        file.seekg(entry->entryOffset);
        file.read(buffer.data(), entry->entryLength);
        if (file.gcount() != static_cast<std::streamsize>(entry->entryLength)) {
            Logger::Error("VPK: Failed to read complete file data: " + entry->name);
            return false;
        }
    }

    return true;
}

bool VPK::GetFilesInDirectory(const std::string& directoryPath, std::vector<std::string>& files, bool recursive) const {
    files.clear();
    
    VPKDirectoryEntry* directory = GetDirectory(directoryPath);
    if (!directory) {
        return false;
    }

    CollectFilesInDirectory(directory, files, recursive);
    return true;
}

VPKDirectoryEntry* VPK::GetDirectory(const std::string& path) const {
    if (path.empty() || path == "/") {
        return rootDirectory.get();
    }

    VPKDirectoryEntry* current = rootDirectory.get();
    std::string currentPath = path;
    
    // Remove leading slash if present
    if (!currentPath.empty() && currentPath[0] == '/') {
        currentPath = currentPath.substr(1);
    }

    // Split path into components
    size_t pos = 0;
    while ((pos = currentPath.find('/')) != std::string::npos) {
        std::string dirName = currentPath.substr(0, pos);
        currentPath.erase(0, pos + 1);

        auto it = current->subdirectories.find(dirName);
        if (it == current->subdirectories.end()) {
            return nullptr;
        }
        current = it->second.get();
    }

    // Handle the final component
    if (!currentPath.empty()) {
        auto it = current->subdirectories.find(currentPath);
        if (it == current->subdirectories.end()) {
            return nullptr;
        }
        current = it->second.get();
    }

    return current;
}

void VPK::CollectFilesInDirectory(const VPKDirectoryEntry* directory, std::vector<std::string>& files, bool recursive) const {
    // Add files in current directory
    for (const auto& file : directory->files) {
        files.push_back(file->name);
    }

    // Add files from subdirectories if recursive
    if (recursive) {
        for (const auto& subdir : directory->subdirectories) {
            CollectFilesInDirectory(subdir.second.get(), files, true);
        }
    }
}

bool VPK::GetAllFiles(std::vector<std::string>& files) const {
    files.clear();
    CollectFiles(rootDirectory.get(), "", files);
    return true;
}

void VPK::CollectFiles(const VPKDirectoryEntry* directory, const std::string& prefix, std::vector<std::string>& files) const {
    // Add files in current directory
    for (const auto& file : directory->files) {
        std::string fullPath = prefix;
        if (!prefix.empty()) {
            fullPath += "/";
        }
        fullPath += file->name;
        files.push_back(fullPath);
    }

    // Add files from subdirectories
    for (const auto& subdir : directory->subdirectories) {
        std::string newPrefix = prefix;
        if (!prefix.empty()) {
            newPrefix += "/";
        }
        newPrefix += subdir.first;
        CollectFiles(subdir.second.get(), newPrefix, files);
    }
}

size_t VPK::GetFileCount() const {
    return fileIndex.size();
}

} // namespace veex