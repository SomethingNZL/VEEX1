#include "veex/VPK.h"
#include "veex/Logger.h"
#include <algorithm>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/md5.h>

namespace veex {

VPK::VPK(const std::string& vpkPath) 
    : vpkPath(vpkPath), valid(false), isVersion2(false), 
      treeSize(0), fileDataSectionSize(0), archiveMD5SectionSize(0),
      otherMD5SectionSize(0), signatureSectionSize(0) {
}

VPK::~VPK() {
    if (file.is_open()) {
        file.close();
    }
}

bool VPK::Load() {
    file.open(vpkPath, std::ios::binary);
    if (!file.is_open()) {
        Logger::Error("VPK: Could not open file: " + vpkPath);
        return false;
    }

    // Read and validate header
    uint32_t signature;
    file.read(reinterpret_cast<char*>(&signature), sizeof(uint32_t));
    
    if (signature != VPK_SIGNATURE) {
        Logger::Error("VPK: Invalid signature in: " + vpkPath);
        return false;
    }

    file.read(reinterpret_cast<char*>(&treeSize), sizeof(uint32_t));
    
    // Determine version based on file size and structure
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(sizeof(uint32_t) * 2, std::ios::beg);
    
    if (fileSize > (sizeof(VPKHeader_v1) + 4)) {
        isVersion2 = true;
        file.read(reinterpret_cast<char*>(&fileDataSectionSize), sizeof(uint32_t));
        file.read(reinterpret_cast<char*>(&archiveMD5SectionSize), sizeof(uint32_t));
        file.read(reinterpret_cast<char*>(&otherMD5SectionSize), sizeof(uint32_t));
        file.read(reinterpret_cast<char*>(&signatureSectionSize), sizeof(uint32_t));
        treeSize = fileSize - (sizeof(VPKHeader_v2) - sizeof(VPKHeader_v1));
    } else {
        isVersion2 = false;
        fileDataSectionSize = 0;
        archiveMD5SectionSize = 0;
        otherMD5SectionSize = 0;
        signatureSectionSize = 0;
    }

    Logger::Info("VPK: Loading " + std::string(isVersion2 ? "version 2" : "version 1") + " archive: " + vpkPath);

    if (!ParseDirectory()) {
        Logger::Error("VPK: Failed to parse directory in: " + vpkPath);
        return false;
    }

    if (!Validate()) {
        Logger::Error("VPK: Validation failed for: " + vpkPath);
        return false;
    }

    valid = true;
    Logger::Info("VPK: Successfully loaded " + std::to_string(GetFileCount()) + " files from: " + vpkPath);
    return true;
}

bool VPK::ParseDirectory() {
    file.seekg(-static_cast<int>(treeSize), std::ios::end);
    rootDirectory = std::make_shared<VPKDirectoryEntry>("");

    std::string extension;
    while (true) {
        std::getline(file, extension, '\0');
        if (extension.empty() && file.eof()) break;

        std::string path;
        while (true) {
            std::getline(file, path, '\0');
            if (path.empty() && file.eof()) break;
            if (!ReadFileEntry(rootDirectory.get(), extension, path)) {
                return false;
            }
        }
    }
    return true;
}

bool VPK::ReadFileEntry(VPKDirectoryEntry* directory, const std::string& extension, const std::string& path) {
    std::string fileName;
    std::getline(file, fileName, '\0');
    if (fileName.empty() && file.eof()) return true;

    std::string fullPath = path;
    if (!path.empty()) {
        fullPath += "/" + fileName;
    } else {
        fullPath = fileName;
    }
    if (!extension.empty()) {
        fullPath += "." + extension;
    }

    auto fileEntry = std::make_shared<VPKFileEntry>();
    fileEntry->name = fullPath;
    fileEntry->extension = extension;
    fileEntry->path = path;
    fileEntry->archiveName = GetArchivePath(fileEntry->archiveIndex);

    file.read(reinterpret_cast<char*>(&fileEntry->crc32), sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(&fileEntry->preloadBytes), sizeof(uint16_t));
    file.read(reinterpret_cast<char*>(&fileEntry->archiveIndex), sizeof(uint16_t));
    file.read(reinterpret_cast<char*>(&fileEntry->entryOffset), sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(&fileEntry->entryLength), sizeof(uint32_t));

    if (fileEntry->preloadBytes > 0) {
        fileEntry->preloadData.resize(fileEntry->preloadBytes);
        file.read(fileEntry->preloadData.data(), fileEntry->preloadBytes);
    }

    directory->files.push_back(fileEntry);
    fileIndex[fullPath] = fileEntry;
    return true;
}

std::string VPK::GetArchivePath(uint32_t archiveIndex) const {
    if (!isVersion2 || archiveIndex == 0xFFFF) {
        return vpkPath;
    }
    size_t pos = vpkPath.find_last_of('.');
    if (pos != std::string::npos) {
        return vpkPath.substr(0, pos) + "_" + std::to_string(archiveIndex) + ".vpk";
    }
    return vpkPath;
}

bool VPK::ReadArchiveFile(uint32_t archiveIndex, std::vector<char>& buffer) const {
    if (archiveIndex >= archiveFiles.size()) {
        std::string archivePath = GetArchivePath(archiveIndex);
        std::ifstream archiveFile(archivePath, std::ios::binary);
        if (!archiveFile.is_open()) {
            Logger::Error("VPK: Could not open archive file: " + archivePath);
            return false;
        }
        archiveFile.seekg(0, std::ios::end);
        size_t size = archiveFile.tellg();
        archiveFile.seekg(0, std::ios::beg);
        buffer.resize(size);
        archiveFile.read(buffer.data(), size);
        if (archiveFile.gcount() != static_cast<std::streamsize>(size)) {
            Logger::Error("VPK: Failed to read complete archive: " + archivePath);
            return false;
        }
        if (archiveIndex >= archiveFiles.size()) {
            archiveFiles.resize(archiveIndex + 1);
        }
        archiveFiles[archiveIndex] = buffer;
    } else {
        buffer = archiveFiles[archiveIndex];
    }
    return true;
}

bool VPK::ReadFileData_V1(const VPKFileEntry* entry, std::vector<char>& buffer) const {
    std::vector<char> archiveData;
    if (!ReadArchiveFile(entry->archiveIndex, archiveData)) {
        return false;
    }
    buffer.resize(entry->entryLength);
    std::memcpy(buffer.data(), archiveData.data() + entry->entryOffset, entry->entryLength);
    return true;
}

bool VPK::ReadFileData_V2(const VPKFileEntry* entry, std::vector<char>& buffer) const {
    if (entry->archiveIndex == 0xFFFF) {
        file.seekg(entry->entryOffset);
        buffer.resize(entry->entryLength);
        file.read(buffer.data(), entry->entryLength);
        if (file.gcount() != static_cast<std::streamsize>(entry->entryLength)) {
            Logger::Error("VPK: Failed to read complete file data: " + entry->name);
            return false;
        }
    } else {
        std::vector<char> archiveData;
        if (!ReadArchiveFile(entry->archiveIndex, archiveData)) {
            return false;
        }
        buffer.resize(entry->entryLength);
        std::memcpy(buffer.data(), archiveData.data() + entry->entryOffset, entry->entryLength);
    }
    return true;
}

bool VPK::ReadFileData(const VPKFileEntry* entry, std::vector<char>& buffer) const {
    if (isVersion2) {
        return ReadFileData_V2(entry, buffer);
    } else {
        return ReadFileData_V1(entry, buffer);
    }
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
    if (entry->preloadBytes > 0) {
        std::memcpy(buffer.data(), entry->preloadData.data(), entry->preloadBytes);
        offset += entry->preloadBytes;
    }
    if (entry->entryLength > 0) {
        std::vector<char> fileData;
        if (!ReadFileData(entry.get(), fileData)) {
            return false;
        }
        std::memcpy(buffer.data() + offset, fileData.data(), entry->entryLength);
    }
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

bool VPK::ValidateSignature() const {
    if (signatureSectionSize == 0) return true;
    std::vector<char> signatureData(signatureSectionSize);
    file.seekg(-static_cast<int>(signatureSectionSize), std::ios::end);
    file.read(signatureData.data(), signatureData.size());
    Logger::Info("VPK: Signature validation not implemented (placeholder)");
    return true;
}

bool VPK::ValidateMD5Checksums() const {
    if (archiveMD5SectionSize == 0) return true;
    std::vector<char> md5Data(archiveMD5SectionSize);
    file.seekg(-static_cast<int>(archiveMD5SectionSize), std::ios::end);
    file.read(md5Data.data(), md5Data.size());
    Logger::Info("VPK: MD5 checksum validation not implemented (placeholder)");
    return true;
}

bool VPK::ValidateTreeChecksum() const {
    if (otherMD5SectionSize < MD5_DIGEST_LENGTH) return true;
    file.seekg(-static_cast<int>(treeSize), std::ios::end);
    std::vector<char> treeData(treeSize);
    file.read(treeData.data(), treeSize);
    unsigned char calculatedMD5[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(treeData.data()), treeSize, calculatedMD5);
    unsigned char expectedMD5[MD5_DIGEST_LENGTH];
    file.read(reinterpret_cast<char*>(expectedMD5), MD5_DIGEST_LENGTH);
    if (std::memcmp(calculatedMD5, expectedMD5, MD5_DIGEST_LENGTH) != 0) {
        Logger::Error("VPK: Tree checksum validation failed");
        return false;
    }
    return true;
}

bool VPK::Validate() {
    if (!ValidateSignature()) return false;
    if (!ValidateMD5Checksums()) return false;
    if (!ValidateTreeChecksum()) return false;
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
    if (!currentPath.empty() && currentPath[0] == '/') {
        currentPath = currentPath.substr(1);
    }
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
    for (const auto& fileEntry : directory->files) {
        files.push_back(fileEntry->name);
    }
    if (recursive) {
        for (const auto& subdir : directory->subdirectories) {
            CollectFilesInDirectory(subdir.second.get(), files, true);
        }
    }
}

void VPK::CollectFiles(const VPKDirectoryEntry* directory, const std::string& prefix, std::vector<std::string>& files) const {
    for (const auto& fileEntry : directory->files) {
        std::string fullPath = prefix;
        if (!prefix.empty()) {
            fullPath += "/";
        }
        fullPath += fileEntry->name;
        files.push_back(fullPath);
    }
    for (const auto& subdir : directory->subdirectories) {
        std::string newPrefix = prefix;
        if (!prefix.empty()) {
            newPrefix += "/";
        }
        newPrefix += subdir.first;
        CollectFiles(subdir.second.get(), newPrefix, files);
    }
}

void VPK::GetAllFiles(std::vector<std::string>& files) const {
    files.clear();
    CollectFiles(rootDirectory.get(), "", files);
}

size_t VPK::GetFileCount() const {
    return fileIndex.size();
}

} // namespace veex
 
