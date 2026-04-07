cmake_minimum_required(VERSION 3.15)
project(VEEXGameData NONE)

# game data target to make it easy to copy assets in future
add_custom_target(GameData ALL
    COMMENT "Game data target - no compilation"
)
