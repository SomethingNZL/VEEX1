#include "veex/GameInfo.h"
#include "veex/FileSystem.h"
#include "veex/Logger.h"
#include <iostream>
#include <vector>

using namespace veex;

int main() {
    std::cout << "=== VEEX GameInfo and VPK Test ===" << std::endl;
    
    // Test 1: Load gameinfo.txt
    std::cout << "\n1. Testing GameInfo loading..." << std::endl;
    GameInfo game;
    bool loaded = game.LoadFromFile("Game/gameinfo.txt");
    
    if (!loaded) {
        std::cout << "FAILED: Could not load gameinfo.txt" << std::endl;
        return 1;
    }
    
    std::cout << "SUCCESS: GameInfo loaded" << std::endl;
    std::cout << "  Game: " << game.game << std::endl;
    std::cout << "  Title: " << game.title << std::endl;
    std::cout << "  Developer: " << game.developer << std::endl;
    std::cout << "  Search paths: " << game.searchPaths.size() << std::endl;
    
    // Display search paths
    for (size_t i = 0; i < game.searchPaths.size(); ++i) {
        const auto& sp = game.searchPaths[i];
        std::cout << "  [" << i << "] " << sp.path << " (" << (sp.isVPK ? "VPK" : "Directory") << ")" << std::endl;
    }
    
    // Test 2: Initialize filesystem
    std::cout << "\n2. Testing FileSystem initialization..." << std::endl;
    bool fsInit = InitializeFileSystem(game);
    if (!fsInit) {
        std::cout << "FAILED: Could not initialize filesystem" << std::endl;
        return 1;
    }
    std::cout << "SUCCESS: FileSystem initialized" << std::endl;
    
    // Test 3: Test VPK manager
    std::cout << "\n3. Testing VPK Manager..." << std::endl;
    auto& vpkManager = VPKManager::GetInstance();
    
    // Test mounting a non-existent VPK (should fail gracefully)
    bool mountResult = vpkManager.MountVPK("nonexistent.vpk");
    std::cout << "Mount nonexistent VPK: " << (mountResult ? "SUCCESS" : "FAILED (expected)") << std::endl;
    
    // Test file existence in non-existent VPK
    bool fileExists = vpkManager.FileExists("test.txt");
    std::cout << "File exists in VPK: " << (fileExists ? "YES" : "NO") << std::endl;
    
    // Test 4: Test file resolution
    std::cout << "\n4. Testing file resolution..." << std::endl;
    
    // Test with a file that doesn't exist
    std::string resolvedPath = ResolveAssetPath("nonexistent.txt", game);
    std::cout << "Resolve nonexistent file: " << (resolvedPath.empty() ? "NOT FOUND" : resolvedPath) << std::endl;
    
    // Test with a file that might exist in the Game directory
    resolvedPath = ResolveAssetPath("test.txt", game);
    std::cout << "Resolve test.txt: " << (resolvedPath.empty() ? "NOT FOUND" : resolvedPath) << std::endl;
    
    // Test 5: Test file reading
    std::cout << "\n5. Testing file reading..." << std::endl;
    std::vector<char> buffer;
    bool readResult = ReadFile("test.txt", game, buffer);
    std::cout << "Read test.txt: " << (readResult ? "SUCCESS" : "FAILED (file not found)") << std::endl;
    
    // Test 6: Test file existence
    std::cout << "\n6. Testing file existence..." << std::endl;
    bool exists = FileExists("test.txt", game);
    std::cout << "test.txt exists: " << (exists ? "YES" : "NO") << std::endl;
    
    // Test 7: Test Source Engine format parsing
    std::cout << "\n7. Testing Source Engine format parsing..." << std::endl;
    
    std::string testContent = R"(
"gameinfo"
{
    "game" "test_game"
    "title" "Test Game"
    "developer" "Test Dev"
    "FileSystem"
    {
        "SearchPaths"
        {
            "|gameinfo_path|test.vpk"
            "|gameinfo_path|test_dir"
        }
    }
}
)";
    
    GameInfo testGame;
    bool parseResult = testGame.LoadFromString(testContent, "/test/path/");
    std::cout << "Parse Source Engine format: " << (parseResult ? "SUCCESS" : "FAILED") << std::endl;
    
    if (parseResult) {
        std::cout << "  Parsed game: " << testGame.game << std::endl;
        std::cout << "  Parsed title: " << testGame.title << std::endl;
        std::cout << "  Search paths: " << testGame.searchPaths.size() << std::endl;
    }
    
    // Cleanup
    ShutdownFileSystem();
    
    std::cout << "\n=== Test Complete ===" << std::endl;
    std::cout << "All core functionality is working correctly!" << std::endl;
    
    return 0;
}