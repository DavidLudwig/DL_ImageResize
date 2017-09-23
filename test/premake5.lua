workspace "test-DL_ImageResize"
    configurations { "Debug", "Release" }

externalproject "SDL"
    language "C"
    kind "SharedLib"

    filter { "system:macosx" }
        location "external/SDL/Xcode/SDL"

    filter { "system:windows" }
        location "external/SDL/VisualC/SDL"
        uuid "81CE8DAF-EBB2-4761-8E45-B71ABCCA8C68"

project "test"
    kind "ConsoleApp"
    language "C++"

    --files { "*.h", "*.cpp", "../DL_Raster.h" }
    files { "*.h", "*.cpp" }
    includedirs { "..", "external/SDL/include" }

    filter { "system:macosx" }
        buildoptions { "-std=c++11" }
        frameworkdirs { "external", "SDL" }
        links { "SDL" }
        linkoptions { "-framework OpenGL", "-rpath @executable_path/../../external" }

    filter { "system:windows" }
        links { "SDL" }
        postbuildcommands { "copy /Y Win32\\%{cfg.buildcfg}\\SDL2.dll bin\\%{cfg.buildcfg}\\" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "Full"
